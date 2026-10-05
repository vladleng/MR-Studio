#pragma once
#include <mrs/audio.hpp>

namespace mrs::audio {
enum class DevicePhase { closed, open, running, stopped, error };
struct DeviceInfo {
    int index{-1}; // session-local; re-enumerate after reconnect
    std::string name;
    std::vector<std::string> inputs, outputs;
    long min_buffer{}, max_buffer{}, preferred_buffer{}, granularity{};
};
struct DeviceConfig {
    int device{-1};
    std::uint32_t sample_rate{48000}, buffer_frames{128};
    std::vector<int> inputs; // zero-based hardware channel selectors
    std::vector<int> outputs{0, 1};
    std::uint32_t processing_workers{2}; // callback + helpers, 1 serial, maximum 8
};
struct DeviceStatus {
    DevicePhase phase{DevicePhase::closed};
    double sample_rate{}, input_latency_ms{}, output_latency_ms{}, cpu_load{};
    std::string error;
};
bool supports_buffer(const DeviceInfo&, std::uint32_t frames);
void validate_device_config(const DeviceInfo&, const DeviceConfig&);
class IAudioDevice {
public:
    virtual ~IAudioDevice() = default;
    virtual std::vector<DeviceInfo> enumerate() = 0;
    virtual void control_panel(int device) = 0;
    virtual void open(const DeviceConfig&, std::shared_ptr<AudioEngine>) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void close() noexcept = 0;
    virtual DeviceStatus status() = 0;
};
// Windows builds only, ASIO-only PortAudio host; no implicit WASAPI fallback.
std::unique_ptr<IAudioDevice> make_asio_device();
} // namespace mrs::audio
