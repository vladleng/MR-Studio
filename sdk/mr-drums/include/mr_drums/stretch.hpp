#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <stop_token>
#include <vector>

namespace mr::drums {
inline constexpr const char* version = "0.1.0-experimental";
enum class Status { ok, invalid_argument, resource_limit, cancelled };
struct Config {
    std::uint32_t sample_rate{48000}, channels{2};
    double duration_ratio{1.0}; // output/input; BPM conversion = source BPM / target BPM
    double grain_ms{24}, search_ms{4}, attack_ms{5};
    bool detect_transients{true};
    // Includes result PCM and internal anchors, excludes caller-owned source PCM.
    std::size_t memory_budget_bytes{256u * 1024u * 1024u};
};
struct Result {
    Status status{Status::ok};
    std::vector<float> samples; // interleaved; empty on any failure
    std::vector<std::size_t> transients; // source-frame positions used
    std::size_t frames{};
};
// Offline/worker only: allocates and performs an entire bounded clip render.
// Independent implementation: NOT zplane/elastique, NOT realtime validated.
// Optional explicit transient markers replace automatic detection; sorted, unique,
// interior source-frame positions only. All channels share the same mapping.
Result render(std::span<const float> source, const Config&,
              std::span<const std::size_t> transient_markers = {},
              std::stop_token stop = {});
} // namespace mr::drums
