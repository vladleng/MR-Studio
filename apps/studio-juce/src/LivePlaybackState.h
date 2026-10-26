#pragma once
#include "LiveShowState.h"
#include <cmath>

namespace ui::live {
// Show-owned references/settings; decoded audio and transport belong to shared Core.
inline juce::var playback(const juce::var &state, const juce::String &item) {
    return state["performance"]["nativePlayback"][juce::Identifier(item)];
}
inline void validatePlayback(const juce::var &state) {
    const auto rows = state["performance"]["nativePlayback"];
    if (rows.isVoid())
        return;
    if (!rows.isObject())
        throw std::runtime_error("Invalid Live Playback bindings");
    for (const auto &pair : rows.getDynamicObject()->getProperties()) {
        const auto row = pair.value;
        if (!find(state, "items", pair.name.toString()).isObject() || !row.isObject() || !row["file"].isString() ||
            row["file"].toString().isEmpty() || !row["mute"].isBool() || !row["solo"].isBool())
            throw std::runtime_error("Invalid Live Playback item binding");
        const auto gain = row["gain"];
        if (!(gain.isDouble() || gain.isInt()) || !std::isfinite(static_cast<double>(gain)) ||
            static_cast<double>(gain) < -60. || static_cast<double>(gain) > 12.)
            throw std::runtime_error("Playback gain must be -60..12 dB");
        const auto start = row["trimStart"], end = row["trimEnd"];
        if ((!start.isVoid() &&
             (!number(start) || (static_cast<double>(start) < 0. || static_cast<double>(start) > 1.e9))) ||
            (!end.isVoid() && (!number(end) || (static_cast<double>(end) <= static_cast<double>(start) ||
                                                static_cast<double>(end) > 1.e9))))
            throw std::runtime_error("Invalid Playback trim range");
    }
}
inline void playbackPaths(juce::var &state, const juce::File &show, bool reading, bool savedVersions = true) {
    auto rows = state["performance"]["nativePlayback"];
    if (rows.isObject())
        for (const auto &pair : rows.getDynamicObject()->getProperties()) {
            const auto resolved = show.getParentDirectory().getChildFile(pair.value["file"].toString());
            auto row = pair.value;
            put(row, "file",
                reading ? resolved.getFullPathName() : resolved.getRelativePathFrom(show.getParentDirectory()));
        }
    // Saved item versions must use the same coordinate system as working references.
    if (!savedVersions)
        return;
    const auto library = state["library"];
    if (library.isObject())
        for (const auto &pair : library.getDynamicObject()->getProperties()) {
            if (!pair.value.isString())
                continue;
            auto saved = juce::JSON::parse(pair.value.toString());
            if (!saved["performance"]["nativePlayback"].isObject())
                continue;
            if (reading)
                validatePlayback(saved);
            playbackPaths(saved, show, reading, false);
            put(map(state, "library"), pair.name, juce::JSON::toString(saved, true, 17));
        }
}
} // namespace ui::live
