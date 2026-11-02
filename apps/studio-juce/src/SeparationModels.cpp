#include "SeparationModels.h"
#include <juce_cryptography/juce_cryptography.h>
#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>

namespace mr::separation {
namespace {
struct Error : std::runtime_error { juce::String code; explicit Error(juce::String value):std::runtime_error(value.toStdString()),code(std::move(value)){} };
void need(bool ok, const char* code) { if (!ok) throw Error{code}; }
void check(const std::atomic<bool>& stop) { need(!stop.load(), "cancelled"); }
juce::var object() { return juce::var(new juce::DynamicObject); }
void put(juce::var& v, const char* key, juce::var value) { v.getDynamicObject()->setProperty(key, std::move(value)); }
juce::String sha(const juce::File& file) {
    auto input = file.createInputStream(); need(input && input->openedOk(), "io_error");
    return juce::SHA256(*input).toHexString();
}
juce::String textHash(const juce::String& text) { const auto bytes = text.toUTF8(); return juce::SHA256(bytes.getAddress(), text.getNumBytesAsUTF8()).toHexString(); }
bool plain(const juce::File& f) { const auto a=GetFileAttributesW(f.getFullPathName().toWideCharPointer()); return a!=INVALID_FILE_ATTRIBUTES && !(a&FILE_ATTRIBUTE_REPARSE_POINT); }
bool relativeName(const juce::String& name) {
    if(name.isEmpty() || name.containsChar(':') || name.startsWithChar('/') || name.startsWithChar('\\'))return false;
    const auto parts=juce::StringArray::fromTokens(name.replaceCharacter('\\','/'),"/","");
    for(const auto& part:parts)if(part.isEmpty()||part==".."||part==".")return false;
    return true;
}
bool leaf(const juce::String& name) { return relativeName(name) && !name.containsAnyOf("/\\") && name.length()<=128; }
void walk(const juce::File& root, const juce::File& dir, std::vector<juce::File>& files, int depth=0) {
    need(depth<40 && plain(dir),"unsafe_path");
    for(const auto& child:dir.findChildFiles(juce::File::findFilesAndDirectories,false)) {
        need(plain(child) && child.isAChildOf(root),"unsafe_path");
        if(child.isDirectory())walk(root,child,files,depth+1);else files.push_back(child);
        need(files.size()<=100000,"resource_limit");
    }
}
juce::var manifest(const juce::File& dir,const juce::String& identity) {
    need(dir.isDirectory() && plain(dir),"installation_missing");
    const auto file=dir.getChildFile("managed.json");
    need(file.existsAsFile() && plain(file) && file.getSize()<=16*1024*1024,"unmanaged_installation");
    auto value=juce::JSON::parse(file);
    need(value.isObject() && static_cast<int>(value["schema"])==1 && value["identity"].toString()==identity && value["files"].isArray(),"unmanaged_installation");
    return value;
}
void inventory(const juce::File& dir,const juce::String& identity,const juce::var& provenance,const std::atomic<bool>& stop) {
    std::vector<juce::File> files; walk(dir,dir,files);
    juce::Array<juce::var> entries;
    for(const auto& f:files) {
        check(stop); need(f.getFileName()!="managed.json","invalid_inventory");
        auto entry=object();put(entry,"path",f.getRelativePathFrom(dir).replaceCharacter('\\','/'));put(entry,"sha256",sha(f));entries.add(entry);
    }
    auto value=object();put(value,"schema",1);put(value,"identity",identity);put(value,"provenance",provenance);put(value,"files",juce::var(entries));
    need(dir.getChildFile("managed.json").replaceWithText(juce::JSON::toString(value,true)),"io_error");
}
void verifyInventory(const juce::File& dir,const juce::String& id,const std::atomic<bool>& stop,bool hashes) {
    const auto value=manifest(dir,id); std::set<juce::String> expected{"managed.json"};
    for(const auto& entry:*value["files"].getArray()) {
        check(stop);const auto path=entry["path"].toString();need(relativeName(path)&&expected.insert(path).second,"unsafe_path");
        const auto f=dir.getChildFile(path);
        if(f.exists()){need(f.existsAsFile()&&plain(f),"integrity_failure");if(hashes)need(sha(f)==entry["sha256"].toString(),"integrity_failure");}
        else need(!hashes,"integrity_failure");
    }
    std::vector<juce::File> files;walk(dir,dir,files);
    for(const auto& f:files)need(expected.count(f.getRelativePathFrom(dir).replaceCharacter('\\','/'))!=0,"unmanaged_files");
}
struct Internet { HINTERNET h{}; ~Internet(){if(h)WinHttpCloseHandle(h);} };
void fetchHttps(const DownloadArtifact& artifact,const juce::File& target,const std::atomic<bool>& stop,ModelManager::Progress progress) {
    juce::String url=artifact.url;
    for(int redirects=0;redirects<5;++redirects) {
        check(stop);URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=parts.dwUserNameLength=parts.dwPasswordLength=static_cast<DWORD>(-1);
        need(WinHttpCrackUrl(url.toWideCharPointer(),0,0,&parts)!=FALSE,"download_url");
        const juce::String host(parts.lpszHostName,static_cast<int>(parts.dwHostNameLength));
        need(parts.nScheme==INTERNET_SCHEME_HTTPS && parts.nPort==443 && parts.dwUserNameLength==0 && parts.dwPasswordLength==0 &&
             (host=="www.python.org"||host=="raw.githubusercontent.com"||host=="github.com"||host=="release-assets.githubusercontent.com"||host=="objects.githubusercontent.com"||host=="dl.fbaipublicfiles.com"),"download_url");
        const auto path=juce::String(parts.lpszUrlPath,static_cast<int>(parts.dwUrlPathLength))+juce::String(parts.lpszExtraInfo,static_cast<int>(parts.dwExtraInfoLength));
        Internet session{WinHttpOpen(L"MR-Studio-Separation/0.9.3",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
        need(session.h!=nullptr,"network_error");WinHttpSetTimeouts(session.h,10000,10000,10000,10000);
        Internet connection{WinHttpConnect(session.h,host.toWideCharPointer(),443,0)};need(connection.h!=nullptr,"network_error");
        Internet request{WinHttpOpenRequest(connection.h,L"GET",path.toWideCharPointer(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};need(request.h!=nullptr,"network_error");
        DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;need(WinHttpSetOption(request.h,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy))!=FALSE,"network_error");
        need(WinHttpSendRequest(request.h,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)!=FALSE && WinHttpReceiveResponse(request.h,nullptr)!=FALSE,"network_error");
        DWORD status{},size=sizeof(status);need(WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)!=FALSE,"network_error");
        if(status>=300 && status<400) {
            DWORD bytes{};WinHttpQueryHeaders(request.h,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,nullptr,&bytes,WINHTTP_NO_HEADER_INDEX);
            need(bytes>0&&bytes<32768,"download_url");std::vector<wchar_t> location(bytes/sizeof(wchar_t)+1);
            need(WinHttpQueryHeaders(request.h,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,location.data(),&bytes,WINHTTP_NO_HEADER_INDEX)!=FALSE,"download_url");
            url=juce::String(location.data());continue;
        }
        need(status==200,"http_error");
        auto output=target.createOutputStream();need(output&&output->openedOk(),"io_error");
        std::array<char,65536> buffer{};juce::int64 total{};
        for(;;) {
            check(stop);DWORD count{};need(WinHttpReadData(request.h,buffer.data(),static_cast<DWORD>(buffer.size()),&count)!=FALSE,"network_error");
            if(!count)break;total+=count;need(total<=artifact.bytes,"download_size");
            need(output->write(buffer.data(),count),"io_error");progress(total,artifact.bytes);
        }
        output->flush();need(output->getStatus().wasOk()&&total==artifact.bytes,"download_size");return;
    }
    throw Error{"download_url"};
}
const DownloadArtifact pythonZip{"python-embed.zip","https://www.python.org/ftp/python/3.11.9/python-3.11.9-embed-amd64.zip","009d6bf7e3b2ddca3d784fa09f90fe54336d5b60f0e0f305c37f400bf83cfd3b",11249023};
const DownloadArtifact getPip{"get-pip.py","https://raw.githubusercontent.com/pypa/get-pip/37ae3c4daaf63a0c203176251fb99c0e238562b5/public/get-pip.py","f4448a01e3ab2914c65f4ca59d4cacba6b2dbde19a483fbac46587f8bbaff424",2234489};
// Canonical LF text: controlled profile, never a user-supplied pip requirements list.
constexpr auto requirementsHash="0184624b3268c94f6e98fa0ccefa3fcdef614a719911f726740dcbf7f0fe414c";
struct UseLease { HANDLE handle{INVALID_HANDLE_VALUE}; ~UseLease(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);} };
struct Staging { juce::File dir; bool owned{}; ~Staging(){if(owned&&dir!=juce::File())dir.deleteRecursively();} };
}

const std::vector<ModelRecord>& modelCatalogue() {
    static const std::vector<ModelRecord> records{
        {"official-htdemucs-derived-2-v1","955717e8","HTDemucs","python-audio-separator 0.48.0","Vocals / Instrumental (derived)","Drums / Bass / Other / Vocals","Windows x64; local CPU. GPU runtime not installed.","Demucs and audio-separator: MIT","Checkpoint-specific review deferred; not bundled with Studio.","Official Meta Demucs; catalogue pinned by full SHA256.",{},
         {{"htdemucs.yaml","https://github.com/nomadkaraoke/python-audio-separator/releases/download/model-configs/htdemucs.yaml","239c445d0b14454d541ad8bd9bb271c9e536d267e8a4625208744cbb2e7bb66c",21},
          {"955717e8-8726e21a.th","https://dl.fbaipublicfiles.com/demucs/hybrid_transformer/955717e8-8726e21a.th","8726e21a993978c7ba086d3872e7608d7d5bfca646ca4aca459ffda844faa8b4",84141911}}},
        {"htdemucs-6s","5c90dfd2","HTDemucs 6 sources","python-audio-separator","Not available","Bass / Drums / Other / Vocals / Guitar / Piano","Not validated in this version","MIT","Review deferred","Official Meta Demucs","Extended outputs are not supported by this version.",{}},

    };return records;
}
juce::File ModelManager::defaultRoot() {
    PWSTR value{};need(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&value)),"io_error");
    const juce::File root{juce::String(value)};CoTaskMemFree(value);return root.getChildFile("MoonRiverStudio/Separation");
}
ModelManager::ModelManager(juce::File root,juce::File worker,juce::File requirements,Fetch fetch,std::vector<ModelRecord> catalogue)
    :root_(std::move(root)),worker_(std::move(worker)),requirements_(std::move(requirements)),fetch_(fetch?std::move(fetch):Fetch(fetchHttps)),catalogue_(std::move(catalogue)) {
    for(const auto& r:catalogue_) {need(leaf(r.id)&&leaf(r.version),"invalid_catalogue");for(const auto& f:r.files)need(leaf(f.name)&&f.bytes>0&&f.sha256.length()==64,"invalid_catalogue");status_.models.push_back({r,r.unsupportedReason.isEmpty()?InstallationState::downloadable:InstallationState::unsupported,r.unsupportedReason});}
}
ModelManager::~ModelManager(){cancel();if(thread_.joinable())thread_.join();}
juce::File ModelManager::modelsPath()const{return root_.getChildFile("models");}
juce::File ModelManager::runtimePath()const{return root_.getChildFile("runtimes").getChildFile(runtimeProfile);}
const ModelRecord& ModelManager::record(const juce::String& id)const{for(const auto& r:catalogue_)if(r.id==id)return r;throw Error{"unsupported_model"};}
void ModelManager::ensureRoot(){need(root_.createDirectory().wasOk()&&plain(root_),"unsafe_path");for(const auto* name:{"models","runtimes","locks","jobs"}){auto d=root_.getChildFile(name);need(d.createDirectory().wasOk()&&plain(d),"unsafe_path");}}
void ModelManager::cancel()noexcept{std::lock_guard lock(mutex_);if(status_.cancellable)cancelled_.store(true);}
ManagerStatus ModelManager::status()const{std::lock_guard lock(mutex_);return status_;}
juce::String ModelManager::diagnostics()const{std::lock_guard lock(mutex_);return log_;}
void ModelManager::phase(juce::String phaseName,juce::String detail){std::lock_guard lock(mutex_);status_.phase=std::move(phaseName);status_.detail=std::move(detail);status_.completedBytes=status_.totalBytes=0;}
bool ModelManager::start(juce::String operation,std::function<void()> task) {
    {std::lock_guard lock(mutex_);if(status_.busy)return false;}
    if(thread_.joinable())thread_.join();cancelled_.store(false);
    {std::lock_guard lock(mutex_);status_.busy=true;status_.cancellable=true;status_.operation=operation;status_.phase="Starting";status_.error.clear();log_.clear();}
    try {thread_=std::thread([this,task=std::move(task)]{
        juce::String error;
        {juce::InterProcessLock lock("MRStudio.Separation."+textHash(root_.getFullPathName().toLowerCase()).substring(0,24));
         if(!lock.enter(0))error="busy";
         else {try{task();}catch(const Error& e){error=cancelled_.load()?"cancelled":e.code;}catch(...){error=cancelled_.load()?"cancelled":"io_error";}lock.exit();}}
        std::lock_guard lock(mutex_);status_.busy=false;status_.error=error;status_.phase=error.isEmpty()?"Ready":error=="cancelled"?"Cancelled":"Error";
    });}catch(...){std::lock_guard lock(mutex_);status_.busy=false;status_.error="resource_limit";return false;}
    return true;
}
std::shared_ptr<void> ModelManager::lease(const juce::String& id,bool exclusive)const {
    auto result=std::make_shared<UseLease>();const auto file=root_.getChildFile("locks").getChildFile(id+".use");
    need(file.getParentDirectory().isDirectory()&&plain(file.getParentDirectory())&&(!file.exists()||plain(file)),"unsafe_path");
    result->handle=CreateFileW(file.getFullPathName().toWideCharPointer(),exclusive?GENERIC_READ|GENERIC_WRITE:GENERIC_READ,exclusive?0:FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    need(result->handle!=INVALID_HANDLE_VALUE,"in_use");return result;
}
void ModelManager::scan() {
    phase("Checking local files");std::vector<ModelState> models;
    for(const auto& r:catalogue_) {
        ModelState item{r,InstallationState::downloadable,{}};
        if(r.unsupportedReason.isNotEmpty()){item.state=InstallationState::unsupported;item.reason=r.unsupportedReason;}
        else {const auto dir=modelsPath().getChildFile(r.id);if(dir.exists()){
            item.state=InstallationState::installed;
            try{manifest(dir,r.id);for(const auto& f:r.files){check(cancelled_);const auto path=dir.getChildFile(f.name);need(path.existsAsFile()&&plain(path)&&path.getSize()==f.bytes&&sha(path)==f.sha256,"integrity_failure");}}
            catch(const Error& e){if(e.code=="cancelled")throw;item.state=InstallationState::corrupt;item.reason=e.code;}
        }}models.push_back(std::move(item));
    }
    auto runtime=InstallationState::downloadable;
    if(runtimePath().exists()) {
        runtime=InstallationState::installed;
        try{const auto m=manifest(runtimePath(),runtimeProfile);need(m["provenance"]["requirements_sha256"].toString()==requirementsHash,"integrity_failure");
            for(const auto* name:{"python.exe","python311.dll","python311._pth"}){
                bool found{};for(const auto& entry:*m["files"].getArray())if(entry["path"].toString()==name){found=true;need(plain(runtimePath().getChildFile(name))&&sha(runtimePath().getChildFile(name))==entry["sha256"].toString(),"integrity_failure");}need(found,"integrity_failure");}
        }catch(const Error& e){if(e.code=="cancelled")throw;runtime=InstallationState::corrupt;}
    }
    std::lock_guard lock(mutex_);status_.models=std::move(models);status_.runtime=runtime;
}
bool ModelManager::refresh(){return start("refresh",[this]{scan();});}
void ModelManager::install(const ModelRecord& r) {
    need(r.unsupportedReason.isEmpty()&&!r.files.empty(),"unsupported_model");ensureRoot();auto use=lease(r.id,true);
    const auto destination=modelsPath().getChildFile(r.id);need(!destination.exists(),"already_installed");
    juce::int64 bytes{};for(const auto& f:r.files)bytes+=f.bytes;need(root_.getBytesFreeOnVolume()>bytes+16*1024*1024,"disk_space");
    Staging stage{modelsPath().getChildFile(".part-"+juce::Uuid().toString())};need(CreateDirectoryW(stage.dir.getFullPathName().toWideCharPointer(),nullptr)!=FALSE,"io_error");
    stage.owned=true;
    phase("Downloading model",r.title);juce::int64 offset{};
    for(const auto& f:r.files) {
        check(cancelled_);const auto target=stage.dir.getChildFile(f.name);
        fetch_(f,target,cancelled_,[this,offset,bytes](auto done,auto){std::lock_guard lock(mutex_);status_.completedBytes=offset+done;status_.totalBytes=bytes;});
        check(cancelled_);need(target.existsAsFile()&&plain(target)&&target.getSize()==f.bytes&&sha(target)==f.sha256,"integrity_failure");offset+=f.bytes;
    }
    phase("Verifying model",r.title);auto provenance=object();put(provenance,"model_version",r.version);put(provenance,"source",r.provenance);put(provenance,"code_license",r.codeLicense);put(provenance,"weights_license",r.weightsLicense);
    juce::Array<juce::var> artifacts;for(const auto& file:r.files){auto item=object();put(item,"url",file.url);put(item,"sha256",file.sha256);put(item,"bytes",file.bytes);artifacts.add(item);}put(provenance,"artifacts",juce::var(artifacts));
    inventory(stage.dir,r.id,provenance,cancelled_);check(cancelled_);need(stage.dir.moveFileTo(destination),"io_error");stage.dir=juce::File();scan();
}
bool ModelManager::installModel(const juce::String& id){return start("install_model",[this,id]{install(record(id));});}
void ModelManager::uninstall(const juce::File& dir,const juce::String& id) {
    ensureRoot();auto use=lease(id,true);verifyInventory(dir,id,cancelled_,false);check(cancelled_);
    // No cancellation during the short final remove: only inventoried managed files.
    {std::lock_guard lock(mutex_);status_.cancellable=false;status_.phase="Removing files";}
    need(dir.deleteRecursively(),"io_error");
}
bool ModelManager::removeModel(const juce::String& id){return start("remove_model",[this,id]{const auto& r=record(id);phase("Removing model",r.title);uninstall(modelsPath().getChildFile(id),id);scan();});}
bool ModelManager::removeRuntime(){return start("remove_runtime",[this]{phase("Removing processing runtime");uninstall(runtimePath(),runtimeProfile);scan();});}
void ModelManager::probeRuntime(const juce::File& dir) {
    phase("Checking processing runtime");
    const juce::String code="import sys,struct,importlib.metadata as m; import numpy,scipy,soundfile,torch,torchvision,onnxruntime; onnxruntime.disable_telemetry_events(); from audio_separator.separator import Separator; assert sys.version_info[:3]==(3,11,9) and struct.calcsize('P')==8; assert all(m.version(k).split('+')[0]==v for k,v in [('audio-separator','0.48.0'),('numpy','2.2.6'),('scipy','1.15.3'),('soundfile','0.13.1'),('librosa','0.11.0'),('audioread','3.0.1'),('onnxruntime','1.22.1'),('torch','2.8.0'),('torchvision','0.23.0')]); assert torch.version.cuda is None; assert torch.ones(2).sum().item()==2; print('MR_RUNTIME_CPU_READY')";
    auto result=runLocalProcess(dir.getChildFile("python.exe"),{"-I","-B","-u","-c",code},dir,cancelled_,std::chrono::minutes(2));
    {std::lock_guard lock(mutex_);log_=result.log;}
    need(result.success&&result.log.contains("MR_RUNTIME_CPU_READY"),result.error=="cancelled"?"cancelled":"runtime_probe_failed");
}
bool ModelManager::verifyRuntime(){return start("verify_runtime",[this]{verifyInventory(runtimePath(),runtimeProfile,cancelled_,true);probeRuntime(runtimePath());scan();});}
void ModelManager::setupRuntime() {
    ensureRoot();auto use=lease(runtimeProfile,true);need(!runtimePath().exists(),"already_installed");
    need(requirements_.existsAsFile()&&textHash(requirements_.loadFileAsString().replace("\r\n","\n"))==requirementsHash,"requirements_integrity");
    need(root_.getBytesFreeOnVolume()>6LL*1024*1024*1024,"disk_space");
    Staging stage{root_.getChildFile("runtimes").getChildFile(".part-"+juce::Uuid().toString())};need(CreateDirectoryW(stage.dir.getFullPathName().toWideCharPointer(),nullptr)!=FALSE,"io_error");
    stage.owned=true;
    auto download=[&](const DownloadArtifact& artifact){phase("Downloading processing runtime",artifact.name);const auto file=stage.dir.getChildFile(artifact.name);fetch_(artifact,file,cancelled_,[this](auto n,auto total){std::lock_guard lock(mutex_);status_.completedBytes=n;status_.totalBytes=total;});check(cancelled_);need(plain(file)&&file.getSize()==artifact.bytes&&sha(file)==artifact.sha256,"integrity_failure");return file;};
    const auto zip=download(pythonZip);
    phase("Preparing Python");
    {juce::ZipFile archive(zip);need(archive.getNumEntries()>0,"runtime_archive");juce::int64 extracted{};
     for(int i=0;i<archive.getNumEntries();++i){check(cancelled_);const auto* entry=archive.getEntry(i);need(entry&&leaf(entry->filename)&&entry->uncompressedSize>0&&entry->uncompressedSize<=64*1024*1024,"runtime_archive");extracted+=entry->uncompressedSize;need(extracted<128*1024*1024,"runtime_archive");std::unique_ptr<juce::InputStream> input{archive.createStreamForEntry(i)};const auto file=stage.dir.getChildFile(entry->filename);need(!file.exists(),"runtime_archive");auto output=file.createOutputStream();need(input&&output&&output->writeFromInputStream(*input,entry->uncompressedSize)==entry->uncompressedSize,"io_error");}}
    need(zip.deleteFile(),"io_error");
    need(stage.dir.getChildFile("python311._pth").replaceWithText("python311.zip\n.\nLib/site-packages\nimport site\n",false,false,"\n"),"io_error");
    const auto bootstrap=download(getPip);const auto python=stage.dir.getChildFile("python.exe");
    auto run=[&](const juce::StringArray& args){check(cancelled_);auto result=runLocalProcess(python,args,stage.dir,cancelled_,std::chrono::minutes(20),8);{std::lock_guard lock(mutex_);log_=result.log;}need(result.success,result.error=="cancelled"?"cancelled":"runtime_install_failed");};
    phase("Installing package tools");run({"-I","-B","-u",bootstrap.getFullPathName(),"--isolated","--no-input","--no-compile","--no-cache-dir","--disable-pip-version-check","--only-binary=:all:","--index-url","https://pypi.org/simple","pip==25.0.1","setuptools==78.1.0","wheel==0.45.1"});
    need(bootstrap.deleteFile(),"io_error");
    phase("Installing CPU processing libraries");
    run({"-I","-B","-u","-m","pip","--isolated","install","--no-input","--no-compile","--no-cache-dir","--disable-pip-version-check","--only-binary=:all:","--timeout","15","--retries","1","--index-url","https://download.pytorch.org/whl/cpu","torch==2.8.0","torchvision==0.23.0"});
    const auto requirements=stage.dir.getChildFile("requirements.txt");need(requirements.replaceWithText(requirements_.loadFileAsString().replace("\r\n","\n"),false,false,"\n"),"io_error");
    run({"-I","-B","-u","-m","pip","--isolated","install","--no-input","--no-compile","--no-cache-dir","--disable-pip-version-check","--only-binary=:all:","--timeout","15","--retries","1","--index-url","https://pypi.org/simple","-r",requirements.getFullPathName()});
    run({"-I","-B","-u","-m","pip","check"});probeRuntime(stage.dir);
    auto freeze=runLocalProcess(python,{"-I","-B","-m","pip","freeze"},stage.dir,cancelled_,std::chrono::minutes(1));need(freeze.success&&stage.dir.getChildFile("resolved-packages.txt").replaceWithText(freeze.log),"runtime_probe_failed");
    phase("Verifying processing runtime");auto provenance=object();put(provenance,"requirements_sha256",requirementsHash);put(provenance,"python_url",pythonZip.url);put(provenance,"python_sha256",pythonZip.sha256);put(provenance,"bootstrap_url",getPip.url);put(provenance,"bootstrap_sha256",getPip.sha256);put(provenance,"resolved_packages_sha256",sha(stage.dir.getChildFile("resolved-packages.txt")));put(provenance,"device","cpu");put(provenance,"python_license","PSF-2.0 (LICENSE.txt)");
    inventory(stage.dir,runtimeProfile,provenance,cancelled_);check(cancelled_);need(stage.dir.moveFileTo(runtimePath()),"io_error");stage.dir=juce::File();scan();
}
bool ModelManager::installRuntime(){return start("install_runtime",[this]{setupRuntime();});}
ManagedInstallation ModelManager::acquire(const juce::String& id,Device device)const {
    need(device!=Device::cuda,"unsupported_device");const auto& r=record(id);need(r.unsupportedReason.isEmpty(),"unsupported_model");
    need(plain(root_)&&plain(modelsPath())&&plain(runtimePath().getParentDirectory()),"unsafe_path");
    ManagedInstallation result;result.runtimeLease=lease(runtimeProfile,false);result.modelLease=lease(id,false);
    manifest(runtimePath(),runtimeProfile);manifest(modelsPath().getChildFile(id),id);
    result.runner={runtimePath().getChildFile("python.exe"),worker_,root_.getChildFile("jobs")};result.modelDirectory=modelsPath().getChildFile(id);return result;
}
} // namespace mr::separation
