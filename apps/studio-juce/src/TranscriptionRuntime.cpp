#include "TranscriptionRuntime.h"
#include <transcription_engine.h>
#include <juce_cryptography/juce_cryptography.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdexcept>
#include <algorithm>
namespace ui {
namespace {
constexpr auto identity="muscriptor-medium-python-8a1ef886-v1";
void need(bool b,const char* message){if(!b)throw std::runtime_error(message);}
juce::String hash(const juce::File& file){auto in=file.createInputStream();need(in&&in->openedOk(),"runtime_io");return juce::SHA256(*in).toHexString();}
struct Lock{HANDLE h{INVALID_HANDLE_VALUE};~Lock(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}};
struct Stage{juce::File dir;~Stage(){if(dir!=juce::File())dir.deleteRecursively();}};
void probe(const juce::File& dir,const std::atomic<bool>& stop){
    // Single quotes keep this fixed -c program compatible with the existing
    // shell-free installer process helper.
    const juce::String code="import sys,struct,torch,torchaudio; from muscriptor.transcription_model import TranscriptionModel; from beat_this.inference import Audio2Beats; assert sys.version_info[:3]==(3,11,9) and struct.calcsize('P')==8; assert torch.__version__.split('+')[0]=='2.8.0' and torch.version.cuda is None; assert torch.ones(2).sum().item()==2; print('MR_MUSCRIPTOR_READY',torch.backends.cpu.get_cpu_capability())";
    const auto result=mr::separation::runLocalProcess(dir.getChildFile("python.exe"),{"-I","-B","-u","-c",code},dir,stop,std::chrono::minutes(2));
    need(result.success&&result.log.contains("MR_MUSCRIPTOR_READY"),"muscriptor_runtime_probe_failed");
}
}
std::shared_ptr<void> TranscriptionRuntime::lock(bool exclusive)const{
    need(root_.createDirectory().wasOk(),"runtime_io");auto guard=std::make_shared<Lock>();
    guard->h=CreateFileW(root_.getChildFile("python-medium.lock").getFullPathName().toWideCharPointer(),GENERIC_READ,exclusive?0:FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    need(guard->h!=INVALID_HANDLE_VALUE,"runtime_busy");return guard;
}
std::shared_ptr<void> TranscriptionRuntime::acquire()const{
    auto guard=lock(false);need(directory().getChildFile("managed.json").existsAsFile(),"runtime_missing");return guard;
}
void TranscriptionRuntime::verify(std::stop_token token)const{
    auto guard=lock(false);std::atomic<bool> stop{token.stop_requested()};std::stop_callback cancel(token,[&]{stop=true;});
    mr::separation::verifyRuntimeInventory(directory(),identity,stop);probe(directory(),stop);
}
void TranscriptionRuntime::remove()const{
    auto guard=lock(true);if(!present())return;std::atomic<bool> stop{};
    // Verify ownership/file allowlist before deletion; damaged known files
    // remain removable, while foreign files and linked paths are refused.
    mr::separation::verifyRuntimeInventory(directory(),identity,stop,false);need(directory().deleteRecursively(),"runtime_remove_failed");
}
void TranscriptionRuntime::install(std::stop_token token)const{
    auto guard=lock(true);need(!present(),"runtime_already_installed");
    std::atomic<bool> stop{token.stop_requested()};std::stop_callback cancel(token,[&]{stop=true;});
    need(requirements_.existsAsFile(),"runtime_requirements_missing");
    const auto canonical=requirements_.loadFileAsString().replace("\r\n","\n");need(juce::SHA256(canonical.toRawUTF8(),canonical.getNumBytesAsUTF8()).toHexString()==mrs::transcription_requirements_sha256,"runtime_requirements_changed");
    Stage stage{root_.getChildFile(".python-install-"+juce::Uuid().toString())};need(stage.dir.createDirectory().wasOk(),"runtime_io");
    const auto fetch=[&](mr::separation::DownloadArtifact artifact){auto file=stage.dir.getChildFile(artifact.name);mr::separation::fetchRuntimeArtifact(artifact,file,stop);return file;};
    const auto zip=fetch({"python.zip","https://www.python.org/ftp/python/3.11.9/python-3.11.9-embed-amd64.zip","009d6bf7e3b2ddca3d784fa09f90fe54336d5b60f0e0f305c37f400bf83cfd3b",11249023});
    {juce::ZipFile archive(zip);for(int i=0;i<archive.getNumEntries();++i){const auto* entry=archive.getEntry(i);need(entry&&!entry->filename.containsAnyOf("/\\:")&&!entry->filename.startsWithChar('.')&&entry->uncompressedSize>0&&entry->uncompressedSize<64*1024*1024,"runtime_archive");need(archive.uncompressEntry(i,stage.dir).wasOk(),"runtime_archive");}}
    need(zip.deleteFile(),"runtime_io");
    need(stage.dir.getChildFile("python311._pth").replaceWithText("python311.zip\n.\nLib/site-packages\nimport site\n",false,false,"\n"),"runtime_io");
    const auto pip=fetch({"get-pip.py","https://raw.githubusercontent.com/pypa/get-pip/37ae3c4daaf63a0c203176251fb99c0e238562b5/public/get-pip.py","f4448a01e3ab2914c65f4ca59d4cacba6b2dbde19a483fbac46587f8bbaff424",2234489});
    const auto run=[&](juce::StringArray args){const auto result=mr::separation::runLocalProcess(stage.dir.getChildFile("python.exe"),args,stage.dir,stop,std::chrono::minutes(20),8);root_.getChildFile("runtime-last-operation.log").replaceWithText(result.log.substring((std::max)(0,result.log.length()-32768)));need(result.success,stop?"cancelled":"muscriptor_runtime_install_failed");};
    run({"-I","-B","-u",pip.getFullPathName(),"--isolated","--no-input","--no-cache-dir","--disable-pip-version-check","pip==25.0.1"});
    run({"-I","-B","-u","-m","pip","--isolated","install","--no-input","--no-compile","--no-cache-dir","--disable-pip-version-check","--only-binary=:all:","--index-url","https://download.pytorch.org/whl/cpu","torch==2.8.0","torchaudio==2.8.0"});
    const auto requirements=stage.dir.getChildFile("requirements.txt");need(requirements_.copyFileTo(requirements),"runtime_io");
    run({"-I","-B","-u","-m","pip","--isolated","install","--no-input","--no-compile","--no-cache-dir","--disable-pip-version-check","--only-binary=:all:","--index-url","https://pypi.org/simple","-r",requirements.getFullPathName()});
    const auto source=fetch({"muscriptor.zip","https://codeload.github.com/muscriptor/muscriptor/zip/8a1ef88672cc23c26619adaff37ec48d82c0b906","8011c13efa70b1bfe6cc7d7328306d8238978c38a247fa3c3d842f387bc6929e",2952361});
    {juce::ZipFile archive(source);const juce::String prefix="muscriptor-8a1ef88672cc23c26619adaff37ec48d82c0b906/";
     for(int i=0;i<archive.getNumEntries();++i){const auto* entry=archive.getEntry(i);need(entry!=nullptr,"runtime_archive");
        if(!entry->filename.startsWith(prefix))continue;const auto relative=entry->filename.substring(prefix.length());
        if(!(relative.startsWith("muscriptor/")&&relative.endsWith(".py"))&&relative!="LICENSE")continue;
        need(!relative.contains("..")&&!relative.containsAnyOf("\\:")&&entry->uncompressedSize>=0&&entry->uncompressedSize<1024*1024,"runtime_archive");
        const auto target=relative=="LICENSE"?stage.dir.getChildFile("MuScriptor-MIT-LICENSE.txt"):stage.dir.getChildFile("Lib/site-packages/"+relative);
        need(target.getParentDirectory().createDirectory().wasOk(),"runtime_io");std::unique_ptr<juce::InputStream> input{archive.createStreamForEntry(i)};auto output=target.createOutputStream();need(input&&output&&output->writeFromInputStream(*input,entry->uncompressedSize)==entry->uncompressedSize,"runtime_io");
     }}
    need(source.deleteFile()&&pip.deleteFile(),"runtime_io");
    const auto beat=fetch({"beat-this-final0.ckpt","https://cloud.cp.jku.at/public.php/dav/files/7ik4RrBKTS273gp/final0.ckpt","8c328b45f59d8dd3dff219253ff6a8d6482be57d0133a29140e2febbf8eb8331",81058141});
    const auto beatCache=stage.dir.getChildFile("torch-cache/hub/checkpoints");need(beatCache.createDirectory().wasOk()&&beat.moveFileTo(beatCache.getChildFile("beat_this-final0.ckpt")),"runtime_io");
    run({"-I","-B","-m","pip","check"});probe(stage.dir,stop);
    const auto freeze=mr::separation::runLocalProcess(stage.dir.getChildFile("python.exe"),{"-I","-B","-m","pip","freeze"},stage.dir,stop,std::chrono::minutes(1));
    need(freeze.success&&stage.dir.getChildFile("resolved-packages.txt").replaceWithText(freeze.log),"runtime_probe_failed");
    auto* provenance=new juce::DynamicObject;provenance->setProperty("source_commit","8a1ef88672cc23c26619adaff37ec48d82c0b906");provenance->setProperty("requirements_sha256",hash(requirements));
    mr::separation::publishRuntimeInventory(stage.dir,identity,juce::var(provenance),stop);need(!stop&&stage.dir.moveFileTo(directory()),"runtime_publish_failed");stage.dir=juce::File();
}
}
