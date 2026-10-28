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
    auto channel = find(state, "channels", playbackChannelId(state, row));
    const auto primary = find(state, "channels", playbackChannelId(state, juce::var()));
    const bool cue = static_cast<bool>(row["cue"]);
    const bool groupChild = !cue && channel["id"] != primary["id"];
    // The primary strip is now the musical group. A Cue promoted to the
    // primary source still has its own source level, not the group fader.
    if (cue && channel["id"] == primary["id"]) channel = juce::var();
    const double parentGain = groupChild ? static_cast<double>(primary["gain"]) : 0.;
    mrs::Track::Mix mix;
    const double group = cue ? 0. : playbackGain(state) + itemPlaybackGain(state, id);
    mix.gain = static_cast<float>(std::pow(10., (static_cast<double>(row["gain"]) + group +
                                                static_cast<double>(channel["gain"]) + parentGain) / 20.));
    mix.pan = static_cast<float>(juce::jlimit(-1., 1., channelPan(channel) + (groupChild ? channelPan(primary) : 0.)));
    mix.mute = static_cast<bool>(row["mute"]) || static_cast<bool>(channel["mute"]) || (groupChild && static_cast<bool>(primary["mute"]));
    mix.solo = static_cast<bool>(row["solo"]) || static_cast<bool>(channel["solo"]) || (groupChild && static_cast<bool>(primary["solo"]));
    return mix;
}
inline juce::String playbackMixKey(const juce::var &state, const juce::String &id) {
    return canonical(playback(state, id)) + canonical(liveAudio(state)) +
           juce::String(itemPlaybackGain(state, id), 17) + canonical(state["channels"]) + canonical(state["performance"]["clips"]);
}
// Browser regions are in bars, keyed by the item track ID. Native-only
// sources without a browser track retain the same rows on their source binding.
inline juce::String playbackTrackId(const juce::var &state,const juce::String &item,const juce::var &row) {
    const auto tracks=find(state,"items",item)["tracks"];
    if(const auto* values=tracks.getArray())for(const auto& track:*values)
        if(track["channelId"].toString()==playbackChannelId(state,row))return track["id"].toString();
    return {};
}
inline juce::var playbackRegions(const juce::var &state,const juce::String &item,const juce::var &row) {
    const auto track=playbackTrackId(state,item,row);
    return track.isEmpty()?row["regions"]:state["performance"]["clips"][juce::Identifier(track)];
}
inline void validatePlaybackRegions(const juce::var &rows) {
    if(rows.isVoid())return;
    if(!rows.isArray() || rows.size()>128)throw std::runtime_error("Playback supports up to 128 regions");
    std::set<juce::String> ids;
    for(const auto& row:*rows.getArray()){
        if(!named(row["id"])||!ids.insert(row["id"].toString()).second||!number(row["start"])||!number(row["length"])||!number(row["offset"])||
            static_cast<double>(row["start"])<0.||static_cast<double>(row["length"])<.25||
            static_cast<double>(row["start"])+static_cast<double>(row["length"])>4096.+1.e-7||static_cast<double>(row["offset"])<0. || static_cast<double>(row["offset"])+static_cast<double>(row["length"])>1.e9)
            throw std::runtime_error("Invalid Playback region");
    }
}
inline void setPlaybackRegions(juce::var &state,const juce::String &item,const juce::String &stem,const juce::var &rows) {
    validatePlaybackRegions(rows);
    auto row=playbackStem(state,item,stem);
    if(!row.isObject())throw std::runtime_error("Playback source was removed");
    const auto track=playbackTrackId(state,item,row);
    if(track.isEmpty())put(row,"regions",rows.clone());
    else put(map(performance(state),"clips"),track,rows.clone());
}
inline double playbackSecondsPerBar(const juce::var &state,const juce::String &item) {
    const auto source=find(state,"items",item);const auto meter=source["meter"].toString();
    return 60./static_cast<double>(source["tempo"])*4.*meter.upToFirstOccurrenceOf("/",false,false).getDoubleValue()/
        meter.fromFirstOccurrenceOf("/",false,false).getDoubleValue();
}
inline juce::var defaultPlaybackRegions(const juce::var &row,double sourceBars,double secondsPerBar) {
    const double start=static_cast<double>(row["trimStart"])/secondsPerBar;
    const double end=row["trimEnd"].isVoid()?sourceBars:static_cast<double>(row["trimEnd"])/secondsPerBar;
    auto region=object();put(region,"id","source");put(region,"start",start);put(region,"length",end-start);put(region,"offset",start);
    return juce::Array<juce::var>{region};
}
inline double clipSnap(double value){return std::round(value*4.)/4.;}
inline juce::var editPlaybackRegion(const juce::var &rows,const juce::String &id,double delta,int edge,double sourceBars) {
    auto result=rows.clone();bool found=false;
    for(auto& row:*result.getArray())if(row["id"].toString()==id){
        found=true;const double start=row["start"],length=row["length"],offset=row["offset"];
        if(!edge)put(row,"start",juce::jlimit(0.,4096.-length,clipSnap(start+delta)));
        if(edge<0){const double change=juce::jlimit((std::max)(-start,-offset),length-.25,clipSnap(start+delta)-start);
            put(row,"start",start+change);put(row,"offset",offset+change);put(row,"length",length-change);}
        if(edge>0)put(row,"length",juce::jlimit(.25,(std::min)(sourceBars-offset,4096.-start),clipSnap(start+length+delta)-start));
    }
    if(!found)throw std::runtime_error("Playback region was removed");
    validatePlaybackRegions(result);return result;
}
inline juce::var splitPlaybackRegion(const juce::var &rows,const juce::String &id,double at) {
    auto result=rows.clone();bool found=false;
    for(int i=0;i<result.size();++i)if(result[i]["id"].toString()==id){
        auto left=result[i].clone(),right=left.clone();const double start=left["start"],length=left["length"];
        at=clipSnap(at);const double first=at-start;
        if(first<.25 || length-first<.25)throw std::runtime_error("Cut must leave at least 1/4 bar on each side");
        put(left,"length",first);put(right,"id",freshId());put(right,"start",at);put(right,"length",length-first);
        put(right,"offset",static_cast<double>(left["offset"])+first);
        result.getArray()->set(i,left);result.getArray()->insert(i+1,right);found=true;break;
    }
    if(!found)throw std::runtime_error("Playback region was removed");
    validatePlaybackRegions(result);return result;
}
inline juce::String playbackRegionsKey(const juce::var& state,const juce::String& item){
    juce::Array<juce::var> rows;
    for(const auto& row:playbackRows(state,item))rows.add(juce::Array<juce::var>{row["trimStart"],row["trimEnd"],playbackRegions(state,item,row)});
    return canonical(rows)+juce::String(playbackSecondsPerBar(state,item),17);
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
        for (const auto &row : playbackRows(state, pair.name.toString())) { playbackMix(state, pair.name.toString(), row).validate(); validatePlaybackRegions(playbackRegions(state,pair.name.toString(),row)); }
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
struct PlaybackLane {
    juce::String channel;
    bool container{}, child{};
};
inline juce::String playbackContainer(const juce::var &state) {
    if (const auto *channels = state["channels"].getArray())
        for (const auto &channel : *channels)
            if (channel["type"].toString() == "Playback") return channel["id"].toString();
    return {};
}
inline bool playbackExpanded(const juce::var &state) {
    return static_cast<bool>(state["performance"]["stemsExpanded"]);
}
inline std::vector<PlaybackLane> playbackLanes(const juce::var &state) {
    std::vector<PlaybackLane> result;
    const auto primary = playbackContainer(state);
    std::set<juce::String> children;
    bool primarySource = false;
    if (const auto *items = state["items"].getArray())
        for (const auto &item : *items) {
            const auto rows = playbackRows(state, item["id"].toString());
            primarySource = primarySource || !rows.empty();
            for (std::size_t i = 1; i < rows.size(); ++i) children.insert(rows[i]["id"].toString());
        }
    if (const auto *channels = state["channels"].getArray())
        for (const auto &channel : *channels) {
            const auto id = channel["id"].toString();
            if (children.contains(id)) continue;
            result.push_back({id, id == primary, false});
            if (id != primary || !playbackExpanded(state)) continue;
            if (primarySource) result.push_back({primary, false, true});
            for (const auto &child : *channels)
                if (children.contains(child["id"].toString())) result.push_back({child["id"].toString(), false, true});
        }
    return result;
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
