#include <mrs/persistence.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace mrs::persistence {
namespace {
using namespace processing;
constexpr std::size_t text_limit = 1024 * 1024, count_limit = 65536;
[[noreturn]] void fail(const char* message) { throw std::invalid_argument(message); }
void require(bool condition, const char* message) { if (!condition) fail(message); }
void identifier(const Id& id) { require(!id.value.empty() && id.value.size() <= 128, "invalid persisted ID"); }
template<class T> bool contains(const std::vector<T>& items, const Id& id) {
    return std::any_of(items.begin(), items.end(), [&](const auto& item) { return item.id == id; });
}
void text_ok(std::string_view value) { require(value.size() <= text_limit, "persisted text too long"); }
void tag_ok(std::string_view tag) {
    require(tag.size() == 4 && std::all_of(tag.begin(), tag.end(), [](unsigned char c) { return c >= 33 && c <= 126; }), "invalid extension tag");
}
bool known(std::string_view tag, bool show) {
    return show ? tag == "SHOW" : tag == "PROJ" || tag == "GRPH" || tag == "MIXR" || tag == "MIDI" || tag == "LIVE";
}
void extensions_ok(const std::vector<Extension>& extensions, bool show) {
    require(extensions.size() <= count_limit, "too many extensions");
    for (const auto& extension : extensions) {
        tag_ok(extension.tag);
        require(!known(extension.tag, show), "extension shadows a known chunk");
        require(extension.bytes.size() <= max_archive_bytes, "extension too large");
    }
}
struct Writer {
    std::string bytes;
    void raw(std::string_view data) {
        require(data.size() <= max_archive_bytes - bytes.size(), "archive too large");
        bytes.append(data);
    }
    void u32(std::uint32_t v) { for (int i = 0; i < 4; ++i) { char b = static_cast<char>((v >> (i * 8)) & 255); raw({&b,1}); } }
    void u64(std::uint64_t v) { for (int i = 0; i < 8; ++i) { char b = static_cast<char>((v >> (i * 8)) & 255); raw({&b,1}); } }
    void size(std::size_t n) { require(n <= count_limit, "too many persisted items"); u32(static_cast<std::uint32_t>(n)); }
    void str(std::string_view v) { text_ok(v); u32(static_cast<std::uint32_t>(v.size())); raw(v); }
    void id(const Id& v) { str(v.value); }
    void optional(const std::optional<Id>& v) { u32(v ? 1 : 0); if (v) id(*v); }
    void boolean(bool v) { u32(v ? 1 : 0); }
    void number(float v) { u32(std::bit_cast<std::uint32_t>(v)); }
    void integer(int v) { u32(std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(v))); }
    void blob(const std::vector<std::byte>& v) {
        require(v.size() <= 16 * 1024 * 1024, "plugin state too large");
        u32(static_cast<std::uint32_t>(v.size()));
        if (!v.empty()) raw({reinterpret_cast<const char*>(v.data()), v.size()});
    }
};
struct Reader {
    std::string_view bytes;
    std::size_t cursor{};
    std::string_view raw(std::size_t n) {
        require(n <= bytes.size() - cursor, "truncated archive");
        auto result = bytes.substr(cursor,n); cursor += n; return result;
    }
    std::uint32_t u32() {
        auto v = raw(4); std::uint32_t result{};
        for (int i = 0; i < 4; ++i) result |= static_cast<std::uint32_t>(static_cast<unsigned char>(v[static_cast<std::size_t>(i)])) << (i * 8);
        return result;
    }
    std::uint64_t u64() {
        auto v = raw(8); std::uint64_t result{};
        for (int i = 0; i < 8; ++i) result |= static_cast<std::uint64_t>(static_cast<unsigned char>(v[static_cast<std::size_t>(i)])) << (i * 8);
        return result;
    }
    std::size_t size() { const auto n = u32(); require(n <= count_limit, "too many persisted items"); return n; }
    std::string str() { auto n = u32(); require(n <= text_limit, "persisted text too long"); return std::string(raw(n)); }
    Id id() { return {str()}; }
    bool boolean() { auto n = u32(); require(n <= 1, "invalid boolean"); return n != 0; }
    std::optional<Id> optional() { if (boolean()) return id(); return {}; }
    float number() { return std::bit_cast<float>(u32()); }
    int integer() { return static_cast<int>(std::bit_cast<std::int32_t>(u32())); }
    std::vector<std::byte> blob() {
        const auto n = u32(); require(n <= 16 * 1024 * 1024, "plugin state too large");
        auto v = raw(n); std::vector<std::byte> result(n);
        for (std::size_t i = 0; i < n; ++i) result[i] = static_cast<std::byte>(static_cast<unsigned char>(v[i]));
        return result;
    }
    void end() const { require(cursor == bytes.size(), "unexpected known chunk data"); }
};
std::uint32_t crc(std::string_view bytes) {
    std::uint32_t value = 0xffffffffU;
    for (unsigned char c : bytes) {
        value ^= c;
        for (int bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1U) ? 0xedb88320U : 0U);
    }
    return ~value;
}
void graph(Writer& w, const GraphState& g) {
    w.id(g.id); w.str(g.patch_name); w.size(g.nodes.size());
    for (const auto& n : g.nodes) {
        w.id(n.id); w.str(n.processor_id); w.u32(static_cast<std::uint32_t>(n.format));
        w.str(n.plugin.class_id); w.blob(n.plugin.component); w.blob(n.plugin.controller); w.boolean(n.bypass);
        w.size(n.parameters.size()); for (const auto& p : n.parameters) { w.u32(p.id); w.number(p.value); }
    }
    w.size(g.edges.size()); for (const auto& e : g.edges) { w.optional(e.from); w.id(e.to); w.number(e.gain); }
    w.size(g.outputs.size()); for (const auto& o : g.outputs) w.id(o);
    w.size(g.midi_routes.size());
    for (const auto& m : g.midi_routes) { w.id(m.source_port); w.id(m.target_node); w.integer(m.input_channel); w.integer(m.output_channel); w.integer(m.transpose); }
}
GraphState graph(Reader& r) {
    GraphState g; g.id = r.id(); g.patch_name = r.str();
    auto count = r.size(); require(count <= max_nodes, "too many graph nodes");
    for (std::size_t i = 0; i < count; ++i) {
        NodeState n; n.id = r.id(); n.processor_id = r.str();
        auto format = r.u32(); require(format <= 1, "unknown processor format"); n.format = static_cast<ProcessorFormat>(format);
        n.plugin.class_id = r.str(); n.plugin.component = r.blob(); n.plugin.controller = r.blob(); n.bypass = r.boolean();
        const auto parameters = r.size();
        for (std::size_t j = 0; j < parameters; ++j) { auto id = r.u32(); auto value = r.number(); n.parameters.push_back({id,value}); }
        g.nodes.push_back(std::move(n));
    }
    count = r.size(); for (std::size_t i = 0; i < count; ++i) { auto from = r.optional(); auto to = r.id(); auto gain = r.number(); g.edges.push_back({from,to,gain}); }
    count = r.size(); for (std::size_t i = 0; i < count; ++i) g.outputs.push_back(r.id());
    count = r.size();
    for (std::size_t i = 0; i < count; ++i) {
        MidiRoute route; route.source_port = r.id(); route.target_node = r.id(); route.input_channel = r.integer(); route.output_channel = r.integer(); route.transpose = r.integer();
        g.midi_routes.push_back(std::move(route));
    }
    g.validate(); return g;
}
void event(Writer& w, const MidiEvent& e) {
    w.u32(e.offset); w.u32(static_cast<std::uint32_t>(e.kind)); w.u32(e.channel); w.u32(e.data1); w.u32(e.data2);
}
MidiEvent event(Reader& r) {
    MidiEvent e; e.offset = r.u32(); auto kind = r.u32(); require(kind <= 6, "unknown MIDI event");
    e.kind = static_cast<MidiKind>(kind);
    const auto channel = r.u32(), data1 = r.u32(), data2 = r.u32();
    require(channel < 16 && data1 < 128 && data2 < 128, "invalid MIDI data");
    e.channel = static_cast<std::uint8_t>(channel); e.data1 = static_cast<std::uint8_t>(data1); e.data2 = static_cast<std::uint8_t>(data2); e.validate(); return e;
}
std::string envelope(std::uint32_t kind, std::uint64_t generation, const std::vector<Extension>& chunks) {
    Writer w; w.raw("MRSARCH1"); w.u32(archive_version); w.u32(kind); w.u64(generation); w.size(chunks.size());
    for (const auto& chunk : chunks) { w.raw(chunk.tag); w.u64(chunk.bytes.size()); w.raw(chunk.bytes); }
    w.u32(crc(w.bytes)); return w.bytes;
}
struct Archive { std::uint64_t generation{}; std::vector<Extension> chunks; };
Archive envelope(std::string_view bytes, std::uint32_t kind) {
    require(bytes.size() >= 32 && bytes.size() <= max_archive_bytes, "invalid archive size");
    Reader tail{bytes.substr(bytes.size()-4)};
    require(crc(bytes.substr(0,bytes.size()-4)) == tail.u32(), "archive checksum mismatch");
    Reader r{bytes.substr(0,bytes.size()-4)};
    require(r.raw(8) == "MRSARCH1", "invalid archive magic");
    require(r.u32() == archive_version, "unsupported archive version");
    require(r.u32() == kind, "wrong archive document kind");
    Archive a; a.generation = r.u64(); auto count = r.size(); std::set<std::string> seen;
    for (std::size_t i = 0; i < count; ++i) {
        auto tag = std::string(r.raw(4)); tag_ok(tag);
        auto n = r.u64(); require(n <= max_archive_bytes, "chunk too large");
        auto data = r.raw(static_cast<std::size_t>(n));
        if (known(tag,kind == 2)) require(seen.insert(tag).second, "duplicate known chunk");
        a.chunks.push_back({std::move(tag),std::string(data)});
    }
    r.end(); return a;
}
} // namespace
void ProjectDocument::validate() const {
    project.validate(); graph.validate(); text_ok(live_notes); extensions_ok(extensions,false);
    text_ok(project.title); text_ok(project.artist);
    require(project.time.tempos.size() <= count_limit && project.time.meters.size() <= count_limit &&
        project.folders.size() <= count_limit && project.tracks.size() <= count_limit && project.clips.size() <= count_limit &&
        project.markers.size() <= count_limit && project.chords.size() <= count_limit && project.sections.size() <= count_limit, "too many core items");
    for (const auto& f : project.folders) text_ok(f.name);
    for (const auto& t : project.tracks) text_ok(t.name);
    for (const auto& c : project.clips) { text_ok(c.name); text_ok(c.source); }
    for (const auto& m : project.markers) text_ok(m.name);
    for (const auto& s : project.sections) text_ok(s.name);
    require(mixer.size() <= count_limit && midi.size() <= count_limit && patches.size() <= count_limit && section_patches.size() <= count_limit && actions.size() <= count_limit, "too many document items");
    std::set<std::string> tracks, ports, patch_ids, section_ids, action_ids;
    for (const auto& m : mixer) {
        require(contains(project.tracks,m.track) && tracks.insert(m.track.value).second, "invalid mixer track");
        require(std::isfinite(m.gain) && m.gain >= 0 && m.gain <= 16 && std::isfinite(m.pan) && m.pan >= -1 && m.pan <= 1, "invalid mixer values");
    }
    for (const auto& m : midi) { identifier(m.port); text_ok(m.device_key); text_ok(m.name); require(ports.insert(m.port.value).second && (m.input || m.output), "invalid MIDI binding"); }
    auto routes = [&](const GraphState& g) {
        text_ok(g.patch_name);
        for (const auto& n : g.nodes) { text_ok(n.processor_id); text_ok(n.plugin.class_id); }
        for (const auto& route : g.midi_routes) {
            auto port = std::find_if(midi.begin(),midi.end(),[&](const auto& m) { return m.port == route.source_port; });
            require(port != midi.end() && port->input, "MIDI route has no input binding");
        }
    };
    routes(graph);
    for (const auto& p : patches) {
        identifier(p.id); require(patch_ids.insert(p.id.value).second, "duplicate live patch");
        p.state.validate(); require(p.state.id == graph.id, "patch belongs to another graph"); routes(p.state);
    }
    for (const auto& p : section_patches) require(contains(project.sections,p.section) && patch_ids.contains(p.patch.value) && section_ids.insert(p.section.value).second, "invalid section patch");
    for (const auto& a : actions) {
        identifier(a.id); a.event.validate(); require(a.event.offset == 0, "live action must have zero runtime offset");
        require(action_ids.insert(a.id.value).second && contains(project.markers,a.marker), "invalid live action marker");
        auto port = std::find_if(midi.begin(),midi.end(),[&](const auto& m) { return m.port == a.port; });
        require(port != midi.end() && port->output, "live action has no output binding");
    }
}
void ShowDocument::validate() const {
    identifier(id); text_ok(title); extensions_ok(extensions,true); require(entries.size() <= count_limit, "too many show entries");
    std::set<std::string> ids;
    for (const auto& e : entries) {
        identifier(e.id); identifier(e.project); text_ok(e.path); text_ok(e.notes);
        require(!e.path.empty() && ids.insert(e.id.value).second, "invalid show reference"); if (e.patch) identifier(*e.patch);
    }
    require(!selected || contains(entries,*selected), "selected show entry missing");
}
std::string encode(const ProjectDocument& d) {
    d.validate(); std::vector<Extension> chunks{{"PROJ",serialize(d.project)}};
    Writer g; graph(g,d.graph); chunks.push_back({"GRPH",std::move(g.bytes)});
    Writer mix; mix.size(d.mixer.size());
    for (const auto& m : d.mixer) { mix.id(m.track); mix.number(m.gain); mix.number(m.pan); mix.boolean(m.mute); mix.boolean(m.solo); }
    chunks.push_back({"MIXR",std::move(mix.bytes)});
    Writer midi; midi.size(d.midi.size());
    for (const auto& m : d.midi) { midi.id(m.port); midi.str(m.device_key); midi.str(m.name); midi.boolean(m.input); midi.boolean(m.output); }
    chunks.push_back({"MIDI",std::move(midi.bytes)});
    Writer live; live.str(d.live_notes); live.size(d.patches.size());
    for (const auto& p : d.patches) { live.id(p.id); graph(live,p.state); }
    live.size(d.section_patches.size()); for (const auto& p : d.section_patches) { live.id(p.section); live.id(p.patch); }
    live.size(d.actions.size()); for (const auto& a : d.actions) { live.id(a.id); live.id(a.marker); live.id(a.port); event(live,a.event); }
    chunks.push_back({"LIVE",std::move(live.bytes)});
    chunks.insert(chunks.end(),d.extensions.begin(),d.extensions.end());
    return envelope(1,d.generation,chunks);
}
ProjectDocument decode_project(std::string_view bytes) {
    require(bytes.size() <= max_archive_bytes, "archive too large");
    if (bytes.starts_with("MRS_CORE_SNAPSHOT")) {
        ProjectDocument d; d.project = deserialize(bytes); d.graph.id = {"graph-main"}; d.validate(); return d;
    }
    auto a = envelope(bytes,1); ProjectDocument d; d.generation = a.generation; std::set<std::string> seen;
    for (const auto& c : a.chunks) {
        if (!known(c.tag,false)) { d.extensions.push_back(c); continue; }
        seen.insert(c.tag); Reader r{c.bytes}; std::size_t count{};
        if (c.tag == "PROJ") { d.project = deserialize(c.bytes); continue; }
        if (c.tag == "GRPH") d.graph = graph(r);
        if (c.tag == "MIXR") {
            count = r.size(); for (std::size_t i = 0; i < count; ++i) {
                MixerChannel m; m.track = r.id(); m.gain = r.number(); m.pan = r.number(); m.mute = r.boolean(); m.solo = r.boolean(); d.mixer.push_back(m);
            }
        }
        if (c.tag == "MIDI") {
            count = r.size(); for (std::size_t i = 0; i < count; ++i) {
                MidiBinding m; m.port = r.id(); m.device_key = r.str(); m.name = r.str(); m.input = r.boolean(); m.output = r.boolean(); d.midi.push_back(std::move(m));
            }
        }
        if (c.tag == "LIVE") {
            d.live_notes = r.str(); count = r.size();
            for (std::size_t i = 0; i < count; ++i) { auto id = r.id(); auto state = graph(r); d.patches.push_back({id,std::move(state)}); }
            count = r.size(); for (std::size_t i = 0; i < count; ++i) { auto section = r.id(); auto patch = r.id(); d.section_patches.push_back({section,patch}); }
            count = r.size(); for (std::size_t i = 0; i < count; ++i) {
                LiveAction action; action.id = r.id(); action.marker = r.id(); action.port = r.id(); action.event = event(r); d.actions.push_back(std::move(action));
            }
        }
        r.end();
    }
    require(seen.size() == 5, "required project chunks missing"); d.validate(); return d;
}
std::string encode(const ShowDocument& d) {
    d.validate(); Writer w; w.id(d.id); w.str(d.title); w.optional(d.selected); w.size(d.entries.size());
    for (const auto& e : d.entries) { w.id(e.id); w.id(e.project); w.str(e.path); w.str(e.notes); w.optional(e.patch); }
    std::vector<Extension> chunks{{"SHOW",std::move(w.bytes)}}; chunks.insert(chunks.end(),d.extensions.begin(),d.extensions.end()); return envelope(2,d.generation,chunks);
}
ShowDocument decode_show(std::string_view bytes) {
    auto a = envelope(bytes,2); ShowDocument d; d.generation = a.generation; bool found{};
    for (const auto& c : a.chunks) {
        if (c.tag != "SHOW") { d.extensions.push_back(c); continue; }
        found = true; Reader r{c.bytes}; d.id = r.id(); d.title = r.str(); d.selected = r.optional();
        const auto count = r.size(); for (std::size_t i = 0; i < count; ++i) {
            ShowEntry e; e.id = r.id(); e.project = r.id(); e.path = r.str(); e.notes = r.str(); e.patch = r.optional(); d.entries.push_back(std::move(e));
        }
        r.end();
    }
    require(found,"required show chunk missing"); d.validate(); return d;
}
ProjectDocument demo_document() {
    ProjectDocument d; d.project = demo_project(); d.graph = processing::demo_graph();
    d.midi.push_back({{"mock-midi"},"mock-device","Demo MIDI",true,true});
    if (!d.project.tracks.empty()) d.mixer.push_back({d.project.tracks.front().id,0.75f,-0.2f,false,true});
    d.patches.push_back({{"patch-clean"},d.graph});
    if (!d.project.markers.empty()) d.actions.push_back({{"action-start"},d.project.markers.front().id,{"mock-midi"},{0,MidiKind::program,0,12,0}});
    d.live_notes = "Shared project / Live metadata"; d.validate(); return d;
}
SharedSession::SharedSession(ProjectDocument document) : base_(std::move(document)) {
    base_.validate();
    for (const auto& m : base_.mixer) for (auto& t : base_.project.tracks) if (t.id == m.track)
        t.mix = {m.gain,m.pan,m.mute,m.solo};
    services_.projects = std::make_shared<ProjectStore>(base_.project);
    services_.transport = std::make_shared<MockTransport>(Timeline(base_.project.time,base_.project.sample_rate));
    graphs_ = std::make_shared<processing::GraphStore>(base_.graph);
}
ProjectDocument SharedSession::capture() const {
    auto project = services_.projects->state(); auto graph_state = graphs_->state();
    require(project.revision <= std::numeric_limits<std::uint64_t>::max() - base_.generation, "generation overflow");
    auto generation = base_.generation + project.revision;
    require(graph_state.revision <= std::numeric_limits<std::uint64_t>::max() - generation, "generation overflow");
    auto result = base_; result.project = *project.project; result.graph = *graph_state.graph;
    result.mixer.clear();
    for (const auto& t : result.project.tracks) result.mixer.push_back({t.id,t.mix.gain,t.mix.pan,t.mix.mute,t.mix.solo});
    result.generation = generation + graph_state.revision; result.validate(); return result;
}
} // namespace mrs::persistence
