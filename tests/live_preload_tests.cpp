#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <mrs/desktop.hpp>
#include <thread>

namespace {
using namespace mrs;
using namespace mrs::desktop;
int checks{};
void check(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void rejects(F action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception &) {
        failed = true;
    }
    check(failed, "Expected guarded handoff failure");
}
struct Folder {
    std::filesystem::path path = std::filesystem::temp_directory_path() / ("mrs-preload-test-" + new_id().value);
    Folder() { std::filesystem::create_directories(path); }
    ~Folder() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
void wav(const std::filesystem::path &path, int sign = 1, unsigned rate = 48000) {
    std::ofstream out(path, std::ios::binary);
    const auto u16 = [&](unsigned n) {
        out.put(static_cast<char>(n & 255));
        out.put(static_cast<char>((n >> 8) & 255));
    };
    const auto u32 = [&](unsigned n) {
        u16(n & 65535);
        u16(n >> 16);
    };
    const auto frames = rate / 10;
    out.write("RIFF", 4);
    u32(36 + frames * 4);
    out.write("WAVEfmt ", 8);
    u32(16);
    u16(1);
    u16(2);
    u32(rate);
    u32(rate * 4);
    u16(4);
    u16(16);
    out.write("data", 4);
    u32(frames * 4);
    for (unsigned i = 0; i < frames * 2; ++i)
        u16(static_cast<unsigned short>(sign * 8192));
}
struct Calls {
    int opens{}, closes{}, starts{}, stops{};
    bool failStart{};
};
class Device final : public audio::IAudioDevice {
    std::shared_ptr<Calls> calls;
    audio::DevicePhase phase{audio::DevicePhase::closed};

  public:
    explicit Device(std::shared_ptr<Calls> value) : calls(std::move(value)) {}
    std::vector<audio::DeviceInfo> enumerate() override {
        return {{0, "Prepared Playback test", {}, {"1", "2", "3", "4"}, 32, 2048, 128, -1}};
    }
    void control_panel(int) override {}
    void open(const audio::DeviceConfig &, std::shared_ptr<audio::AudioEngine>) override {
        ++calls->opens;
        phase = audio::DevicePhase::open;
    }
    void start() override {
        ++calls->starts;
        if (calls->failStart) {
            calls->failStart = false;
            throw std::runtime_error("Injected start failure");
        }
        phase = audio::DevicePhase::running;
    }
    void stop() override {
        ++calls->stops;
        phase = audio::DevicePhase::stopped;
    }
    void close() noexcept override {
        ++calls->closes;
        phase = audio::DevicePhase::closed;
    }
    audio::DeviceStatus status() override { return {phase, 48000, 0, 0, 0, {}}; }
};
void ready(Application &app, const std::shared_ptr<Application::PlaybackPreparation> &job) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < end) {
        const auto status = app.playback_preparation(job);
        if (!status.busy) {
            check(status.message.empty(), status.message.c_str());
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("Playback preload timed out");
}
void run() {
    Folder folder;
    const auto a = folder.path / "a.wav", b = folder.path / "b.wav", r = folder.path / "resample.wav";
    wav(a);
    wav(b, -1);
    wav(r, 1, 44100);
    Application app;
    app.new_project();
    app.workspace(Workspace::live);
    app.disconnect();
    auto offline = app.prepare_playback({a});
    ready(app, offline);
    Application::PlaybackLayout offlineLayout;
    offlineLayout.stems.resize(1);
    app.activate_playback(offline, offlineLayout);
    check(app.audio_name() == "Offline clock (no sound)" && app.services().projects->state().project->clips.size() == 1,
          "Disconnected editing materializes WAV using the shared offline clock");
    const auto calls = std::make_shared<Calls>();
    app.connect(std::make_unique<Device>(calls), {0, 48000, 128, {}, {0, 1, 2, 3}, 1});
    const auto engine = app.engine();
    const auto graphStore = app.graphs();
    Application::PlaybackLayout one;
    one.stems.resize(1);
    auto first = app.prepare_playback({a});
    ready(app, first);
    app.activate_playback(first, one);
    check(app.engine() == engine && app.graphs() == graphStore, "Handoff reuses Core Engine and GraphStore");
    check(calls->opens == 1 && calls->closes == 0, "Initial activation keeps the open device");
    check(!app.services().projects->state().can_undo, "Runtime projection has no misleading project Undo history");
    std::array<float, 512> out{};
    const auto render = [&] { engine->process(nullptr, out.data(), 128); };
    app.play();
    render();
    check(out[400] > .03f, "Prepared audio reaches the common Core output");
    const auto current = app.services().projects;
    const auto before = engine->state().sample;
    const auto stops = calls->stops;
    auto second = app.prepare_playback({b, a});
    auto mix = current->state().project->tracks.front().mix;
    mix.gain = .8f;
    app.set_track_mix(current->state().project->tracks.front().id, mix);
    ready(app, second);
    render();
    check(engine->state().sample > before && current == app.services().projects,
          "Preload and mixer edits do not replace the active item");
    check(calls->stops == stops && out[400] > .03f, "Preparing the next item never stops the current device/audio");
    Application::PlaybackLayout two;
    two.time.tempos = {{0, 90}};
    two.time.meters = {{1, 3, 4}};
    two.stems.resize(2);
    two.stems[0].outputs = {2};
    two.stems[1].outputs = {3};
    app.activate_playback(second, two);
    check(engine->state().sample == 0 && engine->state().playback == PlaybackState::stopped,
          "Manual item switch starts at zero without autoplay");
    check(app.services().projects->state().project->tracks.size() == 2 &&
              app.services().projects->state().project->time == two.time,
          "Stem count and musical map publish together");
    app.play();
    render();
    check(out[400] == 0 && out[401] == 0 && out[402] < -.03f && out[403] > .03f,
          "Prepared stems have no cross-song or Master leakage");
    app.seek(0); render();
    const auto baseLeft = out[402], baseRight = out[403];
    const auto mixState = app.services().projects->state();
    auto leftMix = mixState.project->tracks[0].mix, rightMix = mixState.project->tracks[1].mix;
    leftMix.gain = .25f; rightMix.gain = .5f;
    app.seek(0);
    check(app.preview_mixes({{mixState.project->tracks[0].id,leftMix},{mixState.project->tracks[1].id,rightMix}},mixState.project->master_gain),
          "Group mix preview uses one shared bounded mixer update");
    render();
    check(std::abs((1.f-out[402]/baseLeft)/.75f-(1.f-out[403]/baseRight)/.5f)<.00001f,
          "Both channels enter the same existing Core fader ramp");
    for (int i=0; i<3; ++i) render();
    check(std::abs(out[402]-baseLeft*.25f)<.00001f && std::abs(out[403]-baseRight*.5f)<.00001f,
          "Group preview affects both routed stems coherently");
    check(app.services().projects->state().revision==mixState.revision,
          "Mix preview does not write Project history");
    app.cancel_mix_preview(); app.seek(0);
    for (int i=0; i<4; ++i) render();
    check(std::abs(out[402]-baseLeft)<.00001f && std::abs(out[403]-baseRight)<.00001f,
          "Cancel restores all previewed channels");
    check(!app.preview_mixes({{new_id(),leftMix}},1.f),"Unknown group preview channel is rejected");
    // Regions use the already prepared source and the same Core mixer/device.
    auto arranged=one;
    arranged.stems[0].regions=std::vector<Application::PlaybackRegion>{
        {{"region-left"},0,1200,0},{{"region-right"},1200,1200,1200}};
    app.activate_playback(first,arranged);
    const auto originalSource=app.playback_sources().front();
    check(app.services().projects->state().project->clips.size()==2 &&
          app.services().projects->state().project->clips[1].source_offset==1200,
          "Prepared handoff materializes multiple regions and their source offsets");
    app.play();render();const auto singleRegion=out[400];
    rejects([&]{app.set_playback_regions(arranged.stems);});
    app.pause();render();arranged.stems[0].regions->at(1).start=0;
    app.set_playback_regions(arranged.stems);app.seek(0);app.play();render();
    check(singleRegion>.03f && std::abs(out[400]-2.f*singleRegion)<.00001f,
          "Overlapping regions sum through the existing Core mixer");
    app.pause();render();auto invalidRegions=arranged.stems;invalidRegions[0].regions->at(0).offset=4800;
    const auto priorRegions=app.services().projects->state().project;
    rejects([&]{app.validate_playback_regions(invalidRegions);});
    check(app.services().projects->state().project==priorRegions,"Invalid source offset leaves model/audio unchanged");
    arranged.stems[0].regions->clear();app.set_playback_regions(arranged.stems);
    check(app.services().projects->state().project->clips.empty() && app.source_frames(originalSource.id)==4800,
          "Deleting all regions retains source extent for restore");
    app.set_playback_regions(one.stems);
    check(app.services().projects->state().project->clips.size()==1 &&
          app.services().projects->state().project->clips.front().length==4800 && app.engine()==engine && calls->opens==1,
          "Restore recovers complete source without a second engine or reopening device");
    // Automatic Core handoff: different track counts and routes, no device stop/start.
    auto autoOne=one;autoOne.performance=PerformancePlan{};autoOne.performance->end=65;autoOne.token=100;
    auto autoTwo=two;autoTwo.performance=PerformancePlan{};autoTwo.performance->end=127;autoTwo.performance->mode=PerformanceMode::stop;autoTwo.token=101;
    app.activate_playback(first,autoOne);app.play();
    const auto autoStops=calls->stops,autoStarts=calls->starts;
    app.stage_playback(second,autoTwo,100);
    check(app.services().projects->state().project->tracks.size()==1,"Staging retains current control model");
    render();
    check(engine->performance_status().token==101&&engine->state().sample==63,"Callback adopts prepared item at intra-block boundary");
    check(out[64*4]>.03f&&out[65*4]==0&&out[65*4+2]<-.03f,"PCM switches source/routes on the exact frame");
    app.poll();
    check(app.services().projects->state().project->tracks.size()==2&&app.playback_sources().size()==2&&
          app.services().projects->state().project->time.tempos.front().bpm==90,"Poll adopts next Core project, sources and musical timeline");
    check(calls->stops==autoStops&&calls->starts==autoStarts&&calls->opens==1&&app.engine()==engine,
          "Automatic transition uses the same running device and engine without stop/start");
    render();check(engine->state().sample==127&&engine->state().playback==PlaybackState::stopped,"Next stop mode executes in the adopted graph");
    auto extended=autoOne;extended.token=102;extended.extend_to_audio=true;extended.performance->end=1;
    extended.performance->mode=PerformanceMode::stop;app.activate_playback(first,extended);
    app.seek(95990);app.play();render();
    check(engine->state().sample==96000&&engine->state().playback==PlaybackState::stopped,
          "Implicit WAV extent rounds to the same full bar as the visible timeline");
    app.activate_playback(first,one);
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 32; ++i) {
        app.activate_playback(first, one);
        app.play();
        render();
        check(out[400] > .03f && out[402] == 0, "Warm return to first item uses its own PCM/routes");
        app.activate_playback(second, two);
        app.play();
        render();
        check(out[400] == 0 && out[402] < -.03f, "Repeated warm switch cannot publish stale PCM");
    }
    check(calls->opens == 1 && calls->closes == 0 && app.engine() == engine,
          "Stress switches never reopen the device or create another engine");
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    app.activate_playback(first, one);
    app.play();
    render();
    const auto prior = app.services().projects;
    const auto priorHead = engine->state();
    const auto priorStops = calls->stops;
    auto invalid = one;
    invalid.stems[0].outputs = {9};
    rejects([&] { app.activate_playback(first, invalid); });
    invalid = one;
    invalid.stems[0].end = 10.;
    rejects([&] { app.activate_playback(first, invalid); });
    rejects([&] { app.prepare_playback({folder.path / "missing.wav"}); });
    check(app.services().projects == prior && engine->state().sample == priorHead.sample && calls->stops == priorStops,
          "Preflight failures leave current model/audio/device untouched");
    calls->failStart = true;
    rejects([&] { app.activate_playback(second, two); });
    render();
    check(app.services().projects == prior && engine->state().playback == PlaybackState::playing && out[400] > .03f,
          "Device restart failure rolls back and resumes the previous item");
    auto converted = app.prepare_playback({r});
    ready(app, converted);
    app.activate_playback(converted, one);
    check(app.services().projects->state().project->clips.front().length == 4800,
          "Preload uses the shared worker resampler");
    app.cancel_playback(converted);
    converted.reset();
    app.play();
    render();
    check(out[400] > .03f, "Retiring a preparation cannot destroy active decoded/resampled audio");
    auto cancelled = app.prepare_playback({a, b});
    app.cancel_playback(cancelled);
    rejects([&] { app.activate_playback(cancelled, two); });
    ready(app, first);
    {
        std::ofstream changed(a, std::ios::binary | std::ios::app);
        changed.put(0);
    }
    rejects([&] { app.activate_playback(first, one); });
    check(calls->opens == 1 && calls->closes == 0, "Failed/stale/cancelled preparations keep the device open");
    std::cout << "PASS Live preload: " << checks << " checks; 64 synthetic warm handoffs " << elapsed
              << " ms (not hardware ASIO)\n";
}
} // namespace
int main() {
    try {
        run();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
