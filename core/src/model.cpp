#include <mrs/core.hpp>
#include <mrs/arrangement.hpp>
#include <algorithm>
#include <filesystem>
#include <atomic>
#include <cmath>
#include <iomanip>
#include <random>
#include <sstream>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace mrs {
namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::invalid_argument(message);
}
}
Id new_id() {
    static std::atomic<std::uint64_t> counter{};
    std::random_device random;
    std::ostringstream out;
    out << "mrs-" << std::hex << std::setfill('0');
    for (int i = 0; i < 4; ++i) out << std::setw(8) << random();
    out << '-' << ++counter;
    return Id{out.str()};
}
void TimeMap::validate() const {
    require(!tempos.empty() && tempos.front().tick == 0, "tempo map must start at tick 0");
    Tick last = -1;
    for (const auto& p : tempos) {
        require(p.tick > last && p.tick <= max_tick, "tempo positions must be strictly increasing");
        require(std::isfinite(p.bpm) && p.bpm >= 1 && p.bpm <= 1000, "invalid tempo");
        last = p.tick;
    }
    require(!meters.empty() && meters.front().bar == 1, "meter map must start at bar 1");
    std::int64_t last_bar = 0;
    Tick start_tick = 0;
    MeterPoint previous;
    for (const auto& p : meters) {
        require(p.bar > last_bar && p.bar <= max_tick + 1, "invalid meter bar order");
        require(p.numerator >= 1 && p.numerator <= 64, "invalid meter numerator");
        require(p.denominator >= 1 && p.denominator <= 64 &&
                (p.denominator & (p.denominator - 1)) == 0, "invalid meter denominator");
        if (last_bar != 0) {
            start_tick += (p.bar - last_bar) * previous.numerator * (4 * ppq / previous.denominator);
            require(start_tick <= max_tick, "meter position out of range");
        }
        previous = p;
        last_bar = p.bar;
    }
}
void Track::Mix::validate() const {
    require(std::isfinite(gain) && gain >= minimum_gain && gain <= maximum_gain &&
        std::isfinite(pan) && pan >= minimum_pan && pan <= maximum_pan, "invalid track mix");
}
void CabIr::validate() const {
    require(name.size()<=4096 && sample_rate>=8000 && sample_rate<=192000 && (channels==1 || channels==2), "Cab IR requires mono/stereo at 8–192 kHz");
    require(!samples.empty() && samples.size()%channels==0 && samples.size()/channels<=sample_rate, "Cab IR length must be greater than zero and at most one second");
    bool nonzero=false; for(float v:samples){require(std::isfinite(v) && std::abs(v)<=16,"invalid Cab IR sample");nonzero|=v!=0;}require(nonzero,"Cab IR is silent");
    require(std::isfinite(mix)&&mix>=0&&mix<=1&&std::isfinite(low_cut)&&low_cut>=20&&low_cut<=20000&&std::isfinite(high_cut)&&high_cut>=20&&high_cut<=20000&&low_cut<high_cut,"invalid Cab IR controls");
}
void ReverbState::validate() const {
    require(std::isfinite(gate_threshold)&&gate_threshold>=-120&&gate_threshold<=-60,"invalid MR Reverb gate threshold");
    require(std::isfinite(pre_delay)&&pre_delay>=0&&pre_delay<=200&&std::isfinite(tone)&&tone>=-6&&tone<=6&&std::isfinite(mix)&&mix>=0&&mix<=1,"invalid MR Reverb controls");
    const auto reference=[](const ReverbReference& r){
        require(r.relative.size()<=32768&&r.absolute.size()<=32768,"invalid IR path");
        require(r.relative.find('\0')==std::string::npos&&r.absolute.find('\0')==std::string::npos,"invalid IR path characters");
        require(r.digest.empty()||(r.digest.size()==24&&r.digest.starts_with("fnv1a64:")&&r.digest.substr(8).find_first_not_of("0123456789abcdef")==std::string::npos),"invalid IR fingerprint");
        require(r.bytes<=128*1024*1024,"invalid IR byte budget");
    };
    reference(first);reference(second);require(name.size()<=4096&&quad_map<=1,"invalid IR identity");
    if(first.digest.empty()){require(first==ReverbReference{}&&second==ReverbReference{}&&name.empty()&&sample_rate==0&&channels==0&&frames==0&&quad_map==0,"incomplete IR identity");return;}
    require(!first.relative.empty()||!first.absolute.empty(),"IR path required");
    require(sample_rate>=8000&&sample_rate<=192000&&frames>0&&(channels==1||channels==2||channels==4)&&frames<=64*1024*1024/(sizeof(float)*channels),"invalid IR metadata");
    if(!second.digest.empty())require(channels==4&&(!second.relative.empty()||!second.absolute.empty()),"invalid stereo IR pair");
    else require(second==ReverbReference{},"incomplete IR pair");
    require(channels!=4||!second.digest.empty()||quad_map==1,"quad channel map must be confirmed");
}
void validate_cab_metadata(const std::map<std::string,std::string>& metadata){
    require(metadata.size()<=128,"Too many Cab metadata fields");
    std::size_t bytes{};
    for(const auto& [key,value]:metadata){
        require(!key.empty()&&key.size()<=128&&value.size()<=4096&&key.find('\0')==std::string::npos&&value.find('\0')==std::string::npos,"Invalid Cab metadata");
        bytes+=key.size()+value.size();
    }
    require(bytes<=16384,"Cab metadata byte budget exceeded");
    if(const auto it=metadata.find("ir_role");it!=metadata.end())require(it->second=="cabinet"||it->second=="full_combo"||it->second=="unknown","Invalid Cab role");
}
const ModuleDescriptor* rig_module_descriptor(const RigModule& module) noexcept {
    for(const auto& d:rig_modules)if(d.type_id==module.type_id&&d.version==module.version&&module.opaque_state.empty())return &d;
    return nullptr;
}
RigModule make_rig_module(std::string_view type){
    RigModule m;m.id=new_id();m.type_id=type;
    const auto* descriptor=rig_module_descriptor(m);require(descriptor!=nullptr,"Unavailable Rig module");
    for(const auto& p:descriptor->parameters){m.parameters.push_back({p.id,p.initial});}
    return m;
}
void RigState::validate() const {
    require(version==1||version==2,"Unsupported RigState version");
    require(std::isfinite(input_gain)&&input_gain>=0&&input_gain<=4&&std::isfinite(output_gain)&&output_gain>=0&&output_gain<=4,"Invalid Rig input/output gain");
    require(pre_fx.size()<=rig_lane_capacity&&post_fx.size()<=rig_lane_capacity,"Rig lane capacity exceeded");
    require(version>=2||(pre_fx.empty()&&post_fx.empty()),"Rig v1 has no modules");
    std::set<std::string> ids;
    for(const auto* lane:{&pre_fx,&post_fx})for(const auto& m:*lane){
        require(!m.id.value.empty()&&m.id.value.size()<=128&&m.id.value.find_first_of("\r\n")==std::string::npos&&m.id.value.find('\0')==std::string::npos&&ids.insert(m.id.value).second,"Invalid or duplicate Rig module identity");
        require(!m.type_id.empty()&&m.type_id.size()<=256&&m.type_id.find_first_of("\r\n")==std::string::npos&&m.type_id.find('\0')==std::string::npos&&m.version>0,"Invalid Rig module type/version");
        require(m.opaque_state.size()<=65536&&m.parameters.size()<=64,"Rig module state budget exceeded");
        const auto* descriptor=rig_module_descriptor(m);std::set<std::uint32_t> parameters;
        if(descriptor)require(m.parameters.size()==descriptor->parameters.size(),"Incomplete Rig module parameters");
        for(const auto& p:m.parameters){require(std::isfinite(p.value)&&parameters.insert(p.id).second,"Invalid Rig module parameter");if(descriptor){
            const auto at=std::find_if(descriptor->parameters.begin(),descriptor->parameters.end(),[&](const auto& info){return info.id==p.id;});
            require(at!=descriptor->parameters.end()&&p.value>=at->minimum&&p.value<=at->maximum,"Invalid Rig module parameter value");
        }}
    }
    require(name.size()<=256&&name.find_first_of("\r\n")==std::string::npos&&name.find('\0')==std::string::npos,"Invalid rig name");
    require(artwork.size()<=32768&&artwork.find('\0')==std::string::npos&&artwork_rights.size()<=1024,"Invalid Rig artwork");
    require(artwork_rights.find('\0')==std::string::npos,"Invalid Rig artwork rights text");
    require(artwork.empty()||(!artwork_rights.empty()&&std::filesystem::path(std::u8string(artwork.begin(),artwork.end())).is_absolute()),"Rig artwork needs an absolute path and rights note");
}
void NativeInsert::validate() const {
    require(kind==InsertKind::rig||rig==RigState{},"RigState belongs to MR Rig only");
    if(kind==InsertKind::rig){rig.validate();require(ir.samples.empty()&&component_state.empty()&&controller_state.empty(),"Rig media stays external");}
    validate_cab_metadata(cab_metadata);
    require(wav_core(kind)||cab_metadata.empty(),"Cab metadata belongs to MR Cab only");
    require(preset_name.size()<=1024 && preset_name.find_first_of("\r\n") == std::string::npos,"invalid preset label");
    if(kind==InsertKind::cab_ir)ir.validate();
    if(kind==InsertKind::reverb||wav_core(kind))reverb.validate();
    if(wav_core(kind)){require(reverb.channels<=2&&reverb.frames<=reverb.sample_rate,"Cab requires mono/stereo <=1s");require(cab_collection_id.size()<=4096&&cab_variant_id.size()<=4096,"Invalid Cab identity");require(std::isfinite(ir.mix)&&ir.mix>=0&&ir.mix<=1&&std::isfinite(ir.low_cut)&&ir.low_cut>=20&&ir.low_cut<=20000&&std::isfinite(ir.high_cut)&&ir.high_cut>=20&&ir.high_cut<=20000&&ir.low_cut<ir.high_cut,"Invalid Cab controls");}
    for (const auto& b : bands) require(std::isfinite(b.frequency) && b.frequency>=20 && b.frequency<=20000 && std::isfinite(b.gain) && b.gain>=-24 && b.gain<=24 && std::isfinite(b.q) && b.q>=0.1f && b.q<=10,"invalid EQ band");
    require(component_state.size()<=1024*1024 && controller_state.size()<=1024*1024 && parameters.size()<=4096,"plugin state budget exceeded");
    if (kind==InsertKind::vst3) {
        require(!plugin_path.empty() && plugin_path.size()<=32768 && plugin_name.size()<=4096 && class_id.size()==32 && class_id.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos,"invalid VST3 identity");
        for (std::size_t i=0;i<parameters.size();++i) { const auto& p=parameters[i]; require(std::isfinite(p.value) && p.value>=0 && p.value<=1,"invalid VST3 parameter"); for(std::size_t j=0;j<i;++j) require(p.id!=parameters[j].id,"duplicate VST3 parameter"); }
    }
    require(kind >= InsertKind::gain && kind <= InsertKind::rig,"invalid insert kind");
    require(std::isfinite(gain) && (kind == InsertKind::eq ? gain >= -24 && gain <= 24 : gain >= 0 && gain <= 4),"invalid insert gain");
    require(std::isfinite(frequency) && frequency >= 20 && frequency <= 20000 && std::isfinite(q) && q >= 0.1f && q <= 10,"invalid filter frequency/Q");
}
void Project::validate() const {
    require(version == schema_version, "unsupported project schema");
    require(ara_bindings.size()<=8,"ARA supports up to eight source bindings");
    std::unordered_set<std::string> ara_sources,ara_ids;std::size_t ara_bytes{};
    for(const auto& b:ara_bindings){
        for(const auto* s:{&b.source,&b.persistent_id,&b.module,&b.class_id,&b.name,&b.factory_id,&b.version,&b.archive_id,&b.document_id})require(s->size()<=32768&&s->find('\0')==std::string::npos,"Invalid ARA metadata");
        if(!b.document_id.empty())for(const auto& other:ara_bindings)if(other.document_id==b.document_id)
            require(other.module==b.module&&other.class_id==b.class_id&&other.factory_id==b.factory_id&&other.version==b.version&&other.archive_id==b.archive_id,"ARA document contains different plugin identities");
        require(!b.source.empty()&&!b.persistent_id.empty()&&!b.module.empty()&&!b.class_id.empty(),"Incomplete ARA binding");
        require(ara_sources.insert(b.source).second&&ara_ids.insert(b.persistent_id).second,"Duplicate ARA source or persistent ID");
        for(const auto* data:{&b.archive,&b.component_state,&b.controller_state}){require(data->size()<=1024*1024,"ARA state exceeds 1 MiB per blob");ara_bytes+=data->size();}
    }
    require(ara_bytes<=4*1024*1024,"ARA project state exceeds 4 MiB");
    require(sample_rate >= 8000 && sample_rate <= 768000, "invalid sample rate");
    time.validate();
    require(std::isfinite(master_gain) && master_gain >= minimum_master_gain && master_gain <= maximum_master_gain, "invalid master gain");
    const auto physical = [](const std::vector<int>& outputs) {
        require(outputs.size() <= 2,"output route must be mono/stereo");
        for (const auto index : outputs) require(index >= 0 && index < 64,"invalid physical output index");
        require(outputs.size() != 2 || outputs[0] != outputs[1],"duplicate physical output");
    };
    physical(master_outputs);
    std::unordered_set<std::string> ids;
    const auto add_id = [&ids](const Id& entity) {
        require(!entity.value.empty() && entity.value.size() <= 128, "invalid entity ID");
        require(ids.insert(entity.value).second, "duplicate entity ID");
    };
    std::size_t insert_count{};
    const auto inserts = [&](const std::vector<NativeInsert>& chain) {
        require(chain.size() <= 8,"insert chain supports up to eight effects"); insert_count += chain.size();
        require(insert_count <= 32,"project supports up to 32 native inserts");
        for (const auto& effect : chain) { effect.validate(); add_id(effect.id); }
    };
    inserts(master_inserts);
    add_id(id);
    std::unordered_map<std::string, const Folder*> folder_by_id;
    for (const auto& folder : folders) {
        add_id(folder.id);
        folder_by_id.emplace(folder.id.value, &folder);
    }
    for (const auto& folder : folders) {
        std::unordered_set<std::string> visited{folder.id.value};
        auto parent = folder.parent;
        while (parent) {
            require(folder_by_id.contains(parent->value), "missing parent folder");
            require(visited.insert(parent->value).second, "folder hierarchy cycle");
            parent = folder_by_id.at(parent->value)->parent;
        }
    }
    std::unordered_set<std::string> track_ids;
    for (const auto& track : tracks) {
        track.mix.validate(); physical(track.hardware_outputs); inserts(track.inserts);
        require(!track.fx_channel||track.kind==TrackKind::bus,"FX role requires bus");
        require(track.kind != TrackKind::midi || track.inserts.empty(),"audio inserts require an audio track or bus");
        require(track.hardware_outputs.empty() || (!track.output && track.kind != TrackKind::midi),"hardware output conflicts with bus/MIDI routing");
        add_id(track.id);
        require(track.kind == TrackKind::audio || track.kind == TrackKind::midi || track.kind == TrackKind::bus || track.kind == TrackKind::instrument, "invalid track kind");
        require(track.midi_input.size()<=256 && track.midi_channel>=-1 && track.midi_channel<=15,"invalid MIDI input");
        require(track.midi_output.size()<=256&&track.midi_output_channel>=-1&&track.midi_output_channel<=15,"invalid MIDI output");
        require(track.kind==TrackKind::instrument||(track.midi_output.empty()&&track.midi_output_channel==-1),"MIDI output requires an instrument track");
        require(track.midi_output.empty()||track.midi_input!=track.midi_output,"MIDI feedback: input and output cannot use the same endpoint");
        require(track.kind==TrackKind::instrument || (track.midi_input.empty()&&track.midi_channel==-1&&track.midi_monitor),"MIDI input requires an instrument track");
        require(track.kind!=TrackKind::instrument || track.inserts.empty() || track.inserts.front().kind==InsertKind::vst3,"instrument must be the first VST3 insert");
        require(!track.folder || folder_by_id.contains(track.folder->value), "missing track folder");
        require(!track.color || *track.color<=0xFFFFFF,"invalid channel RGB");
        track_ids.insert(track.id.value);
    }
    std::unordered_map<std::string, const Track*> channels;
    for (const auto& track : tracks) channels.emplace(track.id.value,&track);
    std::unordered_set<std::string> linked_buses;
    for(const auto& folder:folders)if(folder.bus){
        require(channels.contains(folder.bus->value)&&channels.at(folder.bus->value)->kind==TrackKind::bus,"Missing folder Bus");
        require(linked_buses.insert(folder.bus->value).second,"Bus linked to multiple folders");
    }
    std::unordered_map<std::string,int> color;
    for (const auto& track : tracks) {
        require(track.kind != TrackKind::midi || (!track.output && track.sends.empty()), "MIDI audio routing is not supported");
        require(!track.input_stereo || (track.kind == TrackKind::audio && track.input >= 0 && track.input < 63),"stereo input requires adjacent physical channels");
        require(!track.input_monitor || track.kind == TrackKind::audio,"only audio tracks can monitor input");
        require(track.input >= -2 && track.input < 64 && (track.kind == TrackKind::audio || track.input == -2), "invalid track input");
        require(track.sends.size() <= 8, "at most eight sends per channel");
        std::unordered_set<std::string> destinations;
        for (const auto& send : track.sends) {
            require(std::isfinite(send.gain) && send.gain >= 0 && send.gain <= 16, "invalid send gain");
            require(destinations.insert(send.bus.value).second, "duplicate send destination");
        }
    }
    // Iterative DFS avoids recursion limits for imported projects. Main output and
    // sends participate in the same DAG, including disabled/zero-level sends.
    for (const auto& root : tracks) {
        std::vector<std::pair<const Track*,std::size_t>> stack;
        if (color[root.id.value] == 2) continue;
        color[root.id.value] = 1; stack.emplace_back(&root,0);
        while (!stack.empty()) {
            auto& [track,index] = stack.back();
            if (index == track->sends.size()+1) { color[track->id.value] = 2; stack.pop_back(); continue; }
            const auto edge_index = index++;
            const auto destination = edge_index == 0 ? track->output : std::optional<Id>{track->sends[edge_index-1].bus};
            if (!destination) continue;
            require(channels.contains(destination->value), "missing output/send bus");
            const auto* bus = channels.at(destination->value);
            require(bus->kind == TrackKind::bus, "output/send destination must be a bus");
            require(color[bus->id.value] != 1, "audio routing cycle");
            if (color[bus->id.value] == 0) { color[bus->id.value] = 1; stack.emplace_back(bus,0); }
        }
    }
    std::size_t midi_notes{},midi_events{};
    for (const auto& clip : clips) {
        add_id(clip.id);
        require(!clip.color||*clip.color<=0xFFFFFF,"invalid clip color");
        require(track_ids.contains(clip.track.value), "missing clip track");
        require(channels.at(clip.track.value)->kind != TrackKind::bus, "bus cannot contain clips");
        if (clip.midi) {
            const auto& m=*clip.midi;
            require(channels.at(clip.track.value)->kind==TrackKind::instrument,"MIDI clip requires an instrument track");
            require(!clip.file_tempo&&!clip.stretch&&clip.fade_in==0&&clip.fade_out==0&&clip.start==0 && clip.length==1 && clip.source_offset==0 && clip.source.empty(),"MIDI clip sample fields must be defaults");
            require(m.start>=0 && m.start<=max_tick && m.length>0 && m.length<=max_tick-m.start,"invalid MIDI clip range");
            require(m.source_offset>=0 && m.source_offset<=max_tick-m.length,"invalid MIDI source offset");
            midi_notes+=m.notes.size();
            require(m.notes.size()<=max_project_midi_notes && midi_notes<=max_project_midi_notes,"project supports up to 32768 MIDI notes");
            for(const auto& n:m.notes){add_id(n.id);require(n.start>=0 && n.start<=max_tick && n.length>0 && n.length<=max_tick-n.start,"invalid MIDI note range");require(n.pitch>=0&&n.pitch<=127&&n.velocity>=1&&n.velocity<=127&&n.channel>=0&&n.channel<=15,"invalid MIDI note data");}
            midi_events+=m.events.size();require(m.events.size()<=8192&&midi_events<=65536,"too many MIDI channel events");
            for(const auto& e:m.events){add_id(e.id);require(e.start>=0&&e.start<=max_tick&&e.kind>=2&&e.kind<=6&&e.channel>=0&&e.channel<16&&e.data1>=0&&e.data1<128&&e.data2>=0&&e.data2<128,"invalid MIDI channel event");}
            continue;
        }
        require(clip.start >= 0 && clip.start <= max_sample, "invalid clip start");
        require(clip.length > 0 && clip.length <= max_sample - clip.start, "invalid clip length");
        require(clip.fade_in>=0&&clip.fade_out>=0&&clip.fade_in<=clip.length&&clip.fade_out<=clip.length,"invalid audio fade duration");
        require(!clip.file_tempo||(std::isfinite(*clip.file_tempo)&&*clip.file_tempo>=1&&*clip.file_tempo<=999),"Invalid file tempo");
        validate_audio_stretch(clip);
        require(!clip.stretch||std::none_of(ara_bindings.begin(),ara_bindings.end(),[&](const auto& b){return b.source==clip.source;}),"Detach native time / pitch before attaching ARA processing");
        require(clip.source_offset >= 0 && clip.source_offset <= max_sample - audio_source_span(clip),
                "invalid source offset");
    }
    const auto lane = [&add_id](const auto& events) {
        Tick last_end = 0;
        for (const auto& event : events) {
            add_id(event.id);
            require(event.start >= last_end && event.start >= 0, "unordered or overlapping musical lane");
            require(event.end > event.start && event.end <= max_tick, "invalid musical range");
            last_end = event.end;
        }
    };
    lane(chords);
    lane(sections);
    for (const auto& chord : chords) {
        require(!chord.symbol.empty() && chord.symbol.size() <= 256, "invalid chord symbol");
        require(!chord.color||*chord.color<=0xFFFFFF,"invalid chord color");
    }
    for (const auto& section : sections) {
        require(!section.name.empty(), "empty section name");
        require(section.color <= 0xFFFFFF, "invalid section color");
    }
    for (const auto& marker : markers) {
        add_id(marker.id);
        require(marker.tick >= 0 && marker.tick <= max_tick, "invalid marker position");
        const auto kind = static_cast<int>(marker.kind);
        require(kind >= 0 && kind <= static_cast<int>(MarkerKind::navigation), "invalid marker kind");
        require(!marker.color || *marker.color<=0xFFFFFF,"invalid marker color");
    }
}
Connection::Connection(std::function<void()> disconnect) : disconnect_(std::move(disconnect)) {}
Connection::Connection(Connection&& other) noexcept
    : disconnect_(std::exchange(other.disconnect_, {})) {}
Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        disconnect();
        disconnect_ = std::exchange(other.disconnect_, {});
    }
    return *this;
}
Connection::~Connection() { disconnect(); }
void Connection::disconnect() noexcept {
    if (disconnect_) {
        auto action = std::exchange(disconnect_, {});
        try { action(); } catch (...) {}
    }
}
} // namespace mrs
