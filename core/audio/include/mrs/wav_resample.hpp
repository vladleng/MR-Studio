#pragma once
#include <functional>
#include <filesystem>
#include <stop_token>
#include <cstdint>

namespace mrs::audio {
// Worker/control only. Bounded, antialiased SRC to a NEW float32 WAV.
// Duration/pitch are retained; no tempo mapping, normalisation or source writes.
// Destination must not exist. Partial output is removed on failure/cancellation.
void resample_wav_file(const std::filesystem::path& source,
                       const std::filesystem::path& destination,
                       std::uint32_t output_rate,std::stop_token stop={},
                       std::function<void(float)> progress={});
}
