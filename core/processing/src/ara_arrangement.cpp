#include <mrs/ara.hpp>
#include <mrs/audio.hpp>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <set>

namespace mrs::ara {
struct Arrangement::Impl {
    struct Binding {
        AraBinding metadata;
        std::shared_ptr<Session> session;
        std::vector<Clip> clips;
        TimeMap time;
        std::uint32_t rate{};
        std::vector<std::pair<Id,std::string>> tracks;
        std::shared_ptr<const audio::AudioData> input;
        std::vector<ClipCache> caches;
        std::uint64_t content_revision{};
        std::uint64_t state_revision{};
        std::string status{"pending"};
        bool bypass{};
    };
    std::map<std::string,Binding> bindings;
    std::map<std::string,AraBinding> retired;
    std::shared_ptr<std::atomic<bool>> cancel;
    std::function<void()> wait;
    bool prefer_live{};
};
Arrangement::Arrangement() : impl_(std::make_unique<Impl>()) {}
Arrangement::~Arrangement() = default;
void Arrangement::attach(std::string source, std::string module, std::string class_id) {
    if (source.empty() || (!impl_->bindings.contains(source) && impl_->bindings.size() >= 8))
        throw std::invalid_argument("Invalid ARA binding or binding budget exceeded");
    if(impl_->wait)impl_->wait();
    std::shared_ptr<Session> session;std::string document;
    for(const auto& [id,b]:impl_->bindings){(void)id;if(b.session&&!b.metadata.document_id.empty()&&b.metadata.module==module&&b.metadata.class_id==class_id){session=b.session;document=b.metadata.document_id;break;}}
    if(!session)session=Session::open(module,class_id,impl_->cancel);
    session->set_preparation(impl_->cancel,impl_->wait);
    Impl::Binding binding;const auto& cap=session->capability();
    binding.metadata={source,new_id().value,module,class_id,cap.name,cap.factory_id,cap.version,cap.archive_id};
    binding.metadata.document_id=document.empty()?new_id().value:document;
    binding.session=std::move(session);
    binding.session->configure_source(binding.metadata);
    impl_->bindings.insert_or_assign(std::move(source),std::move(binding));
}
void Arrangement::detach(const std::string& source) {const auto at=impl_->bindings.find(source);if(at==impl_->bindings.end())return;const auto session=at->second.session;impl_->bindings.erase(at);for(auto& [id,b]:impl_->bindings){(void)id;if(session&&b.session==session)b.status="pending refresh";}}
void Arrangement::synchronize(const Project& project, const Resolver& resolve) {
    std::uint64_t budget{};
    std::set<Session*> visited;
    std::vector<std::pair<Id,std::string>> tracks;for(const auto& t:project.tracks)tracks.emplace_back(t.id,t.name);
    for (auto& [source,b] : impl_->bindings) {
        if(!b.session||!visited.insert(b.session.get()).second)continue;
        std::vector<Impl::Binding*> members;for(auto& [id,other]:impl_->bindings){(void)id;if(other.session==b.session)members.push_back(&other);}
        try {
            b.session->notify();
            std::vector<SourceSnapshot> sources;
            std::map<std::string,std::vector<Clip>> clips;
            bool changed=false;
            for(auto* member:members){const auto& id=member->metadata.source;auto& selected=clips[id];
                for(const auto& c:project.clips)if(!c.midi&&c.source==id)selected.push_back(c);
                auto input=selected.empty()?member->input:resolve(id);
                if(!selected.empty()&&!input)throw std::runtime_error("ARA source is unavailable");
                if(input)budget+=static_cast<std::uint64_t>(input->frames())*input->channels;
                for(const auto& c:selected)budget+=static_cast<std::uint64_t>(c.length)*(input?input->channels:0);
                changed|=member->clips!=selected||member->time!=project.time||member->rate!=project.sample_rate||member->input!=input||member->tracks!=tracks||member->content_revision!=b.session->content_revision()||!member->status.starts_with("ready");
                if(!selected.empty())sources.push_back({id,0,input});
            }
            if(budget>64*1024*1024)throw std::runtime_error("ARA source/cache preparation exceeds 256 MiB");
            if(b.session->analysing()){for(auto* member:members){member->caches.clear();member->status="analysing (dry source until prepared)";}continue;}
            if(!changed)continue;
            b.session->prefer_live(impl_->prefer_live);
            auto caches = b.session->synchronize(project,sources);
            for(auto* member:members){const auto& id=member->metadata.source;member->clips=std::move(clips[id]);member->time=project.time;member->rate=project.sample_rate;member->tracks=tracks;
                const auto input=std::find_if(sources.begin(),sources.end(),[&](const auto& s){return s.id==id;});if(input!=sources.end())member->input=input->pcm;
                member->caches.clear();for(const auto& cache:caches)if(cache.clip.source==id)member->caches.push_back(cache);
                member->status=b.session->analysing()?"analysing (dry source until prepared)":b.session->live_current()?"ready (live ARA playback)":"ready (prepared PCM cache)";member->content_revision=b.session->content_revision();
            }
        } catch (const std::exception& error) {
            if(impl_->cancel&&impl_->cancel->load()){for(auto* member:members)member->status="pending refresh (preparation cancelled)";continue;}
            if(std::string_view(error.what())=="ARA content changed during preparation; refresh before publishing"){for(auto* member:members)member->status="pending refresh (plugin still updating)";continue;}
            for(auto* member:members){member->caches.clear();member->status=std::string("dry fallback: ")+error.what();}
        }
    }
}
void Arrangement::invalidate() {std::set<Session*> visited;for(auto& [id,b]:impl_->bindings){(void)id;b.status="pending refresh";if(b.session&&visited.insert(b.session.get()).second)b.session->invalidate();}}
void Arrangement::apply(const Project& project, audio::RenderGraph& graph,bool editor_preview) const {
    std::set<Session*> editors;
    if(editor_preview)for(const auto& [id,b]:impl_->bindings){(void)id;if(b.session&&editors.insert(b.session.get()).second)graph.editor_renderers.push_back(b.session->editor_renderer());}
    std::size_t voice{};
    for (const auto& clip : project.clips) if (!clip.midi) {
        if (voice >= graph.voices.size()) throw std::invalid_argument("ARA voice mapping mismatch");
        auto& output = graph.voices[voice++];
        const auto binding = impl_->bindings.find(clip.source);
        if (binding == impl_->bindings.end() || binding->second.bypass) continue;
        if (binding->second.time != project.time || binding->second.rate != project.sample_rate) continue;
        const auto& caches = binding->second.caches;
        const auto cache = std::find_if(caches.begin(),caches.end(),[&](const auto& c) { return c.clip == clip; });
        if (cache == caches.end()) continue; // never publish a stale revision/range
        if (cache->pcm->sample_rate != project.sample_rate || cache->pcm->frames() != clip.length ||
            cache->pcm->channels != output.asset->channels) throw std::invalid_argument("ARA cache geometry mismatch");
        output.asset = cache->pcm; output.source_offset = 0; output.stream.reset();
        output.renderer=binding->second.session->live_renderer(clip.id);
    }
}
std::string Arrangement::status(const std::string& source) const {
    const auto found = impl_->bindings.find(source);
    if (found == impl_->bindings.end()) return "unbound";
    if (found->second.bypass) return "disabled (dry source)";
    if(!found->second.session)return found->second.status;
    if(found->second.session->live_failed())return "failed (ARA live renderer; Update playback or bypass to recover)";
    if (found->second.session->analysing()) return "analysing (refresh playback when complete)";
    if (found->second.status.starts_with("dry fallback")) return "failed ("+found->second.status+")";
    if (found->second.content_revision != found->second.session->content_revision()&&!found->second.session->live_current()) return "pending refresh (plugin edits)";
    auto status=found->second.status;
    if(status.starts_with("ready")){const auto stats=found->second.session->preparation_stats();status+="; PCM full="+std::to_string(stats.full)+", partial="+std::to_string(stats.partial)+", reused="+std::to_string(stats.reused)+", frames="+std::to_string(stats.rendered_frames)+", ms="+std::to_string(stats.milliseconds)+", live="+std::to_string(stats.live);
        std::string reasons;
        const auto add=[&](std::uint32_t flag,const char* text){if(stats.causes&flag){if(!reasons.empty())reasons+=", ";reasons+=text;}};
        add(preparation_model,"model/cache changed");add(preparation_parameter,"VST parameter");add(preparation_analysis,"analysis");add(preparation_restart,"VST restart");add(preparation_explicit,"host refresh");add(preparation_off_thread,"off-thread notification");add(preparation_unknown_range,"no precise range");add(preparation_context,"context/tail");add(preparation_latency,"latency changed");
        status+="; reason="+(reasons.empty()?std::string(stats.partial?"precise range":"cache reuse"):reasons);
    }
    return status+(found->second.session->transport_failed()?"; editor transport unavailable (reopen editor)":"");
}
std::vector<ClipCache> Arrangement::caches() const {
    std::vector<ClipCache> result;
    for (const auto& [id,b] : impl_->bindings) { (void)id; result.insert(result.end(),b.caches.begin(),b.caches.end()); }
    return result;
}
void Arrangement::bypass(const std::string& source,bool disabled) {
    auto& b=impl_->bindings.at(source); b.bypass=disabled;
    if (!disabled) b.status="pending refresh";
}
bool Arrangement::bypassed(const std::string& source) const {
    const auto at=impl_->bindings.find(source); return at!=impl_->bindings.end() && at->second.bypass;
}
ModelStats Arrangement::model_stats(const std::string& source)const {const auto& b=impl_->bindings.at(source);return b.session?b.session->model_stats():ModelStats{};}
bool Arrangement::poll() {
    bool dirty=false;
    std::set<Session*> visited;
    for (auto& [source,b]:impl_->bindings) { (void)source;
        if(!b.session)continue;
        try { if(visited.insert(b.session.get()).second)b.session->notify(); }
        catch(const std::exception& e) { b.status=std::string("dry fallback: ")+e.what(); continue; }
        if (!b.bypass && b.content_revision!=b.session->content_revision()) dirty=true;
        if(b.session->live_current())b.content_revision=b.session->content_revision();
        const auto state=b.session->state_revision();if(state!=b.state_revision){b.state_revision=state;dirty=true;}
    } return dirty;
}
std::optional<std::uint64_t> Arrangement::pending_revision() const {
    bool pending=false;std::uint64_t revision{};
    for(const auto& [source,b]:impl_->bindings){(void)source;if(!b.session||b.bypass)continue;
        const auto current=b.session->content_revision();revision+=current;
        if(current!=b.content_revision&&!b.session->live_current()){if(b.session->analysing())return {};pending=true;}
    }
    return pending?std::optional{revision}:std::nullopt;
}
bool Arrangement::open_editor(const std::string& source,const Id& clip,void* parent,int& width,int& height) {
    auto& binding=impl_->bindings.at(source);if(!binding.session)throw std::runtime_error(binding.status);auto& session=*binding.session;
    session.select_clip(clip);const bool opened=session.open_editor(parent,width,height);session.select_clip(clip);return opened;
}
void Arrangement::close_editor(const std::string& source) {
    const auto at=impl_->bindings.find(source); if (at!=impl_->bindings.end()&&at->second.session) at->second.session->close_editor();
}
bool Arrangement::resize_editor(const std::string& source,int& width,int& height) {const auto at=impl_->bindings.find(source);return at!=impl_->bindings.end()&&at->second.session&&at->second.session->resize_editor(width,height);}
void Arrangement::close_editors() {std::set<Session*> seen;for(auto& [source,b]:impl_->bindings){(void)source;if(b.session&&seen.insert(b.session.get()).second)b.session->close_editor();}}
void Arrangement::select_clip(const std::string& source,const Id& clip) { auto& b=impl_->bindings.at(source);if(!b.session)throw std::runtime_error(b.status);b.session->select_clip(clip); }
void Arrangement::set_preparation(std::shared_ptr<std::atomic<bool>> cancel,std::function<void()> wait) {
    impl_->cancel=std::move(cancel);impl_->wait=std::move(wait);
    for(auto& [source,b]:impl_->bindings){(void)source;if(b.session)b.session->set_preparation(impl_->cancel,impl_->wait);}
}
void Arrangement::prefer_live(bool enabled){if(impl_->prefer_live==enabled)return;impl_->prefer_live=enabled;for(auto& [id,b]:impl_->bindings){(void)id;if(b.session){b.session->prefer_live(enabled);b.status="pending refresh";}}}
void Arrangement::transport(const Project& project,const audio::RealtimeState& state) {std::set<Session*> seen;for(auto& [source,b]:impl_->bindings){(void)source;if(b.session&&seen.insert(b.session.get()).second)b.session->transport(project,state);}}
std::vector<TransportRequest> Arrangement::take_transport_requests() {
    std::vector<TransportRequest> result;std::set<Session*> seen;for(auto& [source,b]:impl_->bindings){(void)source;if(b.session&&seen.insert(b.session.get()).second){auto requests=b.session->take_transport_requests();result.insert(result.end(),requests.begin(),requests.end());}}return result;
}
AraBinding Arrangement::binding(const std::string& source)const{return impl_->bindings.at(source).metadata;}
void Arrangement::relocate_source(const std::string& source,std::shared_ptr<const audio::AudioData> data){
    const auto at=impl_->bindings.find(source);if(at==impl_->bindings.end())return;
    if(at->second.session)at->second.session->relocate_source(source,data);at->second.input=std::move(data);
}
std::vector<AraBinding> Arrangement::save_bindings(){
    std::vector<AraBinding> result;
    for(auto& [source,b]:impl_->bindings){(void)source;auto metadata=b.metadata;metadata.bypass=b.bypass;if(b.session&&!b.status.starts_with("dry fallback")){b.session->save_state(metadata);b.state_revision=b.session->state_revision();}b.metadata=metadata;result.push_back(std::move(metadata));}
    return result;
}
void Arrangement::restore_bindings(const std::vector<AraBinding>& bindings){
    for(auto it=impl_->bindings.begin();it!=impl_->bindings.end();){auto& b=it->second;
        const auto found=std::find_if(bindings.begin(),bindings.end(),[&](const auto& desired){return desired.source==it->first&&desired.persistent_id==b.metadata.persistent_id&&desired.module==b.metadata.module&&desired.class_id==b.metadata.class_id&&desired.document_id==b.metadata.document_id;});
        if(found!=bindings.end()){b.bypass=found->bypass;++it;continue;}
        auto archived=b.metadata;archived.bypass=b.bypass;if(b.session&&!b.status.starts_with("dry fallback"))b.session->save_state(archived);
        std::size_t bytes=archived.archive.size()+archived.component_state.size()+archived.controller_state.size();for(const auto& [id,old]:impl_->retired){(void)id;bytes+=old.archive.size()+old.component_state.size()+old.controller_state.size();}
        if(bytes>64*1024*1024)throw std::runtime_error("ARA Undo archive budget exceeded; save and reopen before more binding changes");
        for(auto& [id,other]:impl_->bindings){(void)id;if(b.session&&other.session==b.session&&!other.status.starts_with("dry fallback"))other.status="pending refresh";}
        impl_->retired.insert_or_assign(archived.persistent_id,std::move(archived));it=impl_->bindings.erase(it);
    }
    for(const auto& desired:bindings){if(impl_->bindings.contains(desired.source))continue;
        Impl::Binding b;b.metadata=desired;b.bypass=desired.bypass;
        if(const auto old=impl_->retired.find(desired.persistent_id);old!=impl_->retired.end()&&old->second.module==desired.module&&old->second.class_id==desired.class_id){b.metadata=old->second;b.metadata.source=desired.source;}
        try{
            if(!desired.document_id.empty())for(const auto& [id,other]:impl_->bindings){(void)id;if(other.session&&other.metadata.document_id==desired.document_id&&other.metadata.module==desired.module&&other.metadata.class_id==desired.class_id){b.session=other.session;break;}}
            if(!b.session){if(impl_->wait)impl_->wait();b.session=Session::open(desired.module,desired.class_id,impl_->cancel);b.session->set_preparation(impl_->cancel,impl_->wait);b.session->restore_state(b.metadata);}
            b.session->configure_source(b.metadata);
        }
        catch(const std::exception& e){b.session.reset();b.status=std::string("unavailable (saved ARA state preserved): ")+e.what();}
        impl_->bindings.emplace(desired.source,std::move(b));
    }
}
void Arrangement::retry(const std::string& source){
    auto& selected=impl_->bindings.at(source);std::vector<Impl::Binding*> members;
    for(auto& [id,b]:impl_->bindings)if(id==source||(!selected.metadata.document_id.empty()&&b.metadata.document_id==selected.metadata.document_id))members.push_back(&b);
    // Capture surviving members before constructing a replacement. A failed
    // retry must not retire a healthy neighbour or its unsaved private edits.
    for(auto* b:members)if(b->session&&!b->status.starts_with("dry fallback")){auto state=b->metadata;b->session->save_state(state);b->metadata=std::move(state);}
    try{auto session=std::shared_ptr<Session>(Session::open(selected.metadata.module,selected.metadata.class_id,impl_->cancel));session->set_preparation(impl_->cancel,impl_->wait);session->restore_state(selected.metadata);
        for(auto* b:members)session->configure_source(b->metadata);
        for(auto* b:members){b->session=session;b->caches.clear();b->clips.clear();b->input.reset();b->status="pending refresh";}
    }catch(const std::exception& e){for(auto* b:members)if(!b->session)b->status=std::string("unavailable (saved ARA state preserved): ")+e.what();}
}
}
