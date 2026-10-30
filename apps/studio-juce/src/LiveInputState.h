#pragma once
#include "LiveShowState.h"
#include <mrs/core.hpp>
#include <algorithm>
#include <cstring>

namespace ui::live {
// A rack edit changes order/bypass/identity, while the editor owns the latest
// binary and parameter state. Preserve that state for surviving instances.
inline void retainCapturedPluginState(std::vector<mrs::NativeInsert>& edited,
                                     const std::vector<mrs::NativeInsert>& original,
                                     const std::vector<mrs::NativeInsert>& captured) {
    for(auto& fx:edited) {
        if(fx.kind!=mrs::InsertKind::vst3)continue;
        const auto same=[&](const auto& old){return old.id==fx.id && old.kind==fx.kind &&
            old.plugin_path==fx.plugin_path && old.class_id==fx.class_id;};
        const auto before=std::find_if(original.begin(),original.end(),same);
        const auto current=std::find_if(captured.begin(),captured.end(),same);
        if(before==original.end() || current==captured.end())continue;
        if(fx.component_state==before->component_state)fx.component_state=current->component_state;
        if(fx.controller_state==before->controller_state)fx.controller_state=current->controller_state;
        if(fx.parameters==before->parameters)fx.parameters=current->parameters;
    }
}
// Show-owned VST3 descriptors; stable Core tracks are runtime projections. Capturing
// current processor states must happen on the control thread while stopped.
// Large VSTi (Omnisphere, sample instruments) can exceed the old 256 KiB cap;
// match the bounded Core VST3 state allowance and the whole-show file budget.
inline constexpr std::size_t maximumLiveVst3StateBytes=16u*1024u*1024u;
inline constexpr std::size_t maximumLiveVst3Base64Length=((maximumLiveVst3StateBytes+2u)/3u)*4u;
inline juce::String liveInputB64(const std::vector<std::byte>& bytes) {
    if (bytes.size() > maximumLiveVst3StateBytes)
        throw std::runtime_error("Live VST3 state exceeds 16 MiB");
    return bytes.empty() ? juce::String() : juce::Base64::toBase64(bytes.data(), bytes.size());
}
inline std::vector<std::byte> liveInputBytes(const juce::var& value) {
    if (!value.isString() || static_cast<std::size_t>(value.toString().length()) > maximumLiveVst3Base64Length)
        throw std::runtime_error("Invalid Live VST3 state encoding");
    if (value.toString().isEmpty()) return {};
    juce::MemoryOutputStream output;
    if (!juce::Base64::convertFromBase64(output, value.toString()) || output.getDataSize() > maximumLiveVst3StateBytes)
        throw std::runtime_error("Invalid Live VST3 state data");
    std::vector<std::byte> result(output.getDataSize());
    if (!result.empty()) std::memcpy(result.data(), output.getData(), result.size());
    return result;
}
inline juce::var liveInputProfile(const mrs::Track& track, const juce::String& physicalName,
                                  const juce::String& deviceName, const std::vector<mrs::NativeInsert>& captured) {
    if ((track.kind != mrs::TrackKind::audio && track.kind != mrs::TrackKind::instrument) ||
        (track.kind == mrs::TrackKind::audio && (track.input < 0 || physicalName.isEmpty())) ||
        physicalName.length() > 256 || !named(deviceName, 256) || captured.size() > 8)
        throw std::runtime_error("Invalid Live Input track/profile");
    auto profile = object();
    put(profile, "version", track.kind==mrs::TrackKind::instrument ? 2 : 1);
    if(track.kind==mrs::TrackKind::instrument) {
        put(profile,"kind","instrument");
        put(profile,"midiInput",juce::String::fromUTF8(track.midi_input.c_str()));
        put(profile,"midiChannel",track.midi_channel);
    }
    // Core uses -2 for tracks with no physical audio input, whereas the
    // existing .mrlive MIDI descriptor uses -1. Keep saved shows compatible
    // when capturing state from an activated Instrument channel.
    put(profile, "input", track.kind==mrs::TrackKind::instrument ? -1 : track.input);
    put(profile, "inputName", physicalName);
    put(profile, "deviceName", deviceName);
    put(profile, "stereo", track.input_stereo);
    auto mix = object();
    put(mix, "gain", track.mix.gain);
    put(mix, "pan", track.mix.pan);
    put(mix, "mute", track.mix.mute);
    put(mix, "solo", track.mix.solo);
    put(profile, "mix", mix);
    juce::Array<juce::var> outputs;
    for (int channel : track.hardware_outputs) outputs.add(channel);
    put(profile, "outputs", outputs);
    juce::Array<juce::var> plugins;
    for (const auto& fx : captured) {
        if(fx.kind==mrs::InsertKind::gain) {
            fx.validate();auto entry=object();
            put(entry,"kind","gain");put(entry,"id",juce::String::fromUTF8(fx.id.value.c_str()));
            put(entry,"gain",fx.gain);put(entry,"bypass",fx.bypass);plugins.add(entry);continue;
        }
        if (fx.kind != mrs::InsertKind::vst3 || fx.plugin_path.empty() ||
            fx.class_id.empty() || fx.id.value.empty())
            throw std::runtime_error("Live Input v1 accepts VST3 effects only");
        fx.validate();
        auto entry = object();
        put(entry, "id", juce::String::fromUTF8(fx.id.value.c_str()));
        put(entry, "path", juce::String::fromUTF8(fx.plugin_path.c_str()));
        put(entry, "classId", juce::String::fromUTF8(fx.class_id.c_str()));
        put(entry, "name", juce::String::fromUTF8(fx.plugin_name.c_str()));
        put(entry, "bypass", fx.bypass);
        put(entry, "component", liveInputB64(fx.component_state));
        put(entry, "controller", liveInputB64(fx.controller_state));
        juce::Array<juce::var> parameters;
        for (const auto& parameter : fx.parameters) {
            auto p = object();
            put(p, "id", static_cast<juce::int64>(parameter.id));
            put(p, "value", parameter.value);
            parameters.add(p);
        }
        put(entry, "parameters", parameters);
        plugins.add(entry);
    }
    put(profile, "inserts", plugins);
    return profile;
}
struct LiveInputLoaded {
    bool instrument{};
    juce::String midiInput;
    int midiChannel{-1};
    int input{-1};
    juce::String name, device, rightName;
    std::vector<juce::String> outputNames;
    bool stereo{};
    std::vector<int> outputs;
    mrs::Track::Mix mix;
    std::vector<mrs::NativeInsert> inserts;
};
inline int liveCoreTrackInput(const LiveInputLoaded& profile) noexcept {
    // Instrument consumes MIDI, never a physical audio device input. Core
    // Project::validate accepts -2 for every non-audio track, not -1.
    return profile.instrument ? -2 : profile.input;
}
inline LiveInputLoaded decodeLiveInputProfile(const juce::var& state) {
    if (!state.isObject() || !state["version"].isInt() || (static_cast<int>(state["version"]) != 1 && static_cast<int>(state["version"]) != 2) ||
        !state["input"].isInt() || !state["inputName"].isString() || state["inputName"].toString().length()>256 ||
        !named(state["deviceName"], 256) ||
        !state["stereo"].isBool() || !state["mix"].isObject() ||
        !state["outputs"].isArray() || state["outputs"].size() > 2 ||
        !state["inserts"].isArray() || state["inserts"].size() > 8)
        throw std::runtime_error("Invalid saved Live Input profile");
    LiveInputLoaded result;
    result.input=static_cast<int>(state["input"]);
    result.instrument=static_cast<int>(state["version"])==2;
    if(result.instrument) {
        if(state["kind"].toString()!="instrument" || !state["midiInput"].isString() ||
           state["midiInput"].toString().length()>256 || !state["midiChannel"].isInt() ||
           static_cast<int>(state["midiChannel"]) < -1 || static_cast<int>(state["midiChannel"])>15)
            throw std::runtime_error("Invalid Live MIDI input");
        result.midiInput=state["midiInput"].toString();result.midiChannel=static_cast<int>(state["midiChannel"]);
    } else if(result.input<0 || result.input>=64 || state["inputName"].toString().isEmpty())
        throw std::runtime_error("Invalid Live Input index/name");
    result.name=state["inputName"].toString();
    result.device=state["deviceName"].toString();
    result.stereo=static_cast<bool>(state["stereo"]);
    if((!result.instrument && result.stereo && result.input>=63) ||
       (result.instrument && (result.input!=-1 || result.stereo)))
        throw std::runtime_error("Invalid Live input kind / stereo pair");
    if(!state["inputRightName"].isVoid()) {
        if(!named(state["inputRightName"],256))throw std::runtime_error("Invalid stereo port name");
        result.rightName=state["inputRightName"].toString();
    }
    auto mix=state["mix"];
    if (!number(mix["gain"]) || !number(mix["pan"]) ||
        !mix["mute"].isBool() || !mix["solo"].isBool())
        throw std::runtime_error("Invalid Live Input mix");
    result.mix.gain=static_cast<float>(static_cast<double>(mix["gain"]));
    result.mix.pan=static_cast<float>(static_cast<double>(mix["pan"]));
    result.mix.mute=static_cast<bool>(mix["mute"]);
    result.mix.solo=static_cast<bool>(mix["solo"]);
    result.mix.validate();
    std::set<int> uniqueOutputs;
    for(const auto& value : *state["outputs"].getArray()) {
        if(!value.isInt() || static_cast<int>(value)<0 || static_cast<int>(value)>=64 ||
           !uniqueOutputs.insert(static_cast<int>(value)).second)
            throw std::runtime_error("Invalid Live Input output routing");
        result.outputs.push_back(static_cast<int>(value));
    }
    if(!state["outputNames"].isVoid()) {
        if(!state["outputNames"].isArray() || state["outputNames"].size()!=state["outputs"].size())
            throw std::runtime_error("Invalid output port names");
        for(const auto& name:*state["outputNames"].getArray()) {
            if(!named(name,256))throw std::runtime_error("Invalid output port name");
            result.outputNames.push_back(name.toString());
        }
    }
    std::set<std::string> uniqueIds;
    for(const auto& value : *state["inserts"].getArray()) {
        if(value["kind"].toString()=="gain") {
            if(!value.isObject() || !named(value["id"],128) || !number(value["gain"]) || !value["bypass"].isBool())
                throw std::runtime_error("Invalid Live native gain descriptor");
            mrs::NativeInsert fx;fx.id=mrs::Id{value["id"].toString().toStdString()};fx.kind=mrs::InsertKind::gain;
            if(!uniqueIds.insert(fx.id.value).second)throw std::runtime_error("Duplicate Live insert ID");
            fx.gain=static_cast<float>(static_cast<double>(value["gain"]));fx.bypass=static_cast<bool>(value["bypass"]);
            fx.validate();result.inserts.push_back(std::move(fx));continue;
        }
        if (!value.isObject() || !named(value["id"], 128) || !named(value["path"], 4096) ||
            !named(value["classId"], 256) || !value["name"].isString() ||
            !value["bypass"].isBool() || !value["parameters"].isArray() ||
            value["parameters"].size()>2048)
            throw std::runtime_error("Invalid Live Input VST3 descriptor");
        mrs::NativeInsert fx;
        fx.id=mrs::Id{value["id"].toString().toStdString()};
        if(!uniqueIds.insert(fx.id.value).second)
            throw std::runtime_error("Duplicate Live Input VST3 insert");
        fx.kind=mrs::InsertKind::vst3;
        fx.plugin_path=value["path"].toString().toStdString();
        fx.class_id=value["classId"].toString().toStdString();
        fx.plugin_name=value["name"].toString().toStdString();
        fx.bypass=static_cast<bool>(value["bypass"]);
        fx.component_state=liveInputBytes(value["component"]);
        fx.controller_state=liveInputBytes(value["controller"]);
        for(const auto& parameter : *value["parameters"].getArray()) {
            if(!parameter.isObject() || !number(parameter["id"]) || !number(parameter["value"]))
                throw std::runtime_error("Invalid Live Input plugin parameter");
            const auto id=static_cast<double>(parameter["id"]);
            const auto level=static_cast<double>(parameter["value"]);
            if(!std::isfinite(id) || id<0 || id>4294967295. || std::floor(id)!=id ||
               !std::isfinite(level) || level<0 || level>1)
                throw std::runtime_error("Invalid Live Input plugin parameter range");
            fx.parameters.push_back({static_cast<std::uint32_t>(id),static_cast<float>(level)});
        }
        fx.validate();
        result.inserts.push_back(std::move(fx));
    }
    return result;
}
inline void validateLiveInputProfiles(const juce::var& state) {
    if(const auto* channels=state["channels"].getArray())
        for(const auto& channel:*channels) {
            const auto profile=channel["liveInput"];
            if(!profile.isVoid()) {
                if(channel["type"].toString()=="Playback")
                    throw std::runtime_error("Playback is not a Live Input");
                const auto decoded=decodeLiveInputProfile(profile);
                if(decoded.instrument && channel["type"].toString()!="Instrument")
                    throw std::runtime_error("MIDI / VSTi requires an Instrument show channel");
            }
        }
}
} // namespace ui::live
