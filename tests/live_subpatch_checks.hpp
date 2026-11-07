#pragma once
#include "../apps/studio-juce/src/LiveInputState.h"

// #289 B - sub-patch state variants of the same chain: effective inserts,
// capture rules, validation, item persistence and reference cleanup.
namespace ui::live::test {
inline void subPatchChainVariants() {
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
    base.component_state={std::byte{1},std::byte{2}};
    base.parameters={{0,0.5f}};
    base.validate();
    const auto patchId=addChannelPatch(state,require(state,"channels",channel),
                                       std::vector<mrs::NativeInsert>{base},"Clean","blue");
    auto& patchRow=require(require(state,"channels",channel),"patches",patchId);
    const auto trackOf=[&](juce::var& target)->juce::var& {
        for(auto& entry:array(target,"items"))
            if(entry["id"].toString()==item)
                for(auto& track:*entry["tracks"].getArray())
                    if(track["channelId"].toString()==channel)
                        return track;
        throw std::runtime_error("Missing test track");
    };
    const auto channelPatch=[&](const juce::var& show)->juce::var {
        return find(find(show,"channels",channel),"patches",patchId);
    };
    const auto subPatchOf=[&](const juce::var& show)->juce::var {
        return find(channelPatch(show),"subPatches","sub-rhythm");
    };
    const auto baseInserts=decodeLivePatchInserts(state,channelPatch(state));
    auto variant=baseInserts;
    variant[0].component_state={std::byte{9},std::byte{9}};
    variant[0].parameters[0].value=0.9f;
    check(!(baseInserts==variant),"The test variant differs from the base chain");
    auto sub=subPatchChain(state,baseInserts,variant);
    put(sub,"id","sub-rhythm");
    put(sub,"name","Rhythm");
    put(sub,"color","violet");
    array(patchRow,"subPatches").add(sub);
    validate(state);
    const auto effective=patchInsertsWithSubPatch(state,channelPatch(state),subPatchOf(state));
    check(effective.size()==1&&effective[0].component_state==variant[0].component_state&&
          std::abs(effective[0].parameters[0].value-0.9f)<1.e-6f,
          "A sub-patch applies its captured same-chain state");
    check(sameInsertStructure(effective,baseInserts),
          "Sub-patch switching never changes the insert chain structure");
    const auto baseOnly=patchInsertsWithSubPatch(state,channelPatch(state),{});
    check(baseOnly.size()==1&&baseOnly[0].component_state==base.component_state&&
          std::abs(baseOnly[0].parameters[0].value-0.5f)<1.e-6f,
          "Base restores the captured patch state");
    checkThrows([&]{subPatchChain(state,baseInserts,baseInserts);},
                "An unchanged chain must not produce a sub-patch");
    auto otherStructure=baseInserts;
    otherStructure[0].id=mrs::Id{"fx-2"};
    checkThrows([&]{subPatchChain(state,baseInserts,otherStructure);},
                "A different chain must not be captured as a sub-patch");
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
        subs.getReference(0)["inserts"].getArray()->getReference(0)
            .getDynamicObject()->setProperty("id","fx-9");
        checkThrows([&]{validate(broken);},"A sub-patch chain with a different insert must be rejected");
    }
    {
        auto broken=state.clone();
        auto& subs=array(require(require(broken,"channels",channel),"patches",patchId),"subPatches");
        const auto hash=subs.getReference(0)["inserts"][0]["state"].toString();
        check(broken["patchStates"].isObject(),"Patch state store exists");
        broken["patchStates"].getDynamicObject()->removeProperty(juce::Identifier(hash));
        checkThrows([&]{validate(broken);},"A sub-patch with missing opaque state must be rejected");
    }
    {
        auto duplicate=state.clone();
        auto& subs=array(require(require(duplicate,"channels",channel),"patches",patchId),"subPatches");
        auto other=object();
        put(other,"id","sub-solo");
        put(other,"name","Rhythm");
        put(other,"inserts",subs.getReference(0)["inserts"].clone());
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
