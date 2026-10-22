#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>

namespace mrs::stretch {
enum class Status { ok, need_input, output_full, drained, invalid_parameter, invalid_buffer,
                    unsupported, not_prepared, cancelled, io_error, backend_error };
enum class Quality { balanced, economical };
enum class Formants { follow_pitch, preserve, shift };
// Serializable user intent; no vendor settings or backend identity. Standalone
// codec only in P1: adding this state to Project/Clip belongs to #61.
struct Parameters {
    bool enabled{true};
    double duration_ratio{1}; // output duration / source duration
    double semitones{}, cents{};
    Formants formants{Formants::follow_pitch};
    double formant_semitones{}, formant_base_hz{}; // 0 => documented adapter default
    Quality quality{Quality::balanced};
    bool operator==(const Parameters&) const = default;
};
Status validate(const Parameters&) noexcept;
bool neutral(const Parameters&) noexcept;
std::string serialize(const Parameters&); // control/worker only, locale-independent
Parameters deserialize(std::string_view); // rejects unsupported version/trailing data
// Absolute mapping: shared split boundaries, not independently rounded lengths.
Status output_frames(std::int64_t source_frames, double ratio, std::int64_t& result) noexcept;
class FrameDemand {
public:
    std::uint32_t peek(std::uint32_t output, double ratio) const noexcept;
    void advance(std::uint32_t output, double ratio) noexcept;
    void reset() noexcept { fraction_ = 0; }
private:
    long double fraction_{};
};
struct Config {
    std::uint32_t sample_rate{48000}, channels{2};
    std::uint32_t max_input_frames{32768}, max_output_frames{4096};
    bool split_computation{}; // runtime policy, not serialized clip state
};
struct Latency {
    std::uint32_t input_frames{}, output_frames{}, seek_context_frames{};
};
struct Capabilities {
    std::uint32_t max_channels{2};
    bool realtime{}, offline{}, formants{}, dynamic_parameters{};
};
struct Result { Status status{Status::ok}; std::uint32_t consumed{}, produced{}; };
// One processing owner; caller quiesces it before prepare/reset/seek/flush or
// destruction. No cross-thread publication/ownership is implied by this API.
class ITimeStretchEngine {
public:
    virtual ~ITimeStretchEngine() = default;
    virtual Capabilities capabilities() const noexcept = 0;
    virtual Status prepare(const Config&, const Parameters&) = 0; // may allocate
    virtual Status reset(std::uint64_t generation) = 0; // worker/control only
    virtual Status seek(std::span<const float* const> context, std::uint32_t frames,
                        std::uint64_t generation) = 0; // context ends at next input position
    virtual Status set_parameters(const Parameters&) noexcept = 0; // same quality only
    virtual Latency latency() const noexcept = 0;
    virtual std::uint64_t generation() const noexcept = 0;
    virtual std::uint32_t input_required(std::uint32_t output_frames) const noexcept = 0;
    // Separate planar channel buffers. Short capacity returns need_input/output_full
    // without advancing DSP/history. Sample buffers must not overlap each other.
    virtual Result process(std::span<const float* const> input, std::uint32_t available,
                           std::span<float* const> output, std::uint32_t frames) noexcept = 0;
    // One-shot worker/control drain into a caller-owned buffer of output latency.
    // Upstream flush resets history; subsequent process requires reset/seek.
    virtual Result flush(std::span<float* const> output, std::uint32_t capacity) = 0;
};
std::unique_ptr<ITimeStretchEngine> make_signalsmith_engine();
std::string_view backend_revision() noexcept;

// Worker-only bounded streaming render; no complete-file allocation. Read/write
// callbacks use interleaved PCM at Config::sample_rate. A false callback aborts.
using Read = std::function<bool(std::int64_t first, std::span<float> interleaved)>;
using Write = std::function<bool(std::int64_t first, std::span<const float> interleaved)>;
struct RenderResult { Status status{Status::ok}; std::int64_t frames{}; };
RenderResult render(ITimeStretchEngine&, const Config&, const Parameters&, std::int64_t source_frames,
                    const Read&, const Write&, std::stop_token = {});
} // namespace mrs::stretch
