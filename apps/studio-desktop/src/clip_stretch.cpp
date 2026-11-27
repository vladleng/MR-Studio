#include <mrs/desktop.hpp>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <mutex>
#include <thread>
#include <utility>

namespace mrs::desktop {
namespace {
bool processed(const Clip& c){return c.stretch&&!stretch::neutral(c.stretch->parameters);}
}
struct Application::StretchJob {
    std::stop_source stop;
    std::atomic<float> progress{};
    std::atomic<bool> checkingCache{};
    unsigned workers{1};
    std::shared_ptr<IProjectStore> projects;
    std::uint64_t revision{};
    using Entries=std::map<std::string,std::shared_ptr<const audio::StretchCache>>;
    std::future<Entries> result;
};
stretch::Backend Application::clip_stretch_backend(const Clip& c) const {
    for(const auto& t:services_.projects->state().project->tracks)if(t.id==c.track)return t.stretch_backend;
    throw std::invalid_argument("Missing clip channel");
}
void Application::set_track_stretch_backend(const Id& id,stretch::Backend backend){
    require_not_playing();edit(SetTrackStretchBackend{id,backend},{},true);retry_stretch();
}
void Application::set_clip_stretch(const Id& id,stretch::Parameters parameters,bool reset,StretchAnchor anchor){
    require_not_playing();
    const auto project=services_.projects->state().project;
    const auto clip=std::find_if(project->clips.begin(),project->clips.end(),[&](const auto& c){return c.id==id;});
    if(clip==project->clips.end()||clip->midi)throw std::invalid_argument("Select an audio clip");
    if(!reset&&!stretch::neutral(parameters)){
        const auto source=asset(clip->source);
        if(source->channels>2||(source->sample_rate!=44100&&source->sample_rate!=48000&&source->sample_rate!=96000))
            throw std::invalid_argument("Time / pitch supports mono/stereo at 44.1, 48 or 96 kHz");
    }
    edit(SetAudioStretch{id,parameters,reset,anchor},{},true);
    retry_stretch();
}
Sample Application::clip_playback_offset(const Clip& c) const {
    return c.stretch?(processed(c)?audio_output_offset(c):c.source_offset+audio_output_offset(c)):c.source_offset;
}
const audio::Waveform* Application::clip_waveform(const Clip& c) const {
    if(!processed(c))return waveform(c.source);
    const auto source=assets_.find(c.source);if(source==assets_.end())return nullptr;
    const auto entry=stretch_cache_.find(audio::stretch_cache_key(*source->second.data,c,clip_stretch_backend(c)));
    return entry==stretch_cache_.end()?nullptr:&entry->second->waveform;
}
std::shared_ptr<const audio::AudioData> Application::clip_audio(const Clip& c,std::shared_ptr<const audio::AudioData> source){
    if(!processed(c))return source;
    const auto entry=stretch_cache_.find(audio::stretch_cache_key(*source,c,clip_stretch_backend(c)));
    return entry==stretch_cache_.end()?nullptr:entry->second->data;
}
Application::StretchStatus Application::stretch_status() const {
    auto result=stretch_status_;if(stretch_job_){result.progress=stretch_job_->progress.load();result.message=stretch_job_->checkingCache.load()?"Checking Time / Pitch cache":"Preparing Time / Pitch ("+std::to_string(stretch_job_->workers)+" workers max)";}return result;
}
void Application::cancel_stretch(){
    if(stretch_job_){stretch_job_->stop.request_stop();retired_stretch_.push_back(std::move(stretch_job_));}
    stretch_checked_revision_=services_.projects->state().revision;
    stretch_status_.busy=false;stretch_status_.message="Time / pitch cancelled. Apply again to prepare.";++stretch_status_.revision;
}
unsigned Application::stretch_worker_limit(unsigned requested,unsigned hardware) noexcept {
    hardware=std::max(1u,hardware);
    return std::min({requested?requested:std::min(4u,std::max(1u,hardware/2)),hardware,16u});
}
void Application::set_stretch_workers(unsigned workers){
    if(workers>16)throw std::invalid_argument("Time / Pitch workers must be Auto or 1–16");
    // A running batch keeps its captured policy; subsequent batches use this.
    stretch_workers_=workers;
}
void Application::retry_stretch(){
    require_not_playing();stretch_checked_revision_=~std::uint64_t{};poll_stretch();
}
bool Application::stretch_ready(bool check_source){
    for(const auto& c:services_.projects->state().project->clips)if(!c.midi&&processed(c)){
        if(stretch_publication_failed_)return false;
        const auto source=asset(c.source);
        const auto entry=stretch_cache_.find(audio::stretch_cache_key(*source,c,clip_stretch_backend(c)));
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
    // A callback may be publishing a coherent transport snapshot. Retry on
    // the next UI poll; do not turn temporary contention into a render error.
    audio::RealtimeState head;
    if(!engine_->try_state(head))return;
    const auto state=services_.projects->state();
    if(stretch_job_&&(stretch_job_->projects!=services_.projects||stretch_job_->revision!=state.revision)){
        stretch_job_->stop.request_stop();retired_stretch_.push_back(std::move(stretch_job_));stretch_status_.busy=false;
    }
    if(stretch_job_&&stretch_job_->result.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        auto job=std::move(stretch_job_);stretch_status_.busy=false;++stretch_status_.revision;
        try{
            auto entries=job->result.get();
            for(auto& [key,entry]:entries)stretch_cache_.insert_or_assign(key,std::move(entry));
            // A complete cache is not a complete playback graph. Keep playback
            // blocked until the stopped voice publication succeeds.
            stretch_publication_failed_=true;
            if(!refresh_clip_audio(*state.project))rebuild_audio(true);
            stretch_publication_failed_=false;
            stretch_status_.message="Time / pitch ready";stretch_status_.progress=1;
        }catch(const std::exception& e){stretch_status_.message=std::string("Time / pitch failed: ")+e.what();}
    }
    if(!retired_stretch_.empty()&&stretch_checked_revision_!=state.revision&&!recording()&&head.playback!=PlaybackState::playing){
        if(!stretch_status_.busy){stretch_status_.busy=true;stretch_status_.message="Finishing cancelled time / pitch task...";++stretch_status_.revision;}
        return;
    }
    if(stretch_job_||!retired_stretch_.empty()||stretch_checked_revision_==state.revision||
       recording()||head.playback==PlaybackState::playing)return;
    stretch_checked_revision_=state.revision;
    try{
        struct Request {std::string key;std::shared_ptr<const audio::AudioData> source;Clip clip;stretch::Backend backend;};
        std::vector<Request> requests;std::set<std::string> needed;
        for(const auto& c:state.project->clips)if(!c.midi&&processed(c)){
            const auto source=asset(c.source);const auto key=audio::stretch_cache_key(*source,c,clip_stretch_backend(c));
            if(!needed.insert(key).second)continue;
            const auto bytes=static_cast<std::uintmax_t>(audio_output_extent(c))*source->channels*4;
            if(bytes>512ull*1024*1024)throw std::invalid_argument("One Time / pitch cache entry exceeds 512 MiB; split the source region");
            if(!stretch_cache_.contains(key)){
                requests.push_back({key,source,c,clip_stretch_backend(c)});
            }
        }
        // Active entries must remain available for playback, regardless of the
        // total streamed PCM size. Limit only unused/warm Undo entries.
        const auto tick=++stretch_cache_clock_;
        std::erase_if(stretch_cache_used_,[&](const auto& item){return !needed.contains(item.first)&&!stretch_cache_.contains(item.first);});
        for(const auto& key:needed)stretch_cache_used_[key]=tick;
        std::uintmax_t warmBytes{};std::size_t warmCount{};
        for(const auto& [key,entry]:stretch_cache_)if(!needed.contains(key)){warmBytes+=entry->bytes;++warmCount;}
        while(warmBytes>512ull*1024*1024||warmCount>64){
            auto victim=stretch_cache_.end();
            for(auto it=stretch_cache_.begin();it!=stretch_cache_.end();++it)if(!needed.contains(it->first)&&
               (victim==stretch_cache_.end()||stretch_cache_used_[it->first]<stretch_cache_used_[victim->first]))victim=it;
            if(victim==stretch_cache_.end())break;
            warmBytes-=victim->second->bytes;--warmCount;stretch_cache_used_.erase(victim->first);stretch_cache_.erase(victim);
        }
        if(requests.empty()){
            if(needed.empty())stretch_publication_failed_=false;
            else if(stretch_publication_failed_){
                // Explicit retry or an edit may fix the graph without requiring
                // another DSP render. A failed retry remains a failed state.
                if(!refresh_clip_audio(*state.project))rebuild_audio(true);
                stretch_publication_failed_=false;
            }
            const auto message=needed.empty()?(stretch_status_.message.empty()?std::string{}:std::string{"Time / pitch inactive"}):std::string{"Time / pitch ready"};
            if(stretch_status_.busy||stretch_status_.message!=message){stretch_status_.message=message;++stretch_status_.revision;}
            stretch_status_.busy=false;stretch_status_.progress=1;return;
        }
        const auto cacheRoot=path_.extension()==".mrsproject"?path_.parent_path():std::filesystem::path{};
        auto job=std::make_shared<StretchJob>();job->projects=services_.projects;job->revision=state.revision;
        auto stop=job->stop;auto* progress=&job->progress;auto* checking=&job->checkingCache;
        const auto workers=std::min<unsigned>(stretch_worker_limit(stretch_workers_,std::thread::hardware_concurrency()),static_cast<unsigned>(requests.size()));
        job->workers=workers;
        checking->store(!cacheRoot.empty());
        job->result=std::async(std::launch::async,[requests=std::move(requests),cacheRoot,stop,progress,checking,workers]() mutable {
            const auto token=stop.get_token();
            StretchJob::Entries entries;
            // Fingerprints are immutable before workers start. Identical
            // content with different source instances may address one disk
            // cache file; serialize that rare group to avoid competing writes.
            std::map<const audio::AudioData*,audio::StretchSourceFingerprint> fingerprints;
            std::map<std::string,unsigned> digestCounts;
            if(!cacheRoot.empty())for(const auto& r:requests){
                if(fingerprints.contains(r.source.get()))continue;
                auto fp=audio::fingerprint_stretch_source(*r.source,token);
                ++digestCounts[fp.digest];fingerprints.emplace(r.source.get(),std::move(fp));
            }
            std::map<std::string,std::mutex> duplicateLocks;
            for(const auto& [digest,count]:digestCounts)if(count>1)duplicateLocks.try_emplace(digest);
            std::vector<std::size_t> parallel,drums;
            for(std::size_t i=0;i<requests.size();++i)
                (requests[i].backend==stretch::Backend::drums?drums:parallel).push_back(i);
            std::stable_sort(parallel.begin(),parallel.end(),[&](auto a,auto b){
                return audio_output_extent(requests[a].clip)>audio_output_extent(requests[b].clip);
            });
            std::vector<std::shared_ptr<const audio::StretchCache>> results(requests.size());
            std::vector<float> fractions(requests.size());
            std::mutex reportMutex,errorMutex;
            std::exception_ptr error;
            bool rendering=cacheRoot.empty();
            auto execute=[&](std::size_t i){
                if(token.stop_requested())return;
                const auto& r=requests[i];
                const audio::StretchSourceFingerprint* fp=nullptr;
                std::unique_lock<std::mutex> sameContent;
                if(!cacheRoot.empty()){
                    fp=&std::as_const(fingerprints).at(r.source.get());
                    if(auto lock=duplicateLocks.find(fp->digest);lock!=duplicateLocks.end())
                        sameContent=std::unique_lock<std::mutex>(lock->second);
                }
                if(token.stop_requested())return;
                results[i]=audio::prepare_stretch_cache(r.source,r.clip,token,[&,i](float value){
                    std::lock_guard guard(reportMutex);
                    if(value<1)rendering=true;
                    checking->store(!rendering);
                    fractions[i]=std::max(fractions[i],value);
                    double total{};for(float fraction:fractions)total+=fraction;
                    progress->store(static_cast<float>(total/fractions.size()));
                },r.backend,cacheRoot,fp);
            };
            auto fail=[&]{
                {std::lock_guard guard(errorMutex);if(!error)error=std::current_exception();}
                stop.request_stop();
            };
            std::atomic<std::size_t> next{};
            auto run=[&]{
                try{
                    while(!token.stop_requested()){
                        const auto at=next.fetch_add(1);
                        if(at>=parallel.size())return;
                        execute(parallel[at]);
                    }
                }catch(...){fail();}
            };
            {
                // The coordinator participates; jthread scope joins every
                // child before result publication or exception propagation.
                std::vector<std::jthread> pool;
                const auto count=std::min<std::size_t>(workers,parallel.size());
                try{for(std::size_t i=1;i<count;++i)pool.emplace_back(run);if(count)run();}
                catch(...){fail();}
            }
            if(error)std::rethrow_exception(error);
            // Drums uses large whole-source arrays: keep its existing
            // sequential memory footprint, after all Signalsmith work joins.
            for(auto i:drums){if(token.stop_requested())break;execute(i);}
            if(token.stop_requested())throw std::runtime_error("Time / pitch preparation cancelled");
            for(std::size_t i=0;i<requests.size();++i){
                if(!results[i])throw std::runtime_error("Missing Time / pitch worker result");
                entries.emplace(requests[i].key,std::move(results[i]));
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
    stretch_job_.reset();retired_stretch_.clear();stretch_cache_.clear();stretch_cache_used_.clear();
    stretch_checked_revision_=~std::uint64_t{};stretch_status_={};stretch_publication_failed_=false;
}
} // namespace mrs::desktop
