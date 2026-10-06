#pragma once
#include <mrs/core.hpp>
#include <array>
#include <cstddef>
#include <span>

namespace mrs::processing {
inline constexpr std::size_t event_capacity = 256;
inline constexpr std::size_t max_nodes = 32;
enum class MidiKind { note_off, note_on, cc, program, pressure, pitch_bend, poly_pressure };
struct MidiEvent {
    std::uint32_t offset{}; // sample offset within the NEXT callback block
    MidiKind kind{MidiKind::note_on};
    std::uint8_t channel{}, data1{}, data2{}; // channel 0..15, MIDI data 0..127
    bool operator==(const MidiEvent&) const = default;
    void validate() const;
};
struct MidiBuffer {
    std::array<MidiEvent,event_capacity> events{};
    std::size_t size{};
    bool push(MidiEvent) noexcept;
    std::span<const MidiEvent> view() const { return {events.data(),size}; }
    void clear() noexcept { size = 0; }
};
struct MidiPort {
    Id id;
    std::string name;
    bool input{}, output{};
};
class IMidiDevice {
public:
    virtual ~IMidiDevice() = default;
    virtual std::vector<MidiPort> ports() const = 0;
    virtual void open(const Id&) = 0;
    virtual void close() noexcept = 0;
    virtual bool connected() const = 0;
    virtual bool receive(MidiEvent&) = 0; // control-thread pump; never an audio callback syscall
    virtual void send(MidiEvent) = 0;
};
class MockMidiDevice final : public IMidiDevice {
public:
    std::vector<MidiPort> ports() const override;
    void open(const Id&) override;
    void close() noexcept override;
    bool connected() const override { return open_; }
    bool receive(MidiEvent&) override;
    void send(MidiEvent) override;
    void inject(MidiEvent);
    const std::vector<MidiEvent>& sent() const { return sent_; }
private:
    bool open_{};
    std::deque<MidiEvent> input_;
    std::vector<MidiEvent> sent_;
};
struct MidiRoute {
    Id source_port, target_node;
    int input_channel{-1}; // -1 all channels
    int output_channel{-1}; // -1 preserve
    int transpose{}; // note numbers only
    bool operator==(const MidiRoute&) const = default;
};
struct ParameterInfo {
    std::uint32_t id{};
    float minimum{}, maximum{1}, initial{};
    bool automatable{true};
    std::string name{};
    bool hidden{};
};
struct ParameterValue {
    std::uint32_t id{};
    float value{};
    bool operator==(const ParameterValue&) const = default;
};
struct ParameterChange {
    std::uint32_t offset{}, id{};
    float value{};
};
enum class ProcessorFormat { native, vst3 };
struct PluginState {
    std::string class_id; // VST3 FUID: exactly 32 hexadecimal characters
    std::vector<std::byte> component, controller; // opaque, off-thread ownership
    bool operator==(const PluginState&) const = default;
};
struct NodeState {
    Id id;
    std::string processor_id;
    ProcessorFormat format{ProcessorFormat::native};
    PluginState plugin;
    std::vector<ParameterValue> parameters;
    bool bypass{};
    bool operator==(const NodeState&) const = default;
};
struct AudioEdge {
    std::optional<Id> from; // null = engine's mixed playback + monitor input
    Id to;
    float gain{1};
    bool operator==(const AudioEdge&) const = default;
};
struct GraphState {
    Id id;
    std::string patch_name;
    std::vector<NodeState> nodes;
    std::vector<AudioEdge> edges;
    std::vector<Id> outputs; // unity-summed; empty graph is transparent
    std::vector<MidiRoute> midi_routes;
    void validate() const;
    bool operator==(const GraphState&) const = default;
};
struct GraphSnapshot {
    std::shared_ptr<const GraphState> graph;
    std::uint64_t revision{};
    bool can_undo{}, can_redo{};
};
// Application-thread state service injected once into Arrange/Mix/Live.
// Runtime graph preparation/publication is explicit and quiescent, not automatic.
class GraphStore {
public:
    explicit GraphStore(GraphState, std::size_t history_limit = 64);
    GraphSnapshot state() const;
    void replace(GraphState);
    bool undo();
    bool redo();
    Connection subscribe(std::function<void(const GraphSnapshot&)>);
private:
    std::shared_ptr<const GraphState> graph_;
    std::vector<std::shared_ptr<const GraphState>> undo_, redo_;
    std::uint64_t revision_{};
    std::size_t limit_;
    Signal<GraphSnapshot> changes_;
};
struct ProcessConfig {
    std::uint32_t sample_rate{48000}, channels{2}, max_block{8192}, live_latency_budget{128};
};
struct ProcessBlock {
    std::span<float> audio; // interleaved, in-place, frames * channels
    std::uint32_t frames{}, channels{};
    std::span<const MidiEvent> midi;
    std::span<const ParameterChange> parameters;
    MidiBuffer& midi_output;
    Sample position{};bool playing{};double tempo{120}, quarter{};
};
class IProcessor {
public:
    virtual ~IProcessor() = default;
    virtual std::vector<ParameterInfo> parameters() const = 0;
    virtual void prepare(ProcessConfig) = 0;
    virtual void restore(const PluginState&) = 0;
    virtual PluginState capture() const = 0; // quiescent/control thread only
    virtual bool set_parameter(std::uint32_t,float) noexcept = 0;
    virtual std::optional<float> parameter_value(std::uint32_t) const noexcept = 0;
    virtual std::uint32_t latency() const noexcept = 0;
    virtual bool live_safe() const noexcept = 0;
    // Opt in only when reset_anticipation clears DSP history while retaining parameter
    // targets, device-sized packet rendering is valid, and all external edits are
    // represented in PreparedGraph revisions. Unsupported processors stay direct.
    virtual bool anticipation_safe() const noexcept { return false; }
    virtual void reset_anticipation() noexcept { reset(); }
    virtual void warm() = 0; // off-thread after prepare/restore
    virtual void reset() noexcept = 0;
    virtual void process(ProcessBlock) noexcept = 0; // bounded RT implementation required
    virtual bool open_editor(void*,int&,int&) { return false; }
    virtual void close_editor() noexcept {}
    virtual bool edited() noexcept { return false; }
    virtual bool failed() const noexcept { return false; }
    virtual void sync_controller(std::uint32_t,float) {}
};
using ProcessorFactory = std::function<std::unique_ptr<IProcessor>(const NodeState&)>;
std::unique_ptr<IProcessor> native_factory(const NodeState&);
std::unique_ptr<IProcessor> cab_ir_factory();
PluginState cab_ir_state(const CabIr&);
struct NodeLatency {
    Id id;
    std::uint32_t own{};
    std::uint64_t path{};
    bool live_safe{};
};
struct LatencyReport {
    std::vector<NodeLatency> nodes;
    std::uint64_t output{};
    bool live_safe{true};
    bool parallel_paths_need_compensation{};
    bool compensation_applied{};
};
struct GraphMetrics {
    std::uint64_t dropped_midi{}, dropped_parameters{}, invalid_events{}, output_overflows{}, panics{};
};
struct MidiOutput { std::uint16_t node{}; MidiEvent event; };
class PreparedGraph {
public:
    PreparedGraph(GraphSnapshot, ProcessConfig, ProcessorFactory = native_factory);
    ~PreparedGraph();
    PreparedGraph(const PreparedGraph&) = delete;
    PreparedGraph& operator=(const PreparedGraph&) = delete;
    // Single application-thread producer; buffers refer to the next block.
    bool enqueue_midi(const Id& source_port, MidiEvent);
    bool enqueue_parameter(const Id& node, ParameterChange);
    bool enqueue_parameters(const GraphState&); // atomic bounded parameter/bypass update
    void panic() noexcept;
    void reset_anticipation() noexcept; // single quiescent DSP owner, retain parameter targets
    // Single audio-thread consumer; no allocation, locks or I/O.
    void process(float* interleaved, std::uint32_t frames, Sample position=0,bool playing=false,double tempo=120,double quarter=0) noexcept;
    bool pop_midi_output(MidiOutput&) noexcept; // single control-thread consumer
    const GraphSnapshot& snapshot() const;
    ProcessConfig config() const;
    const LatencyReport& latency() const;
    GraphMetrics metrics() const noexcept;
    GraphState capture() const; // quiescent: reads processor state
    std::uint32_t node_latency(const Id&) const;
    std::vector<ParameterInfo> parameter_infos(const Id&) const;
    bool open_editor(const Id&,void*,int&,int&);
    void close_editor(const Id&) noexcept;
    void close_editors() noexcept;
    void restore_node(const NodeState&); // quiescent: retain processor/editor instance
    bool consume_edits() noexcept;
    bool failed() const noexcept;
    bool anticipation_safe() const noexcept;
    std::uint64_t control_revision() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
double eq_response_db(const NativeInsert&, double frequency, std::uint32_t sample_rate);
GraphState insert_graph(std::span<const NativeInsert>);
GraphState demo_graph();
} // namespace mrs::processing
