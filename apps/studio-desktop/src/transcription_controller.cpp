#include <mrs/transcription_controller.hpp>
#include <algorithm>
#include <atomic>
#include <stdexcept>

namespace mrs::transcription {
namespace {
void need(bool b,const char* message){if(!b)throw std::invalid_argument(message);}
}
struct Controller::Job {
    std::stop_source stop;
    std::atomic<std::uint64_t> progress{};
    std::future<Result> result;
    bool obsolete{};
};
Controller::Controller()=default;
Controller::~Controller(){shutdown();}
Controller::Stamp Controller::stamp(const std::filesystem::path& path){return {std::filesystem::file_size(path),std::filesystem::last_write_time(path)};}
bool Controller::current(const std::shared_ptr<IProjectStore>& store,const std::filesystem::path& path)const{
    if(!store||owner_.lock()!=store||project_path_!=path)return false;
    const auto now=store->state();
    if(now.revision!=captured_.revision||now.project->id!=captured_.project->id||*now.project!=*captured_.project)return false;
    try{return stamp(source_)==stamp_;}catch(...){return false;}
}
void Controller::start(const std::shared_ptr<IProjectStore>& store,std::filesystem::path path,Id clip,
                       std::filesystem::path source,ModelStore models,Request request,std::shared_ptr<const void> media){
    need(!job_&&!result_,"A transcription job/result is already pending");need(store!=nullptr,"Missing project");
    auto capture=store->state();capture.project->validate();
    const auto at=std::find_if(capture.project->clips.begin(),capture.project->clips.end(),[&](const auto& c){return c.id==clip;});
    need(at!=capture.project->clips.end()&&!at->midi,"Choose an audio clip");
    need(!at->stretch&&std::none_of(capture.project->ara_bindings.begin(),capture.project->ara_bindings.end(),[&](const auto& b){return b.source==at->source;}),"Render stretched/ARA audio before transcription");
    const auto source_stamp=stamp(source);request.source=source;request.project_id=capture.project->id;request.clip_id=clip;request.project_revision=capture.revision;
    auto job=std::make_unique<Job>();
    job->result=std::async(std::launch::async,[models=std::move(models),request=std::move(request),media=std::move(media),
        stop=job->stop.get_token(),progress=&job->progress]()mutable{
        (void)media; // lifetime held through native process termination/cleanup
        try{
            if(stop.stop_requested()){Result r;r.state=State::cancelled;return r;}
            const auto lease=models.acquire();lease.configure(request);
            return run(request,stop,[progress](int completed,int total){
                progress->store((static_cast<std::uint64_t>(total)<<32)|static_cast<std::uint32_t>(completed),std::memory_order_relaxed);
            });
        }catch(const std::exception& e){Result r;r.state=stop.stop_requested()?State::cancelled:State::failed;r.error=e.what();return r;}
    });
    owner_=store;captured_=std::move(capture);project_path_=std::move(path);clip_=std::move(clip);source_=std::move(source);stamp_=source_stamp;
    status_={JobState::running,0,0,{}};job_=std::move(job);
}
void Controller::poll(const std::shared_ptr<IProjectStore>& store,const std::filesystem::path& path){
    if(result_&&!current(store,path)){result_.reset();status_={JobState::stale,0,0,"Project/source changed; run transcription again"};}
    if(!job_)return;
    const auto progress=job_->progress.load(std::memory_order_relaxed);status_.completed=static_cast<int>(progress&0xffffffffU);status_.total=static_cast<int>(progress>>32);
    if(!job_->stop.stop_requested()&&!current(store,path)){job_->obsolete=true;job_->stop.request_stop();status_.state=JobState::cancelling;}
    if(job_->result.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
    auto job=std::move(job_);auto result=job->result.get();
    const auto final_progress=job->progress.load(std::memory_order_relaxed);status_.completed=static_cast<int>(final_progress&0xffffffffU);status_.total=static_cast<int>(final_progress>>32);
    if(job->obsolete){status_={JobState::stale,0,0,"Project/source changed; run transcription again"};return;}
    if(job->stop.stop_requested()||result.state==State::cancelled){status_={JobState::cancelled,0,0,{}};return;}
    if(!current(store,path)){status_={JobState::stale,0,0,"Project/source changed; run transcription again"};return;}
    if(result.state==State::ready||result.state==State::empty){
        need(result.project_id==captured_.project->id&&result.clip_id==clip_&&result.project_revision==captured_.revision,"Worker identity mismatch");
        const auto& clips=captured_.project->clips;const auto at=std::find_if(clips.begin(),clips.end(),[&](const auto& c){return c.id==clip_;});
        need(at!=clips.end(),"Captured audio clip missing");
        const double first=static_cast<double>(at->source_offset)/captured_.project->sample_rate;
        const double last=static_cast<double>(at->source_offset+at->length)/captured_.project->sample_rate;
        for(auto& note:result.notes){note.start=std::max(note.start,first);note.end=std::min(note.end,last);}
        std::erase_if(result.notes,[](const auto& n){return n.end<=n.start;});result.state=result.notes.empty()?State::empty:State::ready;
        status_.state=result.state==State::ready?JobState::ready:JobState::empty;status_.error.clear();result_=std::move(result);
    }else{status_.state=result.state==State::timeout?JobState::timeout:JobState::failed;status_.error=std::move(result.error);}
}
void Controller::cancel(){
    result_.reset();if(job_){job_->stop.request_stop();status_.state=JobState::cancelling;status_.error.clear();}
    else status_={JobState::cancelled,0,0,{}};
}
void Controller::shutdown(){
    if(job_){job_->stop.request_stop();if(job_->result.valid())job_->result.wait();job_.reset();}result_.reset();status_={JobState::cancelled,0,0,{}};
}
JobStatus Controller::status()const{
    auto value=status_;if(job_){const auto n=job_->progress.load(std::memory_order_relaxed);value.completed=static_cast<int>(n&0xffffffffU);value.total=static_cast<int>(n>>32);}return value;
}
const Result* Controller::preview()const{return result_?&*result_:nullptr;}
PlaceTranscription Controller::prepare(const std::shared_ptr<IProjectStore>& store,const std::filesystem::path& path,
                                      const std::optional<std::vector<std::string>>& roles){
    poll(store,path);need(status_.state==JobState::ready&&result_.has_value(),"No current transcription result");
    std::vector<TranscribedNote> notes;
    if(roles){
        need(!roles->empty(),"Select at least one recognized part");std::vector<std::string> seen;
        for(const auto& role:*roles){need(std::find(seen.begin(),seen.end(),role)==seen.end()&&std::any_of(result_->notes.begin(),result_->notes.end(),[&](const auto& n){return n.instrument==role;}),"Invalid/duplicate transcription part");seen.push_back(role);}
    }
    for(const auto& n:result_->notes)if(!roles||std::find(roles->begin(),roles->end(),n.instrument)!=roles->end())notes.push_back(n);
    return PlaceTranscription(captured_.project,clip_,notes);
}
void Controller::placed(){need(status_.state==JobState::ready&&result_.has_value(),"No placed transcription result");result_.reset();status_.state=JobState::placed;}
}
