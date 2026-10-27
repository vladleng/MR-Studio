#pragma once
#include "LiveShowState.h"
#include <cmath>
#include <mrs/metronome.hpp>

namespace ui::live {
inline juce::var liveAudio(const juce::var &state) { return state["performance"]["nativeLiveAudio"]; }
inline double playbackGain(const juce::var &state) {
    const auto value = liveAudio(state)["gain"];
    return value.isVoid() ? 0. : static_cast<double>(value);
}
inline mrs::audio::ClickSettings liveClick(const juce::var &state) {
    mrs::audio::ClickSettings result;
    const auto value = liveAudio(state)["click"];
    if (!value.isObject())
        return result;
    result.playback = static_cast<bool>(value["enabled"]);
    result.accent = value["accent"].isVoid() || static_cast<bool>(value["accent"]);
    result.level = value["level"].isVoid() ? 20 : static_cast<int>(value["level"]);
    if (const auto *outputs = value["outputs"].getArray(); outputs && !outputs->isEmpty()) {
        result.output_left = static_cast<int>((*outputs)[0]);
        if (outputs->size() == 2)
            result.output_right = static_cast<int>((*outputs)[1]);
    }
    return result;
}
inline void validateLiveAudio(const juce::var &state) {
    const auto value = liveAudio(state);
    if (value.isVoid())
        return;
    if (!value.isObject())
        throw std::runtime_error("Invalid Live audio settings");
    const auto gain = value["gain"];
    if (!gain.isVoid() && (!number(gain) || static_cast<double>(gain) < -60. || static_cast<double>(gain) > 12.))
        throw std::runtime_error("Live Playback gain must be -60..12 dB");
    const auto click = value["click"];
    if (click.isVoid())
        return;
    if (!click.isObject() || !click["enabled"].isBool() || (!click["accent"].isVoid() && !click["accent"].isBool()) ||
        (!click["level"].isVoid() && !click["level"].isInt()))
        throw std::runtime_error("Invalid Live Click settings");
    const auto outputs = click["outputs"];
    if (!outputs.isVoid()) {
        if (!outputs.isArray() || outputs.size() > 2)
            throw std::runtime_error("Click needs mono/stereo outputs or Master");
        for (const auto &output : *outputs.getArray())
            if (!output.isInt() || static_cast<int>(output) < 0 || static_cast<int>(output) > 127)
                throw std::runtime_error("Invalid Click output");
    }
    liveClick(state).validate();
}
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
// The native source row owns stem gain; the common channel owns the show mix.
inline juce::String playbackChannelId(const juce::var &state, const juce::var &row) {
    if (row["id"].isString()) return row["id"].toString();
    for (const auto &channel : *state["channels"].getArray())
        if (channel["type"].toString() == "Playback") return channel["id"].toString();
    return {};
}
inline double importedStemGain(const juce::var &state, const juce::String &item, const juce::var &row) {
    const auto channel = playbackChannelId(state, row);
    const auto tracks = find(state, "items", item)["tracks"];
    if (const auto *entries = tracks.getArray()) for (const auto &track : *entries)
        if (track["channelId"].toString() == channel && number(track["playbackGain"])) return static_cast<double>(track["playbackGain"]);
    return 0.;
}
inline double itemPlaybackGain(const juce::var &state, const juce::String &id) {
    const auto value = find(state, "items", id)["playbackGroupGain"];
    return value.isVoid() ? 0. : static_cast<double>(value);
}
// Show/editor channel pan is percent (-100..100). Core uses a normalized scalar.
inline double channelPan(const juce::var &channel) {
    return static_cast<double>(channel["pan"]) / 100.;
}
inline mrs::Track::Mix playbackMix(const juce::var &state, const juce::String &id, const juce::var &row) {
    const auto channel = find(state, "channels", playbackChannelId(state, row));
    mrs::Track::Mix mix;
    const double group = static_cast<bool>(row["cue"]) ? 0. : playbackGain(state) + itemPlaybackGain(state, id);
    mix.gain = static_cast<float>(std::pow(10., (static_cast<double>(row["gain"]) + group +
                                                static_cast<double>(channel["gain"])) / 20.));
    mix.pan = static_cast<float>(channelPan(channel));
    mix.mute = static_cast<bool>(row["mute"]) || static_cast<bool>(channel["mute"]);
    mix.solo = static_cast<bool>(row["solo"]) || static_cast<bool>(channel["solo"]);
    return mix;
}
inline juce::String playbackMixKey(const juce::var &state, const juce::String &id) {
    return canonical(playback(state, id)) + canonical(liveAudio(state)) +
           juce::String(itemPlaybackGain(state, id), 17) + canonical(state["channels"]);
}
inline void validatePlaybackRow(const juce::var &row) {
    if ((!row["cue"].isVoid() && !row["cue"].isBool()) || !row.isObject() || !row["file"].isString() ||
        row["file"].toString().isEmpty() || !row["mute"].isBool() || !row["solo"].isBool())
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
    validateLiveAudio(state);
    const auto bindings = state["performance"]["nativePlayback"];
    if (bindings.isVoid())
        return;
    if (!bindings.isObject())
        throw std::runtime_error("Invalid Live Playback bindings");
    for (const auto &pair : bindings.getDynamicObject()->getProperties()) {
        if (!find(state, "items", pair.name.toString()).isObject())
            throw std::runtime_error("Missing Playback item");
        validatePlaybackRow(pair.value);
        for (const auto &row : playbackRows(state, pair.name.toString())) playbackMix(state, pair.name.toString(), row).validate();
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
inline int entryIndex(const juce::var &state, const juce::String &item, const juce::String &preferred = {}) {
    const auto entries = activeSetlist(state)["entries"];
    int first = -1;
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i]["itemId"].toString() == item) {
            if (entries[i]["id"].toString() == preferred)
                return i;
            if (first < 0)
                first = i;
        }
    return first;
}
inline juce::var neighborEntry(const juce::var &state, int direction, const juce::String &preferred = {}) {
    direction = direction < 0 ? -1 : 1;
    const auto entries = activeSetlist(state)["entries"];
    int at = entryIndex(state, state["currentItem"].toString(), preferred);
    if (at < 0)
        at = direction > 0 ? -1 : entries.size();
    for (int i = at + direction; i >= 0 && i < entries.size(); i += direction)
        if (find(state, "items", entries[i]["itemId"].toString()).isObject())
            return entries[i];
    return {};
}
inline juce::String neighborItem(const juce::var &state, int direction, const juce::String &preferred = {}) {
    return neighborEntry(state, direction, preferred)["itemId"].toString();
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
