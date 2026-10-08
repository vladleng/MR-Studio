#pragma once
#include <mrs/delay.hpp>
#include <mrs/metronome.hpp>
#include <mrs/midi_playback.hpp>
#include <mrs/core.hpp>
#include <mrs/processing.hpp>
#include <mrs/profiling.hpp>
#include <array>
#include <bitset>
#include <atomic>
#include <filesystem>
#include <span>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // intentional cache-line padding of SPSC indices
#endif

namespace mrs::processing { class PreparedGraph; }

namespace mrs::audio {
inline constexpr std::size_t max_channels = 64;
inline constexpr std::size_t max_voices = 128;
inline constexpr std::size_t max_mixer_tracks = 128;
inline constexpr std::size_t no_mixer_track = max_mixer_tracks;
struct MixerUpdate {
    std::array<Track::Mix,max_mixer_tracks> tracks{};
    std::size_t count{};
    float master_gain{1};
    std::array<std::array<float,8>,max_mixer_tracks> send_gains{};
    std::array<bool,max_mixer_tracks> input_monitoring{};
    std::uint64_t serial{};
};
struct StereoPeak { float left{}, right{}; };
struct MixerMeters {
    std::array<StereoPeak,max_mixer_tracks> tracks{};
    StereoPeak master{};
};
struct WavFile {
    std::filesystem::path path;
    std::uint32_t sample_rate{}, channels{}, bits{}, format{};
    std::uint64_t data_offset{};
    Sample frame_count{};
    // Worker/control only: bounded block decode, independent file handle.
    void read(Sample first, std::span<float> interleaved) const;
};
WavFile inspect_wav(const std::filesystem::path&);
CabIr load_cab_ir(const std::filesystem::path&);
class ReadAhead;
class Recorder;
class ChannelWorkers;
class AheadRenderer;
class MixedRenderer;
struct AudioData {
    std::uint32_t sample_rate{48000};
    std::uint32_t channels{2};
    std::vector<float> samples; // interleaved, immutable after preload
    std::shared_ptr<const WavFile> file{}; // long sources retain metadata only
    Sample frames() const;
    void validate() const;
};
struct PlaybackRoute { std::uint32_t source_channel{}, output_channel{}; float gain{1}; };
class MidiRecorder;
struct Voice {
    std::shared_ptr<const AudioData> asset;
    Sample start{}, source_offset{}, length{};
    std::vector<PlaybackRoute> routes;
    std::shared_ptr<ReadAhead> stream{}; // per-voice cursor, prepared off RT
    std::size_t mixer_track{no_mixer_track};
};
struct MonitorRoute { std::uint32_t input_channel{}, output_channel{}; float gain{1}; std::size_t mixer_track{no_mixer_track}; };
struct SendRoute { std::size_t destination{}; float gain{1}; bool pre_fader{}; };
struct RenderGraph {
    std::vector<Voice> voices;
    std::vector<MonitorRoute> monitor;
    std::shared_ptr<processing::PreparedGraph> processors{};
    std::shared_ptr<Recorder> recording{};
    bool monitoring{true};
    std::vector<Track::Mix> mixer{};
    float master_gain{1};
    std::size_t monitor_track{no_mixer_track};
    std::vector<std::size_t> outputs{}; // channel destinations, no_mixer_track -> master
    std::vector<std::vector<std::size_t>> hardware_outputs{}; // explicit routes bypass master processor/gain
    std::vector<std::size_t> master_outputs{}; // empty preserves legacy interleaved renderer
    std::vector<std::vector<SendRoute>> sends{};
    std::vector<std::shared_ptr<Recorder>> recordings{};
    struct MidiCapture {std::size_t track{};std::shared_ptr<MidiRecorder> recorder;};
    std::vector<MidiCapture> midi_recordings{};
    std::vector<bool> midi_monitor{};
    std::vector<std::vector<PlaybackMidiEvent>> midi_events{};
    std::vector<bool> input_monitoring{};
    std::vector<std::shared_ptr<processing::PreparedGraph>> inserts{};
    std::shared_ptr<processing::PreparedGraph> master_inserts{};
    struct TempoSegment{Sample sample{};double bpm{120},quarter{};};
    std::vector<TempoSegment> tempos{};
    std::vector<bool> buses{}; // same indices as mixer; no clips/input directly on buses
    std::vector<bool> live_midi{}; // reserves device ownership even with monitoring disabled
    std::vector<std::vector<PlaybackMidiNote>> midi_notes{}; // sample projection of immutable tick data
    ClickSettings click{};
    TimeMap click_time{};
    Sample count_frames{};
    double count_beat_frames{};
    int count_beats{4};
};
struct RenderConfig {
    std::uint32_t sample_rate{48000};
    std::uint32_t input_channels{};
    std::uint32_t output_channels{2};
    std::uint32_t max_block{8192};
    std::uint32_t processing_block{}; // preferred host chunk; zero uses max_block
    std::uint32_t processing_workers{1}; // total participants including callback; 1 = serial reference
    bool worker_mmcss{true}; // Windows Pro Audio scheduling, no affinity
    std::uint32_t worker_wait_ms{20}; // bounded scheduler watchdog, separate from buffer deadline
    bool adaptive_parallel{true}; // skip dispatch if measured savings do not cover calibrated wake cost
};
// Prepared P3 candidate ownership; runtime may retain additional device nodes
// (in particular Master), and requires supported capabilities and worker budget.
// Mutable plugin/PDC instances belong to exactly one domain. Never use this
// snapshot after external scheduling capabilities change without revalidation.
struct ProcessingDomains {
    enum class Owner { ahead, device };
    enum Reason : std::uint8_t { live_input = 1, unsupported_processor = 2 };
    struct Channel { Owner owner{Owner::ahead}; std::uint8_t reasons{}; };
    enum class MergeKind { main, send, master, hardware, legacy, master_output };
    struct Merge {
        std::size_t source{}, destination{no_mixer_track}, send_index{};
        MergeKind kind{};
        std::uint64_t compensation_frames{};
    };
    std::vector<Channel> channels;
    Channel master;
    std::vector<Merge> merges; // ahead -> device edges, including physical outputs
    bool raw_capture{}; // capture belongs to the device, never the producer
    std::uint64_t monitoring_compensation_frames{}; // excludes driver latency
};
struct EngineProfile {
    bool enabled{};
    std::size_t channels{};
    std::array<TimingSample,max_mixer_tracks> device{},ahead{};
    std::array<TimingSample,8> device_workers{},ahead_workers{};
    TimingSample device_master{},ahead_master{},device_path{},ahead_path{};
};
struct Metrics {
    std::uint64_t callbacks{}, input_overflows{}, input_underflows{};
    std::uint64_t output_underflows{}, output_overflows{}, deadline_misses{};
    std::uint64_t invalid_blocks{}, clipped_samples{}, missing_inputs{};
    std::uint64_t disk_underruns{}, disk_errors{};
    std::uint64_t max_callback_ns{}, measured_callbacks{};
    std::uint32_t min_frames{}, max_frames{};
    double p50_load_percent{}, p95_load_percent{}, p99_load_percent{};
    float input_peak{};
    std::uint32_t processing_workers{1}, audio_scheduled_workers{};
    std::uint64_t parallel_batches{}, worker_timeouts{};
    std::uint64_t scheduler_overhead_ns{};
    bool processing_fault{}; // silence until stopped/reprepared; never concurrent serial retry
    bool anticipation_active{};
    bool mixed_anticipation{},monitoring_available{};
    std::uint64_t monitoring_compensation_frames{};
    std::uint32_t process_buffer_frames{}, ahead_buffered_frames{};
    std::uint64_t ahead_underruns{}, ahead_invalidations{}, ahead_max_process_ns{};
    std::uint64_t ahead_memory_bytes{}; // bounded packet/mailbox/journal storage, excludes DSP/assets
};
// One producer (control thread), one consumer (audio thread), fixed storage.
template<class T, std::size_t Capacity> class SpscQueue {
    static_assert(Capacity > 1);
    std::array<T, Capacity> values_{};
    alignas(64) std::atomic<std::size_t> write_{};
    alignas(64) std::atomic<std::size_t> read_{};
public:
    bool push(const T& value) noexcept {
        const auto write = write_.load(std::memory_order_relaxed);
        const auto next = (write + 1) % Capacity;
        if (next == read_.load(std::memory_order_acquire)) return false;
        values_[write] = value;
        write_.store(next, std::memory_order_release);
        return true;
    }
    bool pop(T& value) noexcept {
        const auto read = read_.load(std::memory_order_relaxed);
        if (read == write_.load(std::memory_order_acquire)) return false;
        value = values_[read];
        read_.store((read + 1) % Capacity, std::memory_order_release);
        return true;
    }
};
enum class ControlKind { play, pause, stop, seek, loop, monitor, prepared_seek, click };
struct Control { ControlKind kind{}; Sample a{}, b{}; std::uint64_t serial{}; };
struct RealtimeState {
    PlaybackState playback{PlaybackState::stopped};
    Sample sample{};
    std::optional<LoopRange> loop;
    Sample play_start{};
    Sample count_remaining{};
};
class AudioEngine {
public:
    struct LiveMidi {std::uint64_t generation{};std::size_t track{};processing::MidiEvent event;std::uint64_t timestamp_ns{};std::uint32_t audition_release_ms{};};
    // Single non-RT MIDI bridge producer, device callback consumer. No graph access here.
    bool enqueue_live_midi(const LiveMidi& event) noexcept {if(live_midi_queue_.push(event))return true;++midi_dropped_;midi_panic_=true;midi_record_fault_=true;return false;}
    // Message-thread producer only; independent of the driver bridge SPSC. Audition is not recorded.
    bool enqueue_audition_midi(const LiveMidi& event) noexcept {if(audition_midi_queue_.push(event))return true;++midi_dropped_;midi_panic_=true;return false;}
    void midi_panic() noexcept {midi_panic_=true;}
    void midi_overflow() noexcept {++midi_dropped_;midi_panic_=true;midi_record_fault_=true;}
    void midi_input_lost() noexcept {midi_panic_=true;midi_record_fault_=true;}
    std::uint64_t midi_generation() const noexcept {return midi_generation_.load();}
    std::uint64_t midi_dropped() const noexcept {return midi_dropped_.load();}
    AudioEngine();
    ~AudioEngine();
    // Control thread, ONLY while callback/device is stopped; graph/assets stay
    // alive until callback is stopped again. No RT ownership/deallocation.
    // Optional quiescent state retains a stopped/paused position and loop; never autoplay.
    void prepare(RenderConfig, RenderGraph, RealtimeState initial = {});
    void quiesce() noexcept; // device stopped, before plugin state/editor access
    bool enqueue(Control) noexcept;
    bool enqueue_mix(const MixerUpdate&) noexcept;
    MixerMeters take_meters() noexcept; // one UI consumer; peak hold since previous read
    void prime_streams(Sample); // control thread, before publishing a seek/play
    void prime_loop(std::optional<LoopRange>); // control thread
    // RT entry: supplied interleaved buffers have frames * configured channels.
    // No locks/allocations/I/O/listeners/ProjectStore calls in this function.
    void process(const float* input, float* output, std::uint32_t frames, std::uint32_t input_flags = 0) noexcept;
    // Backend passes measured callback body duration, on the same audio thread.
    void observe(std::uint64_t duration_ns, std::uint32_t frames, std::uint32_t flags) noexcept;
    RealtimeState state() const; // control-thread bounded coherent mailbox read
    bool try_state(RealtimeState&) const noexcept;
    bool anticipation_safe() const noexcept;
    std::uint64_t control_revision() const noexcept;
    Metrics metrics() const;
    void set_profiling(bool) noexcept; // UI/control thread; counters reset only on stopped prepare
    EngineProfile profile() const noexcept;
    std::vector<std::shared_ptr<processing::PreparedGraph>> profile_graphs() const; // UI only, channel order + legacy + Master
    RenderConfig config() const { return config_; }
    struct CompensationReport {std::vector<std::uint64_t> track_paths;std::uint64_t output{},master{};std::size_t memory_bytes{};};
    const CompensationReport& compensation() const {return compensation_;} // prepared, control thread
    const ProcessingDomains& processing_domains() const {return domains_;} // candidate plan, control thread
private:
    void process_block(const float*,float*,std::uint32_t,std::uint32_t) noexcept;
    Metronome metronome_;
    ClickSettings click_;
    bool count_started_{};
    std::uint64_t record_start_ns_{};
    std::atomic<Sample> published_count_{};
    SpscQueue<LiveMidi,1024> live_midi_queue_;
    SpscQueue<LiveMidi,128> audition_midi_queue_;
    struct AuditionTail {processing::MidiEvent off;std::uint32_t remaining{},fade{};bool active{},pending{};};
    // Callback prepares each track's tail; its channel worker alone advances it, then joins.
    std::array<AuditionTail,max_mixer_tracks> audition_tails_{};
    std::array<std::uint32_t,max_mixer_tracks> audition_live_busy_{};
    std::array<std::bitset<2048>,max_mixer_tracks> audition_live_notes_{};
    std::array<std::bitset<32>,max_mixer_tracks> audition_live_sustain_{};
    std::vector<processing::MidiBuffer> live_midi_buffers_; // allocated only by quiescent prepare
    std::vector<processing::MidiBuffer> chunk_midi_buffers_;
    std::vector<MidiPlayback> midi_playback_;
    std::atomic<std::uint64_t> midi_generation_{},midi_dropped_{};
    std::atomic<bool> midi_panic_{},midi_record_fault_{};
    friend class AheadRenderer;
    friend class MixedRenderer;
    MixedRenderer* mixed_owner_{}; // immutable while device runs
    std::array<bool,max_mixer_tracks> domain_owned_{};
    std::array<std::size_t,max_mixer_tracks> domain_edges_{};
    const float* domain_input_{};
    float* domain_output_{};
    std::uint32_t domain_stride_{}, job_base_{};
    std::array<StereoPeak,max_mixer_tracks> domain_meters_{};
    void consume_controls() noexcept;
    bool owns_voice(const Voice& v) const noexcept {return v.mixer_track==no_mixer_track||domain_owned_[v.mixer_track];}
    bool speculative_{}; // changed only while both device and producer are stopped
    AheadRenderer* ahead_owner_{};
    void apply_control(const Control&) noexcept;
    RealtimeState last_render_start_{};
    MixerMeters last_render_meters_{};
    struct MixHead {
        std::array<std::array<float,2>,max_mixer_tracks> gain{},target{},step{};
        std::array<std::array<float,8>,max_mixer_tracks> send{},send_target{},send_step{};
        std::array<float,max_mixer_tracks> gate{},gate_target{},gate_step{};
        float master{1},master_target{1},master_step{};
        std::uint32_t ramp{};
    };
    MixHead last_render_mix_{};
    MixHead mix_head() const noexcept;
    void restore_mix(const MixHead&) noexcept;
    std::atomic<std::uint64_t> control_revision_{};
    std::atomic<bool> anticipation_active_{};
    std::atomic<bool> mixed_anticipation_{};
    std::atomic<std::uint32_t> process_buffer_frames_{}, ahead_buffered_frames_{};
    std::atomic<std::uint64_t> ahead_underruns_{},ahead_invalidations_{},ahead_max_process_ns_{};
    std::atomic<std::uint64_t> ahead_memory_bytes_{};
    void publish_delivered(const RealtimeState&,const MixerMeters&) noexcept;
    void rebase_head(const RealtimeState&) noexcept;
    RenderConfig config_;
    RenderGraph graph_;
    std::unique_ptr<ChannelWorkers> workers_; // joined before graph/plugin storage is replaced
    std::vector<std::vector<std::size_t>> channel_levels_;
    std::vector<std::size_t> mix_levels_;
    std::size_t job_level_{};
    std::uint32_t job_frames_{};
    std::uint32_t job_ramp_{};
    Sample job_position_{};
    bool job_playing_{};
    double job_tempo_{120}, job_quarter_{};
    static void channel_job(void*, std::uint32_t) noexcept;
    void process_channel(std::size_t) noexcept;
    std::vector<std::vector<float>> route_contributions_;
    std::vector<std::vector<std::vector<float>>> send_contributions_;
    std::array<std::array<float,2>,max_mixer_tracks> channel_peaks_{};
    std::array<std::uint64_t,max_mixer_tracks> channel_cost_ns_{};
    std::uint64_t scheduler_overhead_ns_{};
    bool measure_channels_{}; // immutable until previous workers are joined
    struct ProfileStorage {
        std::atomic<bool> enabled{};
        std::array<TimingCounter,max_mixer_tracks> device,ahead;
        std::array<TimingCounter,8> device_workers,ahead_workers;
        TimingCounter device_master,ahead_master,device_path,ahead_path;
    };
    std::shared_ptr<ProfileStorage> profile_;
    bool profile_block_{}; // fixed for this callback and its joined jobs
    std::array<std::uint64_t,max_mixer_tracks> profile_duration_{},profile_path_{};
    std::atomic<bool> processing_fault_{};
    std::atomic<std::uint64_t> parallel_batches_{}, worker_timeouts_{};
    CompensationReport compensation_;
    ProcessingDomains domains_;
    ProcessingDomains compile_domains(const RenderGraph&, const std::vector<std::size_t>&,
        const CompensationReport&, const std::vector<std::uint64_t>&) const;
    std::vector<CompensationDelay> route_delays_;
    std::vector<std::vector<CompensationDelay>> send_delays_;
    CompensationDelay legacy_delay_;
    void reset_compensation() noexcept;
    bool monitor_enabled_{true};
    bool pending_play_anchor_{};
    std::array<bool,max_mixer_tracks> input_monitoring_{};
    std::atomic<Sample> published_play_start_{};
    std::atomic<float> input_peak_{};
    SpscQueue<Control, 64> controls_;
    SpscQueue<MixerUpdate,8> mixer_controls_;
    std::vector<std::vector<float>> track_block_;
    std::uint32_t insert_block_size_{64};
    std::array<std::array<float,2>,max_mixer_tracks> mix_gain_{}, mix_target_{}, mix_step_{};
    std::array<std::array<float,8>,max_mixer_tracks> send_gain_{}, send_target_{}, send_step_{};
    std::array<float,max_mixer_tracks> gate_{}, gate_target_{}, gate_step_{};
    float master_gain_{1}, master_target_{1}, master_step_{};
    std::vector<float> master_envelope_;
    std::vector<float> direct_output_; // prepared, never resized in callback
    std::uint32_t mix_ramp_{};
    std::vector<std::size_t> mix_order_; // source before destination, prepared off RT
    std::array<std::array<std::atomic<float>,2>,max_mixer_tracks+1> meter_peaks_{};
    void set_mix(const MixerUpdate&, bool ramp) noexcept;
    RealtimeState rt_;
    std::optional<Sample> pending_seek_; // RT-owned, coalesces prepared UI seeks
    std::atomic<std::uint64_t> sequence_{};
    std::atomic<Sample> published_sample_{}, published_loop_start_{}, published_loop_end_{};
    std::atomic<int> published_playback_{};
    std::atomic<std::uint64_t> callbacks_{}, input_overflows_{}, input_underflows_{};
    std::atomic<std::uint64_t> output_underflows_{}, output_overflows_{}, deadlines_{};
    std::atomic<std::uint64_t> invalid_blocks_{}, clipped_{}, missing_inputs_{};
    std::atomic<std::uint64_t> disk_underruns_{};
    std::atomic<std::uint64_t> max_ns_{}, measured_{};
    std::atomic<std::uint32_t> min_frames_{}, max_frames_{};
    std::array<std::atomic<std::uint64_t>, 101> load_histogram_{};
    void publish() noexcept;
};
class EngineTransport final : public ITransport {
public:
    EngineTransport(std::shared_ptr<AudioEngine>, Timeline);
    TransportState state() const override;
    void rebind_timeline(Timeline) override;
    void play() override;
    void pause() override;
    void stop() override;
    void seek(Sample sample) override;
    void set_loop(std::optional<LoopRange> loop) override;
    Connection subscribe(std::function<void(const TransportState&)>) override;
    void poll(); // control/UI pump only; audio keeps running if it stalls
private:
    std::shared_ptr<AudioEngine> engine_;
    Timeline timeline_;
    TransportState last_;
    Signal<TransportState> changes_;
    void send(Control);
};
AudioData load_wav(const std::filesystem::path&, std::size_t max_decoded_bytes = 256 * 1024 * 1024);
AudioData open_wav(const std::filesystem::path&, std::size_t preload_bytes = 8 * 1024 * 1024);
AudioData sine_fixture(std::uint32_t rate, std::uint32_t channels, Sample frames, double frequency);
} // namespace mrs::audio
#ifdef _MSC_VER
#pragma warning(pop)
#endif
