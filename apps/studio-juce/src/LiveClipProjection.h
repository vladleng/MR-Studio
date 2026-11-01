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
// Attribute import must not commit an inactive Item whose existing region
// sample extents become invalid at the imported tempo. Physical routes are
// unchanged and deliberately excluded from this musical-source check.
inline std::vector<mrs::audio::PlaybackSourceCheck> contextImportPlaybackChecks(
    const juce::var& before,const juce::var& after,const std::vector<juce::String>& targets,int rate) {
    std::vector<mrs::audio::PlaybackSourceCheck> result;
    for(const auto& item:targets) {
        if(playbackSecondsPerBar(before,item)==playbackSecondsPerBar(after,item))continue;
        const auto sources=playbackRows(after,item);
        for(std::size_t i=0;i<sources.size();++i) {
            if(!playbackRegions(after,item,sources[i]).isArray())continue;
            const auto layout=playbackStemLayout(after,item,sources[i],rate,i);
            mrs::audio::PlaybackSourceCheck request;
            request.key=item.toStdString()+":"+std::to_string(i);
            request.file=std::filesystem::path(sources[i]["file"].toString().toWideCharPointer());
            request.begin=layout.begin;request.end=layout.end;
            request.regions.emplace();
            for(const auto& region:*layout.regions)request.regions->push_back({region.offset,region.length});
            result.push_back(std::move(request));
        }
    }
    return result;
}

}
