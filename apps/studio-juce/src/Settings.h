#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <mrs/device.hpp>
namespace ui {
inline juce::String deviceLayout(const mrs::audio::DeviceInfo& info) {
    juce::Array<juce::var> inputs,outputs;
    for(const auto& name:info.inputs)inputs.add(juce::String::fromUTF8(name.c_str()));
    for(const auto& name:info.outputs)outputs.add(juce::String::fromUTF8(name.c_str()));
    juce::Array<juce::var> layout;layout.add(inputs);layout.add(outputs);return juce::JSON::toString(layout,true);
}
struct ViewSettings {
    bool sidebar{true},snap{},controllersOpen{true},editorAttached{};
    int browserWidth{260},trackHeight{128},mixerHeight{365};
    double pixelsPerSecond{30};
    juce::String windowState,inputs,deviceChannels;
    juce::String encode() const {
        auto* object=new juce::DynamicObject;
        object->setProperty("version",1);object->setProperty("sidebar",sidebar);
        object->setProperty("snap",snap);object->setProperty("browserWidth",browserWidth);
        object->setProperty("trackHeight",trackHeight);object->setProperty("pixelsPerSecond",pixelsPerSecond);
        object->setProperty("mixerHeight",mixerHeight);object->setProperty("controllersOpen",controllersOpen);object->setProperty("editorAttached",editorAttached);
        object->setProperty("windowState",windowState);object->setProperty("inputs",inputs);
        object->setProperty("deviceChannels",deviceChannels);
        return juce::JSON::toString(juce::var(object));
    }
    static ViewSettings decode(const juce::String& bytes) {
        ViewSettings result;juce::var value;
        if(bytes.length()>65536||juce::JSON::parse(bytes,value).failed()||!value.isObject()||static_cast<int>(value["version"])!=1)
            throw std::runtime_error("Invalid JUCE view settings");
        if(value.hasProperty("controllersOpen"))result.controllersOpen=static_cast<bool>(value["controllersOpen"]);if(value.hasProperty("editorAttached"))result.editorAttached=static_cast<bool>(value["editorAttached"]);
        result.sidebar=static_cast<bool>(value["sidebar"]);result.snap=static_cast<bool>(value["snap"]);
        result.browserWidth=juce::jlimit(200,700,static_cast<int>(value["browserWidth"]));
        result.trackHeight=juce::jlimit(128,360,static_cast<int>(value["trackHeight"]));
        if(value.hasProperty("mixerHeight"))result.mixerHeight=juce::jlimit(260,1400,static_cast<int>(value["mixerHeight"]));
        auto scale=static_cast<double>(value["pixelsPerSecond"]);
        result.pixelsPerSecond=std::isfinite(scale)?juce::jlimit(2.,2400.,scale):30.;
        result.windowState=value["windowState"].toString().substring(0,1024);
        result.inputs=value["inputs"].toString().substring(0,256);
        result.deviceChannels=value["deviceChannels"].toString().substring(0,32768);return result;
    }
};
inline void publishSettings(const juce::File& file,const juce::String& bytes) {
    if(!file.getParentDirectory().createDirectory())throw std::runtime_error("Cannot create settings folder");
    juce::TemporaryFile temporary(file);
    if(!temporary.getFile().replaceWithText(bytes)||!temporary.overwriteTargetFileWithTemporary())
        throw std::runtime_error("Cannot publish settings file");
}
}
