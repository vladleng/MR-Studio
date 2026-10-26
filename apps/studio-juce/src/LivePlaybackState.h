#pragma once
#include "LiveShowState.h"
#include <cmath>

namespace ui::live {
// Show-owned references/settings; decoded audio and transport belong to shared Core.
inline juce::var playback(const juce::var &state, const juce::String &item) {
    return state["performance"]["nativePlayback"][juce::Identifier(item)];
}
inline std::vector<juce::var> playbackRows(const juce::var &state, const juce::String &item) {
    const auto row = playback(state, item);
    if (!row.isObject())
        return {};
    std::vector<juce::var> rows{row};
    if (const auto *stems = row["stems"].getArray())
        for (const auto &stem : *stems)
            rows.push_back(stem);
    return rows;
}
inline juce::var playbackStem(const juce::var &state, const juce::String &item, const juce::String &stem) {
    const auto row = playback(state, item);
    if (stem.isEmpty())
        return row;
    if (const auto *stems = row["stems"].getArray())
        for (const auto &entry : *stems)
            if (entry["id"].toString() == stem)
                return entry;
    return {};
}
inline juce::String playbackMediaKey(const juce::var &state, const juce::String &item) {
    juce::Array<juce::var> files;
    for (const auto &row : playbackRows(state, item)) {
        files.add(row["id"]);
        files.add(row["file"]);
    }
    return canonical(files);
}
inline std::vector<int> playbackOutputs(const juce::var &row) {
    std::vector<int> result;
    if (const auto *outputs = row["outputs"].getArray())
        for (const auto &output : *outputs)
            result.push_back(static_cast<int>(output));
    return result;
}
inline void validatePlaybackRow(const juce::var &row) {
    if (!row.isObject() || !row["file"].isString() || row["file"].toString().isEmpty() || !row["mute"].isBool() ||
        !row["solo"].isBool())
        throw std::runtime_error("Invalid Live Playback binding");
    const auto gain = row["gain"];
    if (!number(gain) || static_cast<double>(gain) < -60. || static_cast<double>(gain) > 12.)
        throw std::runtime_error("Playback gain must be -60..12 dB");
    const auto start = row["trimStart"], end = row["trimEnd"];
    if ((!start.isVoid() && (!number(start) || static_cast<double>(start) < 0. || static_cast<double>(start) > 1.e9)) ||
        (!end.isVoid() &&
         (!number(end) || static_cast<double>(end) <= static_cast<double>(start) || static_cast<double>(end) > 1.e9)))
        throw std::runtime_error("Invalid Playback trim range");
    const auto outputs = row["outputs"];
    if (!outputs.isVoid()) {
        if (!outputs.isArray() || outputs.size() > 2)
            throw std::runtime_error("Playback needs mono/stereo outputs or Master");
        std::set<int> used;
        for (const auto &output : *outputs.getArray())
            if (!output.isInt() || static_cast<int>(output) < 0 || static_cast<int>(output) >= 128 ||
                !used.insert(static_cast<int>(output)).second)
                throw std::runtime_error("Invalid Playback output route");
    }
}
inline void validatePlayback(const juce::var &state) {
    const auto bindings = state["performance"]["nativePlayback"];
    if (bindings.isVoid())
        return;
    if (!bindings.isObject())
        throw std::runtime_error("Invalid Live Playback bindings");
    for (const auto &pair : bindings.getDynamicObject()->getProperties()) {
        if (!find(state, "items", pair.name.toString()).isObject())
            throw std::runtime_error("Missing Playback item");
        validatePlaybackRow(pair.value);
        const auto stems = pair.value["stems"];
        if (!stems.isVoid()) {
            if (!stems.isArray() || stems.size() > 31)
                throw std::runtime_error("Playback supports up to 32 stems");
            std::set<juce::String> ids;
            for (const auto &stem : *stems.getArray()) {
                validatePlaybackRow(stem);
                const auto id = stem["id"].toString();
                if (!named(stem["id"]) || !ids.insert(id).second ||
                    find(state, "channels", id)["type"].toString() != "Playback")
                    throw std::runtime_error("Invalid Playback stem channel");
            }
        }
    }
}
inline void playbackPaths(juce::var &state, const juce::File &show, bool reading, bool savedVersions = true) {
    auto rows = state["performance"]["nativePlayback"];
    if (rows.isObject())
        for (const auto &pair : rows.getDynamicObject()->getProperties()) {
            for (auto row : playbackRows(state, pair.name.toString())) {
                const auto resolved = show.getParentDirectory().getChildFile(row["file"].toString());
                put(row, "file",
                    reading ? resolved.getFullPathName() : resolved.getRelativePathFrom(show.getParentDirectory()));
            }
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
