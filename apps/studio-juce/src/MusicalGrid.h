#pragma once
#include <mrs/core.hpp>
#include <algorithm>
namespace ui {
// Shared display/snap ladder: sixteenth minimum, eighth, quarter, half, bar(s).
// Meter boundaries remain the phase origin; odd bars cap the last subdivision.
inline mrs::Tick displayGridStep(mrs::Tick /*beat*/,mrs::Tick bar,mrs::Tick spacing){
    if(bar<mrs::ppq/4){auto step=bar;while(step<std::max(spacing,mrs::ppq/4)&&step<mrs::max_tick/2)step*=2;return step;}
    auto step=mrs::ppq/4;
    while(step<spacing&&step<bar)step=std::min(step*2,bar);
    while(step<spacing&&step<mrs::max_tick/2)step*=2;
    return step;
}
inline std::pair<mrs::Tick,mrs::Tick> displayGridNeighbours(const mrs::TimeMap& map,mrs::Tick tick,mrs::Tick spacing){
    mrs::Tick lower=0,upper=mrs::max_tick,start=0;bool foundUpper=false;
    for(std::size_t i=0;i<map.meters.size();++i){const auto& point=map.meters[i];const auto beat=4*mrs::ppq/point.denominator,bar=beat*point.numerator;
        const auto end=i+1<map.meters.size()?start+(map.meters[i+1].bar-point.bar)*bar:mrs::max_tick;
        const auto step=displayGridStep(beat,bar,std::max(mrs::Tick{1},spacing));
        const auto local=std::clamp(tick,start,end-1);
        const auto origin=step<bar?start+((local-start)/bar)*bar:start;
        const auto floor=origin+((local-origin)/step)*step;
        const auto next=step<bar?std::min(floor+step,origin+bar):(floor<=mrs::max_tick-step?floor+step:floor);
        for(const auto candidate:{floor,next})if(candidate>=start&&candidate<end){if(candidate<=tick)lower=std::max(lower,candidate);if(candidate>=tick){upper=std::min(upper,candidate);foundUpper=true;}}
        start=end;
    }
    return {lower,foundUpper?upper:lower};
}
template<class Emit> void visitDisplayGrid(const mrs::TimeMap& map,mrs::Tick first,mrs::Tick last,mrs::Tick spacing,Emit emit){
    spacing=std::max(mrs::Tick{1},spacing);mrs::Tick start=0;
    for(std::size_t i=0;i<map.meters.size();++i){const auto& point=map.meters[i];const mrs::Tick beat=4*mrs::ppq/point.denominator,bar=beat*point.numerator;
        const auto end=i+1<map.meters.size()?start+(map.meters[i+1].bar-point.bar)*bar:mrs::max_tick;
        const auto step=displayGridStep(beat,bar,spacing);
        const auto left=std::max(first,start),right=std::min(last,end-1);
        const auto line=[&](mrs::Tick tick){const auto local=tick-start;emit(tick,mrs::MusicalPosition{point.bar+local/bar,static_cast<int>((local%bar)/beat)+1,local%beat});};
        if(left<=right){
            if(step<bar){for(auto origin=start+((left-start)/bar)*bar;origin<=right;){
                const auto limit=std::min(right,origin+bar-1);
                for(auto tick=origin+((std::max(left,origin)-origin+step-1)/step)*step;tick<=limit;tick+=step)line(tick);
                if(origin>mrs::max_tick-bar)break;origin+=bar;
            }}else for(auto tick=start+((left-start+step-1)/step)*step;tick<=right;){line(tick);if(tick>mrs::max_tick-step)break;tick+=step;}
        }
        start=end;if(start>last)break;
    }
}
// Message-thread visible-range projection. Beat widths follow denominator;
// MIDI snap/quantize retains its existing quarter-note tick units.
template<class Emit> void visitMeterGrid(const mrs::TimeMap& map,mrs::Tick first,mrs::Tick last,mrs::Tick spacing,Emit emit){
    spacing=std::max(mrs::Tick{1},spacing);mrs::Tick start=0;
    for(std::size_t i=0;i<map.meters.size();++i){const auto& point=map.meters[i];const mrs::Tick beat=4*mrs::ppq/point.denominator,bar=beat*point.numerator;
        const auto end=i+1<map.meters.size()?start+(map.meters[i+1].bar-point.bar)*bar:mrs::max_tick;
        const auto left=std::max(first,start),right=std::min(last,end-1);if(left<=right){
            const auto beatStep=beat*std::max(mrs::Tick{1},(spacing+beat-1)/beat),barStep=bar*std::max(mrs::Tick{1},(spacing+bar-1)/bar);
            auto nextBeat=start+((left-start+beatStep-1)/beatStep)*beatStep;auto nextBar=start+((left-start+barStep-1)/barStep)*barStep;
            while(std::min(nextBeat,nextBar)<=right){const auto tick=std::min(nextBeat,nextBar);const auto local=tick-start;emit(tick,mrs::MusicalPosition{point.bar+local/bar,static_cast<int>((local%bar)/beat)+1,0});if(nextBeat==tick)nextBeat+=beatStep;if(nextBar==tick)nextBar+=barStep;}
        }start=end;if(start>last)break;
    }
}
}
