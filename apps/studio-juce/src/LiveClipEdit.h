#pragma once
#include "LivePlaybackState.h"
#include "LiveEditProtocol.h"

namespace ui::live {
inline EditPath playbackRegionPath(const juce::var& state,const juce::String& item,const juce::String& stem) {
    if(!find(state,"items",item).isObject())throw std::runtime_error("Playback Item was removed");
    const auto row=playbackStem(state,item,stem);
    if(!row.isObject())throw std::runtime_error("Playback source was removed");
    const auto track=playbackTrackId(state,item,row);
    if(track.isNotEmpty())return {"performance","clips",track};
    EditPath path{"performance","nativePlayback",item};
    if(stem.isNotEmpty()){path.emplace_back("stems");path.push_back(EditStep::id(stem));}
    path.emplace_back("regions");return path;
}
inline ValuePatch playbackRegionPatch(const juce::var& state,const juce::String& item,const juce::String& stem,const juce::var& rows) {
    auto path=playbackRegionPath(state,item,stem);
    validatePlaybackRegions(rows);
    const auto source=playbackStem(state,item,stem);
    const auto current=playbackRegions(state,item,source),stored=storedPlaybackRegions(state,item,source);
    if(sameShowValue(rows,current)){
        const auto value=editValue(state,path);return {std::move(path),value,value};
    }
    auto saved=rows.clone();
    for(auto& region:*saved.getArray()){
        bool unchanged=false;
        if(const auto* before=current.getArray())for(int i=0;i<before->size();++i)
            if(sameShowValue(region,(*before)[i])){region=stored[i].clone();unchanged=true;break;}
        if(!unchanged)put(region,"audioSecondsPerBar",playbackSecondsPerBar(state,item));
    }
    return valuePatch(state,std::move(path),saved);
}
}
