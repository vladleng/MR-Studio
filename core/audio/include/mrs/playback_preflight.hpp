#pragma once
#include <mrs/audio.hpp>
#include <algorithm>
#include <stop_token>
namespace mrs::audio {
// Explicit non-RT inspection. A successful header/source check is not a claim
// that decoding, DSP preparation or physical device validation has completed.
using PlaybackFileStamp=std::pair<std::uintmax_t,std::filesystem::file_time_type>;
// Control-thread adapter for old Live regions without a saved audio timebase.
// Their unavailable source tail is silence, never an out-of-bounds Core voice.
inline std::optional<Sample> playback_region_length(Sample offset,Sample length,Sample frames,bool legacy_silent_tail=false) {
    if(offset<0||length<=0||frames<=0)return std::nullopt;
    if(legacy_silent_tail)return offset>=frames?0:(std::min)(length,frames-offset);
    if(offset>frames||length>frames-offset)return std::nullopt;
    return length;
}
struct PlaybackSourceCheck {
    struct Region {Sample offset{},length{};bool legacy_silent_tail{};};
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
    std::optional<PlaybackFileStamp> inspected_stamp;
};
std::vector<PlaybackCheckResult> check_playback_sources(std::span<const PlaybackSourceCheck>,
    std::uint32_t target_rate,std::span<const int> active_outputs,std::stop_token={});
}
