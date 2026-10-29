#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <mrs/desktop.hpp>
#include <mrs/playback_preflight.hpp>
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
        return {{0, "Prepared Playback test", {"Input 1","Input 2"}, {"1", "2", "3", "4"}, 32, 2048, 128, -1}};
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
void live_channels(const std::filesystem::path& file) {
    Application app;app.new_project(48000);app.workspace(Workspace::live);
    const auto calls=std::make_shared<Calls>();app.connect(std::make_unique<Device>(calls),{0,48000,128,{0,1},{0,1,2,3}});
    const auto engine=app.engine();
    Track left{Id{"show-channel-left"},"Left",TrackKind::audio,{}};
    left.input=0;left.input_monitor=true;left.hardware_outputs={0};
    NativeInsert gain;gain.id=Id{"left-gain"};gain.kind=InsertKind::gain;gain.gain=.5f;left.inserts={gain};
    Track right{Id{"show-channel-right"},"Right",TrackKind::audio,{}};
    right.input=1;right.input_monitor=true;right.hardware_outputs={2};
    gain.id=Id{"right-gain"};gain.gain=.25f;right.inserts={gain};
    app.update_live_channels({}, {left,right});
    const std::vector<Id> ids{left.id,right.id};
    std::array<float,256> in{};std::array<float,512> out{};
    for(std::size_t i=0;i<128;++i){in[i*2]=.4f;in[i*2+1]=.8f;}
    const auto render=[&]{engine->process(in.data(),out.data(),128);};
    render();render();
    check(out[400]>.15f&&out[400]<.25f&&out[402]>.15f&&out[402]<.25f&&out[401]==0&&out[403]==0,
          "Two physical channels run independent inputs, chains and direct outputs on one Core");
    check(app.engine()==engine&&app.services().projects->state().project->tracks.size()==2,"Multi-input uses the original Engine");
    auto media=app.prepare_playback({file});ready(app,media);
    Application::PlaybackLayout layout;layout.stems.resize(1);layout.stems[0].outputs={1,3};layout.live_tracks=ids;
    app.activate_playback(media,layout);app.play();render();render();
    check(out[400]>.15f&&out[402]>.15f&&out[401]>.03f&&out[403]>.03f,
          "Independent monitored channels and WAV Playback share the graph");
    rejects([&]{app.activate_playback(media,layout);});
    app.stop();render();
    auto current=app.services().projects->state().project;
    left=*std::find_if(current->tracks.begin(),current->tracks.end(),[&](const auto& t){return t.id==left.id;});
    right=*std::find_if(current->tracks.begin(),current->tracks.end(),[&](const auto& t){return t.id==right.id;});
    left.input_monitor=false;app.update_live_channels(ids,{left,right});
    app.activate_playback(media,layout);render();render();
    check(out[400]==0&&out[402]>.15f,"Stopped item handoff retains Monitor OFF independently of the other live input");
    auto invalid=layout;invalid.live_tracks={left.id,left.id};const auto previous=app.services().projects->state().project;
    rejects([&]{app.activate_playback(media,invalid);});
    check(app.services().projects->state().project==previous,"Duplicate live mapping rejects without changing the project");
    right.hardware_outputs={9};rejects([&]{app.update_live_channels(ids,{left,right});});
    check(app.services().projects->state().project==previous,"Invalid channel route leaves the old graph/model intact");
    right.hardware_outputs={2};left.input_monitor=true;calls->failStart=true;
    rejects([&]{app.update_live_channels(ids,{left,right});});render();render();
    check(app.services().projects->state().project==previous&&app.audio_running()&&out[400]==0&&out[402]>.15f,
          "Channel restart failure rolls back model, inputs, device and running PCM");
    app.update_live_channels(ids,{left,right});render();render();
    check(out[400]>.15f&&out[402]>.15f,"Explicit monitor re-enable restores only the chosen channel");
    auto empty=app.prepare_playback({});ready(app,empty);Application::PlaybackLayout emptyLayout;emptyLayout.live_tracks=ids;
    app.activate_playback(empty,emptyLayout);render();render();
    check(app.services().projects->state().project->tracks.size()==2&&app.playback_sources().empty()&&out[400]>.15f&&out[402]>.15f,
          "WAV to empty item retains both stable input channels and their chains");
    app.update_live_channels(ids,{});
    check(app.services().projects->state().project->tracks.empty(),"Removing Live channels retires their runtime off callback");
}
void run() {
    Folder folder;
    const auto a = folder.path / "a.wav", b = folder.path / "b.wav", r = folder.path / "resample.wav";
    wav(a);
    wav(b, -1);
    wav(r, 1, 44100);
    live_channels(a);
    {using namespace mrs::audio;
        PlaybackSourceCheck source;source.key="source";source.file=a;
        const std::array<int,2> outputs{0,1};
        const auto inspect=[&]{return check_playback_sources(std::span<const PlaybackSourceCheck>(&source,1),48000,outputs).front();};
        check(inspect().code==PlaybackCheckCode::ok,"Preflight checks a valid WAV without activating a project");
        source.outputs={9};check(inspect().code==PlaybackCheckCode::outputs,"Preflight explains unavailable physical outputs");
        source.outputs.clear();source.end=10.;check(inspect().code==PlaybackCheckCode::trim,"Preflight rejects out-of-source trim");
        source.end=-1;source.regions=std::vector<PlaybackSourceCheck::Region>{{4790,20}};
        check(inspect().code==PlaybackCheckCode::regions,"Preflight checks source bounds of audio regions");source.regions.reset();
        source.expected=PlaybackFileStamp{0,std::filesystem::last_write_time(a)};
        check(inspect().code==PlaybackCheckCode::changed,"Preflight detects stale prepared media");source.expected.reset();
        source.file=r;check(inspect().resample&&inspect().frames==4800,"Preflight uses the shared resampler extent");
        source.file=folder.path/"missing.wav";check(inspect().code==PlaybackCheckCode::missing,"Preflight reports missing sources");
        const auto corrupt=folder.path/"corrupt.wav";{std::ofstream out(corrupt);out<<"invalid";}source.file=corrupt;
        check(inspect().code==PlaybackCheckCode::unreadable,"Preflight reports corrupt WAV without changing sources");
        std::stop_source cancelled;cancelled.request_stop();
        check(check_playback_sources(std::span<const PlaybackSourceCheck>(&source,1),48000,outputs,cancelled.get_token()).empty(),"Preflight cancellation retires outside audio processing");
    }
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
    app.connect(std::make_unique<Device>(calls), {0, 48000, 128, {0, 1}, {0, 1, 2, 3}, 1});
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
    auto extended=autoOne;extended.token=102;extended.extend_to_audio=true;extended.performance->end=16*96000;
    extended.performance->sections={{0,16*96000,PerformanceMode::loop}};
    extended.performance->mode=PerformanceMode::stop;app.activate_playback(first,extended);
    const auto audioEnd=app.playback_sources().front().length;
    app.seek(audioEnd-10);app.play();render();
    check(engine->state().sample==audioEnd&&engine->state().playback==PlaybackState::stopped,
          "Short audio replaces the sixteen-bar empty default and stops at its exact sample end");
    auto multiExtent=extended;multiExtent.token=103;multiExtent.stems.resize(2);
    multiExtent.stems[0].end=.05;
    app.activate_playback(second,multiExtent);
    check(app.playback_sources()[0].length<app.playback_sources()[1].length,"Multistem extent fixture has a shorter first stem");
    Sample longest{};for(const auto& clip:app.playback_sources())longest=std::max(longest,clip.start+clip.length);
    app.seek(longest-10);app.play();render();
    check(engine->state().sample==longest&&engine->state().playback==PlaybackState::stopped,
          "Multistem item ends at the longest prepared audio source");
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
