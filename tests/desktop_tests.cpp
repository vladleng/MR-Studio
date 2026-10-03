#include <mrs/desktop.hpp>
#include <mrs/offline_device.hpp>
#include <chrono>
#include <thread>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
using namespace mrs; using namespace mrs::desktop;
void check(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),#__VA_ARGS__)
template<class F> void rejects(F f) { bool bad{}; try { f(); } catch (const std::exception&) { bad = true; } CHECK(bad); }
struct Directory {
    std::filesystem::path path = std::filesystem::temp_directory_path()/("mrs-shell-"+new_id().value);
    Directory() { std::filesystem::create_directory(path); }
    ~Directory() { std::error_code error; std::filesystem::remove_all(path,error); }
};
template<class F> void eventually(F f) {
    auto end = std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while (std::chrono::steady_clock::now()<end) {
        try { if (f()) return; } catch (const std::runtime_error&) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    throw std::runtime_error("timed out");
}
void workspaces() {
    Application app; auto services = app.services(); auto graph = app.graphs(); auto engine = app.engine();
    for (auto w : {Workspace::arrange,Workspace::mix,Workspace::edit,Workspace::live}) {
        app.workspace(w); CHECK(app.workspace() == w && app.services().projects == services.projects &&
            app.services().transport == services.transport && app.graphs() == graph && app.engine() == engine);
    }
    app.rename_track(services.projects->state().project->tracks.front().id,"From Arrange");
    CHECK(app.musical().state().project->tracks.front().name == "From Arrange");
    CHECK(services.projects->undo() && app.musical().state().project->tracks.front().name == "Demo tone");
    CHECK(services.projects->redo()); rejects([&] { app.workspace(static_cast<Workspace>(10)); });
}
void transport() {
    Application app; app.play();
    // Do not pump the application/UI: audio callback must continue independently.
    eventually([&] { return app.engine()->state().sample >= 1024; });
    app.poll(); CHECK(app.musical().state().transport.playback == PlaybackState::playing);
    app.pause(); eventually([&] { return app.engine()->state().playback == PlaybackState::paused; });
    const auto sample = app.engine()->state().sample; std::this_thread::sleep_for(std::chrono::milliseconds(10)); CHECK(app.engine()->state().sample == sample);
    app.musical().seek_section({"section-verse"}); eventually([&] { return app.engine()->state().sample > sample; });
    app.poll(); CHECK(app.musical().state().current_section->name == "Verse");
    app.musical().loop_section({"section-verse"}); eventually([&] { return app.engine()->state().loop.has_value(); });
    app.stop(); eventually([&] { return app.engine()->state().sample == 0 && app.engine()->state().playback == PlaybackState::stopped; });
    app.poll(); CHECK(app.musical().state().transport.sample == 0);
    rejects([&] { app.seek(-1); });
}
void files() {
    Directory dir; Application app; auto path = dir.path / std::filesystem::path(std::u8string(u8"Песня.mrsproject"));
    auto id = app.services().projects->state().project->id; app.save_project(path); CHECK(!app.dirty());
    app.rename_track(app.services().projects->state().project->tracks.front().id,"Saved"); CHECK(app.dirty()); app.save_project(path);
    auto graph = app.graphs()->state().graph; app.workspace(Workspace::live); app.open_project(path);
    CHECK(app.services().projects->state().project->id == id && app.services().projects->state().project->tracks.front().name == "Saved");
    CHECK(app.graphs()->state().graph->id == graph->id && app.workspace() == Workspace::live && !app.dirty());
    CHECK(app.services().transport->state().playback == PlaybackState::stopped);
    auto snapshot = app.snapshot(); snapshot.extensions = {{"FUTR","opaque"}};
    auto second = dir.path / "future.mrsproject"; persistence::save_project(second,snapshot);
    app.open_project(second); app.rename_track(app.services().projects->state().project->tracks.front().id,"Future"); app.save_project(second);
    CHECK(persistence::load_project(second).extensions == snapshot.extensions);
    auto before = app.services().projects; rejects([&] { app.open_project(dir.path / "missing"); }); CHECK(app.services().projects == before);
    { std::ofstream out(dir.path / "broken"); out << "bad"; }
    rejects([&] { app.open_project(dir.path / "broken"); }); CHECK(app.services().projects == before);
    app.demo(); CHECK(app.dirty() && app.path().empty());
}
void wav(const std::filesystem::path& path) {
    // Exact PCM16 stereo RIFF fixture at a non-default sample rate.
    std::ofstream f(path,std::ios::binary);
    auto u16 = [&](std::uint16_t value) { for (int i = 0; i < 2; ++i) f.put(static_cast<char>((value >> (8*i)) & 255)); };
    auto u32 = [&](std::uint32_t value) { for (int i = 0; i < 4; ++i) f.put(static_cast<char>((value >> (8*i)) & 255)); };
    f.write("RIFF",4); u32(36+4000); f.write("WAVEfmt ",8); u32(16); u16(1); u16(2); u32(44100); u32(176400); u16(4); u16(16);
    f.write("data",4); u32(4000); for (int i = 0; i < 2000; ++i) u16(4000);
}
class ManualDevice final : public audio::IAudioDevice {
public:
    std::vector<audio::DeviceInfo> enumerate() override { return {{0,"Manual render test",{},{"L","R"},32,2048,128,-1}}; }
    void control_panel(int) override {}
    void open(const audio::DeviceConfig&,std::shared_ptr<audio::AudioEngine>) override { phase_ = audio::DevicePhase::open; }
    void start() override { phase_ = audio::DevicePhase::running; }
    void stop() override { phase_ = audio::DevicePhase::stopped; }
    void close() noexcept override { phase_ = audio::DevicePhase::closed; }
    audio::DeviceStatus status() override { return {phase_,44100,0,0,0,{}}; }
private:
    audio::DevicePhase phase_{audio::DevicePhase::closed};
};
void assets() {
    Directory dir; auto file = dir.path / "music.wav"; wav(file); Application app; app.import_wav(file);
    const auto p = app.services().projects->state().project;
    CHECK(p->sample_rate == 44100 && p->clips.front().length == 1000 && p->chords.empty() && p->sections.empty());
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}});
    app.play(); std::array<float,256> output{};
    app.engine()->process(nullptr,output.data(),128); // manual device has no concurrent consumer
    CHECK(output[0] == 0.06103515625f && output[1] == 0.06103515625f); // PCM16 4000/32768 through shared native gain 0.5
    app.poll(); CHECK(app.musical().state().transport.sample == 128);
    app.disconnect(); CHECK(app.engine()->state().sample == 0);
    auto services = app.services(); rejects([&] { app.import_wav(dir.path / "absent.wav"); }); CHECK(app.services().projects == services.projects);
}
void audio_settings() {
    Application app;
    app.connect(audio::make_offline_device(),{0,48000,128,{}, {0,1}}); CHECK(app.audio_running());
    app.connect(audio::make_offline_device(),{0,48000,64,{0}, {0,1}}); CHECK(app.audio_running() && app.engine()->config().input_channels == 1);
    rejects([&] { app.connect(audio::make_offline_device(),{0,48000,63,{}, {0,1}}); }); CHECK(app.audio_running()); // validate before closing
    rejects([&] { app.connect(audio::make_offline_device(),{0,44100,128,{}, {0,1}}); }); CHECK(!app.audio_running());
    rejects([&] { app.play(); });
    app.connect(audio::make_offline_device(),{0,48000,128,{}, {0,1}}); CHECK(app.audio_running());
    app.disconnect(); CHECK(!app.audio_running() && app.engine()->state().sample == 0);
}
void config() {
    Preferences p; p.workspace = Workspace::live; p.device_name = "Komplete Audio ASIO Driver"; p.reconnect_audio = true;
    CHECK(decode_preferences(encode_preferences(p)) == p);
    auto disabled = p; disabled.reconnect_audio = false;
    CHECK(decode_preferences(encode_preferences(disabled)) == disabled);
    const auto legacy = decode_preferences("MRS_DESKTOP_CONFIG 1\n0 48000 128 -1 \"Komplete Audio ASIO Driver\" 2 0 1\n");
    CHECK(legacy.reconnect_audio && legacy.device_name == p.device_name);
    CHECK(!decode_preferences("MRS_DESKTOP_CONFIG 1\n0 48000 128 -1 \"\" 2 0 1\n").reconnect_audio);
    rejects([&] { (void)decode_preferences("MRS_DESKTOP_CONFIG 2\n0 48000 128 -1 \"\" 2 0 1 2\n"); });
    CHECK(parse_outputs("1, 2,6") == std::vector<int>({0,1,5}));
    for (auto text : {"","1,","0","1,1","65","x","1,,2","-1"}) rejects([&] { (void)parse_outputs(text); });
    rejects([&] { (void)decode_preferences("MRS_DESKTOP_CONFIG 2"); });
    rejects([&] { (void)decode_preferences(encode_preferences(p)+"extra"); });
    auto bad = p; bad.outputs = {0,0}; rejects([&] { (void)encode_preferences(bad); });
    Directory dir; Logger log(dir.path / "app.log"); log.write("control-thread log");
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected suite");
        std::string name = argv[1];
        if (name == "workspaces") workspaces(); else if (name == "transport") transport();
        else if (name == "files") files(); else if (name == "assets") assets();
        else if (name == "audio") audio_settings(); else if (name == "config") config(); else throw std::runtime_error("unknown suite");
        std::cout << "PASS desktop " << name << '\n'; return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
