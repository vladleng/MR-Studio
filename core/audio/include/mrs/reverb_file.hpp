#pragma once
#include <mrs/reverb.hpp>
#include <filesystem>

namespace mrs::audio {
// Worker/control only. Uses the shared bounded PCM16/24/32 / float32 decoder.
// Quad mapping must be confirmed by the caller, never inferred from channel count.
processing::ReverbImpulse load_reverb_ir(const std::filesystem::path&,
    processing::ReverbQuadMap = processing::ReverbQuadMap::unspecified,std::stop_token = {});
processing::ReverbImpulse load_reverb_pair(const std::filesystem::path& left,
    const std::filesystem::path& right,std::stop_token = {});
}
