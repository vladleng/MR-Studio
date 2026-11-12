#include <mrs/transcription_controller.hpp>
#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <cmath>
#include <sstream>

namespace mrs::transcription {
namespace {
void need(bool b,const char* message){if(!b)throw std::invalid_argument(message);}
double tempo_at(const TimeMap& map,Tick tick){double bpm=map.tempos.front().bpm;for(const auto& p:map.tempos){if(p.tick>tick)break;bpm=p.bpm;}return bpm;}
TimeMap imported_map(const Project& project,const Clip& clip,const Result& result){
    need(result.detected_bpm>0,"No BPM available to import; enter BPM or enable detection");
    const Timeline old(project.time,project.sample_rate);const auto anchor=old.to_ticks(clip.start),old_end=old.to_ticks(clip.start+clip.length);
    TimeMap map=project.time;std::erase_if(map.tempos,[&](const auto& p){return p.tick>=anchor;});
    double previous_seconds=0,previous_bpm=result.detected_bpm,ticks=static_cast<double>(anchor);
    const auto append=[&](Tick tick,double bpm){need(std::isfinite(bpm)&&bpm>=1&&bpm<=999,"Detected tempo outside project limits");if(!map.tempos.empty()&&map.tempos.back().tick==tick)map.tempos.back().bpm=bpm;else map.tempos.push_back({tick,bpm});};
    append(anchor,previous_bpm);
    if(result.musical.tempo_map)for(std::size_t i=0;i+1<result.beats.size();++i){
        const auto seconds=result.beats[i];if(seconds>=result.analysis_seconds)break;
        const auto bpm=60./(result.beats[i+1]-seconds);ticks+=(seconds-previous_seconds)*previous_bpm*ppq/60.;append(static_cast<Tick>(std::llround(ticks)),bpm);previous_seconds=seconds;previous_bpm=bpm;
    }
    ticks+=(result.analysis_seconds-previous_seconds)*previous_bpm*ppq/60.;const auto end=static_cast<Tick>(std::llround(ticks));
    if(end>anchor)append(end,tempo_at(project.time,old_end));
    for(const auto& p:project.time.tempos)if(p.tick>old_end)map.tempos.push_back({p.tick+(end-old_end),p.bpm});
    if(result.detected_meter){const auto pos=Timeline(map,project.sample_rate).musical_position(Timeline(map,project.sample_rate).to_ticks(clip.start+static_cast<Sample>(std::llround(result.first_downbeat*project.sample_rate))));need(pos.beat==1&&pos.tick==0,"Align the first downbeat with a project bar boundary before importing meter");
        std::erase_if(map.meters,[&](const auto& p){return p.bar==pos.bar;});map.meters.push_back({pos.bar,static_cast<int>(result.detected_meter),4});std::sort(map.meters.begin(),map.meters.end(),[](const auto& a,const auto& b){return a.bar<b.bar;});}
    map.validate();return map;
}

}
struct Controller::Job {
    std::stop_source stop;
    std::atomic<std::uint64_t> progress{};
    std::atomic<Phase> phase{Phase::checking_files};
    std::future<Result> result;
    bool obsolete{};
    std::chrono::steady_clock::time_point began{std::chrono::steady_clock::now()};
    unsigned cpu_threads{};
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
    const auto placement=request.placement;
    need(placement.range_offset>=0&&placement.range_offset<at->length,"Invalid selected audio range");
    const auto frames=placement.range_length.value_or(at->length-placement.range_offset);
    need(frames>0&&frames<=at->length-placement.range_offset,"Invalid selected audio range");
    if(placement.target_track){
        need(request.instruments.size()==1,"Choose exactly one stem instrument");
        need(std::any_of(capture.project->tracks.begin(),capture.project->tracks.end(),[&](const auto& t){return t.id==*placement.target_track&&t.kind==TrackKind::instrument;}),"Choose an existing instrument track");
    }
    const auto instruments=request.instruments;
    const auto source_stamp=stamp(source);request.range=SourceRange{at->source_offset+placement.range_offset,frames,capture.project->sample_rate};request.source=source;request.project_id=capture.project->id;request.clip_id=clip;request.project_revision=capture.revision;
    auto job=std::make_unique<Job>();job->cpu_threads=choose_cpu_threads(request.cpu_threads);request.cpu_threads=job->cpu_threads;
    job->result=std::async(std::launch::async,[models=std::move(models),request=std::move(request),media=std::move(media),
        stop=job->stop.get_token(),progress=&job->progress,phase=&job->phase]()mutable{
        (void)media; // lifetime held through worker process termination/cleanup
        try{
            if(stop.stop_requested()){Result r;r.state=State::cancelled;return r;}
            const auto lease=models.acquire();lease.configure(request);
            return run(request,stop,[progress](int completed,int total){
                progress->store((static_cast<std::uint64_t>(total)<<32)|static_cast<std::uint32_t>(completed),std::memory_order_relaxed);
            },[phase](Phase value){phase->store(value,std::memory_order_relaxed);});
        }catch(const std::exception& e){Result r;r.state=stop.stop_requested()?State::cancelled:State::failed;r.error=e.what();return r;}
    });
    placement_=placement;instruments_=instruments;
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
    status_.diagnostics=result.diagnostics;status_.phase=result.phase;
    if(result.state==State::ready||result.state==State::empty){
        need(result.project_id==captured_.project->id&&result.clip_id==clip_&&result.project_revision==captured_.revision,"Worker identity mismatch");
        const auto& clips=captured_.project->clips;const auto at=std::find_if(clips.begin(),clips.end(),[&](const auto& c){return c.id==clip_;});
        need(at!=clips.end(),"Captured audio clip missing");
        const double first=static_cast<double>(at->source_offset+placement_.range_offset)/captured_.project->sample_rate;
        const double last=static_cast<double>(at->source_offset+placement_.range_offset+placement_.range_length.value_or(at->length-placement_.range_offset))/captured_.project->sample_rate;
        for(auto* notes:{&result.notes,&result.original_notes}){for(auto& note:*notes){note.start=std::max(note.start,first);note.end=std::min(note.end,last);}std::erase_if(*notes,[](const auto& n){return n.end<=n.start;});}
        if(placement_.target_track&&std::any_of(result.notes.begin(),result.notes.end(),[&](const auto& n){return n.instrument!=instruments_.front();})){
            status_.state=JobState::failed;status_.error="Model returned an instrument outside the selected stem category";return;
        }
        result.state=result.notes.empty()&&result.original_notes.empty()?State::empty:State::ready;
        status_.state=result.state==State::ready?JobState::ready:JobState::empty;status_.error.clear();status_.elapsed_seconds=result.elapsed_seconds;status_.cpu_threads=result.cpu_threads;result_=std::move(result);
    }else{status_.state=result.state==State::timeout?JobState::timeout:JobState::failed;status_.error=result.error=="process_exit"&&result.diagnostics.find("missing_EOS_within_2000_tokens")!=std::string::npos?"missing_eos":std::move(result.error);}
}
void Controller::cancel(){
    result_.reset();if(job_){job_->stop.request_stop();status_.state=JobState::cancelling;status_.error.clear();}
    else status_={JobState::cancelled,0,0,{}};
}
void Controller::shutdown(){
    if(job_){job_->stop.request_stop();if(job_->result.valid())job_->result.wait();job_.reset();}result_.reset();status_={JobState::cancelled,0,0,{}};
}
JobStatus Controller::status()const{
    auto value=status_;if(job_){value.elapsed_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-job_->began).count();value.cpu_threads=job_->cpu_threads;value.phase=job_->phase.load(std::memory_order_relaxed);const auto n=job_->progress.load(std::memory_order_relaxed);value.completed=static_cast<int>(n&0xffffffffU);value.total=static_cast<int>(n>>32);}return value;
}
const Result* Controller::preview()const{return result_?&*result_:nullptr;}
PlaceTranscription Controller::prepare(const std::shared_ptr<IProjectStore>& store,const std::filesystem::path& path,
                                      const std::optional<std::vector<std::string>>& roles,bool original_timing){
    poll(store,path);need(status_.state==JobState::ready&&result_.has_value(),"No current transcription result");
    std::vector<TranscribedNote> notes;const auto& selected=original_timing?result_->original_notes:result_->notes;
    if(roles){
        need(!roles->empty(),"Select at least one recognized part");std::vector<std::string> seen;
        for(const auto& role:*roles){need(std::find(seen.begin(),seen.end(),role)==seen.end()&&std::any_of(selected.begin(),selected.end(),[&](const auto& n){return n.instrument==role;}),"Invalid/duplicate transcription part");seen.push_back(role);}
    }
    for(const auto& n:selected)if(!roles||std::find(roles->begin(),roles->end(),n.instrument)!=roles->end())notes.push_back(n);
    const auto& clips=captured_.project->clips;const auto clip=std::find_if(clips.begin(),clips.end(),[&](const auto& c){return c.id==clip_;});need(clip!=clips.end(),"Captured audio clip missing");
    std::optional<TimeMap> time;if(result_->musical.import_tempo)time=imported_map(*captured_.project,*clip,*result_);
    std::vector<TranscribedNote> reference;
    if(!original_timing&&(result_->musical.quantize||result_->musical.fill_gaps>0||result_->musical.trim_leading_bars))for(const auto& n:result_->original_notes)if(!roles||std::find(roles->begin(),roles->end(),n.instrument)!=roles->end())reference.push_back(n);
    std::ostringstream provenance;provenance<<"Audio-to-MIDI "<<(result_->backend==Backend::python_medium?"Python MuScriptor":"Synthetic protocol fixture")<<"; model="<<result_->model_id<<"@"<<result_->model_revision<<"; source_pcm_sha256="<<result_->source_sha256<<"; frames="<<result_->source_first_frame<<"+"<<result_->source_frame_count<<"; rate="<<result_->source_rate<<"; code="<<(result_->backend==Backend::python_medium?python_source_commit:std::string_view("synthetic-fixture"))<<"; executable_sha256="<<result_->executable_sha256<<"; adapter_sha256="<<result_->worker_sha256<<"; weights_sha256="<<result_->weights_sha256;
    return PlaceTranscription(captured_.project,clip_,notes,std::move(time),reference,provenance.str(),placement_);
}
void Controller::placed(){need(status_.state==JobState::ready&&result_.has_value(),"No placed transcription result");result_.reset();status_.state=JobState::placed;}
}
