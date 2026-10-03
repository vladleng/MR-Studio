#include <mrs/core.hpp>
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
void model() {
    auto p = mrs::demo_project();
    p.validate();
    CHECK(p.version == 1 && p.tracks.size() == 4 && p.clips.size() == 2);
    CHECK(p.tracks.front().folder == mrs::Id{"folder-rhythm"});
    CHECK(mrs::new_id() != mrs::new_id());
    invalid_project([](auto& q) { q.id.value.clear(); });
    invalid_project([](auto& q) { q.version = 2; });
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
    rejects([] { (void)mrs::deserialize("MRS_CORE_SNAPSHOT 2\n"); });
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
        if (suite == "model") model();
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
