#pragma once
#include "LiveDocument.h"
#include "LiveShowContextImport.h"
#include <mrs/core.hpp>
#include <algorithm>
#include <cmath>

// Studio -> native Live show. Do not approximate unsupported timing maps.
namespace ui::live {
inline ShowContextSong studioExportContext(const mrs::Project& project,mrs::Sample begin,mrs::Sample end,
                                          const juce::String& title) {
    if(begin<0||end<=begin||end>mrs::max_sample)throw std::invalid_argument("Invalid Live export range");
    const mrs::Timeline timeline(project.time,project.sample_rate);
    const auto firstTick=timeline.to_ticks(begin);
    const auto firstPosition=timeline.musical_position(firstTick);
    if(firstPosition.beat!=1||firstPosition.tick!=0||timeline.to_samples(firstTick)!=begin)
        throw std::runtime_error("MR Live export must start on a bar boundary to preserve click and grid alignment");
    const auto tempo=std::find_if(project.time.tempos.rbegin(),project.time.tempos.rend(),
        [&](const auto& p){return timeline.to_samples(p.tick)<=begin;});
    const auto meter=std::find_if(project.time.meters.rbegin(),project.time.meters.rend(),
        [&](const auto& p){return p.bar<=firstPosition.bar;});
    if(tempo==project.time.tempos.rend()||meter==project.time.meters.rend())
        throw std::runtime_error("Missing source tempo or meter");
    for(const auto& point:project.time.tempos){
        const auto sample=timeline.to_samples(point.tick);
        if(sample>begin&&sample<end&&point.bpm!=tempo->bpm)
            throw std::runtime_error("MR Live does not yet support variable tempo within an item");
    }
    for(const auto& point:project.time.meters){
        const auto sample=timeline.to_samples(timeline.to_ticks(mrs::MusicalPosition{point.bar,1,0}));
        if(sample>begin&&sample<end&&(point.numerator!=meter->numerator||point.denominator!=meter->denominator))
            throw std::runtime_error("MR Live does not yet support meter changes within an item");
    }
    const double secondsPerBar=60./tempo->bpm*4.*meter->numerator/meter->denominator;
    const double samplesPerBar=secondsPerBar*project.sample_rate;
    const double barTicks=static_cast<double>(mrs::ppq)*4.*meter->numerator/meter->denominator;
    // Derive duration from the exact rendered interval, not a rounded tick.
    const double bars=static_cast<double>(end-begin)/samplesPerBar;
    if(bars<1.||bars>4096.||tempo->bpm<20.||tempo->bpm>400.)
        throw std::runtime_error("Export range or tempo is outside MR Live item limits");
    ShowContextSong result;
    result.sourceItemOrdinal=0;result.sourceItemTitle=title;
    result.bpm=tempo->bpm;result.lengthBars=bars;
    result.sourceMeter=juce::String(meter->numerator)+"/"+juce::String(meter->denominator);
    juce::Array<juce::var> sections,chords;
    const auto append=[&](auto& rows,const auto& event,const char* key,const std::string& text){
        const double a=std::max(0.,static_cast<double>(event.start-firstTick)/barTicks);
        const double b=std::min(bars,static_cast<double>(event.end-firstTick)/barTicks);
        if(b<=a)return;
        auto row=object();
        put(row,"id",freshId());put(row,"start",a);put(row,"length",b-a);
        put(row,key,juce::String::fromUTF8(text.c_str()));rows.add(row);
    };
    for(const auto& event:project.sections)append(sections,event,"name",event.name);
    for(const auto& event:project.chords)append(chords,event,"symbol",event.symbol);
    result.sections=juce::var(sections);result.chords=juce::var(chords);
    result.sourceSectionCount=sections.size();result.sourceChordCount=chords.size();
    validateMusicalRows(result.sections,MusicalLane::sections);
    validateMusicalRows(result.chords,MusicalLane::chords);
    return result;
}

// Prepare a detached document on the UI owner. Its file references are
// relative to the eventual export folder, and the existing serializer
// preserves that relationship in both working and Browser saved versions.
inline LiveDocument studioExportDocument(const mrs::Project& project,mrs::Sample begin,mrs::Sample end,
        const juce::String& title,std::span<const mrs::Id> selected,const juce::File& directory){
    if(selected.empty()||selected.size()>32)throw std::runtime_error("MR Live supports 1..32 stems");
    auto context=studioExportContext(project,begin,end,title);
    LiveDocument doc;doc.title=title;doc.ensureInterface();
    auto& show=doc.interfaceState;
    context.targetItemId=addItem(show,title);
    auto item=require(show,"items",context.targetItemId);
    put(item,"meter",context.sourceMeter);
    applyShowContext(show,context,{true,true,true,true});
    // applyShowContext replaces the candidate; reacquire the item object.
    item=require(show,"items",context.targetItemId);
    put(item,"duration",static_cast<double>(end-begin)/project.sample_rate);
    std::vector<mrs::Id> used;
    for(std::size_t i=0;i<selected.size();++i){
        const auto track=std::find_if(project.tracks.begin(),project.tracks.end(),
            [&](const auto& t){return t.id==selected[i];});
        if(track==project.tracks.end()||std::find(used.begin(),used.end(),selected[i])!=used.end())
            throw std::runtime_error("Missing or duplicate selected track");
        used.push_back(selected[i]);
        const auto channel=addChannel(show,"Playback",juce::String::fromUTF8(track->name.c_str()));
        if(i>0)put(require(show,"channels",channel),"playbackParent",primaryPlaybackId(show));
        const auto file=directory.getChildFile("stem-"+juce::String(static_cast<int>(i+1))+".wav");
        bindPlaybackSource(show,context.targetItemId,channel,file.getFullPathName(),false);
    }
    saveItem(show,context.targetItemId);
    // Browser only: never insert a performance entry or start playback.
    doc.validate();
    return doc;
}
}
