#pragma once
#include <mrs/desktop.hpp>
#include "Settings.h"
namespace ui {
inline juce::String encodePreset(const mrs::NativeInsert& effect) {
    auto p=mrs::desktop::foundation_demo().project;
    p.clips.clear();p.tracks.resize(1);p.tracks.front().inserts={effect};p.master_inserts.clear();
    return juce::String("MRS_PLUGIN_PRESET 1\n")+juce::String::fromUTF8(mrs::serialize(p).c_str());
}
inline mrs::NativeInsert decodePreset(const juce::String& bytes,const mrs::NativeInsert& target) {
    const juce::String magic="MRS_PLUGIN_PRESET 1\n";
    if(bytes.getNumBytesAsUTF8()>8*1024*1024||!bytes.startsWith(magic))throw std::runtime_error("Unsupported MR Studio plugin preset");
    auto p=mrs::deserialize(bytes.substring(magic.length()).toStdString());
    if(p.tracks.size()!=1||p.tracks.front().inserts.size()!=1||!p.clips.empty()||!p.master_inserts.empty())throw std::runtime_error("Invalid plugin preset contents");
    auto saved=p.tracks.front().inserts.front();
    if(saved.kind!=target.kind||(saved.kind==mrs::InsertKind::vst3&&saved.class_id!=target.class_id))throw std::runtime_error("Preset belongs to a different plugin");
    saved.id=target.id;saved.bypass=target.bypass;
    if(saved.kind==mrs::InsertKind::vst3){saved.plugin_path=target.plugin_path;saved.plugin_name=target.plugin_name;}
    saved.validate();return saved;
}
}
