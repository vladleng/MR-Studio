#pragma once
#include "LivePlaybackState.h"
#include <mrs/desktop.hpp>
#include <mrs/playback_preflight.hpp>
namespace ui::live {
inline mrs::desktop::Application::PlaybackStem playbackStemLayout(const juce::var &state,const juce::String &item,
                                                                  const juce::var &row,int rate,std::size_t sourceIndex=0) {
    mrs::desktop::Application::PlaybackStem result;
    result.mix=playbackMix(state,item,row);result.outputs=playbackOutputs(row);
    result.begin=static_cast<double>(row["trimStart"]);result.end=row["trimEnd"].isVoid()?-1.:static_cast<double>(row["trimEnd"]);
    const auto rows=storedPlaybackRegions(state,item,row);validatePlaybackRegions(rows);
    if(rows.isArray()){
        result.regions.emplace();
        for(const auto& region:*rows.getArray()){
            const double seconds=region["audioSecondsPerBar"].isVoid()?playbackSecondsPerBar(state,item):static_cast<double>(region["audioSecondsPerBar"]);
            const auto sample=[&](double bars){
                const long double value=static_cast<long double>(bars)*seconds*rate;
                if(!std::isfinite(value)||value<0||value>mrs::max_sample)throw std::runtime_error("Playback region exceeds supported sample range");
                return static_cast<mrs::Sample>(std::llround(value));
            };
            const double start=region["start"],offset=region["offset"],length=region["length"];
            result.regions->push_back({{"live-"+std::to_string(sourceIndex)+":"+region["id"].toString().toStdString()},
                sample(start),sample(offset+length)-sample(offset),sample(offset),
                region["audioSecondsPerBar"].isVoid()||static_cast<bool>(region["legacySourceBounds"])});
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
