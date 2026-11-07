#pragma once
#include "../apps/studio-juce/src/LiveInputState.h"

// #289 B - sub-patch overrides: effective parameters, capture diff, validation,
// item persistence and reference cleanup.
namespace ui::live::test {
inline void subPatchOverrides() {
    const auto check=[](bool pass,const char* message){if(!pass)throw std::runtime_error(message);};
    const auto checkThrows=[&](const auto& body,const char* message){
        try{body();}catch(const std::exception&){return;}
        throw std::runtime_error(message);
    };
    auto state=empty();
    const auto channel=addChannel(state,"Player","Guitar");
    const auto item=addItem(state,"Song A");
    put(state,"currentItem",item);
    mrs::NativeInsert base;
    base.id=mrs::Id{"fx-1"};
    base.kind=mrs::InsertKind::vst3;
    base.plugin_path="C:/VST3/Rig.vst3";
    base.class_id="0123456789abcdef0123456789abcdef";
    base.plugin_name="Rig";
    base.parameters={{0,0.5f},{1,0.25f}};
    base.validate();
    const auto patchId=addChannelPatch(state,require(state,"channels",channel),
                                       std::vector<mrs::NativeInsert>{base},"Clean","blue");
    auto& patchRow=require(require(state,"channels",channel),"patches",patchId);
    auto sub=object();
    put(sub,"id","sub-rhythm");
    put(sub,"name","Rhythm");
    put(sub,"color","violet");
    juce::Array<juce::var> overrides;
    auto overrideEntry=object();
    put(overrideEntry,"insert","fx-1");
    put(overrideEntry,"param",static_cast<juce::int64>(0));
    put(overrideEntry,"value",0.9);
    overrides.add(overrideEntry);
    put(sub,"overrides",overrides);
    array(patchRow,"subPatches").add(sub);
    validate(state);
    const auto trackOf=[&](juce::var& target)->juce::var& {
        for(auto& entry:array(target,"items"))
            if(entry["id"].toString()==item)
                for(auto& track:*entry["tracks"].getArray())
                    if(track["channelId"].toString()==channel)
                        return track;
        throw std::runtime_error("Missing test track");
    };
    const auto subPatch=[&](const juce::var& show)->juce::var {
        return find(find(find(show,"channels",channel),"patches",patchId),"subPatches","sub-rhythm");
    };
    const auto channelPatch=[&](const juce::var& show)->juce::var {
        return find(find(show,"channels",channel),"patches",patchId);
    };
    const auto baseInserts=decodeLivePatchInserts(state,channelPatch(state));
    const auto effective=patchInsertsWithOverrides(state,channelPatch(state),subPatch(state));
    check(effective.size()==1&&effective[0].parameters.size()==2,
          "Sub-patch keeps the captured chain and its parameter list");
    check(std::abs(effective[0].parameters[0].value-0.9f)<1.e-6f&&
          std::abs(effective[0].parameters[1].value-0.25f)<1.e-6f,
          "Sub-patch applies only its overrides on top of the base parameters");
    check(sameInsertStructure(baseInserts,effective),
          "Sub-patch switching never changes the insert chain structure");
    const auto baseOnly=patchInsertsWithOverrides(state,channelPatch(state),{});
    check(std::abs(baseOnly[0].parameters[0].value-0.5f)<1.e-6f,
          "Base restores the captured patch parameters");
    auto tweaked=baseInserts;
    tweaked[0].parameters[1].value=0.7f;
    const auto captured=subPatchOverrides(baseInserts,tweaked);
    check(captured.size()==1&&static_cast<int>(captured[0]["param"])==1&&
          std::abs(static_cast<double>(captured[0]["value"])-0.7)<1.e-5,
          "Capturing a sub-patch stores only changed parameters");
    auto same=baseInserts;
    check(subPatchOverrides(baseInserts,same).size()==0,
          "An unchanged chain produces no overrides");
    auto otherStructure=baseInserts;
    otherStructure[0].id=mrs::Id{"fx-2"};
    checkThrows([&]{subPatchOverrides(baseInserts,otherStructure);},
                "Capturing against a different chain must fail explicitly");
    put(trackOf(state),"patchId",patchId);
    put(trackOf(state),"subPatchId","sub-rhythm");
    validate(state);
    saveItem(state,item);
    check(!itemDirty(state,item),"Saved item keeps its sub-patch selection");
    put(trackOf(state),"subPatchId","");
    check(itemDirty(state,item),"Changing the sub-patch selection dirties the saved item version");
    saveItem(state,item);
    check(!itemDirty(state,item),"An item saved on Base is clean");
    put(trackOf(state),"subPatchId","sub-rhythm");
    {
        auto broken=state.clone();
        put(trackOf(broken),"subPatchId","missing-sub");
        checkThrows([&]{validate(broken);},"Missing item sub-patch reference must be rejected");
    }
    {
        auto broken=state.clone();
        auto& subs=array(require(require(broken,"channels",channel),"patches",patchId),"subPatches");
        subs.getReference(0)["overrides"].getArray()->add(subs.getReference(0)["overrides"][0]);
        checkThrows([&]{validate(broken);},"Duplicate overrides for one parameter must be rejected");
    }
    {
        auto broken=state.clone();
        auto& subs=array(require(require(broken,"channels",channel),"patches",patchId),"subPatches");
        auto entry=object();
        put(entry,"insert","fx-missing");
        put(entry,"param",static_cast<juce::int64>(0));
        put(entry,"value",0.4);
        subs.getReference(0)["overrides"].getArray()->add(entry);
        checkThrows([&]{validate(broken);},"An override targeting a missing insert must be rejected");
    }
    {
        auto broken=state.clone();
        auto& subs=array(require(require(broken,"channels",channel),"patches",patchId),"subPatches");
        subs.getReference(0)["overrides"].getArray()->getReference(0).getDynamicObject()
            ->setProperty("value",1.5);
        checkThrows([&]{validate(broken);},"An out-of-range override value must be rejected");
    }
    {
        auto duplicate=state.clone();
        auto& subs=array(require(require(duplicate,"channels",channel),"patches",patchId),"subPatches");
        auto other=object();
        put(other,"id","sub-solo");
        put(other,"name","Rhythm");
        put(other,"overrides",juce::Array<juce::var>());
        subs.add(other);
        checkThrows([&]{validate(duplicate);},"Duplicate sub-patch names must be rejected");
    }
    {
        auto legacy=state.clone();
        auto& channelRow=require(legacy,"channels",channel);
        auto demo=object();
        put(demo,"id","demo-patch");
        put(demo,"name","Legacy demo");
        put(demo,"engine",object());
        auto demoSub=object();
        put(demoSub,"id","demo-sub");
        put(demoSub,"name","Demo Rhythm");
        put(demoSub,"color","green");
        juce::Array<juce::var> demoSubs;
        demoSubs.add(demoSub);
        put(demo,"subPatches",demoSubs);
        array(channelRow,"patches").add(demo);
        validate(legacy);
        check(legacyPatch(find(channelRow,"patches","demo-patch")),
              "Legacy demo patches with sub-patches stay readable");
    }
    {
        auto edited=state.clone();
        put(trackOf(edited),"subPatchId","sub-rhythm");
        saveItem(edited,item);
        clearSubPatchReferences(edited,channel,patchId,"sub-rhythm");
        check(trackOf(edited)["subPatchId"].isVoid(),
              "Removing a sub-patch clears the item selection");
        clearLibrarySubPatchReferences(edited,channel,patchId,"sub-rhythm");
        auto& subs=array(require(require(edited,"channels",channel),"patches",patchId),"subPatches");
        int index=-1;
        for(int i=0;i<subs.size();++i)
            if(subs[i]["id"].toString()=="sub-rhythm") {index=i;break;}
        check(index>=0,"Test sub-patch exists before deletion");
        subs.remove(index);
        validate(edited);
        const auto saved=libraryItem(edited,item);
        bool savedReference=false;
        if(const auto* tracks=saved["tracks"].getArray())
            for(const auto& track:*tracks)
                if(track["channelId"].toString()==channel&&!track["subPatchId"].isVoid())
                    savedReference=true;
        check(!savedReference,"Deleting a sub-patch clears its saved Browser reference");
    }
}
} // namespace ui::live::test
