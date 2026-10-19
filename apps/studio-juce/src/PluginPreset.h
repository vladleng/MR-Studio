#pragma once
#include <mrs/desktop.hpp>
#include "Settings.h"
#include "Controls.h"
#include <map>
namespace ui {
class PresetMenuTheme final : public juce::LookAndFeel_V4 {
public:
    PresetMenuTheme(){configurePopupColours(*this);}
    void drawPopupMenuBackground(juce::Graphics& g,int width,int height) override {drawPopupSurface(g,width,height);}
    juce::Font getPopupMenuFont() override {return Theme::browserFont();}
    void getIdealPopupMenuItemSize(const juce::String& text,bool separator,int height,int& width,int& resultHeight) override {juce::LookAndFeel_V4::getIdealPopupMenuItemSize(text,separator,separator?height:juce::jmax(22,height),width,resultHeight);}
};
inline juce::File presetFolder(const mrs::NativeInsert& effect,const juce::File& root=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MR Studio/Presets/Plugins")) {
    juce::String name=effect.kind==mrs::InsertKind::vst3?juce::String::fromUTF8(effect.plugin_name.c_str()):"Native-"+juce::String(static_cast<int>(effect.kind));
    name=juce::File::createLegalFileName(name).trim().substring(0,80);
    if(name.isEmpty())name="Plugin";
    if(effect.kind==mrs::InsertKind::vst3)name+="-"+juce::String::fromUTF8(effect.class_id.c_str());
    return root.getChildFile(name);
}
inline juce::Array<juce::File> presetFiles(const mrs::NativeInsert& effect,const juce::File& root=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MR Studio/Presets/Plugins")) {
    auto files=presetFolder(effect,root).findChildFiles(juce::File::findFiles,true,"*.mrspreset",juce::File::FollowSymlinks::no);
    files.sort();return files;
}
inline juce::String presetLabel(const juce::File& file,const juce::File& folder){
    return (file.isAChildOf(folder)?file.getRelativePathFrom(folder):"(external) "+file.getFileName()).replaceCharacter('\\','/').dropLastCharacters(file.getFileExtension().length());
}
inline juce::File presetChild(const juce::File& root,juce::String relative,bool file){
    relative=relative.trim().replaceCharacter('\\','/');
    auto parts=juce::StringArray::fromTokens(relative,"/","");
    if(relative.isEmpty()||relative.startsWithChar('/')||relative.endsWithChar('/')||relative.contains("//")||relative.length()>200||parts.size()>8)throw std::runtime_error("Use a relative preset path (1..200 characters, up to 8 parts)");
    auto result=root;
    for(const auto& part:parts){
        auto stem=part.upToFirstOccurrenceOf(".",false,false).toUpperCase();
        const bool reserved=stem=="CON"||stem=="PRN"||stem=="AUX"||stem=="NUL"||((stem.startsWith("COM")||stem.startsWith("LPT"))&&stem.length()==4&&stem[3]>='1'&&stem[3]<='9');
        if(part.isEmpty()||part=="."||part==".."||part!=part.trim()||part.endsWithChar('.')||reserved||part!=juce::File::createLegalFileName(part))throw std::runtime_error("Use valid folder/preset names; absolute paths and '..' are not allowed");
        result=result.getChildFile(part);if(result.isSymbolicLink())throw std::runtime_error("Preset paths cannot follow symbolic links");
    }
    if(!result.isAChildOf(root))throw std::runtime_error("Preset path must stay in the plugin library");
    return file&&!result.hasFileExtension(".mrspreset")?result.getSiblingFile(result.getFileName()+".mrspreset"):result;
}
inline juce::PopupMenu presetMenu(const juce::Array<juce::File>& files,const juce::File& folder,const juce::String& selected){
    struct Node{std::map<juce::String,Node> folders;std::vector<std::pair<int,juce::String>> items;};Node root;
    for(const auto& directory:folder.findChildFiles(juce::File::findDirectories,true,"*",juce::File::FollowSymlinks::no)){
        auto* node=&root;for(const auto& part:juce::StringArray::fromTokens(directory.getRelativePathFrom(folder).replaceCharacter('\\','/'),"/",""))node=&node->folders[part];
    }
    for(int i=0;i<files.size();++i){auto parts=juce::StringArray::fromTokens(presetLabel(files[i],folder),"/","");auto* node=&root;for(int j=0;j+1<parts.size();++j)node=&node->folders[parts[j]];node->items.emplace_back(i+1,parts[parts.size()-1]);}
    std::function<juce::PopupMenu(const Node&)> build=[&](const Node& n){juce::PopupMenu menu;for(const auto& [name,child]:n.folders)menu.addSubMenu(name,build(child));for(const auto& [id,name]:n.items)menu.addItem(id,name,true,presetLabel(files[id-1],folder)==selected);if(n.folders.empty()&&n.items.empty())menu.addItem(10003,"Empty folder",false);return menu;};return build(root);
}
inline juce::String encodePreset(const mrs::NativeInsert& effect) {
    auto p=mrs::desktop::foundation_demo().project;
    p.clips.clear();p.tracks.resize(1);p.tracks.front().inserts={effect};p.master_inserts.clear();
    return juce::String("MRS_PLUGIN_PRESET 1\n")+juce::String::fromUTF8(mrs::serialize(p).c_str());
}
inline mrs::NativeInsert decodePreset(const juce::String& bytes,const mrs::NativeInsert& target) {
    const juce::String magic="MRS_PLUGIN_PRESET 1\n";
    const auto header=bytes.startsWith("MRS_PLUGIN_PRESET 1\r\n")?juce::String("MRS_PLUGIN_PRESET 1\r\n"):magic;
    if(bytes.getNumBytesAsUTF8()>8*1024*1024||!bytes.startsWith(header))throw std::runtime_error("Unsupported MR Studio plugin preset");
    auto p=mrs::deserialize(bytes.substring(header.length()).toStdString());
    if(p.tracks.size()!=1||p.tracks.front().inserts.size()!=1||!p.clips.empty()||!p.master_inserts.empty())throw std::runtime_error("Invalid plugin preset contents");
    auto saved=p.tracks.front().inserts.front();
    if(saved.kind!=target.kind||(saved.kind==mrs::InsertKind::vst3&&saved.class_id!=target.class_id))throw std::runtime_error("Preset belongs to a different plugin");
    saved.id=target.id;if(saved.kind!=mrs::InsertKind::reverb&&!mrs::wav_core(saved.kind))saved.bypass=target.bypass;
    if(saved.kind==mrs::InsertKind::vst3){saved.plugin_path=target.plugin_path;saved.plugin_name=target.plugin_name;}
    saved.validate();return saved;
}
}
