#pragma once
#include "../apps/studio-juce/src/LivePerformanceEdit.h"
#include "../apps/studio-juce/src/LivePlaybackState.h"
#include <chrono>
#include <iostream>

namespace ui::live::test {
inline void performanceModeEdits() {
    const auto check=[](bool pass,const char* message){if(!pass)throw std::runtime_error(message);};
    auto state=empty();const auto item=addItem(state,"Modes"),other=addItem(state,"Other");
    auto section=object();put(section,"id","mode-section");put(section,"name","Verse");
    put(section,"start",0.);put(section,"length",4.);put(section,"color","blue");
    put(require(state,"items",item),"sections",juce::Array<juce::var>{section});
    map(performance(state),"modes").getDynamicObject()->removeProperty("mode-section");
    performance(state).getDynamicObject()->removeProperty("modes");
    saveItem(state,item);const auto savedModes=itemMetadataDirtyState(state,item);
    const auto original=canonical(state);
    check(!performanceModePatch(state,item,item,mrs::PerformanceMode::normal),"Default Normal has no Undo transaction");
    for(const auto mode:{mrs::PerformanceMode::skip,mrs::PerformanceMode::stop,mrs::PerformanceMode::loop}){
        const auto patch=performanceModePatch(state,item,"mode-section",mode);
        const auto next=patch->apply(state,true);const auto target=validatePerformanceModeChange(state,next);
        check(target.item==item&&target.target=="mode-section"&&target.effective&&itemPerformanceMode(next,"mode-section")==mode,
              "All Section modes have the same typed, owner-validated edit path");
        check(savedModes.dirty(next,item)==itemDirty(next,item)&&savedModes.dirty(next,item),
              "Existing cards detect local mode changes without rebuilding their opaque dirty baseline");
        check(canonical(patch->apply(next,false))==original&&canonical(state)==original,
              "Mode Undo removes a newly created container and never mutates the source show");
        const auto reset=performanceModePatch(next,item,"mode-section",mrs::PerformanceMode::normal);
        const auto cleared=reset->apply(next,true);
        check(!cleared["performance"]["modes"].hasProperty("mode-section")&&
              sameShowValue(reset->apply(cleared,false),next),"Normal removes the override; Undo restores it exactly");
        check(!savedModes.dirty(cleared,item)&&!itemDirty(cleared,item),"Returning to Normal restores the existing card's saved indicator");
        check(!performanceModePatch(next,item,"mode-section",mode),"Same mode is a no-op");
        check(patchByteDelta(state,*patch,true)==editJsonBytes(next)-editJsonBytes(state),
              "Mode map creation retains the exact shared JSON byte budget");
    }
    auto patch=performanceModePatch(state,item,item,mrs::PerformanceMode::stop);
    auto next=patch->apply(state,true);
    check(validatePerformanceModeChange(state,next).item==item,"Item modes retain their own identity");
    const auto reset=performanceModePatch(next,item,item,mrs::PerformanceMode::normal);
    const auto cleared=reset->apply(next,true);
    check(validatePerformanceModeChange(next,cleared).effective,"Removing a non-default Item mode is an effective change");
    const auto reject=[&](auto action,const char* message){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,message);};
    reject([&]{performanceModePatch(state,other,"mode-section",mrs::PerformanceMode::stop);},"A Section cannot be edited through another Item");
    reject([&]{performanceModePatch(state,item,item,static_cast<mrs::PerformanceMode>(99));},"Unknown modes are rejected before history changes");
    const auto sectionPatch=performanceModePatch(next,item,"mode-section",mrs::PerformanceMode::loop);
    const auto edited=sectionPatch->apply(next,true);auto deleted=edited.clone();
    put(require(deleted,"items",item),"sections",juce::Array<juce::var>());
    reject([&]{validatePerformanceModeChange(deleted,sectionPatch->apply(deleted,false));},"Undo cannot write a mode for a deleted Section");
    auto multiple=next.clone();setPerformanceMode(multiple,item,item,mrs::PerformanceMode::loop);
    setPerformanceMode(multiple,other,other,mrs::PerformanceMode::skip);
    reject([&]{validatePerformanceModeChange(next,multiple);},"A scalar mode action cannot hide changes to several targets");
    auto explicitNormal=state.clone();put(map(performance(explicitNormal),"modes"),item,"normal");
    const auto normalPatch=performanceModePatch(explicitNormal,item,item,mrs::PerformanceMode::normal);
    check(!validatePerformanceModeChange(explicitNormal,normalPatch->apply(explicitNormal,true)).effective,
          "Removing a redundant legacy Normal override leaves runtime semantics unchanged");
    auto unrelated=state.clone();setPerformanceMode(unrelated,other,other,mrs::PerformanceMode::skip);
    check(!savedModes.dirty(unrelated,item),"A different Item's mode does not dirty this card");
}
inline void performanceModeEditTimings() {
    using Clock=std::chrono::steady_clock;
    const auto elapsed=[](auto start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();};
    for(const int payload:{0,8*1024*1024}){
        auto state=empty();const auto id=addItem(state,"Mode timing");
        if(payload)for(int i=0;i<127;++i)addItem(state,"Synthetic item "+juce::String(i));
        auto opaque=object();put(opaque,"state",juce::String::repeatedString("P",payload));put(state,"auditOpaque",opaque);
        const auto start=Clock::now();const auto bytes=editJsonBytes(state);const double cold=elapsed(start);
        std::vector<double> legacy,patch;
        for(int i=0;i<9;++i){
            auto at=Clock::now();auto cloned=state.clone();setPerformanceMode(cloned,id,id,mrs::PerformanceMode::stop);
            validate(cloned);validatePlayback(cloned);validatePerformance(cloned);legacy.push_back(elapsed(at));
            at=Clock::now();const auto edit=*performanceModePatch(state,id,id,mrs::PerformanceMode::stop);
            const auto next=edit.apply(state,true);(void)validatePerformanceModeChange(state,next);
            (void)checkedEditBytes(bytes,patchByteDelta(state,edit,true));patch.push_back(elapsed(at));
            if(!sameShowValue(cloned,next)||next["auditOpaque"].getDynamicObject()!=opaque.getDynamicObject())
                throw std::runtime_error("Performance timing paths differ or copy opaque state");
        }
        std::sort(legacy.begin(),legacy.end());std::sort(patch.begin(),patch.end());
        std::cout<<"live_mode_model_profile payload="<<payload<<" items="<<(payload?128:1)
                 <<" cold_budget_ms="<<cold<<" legacy_p50_ms="<<legacy[4]<<" legacy_p95_ms="<<legacy[8]
                 <<" warm_patch_p50_ms="<<patch[4]<<" warm_patch_p95_ms="<<patch[8]<<'\n';
    }
}
}
