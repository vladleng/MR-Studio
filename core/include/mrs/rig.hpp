#pragma once
#include <mrs/core.hpp>
namespace mrs {
struct RigPreset {
    Id id;
    NativeInsert state;
    void validate() const;
    bool operator==(const RigPreset&) const = default;
};
std::string serialize_rig_preset(const RigPreset&);
RigPreset deserialize_rig_preset(std::string_view);
std::string serialize_rig_library(const std::vector<RigPreset>&);
std::vector<RigPreset> deserialize_rig_library(std::string_view);
}
