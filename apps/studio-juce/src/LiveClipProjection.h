#pragma once
#include "LivePlaybackState.h"
#include <mrs/desktop.hpp>
namespace ui::live {
inline mrs::desktop::Application::PlaybackStem playbackStemLayout(const juce::var &state,const juce::String &item,
                                                                  const juce::var &row,int rate,std::size_t sourceIndex=0) {
    mrs::desktop::Application::PlaybackStem result;
    result.mix=playbackMix(state,item,row);result.outputs=playbackOutputs(row);
    result.begin=static_cast<double>(row["trimStart"]);result.end=row["trimEnd"].isVoid()?-1.:static_cast<double>(row["trimEnd"]);
    const auto rows=playbackRegions(state,item,row);validatePlaybackRegions(rows);
    if(rows.isArray()){
        result.regions.emplace();const double samplesPerBar=playbackSecondsPerBar(state,item)*rate;
        for(const auto& region:*rows.getArray()){
            const auto sample=[&](double bars){return static_cast<mrs::Sample>(std::llround(bars*samplesPerBar));};
            const double start=region["start"],offset=region["offset"],length=region["length"];
            result.regions->push_back({{"live-"+std::to_string(sourceIndex)+":"+region["id"].toString().toStdString()},
                sample(start),sample(offset+length)-sample(offset),sample(offset)});
        }
    }
    return result;
}
inline std::vector<mrs::desktop::Application::PlaybackStem> playbackStemLayouts(const juce::var &state,const juce::String &item,int rate){
    std::vector<mrs::desktop::Application::PlaybackStem> result;
    for(const auto& row:playbackRows(state,item))result.push_back(playbackStemLayout(state,item,row,rate,result.size()));
    return result;
}
}
