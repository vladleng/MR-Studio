#include <mrs/ai_query.hpp>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace mrs::ai {
namespace {
struct SizeLimit {};
bool valid_utf8(std::string_view value){
    for(std::size_t i=0;i<value.size();){const auto first=static_cast<unsigned char>(value[i++]);if(first<128)continue;
        unsigned count{},code{};if(first>=0xC2 && first<=0xDF){count=1;code=first&31;}
        else if(first>=0xE0 && first<=0xEF){count=2;code=first&15;}
        else if(first>=0xF0 && first<=0xF4){count=3;code=first&7;}else return false;
        if (count > value.size() - i)
          return false;
        for (unsigned n = 0; n < count; ++n) {
          const auto byte = static_cast<unsigned char>(value[i++]);
          if ((byte & 0xC0) != 0x80)
            return false;
          code = (code << 6) | (byte & 63);
        }
        if((count==1 && code<128) || (count==2 && code<2048) || (count==3 && code<65536) || code>0x10FFFF || (code>=0xD800 && code<=0xDFFF))return false;
    }return true;
}
// Small deterministic writer. Every append is checked before allocating/growing.
class Json {
    std::string text_;
    std::size_t budget_;
    bool first_{true};
public:
    explicit Json(std::size_t budget):budget_(budget){add("{");}
    void add(std::string_view text){if(text.size()>budget_-text_.size())throw SizeLimit{};text_.append(text);}
    void quoted(std::string_view text){
        if(text.size()>4096)throw SizeLimit{};
        if(!valid_utf8(text))throw std::invalid_argument("invalid UTF-8 query text");
        add("\"");constexpr char hex[]="0123456789abcdef";
        for(const unsigned char c:text){
            if(c=='"')add("\\\"");else if(c=='\\')add("\\\\");
            else if(c<32){const char escaped[]={'\\','u','0','0',hex[c>>4],hex[c&15]};add({escaped,6});}
            else {const char byte=static_cast<char>(c);add({&byte,1});}
        }add("\"");
    }
    void key(std::string_view name){if(!first_)add(",");first_=false;quoted(name);add(":");}
    void field(std::string_view name,std::string_view value){key(name);quoted(value);}
    void field(std::string_view name,const std::string& value){field(name,std::string_view(value));}
    void field(std::string_view name,const char* value){field(name,std::string_view(value));}
    void field(std::string_view name,bool value){key(name);add(value?"true":"false");}
    template<class T> requires std::is_arithmetic_v<T>
    void field(std::string_view name,T value){
        if constexpr(std::is_floating_point_v<T>)if(!std::isfinite(value))throw std::invalid_argument("nonfinite query value");
        char buffer[64];const auto result=std::to_chars(buffer,buffer+sizeof(buffer),value);
        if(result.ec!=std::errc{})throw std::invalid_argument("invalid query number");
        key(name);add({buffer,static_cast<std::size_t>(result.ptr-buffer)});
    }
    void id(std::string_view name,const Id& value){field(name,value.value);}
    void id(std::string_view name,const std::optional<Id>& value){if(value)id(name,*value);else raw(name,"null");}
    void raw(std::string_view name,std::string_view value){key(name);add(value);}
    std::string finish(){add("}");return std::move(text_);}
};
constexpr std::size_t max_bytes=1024*1024;
std::string key_json(const ContextKey& key) {
    Json j(1024);j.id("project_id",key.project);j.id("session_id",key.session);
    // Revision identifiers are decimal strings, immune to JSON double rounding.
    j.field("project_revision",std::to_string(key.project_revision));j.field("graph_revision",std::to_string(key.graph_revision));return j.finish();
}
bool valid_id(const Id& id){return !id.value.empty() && id.value.size()<=128;}
bool valid_query(const Query& q){
    if(q.limit==0 || q.limit>256 || q.scan_limit==0 || q.scan_limit>4096 || q.byte_budget<512 || q.byte_budget>max_bytes)return false;
    for(const auto* id:{&q.id,&q.track,&q.clip,&q.plugin})if(*id && !valid_id(**id))return false;
    if(q.samples && (q.samples->start<0 || q.samples->end<=q.samples->start || q.samples->end>max_sample))return false;
    if(q.ticks && (q.ticks->first<0 || q.ticks->second<=q.ticks->first || q.ticks->second>max_tick))return false;
    const bool channels=q.kind==QueryKind::tracks || q.kind==QueryKind::mixer || q.kind==QueryKind::routing;
    const bool clips=q.kind==QueryKind::clips || q.kind==QueryKind::midi_notes || q.kind==QueryKind::midi_events;
    const bool araClips=q.kind==QueryKind::ara_binding||q.kind==QueryKind::ara_status;
    const bool plugins=q.kind==QueryKind::plugins || q.kind==QueryKind::plugin_parameters;
    const bool graph_parameters=q.kind==QueryKind::graph_parameters;
    const bool temporal=clips || q.kind==QueryKind::chords || q.kind==QueryKind::sections || q.kind==QueryKind::markers ||
        q.kind==QueryKind::tempos || q.kind==QueryKind::meters || q.kind==QueryKind::live_bindings || q.kind==QueryKind::live_actions;
    if((q.track && !channels && !clips && !araClips && !plugins) || (q.clip && !clips && !araClips) || (q.plugin && !plugins && !graph_parameters) ||
       (q.master && q.kind!=QueryKind::mixer && !plugins) || ((q.samples || q.ticks) && !temporal))return false;
    if(q.selected_only && (q.kind==QueryKind::graph_nodes || q.kind==QueryKind::graph_edges || q.kind==QueryKind::graph_outputs ||
       q.kind==QueryKind::midi_routes || q.kind==QueryKind::midi_ports || q.kind==QueryKind::live_patches || q.kind==QueryKind::plugin_parameters || graph_parameters))return false;
    return !(q.master && q.track);
}
template<class T> const T* find_id(const std::vector<T>& values,const Id& id){
    const auto at=std::find_if(values.begin(),values.end(),[&](const auto& v){return v.id==id;});
    return at==values.end()?nullptr:&*at;
}
bool selected(const Context& context,EntityKind kind,const Id& id){
    return std::any_of(context.selection.entities.begin(),context.selection.entities.end(),[&](const auto& v){return v.kind==kind && v.id==id;});
}
bool overlap(Tick start,Tick end,const Query& q,const Timeline& timeline){
    if(q.ticks && (end<=q.ticks->first || start>=q.ticks->second))return false;
    if(q.samples){
        const auto sample_at=[&](Tick tick){try{return timeline.to_samples(tick);}catch(const std::out_of_range&){return max_sample;}};
        if(sample_at(end)<=q.samples->start || sample_at(start)>=q.samples->end)return false;
    }
    return true;
}
bool point(Tick tick,const Query& q,const Timeline& timeline){
    if(q.ticks && (tick<q.ticks->first || tick>=q.ticks->second))return false;
    if(q.samples){const auto sample=timeline.to_samples(tick);if(sample<q.samples->start || sample>=q.samples->end)return false;}
    return true;
}
std::string integers(const std::vector<int>& values){
    std::string result="[";for(const auto value:values){if(result.size()>1)result+=',';result+=std::to_string(value);}return result+"]";
}
std::string scope_json(const Query& q){
    Json j(2048);j.id("id",q.id);j.id("track_id",q.track);j.id("clip_id",q.clip);j.id("plugin_id",q.plugin);
    j.field("master",q.master);j.field("selected_only",q.selected_only);
    if(q.samples){j.field("sample_start",q.samples->start);j.field("sample_end",q.samples->end);}
    if(q.ticks){j.field("tick_start",q.ticks->first);j.field("tick_end",q.ticks->second);}
    return j.finish();
}
std::string page_json(const Page& p,const Query& q,std::string_view items){
    Json j(q.byte_budget);j.field("version",contract_version);j.raw("context_key",key_json(p.key));
    j.field("query",query_name(p.kind));j.raw("scope",scope_json(q));j.field("cursor",q.cursor);
    j.field("source_items",p.source_items);j.field("scanned",p.scanned);j.field("returned",p.returned);
    j.field("next_cursor",p.next_cursor);j.field("has_more",p.has_more);j.field("byte_limited",p.byte_limited);
    j.raw("items",items);return j.finish();
}
std::string context_json(const Context& c,std::size_t budget){
    Json j(budget);j.field("version",c.version);j.raw("context_key",key_json(c.key));j.field("title",c.title);j.field("artist",c.artist);
    j.field("sample_rate",c.sample_rate);j.field("ticks_per_quarter",c.ticks_per_quarter);
    j.field("tracks",c.tracks);j.field("folders",c.folders);j.field("clips",c.clips);j.field("chords",c.chords);j.field("sections",c.sections);j.field("markers",c.markers);
    j.field("observed_sample",c.observed_transport.sample);j.field("observed_playback",static_cast<int>(c.observed_transport.playback));
    if(c.observed_transport.loop){j.field("loop_start_samples",c.observed_transport.loop->start);j.field("loop_end_samples",c.observed_transport.loop->end);}
    std::string selection="[";
    for(const auto& e:c.selection.entities){Json item(1024);item.field("kind",static_cast<int>(e.kind));item.id("id",e.id);const auto s=item.finish();
      if (selection.size() + s.size() + 2 > budget)
        throw SizeLimit{};
      if (selection.size() > 1)
        selection += ',';
      selection += s;
    }
    selection+=']';j.raw("selection",selection);
    if(c.selection.sample_range){j.field("selection_start_samples",c.selection.sample_range->start);j.field("selection_end_samples",c.selection.sample_range->end);}
    j.raw("capabilities","[\"summary\",\"timing\",\"selection\",\"entity_queries\",\"snapshots\"]");
    j.raw("unsupported","[\"tools\",\"automation\",\"analysis\",\"live_performance\",\"runtime_plugin_parameters\"]");
    return j.finish();
}
}
std::string_view query_name(QueryKind kind){
    switch(kind){
    case QueryKind::tracks:return "tracks";case QueryKind::folders:return "folders";case QueryKind::clips:return "clips";
    case QueryKind::midi_notes:return "midi_notes";case QueryKind::midi_events:return "midi_events";
    case QueryKind::chords:return "chords";case QueryKind::sections:return "sections";case QueryKind::markers:return "markers";
    case QueryKind::tempos:return "tempos";case QueryKind::meters:return "meters";case QueryKind::mixer:return "mixer";
    case QueryKind::routing:return "routing";case QueryKind::plugins:return "plugins";case QueryKind::plugin_parameters:return "stored_plugin_parameters";
    case QueryKind::graph_nodes:return "graph_nodes";case QueryKind::graph_edges:return "graph_edges";case QueryKind::graph_outputs:return "graph_outputs";
    case QueryKind::midi_routes:return "midi_routes";case QueryKind::live_bindings:return "live_bindings";
    case QueryKind::live_actions:return "live_actions";case QueryKind::live_patches:return "live_patches";
    case QueryKind::automation:return "automation";case QueryKind::analysis:return "analysis";case QueryKind::live_performance:return "live_performance";
    case QueryKind::graph_parameters:return "stored_graph_parameters";
    case QueryKind::midi_ports:return "logical_midi_ports";
    case QueryKind::ara_capabilities:return "list_ara_capabilities";
    case QueryKind::ara_binding:return "query_ara_binding";
    case QueryKind::ara_status:return "get_ara_status";
    }return {};
}
ViewResult capture_view(const ProjectState& p,const processing::GraphSnapshot& g,const persistence::ProjectDocument& d,
                       const ContextKey& key,TransportState transport,Selection selection,ReadAccess access,AraReadState ara){
    if(!access.valid())return {Error::permission_denied,{}};
    if(!g.graph || !p.project)return {Error::invalid_input,{}};
    if(g.revision!=key.graph_revision || d.project.id!=key.project || g.graph->id!=d.graph.id)return {Error::stale_context,{}};
    auto c=make_context(p,key,std::move(transport),std::move(selection));if(c.error!=Error::none)return {c.error,{}};
    if(d.section_patches.size()>4096 || d.actions.size()>4096 || d.patches.size()>4096 || d.midi.size()>4096)return {Error::response_too_large,{}};
    const auto& project=*p.project;
    for(const auto count:{project.tracks.size(),project.folders.size(),project.clips.size(),project.chords.size(),project.sections.size(),project.markers.size()})
        if(count>65536)return {Error::response_too_large,{}};
    auto view=std::shared_ptr<ReadView>(new ReadView());view->context_=std::move(*c.value);view->project_=p.project;view->graph_=g.graph;
    view->access_=std::move(access);
    if(ara.capabilities.size()>1024||ara.clips.size()>65536)return {Error::response_too_large,{}};
    view->ara_=std::move(ara);
    view->bindings_=d.section_patches;view->actions_=d.actions;
    for(const auto& patch:d.patches){if(patch.state.patch_name.size()>4096)return {Error::response_too_large,{}};
        view->patches_.push_back({patch.id,patch.state.id,patch.state.patch_name});}
    for(const auto& port:d.midi){if(port.name.size()>4096)return {Error::response_too_large,{}};
        view->ports_.push_back({port.port,port.name,port.input,port.output});}
    return {Error::none,std::move(view)};
}
Page ReadView::query(const Query& input,ReadPolicy policy) const {
    Query q=input;Page page;page.key=context_.key;page.kind=q.kind;page.next_cursor=q.cursor;
    if(!access_.valid()){page.error=Error::permission_denied;return page;}
    if(!valid_query(q) || query_name(q.kind).empty()){page.error=Error::invalid_input;return page;}
    if((q.kind==QueryKind::ara_capabilities||q.kind==QueryKind::ara_binding||q.kind==QueryKind::ara_status)&&!ara_.enabled){page.error=Error::unsupported;return page;}
    const bool temporal=q.kind==QueryKind::clips || q.kind==QueryKind::midi_notes || q.kind==QueryKind::midi_events ||
        q.kind==QueryKind::chords || q.kind==QueryKind::sections || q.kind==QueryKind::markers || q.kind==QueryKind::tempos ||
        q.kind==QueryKind::meters || q.kind==QueryKind::live_bindings || q.kind==QueryKind::live_actions;
    if(temporal && q.selected_only && !q.samples && !q.ticks && context_.selection.sample_range)q.samples=context_.selection.sample_range;
    if(q.selected_only && (q.kind==QueryKind::tempos || q.kind==QueryKind::meters) && !q.samples && !q.ticks){page.error=Error::invalid_input;return page;}
    if(q.kind==QueryKind::automation || q.kind==QueryKind::analysis || q.kind==QueryKind::live_performance){page.error=Error::unsupported;return page;}
    if((q.kind==QueryKind::plugin_parameters || q.kind==QueryKind::graph_parameters) &&
       (!policy.stored_plugin_parameters || !access_.stored_plugin_parameters)){page.error=Error::permission_denied;return page;}
    const auto& p=*project_;const auto& g=*graph_;const Timeline time(p.time,p.sample_rate);
    const Track* track=q.track?find_id(p.tracks,*q.track):nullptr;
    const Clip* clip=q.clip?find_id(p.clips,*q.clip):nullptr;
    if((q.track && !track) || (q.clip && !clip)){page.error=Error::not_found;return page;}
    if((q.kind==QueryKind::ara_binding||q.kind==QueryKind::ara_status)&&clip&&clip->midi){page.error=Error::unsupported;return page;}
    const bool midi=q.kind==QueryKind::midi_notes || q.kind==QueryKind::midi_events;
    if(midi && !q.clip){page.error=Error::invalid_input;return page;}
    if(midi && !clip->midi){page.error=Error::unsupported;return page;}
    struct Slot {const NativeInsert* insert;std::optional<Id> track;std::size_t index;};
    std::vector<Slot> slots;
    const bool plugins=q.kind==QueryKind::plugins || q.kind==QueryKind::plugin_parameters;
    if(plugins){
        for(std::size_t i=0;i<p.master_inserts.size();++i)slots.push_back({&p.master_inserts[i],{},i});
        for(const auto& t:p.tracks)for(std::size_t i=0;i<t.inserts.size();++i)slots.push_back({&t.inserts[i],t.id,i});
    }
    if(q.plugin && plugins && std::none_of(slots.begin(),slots.end(),[&](const auto& s){return s.insert->id==*q.plugin;})){page.error=Error::not_found;return page;}
    const processing::NodeState* parameter_node=nullptr;
    if(q.kind==QueryKind::graph_parameters){if(!q.plugin || q.id){page.error=Error::invalid_input;return page;}
        parameter_node=find_id(g.nodes,*q.plugin);if(!parameter_node){page.error=Error::not_found;return page;}}
    const Slot* parameter_slot=nullptr;
    struct NativeValue {std::uint32_t id;std::string name;float value;std::string unit;};
    std::vector<NativeValue> native_values;
    if(q.kind==QueryKind::plugin_parameters){
        if(!q.plugin){page.error=Error::invalid_input;return page;}
        const auto at=std::find_if(slots.begin(),slots.end(),[&](const auto& s){return s.insert->id==*q.plugin;});
        if(at==slots.end() || (q.track && at->track!=q.track) || (q.master && at->track)){page.error=Error::not_found;return page;}
        parameter_slot=&*at;
        const auto& f=*parameter_slot->insert;
        if(f.kind!=InsertKind::vst3){
            const auto add=[&](std::string name,float value,std::string unit){native_values.push_back({static_cast<std::uint32_t>(native_values.size()),std::move(name),value,std::move(unit)});};
            switch(f.kind){
            case InsertKind::gain:add("gain",f.gain,"linear");break;
            case InsertKind::highpass:case InsertKind::lowpass:add("frequency",f.frequency,"hz");add("q",f.q,"ratio");break;
            case InsertKind::eq:add("frequency",f.frequency,"hz");add("gain",f.gain,"db");add("q",f.q,"ratio");break;
            case InsertKind::channel_eq:for(std::size_t i=0;i<f.bands.size();++i){const auto name="band"+std::to_string(i)+"_";const auto& b=f.bands[i];add(name+"frequency",b.frequency,"hz");add(name+"gain",b.gain,"db");add(name+"q",b.q,"ratio");add(name+"enabled",b.enabled?1.f:0.f,"boolean");}break;
            case InsertKind::cab_ir:add("mix",f.ir.mix,"ratio");add("low_cut",f.ir.low_cut,"hz");add("high_cut",f.ir.high_cut,"hz");add("invert",f.ir.invert?1.f:0.f,"boolean");break;
            case InsertKind::cab_collection:add("mix",f.ir.mix,"ratio");add("low_cut",f.ir.low_cut,"hz");add("high_cut",f.ir.high_cut,"hz");break;
            case InsertKind::reverb:add("pre_delay",f.reverb.pre_delay,"ms");add("tone",f.reverb.tone,"db");add("mix",f.reverb.mix,"ratio");add("gate_enabled",f.reverb.gate_enabled?1.f:0.f,"boolean");add("gate_threshold",f.reverb.gate_threshold,"db");break;
            case InsertKind::vst3:break;
            case InsertKind::rig:break; // Module parameters are not part of the scalar native projection.
            }
        }
    }
    switch(q.kind){
    case QueryKind::tracks:case QueryKind::routing:page.source_items=p.tracks.size();break;
    case QueryKind::mixer:page.source_items=p.tracks.size()+1;break;
    case QueryKind::folders:page.source_items=p.folders.size();break;case QueryKind::clips:page.source_items=p.clips.size();break;
    case QueryKind::midi_notes:page.source_items=clip->midi->notes.size();break;case QueryKind::midi_events:page.source_items=clip->midi->events.size();break;
    case QueryKind::chords:page.source_items=p.chords.size();break;case QueryKind::sections:page.source_items=p.sections.size();break;
    case QueryKind::markers:page.source_items=p.markers.size();break;case QueryKind::tempos:page.source_items=p.time.tempos.size();break;
    case QueryKind::meters:page.source_items=p.time.meters.size();break;case QueryKind::plugins:page.source_items=slots.size();break;
    case QueryKind::plugin_parameters:page.source_items=parameter_slot->insert->kind==InsertKind::vst3?parameter_slot->insert->parameters.size():native_values.size();break;
    case QueryKind::graph_parameters:page.source_items=parameter_node->parameters.size();break;
    case QueryKind::graph_nodes:page.source_items=g.nodes.size();break;case QueryKind::graph_edges:page.source_items=g.edges.size();break;
    case QueryKind::graph_outputs:page.source_items=g.outputs.size();break;case QueryKind::midi_routes:page.source_items=g.midi_routes.size();break;
    case QueryKind::live_bindings:page.source_items=bindings_.size();break;case QueryKind::live_actions:page.source_items=actions_.size();break;
    case QueryKind::live_patches:page.source_items=patches_.size();break;default:break;
    case QueryKind::midi_ports:page.source_items=ports_.size();break;
    case QueryKind::ara_capabilities:page.source_items=ara_.capabilities.size();break;
    case QueryKind::ara_binding:case QueryKind::ara_status:page.source_items=ara_.clips.size();break;
    }
    if(q.cursor>page.source_items){page.error=Error::invalid_input;return page;}
    // ID existence has a distinct error; it never becomes an ambiguous empty page.
    if(q.id){bool found{};
        switch(q.kind){
        case QueryKind::tracks:case QueryKind::routing:case QueryKind::mixer:found=find_id(p.tracks,*q.id)!=nullptr;break;
        case QueryKind::folders:found=find_id(p.folders,*q.id)!=nullptr;break;case QueryKind::clips:found=find_id(p.clips,*q.id)!=nullptr;break;
        case QueryKind::chords:found=find_id(p.chords,*q.id)!=nullptr;break;case QueryKind::sections:found=find_id(p.sections,*q.id)!=nullptr;break;
        case QueryKind::markers:found=find_id(p.markers,*q.id)!=nullptr;break;case QueryKind::midi_notes:found=find_id(clip->midi->notes,*q.id)!=nullptr;break;
        case QueryKind::midi_events:found=find_id(clip->midi->events,*q.id)!=nullptr;break;
        case QueryKind::plugins:found=std::any_of(slots.begin(),slots.end(),[&](const auto& s){return s.insert->id==*q.id;});break;
        case QueryKind::graph_nodes:found=find_id(g.nodes,*q.id)!=nullptr;break;case QueryKind::live_actions:found=find_id(actions_,*q.id)!=nullptr;break;
        case QueryKind::live_patches:found=find_id(patches_,*q.id)!=nullptr;break;
        case QueryKind::midi_ports:found=find_id(ports_,*q.id)!=nullptr;break;
        case QueryKind::ara_capabilities:found=find_id(ara_.capabilities,*q.id)!=nullptr;break;
        case QueryKind::ara_binding:case QueryKind::ara_status:found=find_id(ara_.clips,*q.id)!=nullptr;break;
        default:page.error=Error::invalid_input;return page;
        }
        if(!found){page.error=Error::not_found;return page;}
    }
    auto identity=[&](EntityKind kind,const Id& id,const std::optional<Id>& parent={}){
        if(q.id && id!=*q.id)return false;
        if(q.track && parent!=q.track && id!=*q.track)return false;
        return !q.selected_only || (temporal && context_.selection.entities.empty() && context_.selection.sample_range) ||
            selected(context_,kind,id) || (parent && selected(context_,EntityKind::track,*parent));
    };
    std::string items="[";
    try{
        for(std::size_t index=q.cursor;index<page.source_items && page.scanned<q.scan_limit && page.returned<q.limit;++index){
            Json row(q.byte_budget);bool match=true;
            switch(q.kind){
            case QueryKind::tracks:case QueryKind::routing:case QueryKind::mixer:{
                const bool master=q.kind==QueryKind::mixer && index==p.tracks.size();
                if(master){match=!q.track && !q.id && !q.selected_only;row.raw("track_id","null");row.field("master",true);row.field("gain",p.master_gain);row.raw("hardware_outputs",integers(p.master_outputs));}
                else{const auto& t=p.tracks[index];match=!q.master && identity(EntityKind::track,t.id);if(!match)break;row.id("id",t.id);
                    row.field("name",t.name);row.field("kind",static_cast<int>(t.kind));row.id("folder_id",t.folder);row.field("fx_channel",t.fx_channel);
                    row.field("gain",t.mix.gain);row.field("pan",t.mix.pan);row.field("mute",t.mix.mute);row.field("solo",t.mix.solo);
                    row.id("output_bus_id",t.output);row.field("output_master",!t.output && t.hardware_outputs.empty());row.raw("hardware_outputs",integers(t.hardware_outputs));
                    row.field("input_index",t.input);row.field("input_stereo",t.input_stereo);row.field("input_monitor",t.input_monitor);
                    // Port presence/channel only: physical port keys/device identifiers stay local.
                    row.field("midi_input_enabled",!t.midi_input.empty());row.field("midi_input_channel",t.midi_channel);
                    row.field("midi_output_enabled",!t.midi_output.empty());row.field("midi_output_channel",t.midi_output_channel);row.field("midi_monitor",t.midi_monitor);
                    std::string sends="[";for(const auto& send:t.sends){Json s(1024);s.id("bus_id",send.bus);s.field("gain",send.gain);s.field("pre_fader",send.pre_fader);s.field("bypass",send.bypass);if(sends.size()>1)sends+=',';sends+=s.finish();}sends+=']';row.raw("sends",sends);
                }break;
            }
            case QueryKind::folders:{const auto& f=p.folders[index];match=identity(EntityKind::folder,f.id);if(!match)break;row.id("id",f.id);row.field("name",f.name);row.id("parent_id",f.parent);break;}
            case QueryKind::clips:{const auto& c=p.clips[index];match=identity(EntityKind::clip,c.id,c.track) && (!q.clip || c.id==*q.clip);
                if(!match)break;
                const auto start=clip_start(c,time),end=clip_end(c,time);
                if(q.samples && (end<=q.samples->start || start>=q.samples->end))match=false;
                if(q.ticks && (end<=time.to_samples(q.ticks->first) || start>=time.to_samples(q.ticks->second)))match=false;
                if(!match)break;
                row.id("id",c.id);row.id("track_id",c.track);row.field("name",c.name);row.field("midi",c.midi.has_value());row.field("sample_start",start);row.field("sample_end",end);
                if (c.midi) {
                  row.field("tick_start", c.midi->start);
                  row.field("tick_length", c.midi->length);
                  row.field("source_offset_ticks", c.midi->source_offset);
                  row.field("notes", c.midi->notes.size());
                  row.field("events", c.midi->events.size());
                } else
                  row.field("source_offset_samples", c.source_offset);
                break;
            }
            case QueryKind::midi_notes:case QueryKind::midi_events:{
                const auto& m=*clip->midi;match=(!q.track || clip->track==*q.track) && (!q.selected_only ||
                    (context_.selection.entities.empty() && context_.selection.sample_range) || selected(context_,EntityKind::clip,clip->id) || selected(context_,EntityKind::track,clip->track));
                if(!match)break;
                row.id("clip_id",clip->id);row.id("track_id",clip->track);
                if(q.kind==QueryKind::midi_notes){const auto& n=m.notes[index];if(q.id && n.id!=*q.id)match=false;
                    const Tick local_start=std::max(n.start,m.source_offset),local_end=std::min(n.start+n.length,m.source_offset+m.length);
                    const Tick start=m.start+local_start-m.source_offset,end=m.start+local_end-m.source_offset;
                    if(local_end<=local_start || !overlap(start,end,q,time))match=false;
                    if(!match)break;
                    row.id("id",n.id);row.field("source_start_ticks",n.start);row.field("source_length_ticks",n.length);row.field("project_start_ticks",start);row.field("project_end_ticks",end);
                    row.field("pitch",n.pitch);row.field("velocity",n.velocity);row.field("channel",n.channel);
                }else{const auto& e=m.events[index];const Tick start=m.start+e.start-m.source_offset;
                    if((q.id && e.id!=*q.id) || e.start<m.source_offset || e.start>=m.source_offset+m.length || !point(start,q,time))match=false;
                    if(!match)break;
                    row.id("id",e.id);row.field("source_start_ticks",e.start);row.field("project_start_ticks",start);row.field("kind",e.kind);row.field("channel",e.channel);row.field("data1",e.data1);row.field("data2",e.data2);
                }break;
            }
            case QueryKind::chords:{const auto& c=p.chords[index];match=identity(EntityKind::chord,c.id) && overlap(c.start,c.end,q,time);if(!match)break;row.id("id",c.id);row.field("symbol",c.symbol);row.field("tick_start",c.start);row.field("tick_end",c.end);if(c.color)row.field("color_rgb",*c.color);break;}
            case QueryKind::sections:{const auto& s=p.sections[index];match=identity(EntityKind::section,s.id) && overlap(s.start,s.end,q,time);if(!match)break;row.id("id",s.id);row.field("name",s.name);row.field("tick_start",s.start);row.field("tick_end",s.end);row.field("color_rgb",s.color);break;}
            case QueryKind::markers:{const auto& m=p.markers[index];match=identity(EntityKind::marker,m.id) && point(m.tick,q,time);if(!match)break;row.id("id",m.id);row.field("name",m.name);row.field("tick",m.tick);row.field("kind",static_cast<int>(m.kind));if(m.color)row.field("color_rgb",*m.color);break;}
            case QueryKind::tempos:{const auto& t=p.time.tempos[index];const auto end=index+1<p.time.tempos.size()?p.time.tempos[index+1].tick:max_tick;match=overlap(t.tick,end,q,time);if(!match)break;row.field("tick",t.tick);row.field("bpm",t.bpm);break;}
            case QueryKind::meters:{const auto& m=p.time.meters[index];const auto start=time.to_ticks(MusicalPosition{m.bar,1,0});const auto end=index+1<p.time.meters.size()?time.to_ticks(MusicalPosition{p.time.meters[index+1].bar,1,0}):max_tick;
                match=overlap(start,end,q,time);if(!match)break;row.field("bar",m.bar);row.field("tick",start);row.field("numerator",m.numerator);row.field("denominator",m.denominator);break;}
            case QueryKind::ara_capabilities:{const auto& c=ara_.capabilities[index];match=!q.id||c.id==*q.id;if(!match)break;row.id("plugin_id",c.id);row.field("name",c.name);row.field("version",c.version);row.field("available",c.available);row.field("can_attach",c.available);row.field("can_reanalyse",false);row.field("note_edit_api",false);break;}
            case QueryKind::ara_binding:case QueryKind::ara_status:{const auto& c=ara_.clips[index];match=(!q.clip||c.id==*q.clip)&&identity(EntityKind::clip,c.id,c.track);if(!match)break;row.id("clip_id",c.id);row.id("track_id",c.track);row.id("plugin_id",c.plugin);row.field("bound",c.bound);row.field("bypass",c.bypass);row.field("status",c.status);row.field("can_open",c.can_open);row.field("can_attach",c.can_attach);row.field("can_remove",c.can_remove);row.field("can_reanalyse",false);row.field("note_edit_api",false);break;}
            case QueryKind::plugins: {
              const auto &s = slots[index];
              const auto &f = *s.insert;
              match = (!q.id || f.id == *q.id) && (!q.plugin || f.id == *q.plugin) &&
                      (!q.track || s.track == q.track) && (!q.master || !s.track) &&
                      (!q.selected_only || (s.track && selected(context_, EntityKind::track, *s.track)));
              if (!match)
                break;
              row.id("id", f.id);
              row.id("track_id", s.track);
              row.field("master", !s.track);
              row.field("slot", s.index);
              row.field("kind", static_cast<int>(f.kind));
              row.field("name", f.plugin_name);
              if (f.kind == InsertKind::vst3)
                row.field("class_id", f.class_id);
              else
                row.raw("class_id", "null");
              row.field("bypass", f.bypass);
              row.field("stored_vst_parameter_count", f.parameters.size());
              row.field("native_controls_available", f.kind != InsertKind::vst3);
              row.field("parameter_source", "authored_project_state");
              break;
            }
            case QueryKind::plugin_parameters:{row.id("plugin_id",parameter_slot->insert->id);row.id("track_id",parameter_slot->track);row.field("source","authored_project_state");
                if(parameter_slot->insert->kind==InsertKind::vst3){const auto& parameter=parameter_slot->insert->parameters[index];row.field("parameter_id",parameter.id);row.field("normalized_value",parameter.value);}
                else{const auto& parameter=native_values[index];row.field("descriptor_index",parameter.id);row.field("name",parameter.name);row.field("value",parameter.value);row.field("unit",parameter.unit);}break;}
            case QueryKind::graph_parameters:{const auto& parameter=parameter_node->parameters[index];row.id("node_id",parameter_node->id);row.field("parameter_id",parameter.id);row.field("value",parameter.value);row.field("source","authored_graph_state");break;}
            case QueryKind::graph_nodes:{const auto& n=g.nodes[index];match=!q.id || n.id==*q.id;if(!match)break;row.id("id",n.id);row.field("format",static_cast<int>(n.format));
                if(n.format==processing::ProcessorFormat::vst3)row.field("class_id",n.plugin.class_id);else row.raw("class_id","null");
                row.field("bypass",n.bypass);row.field("stored_parameter_count",n.parameters.size());break;}
            case QueryKind::graph_edges:{const auto& e=g.edges[index];row.id("from_id",e.from);row.id("to_id",e.to);row.field("gain",e.gain);break;}
            case QueryKind::graph_outputs:row.id("node_id",g.outputs[index]);break;
            case QueryKind::midi_routes:{const auto& r=g.midi_routes[index];row.id("source_port_id",r.source_port);row.id("target_node_id",r.target_node);row.field("input_channel",r.input_channel);row.field("output_channel",r.output_channel);row.field("transpose",r.transpose);break;}
            case QueryKind::live_bindings:{const auto& b=bindings_[index];const auto* s=find_id(p.sections,b.section);match=s && (!q.selected_only || (context_.selection.entities.empty() && context_.selection.sample_range) || selected(context_,EntityKind::section,b.section)) && (!s || overlap(s->start,s->end,q,time));row.id("section_id",b.section);row.id("patch_id",b.patch);break;}
            case QueryKind::live_actions:{const auto& a=actions_[index];const auto* m=find_id(p.markers,a.marker);match=(!q.id || a.id==*q.id) && m && (!q.selected_only || (context_.selection.entities.empty() && context_.selection.sample_range) || selected(context_,EntityKind::marker,a.marker)) && (!m || point(m->tick,q,time));row.id("id",a.id);row.id("marker_id",a.marker);row.id("port_id",a.port);row.field("kind",static_cast<int>(a.event.kind));row.field("channel",a.event.channel);row.field("data1",a.event.data1);row.field("data2",a.event.data2);break;}
            case QueryKind::live_patches:{const auto& patch=patches_[index];match=!q.id || patch.id==*q.id;if(!match)break;row.id("id",patch.id);row.id("graph_id",patch.graph);row.field("name",patch.name);break;}
            case QueryKind::midi_ports:{const auto& port=ports_[index];match=!q.id || port.id==*q.id;if(!match)break;row.id("id",port.id);row.field("name",port.name);row.field("input",port.input);row.field("output",port.output);break;}
            default:break;
            }
            if(match){const auto value=row.finish();
                Page probe=page;++probe.returned;++probe.scanned;probe.next_cursor=index+1;probe.has_more=probe.next_cursor<page.source_items;
                // Measure the small envelope, without repeatedly copying the accumulated array.
                const auto overhead=page_json(probe,q,"[]").size()-2;
                const auto array_bytes=items.size()+(items.size()>1?1u:0u)+value.size()+1;
                if (array_bytes > q.byte_budget - overhead) {
                  page.byte_limited = true;
                  break;
                }
                if (items.size() > 1)
                  items += ',';
                items += value;
                ++page.returned;
            }
            ++page.scanned;page.next_cursor=index+1;
        }
        page.has_more=page.next_cursor<page.source_items;items+=']';
        if(page.byte_limited && page.returned==0 && page.scanned==0){page.error=Error::response_too_large;return page;}
        page.json=page_json(page,q,items);
    }catch(const SizeLimit&){page.error=Error::response_too_large;page.byte_limited=true;page.has_more=true;}
     catch(const std::exception&){page.error=Error::invalid_input;}
    if(!access_.valid()){page.error=Error::permission_denied;page.json.clear();page.returned=0;}
    return page;
}
SnapshotResult ReadView::snapshot(const std::vector<Query>& queries,SnapshotBudget budget,ReadPolicy policy) const {
    if(!access_.valid())return {Error::permission_denied,{}};
    if(queries.empty() || queries.size()>32 || budget.bytes<1024 || budget.bytes>max_bytes || budget.tokens<1024 || budget.tokens>max_bytes)return {Error::invalid_input,{}};
    const auto cap=std::min(budget.bytes,budget.tokens);bool truncated{};std::vector<Page> emitted;
    try{
        const auto header=context_json(context_,cap);std::string pages="[";
        std::string names="[";for(const auto& q:queries){if(!valid_query(q) || query_name(q.kind).empty())return {Error::invalid_input,{}};
            if(q.kind==QueryKind::automation || q.kind==QueryKind::analysis || q.kind==QueryKind::live_performance)return {Error::unsupported,{}};
            if ((q.kind == QueryKind::plugin_parameters || q.kind == QueryKind::graph_parameters) &&
                (!policy.stored_plugin_parameters || !access_.stored_plugin_parameters))
              return {Error::permission_denied, {}};
            if (names.size() > 1)
              names += ',';
            names += '"';
            names += query_name(q.kind);
            names += '"';
        }
        names += ']';
        auto envelope=[&](std::string_view values,bool truncated){Json j(cap);j.field("snapshot_version",1);j.raw("context",header);j.field("truncated",truncated);
            j.field("requested_queries",queries.size());j.raw("requested_query_names",names);j.field("emitted_queries",emitted.size());j.field("token_budget_basis","utf8_bytes_upper_bound");j.raw("pages",values);return j.finish();};
        (void)envelope("[]",false);
        for(auto q:queries){
            const auto base=envelope(pages+"]",true).size();
            if(base+512>=cap){truncated=true;break;}
            q.byte_budget=std::min(q.byte_budget,cap-base-1);auto page=query(q,policy);
            if(page.error==Error::response_too_large){truncated=true;break;}
            if(page.error!=Error::none)return {page.error,{}};
            std::string candidate=pages;if(candidate.size()>1)candidate+=',';candidate+=page.json;candidate+=']';
            try{(void)envelope(candidate,truncated || page.has_more);}catch(const SizeLimit&){truncated=true;break;}
            candidate.pop_back();pages=std::move(candidate);truncated=truncated || page.has_more;emitted.push_back(std::move(page));
        }
        pages+=']';truncated=truncated || emitted.size()!=queries.size();auto payload=envelope(pages,truncated);
        if(!access_.valid())return {Error::permission_denied,{}};
        return {Error::none,std::shared_ptr<const Snapshot>(new Snapshot(context_,std::move(payload),truncated,std::move(emitted),access_))};
    }catch(const SizeLimit&){return {Error::response_too_large,{}};}catch(const std::exception&){return {Error::invalid_input,{}};}
}
std::string ReadView::analysis(const JobControl& control,std::string_view media) const {
    auto check=[&]{if(control.should_stop()||!access_.valid())throw std::runtime_error("Analysis cancelled or access revoked");};check();
    const auto& p=*project_;const Timeline time(p.time,p.sample_rate);
    const auto clipLimit=std::min<std::size_t>(p.clips.size(),128),sectionLimit=std::min<std::size_t>(p.sections.size(),16);
    std::string rows="[";std::size_t work{};bool partial=clipLimit<p.clips.size()||sectionLimit<p.sections.size()||std::count_if(p.clips.begin(),p.clips.end(),[](const auto& c){return !c.midi;})>16;
    auto section = [&](const Id &id, Tick start, Tick stop) {
      Json row(4096);
      std::size_t notes{}, clips{}, overlaps{};
      Tick covered{};
      int low = 128, high = -1;
      std::vector<std::pair<Tick, int>> events;
      for (std::size_t i = 0; i < clipLimit; ++i) {
        check();
        const auto &c = p.clips[i];
        const auto begin = c.midi ? c.midi->start : time.to_ticks(c.start),
                   end = c.midi ? c.midi->start + c.midi->length : time.to_ticks(c.start + c.length);
        if (end <= start || begin >= stop)
          continue;
        ++clips;
        if (c.midi)
          for (const auto &n : c.midi->notes) {
            if (++work > 131072) {
              partial = true;
              break;
            }
            if ((work & 255) == 0)
              check();
            const auto a = std::max(n.start, c.midi->source_offset),
                       b = std::min(n.start + n.length, c.midi->source_offset + c.midi->length);
            if (b <= a)
              continue;
            const auto s = std::max(start, c.midi->start + a - c.midi->source_offset),
                       e = std::min(stop, c.midi->start + b - c.midi->source_offset);
            if (e <= s)
              continue;
            ++notes;
            low = std::min(low, n.pitch);
            high = std::max(high, n.pitch);
            events.emplace_back(s, 1);
            events.emplace_back(e, -1);
          }
      }
      std::sort(events.begin(), events.end());
      int active{};
      Tick last = start;
      for (const auto &[tick, delta] : events) {
        if (active > 0)
          covered += tick - last;
        if (active > 1)
          overlaps += static_cast<std::size_t>(tick - last);
        active += delta;
        last = tick;
      }
      row.id("section_id", id);
      row.field("tick_start", start);
      row.field("tick_end", stop);
      row.field("clips_intersecting", clips);
      row.field("visible_note_intersections", notes);
      row.field("notes_per_quarter", static_cast<double>(notes) * ppq / static_cast<double>(stop - start));
      row.field("note_activity_ticks", covered);
      row.field("polyphonic_ticks", overlaps);
      if (high >= 0) {
        row.field("pitch_min", low);
        row.field("pitch_max", high);
      } else
        row.raw("pitch_min", "null");
      if (rows.size() > 1)
        rows += ',';
      rows += row.finish();
    };
    for(std::size_t i=0;i<sectionLimit;++i){const auto& s=p.sections[i];section(s.id,s.start,s.end);if(work>131072)break;}
    if(sectionLimit==0){Tick stop=ppq;for(std::size_t i=0;i<clipLimit;++i){const auto& c=p.clips[i];stop=std::max(stop,c.midi?c.midi->start+c.midi->length:time.to_ticks(c.start+c.length));}section({},0,stop);}rows+=']';
    std::string mix="[";for(std::size_t i=0;i<std::min<std::size_t>(p.tracks.size(),16);++i){check();const auto& t=p.tracks[i];Json row(2048);row.id("track_id",t.id);row.field("authored_gain_linear",t.mix.gain);row.field("authored_pan",t.mix.pan);row.field("mute",t.mix.mute);row.field("solo",t.mix.solo);row.id("output_track_id",t.output);row.field("send_count",t.sends.size());std::string sends="[";for(std::size_t s=0;s<std::min<std::size_t>(t.sends.size(),4);++s){const auto& value=t.sends[s];Json send(512);send.id("bus_id",value.bus);send.field("gain_linear",value.gain);send.field("pre_fader",value.pre_fader);send.field("bypass",value.bypass);if(sends.size()>1)sends+=',';sends+=send.finish();}sends+=']';row.raw("sends",sends);partial=partial||t.sends.size()>4;if(mix.size()>1)mix+=',';mix+=row.finish();}mix+=']';partial=partial||p.tracks.size()>16;
    Json report(32768);report.field("analysis_version",1);report.raw("context_key",key_json(context_.key));report.field("status",partial?"partial":"available");report.field("source","authored_project_and_offline_source_pcm");report.field("confidence","deterministic_metrics_not_musical_judgment");report.field("ticks_per_quarter",ppq);report.raw("sections",rows);report.raw("mixer",mix);report.raw("source_audio_statistics",media);report.field("audio_clip_limit",16);report.field("audio_frame_limit_per_clip",65536);report.field("audio_measurement_scope","dry_source_clip_prefix_only_not_instrument_or_processed_mix");report.field("spectral_status","unavailable_not_computed");report.field("processed_levels_status","unavailable_no_offline_render_requested");report.field("media_cache_basis","project_revision_and_source_identity_file_size_mtime");check();return report.finish();
}
} // namespace mrs::ai
