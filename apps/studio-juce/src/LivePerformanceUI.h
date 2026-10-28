#pragma once
#include "LiveActionMenu.h"
#include "LivePerformanceState.h"
namespace ui {
inline void addLivePerformanceModes(juce::PopupMenu& menu,const juce::var& state,const juce::String& target,int first){
    const auto selected=live::itemPerformanceMode(state,target);
    int index=0;for(const auto mode:{mrs::PerformanceMode::normal,mrs::PerformanceMode::skip,mrs::PerformanceMode::stop,mrs::PerformanceMode::loop})
        menu.addItem(first+index++,live::performanceModeLabel(mode),true,mode==selected);
}
inline void showLivePerformanceModes(Desktop& owner,const juce::String& item,const juce::String& target,juce::Point<int> screen){
    if(!owner.liveDocument)return;
    juce::PopupMenu menu;addLivePerformanceModes(menu,owner.liveDocument->interfaceState,target,1);
    juce::Component::SafePointer<Desktop> safe(&owner);
    showLiveActionMenu(menu,owner,liveContextTarget(screen),[safe,item,target](int choice){
        if(safe && choice>=1 && choice<=4)safe->run([&]{safe->editLiveInterface("Performance mode",[&](auto& state){
            live::setPerformanceMode(state,item,target,static_cast<mrs::PerformanceMode>(choice-1));
        });});
    });
}
class LivePerformanceArt {
 public:
    LivePerformanceArt(){
        const std::array<const char*,4> bytes{BinaryData::play_svg,BinaryData::next_svg,BinaryData::stop_svg,BinaryData::loop_svg};
        const std::array<int,4> sizes{BinaryData::play_svgSize,BinaryData::next_svgSize,BinaryData::stop_svgSize,BinaryData::loop_svgSize};
        for(std::size_t i=0;i<icons.size();++i){icons[i]=juce::Drawable::createFromImageData(bytes[i],sizes[i]);if(!icons[i])throw std::runtime_error("Missing performance SVG");}
    }
    void draw(juce::Graphics& g,mrs::PerformanceMode mode,juce::Rectangle<int> area){const auto index=static_cast<std::size_t>(mode);if(index<icons.size())icons[index]->drawWithin(g,area.toFloat(),juce::RectanglePlacement::centred,.85f);}
 private:
    std::array<std::unique_ptr<juce::Drawable>,4> icons;
};
}
