#pragma once
#include <mrs/waveform.hpp>
#include <mrs/stretch.hpp>

namespace mrs::audio {
// Session cache, prepared and retired on workers/control threads only. The
// immutable source is retained, so pointer identity cannot be recycled (ABA).
struct StretchCache {
    std::shared_ptr<const AudioData> source, data;
    Waveform waveform;
    std::uintmax_t bytes{};
    std::filesystem::file_time_type source_time{};
    std::uintmax_t source_bytes{};
    bool source_current() const;
};
std::string stretch_cache_key(const AudioData&, const Clip&);
// Owns a private temporary directory; never writes beside/over the source.
// 512 MiB is the maximum for one entry and the application's total live cache.
std::shared_ptr<const StretchCache> prepare_stretch_cache(
    std::shared_ptr<const AudioData>, const Clip&, std::stop_token = {},
    std::function<void(float)> progress = {});
} // namespace mrs::audio
