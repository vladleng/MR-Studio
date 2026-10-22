#include <mrs/desktop.hpp>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace mrs::desktop {
namespace {
bool processed(const Clip& c){return c.stretch&&!stretch::neutral(c.stretch->parameters);}
}
struct Application::StretchJob {
    std::stop_source stop;
    std::atomic<float> progress{};
    std::shared_ptr<IProjectStore> projects;
    std::uint64_t revision{};
    using Entries=std::map<std::string,std::shared_ptr<const audio::StretchCache>>;
    std::future<Entries> result;
};
void Application::set_clip_stretch(const Id& id,stretch::Parameters parameters,bool reset){
    require_not_playing();
    const auto project=services_.projects->state().project;
    const auto clip=std::find_if(project->clips.begin(),project->clips.end(),[&](const auto& c){return c.id==id;});
    if(clip==project->clips.end()||clip->midi)throw std::invalid_argument("Select an audio clip");
    if(!reset&&!stretch::neutral(parameters)){
        const auto source=asset(clip->source);
        if(source->channels>2||(source->sample_rate!=44100&&source->sample_rate!=48000&&source->sample_rate!=96000))
            throw std::invalid_argument("Time / pitch supports mono/stereo at 44.1, 48 or 96 kHz");
    }
    edit(SetAudioStretch{id,parameters,reset},{},true);
    retry_stretch();
}
Sample Application::clip_playback_offset(const Clip& c) const {
    return c.stretch?(processed(c)?audio_output_offset(c):c.source_offset+audio_output_offset(c)):c.source_offset;
}
const audio::Waveform* Application::clip_waveform(const Clip& c) const {
    if(!processed(c))return waveform(c.source);
    const auto source=assets_.find(c.source);if(source==assets_.end())return nullptr;
    const auto entry=stretch_cache_.find(audio::stretch_cache_key(*source->second.data,c));
    return entry==stretch_cache_.end()?nullptr:&entry->second->waveform;
}
std::shared_ptr<const audio::AudioData> Application::clip_audio(const Clip& c,std::shared_ptr<const audio::AudioData> source){
    if(!processed(c))return source;
    const auto entry=stretch_cache_.find(audio::stretch_cache_key(*source,c));
    return entry==stretch_cache_.end()?nullptr:entry->second->data;
}
Application::StretchStatus Application::stretch_status() const {
    auto result=stretch_status_;if(stretch_job_)result.progress=stretch_job_->progress.load();return result;
}
void Application::cancel_stretch(){
    if(stretch_job_){stretch_job_->stop.request_stop();retired_stretch_.push_back(std::move(stretch_job_));}
    stretch_checked_revision_=services_.projects->state().revision;
    stretch_status_.busy=false;stretch_status_.message="Time / pitch cancelled. Apply again to prepare.";++stretch_status_.revision;
}
void Application::retry_stretch(){
    require_not_playing();stretch_checked_revision_=~std::uint64_t{};poll_stretch();
}
bool Application::stretch_ready(bool check_source){
    for(const auto& c:services_.projects->state().project->clips)if(!c.midi&&processed(c)){
        const auto source=asset(c.source);
        const auto entry=stretch_cache_.find(audio::stretch_cache_key(*source,c));
        if(entry==stretch_cache_.end())return false;
        if(check_source&&!entry->second->source_current()){
            // Reject external mutation; reopening/relinking creates a new immutable
            // AudioData identity. Never play old cached PCM against a changed WAV.
            stretch_status_.message="Source WAV changed. Reopen the project before playback.";
            ++stretch_status_.revision;return false;
        }
    }
    return true;
}
void Application::poll_stretch(){
    std::erase_if(retired_stretch_,[](auto& job){
        if(job->result.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
        try{(void)job->result.get();}catch(const std::exception&){}return true;
    });
    const auto state=services_.projects->state();
    if(stretch_job_&&(stretch_job_->projects!=services_.projects||stretch_job_->revision!=state.revision)){
        stretch_job_->stop.request_stop();retired_stretch_.push_back(std::move(stretch_job_));stretch_status_.busy=false;
    }
    if(stretch_job_&&stretch_job_->result.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        auto job=std::move(stretch_job_);stretch_status_.busy=false;++stretch_status_.revision;
        try{
            auto entries=job->result.get();
            for(auto& [key,entry]:entries)stretch_cache_.insert_or_assign(key,std::move(entry));
            stretch_status_.message="Time / pitch ready";stretch_status_.progress=1;
            // Existing device quiescence/read-ahead/PDC lifecycle is reused.
            // Cached PCM has already had algorithmic pre-roll removed.
            rebuild_audio(true);
        }catch(const std::exception& e){stretch_status_.message=std::string("Time / pitch failed: ")+e.what();}
    }
    if(!retired_stretch_.empty()&&stretch_checked_revision_!=state.revision&&!recording()&&engine_->state().playback!=PlaybackState::playing){
        if(!stretch_status_.busy){stretch_status_.busy=true;stretch_status_.message="Finishing cancelled time / pitch task...";++stretch_status_.revision;}
        return;
    }
    if(stretch_job_||!retired_stretch_.empty()||stretch_checked_revision_==state.revision||
       recording()||engine_->state().playback==PlaybackState::playing)return;
    stretch_checked_revision_=state.revision;
    try{
        struct Request {std::string key;std::shared_ptr<const audio::AudioData> source;Clip clip;};
        std::vector<Request> requests;std::set<std::string> needed;std::uintmax_t bytes{};
        for(const auto& c:state.project->clips)if(!c.midi&&processed(c)){
            const auto source=asset(c.source);const auto key=audio::stretch_cache_key(*source,c);
            if(!needed.insert(key).second)continue;
            bytes+=static_cast<std::uintmax_t>(audio_output_extent(c))*source->channels*4;
            if(bytes>512*1024*1024)throw std::invalid_argument("Time / pitch cache exceeds 512 MiB; shorten/split the selected source region");
            if(!stretch_cache_.contains(key))requests.push_back({key,source,c});
        }
        // Undo/redo regenerates evicted entries. Split views share one full region.
        std::erase_if(stretch_cache_,[&](const auto& e){return !needed.contains(e.first);});
        if(requests.empty()){
            const auto message=needed.empty()?(stretch_status_.message.empty()?std::string{}:std::string{"Time / pitch inactive"}):std::string{"Time / pitch ready"};
            if(stretch_status_.busy||stretch_status_.message!=message){stretch_status_.message=message;++stretch_status_.revision;}
            stretch_status_.busy=false;stretch_status_.progress=1;return;
        }
        auto job=std::make_shared<StretchJob>();job->projects=services_.projects;job->revision=state.revision;
        const auto token=job->stop.get_token();auto* progress=&job->progress;
        job->result=std::async(std::launch::async,[requests=std::move(requests),token,progress]{
            StretchJob::Entries entries;
            for(std::size_t i=0;i<requests.size();++i){
                const auto& r=requests[i];
                auto entry=audio::prepare_stretch_cache(r.source,r.clip,token,[&,i](float value){
                    progress->store((static_cast<float>(i)+value)/static_cast<float>(requests.size()));
                });
                entries.emplace(r.key,std::move(entry));
            }
            return entries;
        });
        stretch_job_=std::move(job);stretch_status_.busy=true;stretch_status_.message="Preparing time / pitch...";++stretch_status_.revision;
    }catch(const std::exception& e){stretch_status_.message=std::string("Time / pitch failed: ")+e.what();stretch_status_.busy=false;++stretch_status_.revision;}
}
void Application::stop_stretch(){
    if(stretch_job_)stretch_job_->stop.request_stop();
    for(auto& job:retired_stretch_)job->stop.request_stop();
    if(stretch_job_&&stretch_job_->result.valid())stretch_job_->result.wait();
    for(auto& job:retired_stretch_)if(job->result.valid())job->result.wait();
    stretch_job_.reset();retired_stretch_.clear();stretch_cache_.clear();
    stretch_checked_revision_=~std::uint64_t{};stretch_status_={};
}
} // namespace mrs::desktop
