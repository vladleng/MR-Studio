#include <mrs/musical.hpp>
#include <mrs/audio.hpp>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool ok, const char* msg) { if (!ok) throw std::runtime_error(msg); }
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__)
template<class F> void rejects(F f) {
    bool rejected = false;
    try { f(); } catch (const std::exception&) { rejected = true; }
    CHECK(rejected);
}
mrs::Services services(mrs::Project p = mrs::musical_demo_project()) {
    auto transport = std::make_shared<mrs::MockTransport>(mrs::Timeline{p.time, p.sample_rate});
    return {std::make_shared<mrs::ProjectStore>(std::move(p)), transport};
}
void model() {
    const auto p = mrs::musical_demo_project();
    const auto bad = [&](auto edit) {
        auto q = p; edit(q); rejects([&] { q.validate(); });
    };
    bad([](auto& q) { q.chords[1].start = 1; });
    bad([](auto& q) { q.sections[0].end = 0; });
    bad([](auto& q) { q.chords[0].start = -1; });
    bad([](auto& q) { q.chords[0].symbol.clear(); });
    bad([](auto& q) { q.chords[0].id = q.id; });
    bad([](auto& q) { q.sections[0].id = q.chords[0].id; });
    bad([](auto& q) { q.sections[0].color = 0x1000000; });
    bad([](auto& q) { q.sections[0].name.clear(); });
    bad([](auto& q) { q.sections.back().end = mrs::max_tick + 1; });
    bad([](auto& q) { std::swap(q.chords[0], q.chords[1]); });
    mrs::TimeMap map;
    map.tempos = {{0, 120}, {4 * mrs::ppq, 60}, {8 * mrs::ppq, 90}};
    map.meters = {{1, 4, 4}, {3, 6, 8}, {5, 3, 4}};
    for (auto rate : {44100U, 48000U, 96000U}) {
        const mrs::Timeline time{map, rate};
        for (mrs::Tick tick = 0; tick < 20000; tick += 7) {
            CHECK(time.to_ticks(time.to_samples(tick)) == tick);
            CHECK(time.to_ticks(time.musical_position(tick)) == tick);
        }
        CHECK(time.musical_position(8 * mrs::ppq) == mrs::MusicalPosition{3, 1, 0});
        CHECK(time.musical_position(14 * mrs::ppq) == mrs::MusicalPosition{5, 1, 0});
    }
}
void context() {
    auto p = mrs::musical_demo_project();
    p.chords[1].start += mrs::ppq; // deliberate gap
    p.markers.insert(p.markers.begin(), {{"same-tick"}, "First tied cue", 8 * mrs::ppq, mrs::MarkerKind::cue});
    std::swap(p.markers[0], p.markers[2]); // markers need not be sorted
    const auto s = services(p);
    mrs::MusicalTimeline time{s};
    const mrs::Timeline clock{p.time, p.sample_rate};
    CHECK(time.state().current_chord->symbol == "Dm7");
    CHECK(time.state().next_chord->symbol == "G7");
    s.transport->seek(clock.to_samples(4 * mrs::ppq));
    CHECK(!time.state().current_chord && time.state().next_chord->symbol == "G7");
    s.transport->seek(clock.to_samples(8 * mrs::ppq));
    CHECK(time.state().current_chord->symbol == "Gmaj7");
    CHECK(time.state().current_section->name == "Verse");
    CHECK(time.state().current_marker->id == mrs::Id{"same-tick"});
    CHECK(time.state().next_marker->id == mrs::Id{"cue-chorus"});
    s.transport->seek(clock.to_samples(48 * mrs::ppq));
    CHECK(!time.state().current_chord && !time.state().next_chord);
    CHECK(!time.state().current_section && !time.state().next_section);
    CHECK(!time.state().next_marker);
    CHECK(time.state().transport.musical == mrs::MusicalPosition{13, 1, 0});
    auto empty = p; empty.chords.clear(); empty.sections.clear(); empty.markers.clear();
    mrs::MusicalTimeline no_lanes{services(empty)};
    CHECK(!no_lanes.state().current_section && !no_lanes.state().next_marker);
}
void navigation() {
    const auto s = services();
    mrs::MusicalTimeline time{s};
    CHECK(!time.previous_section() && !time.previous_marker());
    CHECK(time.next_section());
    CHECK(time.state().current_section->name == "Verse");
    time.seek_marker({"cue-chorus"});
    CHECK(time.state().current_section->name == "Chorus");
    CHECK(!time.next_section() && !time.next_marker());
    time.loop_section({"section-verse"});
    const auto range = s.transport->state().loop;
    CHECK(range);
    s.transport->seek(range->start);
    s.transport->play();
    auto mock = std::dynamic_pointer_cast<mrs::MockTransport>(s.transport);
    mock->advance(range->end - range->start);
    CHECK(s.transport->state().sample == range->start);
    CHECK(time.state().current_section->name == "Verse");
    time.clear_loop(); CHECK(!s.transport->state().loop);
    s.transport->pause();
    time.seek_section({"section-chorus"});
    CHECK(s.transport->state().playback == mrs::PlaybackState::paused);
    CHECK(time.previous_marker());
    const auto before = time.state();
    rejects([&] { time.seek_section({"missing"}); });
    CHECK(time.state() == before);
}
void edits() {
    const auto s = services();
    auto shared = std::make_shared<mrs::MusicalTimeline>(s);
    auto arrange = shared, live = shared;
    int a = 0, l = 0;
    auto ac = arrange->subscribe([&](const auto&) { ++a; });
    auto lc = live->subscribe([&](const auto&) { ++l; });
    auto p = *s.projects->state().project;
    p.chords[0].symbol = "Dm9";
    s.projects->execute(mrs::SetMusicalData{p.time, p.chords, p.sections, p.markers});
    CHECK(a == 1 && l == 1 && live->state().current_chord->symbol == "Dm9");
    CHECK(arrange->state().project == s.projects->state().project);
    CHECK(s.projects->undo());
    CHECK(live->state().current_chord->symbol == "Dm7");
    CHECK(s.projects->redo());
    CHECK(live->state().current_chord->symbol == "Dm9");
    const auto revision = s.projects->state().revision;
    p.chords[0].end = p.chords[1].end;
    rejects([&] { s.projects->execute(mrs::SetMusicalData{p.time,p.chords,p.sections,p.markers}); });
    CHECK(s.projects->state().revision == revision);
    p = *s.projects->state().project;
    s.transport->seek(48000);
    const auto old_tick = shared->state().tick;
    p.time.tempos[0].bpm = 144;
    s.projects->execute(mrs::SetMusicalData{p.time,p.chords,p.sections,p.markers});
    CHECK(s.transport->state().sample == 48000 && shared->state().tick == 2 * old_tick);
    CHECK(shared->state().transport.musical == s.transport->state().musical);
    CHECK(s.projects->undo() && shared->state().tick == old_tick);
    lc.disconnect();
    const auto before = l;
    s.transport->stop();
    CHECK(l == before);
    auto bad = shared->subscribe([](const auto&) { throw std::runtime_error("listener"); });
    shared->seek_section({"section-verse"});
    CHECK(shared->state().current_section->name == "Verse");
}
void persistence() {
    auto p = mrs::musical_demo_project();
    p.chords[0].symbol = "Dbmaj7/#11";
    p.sections[0].name = "Вступление \"Moon\"";
    const auto bytes = mrs::serialize(p);
    CHECK(mrs::deserialize(bytes) == p);
    CHECK(mrs::serialize(mrs::deserialize(bytes)) == bytes);
    // Exact old snapshot grammar, no CHORDS/SECTIONS; stable IDs preserved.
    const std::string v1 =
        "MRS_CORE_SNAPSHOT 1\nPROJECT \"old-id\" \"Old\" \"Artist\" 48000\n"
        "TEMPOS 1\n0 120\nMETERS 1\n1 4 4\nFOLDERS 0\nTRACKS 0\nCLIPS 0\n"
        "MARKERS 1\n\"old-cue\" \"Cue\" 960 1\nEND\n";
    const auto migrated = mrs::deserialize(v1);
    CHECK(migrated.version == mrs::schema_version && migrated.id == mrs::Id{"old-id"});
    CHECK(migrated.chords.empty() && migrated.sections.empty());
    CHECK(migrated.markers[0].id == mrs::Id{"old-cue"});
    CHECK(mrs::deserialize(mrs::serialize(migrated)) == migrated);
    const std::string v2 =
        "MRS_CORE_SNAPSHOT 2\nPROJECT \"v2-id\" \"Old mix\" \"\" 48000\n"
        "TEMPOS 1\n0 120\nMETERS 1\n1 4 4\nFOLDERS 0\nTRACKS 1\n"
        "\"track-old\" \"Audio\" 0 \"\"\nCLIPS 0\nMARKERS 0\nCHORDS 0\nSECTIONS 0\nEND\n";
    const auto old_mix = mrs::deserialize(v2);
    CHECK(old_mix.tracks.front().mix == mrs::Track::Mix{} && old_mix.master_gain == 1);
    auto mixed = old_mix; mixed.tracks.front().mix = {0.625f,-0.75f,true,true}; mixed.master_gain = 0.375f;
    CHECK(mrs::deserialize(mrs::serialize(mixed)) == mixed);
    rejects([&] { (void)mrs::deserialize(bytes + "EXTRA"); });
    rejects([&] { (void)mrs::deserialize(bytes.substr(0, bytes.find("SECTIONS"))); });
}
void engine() {
    auto p = mrs::musical_demo_project();
    auto audio = std::make_shared<mrs::audio::AudioEngine>();
    audio->prepare({48000,0,2,128}, {});
    auto transport = std::make_shared<mrs::audio::EngineTransport>(audio, mrs::Timeline{p.time, p.sample_rate});
    mrs::Services s{std::make_shared<mrs::ProjectStore>(p),transport};
    mrs::MusicalTimeline time{s};
    std::array<float,256> output{};
    time.seek_section({"section-verse"});
    CHECK(time.state().current_section->name == "Intro"); // queued, not optimistic
    audio->process(nullptr,output.data(),128); transport->poll();
    CHECK(time.state().current_section->name == "Verse");
    CHECK(time.state().transport.sample == audio->state().sample);
    time.loop_section({"section-verse"});
    transport->play();
    audio->process(nullptr,output.data(),128); transport->poll();
    CHECK(time.state().transport.loop && time.state().transport.playback == mrs::PlaybackState::playing);
    p.time.tempos[0].bpm = 144;
    s.projects->execute(mrs::SetMusicalData{p.time,p.chords,p.sections,p.markers});
    CHECK(time.state().transport.musical == transport->state().musical);
    CHECK(audio->state().sample == time.state().transport.sample);
    const auto count = audio->metrics().callbacks;
    for (int i = 0; i < 10; ++i) audio->process(nullptr,output.data(),128);
    CHECK(audio->metrics().callbacks == count + 10); // no control/UI pump needed
    transport->poll();
    CHECK(time.state().transport.sample == audio->state().sample);
    rejects([&] { transport->rebind_timeline(mrs::Timeline{p.time,44100}); });
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected suite");
        const std::string name = argv[1];
        if (name == "model") model();
        else if (name == "context") context();
        else if (name == "navigation") navigation();
        else if (name == "edits") edits();
        else if (name == "persistence") persistence();
        else if (name == "engine") engine();
        else throw std::runtime_error("unknown suite");
        std::cout << "PASS musical " << name << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
