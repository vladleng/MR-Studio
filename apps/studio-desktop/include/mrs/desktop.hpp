#pragma once
#include <mrs/midi_input.hpp>
#include <mrs/persistence.hpp>
#include <mrs/musical.hpp>
#include <mrs/device.hpp>
#include <fstream>
#include <mrs/arrangement.hpp>
#include <mrs/waveform.hpp>
#include <future>
#include <mrs/recording.hpp>
#include <mrs/project_folders.hpp>
namespace mrs::desktop {
enum class Workspace { arrange, edit, mix, live };
std::string_view workspace_name(Workspace);
struct DeviceProfile {
    std::string name, device_name;
    std::uint32_t rate{48000}, buffer{128};
    std::vector<int> outputs{0,1};
    int monitor_input{-1};
    std::vector<std::string> output_labels{};
    std::string input_label;
    bool operator==(const DeviceProfile&) const = default;
    void validate() const;
};
DeviceProfile capture_profile(std::string, const audio::DeviceInfo&, const audio::DeviceConfig&);
audio::DeviceConfig resolve_profile(const DeviceProfile&, const audio::DeviceInfo&);
struct Preferences {
    Workspace workspace{Workspace::arrange};
    std::uint32_t rate{48000}, buffer{128};
    std::vector<int> outputs{0,1};
    int monitor_input{-1}; // -1 disabled; other values are zero-based
    std::string device_name;
    bool reconnect_audio{};
    std::vector<std::string> recent_projects{};
    std::vector<DeviceProfile> profiles{};
    std::uint32_t processing_workers{2};
    std::uint32_t process_buffer_frames{};
    bool operator==(const Preferences&) const = default;
    void validate() const;
};
std::string encode_preferences(const Preferences&);
Preferences decode_preferences(std::string_view);
void remember_project(Preferences&, const std::filesystem::path&);
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
    void new_project(std::uint32_t rate = 48000, std::string title = "Untitled");
    Id add_audio_track(std::string name);
    Id add_instrument_track(std::string name);
    void set_midi_input(const Id&,std::string port,int channel=-1,bool monitor=true);
    std::string midi_status(const Id&) const;
    void midi_panic() noexcept {engine_->midi_panic();}
    Id add_bus(std::string name);
    Id add_return_send(const Id& source, std::string name);
    void set_track_output(const Id&, std::optional<Id>);
    void set_track_sends(const Id&, std::vector<Track::Send>);
    void set_send_gain(const Id&, std::size_t, float);
    void set_track_input(const Id&, int, bool stereo = false);
    void set_track_monitoring(const Id&, bool);
    bool stereo_track(const Id&) const;
    void set_track_armed(const Id&, bool);
    bool track_armed(const Id&) const;
    const std::vector<Id>& armed_tracks() const { return armed_tracks_; }
    std::vector<std::string> input_names();
    std::vector<std::string> output_names() const;
    std::vector<int> active_outputs() const;
    void set_hardware_output(std::optional<Id>, std::vector<int>);
    void remove_track(const Id&);
    void reorder_track(const Id&, std::size_t index);
    void import_wavs(const std::vector<std::filesystem::path>&,std::optional<Id> target={},Sample start=0);
    bool undo(); bool redo();
    void move_clip(const Id&, const Id& track, Sample start);
    void trim_clip(const Id&, Sample start, Sample end);
    Id split_clip(const Id&, Sample position);
    void remove_clip(const Id&);
    Sample source_frames(const Id&);
    void prepare_waveforms();
    const audio::Waveform* waveform(std::string_view source) const;
    void open_project(const std::filesystem::path&);
    void import_wav(const std::filesystem::path&);
    void save_project(const std::filesystem::path&);
    bool dirty() const;
    const std::filesystem::path& path() const { return path_; }
    persistence::ProjectDocument snapshot() const;
    void rename_track(const Id&, std::string);
    void set_track_mix(const Id&, Track::Mix);
    void set_master_gain(float);
    void set_inserts(std::optional<Id>, std::vector<NativeInsert>);
    void load_insert_preset(std::optional<Id>,NativeInsert);
    void preview_inserts(std::optional<Id>, const std::vector<NativeInsert>&);
    void cancel_insert_preview();
    std::uint64_t insert_generation() const {return insert_generation_;}
    bool open_plugin_editor(std::optional<Id>,const Id&,void*,int&,int&);
    void close_plugin_editors();
    void close_plugin_editor(std::optional<Id>,const Id&);
    bool plugin_failed() const;
    std::uint32_t plugin_latency(std::optional<Id>,const Id&) const;
    std::vector<processing::ParameterInfo> plugin_parameters(std::optional<Id>,const Id&) const;
    void set_plugin_parameter(std::optional<Id>,const Id&,std::uint32_t,float);
    NativeInsert capture_insert(std::optional<Id>,const Id&);
    bool preview_mix(std::optional<Id>, Track::Mix, float master_gain);
    void cancel_mix_preview();
    void connect(std::unique_ptr<audio::IAudioDevice>, audio::DeviceConfig);
    void disconnect();
    audio::DeviceStatus device_status();
    bool audio_running();
    const std::string& waveform_error() const { return waveform_error_; }
    const std::string& audio_name() const { return audio_name_; }
    void play(); void pause(); void stop(); void seek(Sample);
    void arm_track(std::optional<Id>);
    const std::optional<Id>& armed_track() const { return armed_; }
    void start_recording(const std::filesystem::path& destination);
    bool stop_recording();
    bool recording() const { return static_cast<bool>(recording_); }
    audio::RecordStatus recording_status() const;
    Sample recording_start() const { return recording_ ? recording_->start() : 0; }
    const std::string& recording_error() const { return recording_error_; }
    const std::vector<std::filesystem::path>& last_takes() const { return last_takes_; }
    const std::filesystem::path& last_take() const { return last_take_; }
    void monitoring(bool);
    bool monitoring() const { return monitoring_; }
    bool has_input() const { return device_config_ && !device_config_->inputs.empty(); }
private:
    persistence::ProjectDocument document_;
    Services services_;
    std::shared_ptr<processing::GraphStore> graphs_;
    std::shared_ptr<audio::AudioEngine> engine_;
    std::unique_ptr<MidiInputs> midi_inputs_;
    std::map<std::string,std::string> instrument_errors_;
    std::uint64_t midi_route_revision_{~std::uint64_t{}},midi_route_generation_{};
    void publish_midi_routes();
    std::shared_ptr<audio::EngineTransport> transport_;
    std::unique_ptr<MusicalTimeline> musical_;
    std::unique_ptr<audio::IAudioDevice> device_;
    std::shared_ptr<processing::PreparedGraph> prepared_;
    struct InsertRuntime {std::shared_ptr<processing::PreparedGraph> graph;std::optional<processing::GraphState> pending;};
    std::map<std::string,InsertRuntime> insert_runtime_;
    std::uint64_t insert_generation_{};
    void publish_inserts();
    void capture_insert_state(Project&,std::optional<Id> authoritative={},bool callbacks_stopped=false);
    Workspace workspace_{Workspace::arrange};
    std::shared_ptr<audio::Recorder> recording_;
    std::optional<Id> armed_;
    std::vector<Id> armed_tracks_;
    struct Capture { Id track; std::shared_ptr<audio::Recorder> recorder; };
    std::vector<Capture> captures_;
    std::vector<std::filesystem::path> last_takes_;
    std::string recording_error_;
    std::filesystem::path last_take_;
    audio::RecordStatus last_recording_status_{};
    bool monitoring_{true};
    std::vector<Id> mixer_tracks_;
    std::uint64_t applied_mix_revision_{};
    void publish_mix();
    void prepare_mixer(audio::RenderGraph&);
    void prepare_inserts(audio::RenderGraph&,const audio::DeviceConfig&);
    bool history(bool redo);
    void sync_arm();
    void require_not_recording() const;
    std::filesystem::path path_, asset_root_;
    std::string audio_name_{"Offline clock (no sound)"};
    std::string waveform_error_;
    std::uint64_t saved_project_revision_{}, saved_graph_revision_{};
    bool unsaved_{true};
    struct CachedAsset {
        std::shared_ptr<std::atomic<bool>> cancel{std::make_shared<std::atomic<bool>>(false)};
        std::shared_ptr<const audio::AudioData> data;
        std::future<audio::Waveform> pending;
        std::optional<audio::Waveform> peaks;
    };
    std::map<std::string,CachedAsset> assets_;
    // Runtime source aliases preserve ProjectStore Undo while archives use Media/... paths.
    std::map<std::string,std::filesystem::path> owned_media_;
    std::optional<audio::DeviceConfig> device_config_;
    std::vector<int> default_inputs_;
    std::optional<audio::DeviceInfo> device_info_;
    void validate_hardware(const Project&, const audio::DeviceConfig&) const;
    std::vector<int> selected_inputs(const Project&) const;
    std::vector<int> selected_inputs(const Project&, const std::vector<int>&) const;
    std::shared_ptr<const audio::AudioData> asset(const std::string&);
    void cache_asset(std::string, std::shared_ptr<const audio::AudioData>);
    void edit(const ICommand&,std::optional<Id> authoritative={});
    void require_not_playing() const;
    void rebuild_audio();
    void replace(persistence::ProjectDocument);
    audio::RenderGraph render(const audio::DeviceConfig&);
    void start_empty_clock(bool prepare_plugins=false);
};
} // namespace mrs::desktop
