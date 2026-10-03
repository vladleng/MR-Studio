#pragma once
#include <mrs/persistence.hpp>
#include <mrs/musical.hpp>
#include <mrs/device.hpp>
#include <fstream>
namespace mrs::desktop {
enum class Workspace { arrange, edit, mix, live };
std::string_view workspace_name(Workspace);
struct Preferences {
    Workspace workspace{Workspace::arrange};
    std::uint32_t rate{48000}, buffer{128};
    std::vector<int> outputs{0,1};
    int monitor_input{-1}; // -1 disabled; other values are zero-based
    std::string device_name;
    bool reconnect_audio{};
    bool operator==(const Preferences&) const = default;
    void validate() const;
};
std::string encode_preferences(const Preferences&);
Preferences decode_preferences(std::string_view);
std::vector<int> parse_outputs(std::string_view); // one-based comma list -> zero-based
class Logger {
public:
    explicit Logger(const std::filesystem::path&);
    void write(std::string_view); // application thread only; never audio callback
private:
    std::ofstream stream_;
};
persistence::ProjectDocument foundation_demo();
// Owns application services ONCE. Every workspace receives these same objects.
// Window/UI state and preferences do not duplicate project/transport/audio state.
class Application {
public:
    Application();
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    const Services& services() const { return services_; }
    std::shared_ptr<processing::GraphStore> graphs() const { return graphs_; }
    MusicalTimeline& musical() { return *musical_; }
    std::shared_ptr<audio::AudioEngine> engine() const { return engine_; }
    Workspace workspace() const { return workspace_; }
    void workspace(Workspace);
    void poll();
    void demo();
    void open_project(const std::filesystem::path&);
    void import_wav(const std::filesystem::path&);
    void save_project(const std::filesystem::path&);
    bool dirty() const;
    const std::filesystem::path& path() const { return path_; }
    persistence::ProjectDocument snapshot() const;
    void rename_track(const Id&, std::string);
    void connect(std::unique_ptr<audio::IAudioDevice>, audio::DeviceConfig);
    void disconnect();
    audio::DeviceStatus device_status();
    bool audio_running();
    const std::string& audio_name() const { return audio_name_; }
    void play(); void pause(); void stop(); void seek(Sample);
private:
    persistence::ProjectDocument document_;
    Services services_;
    std::shared_ptr<processing::GraphStore> graphs_;
    std::shared_ptr<audio::AudioEngine> engine_;
    std::shared_ptr<audio::EngineTransport> transport_;
    std::unique_ptr<MusicalTimeline> musical_;
    std::unique_ptr<audio::IAudioDevice> device_;
    std::shared_ptr<processing::PreparedGraph> prepared_;
    Workspace workspace_{Workspace::arrange};
    std::filesystem::path path_, asset_root_;
    std::string audio_name_{"Offline clock (no sound)"};
    std::uint64_t saved_project_revision_{}, saved_graph_revision_{};
    bool unsaved_{true};
    void replace(persistence::ProjectDocument);
    audio::RenderGraph render(const audio::DeviceConfig&);
    void start_empty_clock();
};
} // namespace mrs::desktop
