#include <mrs/ai.hpp>
#include <mrs/ai_query.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace mrs::ai {
bool Context::supports(Capability capability) const noexcept {
    return capability==Capability::summary || capability==Capability::timing || capability==Capability::selection ||
        capability==Capability::entity_queries || capability==Capability::snapshots || capability==Capability::analysis;
}
double Context::seconds(Sample sample) const {
    if (!sample_rate || sample<0 || sample>max_sample) throw std::invalid_argument("invalid AI sample coordinate");
    return static_cast<double>(sample)/sample_rate;
}
namespace {
bool valid_id(const Id& id) { return !id.value.empty() && id.value.size()<=128; }
bool valid_selection(const Project& p,const Selection& selection) {
    if(selection.entities.size()>256) return false;
    if(selection.sample_range && (selection.sample_range->start<0 || selection.sample_range->end<=selection.sample_range->start || selection.sample_range->end>max_sample)) return false;
    const auto contains=[](const auto& values,const Id& id){return std::any_of(values.begin(),values.end(),[&](const auto& v){return v.id==id;});};
    for(std::size_t i=0;i<selection.entities.size();++i) {
        const auto& e=selection.entities[i]; bool found{};
        switch(e.kind) {
        case EntityKind::track: found=contains(p.tracks,e.id); break;
        case EntityKind::folder: found=contains(p.folders,e.id); break;
        case EntityKind::clip: found=contains(p.clips,e.id); break;
        case EntityKind::chord: found=contains(p.chords,e.id); break;
        case EntityKind::section: found=contains(p.sections,e.id); break;
        case EntityKind::marker: found=contains(p.markers,e.id); break;
        default: return false;
        }
        if(!found || !valid_id(e.id)) return false;
        for(std::size_t j=0;j<i;++j)if(e==selection.entities[j])return false;
    }
    return true;
}
bool bounded_context(const Context& c) {
    return c.version==contract_version && valid_id(c.key.project) && valid_id(c.key.session) &&
        c.title.size()<=4096 && c.artist.size()<=4096 && c.sample_rate &&
        c.time.tempos.size()<=4096 && c.time.meters.size()<=4096 && c.selection.entities.size()<=256 &&
        std::all_of(c.selection.entities.begin(),c.selection.entities.end(),[](const auto& e){return valid_id(e.id);});
}
}
ContextResult make_context(const ProjectState& state,const ContextKey& key,TransportState transport,Selection selection) {
    if(!state.project || !valid_id(key.session))return {Error::invalid_input,{}};
    const auto& p=*state.project;
    if(key.project!=p.id || key.project_revision!=state.revision)return {Error::stale_context,{}};
    if(p.title.size()>4096 || p.artist.size()>4096 || p.time.tempos.size()>4096 || p.time.meters.size()>4096)return {Error::invalid_input,{}};
    if(!valid_selection(p,selection))return {Error::invalid_input,{}};
    Context c;
    c.key=key;c.title=p.title;c.artist=p.artist;c.sample_rate=p.sample_rate;c.time=p.time;
    c.observed_transport=std::move(transport);c.selection=std::move(selection);
    c.tracks=p.tracks.size();c.folders=p.folders.size();c.clips=p.clips.size();
    c.chords=p.chords.size();c.sections=p.sections.size();c.markers=p.markers.size();
    if(!bounded_context(c))return {Error::invalid_input,{}};
    return {Error::none,std::move(c)};
}
Worker::Worker(Limits limits):limits_(limits) {
    if(!limits.outstanding || limits.outstanding>64 || !limits.input_bytes || limits.input_bytes>16*1024*1024 ||
       !limits.output_bytes || limits.output_bytes>16*1024*1024)throw std::invalid_argument("invalid AI worker limits");
    thread_=std::thread([this]{run();});
}
Worker::~Worker(){shutdown();}
Submitted Worker::submit(Request request,Task task) {
    if(request.snapshot && !request.snapshot->access_valid())return {Error::permission_denied,0};
    if(request.context.version!=contract_version)return {Error::unsupported,0};
    if(request.snapshot && request.snapshot->context.version!=contract_version)return {Error::unsupported,0};
    if(request.snapshot && request.snapshot->context.key!=request.context.key)return {Error::stale_context,0};
    if(request.snapshot && (request.snapshot->json.size()>limits_.input_bytes ||
       request.input.size()>limits_.input_bytes-request.snapshot->json.size()))return {Error::invalid_input,0};
    if(!task || request.input.size()>limits_.input_bytes || !bounded_context(request.context) ||
       request.timeout.count()<=0 || request.timeout>std::chrono::minutes(5))return {Error::invalid_input,0};
    const auto deadline=std::chrono::steady_clock::now()+request.timeout;
    std::lock_guard lock(mutex_);
    if(stopping_)return {Error::shutdown,0};
    // Completions consume capacity until drained; a stalled UI cannot grow memory.
    if(outstanding_.size()>=limits_.outstanding)return {Error::busy,0};
    if(next_==std::numeric_limits<std::uint64_t>::max())return {Error::busy,0};
    Job job{next_++,std::move(request),std::move(task),{},deadline};
    const auto id=job.id;
    outstanding_.emplace(id,job.cancel);
    try { pending_.push_back(std::move(job)); }
    catch(...) { outstanding_.erase(id);throw; }
    wake_.notify_one();return {Error::none,id};
}
bool Worker::cancel(std::uint64_t id) {
    std::optional<std::stop_source> cancel;
    {std::lock_guard lock(mutex_);const auto at=outstanding_.find(id);
     if(at==outstanding_.end())return false;
     cancel=at->second;}
    cancel->request_stop();return true;
}
void Worker::cancel_all() {
    std::vector<std::stop_source> sources;
    {std::lock_guard lock(mutex_);for(const auto& [id,cancel]:outstanding_){(void)id;sources.push_back(cancel);}}
    for(auto& cancel:sources)cancel.request_stop();
}
std::optional<Result> Worker::take() {
    std::lock_guard lock(mutex_);if(results_.empty())return {};
    auto r=std::move(results_.front());results_.pop_front();
    // Cancellation after completion but before UI delivery still suppresses output.
    const auto at=outstanding_.find(r.request_id);
    if(at!=outstanding_.end() && at->second.stop_requested()){r.error=Error::cancelled;std::string{}.swap(r.output);}
    outstanding_.erase(r.request_id);return r;
}
void Worker::shutdown() {
    {std::lock_guard lock(mutex_);stopping_=true;}
    cancel_all();
    {std::lock_guard lock(mutex_);pending_.clear();results_.clear();}
    wake_.notify_all();if(thread_.joinable())thread_.join();
    std::lock_guard lock(mutex_);outstanding_.clear();
}
void Worker::run() {
    for(;;) {
        Job job;
        {std::unique_lock lock(mutex_);wake_.wait(lock,[this]{return stopping_ || !pending_.empty();});
         if(stopping_)return;
         job=std::move(pending_.front());pending_.pop_front();}
        Result r{job.id,job.request.context.key,Error::none,{}};
        r.policy_revision=job.request.policy_revision;
        const JobControl control{job.cancel.get_token(),job.deadline,limits_.output_bytes};
        try {
            if(!control.should_stop())r.output=job.task(job.request,control);
        }catch(...){r.error=Error::backend_failure;}
        if(control.cancelled())r.error=Error::cancelled;
        else if(control.timed_out())r.error=Error::timeout;
        else if(r.output.size()>limits_.output_bytes)r.error=Error::response_too_large;
        if(r.error!=Error::none)std::string{}.swap(r.output);
        {std::lock_guard lock(mutex_);if(stopping_)return;
         results_.push_back(std::move(r));}
    }
}
} // namespace mrs::ai
