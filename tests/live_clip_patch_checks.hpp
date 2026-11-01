#pragma once
#include "../apps/studio-juce/src/LiveClipEdit.h"
#include "../apps/studio-juce/src/LiveClipProjection.h"
#include <chrono>
#include <iostream>

namespace ui::live::test {
inline void clipRegionPatches() {
    const auto check=[](bool pass,const char* message){if(!pass)throw std::runtime_error(message);};
    auto state=empty();addChannel(state,"Playback","Backing");const auto id=addItem(state,"Region patches");
    auto source=object();put(source,"file","synthetic.wav");put(source,"gain",0.);put(source,"mute",false);put(source,"solo",false);
    put(map(performance(state),"nativePlayback"),id,source);saveItem(state,id);
    const auto clean=itemMetadataDirtyState(state,id);const auto original=canonical(state);
    auto rows=defaultPlaybackRegions(source,16.,playbackSecondsPerBar(state,id));rows=splitPlaybackRegion(rows,"source",4.);
    const auto patch=playbackRegionPatch(state,id,{},rows);const auto split=patch.apply(state,true);
    check(playbackRegions(split,id,playback(split,id)).size()==2&&canonical(state)==original,
          "Browser-track split is a detached region patch, never a mutation of the original show");
    check(clean.dirty(split,id)&&clean.dirty(split,id)==itemDirty(split,id),"Existing saved dots detect local clip changes");
    check(canonical(patch.apply(split,false))==original&&!clean.dirty(patch.apply(split,false),id),
          "Region Undo restores exact parent absence and clean saved dot");
    check(patchByteDelta(state,patch,true)==editJsonBytes(split)-editJsonBytes(state),"Region list creation obeys the exact byte budget");
    auto retimed=split.clone();put(require(retimed,"items",id),"tempo",165.);
    const auto visible=playbackRegions(retimed,id,playback(retimed,id));
    check(!playbackRegionPatch(retimed,id,{},visible).changed(),"Opening/applying an unchanged retimed region does not re-encode its source clock");
    const auto raw=storedPlaybackRegions(retimed,id,playback(retimed,id));
    auto trimmed=editPlaybackRegion(visible,visible[1]["id"].toString(),-1.,1,22.);
    const auto trimPatch=playbackRegionPatch(retimed,id,{},trimmed);const auto after=trimPatch.apply(retimed,true);
    check(sameShowValue(storedPlaybackRegions(after,id,playback(after,id))[0],raw[0]),
          "Trimming one retimed region keeps every untouched region's exact stored source clock");
    auto native=empty();const auto nativeId=addItem(native,"Native source");
    put(map(performance(native),"nativePlayback"),nativeId,source.clone());
    auto bare=defaultPlaybackRegions(source,16.,2.);bare[0].getDynamicObject()->removeProperty("audioSecondsPerBar");
    auto nativeSource=playback(native,nativeId);put(nativeSource,"regions",bare);
    const auto legacyBefore=canonical(native);const auto legacyView=playbackRegions(native,nativeId,playback(native,nativeId));
    const auto cut=splitPlaybackRegion(legacyView,"source",4.);
    const auto legacyPatch=playbackRegionPatch(native,nativeId,{},cut);const auto legacy=legacyPatch.apply(native,true);
    const auto layout=playbackStemLayout(legacy,nativeId,playback(legacy,nativeId),48000);
    check(layout.regions->size()==2&&layout.regions->at(0).legacy_silent_tail&&layout.regions->at(1).legacy_silent_tail,
          "Cuts of old source-bounded audio retain compatibility on both new regions");
    check(canonical(native)==legacyBefore&&sameShowValue(legacyPatch.apply(legacy,false),native),
          "Legacy region views and Undo do not silently rewrite the saved source data");
    auto reopened=juce::JSON::parse(juce::JSON::toString(after,false,17));
    check(sameShowValue(after,reopened),"Trimmed region clocks and arrays survive JSON Save/Open");
    (void)clean.dirty(split,id);
    const auto modeSubset=clean.cachedModeMetadata,clipSubset=clean.cachedClipMetadata;
    const auto note=valuePatch(split,{"items",EditStep::id(id),"notes"},"Notes stay responsive").apply(split,true);
    (void)clean.dirty(note,id);
    check(clean.cachedModeMetadata.getDynamicObject()==modeSubset.getDynamicObject()&&clean.cachedClipMetadata.getDynamicObject()==clipSubset.getDynamicObject(),
          "Notes do not rescan mode or clip maps after local region edits");
}
inline void clipRegionEditTimings() {
    using Clock=std::chrono::steady_clock;
    const auto elapsed=[](auto at){return std::chrono::duration<double,std::milli>(Clock::now()-at).count();};
    for(const int payload:{0,8*1024*1024}){
        auto state=empty();addChannel(state,"Playback","Backing");const auto id=addItem(state,"Clip timing");
        if(payload)for(int i=0;i<127;++i)addItem(state,"Synthetic item "+juce::String(i));
        auto source=object();put(source,"file","synthetic.wav");put(source,"gain",0.);put(source,"mute",false);put(source,"solo",false);
        put(map(performance(state),"nativePlayback"),id,source);
        auto opaque=object();put(opaque,"state",juce::String::repeatedString("P",payload));put(state,"auditOpaque",opaque);
        const auto rows=splitPlaybackRegion(defaultPlaybackRegions(source,16.,2.),"source",4.);
        const auto coldAt=Clock::now();const auto bytes=editJsonBytes(state);const double cold=elapsed(coldAt);
        std::vector<double> full,local;
        for(int i=0;i<9;++i){
            auto at=Clock::now();auto legacy=state.clone();setPlaybackRegions(legacy,id,{},rows);validate(legacy);validatePlayback(legacy);full.push_back(elapsed(at));
            at=Clock::now();const auto patch=playbackRegionPatch(state,id,{},rows);const auto next=patch.apply(state,true);
            validatePlayback(next);(void)checkedEditBytes(bytes,patchByteDelta(state,patch,true));local.push_back(elapsed(at));
            if(!sameShowValue(next,legacy)||next["auditOpaque"].getDynamicObject()!=opaque.getDynamicObject())
                throw std::runtime_error("Region timing paths differ or copy opaque state");
        }
        std::sort(full.begin(),full.end());std::sort(local.begin(),local.end());
        std::cout<<"live_clip_model_profile payload="<<payload<<" items="<<(payload?128:1)<<" cold_budget_ms="<<cold
                 <<" legacy_p50_ms="<<full[4]<<" legacy_p95_ms="<<full[8]<<" local_p50_ms="<<local[4]<<" local_p95_ms="<<local[8]<<'\n';
    }
}
}
