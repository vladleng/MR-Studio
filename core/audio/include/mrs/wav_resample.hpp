#pragma once
#include <mrs/audio.hpp>
#include <functional>
#include <filesystem>
#include <stop_token>
#include <cstdint>

namespace mrs::audio {
struct WavExportResult {
    bool completed{};
    Sample frames{};
    float peak{}; // pre-quantization peak at output rate
    std::uint64_t clipped_samples{};
};
// Separate render and conversion progress, both synchronous non-RT callbacks.
// Existing destinations are never replaced. Cancellation returns incomplete.
struct WavExportCallbacks {
    std::function<bool(const OfflineRenderProgress&)> continue_render{};
    std::function<bool(float)> continue_conversion{};
    std::stop_token stop{};
};
using MasterRenderSource=std::function<OfflineRenderResult(const OfflineRenderCallbacks&)>;
WavExportResult export_wav24(const std::filesystem::path&,std::uint32_t source_rate,
                             Sample source_frames,const MasterRenderSource&,
                             const WavExportCallbacks& = {});
// Worker/control only. Bounded, antialiased SRC to a NEW float32 WAV.
// Duration/pitch are retained; no tempo mapping, normalisation or source writes.
// Destination must not exist. Partial output is removed on failure/cancellation.
void resample_wav_file(const std::filesystem::path& source,
                       const std::filesystem::path& destination,
                       std::uint32_t output_rate,std::stop_token stop={},
                       std::function<void(float)> progress={});
}
