#pragma once
#include <mrs/audio.hpp>
#include <stop_token>
namespace mrs::audio {
// Explicit non-RT inspection. A successful header/source check is not a claim
// that decoding, DSP preparation or physical device validation has completed.
using PlaybackFileStamp=std::pair<std::uintmax_t,std::filesystem::file_time_type>;
struct PlaybackSourceCheck {
    struct Region {Sample offset{},length{};};
    std::string key;
    std::filesystem::path file;
    double begin{},end{-1};
    std::vector<int> outputs;
    std::optional<std::vector<Region>> regions;
    std::optional<PlaybackFileStamp> expected;
};
enum class PlaybackCheckCode {ok,missing,changed,unreadable,trim,regions,outputs};
struct PlaybackCheckResult {
    std::string key,detail;
    PlaybackCheckCode code{PlaybackCheckCode::ok};
    std::uint32_t source_rate{};
    Sample frames{};
    bool resample{};
};
std::vector<PlaybackCheckResult> check_playback_sources(std::span<const PlaybackSourceCheck>,
    std::uint32_t target_rate,std::span<const int> active_outputs,std::stop_token={});
}
