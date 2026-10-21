#pragma once
#include <mrs/sample_library.hpp>
#include <array>
#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
namespace mrs::library {
struct WavPreview {
    std::filesystem::path path;
    std::uint32_t rate{}, channels{}, bits{};
    std::uint64_t frames{};
    std::array<float,256> peaks{}; // display-only, bounded waveform
};
// Resolve ONLY an indexed source-relative WAV; never accept arbitrary names,
// traversal, reparse paths, missing/invalid catalogue entries.
std::optional<std::filesystem::path> asset_path(const Snapshot&,std::int64_t asset_id);
// Non-realtime WAV analysis, cancellable per block. Does not mutate original.
WavPreview build_wav_preview(const std::filesystem::path&,
                             const std::shared_ptr<std::atomic<bool>>& cancel);
} // namespace mrs::library
