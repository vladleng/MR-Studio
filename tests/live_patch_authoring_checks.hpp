#pragma once
#include "../apps/studio-juce/src/LiveInputState.h"
#include "../apps/studio-juce/src/LiveEditProtocol.h"

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
    // #374 A: dragging an individual Patch edge must never push, overlap or
    // shrink a sibling. Snap threshold is in lane pixels converted to bars.
    const auto unpushed=editedPatchSpans(spans,12,"a",0,6,false);
    check(std::abs(blockStart(unpushed["b"])-4)<1.e-9 &&
          std::abs(blockLength(unpushed["b"])-4)<1.e-9,
          "Individual edge resize cannot mutate the neighbouring Patch");
    auto withGap=object();
    put(withGap,"a",blockSpan(0,3));
    put(withGap,"b",blockSpan(4,2));
    const auto magnetLeft=magneticPatchEdge(withGap,"b",3.13,2.87,false,true,.2);
    check(magnetLeft.snapped&&std::abs(magnetLeft.start-3.)<1.e-9 &&
          std::abs(magnetLeft.length-3.)<1.e-9,
          "The left edge magnetically snaps to the end of its preceding block");
    const auto snappedLeft=editedPatchSpans(withGap,12,"b",
        magnetLeft.start,magnetLeft.length,false);
    check(std::abs(blockStart(snappedLeft["b"])-3.)<1.e-9 &&
          std::abs(blockLength(snappedLeft["a"])-3.)<1.e-9,
          "Magnetic snap closes a gap without changing the neighbour");
    const auto magnetRight=magneticPatchEdge(withGap,"a",0,3.88,false,false,.2);
    check(magnetRight.snapped&&std::abs(magnetRight.length-4.)<1.e-9,
          "Right-edge resize snaps to the next Patch boundary");
    const auto magnetMove=magneticPatchEdge(withGap,"b",3.1,2.,true,false,.2);
    check(magnetMove.snapped&&std::abs(magnetMove.start-3.)<1.e-9,
          "Moving a Patch snaps its body to the neighbouring block");
    const auto noMagnet=magneticPatchEdge(withGap,"b",3.5,2.,true,false,.2);
    check(!noMagnet.snapped&&std::abs(noMagnet.start-3.5)<1.e-9,
          "Moving away from a boundary does not snap");
    const auto sharedSeam=editedPatchSpans(spans,12,"b",3.5,4.5,false);
    check(std::abs(blockStart(sharedSeam["b"])-4.)<1.e-9 &&
          std::abs(blockLength(sharedSeam["a"])-4.)<1.e-9,
          "A shared edge remains independent and cannot overlap the preceding Patch");
    // #374 Upd1: upper-half remains independent, lower-half seam changes
    // BOTH blocks in one model value without touching the library ID.
    const auto joined=joinedPatchSpans(spans,"a","b",5.);
    check(std::abs(blockLength(joined["a"])-5.)<1.e-9 &&
          std::abs(blockStart(joined["b"])-5.)<1.e-9 &&
          std::abs(blockLength(joined["b"])-3.)<1.e-9 &&
          std::abs(blockStart(joined["a"])+blockLength(joined["a"])-
                   blockStart(joined["b"]))<1.e-9,
          "Joined seam changes both neighbouring Patch lengths without gaps or overlaps");
    check(std::abs(blockLength(spans["a"])-4.)<1.e-9 &&
          std::abs(blockStart(spans["b"])-4.)<1.e-9,
          "Preview of joined seam cannot mutate its source geometry");
    const auto joinedMax=joinedPatchSpans(spans,"a","b",20.);
    check(std::abs(blockLength(joinedMax["b"])-.25)<1.e-9,
          "Joined seam clamps the right Patch to at least a quarter-bar");
    const auto joinedMin=joinedPatchSpans(spans,"a","b",-20.);
    check(std::abs(blockLength(joinedMin["a"])-.25)<1.e-9,
          "Joined seam clamps the left Patch to at least a quarter-bar");
    checkThrows([&]{joinedPatchSpans(withGap,"a","b",3.5);},
                "Joined seam is unavailable across a gap");
    auto withSub=spans.clone();
    auto a=withSub["a"];
    auto subs=object();
    put(subs,"s",blockSpan(1,2));
    put(a,"subs",subs);
    put(withSub,"a",a);
    const auto joinedWithSub=joinedPatchSpans(withSub,"a","b",5.);
    check(std::abs(blockStart(joinedWithSub["a"]["subs"]["s"])-1.25)<1.e-9 &&
          std::abs(blockLength(joinedWithSub["a"]["subs"]["s"])-2.5)<1.e-9 &&
          std::abs(blockLength(joinedWithSub["b"])-3.)<1.e-9,
          "Joined seam proportionally keeps child strip geometry and the right sibling");
    checkThrows([&]{joinedPatchSpans(withSub,"b","a",5.);},
                "Joined seam rejects reversed neighbours");
    // Parent resize pushes the touched neighbour, otherwise the sibling boundary would clamp the
    // requested length and the proportional scaling could not be observed.
    const auto scaled=editedPatchSpans(withSub,12,"a",0,8,true);
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
    // Scissors give each fragment a private timeline ID, but keep the same
    // shared library reference. Parent splits preserve Sub-patch geometry.
    const auto cut=splitPatchBlock(withSub,juce::Array<juce::var>{"a","b"},"a",2.);
    check(cut.rightId!="a"&&cut.order.size()==3 &&
          cut.order[1].toString()==cut.rightId &&
          patchLibraryId(cut.rightId,cut.spans[juce::Identifier(cut.rightId)])=="a" &&
          std::abs(blockLength(cut.spans["a"])-2.)<1.e-9 &&
          std::abs(blockStart(cut.spans[juce::Identifier(cut.rightId)])-2.)<1.e-9,
          "Patch Scissors create two independently identified spans referencing one library Patch");
    check(std::abs(blockLength(cut.spans["a"]["subs"]["s"])-1.)<1.e-9 &&
          std::abs(blockStart(cut.spans[juce::Identifier(cut.rightId)]["subs"]["s"]))<1.e-9 &&
          std::abs(blockLength(cut.spans[juce::Identifier(cut.rightId)]["subs"]["s"])-1.)<1.e-9,
          "Patch cut splits a crossing Sub-patch strip without losing its library reference");
    checkThrows([&]{splitPatchBlock(withSub,juce::Array<juce::var>{"a","b"},"a",.1);},
                "Scissors never create a too-short Patch fragment");
    const auto subCut=splitSubPatchBlock(withSub,"a","s",2.);
    check(subCut.rightId!="s" &&
          subPatchLibraryId(subCut.rightId,
              subCut.spans["a"]["subs"][juce::Identifier(subCut.rightId)])=="s" &&
          std::abs(blockLength(subCut.spans["a"]["subs"]["s"])-1.)<1.e-9 &&
          std::abs(blockStart(subCut.spans["a"]["subs"][juce::Identifier(subCut.rightId)])-2.)<1.e-9,
          "Sub-patch Scissors preserve two independently editable strips of one library variant");
    const auto movedSubFragment=editedSubPatchSpan(subCut.spans,"a",subCut.rightId,2.5,1.,true);
    check(subPatchLibraryId(subCut.rightId,
              movedSubFragment["a"]["subs"][juce::Identifier(subCut.rightId)])=="s" &&
          std::abs(blockStart(movedSubFragment["a"]["subs"][juce::Identifier(subCut.rightId)])-2.5)<1.e-9,
          "Moving a split Sub-patch fragment preserves its library ID");

    auto state=empty();
    const auto channel=addChannel(state,"Player","Guitar");
    const auto item=addItem(state,"Song A");
    put(state,"currentItem",item);
    // Items carry their bar length; the geometry checks need a known item span.
    for(auto& entry:array(state,"items"))
        if(entry["id"].toString()==item)
            put(entry,"bars",12);
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
        auto sliced=state.clone();
        auto& row=trackOf(sliced);
        const auto originalSpans=row["spans"];
        const auto oldOrder=row["patchOrder"];
        const auto pieces=splitPatchBlock(originalSpans,oldOrder,patchId,2.);
        put(row,"spans",pieces.spans);
        put(row,"patchOrder",pieces.order);
        validate(sliced);
        check(sameShowValue(originalSpans,track["spans"]) &&
              patchLibraryId(pieces.rightId,row["spans"][juce::Identifier(pieces.rightId)])==patchId,
              "Scissors preserve old Item geometry and reference the captured Patch without cloning VST3");
        auto splitSub=splitSubPatchBlock(row["spans"],patchId,"sub-rhythm",1.);
        put(row,"spans",splitSub.spans);
        validate(sliced);
        const auto json=juce::JSON::toString(sliced,false,17);
        juce::var reopened;
        check(juce::JSON::parse(json,reopened).wasOk(),"Split Patch geometry is valid JSON");
        validate(reopened);
        check(sameShowValue(reopened,sliced),
              "Split Patch/Sub-patch instances survive a complete Show JSON round-trip");
        const auto undoValue=valuePatch(state,{"items",EditStep::id(item),
                                          "tracks",EditStep::id(row["id"].toString()),"spans"},
                                       row["spans"]);
        const auto advanced=undoValue.apply(state,true);
        check(advanced["channels"].getArray()==state["channels"].getArray(),
              "Scissors edit never clones channel-owned opaque VST3 state");
        const auto restored=undoValue.apply(advanced,false);
        check(sameShowValue(channelItemTrack(restored,item,channel)["spans"],track["spans"]),
              "One local Undo can restore pre-cut span geometry");
        clearSubPatchReferences(sliced,channel,patchId,"sub-rhythm");
        const auto afterSubRemoval=channelItemTrack(sliced,item,channel)["spans"];
        check(!afterSubRemoval[juce::Identifier(patchId)]["subs"]["sub-rhythm"].isObject() &&
              !afterSubRemoval[juce::Identifier(pieces.rightId)]["subs"]["sub-rhythm"].isObject(),
              "Deleting one library Sub-patch cleans original and split strip references");
        clearChannelPatchReferences(sliced,channel,patchId);
        const auto removedTrack=channelItemTrack(sliced,item,channel);
        check(removedTrack["patchOrder"].size()==0 &&
              removedTrack["spans"].getDynamicObject()->getProperties().isEmpty(),
              "Deleting one library Patch removes all independently split instances");
        validate(sliced);
    }

    {
        // #374 B: two independent references to the same library sound can be
        // assigned another captured Patch without changing time or track order.
        auto grouped=state.clone();
        auto& groupedChannel=require(grouped,"channels",channel);
        const auto replacementId=addChannelPatch(grouped,groupedChannel,
            std::vector<mrs::NativeInsert>{fx},"Solo","violet");
        auto& groupedTrack=trackOf(grouped);
        auto nextBlock=blockSpan(4,2);
        put(nextBlock,"patchId",patchId);
        auto blocks=groupedTrack["spans"].clone();
        put(blocks,"second-instance",nextBlock);
        put(groupedTrack,"spans",blocks);
        put(groupedTrack,"patchOrder",juce::Array<juce::var>{patchId,"second-instance"});
        validate(grouped);
        const auto reassigned=assignedPatchBlocks(groupedTrack["spans"],
            std::vector<juce::String>{patchId,"second-instance"},replacementId);
        check(reassigned[juce::Identifier(patchId)]["patchId"].toString()==replacementId &&
              reassigned["second-instance"]["patchId"].toString()==replacementId &&
              std::abs(blockStart(reassigned["second-instance"])-4.)<1.e-9 &&
              std::abs(blockLength(reassigned["second-instance"])-2.)<1.e-9 &&
              reassigned[juce::Identifier(patchId)]["subs"].isVoid(),
              "Reassignment switches two library references while retaining their timeline geometry");
        check(groupedTrack["spans"]["second-instance"]["patchId"].toString()==patchId,
              "Group assignment cannot mutate the original model before Undo commit");
        const auto groupEdit=valuePatch(grouped,
            {"items",EditStep::id(item),"tracks",EditStep::id(groupedTrack["id"].toString()),"spans"},
            reassigned);
        const auto advanced=groupEdit.apply(grouped,true);
        validate(advanced);
        check(sameShowValue(channelItemTrack(advanced,item,channel)["patchOrder"],
                            groupedTrack["patchOrder"]),
              "Group assignment preserves the order of selected blocks");
        const auto restored=groupEdit.apply(advanced,false);
        check(sameShowValue(channelItemTrack(restored,item,channel)["spans"],groupedTrack["spans"]),
              "One Undo restores both group-selected Patch assignments");
        check(groupedTrack["spans"][juce::Identifier(patchId)]["patchId"].isVoid(),
              "Group assignment leaves the original library block reference untouched before commit");
    }
    // Regression for #374: committing geometry through the established local
    // edit protocol must keep the channel bank and its large VST3 state shared,
    // rather than cloning the entire show on every gesture.
    const auto trackId=track["id"].toString();
    check(trackId.isNotEmpty(),"Patch authoring uses a stable track row ID");
    const auto originalChannels=state["channels"].getArray();
    const auto resized=editedPatchSpans(track["spans"],12,patchId,0,5,false);
    const auto geometryEdit=valuePatch(state,{"items",EditStep::id(item),
                                              "tracks",EditStep::id(trackId),"spans"},resized);
    check(geometryEdit.changed(),"Patch resize produces a bounded local edit");
    const auto afterGeometry=geometryEdit.apply(state,true);
    check(afterGeometry["channels"].getArray()==originalChannels &&
          std::abs(blockLength(channelItemTrack(afterGeometry,item,channel)["spans"][juce::Identifier(patchId)])-5.)<1.e-9,
          "Patch gesture retains untouched channel/VST3 state and updates only spans");
    const auto undoneGeometry=geometryEdit.apply(afterGeometry,false);
    check(sameShowValue(channelItemTrack(undoneGeometry,item,channel)["spans"],track["spans"]),
          "Patch geometry Undo restores the original span map");

    {
        auto broken=state.clone();
        auto& brokenTrack=trackOf(broken);
        const auto second=addChannelPatch(broken,require(broken,"channels",channel),
                                          std::vector<mrs::NativeInsert>{fx},"Second","teal");
        auto spansRow=brokenTrack["spans"];
        put(spansRow,juce::Identifier(second),blockSpan(2,4));
        put(brokenTrack,"spans",spansRow);
        put(brokenTrack,"patchOrder",juce::Array<juce::var>{patchId,second});
        checkThrows([&]{validate(broken);},
                    "Overlapping patch blocks must be rejected");
    }
    {
        auto broken=state.clone();
        auto& brokenTrack=trackOf(broken);
        auto spansRow=brokenTrack["spans"];
        put(spansRow,juce::Identifier(patchId),blockSpan(20,2));
        put(brokenTrack,"spans",spansRow);
        checkThrows([&]{validate(broken);},"A patch block starting outside the item must be rejected");
    }
    {
        auto broken=state.clone();
        auto& brokenTrack=trackOf(broken);
        auto spansRow=brokenTrack["spans"];
        auto stretched=blockSpan(0,4);
        auto subSpans=object();
        put(subSpans,"sub-rhythm",blockSpan(3,2));
        put(stretched,"subs",subSpans);
        put(spansRow,juce::Identifier(patchId),stretched);
        put(brokenTrack,"spans",spansRow);
        checkThrows([&]{validate(broken);},"A sub-patch strip beyond its parent must be rejected");
    }
    {
        auto broken=state.clone();
        auto& brokenTrack=trackOf(broken);
        auto spansRow=brokenTrack["spans"];
        auto block2=blockSpan(0,4);
        auto ghostSubs=object();
        put(ghostSubs,"ghost",blockSpan(0,1));
        put(block2,"subs",ghostSubs);
        put(spansRow,juce::Identifier(patchId),block2);
        put(brokenTrack,"spans",spansRow);
        checkThrows([&]{validate(broken);},"A sub-patch strip without a library sub-patch must be rejected");
    }
    {
        auto broken=state.clone();
        put(trackOf(broken),"patchOrder",juce::Array<juce::var>{"ghost"});
        checkThrows([&]{validate(broken);},"Patch order entries must reference the channel library");
    }
}
} // namespace ui::live::test
