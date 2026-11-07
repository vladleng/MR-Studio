#pragma once
#include "../apps/studio-juce/src/LiveInputState.h"

// #289 A - channel patch library: captured chain snapshots, the shared opaque
// state store, per-item default assignment and saved-version dirty behaviour.
namespace ui::live::test {
inline void channelPatchLibrary() {
    const auto check=[](bool pass,const char* message){if(!pass)throw std::runtime_error(message);};
    const auto checkThrows=[&](const auto& body,const char* message){
        try{body();}catch(const std::exception&){return;}
        throw std::runtime_error(message);
    };
    auto state=empty();
    const auto channel=addChannel(state,"Player","Guitar");
    const auto item=addItem(state,"Song A");
    put(state,"currentItem",item);
    const auto makeVst3=[](const char* id,std::vector<std::byte> component) {
        mrs::NativeInsert fx;fx.id=mrs::Id{id};fx.kind=mrs::InsertKind::vst3;
        fx.plugin_path="C:/VST3/Rig.vst3";
        fx.class_id="0123456789abcdef0123456789abcdef";
        fx.plugin_name="Rig";
        fx.component_state=std::move(component);
        fx.parameters={{7,0.5f}};
        fx.validate();
        return fx;
    };
    auto chain=std::vector<mrs::NativeInsert>{
        makeVst3("fx-1",std::vector<std::byte>(4,std::byte{1})),
        makeVst3("fx-2",std::vector<std::byte>(8,std::byte{2}))};
    mrs::NativeInsert gain;gain.id=mrs::Id{"gain-1"};gain.kind=mrs::InsertKind::gain;gain.gain=.8f;
    chain.push_back(std::move(gain));
    const auto trackOf=[&](juce::var& target)->juce::var& {
        for(auto& entry:array(target,"items"))
            if(entry["id"].toString()==item)
                for(auto& track:*entry["tracks"].getArray())
                    if(track["channelId"].toString()==channel)
                        return track;
        throw std::runtime_error("Missing test track");
    };
    auto& row=require(state,"channels",channel);
    const auto clean=addChannelPatch(state,row,chain,"Clean","blue");
    check(!clean.isEmpty(),"Captured chain becomes a channel patch");
    const auto patch=find(row,"patches",clean);
    check(patch["version"]==1&&patch["inserts"].size()==3,
          "Patch keeps version and the complete insert chain");
    check(state["patchStates"].getDynamicObject()->getProperties().size()==2,
          "Opaque VST3 state is stored once per unique chain");
    const auto decoded=decodeLivePatchInserts(state,patch);
    check(decoded.size()==3&&decoded[0].component_state==chain[0].component_state&&
          decoded[0].parameters==chain[0].parameters&&decoded[2].kind==mrs::InsertKind::gain&&
          std::abs(decoded[2].gain-.8f)<1.e-6f,
          "Patch decode restores VST3 state, parameters and native gain inserts");
    const auto second=addChannelPatch(state,row,chain,"Clean copy","teal");
    check(state["patchStates"].getDynamicObject()->getProperties().size()==2,
          "Identical chains share one opaque state blob");
    const auto lead=addChannelPatch(state,row,
        std::vector<mrs::NativeInsert>{makeVst3("fx-3",std::vector<std::byte>(12,std::byte{3}))},"Lead","amber");
    check(state["patchStates"].getDynamicObject()->getProperties().size()==3,
          "A different chain adds exactly one shared state blob");
    mrs::Track track{mrs::Id{"live-input-test"},"Guitar",mrs::TrackKind::audio,{}};
    track.input=0;track.hardware_outputs={};
    const auto profile=liveInputProfile(track,"Input 1","Live PCM test",chain);
    const auto applied=patchedLiveProfile(profile,find(row,"patches",lead));
    check(applied["inserts"].size()==1&&applied["input"]==profile["input"]&&
          applied["deviceName"]==profile["deviceName"]&&
          applied["mix"]["gain"]==profile["mix"]["gain"]&&
          canonical(applied["outputs"])==canonical(profile["outputs"]),
          "Applying a patch never changes channel input, device, mix or routing");
    put(trackOf(state),"patchId",clean);
    validate(state);
    saveItem(state,item);
    check(!itemDirty(state,item),"Saved item keeps its default channel patch");
    check(channelItemTrack(state,item,channel)["patchId"].toString()==clean,
          "Item track exposes the default channel patch");
    put(trackOf(state),"patchId",second);
    check(itemDirty(state,item),"Changing the item default patch marks the item dirty");
    saveItem(state,item);
    check(!itemDirty(state,item),"A re-saved item with another default patch is clean");
    {
        auto broken=state.clone();
        put(trackOf(broken),"patchId","missing-patch");
        checkThrows([&]{validate(broken);},"A stale item patch reference must be rejected");
    }
    {
        auto broken=state.clone();
        auto& bank=array(require(broken,"channels",channel),"patches");
        put(bank.getReference(0),"name",bank.getReference(1)["name"]);
        checkThrows([&]{validate(broken);},"Duplicate channel patch names must be rejected");
    }
    {
        auto broken=state.clone();
        auto& bank=array(require(broken,"channels",channel),"patches");
        put(bank.getReference(0),"color","chartreuse");
        checkThrows([&]{validate(broken);},"Unknown channel patch colour must be rejected");
    }
    {
        auto broken=state.clone();
        auto& bank=array(require(broken,"channels",channel),"patches");
        const auto hash=bank.getReference(0)["inserts"][0]["state"].toString();
        broken["patchStates"].getDynamicObject()->removeProperty(juce::Identifier(hash));
        checkThrows([&]{validate(broken);},"Missing opaque patch state must be rejected");
        checkThrows([&]{decodeLivePatchInserts(broken,bank.getReference(0));},
                    "Applying a patch with missing state must fail explicitly");
    }
    {
        auto legacy=state.clone();
        auto& bank=array(require(legacy,"channels",channel),"patches");
        auto demo=object();
        put(demo,"id","demo-patch");put(demo,"name","Legacy demo");
        put(demo,"engine",object());put(demo,"subPatches",juce::Array<juce::var>());
        bank.add(demo);
        validate(legacy);
        check(legacyPatch(bank.getReference(bank.size()-1)),"Legacy engine patches are detected");
        checkThrows([&]{decodeLivePatchInserts(legacy,bank.getReference(bank.size()-1));},
                    "Legacy patches cannot be applied as a captured chain");
    }
    {
        auto edited=state.clone();
        auto& bank=array(require(edited,"channels",channel),"patches");
        clearChannelPatchReferences(edited,channel,second);
        prunePatchStates(edited);
        check(trackOf(edited)["patchId"].isVoid(),
              "Clearing a patch also clears the tracked item default");
        while(array(require(edited,"channels",channel),"patches").size()>0) {
            auto& remaining=array(require(edited,"channels",channel),"patches");
            const auto id=remaining[0]["id"].toString();
            clearChannelPatchReferences(edited,channel,id);
            remaining.remove(0);
            prunePatchStates(edited);
        }
        validate(edited);
        check(edited["patchStates"].isVoid()||
              edited["patchStates"].getDynamicObject()->getProperties().isEmpty(),
              "Unreferenced opaque patch state is released after every patch is removed");
    }
}
} // namespace ui::live::test
