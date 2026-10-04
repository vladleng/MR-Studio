#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace mrs {
using Sample = std::int64_t;
using Tick = std::int64_t;
inline constexpr Tick ppq = 960;
inline constexpr Tick max_tick = 1'000'000'000'000;
inline constexpr Sample max_sample = 4'503'599'627'370'496;
inline constexpr std::uint32_t schema_version = 9;
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
enum class TrackKind { audio, midi, bus };
enum class MarkerKind { generic, cue, warning, lyric, action, navigation };
struct Folder {
    Id id;
    std::string name;
    std::optional<Id> parent;
    bool operator==(const Folder&) const = default;
};
enum class InsertKind { gain, highpass, lowpass, eq, channel_eq, vst3 };
struct EqBand {
    float frequency{1000}, gain{}, q{0.70710678f};
    bool enabled{true};
    bool operator==(const EqBand&) const = default;
};
struct InsertParameter {
    std::uint32_t id{}; float value{};
    bool operator==(const InsertParameter&) const = default;
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
    void validate() const;
    bool operator==(const NativeInsert&) const = default;
};
struct Track {
    Id id;
    std::string name;
    TrackKind kind{TrackKind::audio};
    std::optional<Id> folder;
    struct Mix {
        float gain{1}, pan{};
        bool mute{}, solo{};
        bool operator==(const Mix&) const = default;
        void validate() const;
    } mix{};
    std::optional<Id> output{}; // no destination means master; otherwise a shared bus
    struct Send {
        Id bus;
        float gain{1};
        bool pre_fader{};
        bool operator==(const Send&) const = default;
    };
    std::vector<Send> sends{}; // up to eight independent sends to shared buses/returns
    int input{-2}; // audio only: -2 default device input, -1 off, otherwise physical index
    bool input_stereo{};
    bool input_monitor{};
    std::vector<int> hardware_outputs{}; // mono or stereo physical channels; exclusive with output bus
    std::vector<NativeInsert> inserts{}; // pre-fader; at most eight, 32 total in project
    bool operator==(const Track&) const = default;
};
struct Clip {
    Id id;
    Id track;
    std::string name;
    Sample start{};
    Sample length{1};
    Sample source_offset{};
    std::string source; // opaque asset reference, Stage 0 does not open it
    bool operator==(const Clip&) const = default;
};
struct Marker {
    Id id;
    std::string name;
    Tick tick{};
    MarkerKind kind{MarkerKind::generic};
    bool operator==(const Marker&) const = default;
};
// Single non-overlapping lanes. Ranges are [start, end); gaps mean no chord/section.
struct Chord {
    Id id;
    std::string symbol; // authored spelling, no harmonic inference
    Tick start{};
    Tick end{1};
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
struct Project {
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
    float master_gain{1};
    std::vector<int> master_outputs{}; // empty = first selected mono/stereo hardware outputs
    std::vector<NativeInsert> master_inserts{};
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
class IProjectStore {
public:
    virtual ~IProjectStore() = default;
    virtual ProjectState state() const = 0;
    virtual void execute(const ICommand&) = 0;
    virtual bool undo() = 0;
    virtual bool redo() = 0;
    virtual std::shared_ptr<const Project> history_target(bool) const { return {}; }
    virtual Connection subscribe(std::function<void(const ProjectState&)>) = 0;
};
class ProjectStore final : public IProjectStore {
public:
    explicit ProjectStore(Project project, std::size_t history_limit = 128);
    ProjectState state() const override;
    void execute(const ICommand&) override;
    bool undo() override;
    bool redo() override;
    std::shared_ptr<const Project> history_target(bool redo) const override;
    Connection subscribe(std::function<void(const ProjectState&)>) override;
private:
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
