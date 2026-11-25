#pragma once
#include <mrs/core.hpp>

namespace ui {
// Compact only redundant display levels. Keep the real hierarchy, IDs, and assignments intact.
inline bool compactDrumCategory(const mrs::DrumMapCategory& category) {
    if (category.instruments.size() != 1) return false;
    const auto key = [](std::string_view value) {
        std::string result;
        for (auto ch : value) {
            if (ch == ' ' || ch == '-' || ch == '\t' || ch == '_') continue;
            result += ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch;
        }
        return result == "hh" ? std::string{"hihat"} : result == "sd" ? std::string{"snare"} : result;
    };
    const auto group = key(category.name);
    return !group.empty() && group != "toms" && group != "cymbals" && group == key(category.instruments.front().name);
}
}
