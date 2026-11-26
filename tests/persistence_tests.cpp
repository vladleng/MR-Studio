#include <mrs/persistence.hpp>
#include <mrs/musical.hpp>
#include <mrs/drum_maps.hpp>
#include <algorithm>
#include <bit>
#include <array>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
using namespace mrs;
using namespace mrs::persistence;
namespace fs = std::filesystem;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__)
template<class F> void rejects(F f) {
    bool rejected{}; try { f(); } catch (const std::exception&) { rejected = true; } CHECK(rejected);
}
struct Directory {
    fs::path path = fs::temp_directory_path() / ("mrs-persistence-" + new_id().value);
    Directory() { fs::create_directory(path); }
    ~Directory() { std::error_code error; fs::remove_all(path,error); }
};
void write(const fs::path& path, const std::string& bytes) {
    std::ofstream file(path,std::ios::binary); file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    CHECK(file.good());
}
std::string read(const fs::path& path) {
    std::ifstream file(path,std::ios::binary); return {std::istreambuf_iterator<char>(file),{}};
}
std::uint32_t crc(std::string_view bytes) {
    std::uint32_t value = 0xffffffffU;
    for (unsigned char c : bytes) {
        value ^= c; for (int i = 0; i < 8; ++i) value = (value >> 1) ^ ((value & 1U) ? 0xedb88320U : 0U);
    }
    return ~value;
}
void checksum(std::string& bytes) {
    auto value = crc(std::string_view(bytes).substr(0,bytes.size()-4));
    for (int i = 0; i < 4; ++i) bytes[bytes.size()-4+static_cast<std::size_t>(i)] = static_cast<char>((value >> (i*8)) & 255U);
}
ProjectDocument fixture() {
    auto d = demo_document(); d.project = musical_demo_project();
    for(auto& clip:d.project.clips)if(!clip.midi){clip.fade_in=std::min<Sample>(32,clip.length);clip.fade_out=std::min<Sample>(64,clip.length);}
    NativeInsert named;named.id={"preset-archive-fixture"};named.preset_name="Keys/Тёплый звук";d.project.tracks.front().inserts={named};
    d.mixer = {{d.project.tracks.front().id,0.625f,-0.5f,true,false}};
    d.project.tracks.front().mix = {0.625f,-0.5f,true,false};
    for (std::size_t i=1; i<d.project.tracks.size(); ++i) d.mixer.push_back({d.project.tracks[i].id,1,0,false,false});
    d.section_patches = {{d.project.sections.front().id,d.patches.front().id}};
    d.actions.front().marker = d.project.markers.front().id;
    auto& node = d.graph.nodes.front(); node.format = processing::ProcessorFormat::vst3;
    node.processor_id = "unavailable.vendor.processor"; node.plugin.class_id = "00112233445566778899AABBCCDDEEFF";
    node.plugin.component = {std::byte{0},std::byte{255},std::byte{42}};
    node.plugin.controller = {std::byte{127},std::byte{0}}; node.bypass = true;
    d.patches.front().state = d.graph; d.patches.front().state.nodes.front().parameters.front().value = 0.3f;
    d.generation = 7; d.live_notes = "Вступление — Live";
    d.extensions = {{"VNDR",std::string("a\0b",3)},{"FUTR","future"},{"VNDR","another"}};
    d.validate(); return d;
}
ShowDocument show(const ProjectDocument& d) {
    ShowDocument s; s.id = {"show-one"}; s.title = "Setlist";
    s.entries = {{{"entry-one"},d.project.id,"relative/song.mrsproject","Cue notes",d.patches.front().id},
                 {{"entry-two"},d.project.id,"relative/song.mrsproject","Repeat",{}}};
    s.selected = s.entries.front().id; s.extensions = {{"USER","show opaque"}}; return s;
}
void drum_maps() {
    DrumMapProfile base;
    base.id = {"base/../map"}; base.name = "Барабаны \"Base\""; base.description = "line\npath\\text";
    base.vendor = "Vendor"; base.plugin = "VSTi"; base.library = "Library"; base.kit = "Kit"; base.technique = "Base";
    base.categories = {{{"toms"}, "Toms", {{{"floor"}, "Floor", {
        {{"hit"}, "Hit", 36, 9, {"Center"}}, {{"rim"}, "Rim", 36, 10, {}},
        {{"any"}, "Any channel", 36, {}, {}}, {{"none"}, "Unmapped", {}, {}, {}},
        {{"low"}, "Low", 0, 0, {}}, {{"high"}, "High", 127, 15, {}}
    }}}}, {{"kick"}, "Kick", {}}};
    const auto bytes = serialize_drum_map(base);
    CHECK(deserialize_drum_map(bytes) == base);
    rejects([&] { (void)deserialize_drum_map("MRS_DRUM_MAP 2\n"); });
    rejects([&] { (void)deserialize_drum_map(bytes.substr(0, bytes.size() / 2)); });
    rejects([&] { (void)deserialize_drum_map(bytes + "unexpected"); });
    CHECK(drum_map_conflicts(base) == std::vector<Id>{{"hit"}, {"rim"}, {"any"}});
    auto sticks = duplicate_drum_map(base, "Sticks"), brushes = duplicate_drum_map(base, "Brushes");
    CHECK(sticks.id != base.id && brushes.id != base.id && brushes.id != sticks.id);
    auto& rows = sticks.categories.front().instruments.front().articulations;
    rows[2].note.reset();
    CHECK(drum_map_conflicts(sticks).empty()); // distinct channels and labels do not conflict
    rows[1].channel = 9;
    CHECK(drum_map_conflicts(sticks) == std::vector<Id>{{"hit"}, {"rim"}});
    CHECK(base.categories == brushes.categories && base.categories != sticks.categories);
    auto bad = base;
    auto& bad_rows = bad.categories.front().instruments.front().articulations;
    bad_rows[0].note = 128; rejects([&] { bad.validate(); });
    bad_rows[0].note = 36; bad_rows[0].channel = 16; rejects([&] { bad.validate(); });
    bad_rows[0].channel = 9; bad_rows[0].id = bad_rows[1].id; rejects([&] { bad.validate(); });
    const DrumMapBinding binding{base, {{"toms"}, {"floor"}}};
    rejects([&] { DrumMapBinding{base, {{"hit"}}}.validate(); });
    const auto legacy = fixture().project;
    const auto legacy_bytes = serialize(legacy);
    CHECK(legacy_bytes.starts_with("MRS_CORE_SNAPSHOT 32\n") && deserialize(legacy_bytes) == legacy);
    {
        auto project = legacy;
        Track drums; drums.id = {"view-drums"}; drums.name = "Drums"; drums.kind = TrackKind::instrument;
        Track keys; keys.id = {"view-keys"}; keys.name = "Keys"; keys.kind = TrackKind::instrument;
        Track audio; audio.id = {"view-audio"}; audio.name = "Audio";
        project.tracks.push_back(drums); project.tracks.push_back(keys); project.tracks.push_back(audio);
        ProjectStore store(project);
        store.execute(SetTrackMidiDrumMode{drums.id, true});
        auto selected = *store.state().project;
        CHECK(selected.tracks[selected.tracks.size()-3].midi_drum_mode);
        CHECK(!selected.tracks[selected.tracks.size()-2].midi_drum_mode);
        CHECK(!selected.tracks[selected.tracks.size()-3].drum_map);
        const auto encoded = serialize(selected);
        CHECK(encoded.starts_with("MRS_CORE_SNAPSHOT 37\n") && deserialize(encoded) == selected);
        const auto revision = store.state().revision;
        store.execute(SetTrackMidiDrumMode{drums.id, true});
        CHECK(store.state().revision == revision);
        rejects([&] { store.execute(SetTrackMidiDrumMode{audio.id, true}); });
        rejects([&] { store.execute(SetTrackMidiDrumMode{{"missing"}, true}); });
        CHECK(store.undo() && *store.state().project == project);
        CHECK(store.redo() && *store.state().project == selected);
        store.execute(SetTrackDrumMap{drums.id, binding});
        selected = *store.state().project;
        CHECK(deserialize(serialize(selected)) == selected);
        store.execute(SetTrackMidiDrumMode{drums.id, false});
        CHECK(serialize(*store.state().project).starts_with("MRS_CORE_SNAPSHOT 36\n"));
        CHECK(deserialize(serialize(*store.state().project)) == *store.state().project);
        CHECK(store.state().project->tracks[project.tracks.size()-3].drum_map == binding);
        auto malformed=encoded;
        const auto block=malformed.find("MIDI_DRUM_VIEWS 1\n\"view-drums\"\n");
        CHECK(block!=std::string::npos);
        malformed.replace(block,std::string("MIDI_DRUM_VIEWS 1\n\"view-drums\"\n").size(),"MIDI_DRUM_VIEWS 2\n\"view-drums\"\n\"view-drums\"\n");
        rejects([&] { (void)deserialize(malformed); });
    }
    Directory dir;
    const auto library = dir.path / "user-maps";
    CHECK(resolve_drum_map(library, binding).source == DrumMapSource::snapshot_missing);
    save_drum_map(library, base);
    save_drum_map(library, sticks);
    save_drum_map(library, brushes);
    CHECK(load_drum_map(library, base.id) == base && load_drum_map(library, sticks.id) == sticks);
    auto updated = base; updated.name = "Updated";
    rejects([&] { save_drum_map(library, updated); });
    CHECK(load_drum_map(library, base.id) == base);
    save_drum_map(library, updated, true);
    auto resolved = resolve_drum_map(library, binding);
    CHECK(resolved.profile == updated && resolved.source == DrumMapSource::library);
    CHECK(load_drum_map(library, brushes.id) == brushes);
    const auto moved = dir.path / "moved-maps";
    fs::rename(library, moved);
    CHECK(resolve_drum_map(library, binding).profile == base);
    CHECK(resolve_drum_map(moved, binding).profile == updated);
    for (const auto& entry : fs::directory_iterator(moved)) {
        if (entry.path().extension() == ".mrsdrummap" && read(entry.path()) == serialize_drum_map(updated))
            write(entry.path(), serialize_drum_map(sticks)); // valid data with the wrong ID
    }
    resolved = resolve_drum_map(moved, binding);
    CHECK(resolved.profile == base && resolved.source == DrumMapSource::snapshot_unreadable);
}
void drum_map_exchange() {
    Directory dir;
    const auto csv = dir.path / "Base.CSV", tsv = dir.path / "Alternative.tsv", library = dir.path / "maps";
    write(csv, "\xef\xbb\xbf" "group,articulation,instrument,channel,midi_note\r\n"
               "Toms,\"Open, \"\"Hard\"\"\",Floor,10,36\r\nToms,Rim,Floor,10,36\r\n"
               "Toms,Missing,Floor,,\r\nKick,Hit,Kick,,0\r\n");
    const auto base = import_drum_map_file(csv);
    CHECK(base.name == "Base" && base.categories.size() == 2 && base.categories[0].name == "Toms");
    const auto& rows = base.categories[0].instruments[0].articulations;
    CHECK(rows.size() == 3 && rows[0].name == "Open, \"Hard\"" && rows[0].note == 36 && rows[0].channel == 9 && !rows[2].note);
    CHECK(drum_map_conflicts(base) == std::vector<Id>{rows[0].id, rows[1].id});
    write(tsv, "midi_note\tinstrument\tarticulation\tgroup\n127\tRide\tBell\tCymbals\n");
    const auto other = import_drum_map_file(tsv);
    CHECK(other.categories[0].instruments[0].articulations[0].note == 127);
    const std::string header = "midi_note,instrument,articulation,group,channel\n";
    for (const std::string bad : {"128,Kick,Open,Kick,1", "36,Kick,Open,Kick,0", "36,Kick,Open,Kick,17",
                                  "36,Kick,Open,Kick", "36,Kick,,Kick,", "36,\"Kick,Open,Kick,", "36,Kick,Open,Kick,1,extra"})
        rejects([&] { (void)import_drum_map_table(header + bad, ',', "Bad"); });
    rejects([&] { (void)import_drum_map_table(header, ',', "Empty"); });
    rejects([&] { (void)import_drum_map_table(header + "36,Kick,Caf\xe9,Kick,1", ',', "Invalid UTF-8"); });
    rejects([&] { (void)import_drum_map_file(dir.path / "reference.pdf"); });
    save_drum_map(library, base);
    auto sticks = duplicate_drum_map(base, "Sticks"), brushes = duplicate_drum_map(base, "Brushes");
    sticks.categories[0].instruments[0].articulations[0].note = 38;
    brushes.categories[0].instruments[0].articulations[0].note.reset();
    save_drum_map(library, sticks); save_drum_map(library, brushes);
    CHECK(list_drum_maps(library).entries.size() == 3 && load_drum_map(library, base.id) == base);
    CHECK(load_drum_map(library, sticks.id) == sticks && load_drum_map(library, brushes.id) == brushes);
    const auto exported = dir.path / "export.mrsdrummap";
    export_drum_map_file(exported, sticks); CHECK(import_drum_map_file(exported) == sticks);
    rejects([&] { export_drum_map_file(exported, brushes); });
    CHECK(import_drum_map_file(exported) == sticks);
    sticks.name = "Sticks renamed"; save_drum_map(library, sticks, true);
    write(library / "bad.mrsdrummap", "unreadable");
    CHECK(list_drum_maps(library).warnings.size() == 1);
    delete_drum_map(library, sticks.id); CHECK(!load_drum_map(library, sticks.id));
    CHECK(list_drum_maps(library).entries.size() == 2 && load_drum_map(library, base.id) == base);
}
void model() {
    auto d = fixture();
    const auto bad = [&](auto edit) { auto copy = d; edit(copy); rejects([&] { copy.validate(); }); };
    bad([](auto& c) { c.mixer.front().track = {"missing"}; });
    bad([](auto& c) { c.mixer.push_back(c.mixer.front()); });
    bad([](auto& c) { c.mixer.front().gain = std::numeric_limits<float>::quiet_NaN(); });
    bad([](auto& c) { c.mixer.front().pan = 2; });
    bad([](auto& c) { c.midi.clear(); });
    bad([](auto& c) { c.midi.front().input = false; });
    bad([](auto& c) { c.section_patches.front().patch = {"missing"}; });
    bad([](auto& c) { c.patches.front().state.id = {"different-graph"}; });
    bad([](auto& c) { c.actions.front().marker = {"missing"}; });
    bad([](auto& c) { c.actions.front().event.offset = 1; });
    bad([](auto& c) { c.midi.front().output = false; });
    bad([](auto& c) { c.extensions.front().tag = "PROJ"; });
    bad([](auto& c) { c.extensions.front().tag = "BAD"; });
    bad([](auto& c) { c.project.title.assign(1024*1024+1,'x'); });
    bad([](auto& c) { c.patches.push_back(c.patches.front()); });
    auto s = show(d); s.validate();
    s.selected = Id{"missing"}; rejects([&] { s.validate(); });
}
void roundtrip() {
    {
        auto project=fixture().project;
        Track drums;drums.id={"note-colors-drums"};drums.name="Drums";drums.kind=TrackKind::instrument;
        Track keys;keys.id={"note-colors-keys"};keys.name="Keys";keys.kind=TrackKind::instrument;
        Track audio;audio.id={"note-colors-audio"};audio.name="Audio";
        project.tracks.push_back(drums);project.tracks.push_back(keys);project.tracks.push_back(audio);
        const auto legacy=serialize(project);
        ProjectStore store(project);
        store.execute(SetTrackMidiNoteColorMode{drums.id,MidiNoteColorMode::instrument});
        store.execute(SetTrackMidiNoteColorMode{keys.id,MidiNoteColorMode::clip});
        const auto selected=*store.state().project;
        const auto bytes=serialize(selected);
        CHECK(bytes.starts_with("MRS_CORE_SNAPSHOT 38\n") && deserialize(bytes)==selected);
        CHECK(deserialize(legacy)==project);
        CHECK(selected.tracks[selected.tracks.size()-3].midi_note_color_mode==MidiNoteColorMode::instrument);
        CHECK(selected.tracks[selected.tracks.size()-2].midi_note_color_mode==MidiNoteColorMode::clip);
        CHECK(selected.tracks.back().midi_note_color_mode==MidiNoteColorMode::velocity);
        const auto revision=store.state().revision;
        store.execute(SetTrackMidiNoteColorMode{keys.id,MidiNoteColorMode::clip});
        CHECK(store.state().revision==revision);
        rejects([&]{store.execute(SetTrackMidiNoteColorMode{audio.id,MidiNoteColorMode::clip});});
        rejects([&]{store.execute(SetTrackMidiNoteColorMode{keys.id,static_cast<MidiNoteColorMode>(9)});});
        CHECK(store.undo() && store.state().project->tracks[project.tracks.size()-2].midi_note_color_mode==MidiNoteColorMode::velocity);
        CHECK(store.redo() && *store.state().project==selected);
        auto malformed=bytes;
        const std::string row="\"note-colors-keys\" 2\n";
        const auto at=malformed.find(row);
        CHECK(at!=std::string::npos);
        malformed.replace(at,row.size(),"\"note-colors-drums\" 2\n");
        rejects([&]{(void)deserialize(malformed);});
    }
    auto d = fixture(); auto bytes = encode(d); CHECK(decode_project(bytes) == d);
    CHECK(encode(decode_project(bytes)) == bytes);
    CHECK(decode_project(bytes).graph.nodes.front().plugin.component[1] == std::byte{255});
    // No processor factory/device access: unavailable plugins and device references survive.
    SharedSession reopened(decode_project(bytes)); CHECK(reopened.capture() == d);
    CHECK(reopened.services().transport->state().playback == PlaybackState::stopped);
    // Capture actual native processor state while quiescent, archive it, explicitly restore.
    auto native = demo_document();
    processing::PreparedGraph runtime({std::make_shared<const processing::GraphState>(native.graph),0,false,false},{});
    CHECK(runtime.enqueue_parameter({"gain"},{0,0,0.375f}));
    std::array<float,2> audio{1,1}; runtime.process(audio.data(),1);
    native.graph = runtime.capture(); CHECK(!native.graph.nodes.front().plugin.component.empty());
    const auto restored = decode_project(encode(native));
    processing::PreparedGraph engine({std::make_shared<const processing::GraphState>(restored.graph),0,false,false},{});
    audio = {1,1}; engine.process(audio.data(),1); CHECK(audio[0] == 0.375f && audio[1] == 0.375f);
    // Internal audio edges and non-default MIDI remapping also survive.
    auto second = native.graph.nodes.front(); second.id = {"gain-two"};
    native.graph.nodes.push_back(second); native.graph.edges.push_back({Id{"gain"},second.id,0.75f});
    native.graph.outputs = {second.id}; native.graph.midi_routes.front().input_channel = 2;
    native.graph.midi_routes.front().output_channel = 4; native.graph.midi_routes.front().transpose = 12;
    CHECK(decode_project(encode(native)) == native);
}
void migrations() {
    const std::string v1 =
        "MRS_CORE_SNAPSHOT 1\nPROJECT \"old-id\" \"Old\" \"Artist\" 48000\n"
        "TEMPOS 1\n0 120\nMETERS 1\n1 4 4\nFOLDERS 0\nTRACKS 0\nCLIPS 0\n"
        "MARKERS 1\n\"old-cue\" \"Cue\" 960 1\nEND\n";
    auto d = decode_project(v1); CHECK(d.project.version == schema_version && d.project.id == Id{"old-id"});
    CHECK(d.project.markers.front().id == Id{"old-cue"} && d.project.chords.empty() && d.graph.nodes.empty());
    CHECK(decode_project(encode(d)) == d);
    auto v2 = musical_demo_project(); auto migrated = decode_project(serialize(v2));
    CHECK(migrated.project == v2 && migrated.midi.empty()); CHECK(decode_project(encode(migrated)) == migrated);
    rejects([&] { (void)decode_project(v1 + "EXTRA"); });
}
void compatibility() {
    auto d = fixture(); const auto bytes = encode(d);
    CHECK(decode_project(bytes).extensions == d.extensions);
    for (std::size_t n : {std::size_t{0},std::size_t{7},std::size_t{31},bytes.size()-1}) rejects([&] { (void)decode_project(bytes.substr(0,n)); });
    auto bad = bytes; bad[16] ^= 1; rejects([&] { (void)decode_project(bad); }); // checksum covers generation
    bad = bytes; bad[8] = 2; checksum(bad); rejects([&] { (void)decode_project(bad); }); // future schema
    bad = bytes; bad[12] = 2; checksum(bad); rejects([&] { (void)decode_project(bad); }); // wrong kind
    bad = bytes; for (std::size_t i = 24; i < 28; ++i) bad[i] = static_cast<char>(255);
    checksum(bad); rejects([&] { (void)decode_project(bad); }); // impossible count
    bad = bytes; for (std::size_t i = 32; i < 40; ++i) bad[i] = static_cast<char>(255);
    checksum(bad); rejects([&] { (void)decode_project(bad); }); // impossible first length
    // Replace a required tag with unknown: valid checksum but missing PROJ.
    bad = bytes; bad.replace(28,4,"NEWP"); checksum(bad); rejects([&] { (void)decode_project(bad); });
    // Insert duplicate PROJ chunk ahead of footer.
    const auto first_length = serialize(d.project).size(); bad = bytes;
    bad.insert(bad.size()-4,bytes.substr(28,12+first_length)); ++bad[24]; checksum(bad);
    rejects([&] { (void)decode_project(bad); });
    rejects([&] { (void)decode_project(bytes + "extra"); });
}
void show_refs() {
    auto d = fixture(); auto s = show(d); const auto bytes = encode(s);
    CHECK(decode_show(bytes) == s); CHECK(encode(decode_show(bytes)) == bytes);
    CHECK(bytes.find(d.live_notes) == std::string::npos);
    CHECK(bytes.find(d.graph.nodes.front().processor_id) == std::string::npos);
    rejects([&] { (void)decode_project(bytes); }); rejects([&] { (void)decode_show(encode(d)); });
    Directory dir; auto path = dir.path / "set.mrsshow"; save_show(path,s);
    s.generation = 1; s.title = "Updated"; save_show(path,s); CHECK(load_show(path) == s);
    CHECK(load_show(fs::path(path.string()+".bak")).title == "Setlist");
}
void atomic_files() {
    Directory dir; auto path = dir.path / fs::path(std::u8string(u8"Проект Moon River.mrsproject"));
    auto d = fixture(); save_project(path,d); CHECK(load_project(path) == d);
    auto next = d; ++next.generation; next.project.title = "Next"; save_project(path,next);
    auto backup = path; backup += ".bak"; CHECK(load_project(backup) == d);
    const auto original = read(path);
    auto other = next; other.project.id = {"other-project"};
    rejects([&] { save_project(path,other); });
    rejects([&] { save_autosave(path,other); });
    rejects([&] { save_project(path,d); });
    CHECK(read(path) == original);
    for (const auto point : {SavePoint::temporary_synced,SavePoint::backup_synced,SavePoint::before_replace}) {
        auto third = next; ++third.generation;
        rejects([&] { save_project(path,third,[&](SavePoint p) { if (p == point) throw std::runtime_error("injected failure"); }); });
        CHECK(read(path) == original && load_project(path) == next);
        for (const auto& entry : fs::directory_iterator(dir.path)) CHECK(entry.path().extension() != ".tmp");
    }
    const auto safe_backup = read(backup); write(path,"corrupt");
    rejects([&] { save_project(path,next); }); CHECK(read(backup) == safe_backup && read(path) == "corrupt");
    auto invalid = d; invalid.mixer.front().gain = -1;
    rejects([&] { save_project(dir.path / "invalid.mrsproject",invalid); });
    CHECK(!fs::exists(dir.path / "invalid.mrsproject"));
    auto missing_parent = dir.path / "absent" / "project.mrsproject";
    rejects([&] { save_project(missing_parent,d); });
}
void recovery() {
    Directory dir; const auto path = dir.path / "song.mrsproject"; auto d = fixture();
    save_project(path,d); auto next = d; ++next.generation; save_project(path,next);
    auto autosave = next; ++autosave.generation; autosave.live_notes = "Recovered"; save_autosave(path,autosave);
    const auto primary_bytes = read(path); auto r = recover_project(path);
    CHECK(r.document == autosave && r.source == fs::path(path.string()+".autosave"));
    CHECK(read(path) == primary_bytes); // recovery never writes
    SharedSession session(r.document); CHECK(session.services().transport->state().playback == PlaybackState::stopped);
    write(fs::path(path.string()+".autosave"),"broken");
    r = recover_project(path); CHECK(r.document == next && r.source == path && r.warnings.size() == 1);
    write(path,"broken"); r = recover_project(path); CHECK(r.document == d && r.warnings.size() == 2);
    // Orphan temporary files from an actual crash are never considered for recovery.
    write(dir.path / "mrs-write-orphan.tmp",encode(autosave)); CHECK(recover_project(path).document == d);
    write(fs::path(path.string()+".bak"),"broken"); rejects([&] { (void)recover_project(path); });
    rejects([&] { (void)recover_project(dir.path / "missing"); });
    // A valid but unrelated autosave is rejected against a valid primary.
    write(path,encode(d)); auto other = d; other.project.id = {"other-project"}; ++other.generation;
    write(fs::path(path.string()+".autosave"),encode(other));
    r = recover_project(path); CHECK(r.document == d && r.warnings.size() == 2);
    // Ties prefer the explicit primary.
    std::error_code ignored; fs::remove(fs::path(path.string()+".autosave"),ignored);
    write(path,encode(d)); save_autosave(path,d); CHECK(recover_project(path).source == path);
}
void autosave() {
    Directory dir; const auto path = dir.path / "song.mrsproject"; auto d = fixture(); save_project(path,d);
    const auto primary = read(path);
    {
        AutosaveWorker worker(path);
        std::uint64_t ticket{};
        for (int i = 0; i < 20; ++i) { ++d.generation; ticket = worker.submit(std::make_shared<const ProjectDocument>(d)); }
        CHECK(worker.wait(ticket).empty()); CHECK(recover_project(path).document == d && read(path) == primary);
        auto invalid = d; ++invalid.generation; invalid.mixer.front().gain = -1;
        ticket = worker.submit(std::make_shared<const ProjectDocument>(invalid));
        CHECK(!worker.wait(ticket).empty()); CHECK(recover_project(path).document == d);
        d.generation = invalid.generation+1; ticket = worker.submit(std::make_shared<const ProjectDocument>(d));
        CHECK(worker.wait(ticket).empty()); rejects([&] { (void)worker.wait(ticket+1); });
        auto old = d; --old.generation; rejects([&] { (void)worker.submit(std::make_shared<const ProjectDocument>(old)); });
        ++d.generation; (void)worker.submit(std::make_shared<const ProjectDocument>(d)); // destructor drains
    }
    CHECK(recover_project(path).document == d);
}
void session() {
    auto d = fixture(); SharedSession session(d);
    auto arrange = session.services(), live = session.services();
    CHECK(arrange.projects == live.projects && arrange.transport == live.transport);
    auto mix_graph = session.graphs(), live_graph = session.graphs(); CHECK(mix_graph == live_graph);
    arrange.projects->execute(RenameTrack{d.project.tracks.front().id,"Renamed in Arrange"});
    auto g = *mix_graph->state().graph; g.patch_name = "From Mix"; mix_graph->replace(g);
    live.transport->seek(1234); live.transport->play();
    auto saved = session.capture(); CHECK(saved.generation == d.generation+2);
    CHECK(saved.project.tracks.front().name == "Renamed in Arrange" && saved.graph.patch_name == "From Mix");
    CHECK(saved.extensions == d.extensions && saved.patches == d.patches);
    SharedSession reopened(decode_project(encode(saved)));
    CHECK(reopened.capture() == saved && !reopened.services().projects->state().can_undo);
    CHECK(reopened.services().transport->state().playback == PlaybackState::stopped && reopened.services().transport->state().sample == 0);
    CHECK(!reopened.graphs()->state().can_undo);
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected suite");
        const std::string name = argv[1];
        if (name == "drum_maps") { drum_maps(); drum_map_exchange(); } else if (name == "model") model(); else if (name == "roundtrip") roundtrip();
        else if (name == "migrations") migrations(); else if (name == "compatibility") compatibility();
        else if (name == "show") show_refs(); else if (name == "atomic") atomic_files();
        else if (name == "recovery") recovery(); else if (name == "autosave") autosave();
        else if (name == "session") session(); else throw std::runtime_error("unknown suite");
        std::cout << "PASS persistence " << name << '\n'; return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
