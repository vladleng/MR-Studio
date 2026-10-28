#pragma once
#include "../apps/studio-juce/src/LiveClipProjection.h"
namespace ui::live::test {
inline void clipEditing(){
    const auto check=[](bool pass,const char* message){if(!pass)throw std::runtime_error(message);};
    auto state=empty();const auto item=addItem(state,"Clips");put(require(state,"items",item),"tempo",120);put(require(state,"items",item),"meter","4/4");
    auto source=object();put(source,"file","test.wav");put(source,"gain",0.);put(source,"mute",false);put(source,"solo",false);
    put(map(performance(state),"nativePlayback"),item,source);
    auto rows=defaultPlaybackRegions(source,16.,2.);const auto original=canonical(rows);
    rows=splitPlaybackRegion(rows,"source",4.);const auto right=rows[1]["id"].toString();
    rows=splitPlaybackRegion(rows,right,8.);
    check(rows.size()==3 && static_cast<double>(rows[2]["offset"])==8.,"Repeated cuts preserve exact source offsets");
    rows=editPlaybackRegion(rows,right,4.,0,16.);
    check(static_cast<double>(rows[1]["start"])==8. && static_cast<double>(rows[1]["offset"])==4.,"Moving a cut region never slips its source");
    auto trimmed=editPlaybackRegion(rows,right,1.,-1,16.);
    check(static_cast<double>(trimmed[1]["start"])==9. && static_cast<double>(trimmed[1]["offset"])==5. && static_cast<double>(trimmed[1]["length"])==3.,"Left trim changes timeline and source offset together");
    trimmed=editPlaybackRegion(rows,right,100.,1,16.);
    check(static_cast<double>(trimmed[1]["length"])==12.,"Right trim clamps against actual source extent");
    setPlaybackRegions(state,item,"",rows);
    const auto layout=playbackStemLayout(state,item,playback(state,item),48000);
    check(layout.regions && layout.regions->size()==3 && layout.regions->at(1).start==768000 && layout.regions->at(1).offset==384000,
          "Authored bars project to distinct real Core regions and source offsets");
    auto channel=object();put(channel,"id","playback");put(channel,"type","Playback");
    put(state,"channels",juce::Array<juce::var>{channel});
    auto track=object();put(track,"id","browser-track");put(track,"channelId","playback");
    put(require(state,"items",item),"tracks",juce::Array<juce::var>{track});
    setPlaybackRegions(state,item,"",rows);
    check(canonical(state["performance"]["clips"]["browser-track"])==canonical(rows),
          "Browser track regions retain performance.clips mapping");
    put(require(state,"items",item),"tempo",93.);
    const auto fractional=playbackStemLayout(state,item,playback(state,item),48000);
    check(fractional.regions->at(0).offset+fractional.regions->at(0).length==fractional.regions->at(1).offset &&
          fractional.regions->at(1).offset+fractional.regions->at(1).length==fractional.regions->at(2).offset,
          "Repeated cuts at fractional sample/bar preserve contiguous source samples without rounding gaps");
    put(require(state,"items",item),"tempo",120.);
    auto reopened=juce::JSON::parse(juce::JSON::toString(state));
    check(canonical(playbackRegions(reopened,item,playback(reopened,item)))==canonical(rows),"Native-only regions survive show JSON round-trip");
    setPlaybackRegions(state,item,"",juce::Array<juce::var>());
    check(playbackStemLayout(state,item,playback(state,item),48000).regions->empty(),"Explicit empty lane stays empty, not implicit full source");
    check(canonical(defaultPlaybackRegions(source,16.,2.))==original,"Restore yields original full source independently of cut history");
    bool rejected=false;try{splitPlaybackRegion(rows,right,8.);}catch(...){rejected=true;}check(rejected,"Cuts at region boundary are rejected atomically");
}
}
