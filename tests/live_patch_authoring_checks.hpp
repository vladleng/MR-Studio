#pragma once
#include "../apps/studio-juce/src/LiveInputState.h"

// #289 C - free patch/sub-patch block geometry on an item track: placement,
// neighbour rules, parent scaling and validation.
namespace ui::live::test {
inline void patchBlockAuthoring() {
    const auto check=[](bool pass,const char* message){if(!pass)throw std::runtime_error(message);};
    const auto checkThrows=[&](const auto& body,const char* message){
        try{body();}catch(const std::exception&){return;}
        throw std::runtime_error(message);
    };
    auto spans=object();
    put(spans,"a",blockSpan(0,4));
    put(spans,"b",blockSpan(4,4));
    const auto added=addedPatchSpan(spans,12,"c",12);
    check(std::abs(blockStart(added["c"])-8)<1.e-9 && std::abs(blockLength(added["c"])-4)<1.e-9,
          "A new patch block uses the first free interval");
    check(sameShowValue(addedPatchSpan(spans,8,"c",8),spans),
          "A full item rejects a new patch block instead of overlapping");
    const auto movedLeft=editedPatchSpans(spans,12,"b",1,4,false,true);
    check(std::abs(blockStart(movedLeft["b"])-4)<1.e-9,
          "A body move stops at the previous neighbour");
    const auto movedRight=editedPatchSpans(spans,12,"b",5,4,false,true);
    check(std::abs(blockStart(movedRight["b"])-5)<1.e-9,
          "A body move keeps its length in free space");
    const auto pushed=editedPatchSpans(spans,12,"a",0,6,true);
    check(std::abs(blockStart(pushed["b"])-6)<1.e-9 && std::abs(blockLength(pushed["b"])-2)<1.e-9,
          "Pushing a shared edge resizes the neighbour");
    const auto clamped=editedPatchSpans(spans,12,"a",0,6,false);
    check(std::abs(blockLength(clamped["a"])-4)<1.e-9,
          "A clamped resize keeps the neighbour boundary");
    auto withSub=spans.clone();
    auto a=withSub["a"];
    auto subs=object();
    put(subs,"s",blockSpan(1,2));
    put(a,"subs",subs);
    put(withSub,"a",a);
    const auto scaled=editedPatchSpans(withSub,12,"a",0,8,false);
    check(std::abs(blockStart(scaled["a"]["subs"]["s"])-2)<1.e-9 &&
          std::abs(blockLength(scaled["a"]["subs"]["s"])-4)<1.e-9,
          "Parent resize scales nested sub-patch spans proportionally");
    const auto subMoved=editedSubPatchSpan(withSub,"a","s",3,2,true);
    check(std::abs(blockStart(subMoved["a"]["subs"]["s"])-2)<1.e-9,
          "A sub-patch body move stays inside its parent");
    const auto subResized=editedSubPatchSpan(withSub,"a","s",1,9,false);
    check(std::abs(blockLength(subResized["a"]["subs"]["s"])-3)<1.e-9,
          "A sub-patch resize is clamped to the parent length");
    check(!removedPatchSpan(spans,"a")["a"].isObject(),
          "Removing a patch block removes its geometry");
    auto state=empty();
    const auto channel=addChannel(state,"Player","Guitar");
    const auto item=addItem(state,"Song A");
    put(state,"currentItem",item);
    mrs::NativeInsert fx;
    fx.id=mrs::Id{"fx-1"};
    fx.kind=mrs::InsertKind::vst3;
    fx.plugin_path="C:/VST3/Rig.vst3";
    fx.class_id="0123456789abcdef0123456789abcdef";
    fx.plugin_name="Rig";
    fx.component_state=std::vector<std::byte>{std::byte{1}};
    fx.validate();
    const auto patchId=addChannelPatch(state,require(state,"channels",channel),
                                       std::vector<mrs::NativeInsert>{fx},"Clean","blue");
    auto& patchRow=require(require(state,"channels",channel),"patches",patchId);
    auto sub=object();
    put(sub,"id","sub-rhythm");
    put(sub,"name","Rhythm");
    put(sub,"inserts",patchRow["inserts"].clone());
    array(patchRow,"subPatches").add(sub);
    const auto trackOf=[&](juce::var& target)->juce::var& {
        for(auto& entry:array(target,"items"))
            if(entry["id"].toString()==item)
                for(auto& track:*entry["tracks"].getArray())
                    if(track["channelId"].toString()==channel)
                        return track;
        throw std::runtime_error("Missing test track");
    };
    auto& track=trackOf(state);
    auto geometry=object();
    auto block=blockSpan(0,4);
    auto blockSubs=object();
    put(blockSubs,"sub-rhythm",blockSpan(0,2));
    put(block,"subs",blockSubs);
    put(geometry,patchId,block);
    put(track,"spans",geometry);
    put(track,"patchOrder",juce::Array<juce::var>{patchId});
    validate(state);
    check(number(track["spans"][juce::Identifier(patchId)]["length"]),
          "Authored patch geometry validates");
    {
        auto broken=state.clone();
        const auto second=addChannelPatch(broken,require(broken,"channels",channel),
                                          std::vector<mrs::NativeInsert>{fx},"Second","teal");
        auto spansRow=trackOf(broken)["spans"]; // Shares the cloned object with the show state.
        put(spansRow,juce::Identifier(second),blockSpan(2,4));
        put(trackOf(broken),"patchOrder",juce::Array<juce::var>{patchId,second});
        checkThrows([&]{validate(broken);},
                    "Overlapping patch blocks must be rejected");
    }
    {
        auto broken=state.clone();
        auto spansRow=trackOf(broken)["spans"]; // Shares the cloned object with the show state.
        put(spansRow,juce::Identifier(patchId),blockSpan(0,999));
        checkThrows([&]{validate(broken);},"A patch block beyond the item must be rejected");
    }
    {
        auto broken=state.clone();
        auto spansRow=trackOf(broken)["spans"]; // Shares the cloned object with the show state.
        auto stretched=blockSpan(0,4);
        auto subSpans=object();
        put(subSpans,"sub-rhythm",blockSpan(3,2));
        put(stretched,"subs",subSpans);
        put(spansRow,juce::Identifier(patchId),stretched);
        checkThrows([&]{validate(broken);},"A sub-patch strip beyond its parent must be rejected");
    }
    {
        auto broken=state.clone();
        auto spansRow=trackOf(broken)["spans"]; // Shares the cloned object with the show state.
        auto block2=blockSpan(0,4);
        auto ghostSubs=object();
        put(ghostSubs,"ghost",blockSpan(0,1));
        put(block2,"subs",ghostSubs);
        put(spansRow,juce::Identifier(patchId),block2);
        checkThrows([&]{validate(broken);},"A sub-patch strip without a library sub-patch must be rejected");
    }
    {
        auto broken=state.clone();
        put(trackOf(broken),"patchOrder",juce::Array<juce::var>{"ghost"});
        checkThrows([&]{validate(broken);},"Patch order entries must reference the channel library");
    }
}
} // namespace ui::live::test
