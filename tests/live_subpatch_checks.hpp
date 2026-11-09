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
    base.component_state=std::vector<std::byte>{std::byte{1},std::byte{2}};
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
    {
        const auto capturedBaseline=encodeLivePatchParameterBaseline(baseInserts);
        auto restored=baseInserts;
        restored[0].parameters.clear();
        applyLivePatchParameterBaseline(restored,capturedBaseline);
        check(restored[0].parameters==baseInserts[0].parameters,
              "#374 E3: separate baseline preserves VST3 parameters without changing opaque state");
        auto missing=restored;
        missing[0].parameters.clear();
        checkThrows([&]{applyLivePatchParameterBaseline(missing,{});},
                    "#374 E3: older Base Patch without exposed parameters requires recapture");
    }
    auto variant=baseInserts;
    variant[0].component_state=std::vector<std::byte>{std::byte{9},std::byte{9}};
    variant[0].parameters[0].value=0.9f;
    check(!(baseInserts==variant),"The test variant differs from the base chain");
    auto sub=subPatchChain(state,baseInserts,variant);
    put(sub,"id","sub-rhythm");
    put(sub,"name","Rhythm");
    put(sub,"color","violet");
    array(patchRow,"subPatches").add(sub);
    validate(state);
    const auto effective=patchInsertsWithSubPatch(state,channelPatch(state),subPatchOf(state));
    check(sub["overrides"].isArray() && sub["overrides"].size()==1 &&
          sub["inserts"].isVoid(),"New Sub-patch stores only one explicit parameter override");
    check(effective.size()==1&&effective[0].component_state==base.component_state&&
          std::abs(effective[0].parameters[0].value-0.9f)<1.e-6f,
          "A sub-patch applies exposed parameters without replacing base VST3 state");
    check(sameInsertStructure(effective,baseInserts),
          "Sub-patch switching never changes the insert chain structure");
    {
        // A real newly captured Patch keeps parameter baselines separately:
        // its native VST3 inserts contain opaque state but NO full parameter list.
        auto sparse=state.clone();
        auto& patch=live::require(live::require(sparse,"channels",channel),
                                  "patches",patchId);
        put(patch,"parameterBaseline",encodeLivePatchParameterBaseline(baseInserts));
        auto& savedInserts=*patch["inserts"].getArray();
        put(savedInserts.getReference(0),"parameters",juce::var(juce::Array<juce::var>{}));
        validate(sparse);
        const auto sparseVariant=patchInsertsWithSubPatch(sparse,
            find(find(sparse,"channels",channel),"patches",patchId),
            find(find(find(sparse,"channels",channel),"patches",patchId),
                "subPatches","sub-rhythm"));
        check(sparseVariant[0].parameters.size()==1 &&
              std::abs(sparseVariant[0].parameters[0].value-.9f)<1.e-6f,
              "#374 E3: baseline metadata validates sparse override without replaying full VST3 preset");
    }
    const auto baseOnly=patchInsertsWithSubPatch(state,channelPatch(state),{});
    check(baseOnly.size()==1&&baseOnly[0].component_state==base.component_state&&
          std::abs(baseOnly[0].parameters[0].value-0.5f)<1.e-6f,
          "Base restores the captured patch state");
    checkThrows([&]{subPatchChain(state,baseInserts,baseInserts);},
                "An unchanged chain must not produce a sub-patch");
    auto vendorPresetOnly=baseInserts;
    vendorPresetOnly[0].component_state=std::vector<std::byte>{std::byte{18},std::byte{19}};
    checkThrows([&]{subPatchChain(state,baseInserts,vendorPresetOnly);},
                "Opaque-only preset changes must not be misrepresented as Sub-patch parameters");
    // #374 F: full vendor Snapshot is a same-chain Sub-patch, not an
    // additional Patch or a duplicate Base. Both formats share library IDs.
    {
        auto autoState=state.clone();
        const auto automatic=captureSubPatchVariant(autoState,baseInserts,variant);
        check(automatic["type"].toString()=="parameters" &&
              automatic["overrides"].size()==1 && automatic["states"].isVoid(),
              "#374 F: Auto chooses sparse Parameters for exposed changes");
        const auto direct=captureSubPatchVariant(autoState,baseInserts,variant,
                                                 SubPatchCaptureMode::parameters);
        check(direct["type"].toString()=="parameters",
              "#374 F: explicitly selected Parameters remains supported");
        checkThrows([&]{captureSubPatchVariant(autoState,baseInserts,vendorPresetOnly,
                                               SubPatchCaptureMode::parameters);},
                    "#374 F: Parameters refuses opaque-only preset changes");
        auto snapshot=captureSubPatchVariant(autoState,baseInserts,vendorPresetOnly);
        check(snapshot["type"].toString()=="snapshot" &&
              snapshot["states"].size()==1 && snapshot["overrides"].isVoid(),
              "#374 F: Auto falls back to full-state Snapshot for opaque-only VST3");
        put(snapshot,"id","sub-snapshot");
        put(snapshot,"name","Preset");
        put(snapshot,"color","amber");
        auto& bank=require(require(autoState,"channels",channel),"patches",patchId);
        array(bank,"subPatches").add(snapshot);
        validate(autoState);
        auto effectiveSnapshot=patchInsertsWithSubPatch(autoState,
            find(find(autoState,"channels",channel),"patches",patchId),
            find(find(find(autoState,"channels",channel),"patches",patchId),
                 "subPatches","sub-snapshot"));
        check(effectiveSnapshot.size()==1 &&
              effectiveSnapshot[0].component_state==vendorPresetOnly[0].component_state &&
              effectiveSnapshot[0].parameters.empty() &&
              patchInsertsWithSubPatch(autoState,
                find(find(autoState,"channels",channel),"patches",patchId),{})[0].component_state==
                    baseInserts[0].component_state,
              "#374 F: Snapshot changes only selected VST3 state; Base stays intact");
        const auto hash=snapshot["states"][0]["state"].toString();
        prunePatchStates(autoState);
        check(autoState["patchStates"][juce::Identifier(hash)].isObject(),
              "#374 F: Snapshot blobs survive reference pruning");
        auto invalid=autoState.clone();
        auto& snapshots=array(require(require(invalid,"channels",channel),"patches",patchId),"subPatches");
        for(auto& v:snapshots)
            if(v["id"].toString()=="sub-snapshot") {
                auto row=v["states"][0].clone();
                put(row,"insertId","foreign-insert");
                auto list=v["states"].clone();
                list.getArray()->set(0,row);
                put(v,"states",list);
            }
        checkThrows([&]{validate(invalid);},
                    "#374 F: Snapshot cannot reference foreign VST3 insert ID");
        auto both=variant;
        auto forcedSnapshot=captureSubPatchVariant(autoState,baseInserts,both,
                                                   SubPatchCaptureMode::snapshot);
        check(forcedSnapshot["type"].toString()=="snapshot" &&
              forcedSnapshot["states"].size()==1,
              "#374 F: explicit Snapshot works when parameters and state both change");
        auto missing=autoState.clone();
        missing["patchStates"].getDynamicObject()->removeProperty(juce::Identifier(hash));
        checkThrows([&]{validate(missing);},
                    "#374 F: missing Snapshot binary data fails validation");
    }
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
        auto& overrides=*subs.getReference(0)["overrides"].getArray();
        put(overrides.getReference(0),"insertId","fx-9");
        checkThrows([&]{validate(broken);},"A Sub-patch referencing a foreign insert is rejected");
    }
    {
        auto broken=state.clone();
        auto& subs=array(require(require(broken,"channels",channel),"patches",patchId),"subPatches");
        // Legacy full-state Sub-patches must remain readable after migration,
        // including their own opaque state: pruning must not delete those blobs.
        auto& legacySub=subs.getReference(0);
        legacySub.getDynamicObject()->removeProperty("overrides");
        put(legacySub,"inserts",encodeLivePatchInserts(broken,variant));
        validate(broken);
        const auto legacyEffective=patchInsertsWithSubPatch(broken,
            find(find(broken,"channels",channel),"patches",patchId),legacySub);
        check(legacyEffective[0].component_state==variant[0].component_state,
              "Old full-state Sub-patch round-trips without losing its preset");
        const auto hash=legacySub["inserts"][0]["state"].toString();
        prunePatchStates(broken);
        check(broken["patchStates"][juce::Identifier(hash)].isObject(),
              "Pruning retains the legacy Sub-patch opaque VST3 state");
        broken["patchStates"].getDynamicObject()->removeProperty(juce::Identifier(hash));
        checkThrows([&]{validate(broken);},"Legacy Sub-patch with missing opaque state is rejected");
    }
    {
        auto broken=state.clone();
        auto& subs=array(require(require(broken,"channels",channel),"patches",patchId),"subPatches");
        auto& first=*subs.getReference(0)["overrides"].getArray();
        put(first.getReference(0),"parameterId",static_cast<juce::int64>(123456));
        checkThrows([&]{validate(broken);},
                    "Sub-patch override for an unknown base parameter fails validation");
    }
    {
        auto duplicate=state.clone();
        auto& subs=array(require(require(duplicate,"channels",channel),"patches",patchId),"subPatches");
        auto other=object();
        put(other,"id","sub-solo");
        put(other,"name","Rhythm");
        put(other,"overrides",subs.getReference(0)["overrides"].clone());
        subs.add(other);
        checkThrows([&]{validate(duplicate);},"Duplicate sub-patch names must be rejected");
    }
    {
        // #374 F: Base is immutable once variants exist. Silently rebasing it
        // would change both Parameters and Snapshot meanings in existing shows.
        auto edited=state.clone();
        auto replacement=baseInserts;
        replacement[0].component_state={std::byte{4},std::byte{5}};
        replacement[0].parameters[0].value=.3f;
        checkThrows([&]{replaceCapturedPatch(edited,channel,patchId,replacement);},
                    "#374 F: Base with Sub-patches refuses silent rebase");
        const auto preserved=find(find(edited,"channels",channel),"patches",patchId);
        check(decodeLivePatchInserts(edited,preserved)[0].component_state==
                  baseInserts[0].component_state &&
              find(preserved,"subPatches","sub-rhythm").isObject(),
              "#374 F: rejected rebase preserves original Base and child library");
        // Updating a Patch *without* any Sub-patches still captures full state.
        auto withoutSubs=state.clone();
        auto& barePatch=require(require(withoutSubs,"channels",channel),"patches",patchId);
        put(barePatch,"subPatches",juce::var(juce::Array<juce::var>{}));
        replaceCapturedPatch(withoutSubs,channel,patchId,replacement);
        const auto changedPatch=find(find(withoutSubs,"channels",channel),"patches",patchId);
        check(changedPatch["id"].toString()==patchId &&
              decodeLivePatchInserts(withoutSubs,changedPatch)[0].component_state==
                  replacement[0].component_state &&
              changedPatch["subPatches"].isArray() &&
              changedPatch["subPatches"].size()==0,
              "#374 F: Base without Sub-patches supports full-state Update");
        auto changed=baseInserts;
        changed[0].parameters[0].value=.7f;
        replaceCapturedSubPatch(edited,channel,patchId,"sub-rhythm",changed);
        const auto changedSub=find(find(find(edited,"channels",channel),"patches",patchId),"subPatches","sub-rhythm");
        check(changedSub["id"].toString()=="sub-rhythm"&&changedSub["inserts"].isVoid() &&
              changedSub["overrides"].isArray()&&changedSub["overrides"].size()==1,
              "Update Sub-patch keeps ID and captures parameter deltas only");
        validate(edited);
        checkThrows([&]{replaceCapturedSubPatch(edited,channel,patchId,"sub-rhythm",changed);},
                    "Repeated unchanged Sub-patch Update is rejected");
        auto invalid=baseInserts;
        invalid[0].id=mrs::Id{"changed-identity"};
        checkThrows([&]{replaceCapturedSubPatch(edited,channel,patchId,"sub-rhythm",invalid);},
                    "Update Sub-patch refuses changed plugin identity");
        check(find(find(state,"channels",channel),"patches",patchId)["inserts"].isArray(),
              "Update operations leave the previous snapshot intact for Undo");
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

inline void subPatchRailGeometry() {
    const auto check=[](bool condition,const char* message) {
        if(!condition)throw std::runtime_error(message);
    };
    // RED before Upd2: a first Sub-patch used to swallow the entire parent.
    auto parent=blockSpan(0.,8.);
    auto gaps=subPatchBaseGaps(parent);
    check(gaps.size()==1 && std::abs(gaps.front().first)<1.e-9 &&
          std::abs(gaps.front().second-8.)<1.e-9,
          "Empty Sub-patch rail is entirely Base");
    auto strips=addedSubPatchSpanAt(parent["subs"],8.,"rhythm",2.13);
    check(strips["rhythm"].isObject() &&
          std::abs(blockStart(strips["rhythm"])-2.25)<1.e-9 &&
          std::abs(blockLength(strips["rhythm"])-1.)<1.e-9,
          "Click inserts a one-bar Sub-patch snapped to a quarter-bar");
    put(parent,"subs",strips);
    gaps=subPatchBaseGaps(parent);
    check(gaps.size()==2 && std::abs(gaps[0].second-2.25)<1.e-9 &&
          std::abs(gaps[1].first-3.25)<1.e-9,
          "Free intervals remain Base before and after a Sub-patch");
    strips=addedSubPatchSpanAt(strips,8.,"solo",3.5);
    check(std::abs(blockStart(strips["solo"])-3.5)<1.e-9 &&
          std::abs(blockLength(strips["solo"])-1.)<1.e-9,
          "Second strip inserts at the clicked free interval");
    strips=addedSubPatchSpanAt(strips,8.,"rhythm",6.);
    check(strips.getDynamicObject()->getProperties().size()==3 &&
          strips["rhythm"].isObject(),
          "Repeated library Sub-patch uses an independent block instance");
    bool hasRepeatedLibraryReference=false;
    for(const auto& entry:strips.getDynamicObject()->getProperties())
        if(entry.name.toString()!="rhythm" &&
           subPatchLibraryId(entry.name.toString(),entry.value)=="rhythm")
            hasRepeatedLibraryReference=true;
    check(hasRepeatedLibraryReference,
          "Repeated instance references one shared library Sub-patch");
    auto nearlyFull=object();
    put(nearlyFull,"used",blockSpan(0.,7.75));
    auto tail=addedSubPatchSpanAt(nearlyFull,8.,"solo",7.75);
    check(std::abs(blockStart(tail["solo"])-7.75)<1.e-9 &&
          std::abs(blockLength(tail["solo"])-.25)<1.e-9,
          "Insertion shortens to the last available quarter-bar");
    const auto unchanged=addedSubPatchSpanAt(tail,8.,"rhythm",7.75);
    check(sameShowValue(unchanged,tail),
          "A filled rail cannot acquire an overlapping Sub-patch");
}

inline void subPatchAuthoringE() {
    const auto check=[](bool condition,const char* message){
        if(!condition)throw std::runtime_error(message);
    };
    auto spans=object();
    put(spans,"patch",blockSpan(1.,8.));
    auto cut=splitBaseSegment(spans,"patch",4.);
    const auto parent=cut["patch"];
    const auto bases=subPatchBaseSegments(parent);
    check(std::abs(blockStart(parent)-1.)<1.e-9 &&
          std::abs(blockLength(parent)-8.)<1.e-9 &&
          bases.size()==2 && std::abs(bases[1].start-4.)<1.e-9,
          "#374 E1: Base scissors split rail only, never parent Patch");
    auto moved=movedBaseCut(cut,"patch",4.,3.5);
    check(subPatchBaseSegments(moved["patch"]).size()==2 &&
          std::abs(subPatchBaseSegments(moved["patch"])[1].start-3.5)<1.e-9,
          "#374 E2: shared Base boundary moves one grid-aligned marker");
    const auto blocked=movedBaseCut(moved,"patch",3.5,8.);
    check(sameShowValue(blocked,moved),
          "#374 E2: Base boundary does not leave sub-quarter slivers");
    auto assigned=replaceBaseSegment(moved,"patch",baseSegmentId(3.5),"rhythm");
    const auto source=assigned["patch"]["subs"]["rhythm"];
    check(source.isObject() && std::abs(blockStart(source)-3.5)<1.e-9 &&
          std::abs(blockLength(source)-4.5)<1.e-9 &&
          std::abs(blockLength(assigned["patch"])-8.)<1.e-9,
          "#374 E2: replace Base with Rhythm retains exact segment length and Patch");
    auto divided=splitSubPatchBlock(assigned,"patch","rhythm",4.5);
    check(std::abs(blockLength(divided.spans["patch"])-8.)<1.e-9 &&
          std::abs(blockLength(divided.spans["patch"]["subs"]["rhythm"])-1.)<1.e-9 &&
          subPatchLibraryId(divided.rightId,
              divided.spans["patch"]["subs"][juce::Identifier(divided.rightId)])=="rhythm",
          "#374 E1: Sub-patch scissors split only the child, sharing library ID");
    auto seam=joinedSubPatchSpans(divided.spans,"patch","rhythm",divided.rightId,4.75);
    check(std::abs(blockLength(seam["patch"]["subs"]["rhythm"])-1.25)<1.e-9 &&
          std::abs(blockStart(seam["patch"]["subs"][juce::Identifier(divided.rightId)])-4.75)<1.e-9 &&
          std::abs(blockLength(seam["patch"])-8.)<1.e-9,
          "#374 E2: shared Sub-patch seam resizes both children, never parent Patch");
    auto replaced=replaceSubPatchSegment(divided.spans,"patch","rhythm","solo");
    const auto changed=replaced["patch"]["subs"]["rhythm"];
    check(std::abs(blockStart(changed)-3.5)<1.e-9 &&
          std::abs(blockLength(changed)-1.)<1.e-9 &&
          subPatchLibraryId("rhythm",changed)=="solo" &&
          subPatchLibraryId(divided.rightId,
             replaced["patch"]["subs"][juce::Identifier(divided.rightId)])=="rhythm",
          "#374 E2: replace Rhythm with Solo without moving or changing sibling");
    check(sameShowValue(spans,spans.clone()),
          "#374 E2: model states remain replayable for local Undo");
}
} // namespace ui::live::test
