#include "SeparationProject.h"
#include <mrs/sha256.hpp>
#include <algorithm>
#include <condition_variable>
#include <deque>
#include <fstream>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace mr::separation {
namespace {
namespace fs=std::filesystem;
std::string utf8(const fs::path& p){const auto s=p.generic_u8string();return {s.begin(),s.end()};}
juce::File file(const fs::path& p){const auto s=utf8(p);return juce::File(juce::String::fromUTF8(s.c_str()));}
void need(bool ok,const char* error){if(!ok)throw std::runtime_error(error);}
void check(std::stop_token stop){need(!stop.stop_requested(),"Separation cancelled");}
void plainChain(juce::File dir){
    for(;;){if(dir.exists()){const auto attrs=GetFileAttributesW(dir.getFullPathName().toWideCharPointer());need(attrs!=INVALID_FILE_ATTRIBUTES&&!(attrs&FILE_ATTRIBUTE_REPARSE_POINT),"Separation path contains a reparse point");}
        const auto parent=dir.getParentDirectory();if(parent==dir)break;dir=parent;}
}
// Releasing Undo media never recursively unlinks a directory on the GUI/audio thread.
class CleanupQueue {
public:
    CleanupQueue():thread_([this]{run();}){}
    ~CleanupQueue(){{std::lock_guard lock(mutex_);stopping_=true;}cv_.notify_one();thread_.join();}
    void add(juce::File dir){{std::lock_guard lock(mutex_);files_.push_back(std::move(dir));}cv_.notify_one();}
private:
    void run(){for(;;){juce::File dir;{std::unique_lock lock(mutex_);cv_.wait(lock,[&]{return stopping_||!files_.empty();});if(files_.empty())return;dir=std::move(files_.front());files_.pop_front();}
        try{plainChain(dir);dir.deleteRecursively();}catch(...){} }}
    std::mutex mutex_;std::condition_variable cv_;std::deque<juce::File> files_;bool stopping_{};std::thread thread_;
};
void retire(const juce::File& dir){static CleanupQueue queue;queue.add(dir);}
void copyVerified(const fs::path& from,const fs::path& to,const std::string& hash,std::uint64_t bytes,std::stop_token stop){
    check(stop);plainChain(file(from));need(fs::is_regular_file(from)&&fs::file_size(from)==bytes,"Separation asset is missing or incomplete");
    need(mrs::sha256_file(from,stop)==hash,"Separation asset checksum mismatch");
    std::ifstream input(from,std::ios::binary);need(!fs::exists(to),"Separation staging collision");std::ofstream output(to,std::ios::binary);need(input&&output,"Cannot copy separation asset (disk full?)");
    std::array<char,65536> buffer{};std::uint64_t written{};
    while(input){check(stop);input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));output.write(buffer.data(),input.gcount());need(static_cast<bool>(output),"Cannot write separation asset (disk full?)");written+=static_cast<std::uint64_t>(input.gcount());}
    need(input.eof()&&written==bytes,"Separation asset changed during copy");output.close();need(static_cast<bool>(output)&&mrs::sha256_file(to,stop)==hash,"Separation copy checksum mismatch");
}
std::shared_ptr<ProjectResult> deliver(const ProjectInput& input,Result raw,std::stop_token stop){
    const auto work=raw.directory.getParentDirectory();
    need(raw.directory.getFileName()=="result"&&work.getFileName()=="mrs-separation-"+raw.jobId,"Invalid separation staging owner");
    auto original=std::make_shared<ProjectMedia>();original->directory=work;
    check(stop);
    const auto& manifest=raw.manifest;
    need(manifest["model"].toString()==input.model&&manifest["source_start"].toString().getLargeIntValue()==input.clip.source_offset&&manifest["source_frames"].toString().getLargeIntValue()==input.clip.length&&manifest["project_rate"].toString().getLargeIntValue()==input.rate,"Separation source geometry mismatch");
    auto prepared=std::make_shared<ProjectResult>();prepared->expected=input.clip;
    auto& r=prepared->record;r.id={raw.jobId.toStdString()};r.source_clip=input.clip.id;r.source=input.clip.source;
    r.source_sha256=manifest["source_sha256"].toString().toStdString();r.model=input.model.toStdString();r.model_version=modelProfile(input.model).version.toStdString();
    r.source_offset=input.clip.source_offset;r.source_frames=input.clip.length;r.start=input.clip.start;r.frames=input.clip.length;r.fade_in=input.clip.fade_in;r.fade_out=input.clip.fade_out;
    r.source_rate=static_cast<std::uint32_t>(manifest["source_rate"].toString().getLargeIntValue());r.sample_rate=input.rate;
    const auto manifestSource=raw.directory.getChildFile("manifest.json");r.provenance=manifestSource.loadFileAsString().toStdString();
    need(r.provenance.size()<=65536&&juce::JSON::toString(juce::JSON::parse(manifestSource),true)==juce::JSON::toString(manifest,true),"Invalid separation manifest");
    const bool persistent=!input.projectFile.empty();
    fs::path parent;
    if(persistent){auto area=input.projectFile.stem();area+=".assets";parent=input.projectFile.parent_path()/area/"separation";}
    else parent=fs::temp_directory_path()/"MoonRiverStudio-Separation";
    plainChain(file(parent));fs::create_directories(parent);plainChain(file(parent));
    const auto stage=parent/(".part-"+r.id.value),destination=parent/r.id.value;
    need(!fs::exists(destination)&&fs::create_directory(stage),"Separation destination already exists");
    auto media=std::make_shared<ProjectMedia>();media->directory=file(stage);media->persistent=persistent;
    const auto key=[&](const fs::path& p){return utf8(persistent?p.lexically_relative(input.projectFile.parent_path()):p);};
    const auto manifestPath=fs::path(manifestSource.getFullPathName().toWideCharPointer());
    r.manifest={"manifest",key(destination/"manifest.json"),mrs::sha256_file(manifestPath,stop),static_cast<std::uint64_t>(manifestSource.getSize())};
    const auto* outputs=manifest["outputs"].getArray();need(outputs&&outputs->size()==selectedOutputs(modelProfile(input.model),input.selectedStems).size(),"Invalid separation outputs");
    for(const auto& label:selectedOutputs(modelProfile(input.model),input.selectedStems)){
        const auto role=label.toLowerCase();
        bool found{};for(const auto& out:*outputs)if(out["stem"].toString()==role){
            need(!found&&out["path"].toString()==juce::String(role)+".wav"&&out["frames"].toString().getLargeIntValue()==r.frames,"Invalid separation stem");found=true;
            r.stems.push_back({label.toStdString(),key(destination/(role.toStdString()+".wav")),out["sha256"].toString().toStdString(),static_cast<std::uint64_t>(out["bytes"].toString().getLargeIntValue())});}
        need(found,"Missing separation stem");
    }
    std::uint64_t bytes=r.manifest.bytes;for(const auto& stem:r.stems)bytes+=stem.bytes;need(fs::space(parent).available>bytes+16*1024*1024,"Not enough disk space for separation assets");
    copyVerified(manifestPath,stage/"manifest.json",r.manifest.sha256,r.manifest.bytes,stop);
    for(const auto& stem:r.stems){const auto name=juce::String::fromUTF8(stem.role.c_str()).toLowerCase()+".wav";copyVerified(fs::path(raw.directory.getChildFile(name).getFullPathName().toWideCharPointer()),stage/name.toStdString(),stem.sha256,stem.bytes,stop);}
    check(stop);need(!fs::exists(destination)&&media->directory.moveFileTo(file(destination)),"Cannot publish separation assets");media->directory=file(destination);
    prepared->media=std::move(media);return prepared;
}
}
ProjectMedia::~ProjectMedia(){if(directory!=juce::File()&&(!persistent||!accepted.load()))try{retire(directory);}catch(...){} }
ProjectJob::ProjectJob(std::shared_ptr<ModelManager> manager,Processor processor,std::shared_ptr<ModelManager> nativeManager):manager_(std::move(manager)),nativeManager_(std::move(nativeManager)),processor_(std::move(processor)){}
ProjectJob::~ProjectJob(){cancel();if(thread_.joinable())thread_.join();}
Status ProjectJob::status() const{std::lock_guard lock(mutex_);return status_;}
void ProjectJob::phase(juce::String value){std::lock_guard lock(mutex_);status_.phase=std::move(value);}
void ProjectJob::cancel(){stop_.request_stop();std::lock_guard lock(mutex_);if(status_.state==State::ready){outcome_.reset();status_.state=State::cancelled;status_.phase="Cancelled";}}
std::optional<ProjectOutcome> ProjectJob::take(){std::lock_guard lock(mutex_);if(status_.state!=State::ready)return {};auto out=std::move(outcome_);outcome_.reset();status_.state=State::idle;return out;}
bool ProjectJob::begin(std::function<ProjectOutcome(std::stop_token)> action){
    {std::lock_guard lock(mutex_);if(status_.state==State::running)return false;}
    if(thread_.joinable())thread_.join();stop_=std::stop_source{};
    {std::lock_guard lock(mutex_);outcome_.reset();status_={State::running,{},"Starting",{},"cpu"};}
    try{thread_=std::thread([this,action=std::move(action),stop=stop_.get_token()]{
        try{auto out=action(stop);std::lock_guard lock(mutex_);check(stop);outcome_=std::move(out);status_.state=State::ready;status_.phase="Ready";}
        catch(const std::exception& e){std::lock_guard lock(mutex_);status_.state=stop.stop_requested()?State::cancelled:State::failed;status_.phase=stop.stop_requested()?"Cancelled":"Failed";status_.error=juce::String::fromUTF8(e.what());}
    });}catch(...){std::lock_guard lock(mutex_);status_.state=State::failed;status_.error="Cannot start separation service";return false;}return true;
}
Result ProjectJob::process(const ProjectInput& input,std::stop_token stop){
    if(processor_)return processor_(input,stop);
    const auto manager=input.backend==Backend::native?nativeManager_:manager_;
    need(manager!=nullptr,"The selected processing backend is unavailable");
    need(manager->backend()==input.backend,"Processing backend mismatch");
    phase("Checking source and installed model");check(stop);need(!input.clip.midi&&!input.clip.stretch,"Reset Time / Pitch before separating");plainChain(file(input.source));
    const auto info=mrs::audio::inspect_wav(input.source);
    need((info.channels==1||info.channels==2)&&info.sample_rate==input.rate,"Separation requires a mono/stereo WAV at the project sample rate");
    need(input.rate==44100||input.rate==48000||input.rate==88200||input.rate==96000,"Separation supports 44.1, 48, 88.2 or 96 kHz projects");
    need(input.clip.length<=static_cast<mrs::Sample>(input.rate)*600,"Select a range of up to 10 minutes");
    need(input.clip.source_offset>=0&&input.clip.length>0&&input.clip.source_offset<=info.frame_count&&input.clip.length<=info.frame_count-input.clip.source_offset,"Source range is missing or changed");
    auto installation=manager->acquire(input.model,Device::cpu);
    Runner runner(installation.runner);Request request;request.source=file(input.source);request.modelDirectory=installation.modelDirectory;
    request.timeout=std::chrono::seconds(std::clamp<mrs::Sample>(input.clip.length/input.rate*30+600,1800,21600));
    request.selectedStems=input.selectedStems;request.modelId=input.model;request.chunk=input.chunk;request.sessionId=input.session;request.sourceRevision=input.revision;request.sourceStart=input.clip.source_offset;request.sourceFrames=input.clip.length;request.timelineStart=input.clip.start;request.projectRate=static_cast<int>(input.rate);
    need(runner.start(std::move(request)),"Separation service is busy");
    for(;;){const auto s=runner.status();{std::lock_guard lock(mutex_);status_.phase=s.phase;status_.completed=s.completed;status_.total=s.total;status_.currentPass=s.currentPass;status_.passes=s.passes;}if(s.state!=State::running){if(s.state==State::ready){auto result=runner.take();need(result.has_value(),"Missing separation result");return std::move(*result);}throw std::runtime_error(s.error.toStdString());}
        if(stop.stop_requested())runner.cancel();std::this_thread::sleep_for(std::chrono::milliseconds(20));}
}
bool ProjectJob::separate(ProjectInput input){return begin([this,input=std::move(input)](std::stop_token stop){auto raw=process(input,stop);phase("Verifying and keeping project assets");return ProjectOutcome{deliver(input,std::move(raw),stop),{}};});}
bool ProjectJob::preparePlacement(mrs::SeparationResult expected,std::vector<fs::path> paths,std::shared_ptr<void> guard){
    return begin([this,expected=std::move(expected),paths=std::move(paths),guard=std::move(guard)](std::stop_token stop){
        phase("Checking result files");need(paths.size()==expected.stems.size()+1,"Missing separation file references");ProjectPlacement prepared;prepared.expected=expected;
        plainChain(file(paths.back()));need(fs::is_regular_file(paths.back())&&fs::file_size(paths.back())==expected.manifest.bytes&&mrs::sha256_file(paths.back(),stop)==expected.manifest.sha256,"Separation manifest is missing or changed; restore it or separate again");
        for(std::size_t i=0;i<expected.stems.size();++i){check(stop);plainChain(file(paths[i]));need(fs::is_regular_file(paths[i])&&fs::file_size(paths[i])==expected.stems[i].bytes&&mrs::sha256_file(paths[i],stop)==expected.stems[i].sha256,"Separation asset is missing or changed; restore it or separate again");
            auto data=mrs::audio::open_wav(paths[i],0);need(data.frames()==expected.frames&&data.sample_rate==expected.sample_rate&&data.channels==2,"Separation asset geometry mismatch");
            auto wave=std::make_shared<mrs::audio::WavFile>(*data.file);wave->lifetime=guard;data.file=std::move(wave);prepared.audio.push_back(std::make_shared<const mrs::audio::AudioData>(std::move(data)));}
        return ProjectOutcome{{},std::move(prepared)};
    });
}
} // namespace mr::separation
