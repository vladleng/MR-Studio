#include <mrs/device.hpp>
#include <set>
#include <stdexcept>

namespace mrs::audio {
bool supports_buffer(const DeviceInfo& device, std::uint32_t frames) {
    const auto n = static_cast<long>(frames);
    if (frames == 0 || frames > 65536 || n < device.min_buffer || n > device.max_buffer) return false;
    if (device.granularity == -1) return (frames & (frames - 1)) == 0;
    if (device.granularity == 0) return n == device.preferred_buffer;
    return device.granularity > 0 && (n - device.min_buffer) % device.granularity == 0;
}
void validate_device_config(const DeviceInfo& device, const DeviceConfig& config) {
    if (config.device != device.index || config.sample_rate < 8000 || config.sample_rate > 768000 ||
        !supports_buffer(device, config.buffer_frames) || config.outputs.empty() ||
        config.outputs.size() > max_channels || config.inputs.size() > max_channels)
        throw std::invalid_argument("invalid ASIO device/rate/buffer configuration");
    const auto validate_channels = [](const auto& selected, std::size_t total) {
        std::set<int> used;
        for (const auto channel : selected)
            if (channel < 0 || static_cast<std::size_t>(channel) >= total || !used.insert(channel).second)
                throw std::invalid_argument("invalid or duplicate ASIO channel selector");
    };
    validate_channels(config.inputs, device.inputs.size());
    validate_channels(config.outputs, device.outputs.size());
}
} // namespace mrs::audio
