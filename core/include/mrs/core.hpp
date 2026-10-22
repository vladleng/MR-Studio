#pragma once
#include <mrs/stretch.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace mrs {
using Sample = std::int64_t;
using Tick = std::int64_t;
inline constexpr Tick ppq = 960;
inline constexpr std::size_t max_project_midi_notes = 32768;
inline constexpr Tick max_tick = 1'000'000'000'000;
inline constexpr Sample max_sample = 4'503'599'627'370'496;
inline constexpr std::uint32_t schema_version = 31;
struct Id {
    std::string value;
    bool operator==(const Id&) const = default;
};
// Generation is off the realtime thread; saved/imported IDs are preserved.
Id new_id();
struct MusicalPosition {
    std::int64_t bar{1};
    int beat{1}; // denominator-note beat, one-based
    Tick tick{}; // ticks within this beat
    bool operator==(const MusicalPosition&) const = default;
};
struct TempoPoint {
    Tick tick{};
    double bpm{120.0};
    bool operator==(const TempoPoint&) const = default;
};
struct MeterPoint {
    std::int64_t bar{1}; // changes at bar boundaries
    int numerator{4};
    int denominator{4};
    bool operator==(const MeterPoint&) const = default;
};
struct TimeMap {
    std::vector<TempoPoint> tempos{{}};
    std::vector<MeterPoint> meters{{}};
    void validate() const;
    bool operator==(const TimeMap&) const = default;
};
enum class TrackKind { audio, midi, bus, instrument };
enum class MarkerKind { generic, cue, warning, lyric, action, navigation };
struct Folder {
    Id id;
    std::string name;
    std::optional<Id> parent;
    std::optional<Id> bus{};
    bool collapsed{};
    bool operator==(const Folder&) const = default;
};
enum class InsertKind { gain, highpass, lowpass, eq, channel_eq, vst3, cab_ir, reverb, cab_collection, rig };
struct ReverbReference {
    std::string relative, absolute, digest;
    std::uint64_t bytes{};
    bool operator==(const ReverbReference&) const = default;
};
struct ReverbState {
    float pre_delay{}, tone{}, mix{.25f};
    bool gate_enabled{};
    float gate_threshold{-90.f};
    std::string name;
    ReverbReference first, second;
    std::uint32_t sample_rate{}, channels{}, quad_map{};
    std::uint64_t frames{};
    void validate() const;
    bool operator==(const ReverbState&) const = default;
};
struct CabIr {
    std::string name;
    std::uint32_t sample_rate{48000}, channels{1};
    std::vector<float> samples; // embedded original, mono or independent L/R kernels
    float mix{1}, low_cut{20}, high_cut{20000};
    bool invert{};
    void validate() const;
    bool operator==(const CabIr&) const = default;
};
struct EqBand {
    float frequency{1000}, gain{}, q{0.70710678f};
    bool enabled{true};
    bool operator==(const EqBand&) const = default;
};
struct InsertParameter {
    std::uint32_t id{}; float value{};
    bool operator==(const InsertParameter&) const = default;
};
struct RigModule {
    Id id;
    std::string type_id;
    std::uint32_t version{1};
    bool bypass{};
    std::vector<InsertParameter> parameters;
    std::string opaque_state; // Unsupported modules remain recoverable, never substituted.
    bool operator==(const RigModule&) const = default;
};
struct RigParameterDescriptor {
    std::uint32_t id;
    std::string_view name, unit;
    float minimum, maximum, initial;
};
struct ModuleDescriptor {
    std::string_view type_id, name;
    std::uint32_t version{1};
    bool pre{true}, post{true};
    std::uint32_t latency{};
    std::uint32_t channel_layouts{3}; // mono | stereo
    bool realtime_safe{true};
    std::span<const RigParameterDescriptor> parameters;
};
inline constexpr std::array<RigParameterDescriptor,1> rig_gain_parameters{{{0,"Gain","linear",0,4,1}}};
inline constexpr std::array<RigParameterDescriptor,2> rig_filter_parameters{{{0,"Frequency","Hz",20,20000,1000},{1,"Q","",.1f,10,.70710678f}}};
inline constexpr std::array<RigParameterDescriptor,3> rig_eq_parameters{{{0,"Frequency","Hz",20,20000,1000},{1,"Q","",.1f,10,.70710678f},{2,"Gain","dB",-24,24,0}}};
inline constexpr std::array<RigParameterDescriptor,5> rig_compressor_parameters{{
    {0,"Threshold","dB",-60,0,-18},{1,"Ratio",":1",1,20,4},{2,"Attack","ms",.1f,100,10},
    {3,"Release","ms",10,1000,100},{4,"Output","dB",-24,24,0}
}};
inline constexpr std::array<RigParameterDescriptor,3> rig_drive_parameters{{
    {0,"Gain","dB",0,36,6},{1,"Tone","Hz",200,12000,6000},{2,"Level","dB",-24,12,0}
}};
// Private numeric host transport, never a public automation or preset identity.
inline constexpr std::size_t rig_slot_fields=7; // type, bypass, five descriptor parameters
inline constexpr std::array<ModuleDescriptor,6> rig_modules{{
    {"mrs.gain","Gain",1,true,true,0,3,true,rig_gain_parameters},
    {"mrs.highpass","High pass",1,true,true,0,3,true,rig_filter_parameters},
    {"mrs.lowpass","Low pass",1,true,true,0,3,true,rig_filter_parameters},
    {"mrs.eq","Bell EQ",1,true,true,0,3,true,rig_eq_parameters},
    {"mrs.compressor","Compressor",1,true,true,0,3,true,rig_compressor_parameters},
    {"mrs.drive","Drive",1,true,true,0,3,true,rig_drive_parameters}
}};
inline constexpr std::size_t rig_lane_capacity=8;
const ModuleDescriptor* rig_module_descriptor(const RigModule&) noexcept;
RigModule make_rig_module(std::string_view type);
struct RigState {
    std::uint32_t version{2};
    float input_gain{1}, output_gain{1};
    std::string name, artwork, artwork_rights;
    std::vector<RigModule> pre_fx, post_fx;
    void validate() const;
    bool operator==(const RigState&) const = default;
};
struct NativeInsert {
    Id id;
    InsertKind kind{InsertKind::gain};
    float gain{1}, frequency{1000}, q{0.70710678f}; // gain: linear trim or EQ dB
    bool bypass{};
    // HP, three bell bands, LP. Filters initially off, bell bands flat.
    std::array<EqBand,5> bands{{{40,0,0.70710678f,false},{150,0,0.70710678f,true},{1000,0,0.70710678f,true},{6000,0,0.70710678f,true},{16000,0,0.70710678f,false}}};
    std::string plugin_path, class_id, plugin_name;
    std::vector<std::byte> component_state, controller_state;
    std::vector<InsertParameter> parameters;
    CabIr ir;
    ReverbState reverb; // External identity only; global library root belongs to preferences.
    std::string cab_collection_id, cab_variant_id;
    std::map<std::string,std::string> cab_metadata; // Selected external IR role/capture; survives absent catalog.
    std::string preset_name; // Last MR preset saved/loaded; library label, not a file dependency.
    RigState rig;
    void validate() const;
    bool operator==(const NativeInsert&) const = default;
};
inline bool wav_core(InsertKind kind){return kind==InsertKind::cab_collection||kind==InsertKind::rig;}
void validate_cab_metadata(const std::map<std::string,std::string>&);
struct Track {
    Id id;
    std::string name;
    TrackKind kind{TrackKind::audio};
    std::optional<Id> folder;
    struct Mix {
        static constexpr float minimum_gain=0,maximum_gain=16,initial_gain=1;
        static constexpr float minimum_pan=-1,maximum_pan=1,initial_pan=0;
        float gain{initial_gain}, pan{initial_pan};
        bool mute{}, solo{};
        bool operator==(const Mix&) const = default;
        void validate() const;
    } mix{};
    std::optional<Id> output{}; // no destination means master; otherwise a shared bus
    struct Send {
        Id bus;
        float gain{1};
        bool pre_fader{};
        bool bypass{}; // Preserve the configured gain while the send is off.
        bool operator==(const Send&) const = default;
    };
    std::vector<Send> sends{}; // up to eight independent sends to shared buses/returns
    int input{-2}; // audio only: -2 default device input, -1 off, otherwise physical index
    bool input_stereo{};
    bool input_monitor{};
    std::vector<int> hardware_outputs{}; // mono or stereo physical channels; exclusive with output bus
    std::vector<NativeInsert> inserts{}; // pre-fader; at most eight, 32 total in project
    std::string midi_input{}; // stable Windows port key; empty = off
    int midi_channel{-1}; // -1 all, otherwise zero-based 0..15
    bool midi_monitor{true}; // instrument live ingress; no MIDI recording in 4a
    std::string midi_output{}; // external Windows port key; empty = off
    int midi_output_channel{-1}; // -1 preserve authored channel, otherwise 0..15
    bool fx_channel{}; // FX return role; processing/routing remains TrackKind::bus.
    std::optional<std::uint32_t> color{}; // Channel RGB; independent of clip colors.
    bool operator==(const Track&) const = default;
};
struct MidiNote {
    Id id;
    Tick start{}, length{ppq}; // source-relative ticks
    int pitch{60}, velocity{100}, channel{}; // channel zero-based
    bool operator==(const MidiNote&) const = default;
};
struct MidiChannelEvent {
    Id id; Tick start{}; int kind{2},channel{},data1{},data2{}; // MidiKind ordinals 2..6
    bool operator==(const MidiChannelEvent&) const = default;
};
struct MidiClip {
    Tick start{}, length{4*ppq}, source_offset{};
    std::vector<MidiNote> notes;
    std::vector<MidiChannelEvent> events{};
    bool operator==(const MidiClip&) const = default;
};
// A view into a non-destructive processed source span. Shared source-domain
// endpoints survive ratio/bypass changes and keep adjacent splits contiguous.
struct ClipStretch {
    stretch::Parameters parameters;
    Sample source_frames{};
    double window_begin{}, window_end{};
    bool operator==(const ClipStretch&) const = default;
};
struct Clip {
    Id id;
    Id track;
    std::string name;
    Sample start{};
    Sample length{1};
    Sample source_offset{};
    std::string source; // opaque asset reference, Stage 0 does not open it
    std::optional<MidiClip> midi{}; // when present, sample fields stay at defaults
    std::optional<std::uint32_t> color{}; // RGB; absent preserves legacy blue
    Sample fade_in{},fade_out{}; // Audio only, linear ramps in visible clip samples.
    std::optional<ClipStretch> stretch{};
    bool operator==(const Clip&) const = default;
};
struct Marker {
    Id id;
    std::string name;
    Tick tick{};
    MarkerKind kind{MarkerKind::generic};
    std::optional<std::uint32_t> color{}; // unset retains the legacy kind color
    bool operator==(const Marker&) const = default;
};
// Single non-overlapping lanes. Ranges are [start, end); gaps mean no chord/section.
struct Chord {
    Id id;
    std::string symbol; // authored spelling, no harmonic inference
    Tick start{};
    Tick end{1};
    std::optional<std::uint32_t> color{}; // unset retains the legacy neutral fill
    bool operator==(const Chord&) const = default;
};
struct ArrangerSection {
    Id id;
    std::string name;
    Tick start{};
    Tick end{1};
    std::uint32_t color{0x4056D6}; // 0xRRGGBB
    bool operator==(const ArrangerSection&) const = default;
};
// Durable ARA host binding; runtime handles and prepared PCM are never serialized.
// Source is a media reference; persistent_id remains stable across Save As/relink.
struct AraBinding {
    std::string source,persistent_id,module,class_id,name,factory_id,version,archive_id;
    bool bypass{};
    std::vector<std::byte> archive,component_state,controller_state;
    std::uint64_t checksum{},source_checksum{};
    // Empty in schema28: legacy isolated source document. A nonempty ID groups
    // sources of the same plugin within this project.
    std::string document_id;
    bool operator==(const AraBinding&) const = default;
};
struct Project {
    static constexpr float minimum_master_gain=0,maximum_master_gain=16,initial_master_gain=1;
    std::uint32_t version{schema_version};
    Id id;
    std::string title;
    std::string artist;
    std::uint32_t sample_rate{48'000};
    TimeMap time;
    std::vector<Folder> folders;
    std::vector<Track> tracks;
    std::vector<Clip> clips;
    std::vector<Marker> markers;
    std::vector<Chord> chords;
    std::vector<ArrangerSection> sections;
    float master_gain{initial_master_gain};
    std::vector<int> master_outputs{}; // empty = first selected mono/stereo hardware outputs
    std::vector<NativeInsert> master_inserts{};
    std::vector<AraBinding> ara_bindings;
    void validate() const;
    bool operator==(const Project&) const = default;
};
class Timeline {
public:
    Timeline(TimeMap map, std::uint32_t sample_rate);
    Sample to_samples(Tick tick) const; // nearest sample
    Tick to_ticks(Sample sample) const; // nearest tick
    MusicalPosition musical_position(Tick tick) const;
    Tick to_ticks(MusicalPosition position) const;
    std::uint32_t sample_rate() const { return rate_; }
private:
    struct TempoSegment { Tick tick; double sample; double samples_per_tick; };
    struct MeterSegment { Tick tick; MeterPoint meter; };
    std::vector<TempoSegment> tempos_;
    std::vector<MeterSegment> meters_;
    std::uint32_t rate_;
};
// Non-RT projections. MIDI ticks are authoritative; audio sample ranges unchanged.
Sample clip_start(const Clip&, const Timeline&);
Sample clip_end(const Clip&, const Timeline&);
class Connection {
public:
    explicit Connection(std::function<void()> disconnect = {});
    Connection(Connection&& other) noexcept;
    Connection& operator=(Connection&& other) noexcept;
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    ~Connection();
    void disconnect() noexcept;
private:
    std::function<void()> disconnect_;
};
// Application-thread only, never invoked from an audio callback.
// RAII subscriptions; reentrant events are FIFO; listener exceptions are isolated.
template<class T> class Signal {
    struct State {
        std::uint64_t next{1};
        std::map<std::uint64_t, std::function<void(const T&)>> listeners;
        std::deque<T> pending;
        bool dispatching{};
        std::uint64_t errors{};
    };
    std::shared_ptr<State> state_{std::make_shared<State>()};
public:
    Signal() = default;
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    Connection subscribe(std::function<void(const T&)> callback) {
        const auto key = state_->next++;
        state_->listeners.emplace(key, std::move(callback));
        return Connection{[weak = std::weak_ptr<State>(state_), key] {
            if (auto state = weak.lock()) state->listeners.erase(key);
        }};
    }
    void publish(T value) {
        auto state = state_;
        state->pending.push_back(std::move(value));
        if (state->dispatching) return;
        state->dispatching = true;
        try {
            while (!state->pending.empty()) {
                auto event = std::move(state->pending.front());
                state->pending.pop_front();
                const auto callbacks = state->listeners;
                for (const auto& [key, callback] : callbacks) {
                    if (!state->listeners.contains(key)) continue;
                    try { callback(event); } catch (...) { ++state->errors; }
                }
            }
        } catch (...) {
            state->dispatching = false;
            state->pending.clear();
            throw;
        }
        state->dispatching = false;
    }
    std::uint64_t observer_errors() const { return state_->errors; }
};
enum class PlaybackState { stopped, paused, playing };
struct LoopRange {
    Sample start{};
    Sample end{1}; // exclusive
    bool operator==(const LoopRange&) const = default;
};
struct TransportState {
    PlaybackState playback{PlaybackState::stopped};
    Sample sample{};
    MusicalPosition musical;
    std::optional<LoopRange> loop;
    bool operator==(const TransportState&) const = default;
};
class ITransport {
public:
    virtual ~ITransport() = default;
    virtual TransportState state() const = 0;
    // Control thread only. Rebind musical interpretation, preserving sample clock.
    virtual void rebind_timeline(Timeline) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0; // reset to sample zero
    virtual void seek(Sample sample) = 0;
    virtual void set_loop(std::optional<LoopRange> loop) = 0;
    virtual Connection subscribe(std::function<void(const TransportState&)>) = 0;
};
class MockTransport final : public ITransport {
public:
    explicit MockTransport(Timeline timeline);
    TransportState state() const override;
    void rebind_timeline(Timeline) override;
    void play() override;
    void pause() override;
    void stop() override;
    void seek(Sample sample) override;
    void set_loop(std::optional<LoopRange> loop) override;
    Connection subscribe(std::function<void(const TransportState&)>) override;
    void advance(Sample frames); // deterministic fixture clock, not an audio engine
private:
    Timeline timeline_;
    TransportState state_;
    Signal<TransportState> changes_;
    void commit(TransportState next);
};
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual std::string_view name() const = 0;
    // A private candidate is validated before committing; rejection is atomic.
    virtual void apply(Project& candidate) const = 0;
};
class SetAraBindings final : public ICommand {
public:
    explicit SetAraBindings(std::vector<AraBinding> bindings):bindings_(std::move(bindings)){}
    std::string_view name() const override{return "ARA source bindings";}
    void apply(Project& p) const override{p.ara_bindings=bindings_;}
private:
    std::vector<AraBinding> bindings_;
};
// Track folders own presentation membership; only LinkFolderBus changes routing.
class CreateTrackFolder final : public ICommand {
public:
    CreateTrackFolder(Folder folder,std::vector<Id> tracks):folder_(std::move(folder)),tracks_(std::move(tracks)){}
    std::string_view name() const override{return "Group tracks in folder";}
    void apply(Project&) const override;
private: Folder folder_;std::vector<Id> tracks_;
};
class MoveTracksToFolder final : public ICommand {
public:
    MoveTracksToFolder(std::vector<Id> tracks,std::optional<Id> folder):tracks_(std::move(tracks)),folder_(std::move(folder)){}
    std::string_view name() const override{return "Move tracks to folder";}
    void apply(Project&) const override;
private: std::vector<Id> tracks_;std::optional<Id> folder_;
};
class UpdateTrackFolder final : public ICommand {
public:
    UpdateTrackFolder(Id id,std::string name,bool collapsed):id_(std::move(id)),name_(std::move(name)),collapsed_(collapsed){}
    std::string_view name() const override{return "Track folder settings";}
    void apply(Project&) const override;
private: Id id_;std::string name_;bool collapsed_;
};
class RemoveTrackFolder final : public ICommand {
public:
    explicit RemoveTrackFolder(Id id):id_(std::move(id)){}
    std::string_view name() const override{return "Dissolve track folder";}
    void apply(Project&) const override;
private: Id id_;
};
class LinkFolderBus final : public ICommand {
public:
    LinkFolderBus(Id folder,std::optional<Id> bus,std::optional<Track> create={}):folder_(std::move(folder)),bus_(std::move(bus)),create_(std::move(create)){}
    std::string_view name() const override{return "Link folder Bus";}
    void apply(Project&) const override;
private: Id folder_;std::optional<Id> bus_;std::optional<Track> create_;
};
class SetTrackMix final : public ICommand {
public:
    SetTrackMix(Id track, Track::Mix mix) : track_(std::move(track)), mix_(mix) {}
    std::string_view name() const override { return "Track mix"; }
    void apply(Project&) const override;
private:
    Id track_;
    Track::Mix mix_;
};
class SetMasterGain final : public ICommand {
public:
    explicit SetMasterGain(float gain) : gain_(gain) {}
    std::string_view name() const override { return "Master gain"; }
    void apply(Project& p) const override { p.master_gain = gain_; }
private:
    float gain_;
};
class SetTrackSends final : public ICommand {
public:
    SetTrackSends(Id track, std::vector<Track::Send> sends) : track_(std::move(track)), sends_(std::move(sends)) {}
    std::string_view name() const override { return "Set track sends"; }
    void apply(Project&) const override;
private:
    Id track_; std::vector<Track::Send> sends_;
};
class SetInserts final : public ICommand {
public:
    SetInserts(std::optional<Id> track, std::vector<NativeInsert> inserts) : track_(std::move(track)), inserts_(std::move(inserts)) {}
    std::string_view name() const override { return "Set insert chain"; }
    void apply(Project&) const override;
private:
    std::optional<Id> track_; std::vector<NativeInsert> inserts_;
};
class SetTrackInput final : public ICommand {
public:
    SetTrackInput(Id track, int input, bool stereo = false) : track_(std::move(track)), input_(input), stereo_(stereo) {}
    std::string_view name() const override { return "Set track input"; }
    void apply(Project&) const override;
private:
    Id track_; int input_; bool stereo_;
};
class SetTrackMonitoring final : public ICommand {
public:
    SetTrackMonitoring(Id track, bool enabled) : track_(std::move(track)), enabled_(enabled) {}
    std::string_view name() const override { return "Set track monitoring"; }
    void apply(Project&) const override;
private:
    Id track_; bool enabled_;
};
class SetMidiInput final : public ICommand {
public:
    SetMidiInput(Id track, std::string port, int channel, bool monitor)
        : track_(std::move(track)), port_(std::move(port)), channel_(channel), monitor_(monitor) {}
    std::string_view name() const override { return "Set MIDI input"; }
    void apply(Project&) const override;
private:
    Id track_; std::string port_; int channel_; bool monitor_;
};
class SetMidiOutput final : public ICommand {
public:
    SetMidiOutput(Id track,std::string port,int channel=-1):track_(std::move(track)),port_(std::move(port)),channel_(channel){}
    std::string_view name() const override {return "Set external MIDI output";}
    void apply(Project&) const override;
private: Id track_;std::string port_;int channel_;
};
class SetHardwareOutput final : public ICommand {
public:
    SetHardwareOutput(std::optional<Id> track, std::vector<int> outputs) : track_(std::move(track)), outputs_(std::move(outputs)) {}
    std::string_view name() const override { return "Set hardware output"; }
    void apply(Project&) const override;
private:
    std::optional<Id> track_; std::vector<int> outputs_;
};
class SetTrackOutput final : public ICommand {
public:
    SetTrackOutput(Id track, std::optional<Id> output) : track_(std::move(track)), output_(std::move(output)) {}
    std::string_view name() const override { return "Channel output"; }
    void apply(Project&) const override;
private:
    Id track_;
    std::optional<Id> output_;
};
class AddTrack final : public ICommand {
public:
    explicit AddTrack(Track track);
    std::string_view name() const override;
    void apply(Project&) const override;
private:
    Track track_;
};
class RenameTrack final : public ICommand {
public:
    RenameTrack(Id track, std::string name);
    std::string_view name() const override;
    void apply(Project&) const override;
private:
    Id track_;
    std::string name_;
};
struct ProjectState {
    std::shared_ptr<const Project> project;
    std::uint64_t revision{};
    bool can_undo{};
    bool can_redo{};
    std::string command;
};
// Control-thread two-phase edit: preallocate, commit silently, publish only after
// dependent runtime starts. Destruction rolls back an unfinished transaction.
class IProjectTransaction {
public:
    virtual ~IProjectTransaction()=default;
    virtual void commit() noexcept=0;
    virtual void finish()=0;
    virtual void rollback() noexcept=0;
};
class IProjectStore {
public:
    virtual ~IProjectStore() = default;
    virtual ProjectState state() const = 0;
    virtual void execute(const ICommand&) = 0;
    virtual std::unique_ptr<IProjectTransaction> prepare_transaction(const ICommand*,std::optional<bool> = {}) {return {}; }
    virtual bool undo() = 0;
    virtual bool redo() = 0;
    virtual std::shared_ptr<const Project> history_target(bool) const { return {}; }
    virtual std::string_view history_name(bool) const {return {};}
    virtual Connection subscribe(std::function<void(const ProjectState&)>) = 0;
};
class ProjectStore final : public IProjectStore {
public:
    explicit ProjectStore(Project project, std::size_t history_limit = 128);
    ProjectState state() const override;
    void execute(const ICommand&) override;
    std::unique_ptr<IProjectTransaction> prepare_transaction(const ICommand*,std::optional<bool> = {}) override;
    bool undo() override;
    bool redo() override;
    std::shared_ptr<const Project> history_target(bool redo) const override;
    std::string_view history_name(bool redo) const override;
    Connection subscribe(std::function<void(const ProjectState&)>) override;
private:
    class Transaction;
    struct Edit {
        std::shared_ptr<const Project> before;
        std::shared_ptr<const Project> after;
        std::string name;
    };
    std::shared_ptr<const Project> project_;
    std::vector<Edit> undo_;
    std::vector<Edit> redo_;
    std::uint64_t revision_{};
    std::size_t history_limit_;
    std::string command_;
    Signal<ProjectState> changes_;
    bool editing_{};
    void publish();
};
// Versioned Stage 0 text snapshot, NOT a final DAW project package.
// Reject unknown versions/extra data instead of silently discarding it.
std::string serialize(const Project&);
Project deserialize(std::string_view);
Project demo_project();
struct Services {
    std::shared_ptr<IProjectStore> projects;
    std::shared_ptr<ITransport> transport;
};
Services demo_services();
} // namespace mrs
