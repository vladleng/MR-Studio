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
std::string stretch_cache_key(const AudioData&, const Clip&, stretch::Backend = stretch::Backend::signalsmith);
// Owns a private temporary directory; never writes beside/over the source.
// 512 MiB is the maximum for one entry. Active project PCM is streamed from
// disk; only unused/warm entries have a fixed aggregate eviction budget.
// Preflight pending writes against available disk space, not a RAM budget.
void validate_stretch_disk_space(std::uintmax_t pending_bytes,std::uintmax_t available_bytes);
std::shared_ptr<const StretchCache> prepare_stretch_cache(
    std::shared_ptr<const AudioData>, const Clip&, std::stop_token = {},
    std::function<void(float)> progress = {}, stretch::Backend = stretch::Backend::signalsmith);
} // namespace mrs::audio
