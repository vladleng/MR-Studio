#pragma once
#include <mrs/ai.hpp>
#include <mrs/ai_query.hpp>
#include <mrs/ai_tools.hpp>
#include <mrs/midi_input.hpp>
#include <mrs/midi_output.hpp>
#include <mrs/persistence.hpp>
#include <mrs/musical.hpp>
#include <mrs/device.hpp>
#include <fstream>
#include <set>
#include <mrs/arrangement.hpp>
#include <mrs/midi_clips.hpp>
#include <mrs/waveform.hpp>
#include <future>
#include <mrs/recording.hpp>
#ifdef MRS_HAS_ARA
#include <mrs/ara.hpp>
#endif
#include <mrs/midi_recording.hpp>
#include <mrs/project_folders.hpp>
#include <mrs/reverb_library.hpp>
#include <mrs/cab_collection.hpp>
#include <mrs/tone_library.hpp>
#include <mrs/rig.hpp>
namespace mrs::desktop {
enum class Workspace { arrange, edit, mix, live };
struct AudioRecordingPreview { Clip clip; std::optional<audio::RecordPreview> waveform; };
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
    ai::PermissionPolicy ai{};
    audio::ClickSettings click{};
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
    explicit Application(std::unique_ptr<IMidiOutputBackend> = {});
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
    Id add_instrument_track(std::string name, NativeInsert instrument);
    void set_midi_input(const Id&,std::string port,int channel=-1,bool monitor=true);
    void set_midi_output(const Id&,std::string port,int channel=-1);
    std::string midi_output_status(const Id&) const;
    void reconnect_midi();
    std::string midi_status(const Id&) const;
    bool audition_note(const Id& track,int pitch,int velocity,int channel=0,bool on=true,bool soft_release=false);
    void midi_panic() noexcept {engine_->midi_panic();external_midi_->panic();}
    Id add_bus(std::string name);
    Id add_fx_channel(std::string name);
    Id add_return_send(const Id& source, std::string name);
    void set_track_output(const Id&, std::optional<Id>);
    void set_track_sends(const Id&, std::vector<Track::Send>);
    void set_send_gain(const Id&, std::size_t, float);
    void set_send_bypassed(const Id&, std::size_t, bool);
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
    std::vector<Id> import_midi(const std::vector<std::filesystem::path>&,std::optional<Id> target={},Tick start=0);
    bool history_preserves_plugins(bool redo) const;
    bool history_changes_only_inputs(bool redo) const; // permits frontend editor retention before Undo/Redo
    bool undo(); bool redo();
    void move_clip(const Id&, const Id& track, Sample start);
    void trim_clip(const Id&, Sample start, Sample end);
    Id split_clip(const Id&, Sample position);
    std::vector<Id> split_clips(std::vector<Id>,Sample position);
    void trim_clips(const std::vector<Clip>& expected,Sample delta,bool left);
    void set_clip_colors(std::vector<Id>,std::optional<std::uint32_t>);
    void rename_clips(std::vector<Id>,std::string base);
    void set_clip_fades(const Clip& expected,Sample fade_in,Sample fade_out);
    void split_sections(const std::vector<ArrangerSection>&,Tick);
    void split_chords(const std::vector<Chord>&,Tick);
    void remove_clip(const Id&);
    void remove_clips(std::vector<Id>);
    void move_clips(std::vector<Id>,Sample delta,int track_delta=0);
    Id create_midi_clip(const Id& track,Tick start,Tick length=4*ppq);
    void edit_midi_notes(const Id&,std::vector<Id>,const NoteEdit&);
    void set_midi_events(const Id&,std::vector<MidiChannelEvent>);
    void set_midi_notes(const Id&,std::vector<MidiNote>);
    Id duplicate_clip(const Id&);
#ifdef MRS_HAS_ARA
    void attach_ara_source(const std::string& source,const std::string& module,const std::string& class_id);
    void detach_ara_source(const std::string& source);
    void relink_ara_source(const std::string&,const std::string&,const std::string&);
    void retry_ara_source(const std::string&);
    void relink_ara_media(const std::string&,const std::filesystem::path&);
    void refresh_ara(bool force=true);
    std::optional<std::uint64_t> ara_pending_revision() const;
    std::string ara_status(const std::string& source) const;
    std::string ara_document_key(const std::string& source) const;
    std::vector<ara::ClipCache> ara_caches() const;
    void bypass_ara_source(const std::string&,bool);
    bool ara_bypassed(const std::string&) const;
    bool poll_ara();
    bool open_ara_editor(const std::string&,const Id&,void*,int&,int&);
    bool resize_ara_editor(const std::string&,int&,int&);
    void close_ara_editor(const std::string&);
    void close_ara_editors();
    void select_ara_clip(const std::string&,const Id&);
    void set_ara_preparation(std::shared_ptr<std::atomic<bool>>,std::function<void()>);
#endif
    Id merge_midi_clips(std::vector<Id>);
    std::vector<Id> copy_clips(std::vector<Id>,Sample delta,int track_delta=0);
    void set_time_map(TimeMap);
    void set_meters(const TimeMap& expected,std::vector<MeterPoint>);
    void set_tempos(const TimeMap& expected,std::vector<TempoPoint>);
    std::vector<Id> transform_sections(const std::vector<ArrangerSection>&,Tick delta,bool copy);
    void remove_sections(const std::vector<ArrangerSection>&);
    Id copy_section(const ArrangerSection& expected,Tick start);
    Id add_section(std::string,Tick,Tick,std::uint32_t color=0x4056D6);
    void update_section(const ArrangerSection& expected,ArrangerSection);
    void remove_section(const ArrangerSection& expected);
    Id add_chord(std::string symbol, Tick start, Tick end);
    void update_chord(const Chord& expected, Chord value);
    void remove_chord(const Chord& expected);
    std::vector<Id> transform_chords(const std::vector<Chord>&,Tick delta,bool copy);
    void remove_chords(const std::vector<Chord>&);
    std::vector<Id> transform_markers(const std::vector<Marker>&,Tick delta,bool copy);
    void remove_markers(const std::vector<Marker>&);
    Id copy_chord(const Chord& expected, Tick start);
    Id add_marker(std::string name, Tick tick, MarkerKind kind = MarkerKind::generic);
    void update_marker(const Marker& expected, Marker value);
    void remove_marker(const Marker& expected);
    Sample source_frames(const Id&);
    void prepare_waveforms();
    const audio::Waveform* waveform(std::string_view source) const;
    void open_project(const std::filesystem::path&);
    void import_wav(const std::filesystem::path&);
    void save_project(const std::filesystem::path&);
    bool dirty() const;
    const std::filesystem::path& path() const { return path_; }
    persistence::ProjectDocument snapshot() const;
    // AI foundation: application thread only, shared by all workspaces.
    ai::ContextResult ai_context(ai::Selection = {}) const;
    const ai::PermissionPolicy& ai_permissions() const {require_ai_owner();return ai_permissions_;}
    void set_ai_permissions(ai::PermissionPolicy);
    struct AraToolPlugin {std::string module,class_id,name,version;bool operator==(const AraToolPlugin&)const=default;};
    void set_ai_ara_catalog(std::vector<AraToolPlugin>);
    void set_ai_ara_editor_handler(std::function<void(Id)>,std::function<void(Id)> close = {});
    void set_ai_selection(ai::Selection);
    ai::PreparedTool ai_propose(const ai::ToolCall&,std::shared_ptr<const ai::Snapshot>);
    ai::Decision ai_apply(const std::shared_ptr<const ai::ToolPlan>&);
    ai::PreviewResult ai_preview(const std::vector<std::shared_ptr<const ai::ToolPlan>>&);
    ai::PreviewResult ai_rebuild_preview(const std::shared_ptr<ai::Preview>&);
    ai::Decision ai_preview_status(const std::shared_ptr<ai::Preview>&);
    ai::Decision ai_accept_preview(const std::shared_ptr<ai::Preview>&);
    void ai_reject_preview(const std::shared_ptr<ai::Preview>&);
    ai::Page ai_tool_query(const ai::ToolCall&,std::shared_ptr<const ai::Snapshot>) const;
    ai::ViewResult ai_read_view(ai::Selection = {}) const;
    ai::Page ai_query(const ai::ContextKey&,const ai::Query&,ai::Selection = {},ai::ReadPolicy = {}) const;
    ai::SnapshotResult ai_snapshot(const ai::ContextKey&,const std::vector<ai::Query>&,
        ai::Selection = {},ai::SnapshotBudget = {},ai::ReadPolicy = {}) const;
    ai::Submitted request_ai(ai::Request, ai::Task);
    struct AnalysisSource {Id clip; Sample offset{},length{};std::shared_ptr<const audio::AudioData> data;};
    std::vector<AnalysisSource> ai_analysis_sources() const;
    bool cancel_ai(std::uint64_t);
    std::optional<ai::Result> take_ai_result();
    void rename_track(const Id&, std::string);
    void set_track_color(const Id&,std::optional<std::uint32_t>);
    void set_track_mix(const Id&, Track::Mix);
    void set_master_gain(float);
    void set_inserts(std::optional<Id>, std::vector<NativeInsert>);
    std::shared_ptr<audio::CabCollection> cab_library() const {return cab_library_;}
    void set_cab_root(std::filesystem::path,std::string manifest={});
    bool model_load_pending(std::optional<Id>,const Id&)const;
    std::array<float,2> rig_levels(std::optional<Id>,const Id&) const;
    void select_rig_preset(std::optional<Id>,const Id&,const RigPreset&);
    void select_tone_model(std::optional<Id>,const Id&,const audio::ToneModel&,const audio::SourceAsset&);
    void map_cab_assets(std::optional<Id>,const Id&,const std::vector<std::string>&,
        const std::map<std::string,std::string>&,std::string rate_suffix={});
    std::shared_ptr<audio::ReverbLibrary> reverb_library() const {return reverb_library_;}
    void set_reverb_root(std::filesystem::path);
    std::shared_ptr<audio::ReverbLoader> reverb_loader(std::optional<Id>,const Id&,bool ensure=false);
    void load_insert_preset(std::optional<Id>,NativeInsert);
    void name_insert_preset(std::optional<Id>,const Id&,std::string);
    void preview_inserts(std::optional<Id>, const std::vector<NativeInsert>&);
    void cancel_insert_preview();
    std::uint64_t insert_generation() const {return insert_generation_;}
    void transfer_insert(std::optional<Id> source,const Id& slot,std::optional<Id> destination,std::size_t boundary,bool move);
    bool open_plugin_editor(std::optional<Id>,const Id&,void*,int&,int&);
    void close_plugin_editors();
    void close_plugin_editor(std::optional<Id>,const Id&);
    bool plugin_failed() const;
    std::uint32_t plugin_latency(std::optional<Id>,const Id&) const;
    std::vector<processing::ParameterInfo> plugin_parameters(std::optional<Id>,const Id&) const;
    void set_plugin_parameter(std::optional<Id>,const Id&,std::uint32_t,float);
    NativeInsert capture_insert(std::optional<Id>,const Id&);
    bool preview_mix(std::optional<Id>, Track::Mix, float master_gain);
    bool preview_send_gain(const Id& source,const Id& bus,float gain);
    void cancel_mix_preview();
    void connect(std::unique_ptr<audio::IAudioDevice>, audio::DeviceConfig);
    void disconnect();
    audio::DeviceStatus device_status();
    bool audio_running();
    const std::string& waveform_error() const { return waveform_error_; }
    const std::string& audio_name() const { return audio_name_; }
    void play(); void pause(); void stop(); void seek(Sample);
    // Preview never uses a second ASIO device or manipulates project transport.
    // Unavailable/offline/recording state returns false without side effects.
    bool play_library_preview(const std::filesystem::path&,double fraction=0);
    void stop_library_preview() noexcept;
    audio::AudioEngine::LibraryPreviewState library_preview_state() const noexcept;
    void arm_track(std::optional<Id>);
    const std::optional<Id>& armed_track() const { return armed_; }
    void start_recording(const std::filesystem::path& destination);
    // Control thread: managed Media destination; an unsaved session gets its own project.
    void start_project_recording(const std::filesystem::path& projects_folder);
    bool stop_recording();
    void set_click_settings(audio::ClickSettings);
    audio::ClickSettings click_settings() const {return click_settings_;}
    bool recording() const { return static_cast<bool>(recording_)||!midi_captures_.empty(); }
    audio::RecordStatus recording_status() const;
    std::vector<Clip> midi_recording_preview() const;
    std::vector<AudioRecordingPreview> audio_recording_preview() const;
    Sample recording_start() const { return recording()?recording_visible_start_:0; }
    const std::string& recording_error() const { return recording_error_; }
    const std::vector<std::filesystem::path>& last_takes() const { return last_takes_; }
    const std::filesystem::path& last_take() const { return last_take_; }
    void monitoring(bool);
    bool monitoring() const { return monitoring_; }
    bool has_input() const { return device_config_ && !device_config_->inputs.empty(); }
private:
    const std::thread::id ai_owner_{std::this_thread::get_id()};
    Id ai_session_;
    std::unique_ptr<ai::Worker> ai_worker_; // lazy, no thread/network when unused
    ai::PermissionPolicy ai_permissions_;
    std::shared_ptr<std::atomic<std::uint64_t>> ai_permission_revision_{std::make_shared<std::atomic<std::uint64_t>>(1)};
    struct AiReceipt {ai::ContextKey key;std::uint64_t policy;std::weak_ptr<const ai::Snapshot> snapshot;ai::Selection selection;};
    std::map<std::uint64_t,AiReceipt> ai_receipts_;
    std::map<std::string,std::shared_ptr<const ai::ToolPlan>> ai_operations_;
    std::vector<std::shared_ptr<ai::Preview>> ai_previews_;
    std::set<std::pair<std::uint64_t,std::string>> ai_applied_operations_;
    ai::PreviewResult build_ai_preview(const std::vector<std::shared_ptr<const ai::ToolPlan>>&);
    ai::RuntimeSafety ai_safety() const;
    ai::Error ai_receipt(const ai::ToolCall&,const std::shared_ptr<const ai::Snapshot>&) const;
    struct AraToolEntry {Id id;AraToolPlugin plugin;};
    std::vector<AraToolEntry> ai_ara_catalog_;
    std::function<void(Id)> ai_ara_open_;
    std::function<void(Id)> ai_ara_close_;
    std::optional<ai::Selection> ai_selection_;
    ai::AraReadState ai_ara_read_state() const;
    ai::Error ai_ara_validate(const ai::ToolCall&) const;
    ai::PreviewResult ai_ara_preview(const std::vector<std::shared_ptr<const ai::ToolPlan>>&);
    void ai_ara_execute(const ai::ToolCall&);
    Connection ai_project_changed_, ai_graph_changed_;
    void require_ai_owner() const;
    ai::ContextKey ai_key() const;
    persistence::ProjectDocument document_;
    Services services_;
    std::shared_ptr<processing::GraphStore> graphs_;
    std::shared_ptr<audio::AudioEngine> engine_;
    std::unique_ptr<MidiInputs> midi_inputs_;
    std::shared_ptr<audio::ExternalMidiQueue> external_midi_;
    std::unique_ptr<MidiOutputs> midi_outputs_;
    std::map<std::string,std::string> instrument_errors_;
    std::uint64_t midi_route_revision_{~std::uint64_t{}},midi_route_generation_{};
    void publish_midi_routes();
    std::shared_ptr<audio::EngineTransport> transport_;
    audio::ClickSettings click_settings_{};
    std::unique_ptr<MusicalTimeline> musical_;
    std::unique_ptr<audio::IAudioDevice> device_;
    std::shared_ptr<processing::PreparedGraph> prepared_;
    struct InsertRuntime {std::shared_ptr<processing::PreparedGraph> graph;std::optional<processing::GraphState> pending;};
    std::map<std::string,InsertRuntime> insert_runtime_;
    // Control-thread transaction: an empty key denotes Master, a null graph an empty chain.
    std::optional<std::string> staged_insert_target_;
    std::shared_ptr<processing::PreparedGraph> staged_insert_graph_;
    std::filesystem::path cab_root_;
    std::shared_ptr<audio::CabCollection> cab_library_{std::make_shared<audio::CabCollection>()};
    std::shared_ptr<audio::ReverbLibrary> reverb_library_{std::make_shared<audio::ReverbLibrary>()};
    struct ToneRequest {std::uint64_t revision{};std::map<std::string,std::string> metadata;std::string asset,model;std::optional<NativeInsert> rig;};
    struct ReverbRuntime {std::shared_ptr<audio::ReverbLoader> loader;std::optional<Id> track;Id slot;std::uint64_t observed{};std::optional<ToneRequest> tone;};
    std::vector<ReverbRuntime> reverb_runtime_;
    void stop_reverb_loaders();
    void poll_reverbs();
    const Project* preparing_project_{}; // Control-thread candidate, never published to project observers.
    std::uint64_t insert_generation_{};
    std::map<std::string,std::shared_ptr<processing::PreparedGraph>> staged_insert_graphs_;
    void publish_inserts();
    void capture_insert_state(Project&,std::optional<Id> authoritative={},bool callbacks_stopped=false);
    Workspace workspace_{Workspace::arrange};
    std::shared_ptr<audio::Recorder> recording_;
    Sample recording_visible_start_{}; // control-owned take origin; source may contain hidden prefix
    std::optional<Id> armed_;
    std::vector<Id> armed_tracks_;
    struct Capture { Id track; std::shared_ptr<audio::Recorder> recorder; };
    std::vector<Capture> captures_;
    std::vector<std::filesystem::path> last_takes_;
    struct MidiCapture {Id track;std::shared_ptr<audio::MidiRecorder> recorder;};
    std::vector<MidiCapture> midi_captures_;
    std::string recording_error_;
    std::filesystem::path last_take_;
    audio::RecordStatus last_recording_status_{};
    bool monitoring_{true};
    std::vector<Id> mixer_tracks_;
#ifdef MRS_HAS_ARA
    std::unique_ptr<ara::Arrangement> ara_;
    std::optional<LoopRange> ara_cycle_;
    std::shared_ptr<std::atomic<bool>> ara_cancel_;
    std::function<void()> ara_wait_;
    void synchronize_ara(const Project* = nullptr);
    void change_ara_bindings(std::vector<AraBinding>,const std::string& strict_source = {});
#endif
    std::uint64_t applied_mix_revision_{};
    void publish_mix();
    void prepare_mixer(audio::RenderGraph&);
    void prepare_midi_clips(audio::RenderGraph&);
    void prepare_inserts(audio::RenderGraph&,const audio::DeviceConfig&,bool retain=false);
    bool history(bool redo);
    void edit_metadata(const ICommand&,bool staged=false);
    bool edit_timing(Project,const ICommand*,std::optional<bool> history={},bool midi=false,bool allow_loop=false);
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
    void edit(const ICommand&,std::optional<Id> authoritative={},bool retain_inserts=false);
    void require_not_playing() const;
    void rebuild_audio(bool retain_inserts=false);
    void replace(persistence::ProjectDocument);
    audio::RenderGraph render(const audio::DeviceConfig&,bool retain_inserts=false);
    void start_empty_clock(bool prepare_plugins=false);
};
} // namespace mrs::desktop
