#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <mrs/device.hpp>
#include <array>
#include <cmath>
#include <map>
namespace ui {
inline juce::String deviceLayout(const mrs::audio::DeviceInfo& info) {
    juce::Array<juce::var> inputs,outputs;
    for(const auto& name:info.inputs)inputs.add(juce::String::fromUTF8(name.c_str()));
    for(const auto& name:info.outputs)outputs.add(juce::String::fromUTF8(name.c_str()));
    juce::Array<juce::var> layout;layout.add(inputs);layout.add(outputs);return juce::JSON::toString(layout,true);
}
struct ViewSettings {
    struct NativeEditorView {
        int band{1};bool options{},pinned{};juce::String search,folder;
        int rigTab{1},rigRole{1};bool rigFavorites{},rigFiles{};juce::String rigSearch,rigTag;
        bool operator==(const NativeEditorView&)const=default;
    };
    std::map<juce::String,NativeEditorView> nativeEditorViews;
    NativeEditorView& nativeEditor(const juce::String& id){
        if(!nativeEditorViews.contains(id)&&nativeEditorViews.size()>=256)nativeEditorViews.erase(nativeEditorViews.begin());
        return nativeEditorViews[id];
    }
    struct CabBrowserView {
        int role{1};juce::String search;std::array<juce::String,6> facets;
        bool favoritesOnly{},unavailable{};
        bool operator==(const CabBrowserView&)const=default;
    };
    std::map<juce::String,CabBrowserView> cabBrowserViews; // Per insert ID; UI preference, not DSP/project state.
    struct SectionTemplate{juce::String name;std::uint32_t color{};bool operator==(const SectionTemplate&)const=default;};
    std::vector<SectionTemplate> sectionTemplates;
    bool sidebar{true},snap{},controllersOpen{true},editorAttached{};
    bool noteClipColors{}; // Piano roll view preference; no project mutation.
    int browserWidth{260},trackHeight{128},mixerHeight{365},editorHeight{500};
    double liveUiZoom{}, liveMenuOpacity{.9}, liveWindowOpacity{1.};
    double pixelsPerSecond{30};
    int musicalLanes{31}; // UI only: Markers, Sections, Chords, Tempo, Meter.
    juce::String windowState,inputs,deviceChannels;
    juce::String cabRoot,cabManifest,toneLibrary,rigLibrary;juce::StringArray cabFavorites;
    juce::String reverbRoot;bool reverbQuadConfirmed{}; // global user library, never per-instance
    juce::String encode() const {
        auto* object=new juce::DynamicObject;
        object->setProperty("liveContextStyleVersion",1);object->setProperty("liveUiZoom",liveUiZoom);object->setProperty("liveMenuOpacity",liveMenuOpacity);object->setProperty("liveWindowOpacity",liveWindowOpacity);
        object->setProperty("version",1);object->setProperty("sidebar",sidebar);
        object->setProperty("musicalLanes",musicalLanes);
        juce::Array<juce::var> templates;for(const auto& item:sectionTemplates){auto* entry=new juce::DynamicObject;entry->setProperty("name",item.name);entry->setProperty("color",static_cast<int>(item.color));templates.add(juce::var(entry));}object->setProperty("sectionTemplates",templates);
        object->setProperty("snap",snap);object->setProperty("browserWidth",browserWidth);
        object->setProperty("noteClipColors",noteClipColors);
        object->setProperty("trackHeight",trackHeight);object->setProperty("pixelsPerSecond",pixelsPerSecond);
        object->setProperty("mixerHeight",mixerHeight);object->setProperty("editorHeight",editorHeight);object->setProperty("controllersOpen",controllersOpen);object->setProperty("editorAttached",editorAttached);
        object->setProperty("windowState",windowState);object->setProperty("inputs",inputs);
        object->setProperty("deviceChannels",deviceChannels);
        object->setProperty("rigLibrary",rigLibrary);object->setProperty("toneLibrary",toneLibrary);object->setProperty("cabManifest",cabManifest);object->setProperty("cabRoot",cabRoot);juce::Array<juce::var> favorites;for(const auto& f:cabFavorites)favorites.add(f);object->setProperty("cabFavorites",favorites);
        juce::Array<juce::var> cabViews;for(const auto& [id,state]:cabBrowserViews){auto* entry=new juce::DynamicObject;entry->setProperty("id",id);entry->setProperty("role",state.role);entry->setProperty("search",state.search);entry->setProperty("favoritesOnly",state.favoritesOnly);entry->setProperty("unavailable",state.unavailable);juce::Array<juce::var> filters;for(const auto& f:state.facets)filters.add(f);entry->setProperty("facets",filters);cabViews.add(juce::var(entry));}object->setProperty("cabBrowserViews",cabViews);
        object->setProperty("reverbRoot",reverbRoot);object->setProperty("reverbQuadConfirmed",reverbQuadConfirmed);
        juce::Array<juce::var> nativeViews;for(const auto& [id,state]:nativeEditorViews){auto* entry=new juce::DynamicObject;entry->setProperty("id",id);entry->setProperty("band",state.band);entry->setProperty("options",state.options);entry->setProperty("pinned",state.pinned);entry->setProperty("rigTab",state.rigTab);entry->setProperty("rigRole",state.rigRole);entry->setProperty("rigFavorites",state.rigFavorites);entry->setProperty("rigFiles",state.rigFiles);entry->setProperty("rigSearch",state.rigSearch);entry->setProperty("rigTag",state.rigTag);entry->setProperty("search",state.search);entry->setProperty("folder",state.folder);nativeViews.add(juce::var(entry));}object->setProperty("nativeEditorViews",nativeViews);
        return juce::JSON::toString(juce::var(object));
    }
    static ViewSettings decode(const juce::String& bytes) {
        ViewSettings result;juce::var value;
        if(bytes.length()>32*1024*1024||juce::JSON::parse(bytes,value).failed()||!value.isObject()||static_cast<int>(value["version"])!=1)
            throw std::runtime_error("Invalid JUCE view settings");
        if(value.hasProperty("controllersOpen"))result.controllersOpen=static_cast<bool>(value["controllersOpen"]);if(value.hasProperty("editorAttached"))result.editorAttached=static_cast<bool>(value["editorAttached"]);
        if(value.hasProperty("noteClipColors")){if(!value["noteClipColors"].isBool())throw std::runtime_error("Invalid note color mode");result.noteClipColors=static_cast<bool>(value["noteClipColors"]);}
        if(value.hasProperty("musicalLanes")){const auto mask=value["musicalLanes"];if(!mask.isInt()||static_cast<int>(mask)<0||static_cast<int>(mask)>31)throw std::runtime_error("Invalid musical lane visibility");result.musicalLanes=static_cast<int>(mask);}
        if(value.hasProperty("sectionTemplates")){const auto* entries=value["sectionTemplates"].getArray();if(!entries||entries->size()>64)throw std::runtime_error("Invalid section templates");for(const auto& entry:*entries){const auto name=entry["name"].toString();const auto color=entry["color"];if(!entry.isObject()||name.trim().isEmpty()||name.length()>128||!color.isInt()||static_cast<int>(color)<0||static_cast<int>(color)>0xFFFFFF)throw std::runtime_error("Invalid section template");result.sectionTemplates.push_back({name,static_cast<std::uint32_t>(static_cast<int>(color))});}}
        for (const auto& key : {"liveUiZoom", "liveMenuOpacity", "liveWindowOpacity"}) {
            if (!value.hasProperty(key) || (juce::String(key)=="liveMenuOpacity"&&!value.hasProperty("liveContextStyleVersion"))) continue;
            const auto v=value[key];
            if (!(v.isInt()||v.isDouble())||!std::isfinite(static_cast<double>(v)))
                throw std::runtime_error("Invalid Live appearance preference");
            const double number=v;
            if (juce::String(key)=="liveUiZoom") {
                if (number!=0.&&(number<.75||number>1.5)) throw std::runtime_error("Invalid Live UI zoom");
                result.liveUiZoom=number;
            } else {
                if(number<.7||number>1.) throw std::runtime_error("Invalid Live opacity");
                if(juce::String(key)=="liveMenuOpacity")result.liveMenuOpacity=number;else result.liveWindowOpacity=number;
            }
        }
        result.sidebar=static_cast<bool>(value["sidebar"]);result.snap=static_cast<bool>(value["snap"]);
        result.browserWidth=juce::jlimit(200,700,static_cast<int>(value["browserWidth"]));
        result.trackHeight=juce::jlimit(40,360,static_cast<int>(value["trackHeight"]));
        if(value.hasProperty("mixerHeight"))result.mixerHeight=juce::jlimit(260,2160,static_cast<int>(value["mixerHeight"]));
        if(value.hasProperty("editorHeight"))result.editorHeight=juce::jlimit(500,1400,static_cast<int>(value["editorHeight"]));
        auto scale=static_cast<double>(value["pixelsPerSecond"]);
        result.pixelsPerSecond=std::isfinite(scale)?juce::jlimit(.1,2400.,scale):30.;
        result.windowState=value["windowState"].toString().substring(0,1024);
        result.rigLibrary=value["rigLibrary"].toString().substring(0,16*1024*1024);result.toneLibrary=value["toneLibrary"].toString().substring(0,16*1024*1024);result.cabManifest=value["cabManifest"].toString().substring(0,16*1024*1024);result.cabRoot=value["cabRoot"].toString().substring(0,32768);if(auto* a=value["cabFavorites"].getArray())for(const auto& f:*a)if(result.cabFavorites.size()<10000)result.cabFavorites.add(f.toString().substring(0,4096));
        if(const auto* views=value["cabBrowserViews"].getArray())for(const auto& entry:*views){if(result.cabBrowserViews.size()>=256)break;const auto id=entry["id"].toString();if(!entry.isObject()||id.isEmpty()||id.length()>256)continue;CabBrowserView state;state.role=juce::jlimit(1,4,static_cast<int>(entry["role"]));state.search=entry["search"].toString().substring(0,1024);state.favoritesOnly=static_cast<bool>(entry["favoritesOnly"]);state.unavailable=static_cast<bool>(entry["unavailable"]);if(const auto* filters=entry["facets"].getArray())for(int i=0;i<juce::jmin(6,filters->size());++i)state.facets[static_cast<std::size_t>(i)]=filters->getReference(i).toString().substring(0,4096);result.cabBrowserViews[id]=std::move(state);}
        result.reverbRoot=value["reverbRoot"].toString().substring(0,32768);
        if(const auto* views=value["nativeEditorViews"].getArray())for(const auto& entry:*views){if(result.nativeEditorViews.size()>=256)break;const auto id=entry["id"].toString();if(!entry.isObject()||id.isEmpty()||id.length()>256)continue;NativeEditorView state;state.band=juce::jlimit(0,4,static_cast<int>(entry["band"]));state.options=static_cast<bool>(entry["options"]);state.pinned=static_cast<bool>(entry["pinned"]);state.rigTab=entry.hasProperty("rigTab")?juce::jlimit(1,3,static_cast<int>(entry["rigTab"])):1;state.rigRole=entry.hasProperty("rigRole")?juce::jlimit(1,4,static_cast<int>(entry["rigRole"])):1;state.rigFavorites=static_cast<bool>(entry["rigFavorites"]);state.rigFiles=static_cast<bool>(entry["rigFiles"]);state.rigSearch=entry["rigSearch"].toString().substring(0,1024);state.rigTag=entry["rigTag"].toString().substring(0,128);state.search=entry["search"].toString().substring(0,1024);state.folder=entry["folder"].toString().substring(0,4096);result.nativeEditorViews[id]=std::move(state);}
        result.reverbQuadConfirmed=static_cast<bool>(value["reverbQuadConfirmed"]);
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
