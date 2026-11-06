#pragma once
#include "LiveShowState.h"
#include "LivePerformanceState.h"
#include <cmath>
#include <map>
#include <mrs/metronome.hpp>

namespace ui::live {
inline bool clickBlockedAfterDrag(std::optional<juce::uint32> started,juce::uint32 now){
    return started&&static_cast<juce::uint32>(now-*started)<250u;
}

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
// The first Playback was historically the show-wide root. Once channels
// become movable, persist that *identity* independently of display order.
// Old shows still resolve to their first Playback until their first reorder.
inline juce::String primaryPlaybackId(const juce::var& state) {
    const auto pinned=state["performance"]["primaryPlaybackChannel"].toString();
    if(const auto* channels=state["channels"].getArray()) {
        if(pinned.isNotEmpty())
            for(const auto& channel:*channels)
                if(channel["id"].toString()==pinned && channel["type"].toString()=="Playback")
                    return pinned;
        for(const auto& channel:*channels)
            if(channel["type"].toString()=="Playback")return channel["id"].toString();
    }
    return {};
}
// The native source row owns stem gain; the common channel owns the show mix.
inline juce::String playbackChannelId(const juce::var &state, const juce::var &row) {
    if (row["id"].isString()) return row["id"].toString();
    return primaryPlaybackId(state);
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
// A fader preview must not clone a show that can contain many megabytes of
// opaque VST3 state. Only one show-channel's four scalar values are overridden
// for this transient Core mix computation; nothing is persisted or Undo-ed.
struct PlaybackMixOverride {
    juce::String channel;
    double gain{}, pan{};
    bool mute{}, solo{};
};
// Playback / Stems gestures override only the audible scalar mix, never
// show-owned bindings or the saved group/stem levels before mouse release.
struct PlaybackLevelOverride {
    bool group{};
    double gain{};
    bool mute{}, solo{};
};
inline mrs::Track::Mix playbackMix(const juce::var &state, const juce::String &id,
                                   const juce::var &row, const PlaybackMixOverride* preview=nullptr,
                                   const PlaybackLevelOverride* levelOverride=nullptr) {
    auto channel = find(state, "channels", playbackChannelId(state, row));
    const auto primary = find(state, "channels", playbackChannelId(state, juce::var()));
    const bool cue = static_cast<bool>(row["cue"]);
    // User-created Playback lanes are independent of the default Backing group.
    // Old stem channels without an explicit parent still inherit that group.
    const auto selectedParent = find(state, "channels", channel["playbackParent"].toString());
    const auto parent = selectedParent.isObject() && selectedParent["type"].toString() == "Playback"
        ? selectedParent : primary;
    const bool rootOnOtherChannel = row["id"].isString() &&
        row["id"] == playback(state, id)["id"];
    const bool groupChild = !cue && !static_cast<bool>(channel["playbackIndependent"]) &&
        !rootOnOtherChannel && channel["id"] != parent["id"];
    // A Cue promoted to the primary source is independent of the group.
    if (cue && channel["id"] == primary["id"]) channel = juce::var();
    const auto gainFor=[&](const juce::var& value)->double {
        return preview && value["id"].toString()==preview->channel ?
            preview->gain : static_cast<double>(value["gain"]);
    };
    const auto panFor=[&](const juce::var& value)->double {
        return preview && value["id"].toString()==preview->channel ?
            preview->pan : channelPan(value);
    };
    const auto muteFor=[&](const juce::var& value)->bool {
        return preview && value["id"].toString()==preview->channel ?
            preview->mute : static_cast<bool>(value["mute"]);
    };
    const auto soloFor=[&](const juce::var& value)->bool {
        return preview && value["id"].toString()==preview->channel ?
            preview->solo : static_cast<bool>(value["solo"]);
    };
    const double parentGain=groupChild?gainFor(parent):0.;
    mrs::Track::Mix mix;
    const double itemLevel=levelOverride&&levelOverride->group?
        levelOverride->gain:itemPlaybackGain(state,id);
    const double stemLevel=levelOverride&&!levelOverride->group?
        levelOverride->gain:static_cast<double>(row["gain"]);
    const bool stemMute=levelOverride&&!levelOverride->group?
        levelOverride->mute:static_cast<bool>(row["mute"]);
    const bool stemSolo=levelOverride&&!levelOverride->group?
        levelOverride->solo:static_cast<bool>(row["solo"]);
    const double group=cue?0.:playbackGain(state)+itemLevel;
    mix.gain=static_cast<float>(std::pow(10.,(stemLevel+group+
                                              gainFor(channel)+parentGain)/20.));
    mix.pan=static_cast<float>(juce::jlimit(-1.,1.,panFor(channel)+(groupChild?panFor(parent):0.)));
    mix.mute=stemMute||muteFor(channel)||(groupChild&&muteFor(parent));
    mix.solo=stemSolo||soloFor(channel)||(groupChild&&soloFor(parent));
    return mix;
}
// The show channel may contain multi-megabyte Base64 VST3 component/controller
// state. Playback's mix depends only on the four scalar mixer values and channel
// identity, never on a Live Instrument's binary state, input, FX or patches.
// Do NOT put entire channels into a key used by the 30 Hz UI/poll callback.
inline juce::var playbackChannelMixSnapshot(const juce::var& state) {
    juce::Array<juce::var> channels;
    if (const auto* entries=state["channels"].getArray())
        for (const auto& channel:*entries) {
            auto mix=object();
            for(const auto* key:{"id","type","gain","pan","mute","solo"})
                put(mix,key,channel[key]);
            channels.add(std::move(mix));
        }
    return juce::var(std::move(channels));
}
inline juce::String playbackMixKey(const juce::var &state, const juce::String &id) {
    return canonical(playback(state, id)) + canonical(liveAudio(state)) +
           juce::String(itemPlaybackGain(state, id), 17) + canonical(playbackChannelMixSnapshot(state)) +
           canonical(state["performance"]["clips"]);
}
// Browser regions are in bars, keyed by the item track ID. Native-only
// sources without a browser track retain the same rows on their source binding.
inline juce::String playbackTrackId(const juce::var &state,const juce::String &item,const juce::var &row) {
    const auto tracks=find(state,"items",item)["tracks"];
    if(const auto* values=tracks.getArray())for(const auto& track:*values)
        if(track["channelId"].toString()==playbackChannelId(state,row))return track["id"].toString();
    return {};
}
inline double playbackSecondsPerBar(const juce::var &state,const juce::String &item) {
    const auto source=find(state,"items",item);const auto meter=source["meter"].toString();
    return 60./static_cast<double>(source["tempo"])*4.*meter.upToFirstOccurrenceOf("/",false,false).getDoubleValue()/
        meter.fromFirstOccurrenceOf("/",false,false).getDoubleValue();
}
inline juce::var storedPlaybackRegions(const juce::var &state,const juce::String &item,const juce::var &row) {
    const auto track=playbackTrackId(state,item,row);
    return track.isEmpty()?row["regions"]:state["performance"]["clips"][juce::Identifier(track)];
}
// Audio extents keep their own clock. Bars returned to the editor are a view
// of the same source seconds at the current musical tempo, not time stretching.
inline juce::var playbackRegions(const juce::var &state,const juce::String &item,const juce::var &row) {
    const auto stored=storedPlaybackRegions(state,item,row);
    if(!stored.isArray())return stored;
    const double seconds=playbackSecondsPerBar(state,item);
    bool changed=false;
    for(const auto& region:*stored.getArray())if(region["audioSecondsPerBar"].isVoid()||static_cast<double>(region["audioSecondsPerBar"])!=seconds)changed=true;
    if(!changed)return stored;
    auto result=stored.clone();
    for(auto& region:*result.getArray()){
        if(region["audioSecondsPerBar"].isVoid())put(region,"legacySourceBounds",true);
        else {
            const double scale=static_cast<double>(region["audioSecondsPerBar"])/seconds;
            for(const auto* field:{"start","offset","length"})put(region,field,static_cast<double>(region[field])*scale);
        }
        put(region,"audioSecondsPerBar",seconds);
    }
    return result;
}
inline void validatePlaybackRegions(const juce::var &rows) {
    if(rows.isVoid())return;
    if(!rows.isArray() || rows.size()>128)throw std::runtime_error("Playback supports up to 128 regions");
    std::set<juce::String> ids;
    for(const auto& row:*rows.getArray()){
        if(!named(row["id"])||!ids.insert(row["id"].toString()).second||!number(row["start"])||!number(row["length"])||!number(row["offset"])||
            static_cast<double>(row["start"])<0.||static_cast<double>(row["length"])<=0.||
            static_cast<double>(row["start"])+static_cast<double>(row["length"])>4096.+1.e-7||static_cast<double>(row["offset"])<0. || static_cast<double>(row["offset"])+static_cast<double>(row["length"])>1.e9)
            throw std::runtime_error("Invalid Playback region");
        const auto clock=row["audioSecondsPerBar"],legacy=row["legacySourceBounds"];
        if((!clock.isVoid()&&(!number(clock)||static_cast<double>(clock)<=0.||static_cast<double>(clock)>1.e6))||
           (!legacy.isVoid()&&!legacy.isBool()))throw std::runtime_error("Invalid Playback audio timebase");
    }
}
inline void setPlaybackRegions(juce::var &state,const juce::String &item,const juce::String &stem,const juce::var &rows) {
    validatePlaybackRegions(rows);
    auto row=playbackStem(state,item,stem);
    if(!row.isObject())throw std::runtime_error("Playback source was removed");
    const auto track=playbackTrackId(state,item,row);
    auto saved=rows.clone();
    for(auto& region:*saved.getArray())put(region,"audioSecondsPerBar",playbackSecondsPerBar(state,item));
    if(track.isEmpty())put(row,"regions",saved);
    else put(map(performance(state),"clips"),track,saved);
}
inline void pinLegacyPlaybackTimebase(juce::var& state,const juce::String& item,double seconds) {
    for(const auto& row:playbackRows(state,item)){
        const auto stored=storedPlaybackRegions(state,item,row);
        if(!stored.isArray())continue;
        // state is a detached edit/import candidate; never mutate its input.
        for(auto& region:*stored.getArray())if(region["audioSecondsPerBar"].isVoid()){
            put(region,"audioSecondsPerBar",seconds);put(region,"legacySourceBounds",true);
        }
    }
}
inline void preservePlaybackTimebases(const juce::var& before,juce::var& after) {
    for(const auto& item:*after["items"].getArray()){
        const auto id=item["id"].toString();if(!find(before,"items",id).isObject())continue;
        const auto seconds=playbackSecondsPerBar(before,id);
        if(seconds!=playbackSecondsPerBar(after,id))pinLegacyPlaybackTimebase(after,id,seconds);
    }
}
inline void resetPlaybackRegions(juce::var &state,const juce::String &item,juce::var row) {
    const auto track=playbackTrackId(state,item,row);
    if(track.isNotEmpty())map(performance(state),"clips").getDynamicObject()->removeProperty(juce::Identifier(track));
    row.getDynamicObject()->removeProperty("regions");
}
// One WAV/stem source is bound to one persistent show Playback channel.
// The legacy root (without an id) remains bound to the first Playback channel.
// User-created channels can own their own source without reallocating old stems.
inline bool playbackChannelHasSource(const juce::var &state, const juce::String &item,
                                    const juce::String &channel) {
    if (channel.isEmpty()) return false;
    for (const auto &row : playbackRows(state,item))
        if (playbackChannelId(state,row)==channel) return true;
    return false;
}
// Which mixer Playback group owns this WAV/stem. Old stems inherit the first
// group; manually created Playback channels remain independent.
inline juce::String playbackGroupForRow(const juce::var &state,const juce::String &item,
                                        const juce::var &row) {
    const auto id=playbackChannelId(state,row);
    const auto primary=playbackChannelId(state,juce::var());
    if(id==primary)return primary;
    const auto channel=find(state,"channels",id);
    if(static_cast<bool>(channel["playbackIndependent"]) ||
       (row["id"].isString() && row["id"]==playback(state,item)["id"]))
        return id;
    const auto parent=channel["playbackParent"].toString();
    return find(state,"channels",parent)["type"].toString()=="Playback"?parent:primary;
}
// Map a selected stem back to its owning group. Legacy stems inherit Backing.
inline juce::String playbackGroupForChannel(const juce::var &state,const juce::String &item,
                                            const juce::String &channel) {
    const auto primary=playbackChannelId(state,juce::var());
    if(channel.isEmpty())return primary;
    for(const auto &row:playbackRows(state,item))
        if(playbackChannelId(state,row)==channel)
            return playbackGroupForRow(state,item,row);
    const auto target=find(state,"channels",channel);
    const auto parent=target["playbackParent"].toString();
    if(find(state,"channels",parent)["type"].toString()=="Playback" && parent!=channel)
        return parent;
    // A pre-existing *empty* second Playback channel has no
    // playbackIndependent marker yet. Treat it as its own selectable lane,
    // not as a child of the first Backing. Preserve legacy populated stems
    // that were actually used as children in other items.
    if(!static_cast<bool>(target["playbackIndependent"])) {
        if(const auto* items=state["items"].getArray())
            for(const auto& other:*items) {
                const auto otherId=other["id"].toString();
                for(const auto& row:playbackRows(state,otherId))
                    if(playbackChannelId(state,row)==channel) {
                        const auto group=playbackGroupForRow(state,otherId,row);
                        if(group!=channel)return group;
                    }
            }
    }
    return channel;
}
// An empty old Playback channel becomes an independent group on first WAV
// attachment. This is a targeted metadata repair and does not relabel old
// child stems already in use; no new persistence format is required.
inline void ensureIndependentPlaybackTarget(juce::var &state,const juce::String &item,
                                            const juce::String &channel) {
    const auto primary=playbackChannelId(state,juce::var());
    if(channel.isEmpty() || channel==primary ||
       playbackGroupForChannel(state,item,channel)!=channel)return;
    auto target=find(state,"channels",channel);
    if(target["type"].toString()=="Playback" && target["playbackParent"].isVoid() &&
       !static_cast<bool>(target["playbackIndependent"]))
        put(target,"playbackIndependent",true);
}
// Only the UI is filtered: Core continues preparing the whole item in one batch.
inline std::vector<juce::var> playbackGroupRows(const juce::var &state,const juce::String &item,
                                                const juce::String &group) {
    std::vector<juce::var> result;
    for(const auto &row:playbackRows(state,item))
        if(playbackGroupForRow(state,item,row)==group)
            result.push_back(row);
    return result;
}
inline juce::String playbackStemForChannel(const juce::var &state,const juce::String &item,
                                          const juce::String &channel) {
    if (channel.isEmpty()) return {};
    const auto rows=playbackRows(state,item);
    for (std::size_t i=0;i<rows.size();++i)
        if (playbackChannelId(state,rows[i])==channel)
            return i==0?juce::String():rows[i]["id"].toString();
    return {};
}
inline void bindPlaybackSource(juce::var &state,const juce::String &item,
                               const juce::String &channel,const juce::String &file,
                               bool replaceExisting) {
    if (find(state,"channels",channel)["type"].toString()!="Playback")
        throw std::runtime_error("Choose a Playback channel");
    const auto primary=playbackChannelId(state,juce::var());
    auto root=playback(state,item);
    const bool hasRoot=root.isObject() && root["file"].isString();
    const auto oldRootChannel=hasRoot?playbackChannelId(state,root):juce::String();
    auto stems=hasRoot?root["stems"].clone():juce::var();
    if(!stems.isArray())stems=juce::Array<juce::var>();
    bool isRoot=hasRoot && oldRootChannel==channel;
    int stemAt=-1;
    if(!isRoot)for(int i=0;i<stems.size();++i)
        if(stems[i]["id"].toString()==channel){stemAt=i;break;}
    if ((isRoot || stemAt>=0) && !replaceExisting)
        throw std::runtime_error("Playback channel already contains WAV");
    juce::var row=isRoot?root:(stemAt>=0?stems[stemAt]:object());
    if(!isRoot && stemAt<0) {
        if(channel!=primary)put(row,"id",channel);
        put(row,"gain",importedStemGain(state,item,row));
        put(row,"mute",false);put(row,"solo",false);
    }
    // A second lane may have been populated first. Promote the first/default
    // channel to the legacy root only when its own WAV is subsequently loaded.
    // Keep the former root, regions, mix and file as an ordinary stem.
    if(hasRoot && channel==primary && oldRootChannel!=primary) {
        auto previous=root.clone();
        previous.getDynamicObject()->removeProperty("stems");
        if(stemAt>=0)stems.getArray()->remove(stemAt);
        if(stems.size()>=31)throw std::runtime_error("Playback supports up to 32 stems");
        stems.getArray()->insert(0,previous);
        isRoot=true;
        row.getDynamicObject()->removeProperty("id");
    } else if(!hasRoot) {
        isRoot=true;
    } else if(!isRoot && stemAt<0 && stems.size()>=31) {
        throw std::runtime_error("Playback supports up to 32 stems");
    }
    row.getDynamicObject()->removeProperty("trimStart");
    row.getDynamicObject()->removeProperty("trimEnd");
    put(row,"file",file);
    resetPlaybackRegions(state,item,row);
    if(isRoot) {
        if(stems.size()>0)put(row,"stems",stems);
        else row.getDynamicObject()->removeProperty("stems");
        put(map(performance(state),"nativePlayback"),item,row);
    } else {
        if(stemAt>=0)stems.getArray()->set(stemAt,row);
        else stems.getArray()->add(row);
        put(root,"stems",stems);
        put(map(performance(state),"nativePlayback"),item,root);
    }
}
inline juce::var defaultPlaybackRegions(const juce::var &row,double sourceBars,double secondsPerBar) {
    const double start=static_cast<double>(row["trimStart"])/secondsPerBar;
    const double end=row["trimEnd"].isVoid()?sourceBars:static_cast<double>(row["trimEnd"])/secondsPerBar;
    auto region=object();put(region,"id","source");put(region,"start",start);put(region,"length",end-start);put(region,"offset",start);
    put(region,"audioSecondsPerBar",secondsPerBar);
    return juce::Array<juce::var>{region};
}
inline double clipSnap(double value){return std::round(value*4.)/4.;}
inline juce::var editPlaybackRegion(const juce::var &rows,const juce::String &id,double delta,int edge,double sourceBars) {
    auto result=rows.clone();bool found=false;
    for(auto& row:*result.getArray())if(row["id"].toString()==id){
        found=true;const double start=row["start"],length=row["length"],offset=row["offset"];
        if(!edge)put(row,"start",juce::jlimit(0.,4096.-length,clipSnap(start+delta)));
        const double minimum=(std::min)(.25,length);
        if(edge<0){const double change=juce::jlimit((std::max)(-start,-offset),length-minimum,clipSnap(start+delta)-start);
            put(row,"start",start+change);put(row,"offset",offset+change);put(row,"length",length-change);}
        if(edge>0){const double maximum=(std::min)(sourceBars-offset,4096.-start);
            if(maximum<minimum)throw std::runtime_error("Region source is unavailable; restore the full source or edit its offset");
            put(row,"length",juce::jlimit(minimum,maximum,clipSnap(start+length+delta)-start));}
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
    validatePerformance(state);
    validateLiveAudio(state);
    if(const auto *channels=state["channels"].getArray())
        for(const auto &channel:*channels) {
            const auto independent=channel["playbackIndependent"];
            const auto parent=channel["playbackParent"];
            if((!independent.isVoid() && !independent.isBool()) ||
               (!parent.isVoid() && (!parent.isString() || parent.toString().isEmpty() ||
                  parent==channel["id"] ||
                  find(state,"channels",parent.toString())["type"].toString()!="Playback")))
                throw std::runtime_error("Invalid Playback group reference");
        }
    const auto bindings = state["performance"]["nativePlayback"];
    if (bindings.isVoid())
        return;
    if (!bindings.isObject())
        throw std::runtime_error("Invalid Live Playback bindings");
    for (const auto &pair : bindings.getDynamicObject()->getProperties()) {
        if (!find(state, "items", pair.name.toString()).isObject())
            throw std::runtime_error("Missing Playback item");
        validatePlaybackRow(pair.value);
        if (pair.value["id"].isString() &&
            find(state,"channels",pair.value["id"].toString())["type"].toString()!="Playback")
            throw std::runtime_error("Invalid primary Playback channel");
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
    return primaryPlaybackId(state);
}
inline bool playbackExpanded(const juce::var &state) {
    return static_cast<bool>(state["performance"]["stemsExpanded"]);
}
inline std::vector<PlaybackLane> playbackLanes(const juce::var &state) {
    std::vector<PlaybackLane> result;
    const auto primary=playbackContainer(state);
    std::map<juce::String,std::set<juce::String>> children;
    std::set<juce::String> roots;
    bool primarySource=false;
    if(const auto *items=state["items"].getArray())
        for(const auto &item:*items) {
            const auto rows=playbackRows(state,item["id"].toString());
            if(rows.empty())continue;
            const auto rootChannel=playbackChannelId(state,rows[0]);
            roots.insert(rootChannel);
            primarySource=primarySource || rootChannel==primary;
            for(std::size_t i=1;i<rows.size();++i){
                const auto channelId=rows[i]["id"].toString();
                const auto channel=find(state,"channels",channelId);
                if(static_cast<bool>(channel["playbackIndependent"])) {
                    roots.insert(channelId);
                    continue;
                }
                auto parent=channel["playbackParent"].toString();
                if(find(state,"channels",parent)["type"].toString()!="Playback" ||
                   parent==channelId)parent=primary;
                children[parent].insert(channelId);
            }
        }
    if(const auto *channels=state["channels"].getArray())
        for(const auto &channel:*channels) {
            const auto id=channel["id"].toString();
            bool nested=false;
            for(const auto &group:children)
                nested=nested || (group.second.contains(id) && !roots.contains(id));
            if(nested)continue;
            result.push_back({id,id==primary,false});
            if(!playbackExpanded(state))continue;
            if(id==primary && primarySource)result.push_back({primary,false,true});
            // Preserve show channel order, not lexicographic UUID order:
            // the existing Playback UI and saved stem ordering depend on it.
            for(const auto &child:*channels) {
                const auto childId=child["id"].toString();
                if(children[id].contains(childId) && !roots.contains(childId))
                    result.push_back({childId,false,true});
            }
        }
    return result;
}
// Reorder the persisted channels array without editing any channel object,
// item-channel reference, stem binding, plugin profile or routing. Only roots
// can cross groups; stem children may move solely inside their own group.
inline bool reorderChannel(juce::var& ordered, const juce::var& state,
                           const juce::String& source, const juce::String& target, bool after) {
    if(source.isEmpty() || target.isEmpty() || source==target || !ordered.isArray())return false;
    const auto lanes=playbackLanes(state);
    // A primary Backing with WAV has two visual lanes with the same channel
    // ID: its container and its stem. Prefer the root when reordering that ID.
    const auto laneFor=[&](const juce::String& channel) {
        auto found=std::find_if(lanes.begin(),lanes.end(),[&](const auto& lane) {
            return lane.channel==channel && !lane.child;
        });
        if(found==lanes.end())
            found=std::find_if(lanes.begin(),lanes.end(),[&](const auto& lane) {
                return lane.channel==channel;
            });
        return found;
    };
    const auto sourceLane=laneFor(source);
    const auto targetLane=laneFor(target);
    if(sourceLane==lanes.end() || targetLane==lanes.end() ||
       sourceLane->child!=targetLane->child)return false;
    if(sourceLane->child) {
        const auto item=state["currentItem"].toString();
        if(playbackGroupForChannel(state,item,source)!=playbackGroupForChannel(state,item,target))
            return false;
    }
    auto* rows=ordered.getArray();
    const auto locate=[rows](const juce::String& id) {
        for(int i=0;i<rows->size();++i)
            if((*rows)[i]["id"].toString()==id)return i;
        return -1;
    };
    const int from=locate(source);
    if(from<0 || locate(target)<0)return false;
    auto channel=rows->getUnchecked(from);
    rows->remove(from);
    const int at=locate(target)+(after?1:0);
    rows->insert(at,channel);
    return at!=from;
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
inline juce::var nextPerformanceEntry(const juce::var& state,const juce::String& preferred){
    const auto entries=activeSetlist(state)["entries"];
    const auto at=entryIndex(state,state["currentItem"].toString(),preferred);
    for(int i=at+1;i<entries.size();++i){const auto id=entries[i]["itemId"].toString();
        if(find(state,"items",id).isObject()&&itemPerformanceMode(state,id)!=mrs::PerformanceMode::skip)return entries[i];}
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
