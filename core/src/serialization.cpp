#include <algorithm>
#include <mrs/core.hpp>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace mrs {
namespace {
constexpr std::size_t max_bytes = 16 * 1024 * 1024;
constexpr std::size_t max_entities = 100000;
void tag(std::istream& in, std::string_view expected) {
    std::string value;
    if (!(in >> value) || value != expected) throw std::invalid_argument("invalid snapshot section");
}
std::size_t count(std::istream& in) {
    std::int64_t value{};
    if (!(in >> value) || value < 0 || value > static_cast<std::int64_t>(max_entities))
        throw std::invalid_argument("invalid snapshot count");
    return static_cast<std::size_t>(value);
}
std::string quoted(std::istream& in) {
    std::string value;
    in >> std::ws;
    if (in.peek() != '"' || !(in >> std::quoted(value)))
        throw std::invalid_argument("invalid snapshot string");
    return value;
}
std::optional<Id> parent(std::istream& in) {
    auto value = quoted(in);
    return value.empty() ? std::nullopt : std::optional<Id>{Id{std::move(value)}};
}
void check_stream(std::istream& in) {
    if (!in) throw std::invalid_argument("truncated or malformed snapshot");
}
template<class T> void section(std::ostream& out, std::string_view name, const std::vector<T>& values) {
    if (values.size() > max_entities) throw std::invalid_argument("snapshot entity limit exceeded");
    out << name << ' ' << values.size() << '\n';
}
void write_inserts(std::ostream& out, const std::vector<NativeInsert>& inserts) {
    out << "INSERTS " << inserts.size() << '\n';
    const auto blob=[&](const std::vector<std::byte>& data) { static constexpr char hex[]="0123456789abcdef"; std::string s; for(auto b:data) { auto v=std::to_integer<unsigned>(b);s+=hex[v>>4];s+=hex[v&15]; } out<<std::quoted(s)<<' '; };
    for (const auto& fx : inserts) {
        out << std::quoted(fx.id.value) << ' ' << static_cast<int>(fx.kind) << ' ' << fx.gain << ' ' << fx.frequency << ' ' << fx.q << ' ' << fx.bypass << '\n';
        for(const auto& b:fx.bands){out<<b.frequency<<' '<<b.gain<<' '<<b.q<<' '<<b.enabled<<' ';}
        out<<'\n';
        out<<std::quoted(fx.plugin_path)<<' '<<std::quoted(fx.class_id)<<' '<<std::quoted(fx.plugin_name)<<' ';blob(fx.component_state);blob(fx.controller_state);out<<fx.parameters.size();for(const auto& p:fx.parameters) out<<' '<<p.id<<' '<<p.value;out<<'\n';
        const auto& ir=fx.ir;out<<std::quoted(ir.name)<<' '<<ir.sample_rate<<' '<<ir.channels<<' '<<ir.mix<<' '<<ir.low_cut<<' '<<ir.high_cut<<' '<<ir.invert<<' '<<ir.samples.size();for(float v:ir.samples)out<<' '<<v;out<<'\n';
        if(fx.kind==InsertKind::rig){const auto& r=fx.rig;out<<"RIG_STATE "<<r.version<<' '<<r.input_gain<<' '<<r.output_gain<<' '<<std::quoted(r.name)<<' '<<std::quoted(r.artwork)<<' '<<std::quoted(r.artwork_rights)<<' '<<r.pre_fx.size()<<' '<<r.post_fx.size()<<'\n';
            for(const auto* lane:{&r.pre_fx,&r.post_fx})for(const auto& m:*lane){out<<"RIG_MODULE "<<std::quoted(m.id.value)<<' '<<std::quoted(m.type_id)<<' '<<m.version<<' '<<m.bypass<<' '<<std::quoted(m.opaque_state)<<' '<<m.parameters.size();for(const auto& p:m.parameters)out<<' '<<p.id<<' '<<p.value;out<<'\n';}}
        out<<"PRESET_NAME "<<std::quoted(fx.preset_name)<<'\n';
        if(wav_core(fx.kind)){out<<"CAB_SELECTION "<<(fx.cab_metadata.empty()?1:2)<<' '<<std::quoted(fx.cab_collection_id)<<' '<<std::quoted(fx.cab_variant_id);
            if(!fx.cab_metadata.empty()){out<<' '<<fx.cab_metadata.size();for(const auto& [k,v]:fx.cab_metadata)out<<' '<<std::quoted(k)<<' '<<std::quoted(v);}out<<'\n';}
        if(fx.kind==InsertKind::reverb||wav_core(fx.kind)){const auto& r=fx.reverb;out<<"REVERB 2 "<<r.pre_delay<<' '<<r.tone<<' '<<r.mix<<' '<<std::quoted(r.name)<<' '<<r.sample_rate<<' '<<r.channels<<' '<<r.frames<<' '<<r.quad_map<<' '<<r.gate_enabled<<' '<<r.gate_threshold<<'\n';
            for(const auto* f:{&r.first,&r.second})out<<std::quoted(f->relative)<<' '<<std::quoted(f->absolute)<<' '<<std::quoted(f->digest)<<' '<<f->bytes<<'\n';}
    }
}
std::vector<NativeInsert> read_inserts(std::istream& in, std::uint32_t version) {
    tag(in,"INSERTS"); const auto n=count(in); if (n>8) throw std::invalid_argument("too many inserts");
    std::vector<NativeInsert> result;
    for (std::size_t i=0;i<n;++i) {
        NativeInsert fx; fx.id={quoted(in)}; int kind{},bypass{};
        in >> kind >> fx.gain >> fx.frequency >> fx.q >> bypass; check_stream(in);
        if (kind<0 || kind>(version>=24 ? 9 : version>=23 ? 8 : version>=16 ? 7 : version>=10 ? 6 : version>=9 ? 5 : 3) || (bypass!=0 && bypass!=1)) throw std::invalid_argument("invalid insert kind/bypass");
        fx.kind=static_cast<InsertKind>(kind); fx.bypass=bypass!=0;
        if(version>=9) {
            for(auto& b:fx.bands) {int enabled{};in>>b.frequency>>b.gain>>b.q>>enabled;check_stream(in);if(enabled!=0 && enabled!=1) throw std::invalid_argument("invalid EQ enable");b.enabled=enabled!=0;}
            fx.plugin_path=quoted(in);fx.class_id=quoted(in);fx.plugin_name=quoted(in);
            const auto blob=[&]() { auto s=quoted(in);if(s.size()>2*1024*1024 || s.size()%2) throw std::invalid_argument("invalid plugin blob");std::vector<std::byte> data;const auto digit=[](char c)->unsigned {if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::invalid_argument("invalid plugin hex");};for(std::size_t j=0;j<s.size();j+=2)data.push_back(static_cast<std::byte>(digit(s[j])*16+digit(s[j+1])));return data;};
            fx.component_state=blob();fx.controller_state=blob();auto nparams=count(in);if(nparams>4096)throw std::invalid_argument("too many plugin parameters");for(std::size_t j=0;j<nparams;++j){InsertParameter p;in>>p.id>>p.value;check_stream(in);fx.parameters.push_back(p);}
        }
        if(version>=10){auto& ir=fx.ir;ir.name=quoted(in);int invert{};std::int64_t samples{};in>>ir.sample_rate>>ir.channels>>ir.mix>>ir.low_cut>>ir.high_cut>>invert>>samples;check_stream(in);if((invert!=0&&invert!=1)||samples<0||samples>384000)throw std::invalid_argument("invalid Cab IR snapshot");ir.invert=invert!=0;ir.samples.resize(static_cast<std::size_t>(samples));for(float& v:ir.samples)in>>v;check_stream(in);}
        if(fx.kind==InsertKind::rig){tag(in,"RIG_STATE");auto& r=fx.rig;in>>r.version>>r.input_gain>>r.output_gain;r.name=quoted(in);r.artwork=quoted(in);r.artwork_rights=quoted(in);const auto pre=count(in),post=count(in);if(pre>rig_lane_capacity||post>rig_lane_capacity||((version<25||r.version<2)&&(pre||post)))throw std::invalid_argument("Unsupported Rig modules");
            const auto read=[&](std::vector<RigModule>& lane,std::size_t total){for(std::size_t j=0;j<total;++j){tag(in,"RIG_MODULE");RigModule m;m.id={quoted(in)};m.type_id=quoted(in);int bypass{};in>>m.version>>bypass;check_stream(in);if(bypass!=0&&bypass!=1)throw std::invalid_argument("Invalid module bypass");m.bypass=bypass!=0;m.opaque_state=quoted(in);const auto nparams=count(in);if(nparams>64)throw std::invalid_argument("Too many module parameters");for(std::size_t k=0;k<nparams;++k){InsertParameter p;in>>p.id>>p.value;check_stream(in);m.parameters.push_back(p);}lane.push_back(std::move(m));}};
            read(r.pre_fx,pre);read(r.post_fx,post);r.validate();}
        if(version>=15){tag(in,"PRESET_NAME");fx.preset_name=quoted(in);}
        if(wav_core(fx.kind)){tag(in,"CAB_SELECTION");unsigned v{};in>>v;if(v!=1&&v!=2)throw std::invalid_argument("Unsupported Cab selection");fx.cab_collection_id=quoted(in);fx.cab_variant_id=quoted(in);
            if(v==2){const auto fields=count(in);if(fields>128)throw std::invalid_argument("Too many Cab metadata fields");for(std::size_t j=0;j<fields;++j){auto k=quoted(in),value=quoted(in);if(!fx.cab_metadata.emplace(k,value).second)throw std::invalid_argument("Duplicate Cab metadata");}validate_cab_metadata(fx.cab_metadata);}}
        if(fx.kind==InsertKind::reverb||wav_core(fx.kind)){tag(in,"REVERB");unsigned state_version{};in>>state_version;if(state_version!=1&&(state_version!=2||version<17))throw std::invalid_argument("unsupported MR Reverb state");auto& r=fx.reverb;
            in>>r.pre_delay>>r.tone>>r.mix;r.name=quoted(in);in>>r.sample_rate>>r.channels>>r.frames>>r.quad_map;
            if(state_version==2){int enabled{};in>>enabled>>r.gate_threshold;check_stream(in);if(enabled!=0&&enabled!=1)throw std::invalid_argument("invalid MR Reverb gate enable");r.gate_enabled=enabled!=0;}
            for(auto* f:{&r.first,&r.second}){f->relative=quoted(in);f->absolute=quoted(in);f->digest=quoted(in);in>>f->bytes;}check_stream(in);r.validate();}
        result.push_back(std::move(fx));
    }
    return result;
}
}
std::string serialize(const Project& p) {
    p.validate();
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    out << "MRS_CORE_SNAPSHOT " << p.version << '\n';
    out << "PROJECT " << std::quoted(p.id.value) << ' ' << std::quoted(p.title) << ' '
        << std::quoted(p.artist) << ' ' << p.sample_rate << '\n';
    section(out, "TEMPOS", p.time.tempos);
    for (const auto& point : p.time.tempos) out << point.tick << ' ' << point.bpm << '\n';
    section(out, "METERS", p.time.meters);
    for (const auto& point : p.time.meters)
        out << point.bar << ' ' << point.numerator << ' ' << point.denominator << '\n';
    section(out, "FOLDERS", p.folders);
    for (const auto& folder : p.folders)
        out << std::quoted(folder.id.value) << ' ' << std::quoted(folder.name) << ' '
            << std::quoted(folder.parent ? folder.parent->value : "") << '\n';
    section(out, "TRACKS", p.tracks);
    for (const auto& track : p.tracks) {
        out << std::quoted(track.id.value) << ' ' << std::quoted(track.name) << ' '
            << static_cast<int>(track.kind) << ' ' << std::quoted(track.folder ? track.folder->value : "") << '\n';
        out << track.mix.gain << ' ' << track.mix.pan << ' ' << track.mix.mute << ' ' << track.mix.solo << '\n';
        out << std::quoted(track.output ? track.output->value : "") << '\n';
        out << track.input << ' ' << track.sends.size() << '\n';
        for (const auto& send : track.sends) out << std::quoted(send.bus.value) << ' ' << send.gain << ' ' << send.pre_fader << ' ' << send.bypass << '\n';
        out << track.input_stereo << ' ' << track.input_monitor << '\n';
        out << track.hardware_outputs.size(); for (const auto channel : track.hardware_outputs) out << ' ' << channel; out << '\n';
        write_inserts(out,track.inserts);
        out << std::quoted(track.midi_input) << ' ' << track.midi_channel << ' ' << track.midi_monitor << '\n';
        out << "MIDIOUT " << std::quoted(track.midi_output) << ' ' << track.midi_output_channel << '\n';
        out << "FX_ROLE " << track.fx_channel << '\n';
        out << "TRACKCOLOR " << (track.color?1:0) << ' ' << track.color.value_or(0) << '\n';
    }
    section(out, "CLIPS", p.clips);
    for (const auto& clip : p.clips) {
        out << std::quoted(clip.id.value) << ' ' << std::quoted(clip.track.value) << ' '
            << std::quoted(clip.name) << ' ' << clip.start << ' ' << clip.length << ' '
            << clip.source_offset << ' ' << std::quoted(clip.source) << '\n';
        out << "MIDI " << static_cast<int>(clip.midi.has_value()) << '\n';
        if(clip.midi){const auto& m=*clip.midi;out<<m.start<<' '<<m.length<<' '<<m.source_offset<<' '<<m.notes.size()<<'\n';for(const auto& n:m.notes)out<<std::quoted(n.id.value)<<' '<<n.start<<' '<<n.length<<' '<<n.pitch<<' '<<n.velocity<<' '<<n.channel<<'\n';}
        out<<"MIDIEVENTS "<<(clip.midi?clip.midi->events.size():0)<<'\n';if(clip.midi)for(const auto& e:clip.midi->events)out<<std::quoted(e.id.value)<<' '<<e.start<<' '<<e.kind<<' '<<e.channel<<' '<<e.data1<<' '<<e.data2<<'\n';
        out<<"CLIPCOLOR "<<(clip.color?1:0)<<' '<<clip.color.value_or(0)<<'\n';
        out<<"AUDIOFADE "<<clip.fade_in<<' '<<clip.fade_out<<'\n';
    }
    section(out, "MARKERS", p.markers);
    for (const auto& marker : p.markers)
        out << std::quoted(marker.id.value) << ' ' << std::quoted(marker.name) << ' '
            << marker.tick << ' ' << static_cast<int>(marker.kind) << ' ' << (marker.color?1:0) << ' ' << marker.color.value_or(0) << '\n';
    section(out, "CHORDS", p.chords);
    for (const auto& chord : p.chords)
        out << std::quoted(chord.id.value) << ' ' << std::quoted(chord.symbol) << ' '
            << chord.start << ' ' << chord.end << ' ' << (chord.color?1:0) << ' ' << chord.color.value_or(0) << '\n';
    section(out, "SECTIONS", p.sections);
    for (const auto& part : p.sections)
        out << std::quoted(part.id.value) << ' ' << std::quoted(part.name) << ' '
            << part.start << ' ' << part.end << ' ' << part.color << '\n';
    out << "MASTER " << p.master_gain << '\n' << "HARDWARE " << p.master_outputs.size();
    for (const auto channel : p.master_outputs) out << ' ' << channel;
    out << '\n'; write_inserts(out,p.master_inserts);
    if(!p.ara_bindings.empty())out<<"ARA_BINDINGS "<<p.ara_bindings.size()<<'\n';
    for(const auto& b:p.ara_bindings){
        for(const auto* s:{&b.source,&b.persistent_id,&b.module,&b.class_id,&b.name,&b.factory_id,&b.version,&b.archive_id})out<<std::quoted(*s)<<' ';
        out<<std::quoted(b.document_id)<<' '<<b.bypass<<' '<<b.checksum<<' '<<b.source_checksum<<' ';
        for(const auto* blob:{&b.archive,&b.component_state,&b.controller_state}){std::string hex;hex.reserve(blob->size()*2);for(auto byte:*blob){const auto v=std::to_integer<unsigned>(byte);hex+="0123456789abcdef"[v>>4];hex+="0123456789abcdef"[v&15];}out<<std::quoted(hex)<<' ';}out<<'\n';
    }
    std::size_t folder_states{};for(const auto& f:p.folders)if(f.bus||f.collapsed)++folder_states;
    if(folder_states){out<<"FOLDER_STATE "<<folder_states<<'\n';for(const auto& f:p.folders)if(f.bus||f.collapsed)out<<std::quoted(f.id.value)<<' '<<std::quoted(f.bus?f.bus->value:"")<<' '<<f.collapsed<<'\n';}
    const auto stretched=std::count_if(p.clips.begin(),p.clips.end(),[](const auto& c){return c.stretch.has_value();});
    if(stretched){out<<"STRETCH_CLIPS "<<stretched<<'\n';for(const auto& c:p.clips)if(c.stretch){const auto& s=*c.stretch;
        out<<std::quoted(c.id.value)<<' '<<std::quoted(stretch::serialize(s.parameters))<<' '<<s.source_frames<<' '<<s.window_begin<<' '<<s.window_end<<'\n';}}
    out << "END\n";
    auto result = out.str();
    if (result.size() > max_bytes) throw std::invalid_argument("snapshot byte limit exceeded");
    return result;
}
Project deserialize(std::string_view bytes) {
    if (bytes.size() > max_bytes) throw std::invalid_argument("snapshot byte limit exceeded");
    std::istringstream in{std::string(bytes)};
    in.imbue(std::locale::classic());
    Project p;
    tag(in, "MRS_CORE_SNAPSHOT");
    in >> p.version;
    check_stream(in);
    const auto input_version = p.version;
    if (input_version < 1 || input_version > schema_version)
        throw std::invalid_argument("unsupported snapshot version");
    p.version = schema_version; // migrate v1 with empty musical lanes

    tag(in, "PROJECT");
    p.id = {quoted(in)};
    p.title = quoted(in);
    p.artist = quoted(in);
    in >> p.sample_rate;
    check_stream(in);
    tag(in, "TEMPOS");
    auto n = count(in);
    p.time.tempos.clear();
    for (std::size_t i = 0; i < n; ++i) {
        TempoPoint point;
        in >> point.tick >> point.bpm;
        check_stream(in);
        p.time.tempos.push_back(point);
    }
    tag(in, "METERS");
    n = count(in);
    p.time.meters.clear();
    for (std::size_t i = 0; i < n; ++i) {
        MeterPoint point;
        in >> point.bar >> point.numerator >> point.denominator;
        check_stream(in);
        p.time.meters.push_back(point);
    }
    tag(in, "FOLDERS");
    n = count(in);
    for (std::size_t i = 0; i < n; ++i) {
        Folder folder;
        folder.id = {quoted(in)};
        folder.name = quoted(in);
        folder.parent = parent(in);
        p.folders.push_back(std::move(folder));
    }
    tag(in, "TRACKS");
    n = count(in);
    for (std::size_t i = 0; i < n; ++i) {
        Track track;
        track.id = {quoted(in)};
        track.name = quoted(in);
        int kind{};
        in >> kind;
        check_stream(in);
        if (kind < 0 || kind > (input_version >= 11 ? 3 : input_version >= 4 ? 2 : 1)) throw std::invalid_argument("invalid track enum");
        track.kind = static_cast<TrackKind>(kind);
        track.folder = parent(in);
        if (input_version >= 3) {
            int mute{}, solo{};
            in >> track.mix.gain >> track.mix.pan >> mute >> solo;
            check_stream(in);
            if ((mute != 0 && mute != 1) || (solo != 0 && solo != 1)) throw std::invalid_argument("invalid mixer flags");
            track.mix.mute = mute != 0; track.mix.solo = solo != 0;
        }
        if (input_version >= 4) track.output = parent(in);
        if (input_version >= 5) {
            in >> track.input; check_stream(in);
            const auto sends = count(in);
            if (sends > 8) throw std::invalid_argument("send limit exceeded");
            for (std::size_t j=0; j<sends; ++j) {
                Track::Send send; send.bus = {quoted(in)}; int pre{};
                in >> send.gain >> pre; check_stream(in);
                if (pre != 0 && pre != 1) throw std::invalid_argument("invalid send mode");
                send.pre_fader = pre != 0;
                if(input_version>=18){int bypass{};in>>bypass;check_stream(in);if(bypass!=0&&bypass!=1)throw std::invalid_argument("invalid send bypass");send.bypass=bypass!=0;}
                track.sends.push_back(std::move(send));
            }
        }
        if (input_version >= 7) {
            int stereo{}, monitor{}; in >> stereo >> monitor; check_stream(in);
            if ((stereo != 0 && stereo != 1) || (monitor != 0 && monitor != 1)) throw std::invalid_argument("invalid input flags");
            track.input_stereo = stereo != 0; track.input_monitor = monitor != 0;
        }
        if (input_version >= 6) {
            const auto outputs = count(in); if (outputs > 2) throw std::invalid_argument("hardware output count");
            for (std::size_t j=0; j<outputs; ++j) { int channel{}; in >> channel; check_stream(in); track.hardware_outputs.push_back(channel); }
        }
        if (input_version >= 8) track.inserts=read_inserts(in,input_version);
        if(input_version>=11){track.midi_input=quoted(in);int monitor{};in>>track.midi_channel>>monitor;check_stream(in);if(monitor!=0&&monitor!=1)throw std::invalid_argument("invalid MIDI monitor flag");track.midi_monitor=monitor!=0;}
        if(input_version>=14){tag(in,"MIDIOUT");track.midi_output=quoted(in);in>>track.midi_output_channel;check_stream(in);}
        if(input_version>=19){tag(in,"FX_ROLE");int fx{};in>>fx;check_stream(in);if(fx!=0&&fx!=1)throw std::invalid_argument("invalid FX role");track.fx_channel=fx!=0;if(track.fx_channel&&track.kind!=TrackKind::bus)throw std::invalid_argument("FX role requires bus");}
        if(input_version>=27){tag(in,"TRACKCOLOR");int present{};std::uint32_t color{};in>>present>>color;check_stream(in);if((present!=0&&present!=1)||color>0xFFFFFF||(!present&&color))throw std::invalid_argument("invalid track color");if(present)track.color=color;}
        p.tracks.push_back(std::move(track));
    }
    tag(in, "CLIPS");
    n = count(in);
    for (std::size_t i = 0; i < n; ++i) {
        Clip clip;
        clip.id = {quoted(in)};
        clip.track = {quoted(in)};
        clip.name = quoted(in);
        in >> clip.start >> clip.length >> clip.source_offset;
        check_stream(in);
        clip.source = quoted(in);
        if(input_version>=12){tag(in,"MIDI");int present{};in>>present;check_stream(in);if(present!=0&&present!=1)throw std::invalid_argument("invalid MIDI clip flag");if(present){clip.midi.emplace();auto& m=*clip.midi;in>>m.start>>m.length>>m.source_offset;check_stream(in);const auto notes=count(in);if(notes>max_project_midi_notes)throw std::invalid_argument("too many MIDI notes (32768 maximum)");for(std::size_t j=0;j<notes;++j){MidiNote note;note.id={quoted(in)};in>>note.start>>note.length>>note.pitch>>note.velocity>>note.channel;check_stream(in);m.notes.push_back(std::move(note));}}}
        if(input_version>=13){tag(in,"MIDIEVENTS");const auto events=count(in);if(events>8192||(!clip.midi&&events))throw std::invalid_argument("invalid MIDI event count");for(std::size_t j=0;j<events;++j){MidiChannelEvent e;e.id={quoted(in)};in>>e.start>>e.kind>>e.channel>>e.data1>>e.data2;check_stream(in);clip.midi->events.push_back(std::move(e));}}
        if(input_version>=22){tag(in,"CLIPCOLOR");int present{};std::uint32_t color{};in>>present>>color;check_stream(in);if((present!=0&&present!=1)||color>0xFFFFFF||(!present&&color))throw std::invalid_argument("invalid clip color");if(present)clip.color=color;}
        if(input_version>=26){tag(in,"AUDIOFADE");in>>clip.fade_in>>clip.fade_out;check_stream(in);}
        p.clips.push_back(std::move(clip));
    }
    tag(in, "MARKERS");
    n = count(in);
    for (std::size_t i = 0; i < n; ++i) {
        Marker marker;
        marker.id = {quoted(in)};
        marker.name = quoted(in);
        int kind{};
        in >> marker.tick >> kind;
        check_stream(in);
        if (kind < 0 || kind > 5) throw std::invalid_argument("invalid marker enum");
        marker.kind = static_cast<MarkerKind>(kind);
        if(input_version>=20){int present{};std::uint32_t color{};in>>present>>color;check_stream(in);
            if((present!=0&&present!=1)||color>0xFFFFFF||(!present&&color))throw std::invalid_argument("invalid marker color snapshot");
            if(present)marker.color=color;
        }
        p.markers.push_back(std::move(marker));
    }
    if (input_version >= 2) {
        tag(in, "CHORDS");
        n = count(in);
        for (std::size_t i = 0; i < n; ++i) {
            Chord chord;
            chord.id = {quoted(in)}; chord.symbol = quoted(in);
            in >> chord.start >> chord.end;
            check_stream(in);
            if(input_version>=21){int present{};std::uint32_t color{};in>>present>>color;check_stream(in);if((present!=0&&present!=1)||color>0xFFFFFF||(!present&&color))throw std::invalid_argument("invalid chord color snapshot");if(present)chord.color=color;}
            p.chords.push_back(std::move(chord));
        }
        tag(in, "SECTIONS");
        n = count(in);
        for (std::size_t i = 0; i < n; ++i) {
            ArrangerSection part;
            part.id = {quoted(in)}; part.name = quoted(in);
            in >> part.start >> part.end >> part.color;
            check_stream(in);
            p.sections.push_back(std::move(part));
        }
    }
    if (input_version >= 3) { tag(in, "MASTER"); in >> p.master_gain; check_stream(in); }
    if (input_version >= 6) {
        tag(in,"HARDWARE"); const auto outputs = count(in); if (outputs > 2) throw std::invalid_argument("master output count");
        for (std::size_t j=0; j<outputs; ++j) { int channel{}; in >> channel; check_stream(in); p.master_outputs.push_back(channel); }
    }
    if (input_version >= 8) p.master_inserts=read_inserts(in,input_version);
    std::string ending;in>>ending;check_stream(in);
    if(input_version>=28&&ending=="ARA_BINDINGS"){const auto total=count(in);if(total>8)throw std::invalid_argument("Too many ARA bindings");
        for(std::size_t i=0;i<total;++i){AraBinding b;for(auto* s:{&b.source,&b.persistent_id,&b.module,&b.class_id,&b.name,&b.factory_id,&b.version,&b.archive_id})*s=quoted(in);
            if(input_version>=29)b.document_id=quoted(in);
            int bypass{};in>>bypass>>b.checksum>>b.source_checksum;check_stream(in);if(bypass!=0&&bypass!=1)throw std::invalid_argument("Invalid ARA bypass");b.bypass=bypass!=0;
            for(auto* blob:{&b.archive,&b.component_state,&b.controller_state}){const auto hex=quoted(in);if(hex.size()>2*1024*1024||hex.size()%2)throw std::invalid_argument("Invalid ARA blob");auto digit=[](char c)->unsigned{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::invalid_argument("Invalid ARA hex");};for(std::size_t j=0;j<hex.size();j+=2)blob->push_back(static_cast<std::byte>(digit(hex[j])*16+digit(hex[j+1])));}
            p.ara_bindings.push_back(std::move(b));}in>>ending;check_stream(in);}
    if(input_version>=30&&ending=="FOLDER_STATE"){const auto total=count(in);if(total>p.folders.size())throw std::invalid_argument("Too many folder states");std::vector<Id> seen;
        for(std::size_t i=0;i<total;++i){const Id id{quoted(in)};const auto bus=parent(in);int collapsed{};in>>collapsed;check_stream(in);
            if((collapsed!=0&&collapsed!=1)||std::find(seen.begin(),seen.end(),id)!=seen.end())throw std::invalid_argument("Invalid folder state");
            seen.push_back(id);
            const auto at=std::find_if(p.folders.begin(),p.folders.end(),[&](const auto& f){return f.id==id;});if(at==p.folders.end())throw std::invalid_argument("Unknown folder state");at->bus=bus;at->collapsed=collapsed!=0;}
        in>>ending;check_stream(in);}
    if(input_version>=31&&ending=="STRETCH_CLIPS"){
        const auto total=count(in);if(total>p.clips.size())throw std::invalid_argument("Too many stretch records");
        for(std::size_t i=0;i<total;++i){const Id id{quoted(in)};const auto found=std::find_if(p.clips.begin(),p.clips.end(),[&](const auto& c){return c.id==id;});
            if(found==p.clips.end()||found->midi||found->stretch)throw std::invalid_argument("Invalid stretch clip identity");
            ClipStretch s;s.parameters=stretch::deserialize(quoted(in));in>>s.source_frames>>s.window_begin>>s.window_end;check_stream(in);found->stretch=s;}
        in>>ending;check_stream(in);
    }
    if(ending!="END")throw std::invalid_argument("invalid snapshot ending");
    in >> std::ws;
    if (!in.eof()) throw std::invalid_argument("unknown trailing snapshot data");
    p.validate();
    return p;
}
} // namespace mrs
