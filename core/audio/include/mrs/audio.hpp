#pragma once
#include <mrs/delay.hpp>
#include <mrs/core.hpp>
#include <array>
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
struct AudioData {
    std::uint32_t sample_rate{48000};
    std::uint32_t channels{2};
    std::vector<float> samples; // interleaved, immutable after preload
    std::shared_ptr<const WavFile> file{}; // long sources retain metadata only
    Sample frames() const;
    void validate() const;
};
struct PlaybackRoute { std::uint32_t source_channel{}, output_channel{}; float gain{1}; };
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
    std::vector<bool> input_monitoring{};
    std::vector<std::shared_ptr<processing::PreparedGraph>> inserts{};
    std::shared_ptr<processing::PreparedGraph> master_inserts{};
    struct TempoSegment{Sample sample{};double bpm{120},quarter{};};
    std::vector<TempoSegment> tempos{};
    std::vector<bool> buses{}; // same indices as mixer; no clips/input directly on buses
};
struct RenderConfig {
    std::uint32_t sample_rate{48000};
    std::uint32_t input_channels{};
    std::uint32_t output_channels{2};
    std::uint32_t max_block{8192};
    std::uint32_t processing_block{}; // preferred host chunk; zero uses max_block
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
enum class ControlKind { play, pause, stop, seek, loop, monitor, prepared_seek };
struct Control { ControlKind kind{}; Sample a{}, b{}; };
struct RealtimeState {
    PlaybackState playback{PlaybackState::stopped};
    Sample sample{};
    std::optional<LoopRange> loop;
    Sample play_start{};
};
class AudioEngine {
public:
    AudioEngine();
    // Control thread, ONLY while callback/device is stopped; graph/assets stay
    // alive until callback is stopped again. No RT ownership/deallocation.
    // Optional quiescent state retains a stopped/paused position and loop; never autoplay.
    void prepare(RenderConfig, RenderGraph, RealtimeState initial = {});
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
    Metrics metrics() const;
    RenderConfig config() const { return config_; }
    struct CompensationReport {std::vector<std::uint64_t> track_paths;std::uint64_t output{},master{};std::size_t memory_bytes{};};
    const CompensationReport& compensation() const {return compensation_;} // prepared, control thread
private:
    RenderConfig config_;
    RenderGraph graph_;
    CompensationReport compensation_;
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
