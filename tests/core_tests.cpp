#include "legacy_midi_snapshot.hpp"
#include <mrs/core.hpp>
#include <mrs/arrangement.hpp>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
int assertions = 0;
void check(bool condition, const char* expression, int line) {
    ++assertions;
    if (!condition) throw std::runtime_error(std::string("line ") + std::to_string(line) + ": " + expression);
}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__, __LINE__)
template<class F> void rejects(F action) {
    bool threw = false;
    try { action(); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
}
template<class F> void invalid_project(F edit) {
    auto project = mrs::demo_project();
    edit(project);
    rejects([&] { project.validate(); });
}


void marker_colors(){
    auto p=mrs::demo_project();p.markers.front().color=0xAD5677;CHECK(mrs::deserialize(mrs::serialize(p))==p);
    auto bad=p;bad.markers.front().color=0x1000000;rejects([&]{bad.validate();});
    p.markers.front().color.reset();auto legacy=without_marker_colors(mrs::serialize(p));legacy.replace(0,std::string("MRS_CORE_SNAPSHOT 20").size(),"MRS_CORE_SNAPSHOT 19");CHECK(mrs::deserialize(legacy)==p);
}
void inserts() {
    marker_colors();
    using namespace mrs;
    auto p=demo_project(); const auto track=p.tracks.front().id; ProjectStore store{p};
    NativeInsert gain{};gain.id=new_id();gain.gain=.5f;
    NativeInsert eq{};eq.id=new_id();eq.kind=InsertKind::eq;eq.gain=6;eq.frequency=1000;eq.q=1;
    NativeInsert highpass{};highpass.id=new_id();highpass.kind=InsertKind::highpass;
    store.execute(SetInserts{track,{gain,eq}}); store.execute(SetInserts{std::nullopt,{highpass}});
    const auto saved=*store.state().project; CHECK(deserialize(serialize(saved)) == saved);
    CHECK(store.undo() && store.state().project->master_inserts.empty()); CHECK(store.undo() && *store.state().project == p);
    CHECK(store.redo() && store.redo() && *store.state().project == saved);
    auto named=gain;named.preset_name="Keys/Тёплый звук";store.execute(SetInserts{track,{named,eq}});
    CHECK(deserialize(serialize(*store.state().project))==*store.state().project);
    CHECK(store.undo()&&*store.state().project==saved);CHECK(store.redo()&&store.state().project->tracks.front().inserts.front().preset_name==named.preset_name);CHECK(store.undo());
    auto old14=without_fx_roles(serialize(saved));for(auto at=old14.find("PRESET_NAME ");at!=std::string::npos;at=old14.find("PRESET_NAME ")){auto end=old14.find('\n',at);old14.erase(at,end-at+1);}
    old14.replace(0,std::string("MRS_CORE_SNAPSHOT 15").size(),"MRS_CORE_SNAPSHOT 14");CHECK(deserialize(old14)==saved);
    auto invalidLabel=gain;invalidLabel.preset_name=std::string(1025,'x');rejects([&]{store.execute(SetInserts{track,{invalidLabel}});});
    auto invalid=gain; invalid.gain=5; rejects([&] { store.execute(SetInserts{track,{invalid}}); });
    invalid=eq; invalid.frequency=0; rejects([&] { store.execute(SetInserts{track,{invalid}}); });
    rejects([&] { store.execute(SetInserts{track,{gain,gain}}); });
    rejects([&] { store.execute(SetInserts{Id{"absent"},{gain}}); });
    CHECK(*store.state().project == saved);
    NativeInsert reverb;reverb.id=new_id();reverb.kind=InsertKind::reverb;
    CHECK(reverb.reverb.mix==.25f);store.execute(SetInserts{track,{reverb}});
    CHECK(deserialize(serialize(*store.state().project))==*store.state().project);
    auto legacyReverb=without_fx_roles(serialize(*store.state().project));legacyReverb.replace(0,(std::string("MRS_CORE_SNAPSHOT ")+std::to_string(schema_version)).size(),"MRS_CORE_SNAPSHOT 16");
    const auto stateAt=legacyReverb.find("REVERB 2 "),lineEnd=legacyReverb.find('\n',stateAt);legacyReverb.erase(lineEnd-6,6); // default " 0 -90"
    legacyReverb.replace(stateAt,std::string("REVERB 2").size(),"REVERB 1");
    CHECK(deserialize(legacyReverb)==*store.state().project); // legacy gate disabled, default threshold
    auto& r=reverb.reverb;r.name="Зал / True Stereo";r.pre_delay=199.5f;r.tone=-5.5f;r.mix=1;
    r.gate_enabled=true;r.gate_threshold=-75.3f;
    r.first={"Halls/Зал L.wav","D:/IR/Halls/Зал L.wav","fnv1a64:0123456789abcdef",256};r.second={"Halls/Зал R.wav","D:/IR/Halls/Зал R.wav","fnv1a64:fedcba9876543210",256};r.sample_rate=48000;r.channels=4;r.frames=16;
    reverb.bypass=true;store.execute(SetInserts{track,{reverb}});CHECK(deserialize(serialize(*store.state().project))==*store.state().project);
    auto invalidGate=serialize(*store.state().project);const auto gateEnd=invalidGate.find('\n',invalidGate.find("REVERB 2 ")),thresholdAt=invalidGate.rfind(' ',gateEnd),enableAt=invalidGate.rfind(' ',thresholdAt-1);invalidGate[enableAt+1]='2';rejects([&]{deserialize(invalidGate);});
    CHECK(store.undo()&&store.state().project->tracks.front().inserts.front().reverb.name.empty());CHECK(store.redo());
    auto badReverb=reverb;badReverb.reverb.tone=7;rejects([&]{badReverb.validate();});badReverb=reverb;badReverb.reverb.first.digest="wrong";rejects([&]{badReverb.validate();});
    badReverb=reverb;badReverb.reverb.gate_threshold=-59;rejects([&]{badReverb.validate();});
    CHECK(store.undo());CHECK(store.undo());CHECK(*store.state().project==saved);
    p.tracks.push_back({new_id(),"MIDI",TrackKind::midi,{}}); ProjectStore midi{p}; rejects([&] { midi.execute(SetInserts{p.tracks.back().id,{gain}}); });
    std::vector<NativeInsert> excess; for (int n=0;n<9;++n){NativeInsert insert{};insert.id=new_id();excess.push_back(insert);} rejects([&] { store.execute(SetInserts{track,excess}); });
    auto legacy=without_midi_outputs(serialize(demo_project())); const std::string clipMidi="MIDI 0\nMIDIEVENTS 0\n";for(auto pos=legacy.find(clipMidi);pos!=std::string::npos;pos=legacy.find(clipMidi))legacy.erase(pos,clipMidi.size()); const std::string midiLine="\"\" -1 1\n";for(auto pos=legacy.find(midiLine);pos!=std::string::npos;pos=legacy.find(midiLine))legacy.erase(pos,midiLine.size()); legacy.replace(0,std::string("MRS_CORE_SNAPSHOT 14").size(),"MRS_CORE_SNAPSHOT 7");
    const std::string line="INSERTS 0\n"; for (auto pos=legacy.find(line);pos!=std::string::npos;pos=legacy.find(line)) legacy.erase(pos,line.size());
    CHECK(deserialize(legacy) == demo_project());
    auto v9project=demo_project();NativeInsert channel;channel.id=new_id();channel.kind=InsertKind::channel_eq;channel.bands[2].gain=4;v9project.tracks.front().inserts={channel};auto v9=without_midi_outputs(serialize(v9project));for(auto pos=v9.find(clipMidi);pos!=std::string::npos;pos=v9.find(clipMidi))v9.erase(pos,clipMidi.size());for(auto pos=v9.find(midiLine);pos!=std::string::npos;pos=v9.find(midiLine))v9.erase(pos,midiLine.size());v9.replace(0,std::string("MRS_CORE_SNAPSHOT 14").size(),"MRS_CORE_SNAPSHOT 9");const std::string irline="\"\" 48000 1 1 20 20000 0 0\n";for(auto pos=v9.find(irline);pos!=std::string::npos;pos=v9.find(irline))v9.erase(pos,irline.size());CHECK(deserialize(v9)==v9project);

}

void inputs() {
    using namespace mrs;
    auto p=demo_project(); const auto id=p.tracks.front().id; ProjectStore store{p};
    store.execute(SetTrackInput{id,4,true}); store.execute(SetTrackMonitoring{id,true});
    CHECK(store.state().project->tracks.front().input_stereo && store.state().project->tracks.front().input_monitor);
    CHECK(deserialize(serialize(*store.state().project)) == *store.state().project);
    CHECK(store.undo() && !store.state().project->tracks.front().input_monitor);
    CHECK(store.redo() && store.state().project->tracks.front().input_monitor);
    CHECK(store.undo() && store.undo() && *store.state().project == p);
    const auto before=*store.state().project;
    rejects([&] { store.execute(SetTrackInput{id,63,true}); });
    rejects([&] { store.execute(SetTrackInput{id,-2,true}); });
    rejects([&] { store.execute(SetTrackMonitoring{Id{"missing"},true}); });
    CHECK(*store.state().project == before);
    p.tracks.push_back({{"bus"},"Bus",TrackKind::bus,{}}); ProjectStore buses{p};
    rejects([&] { buses.execute(SetTrackMonitoring{Id{"bus"},true}); });
    rejects([&] { buses.execute(SetTrackInput{Id{"bus"},0,true}); });
}

void hardware() {
    using namespace mrs;
    auto p = demo_project(); const auto id = p.tracks.front().id;
    p.tracks.push_back({{"cue"},"Cue",TrackKind::bus,{}}); ProjectStore store{p};
    store.execute(SetTrackOutput{id,Id{"cue"}}); store.execute(SetHardwareOutput{Id{"cue"},{4,5}});
    store.execute(SetHardwareOutput{std::nullopt,{2,3}});
    CHECK(deserialize(serialize(*store.state().project)) == *store.state().project);
    store.execute(RemoveTrack{Id{"cue"}}); CHECK(store.state().project->tracks.front().hardware_outputs == std::vector<int>{4,5});
    CHECK(store.undo() && store.state().project->tracks.front().output == Id{"cue"});
    store.execute(SetHardwareOutput{id,{7}}); CHECK(!store.state().project->tracks.front().output);
    CHECK(store.undo() && store.state().project->tracks.front().hardware_outputs.empty()); CHECK(store.redo());
    const auto before = store.state();
    rejects([&] { store.execute(SetHardwareOutput{id,{1,1}}); });
    rejects([&] { store.execute(SetHardwareOutput{id,{-1}}); });
    rejects([&] { store.execute(SetHardwareOutput{std::nullopt,{64}}); });
    rejects([&] { store.execute(SetHardwareOutput{id,{0,1,2}}); });
    rejects([&] { store.execute(SetHardwareOutput{Id{"missing"},{0}}); });
    CHECK(store.state().revision == before.revision);
    store.execute(SetTrackOutput{id,Id{"cue"}}); CHECK(store.state().project->tracks.front().hardware_outputs.empty());
    const std::string old = "MRS_CORE_SNAPSHOT 5\nPROJECT \"old\" \"Old\" \"\" 48000\nTEMPOS 1\n0 120\nMETERS 1\n1 4 4\nFOLDERS 0\nTRACKS 1\n\"a\" \"Audio\" 0 \"\"\n1 0 0 0\n\"\"\n-2 0\nCLIPS 0\nMARKERS 0\nCHORDS 0\nSECTIONS 0\nMASTER 1\nEND\n";
    const auto migrated = deserialize(old); CHECK(migrated.master_outputs.empty() && migrated.tracks.front().hardware_outputs.empty());
}
void sends() {
    using namespace mrs;
    auto p = demo_project(); const auto id = p.tracks.front().id;
    p.tracks.push_back({{"r1"},"Return 1",TrackKind::bus,{}});
    p.tracks.push_back({{"r2"},"Return 2",TrackKind::bus,{}});
    ProjectStore store{p};
    store.execute(SetTrackSends{id,{{{"r1"},0.5f,true},{{"r2"},0.25f,false}}});
    store.execute(SetTrackInput{id,3});
    const auto saved = *store.state().project;
    CHECK(deserialize(serialize(saved)) == saved);
    auto disabled=saved;disabled.tracks.front().sends[1].bypass=true;
    CHECK(deserialize(serialize(disabled))==disabled);
    auto invalidBypass=serialize(saved);const auto sendLine=invalidBypass.find("\"r2\" 0.25 0 0\n");CHECK(sendLine!=std::string::npos);
    invalidBypass.replace(sendLine,std::string("\"r2\" 0.25 0 0\n").size(),"\"r2\" 0.25 0 2\n");rejects([&]{deserialize(invalidBypass);});
    auto legacy=without_marker_colors(serialize(saved));legacy.replace(0,std::string("MRS_CORE_SNAPSHOT 19").size(),"MRS_CORE_SNAPSHOT 17");
    for(const auto& send:saved.tracks.front().sends){const auto prefix="\""+send.bus.value+"\" ";const auto at=legacy.find(prefix),end=legacy.find('\n',at);CHECK(at!=std::string::npos);legacy.erase(end-2,2);}
    for(auto at=legacy.find("FX_ROLE ");at!=std::string::npos;at=legacy.find("FX_ROLE "))legacy.erase(at,legacy.find('\n',at)-at+1);
    CHECK(deserialize(legacy)==saved); // all old sends remain enabled
    store.execute(SetTrackOutput{Id{"r1"},Id{"r2"}});
    const auto before = store.state();
    rejects([&] { store.execute(SetTrackSends{Id{"r2"},{{{"r1"},0,false}}}); });
    rejects([&] { store.execute(SetTrackSends{id,{{{"r1"},1,false},{{"r1"},1,true}}}); });
    rejects([&] { store.execute(SetTrackSends{id,{{id,1,false}}}); });
    rejects([&] { store.execute(SetTrackSends{id,{{{"absent"},1,false}}}); });
    rejects([&] { store.execute(SetTrackSends{id,{{{"r1"},-1,false}}}); });
    rejects([&] { store.execute(SetTrackInput{Id{"r1"},0}); });
    CHECK(store.state().revision == before.revision && store.state().project == before.project);
    store.execute(RemoveTrack{Id{"r1"}});
    CHECK(store.state().project->tracks.front().sends.size() == 1);
    CHECK(store.undo() && store.state().project->tracks.front().sends.size() == 2);
    auto bytes = serialize(saved); const auto head = bytes.find("MRS_CORE_SNAPSHOT "+std::to_string(schema_version)); CHECK(head == 0);
    const std::string old = "MRS_CORE_SNAPSHOT 4\nPROJECT \"old\" \"Old\" \"\" 48000\nTEMPOS 1\n0 120\nMETERS 1\n1 4 4\nFOLDERS 0\nTRACKS 1\n\"a\" \"Audio\" 0 \"\"\n1 0 0 0\n\"\"\nCLIPS 0\nMARKERS 0\nCHORDS 0\nSECTIONS 0\nMASTER 1\nEND\n";
    const auto migrated = deserialize(old); CHECK(migrated.tracks.front().sends.empty() && migrated.tracks.front().input == -2);
}
void buses() {
    using namespace mrs;
    auto p = demo_project();
    const auto track = p.tracks.front().id;
    p.tracks.push_back({{"sub"},"Subgroup",TrackKind::bus,{}});
    p.tracks.push_back({{"sum"},"Sum",TrackKind::bus,{}});
    ProjectStore store{p};
    store.execute(SetTrackOutput{track,Id{"sub"}});
    store.execute(SetTrackOutput{Id{"sub"},Id{"sum"}});
    const auto before = store.state();
    rejects([&] { store.execute(SetTrackOutput{Id{"sum"},Id{"sub"}}); });
    rejects([&] { store.execute(SetTrackOutput{Id{"sub"},Id{"sub"}}); });
    rejects([&] { store.execute(SetTrackOutput{track,Id{"absent"}}); });
    rejects([&] { store.execute(SetTrackOutput{Id{"sub"},track}); });
    CHECK(store.state().revision == before.revision && store.state().project == before.project);
    store.execute(SetTrackMix{Id{"sub"},{0.5f,-0.5f,true,true}});
    CHECK(deserialize(serialize(*store.state().project)) == *store.state().project);
    const auto routed = *store.state().project;
    store.execute(RemoveTrack{Id{"sub"}});
    CHECK(store.state().project->tracks.front().output == Id{"sum"});
    CHECK(store.undo() && *store.state().project == routed);
    auto bad = routed; bad.clips.front().track = {"sub"}; rejects([&] { bad.validate(); });
    const std::string v3 = "MRS_CORE_SNAPSHOT 3\nPROJECT \"old\" \"Old\" \"\" 48000\nTEMPOS 1\n0 120\nMETERS 1\n1 4 4\nFOLDERS 0\nTRACKS 1\n\"a\" \"Audio\" 0 \"\"\n0.5 -0.25 1 0\nCLIPS 0\nMARKERS 0\nCHORDS 0\nSECTIONS 0\nMASTER 0.75\nEND\n";
    const auto migrated = deserialize(v3);
    CHECK(!migrated.tracks.front().output && migrated.tracks.front().mix.gain == 0.5f && migrated.master_gain == 0.75f);
}
void model() {
    auto p = mrs::demo_project();
    p.validate();
    CHECK(p.version == mrs::schema_version && p.tracks.size() == 4 && p.clips.size() == 2);
    CHECK(p.tracks.front().folder == mrs::Id{"folder-rhythm"});
    CHECK(mrs::new_id() != mrs::new_id());
    invalid_project([](auto& q) { q.id.value.clear(); });
    invalid_project([](auto& q) { q.version = 99; });
    invalid_project([](auto& q) { q.sample_rate = 0; });
    invalid_project([](auto& q) { q.tracks[1].id = q.tracks[0].id; });
    invalid_project([](auto& q) { q.markers[0].id = q.id; });
    invalid_project([](auto& q) { q.folders[0].parent = q.folders[0].id; });
    invalid_project([](auto& q) {
        q.folders.push_back({{"folder-child"}, "Child", q.folders[0].id});
        q.folders[0].parent = mrs::Id{"folder-child"};
    });
    invalid_project([](auto& q) { q.folders[0].parent = mrs::Id{"missing"}; });
    invalid_project([](auto& q) { q.tracks[0].folder = mrs::Id{"missing"}; });
    invalid_project([](auto& q) { q.clips[0].track = {"missing"}; });
    invalid_project([](auto& q) { q.clips[0].start = -1; });
    invalid_project([](auto& q) { q.clips[0].length = 0; });
    invalid_project([](auto& q) { q.clips[0].start = mrs::max_sample; });
    invalid_project([](auto& q) { q.clips[0].source_offset = mrs::max_sample; });
    invalid_project([](auto& q) { q.markers[0].tick = -1; });
    invalid_project([](auto& q) { q.tracks[0].kind = static_cast<mrs::TrackKind>(9); });
    invalid_project([](auto& q) { q.markers[0].kind = static_cast<mrs::MarkerKind>(9); });
    invalid_project([](auto& q) { q.time.tempos.clear(); });
    invalid_project([](auto& q) { q.time.tempos[0].tick = 1; });
    invalid_project([](auto& q) { q.time.tempos[1].tick = 0; });
    invalid_project([](auto& q) { q.time.tempos[0].bpm = 0; });
    invalid_project([](auto& q) { q.time.tempos[0].bpm = std::numeric_limits<double>::quiet_NaN(); });
    invalid_project([](auto& q) { q.time.tempos[0].bpm = std::numeric_limits<double>::infinity(); });
    invalid_project([](auto& q) { q.time.meters[0].bar = 2; });
    invalid_project([](auto& q) { q.time.meters[1].bar = 1; });
    invalid_project([](auto& q) { q.time.meters[0].denominator = 3; });
    invalid_project([](auto& q) { q.time.meters[0].numerator = 0; });
    p.folders.push_back({{"folder-child"}, "Child", p.folders[0].id});
    p.tracks[1].folder = mrs::Id{"folder-child"};
    p.validate();
    CHECK(p.folders.size() == 2);
}
void timeline() {
    mrs::TimeMap map;
    map.tempos = {{0, 120}, {4 * mrs::ppq, 60}};
    map.meters = {{1, 4, 4}, {3, 3, 4}, {5, 6, 8}};
    const mrs::Timeline time{map, 48000};
    CHECK(time.to_samples(0) == 0);
    CHECK(time.to_samples(mrs::ppq) == 24000);
    CHECK(time.to_samples(4 * mrs::ppq) == 96000);
    CHECK(time.to_samples(5 * mrs::ppq) == 144000);
    CHECK(time.to_ticks(96000) == 4 * mrs::ppq);
    CHECK(time.to_ticks(144000) == 5 * mrs::ppq);
    CHECK(time.musical_position(8 * mrs::ppq) == mrs::MusicalPosition{3, 1, 0});
    CHECK(time.musical_position(14 * mrs::ppq) == mrs::MusicalPosition{5, 1, 0});
    CHECK(time.to_ticks({5, 6, 0}) == 14 * mrs::ppq + 5 * mrs::ppq / 2);
    CHECK(time.to_ticks({5, 2, 20}) == 14 * mrs::ppq + mrs::ppq / 2 + 20);
    for (mrs::Tick tick = 0; tick < 20000; tick += 17) {
        CHECK(time.to_ticks(time.to_samples(tick)) == tick);
        CHECK(time.to_ticks(time.musical_position(tick)) == tick);
    }
    for (const auto rate : {44100U, 48000U, 96000U}) {
        mrs::TimeMap fractional;
        fractional.tempos = {{0, 112.25}, {1234, 137.125}, {5678, 92.75}};
        const mrs::Timeline variable{fractional, rate};
        for (const auto tick : {0LL, 1LL, 1233LL, 1234LL, 1235LL, 5678LL, 1000000LL})
            CHECK(variable.to_ticks(variable.to_samples(tick)) == tick);
    }
    mrs::TimeMap narrow;
    narrow.meters = {{1, 1, 64}};
    const mrs::Timeline narrow_time{narrow, 48000};
    CHECK(narrow_time.to_ticks(narrow_time.musical_position(mrs::max_tick)) == mrs::max_tick);
    rejects([&] { (void)time.to_samples(-1); });
    rejects([&] { (void)time.to_ticks(mrs::Sample{-1}); });
    rejects([&] { (void)time.to_ticks(mrs::MusicalPosition{0, 1, 0}); });
    rejects([&] { (void)time.to_ticks(mrs::MusicalPosition{3, 4, 0}); });
    rejects([&] { (void)time.to_ticks(mrs::MusicalPosition{5, 1, 480}); });
    rejects([&] { (void)time.to_samples(mrs::max_tick + 1); });
    rejects([] { (void)mrs::Timeline{mrs::TimeMap{}, 0}; });
}
void transport() {
    mrs::MockTransport transport{mrs::Timeline{mrs::TimeMap{}, 48000}};
    std::vector<mrs::TransportState> states;
    auto connection = transport.subscribe([&](const auto& s) { states.push_back(s); });
    CHECK(transport.state().playback == mrs::PlaybackState::stopped);
    transport.advance(48000);
    CHECK(states.empty());
    transport.play();
    transport.play(); // idempotent, no duplicate events
    CHECK(states.size() == 1);
    transport.advance(48000);
    CHECK(transport.state().sample == 48000);
    CHECK(transport.state().musical == mrs::MusicalPosition{1, 3, 0});
    transport.pause();
    transport.advance(48000);
    CHECK(transport.state().sample == 48000);
    transport.seek(96000);
    CHECK(transport.state().playback == mrs::PlaybackState::paused);
    transport.play();
    transport.stop();
    CHECK(transport.state().sample == 0);
    transport.set_loop(mrs::LoopRange{100, 200});
    transport.seek(150);
    transport.play();
    transport.advance(50); // exact exclusive boundary
    CHECK(transport.state().sample == 100);
    transport.advance(275); // retain multi-loop overshoot
    CHECK(transport.state().sample == 175);
    transport.seek(0); // before loop, preserve intro
    transport.advance(199);
    CHECK(transport.state().sample == 199);
    transport.advance(1);
    CHECK(transport.state().sample == 100);
    transport.seek(250);
    transport.advance(25);
    CHECK(transport.state().sample == 175);
    transport.set_loop(std::nullopt);
    transport.advance(50);
    CHECK(transport.state().sample == 225);
    const auto before = transport.state();
    const auto events = states.size();
    rejects([&] { transport.seek(-1); });
    rejects([&] { transport.set_loop(mrs::LoopRange{200, 100}); });
    rejects([&] { transport.set_loop(mrs::LoopRange{100, 100}); });
    rejects([&] { transport.advance(-1); });
    CHECK(transport.state() == before && states.size() == events);
    connection.disconnect();
    transport.stop();
    CHECK(states.size() == events);
    mrs::Signal<int> signal;
    std::vector<int> first, second;
    auto one = signal.subscribe([&](int value) { first.push_back(value); if (value == 1) signal.publish(2); });
    auto two = signal.subscribe([&](int value) { second.push_back(value); });
    auto bad = signal.subscribe([](int) { throw std::runtime_error("observer"); });
    signal.publish(1);
    CHECK(first == std::vector<int>({1, 2}) && second == first);
    CHECK(signal.observer_errors() == 2);
    two.disconnect();
    signal.publish(3);
    CHECK(second.size() == 2);
    mrs::Connection surviving;
    {
        mrs::Signal<int> temporary;
        surviving = temporary.subscribe([](int) {});
    }
    surviving.disconnect(); // safe after service destruction
}
struct FailingCommand final : mrs::ICommand {
    std::string_view name() const override { return "Fail"; }
    void apply(mrs::Project& p) const override {
        p.title = "partial mutation";
        throw std::runtime_error("rejected");
    }
};
struct ReentrantCommand final : mrs::ICommand {
    mrs::IProjectStore& store;
    explicit ReentrantCommand(mrs::IProjectStore& s) : store(s) {}
    std::string_view name() const override { return "Reentrant"; }
    void apply(mrs::Project&) const override { (void)store.undo(); }
};
void commands() {
    mrs::ProjectStore store{mrs::demo_project()};
    std::vector<mrs::ProjectState> events;
    auto connection = store.subscribe([&](const auto& s) { events.push_back(s); });
    const auto original = store.state().project;
    CHECK(!store.undo() && !store.redo());
    store.execute(mrs::RenameTrack{{"track-guitar"}, "Lead"});
    CHECK(store.state().revision == 1 && store.state().can_undo);
    CHECK(original->tracks.back().name == "Guitar"); // snapshot is immutable
    CHECK(store.state().project->tracks.back().id == original->tracks.back().id);
    CHECK(store.undo() && store.state().project == original);
    CHECK(store.state().revision == 2 && store.state().can_redo);
    const auto before = store.state();
    rejects([&] { store.execute(FailingCommand{}); });
    rejects([&] { store.execute(mrs::RenameTrack{{"absent"}, "Lead"}); });
    rejects([&] { store.execute(mrs::AddTrack{{{"track-guitar"}, "Duplicate", mrs::TrackKind::audio, std::nullopt}}); });
    rejects([&] { store.execute(ReentrantCommand{store}); });
    CHECK(store.state().project == before.project && store.state().revision == before.revision);
    CHECK(store.state().can_redo && events.size() == 2);
    store.execute(mrs::RenameTrack{{"track-guitar"}, "Guitar"}); // no-op preserves redo
    CHECK(store.state().revision == 2 && store.state().can_redo);
    CHECK(store.redo() && store.state().project->tracks.back().name == "Lead");
    CHECK(store.undo());
    store.execute(mrs::RenameTrack{{"track-guitar"}, "New branch"});
    CHECK(!store.redo() && !store.state().can_redo);
    store.execute(mrs::AddTrack{{{"track-vocal"}, "Vocal", mrs::TrackKind::audio, std::nullopt}});
    CHECK(store.state().project->tracks.size() == 5);
    CHECK(store.undo() && store.state().project->tracks.size() == 4);
    CHECK(store.redo() && store.state().project->tracks.back().id == mrs::Id{"track-vocal"});
    mrs::ProjectStore bounded{mrs::demo_project(), 2};
    for (const auto name : {"A", "B", "C"}) bounded.execute(mrs::RenameTrack{{"track-guitar"}, name});
    CHECK(bounded.undo() && bounded.undo() && !bounded.undo());
    CHECK(bounded.state().project->tracks.back().name == "A");
    CHECK(bounded.redo() && bounded.redo() && !bounded.redo());
    rejects([] { (void)mrs::ProjectStore{mrs::demo_project(), 0}; });
    auto observer = store.subscribe([](const auto&) { throw std::runtime_error("UI"); });
    store.execute(mrs::RenameTrack{{"track-guitar"}, "Still committed"});
    CHECK(store.state().project->tracks[3].name == "Still committed");
}
void serialization() {
    {auto project=mrs::demo_project();mrs::AraBinding binding;binding.source=project.clips.front().source;binding.persistent_id="ara-persistent";binding.module="missing.vst3";binding.class_id="saved-class";binding.factory_id="factory";binding.version="1";binding.archive_id="archive";binding.archive={std::byte{0},std::byte{255}};binding.checksum=123;binding.source_checksum=456;project.ara_bindings={binding};
        CHECK(mrs::deserialize(mrs::serialize(project))==project);mrs::ProjectStore store(mrs::demo_project());store.execute(mrs::SetAraBindings{project.ara_bindings});CHECK(store.state().project->ara_bindings==project.ara_bindings);CHECK(store.undo()&&store.state().project->ara_bindings.empty());CHECK(store.redo()&&store.state().project->ara_bindings==project.ara_bindings);
        auto duplicate=project;duplicate.ara_bindings.push_back(binding);rejects([&]{duplicate.validate();});auto oversized=project;oversized.ara_bindings.front().archive.resize(1024*1024+1);rejects([&]{oversized.validate();});
        auto bad=mrs::serialize(project);const auto at=bad.find("ARA_BINDINGS 1");bad.replace(at,std::string("ARA_BINDINGS 1").size(),"ARA_BINDINGS 99");rejects([&]{(void)mrs::deserialize(bad);});
        auto legacy=mrs::serialize(project);legacy.replace(0,std::string("MRS_CORE_SNAPSHOT 29").size(),"MRS_CORE_SNAPSHOT 28");const auto groupField=legacy.find("\"archive\" \"\" ");CHECK(groupField!=std::string::npos);legacy.erase(groupField+std::string("\"archive\" ").size(),3);CHECK(mrs::deserialize(legacy)==project);
        project.ara_bindings.front().document_id="shared-document";auto second=project.ara_bindings.front();second.source="second-source";second.persistent_id="second-persistent";project.ara_bindings.push_back(second);CHECK(mrs::deserialize(mrs::serialize(project))==project);
        project.ara_bindings.back().class_id="different-plugin";rejects([&]{project.validate();});
    }
    {auto colored=mrs::demo_project();colored.tracks.front().color=0x36b9b7u;CHECK(mrs::deserialize(mrs::serialize(colored))==colored);
        auto bad=colored;bad.tracks.front().color=0x1000000u;rejects([&]{bad.validate();});
        auto legacy=mrs::serialize(mrs::demo_project());for(auto at=legacy.find("TRACKCOLOR ");at!=std::string::npos;at=legacy.find("TRACKCOLOR "))legacy.erase(at,legacy.find('\n',at)-at+1);
        legacy.replace(0,std::string("MRS_CORE_SNAPSHOT 27").size(),"MRS_CORE_SNAPSHOT 26");CHECK(mrs::deserialize(legacy)==mrs::demo_project());
        auto invalid=mrs::serialize(colored);auto at=invalid.find("TRACKCOLOR ");invalid.replace(at,invalid.find('\n',at)-at,"TRACKCOLOR 2 0");rejects([&]{(void)mrs::deserialize(invalid);});
    }
    {auto faded=mrs::demo_project();auto& audio=faded.clips.front();audio.fade_in=64;audio.fade_out=128;CHECK(mrs::deserialize(mrs::serialize(faded))==faded);
        auto bad=faded;bad.clips.front().fade_in=bad.clips.front().length+1;rejects([&]{bad.validate();});
        auto legacy=mrs::serialize(mrs::demo_project());for(auto at=legacy.find("TRACKCOLOR ");at!=std::string::npos;at=legacy.find("TRACKCOLOR "))legacy.erase(at,legacy.find('\n',at)-at+1);for(auto at=legacy.find("AUDIOFADE ");at!=std::string::npos;at=legacy.find("AUDIOFADE "))legacy.erase(at,legacy.find('\n',at)-at+1);
        legacy.replace(0,std::string("MRS_CORE_SNAPSHOT 26").size(),"MRS_CORE_SNAPSHOT 25");CHECK(mrs::deserialize(legacy)==mrs::demo_project());
        const auto id=audio.id;const auto length=audio.length;mrs::TrimAudioClip{id,audio.start,audio.start+32,audio.source_offset+length}.apply(faded);CHECK(faded.clips.front().fade_in==32&&faded.clips.front().fade_out==32);faded.validate();
        faded=mrs::demo_project();faded.clips.front().fade_in=64;faded.clips.front().fade_out=128;const auto right=mrs::new_id();mrs::SplitAudioClip{id,faded.clips.front().start+faded.clips.front().length/2,right}.apply(faded);CHECK(faded.clips.front().fade_in==64&&faded.clips.front().fade_out==0&&faded.clips[1].fade_in==0&&faded.clips[1].fade_out==128);faded.validate();
    }

    auto project = mrs::demo_project();
    project.title = "Лунная река \"кавер\"\n第二行";
    project.artist = "Евгения и Влад";
    project.tracks[0].name = "Drums \\ \"room\"";
    project.folders.push_back({{"child"}, "Вложенная папка", project.folders[0].id});
    project.time.tempos[0].bpm = 112.123456789;
    project.clips[0].source_offset = 128;
    const auto bytes = mrs::serialize(project);
    CHECK(mrs::deserialize(bytes) == project);
    CHECK(mrs::serialize(mrs::deserialize(bytes)) == bytes); // canonical, locale independent
    rejects([] { (void)mrs::deserialize(""); });
    rejects([] { (void)mrs::deserialize("MRS_CORE_SNAPSHOT 129\n"); });
    rejects([&] { (void)mrs::deserialize(bytes + "UNKNOWN\n"); });
    rejects([&] { (void)mrs::deserialize(bytes.substr(0, bytes.size() / 2)); });
    for (const auto& replacement : {"TEMPOS -1", "TEMPOS 100001", "TEMPOS abc"}) {
        auto bad = bytes;
        const auto start = bad.find("TEMPOS ");
        bad.replace(start, bad.find('\n', start) - start, replacement);
        rejects([&] { (void)mrs::deserialize(bad); });
    }
    auto bad_id = bytes;
    const auto start = bad_id.find("\"track-bass\"");
    bad_id.replace(start, std::string("\"track-bass\"").size(), "\"track-drums\"");
    rejects([&] { (void)mrs::deserialize(bad_id); });
    rejects([] { (void)mrs::deserialize("MRS_CORE_SNAPSHOT 1\nPROJECT unquoted"); });
    rejects([] { (void)mrs::deserialize(std::string(16 * 1024 * 1024 + 1, 'x')); });
    auto invalid = project;
    invalid.version = 99;
    rejects([&] { (void)mrs::serialize(invalid); });
}
void integration() {
    const auto services = mrs::demo_services();
    const auto arrange = services;
    const auto edit = services;
    const auto mix = services;
    const auto live = services;
    CHECK(arrange.projects == live.projects && edit.projects == mix.projects);
    CHECK(arrange.transport == live.transport && edit.transport == mix.transport);
    int live_changes = 0, arrange_changes = 0;
    auto lp = live.projects->subscribe([&](const auto&) { ++live_changes; });
    auto ap = arrange.projects->subscribe([&](const auto&) { ++arrange_changes; });
    mix.projects->execute(mrs::RenameTrack{{"track-guitar"}, "Lead"});
    CHECK(edit.projects->state().project->tracks.back().name == "Lead");
    CHECK(live_changes == 1 && arrange_changes == 1);
    CHECK(live.projects->undo());
    CHECK(mix.projects->state().project->tracks.back().name == "Guitar");
    live.transport->play();
    arrange.transport->seek(48000);
    CHECK(mix.transport->state().sample == 48000);
    edit.transport->pause();
    CHECK(live.transport->state().playback == mrs::PlaybackState::paused);
    const auto saved = mrs::serialize(*live.projects->state().project);
    mrs::ProjectStore restored{mrs::deserialize(saved)};
    CHECK(*restored.state().project == *arrange.projects->state().project);
    CHECK(!restored.state().can_undo); // history is runtime-only
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected test suite");
        const std::string suite = argv[1];
        if (suite == "inserts") inserts(); else if (suite == "inputs") inputs(); else if (suite == "hardware") hardware(); else if (suite == "sends") sends();
        else if (suite == "buses") buses();
        else if (suite == "model") model();
        else if (suite == "timeline") timeline();
        else if (suite == "transport") transport();
        else if (suite == "commands") commands();
        else if (suite == "serialization") serialization();
        else if (suite == "integration") integration();
        else throw std::runtime_error("unknown suite");
        std::cout << "PASS " << suite << ": " << assertions << " assertions\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
