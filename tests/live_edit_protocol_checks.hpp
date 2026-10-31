#pragma once
#include "../apps/studio-juce/src/LiveEditProtocol.h"
namespace ui::live::test {
inline void editProtocol() {
    const auto check=[](bool condition,const char* message){if(!condition)throw std::runtime_error(message);};
    auto state=empty();const auto id=addItem(state,"Protocol");
    auto opaque=object();put(opaque,"preset",juce::String::repeatedString("opaque-preset-",32768));
    put(state,"opaque",opaque);
    const EditPath notes{"items",EditStep::id(id),"notes"};
    auto patch=valuePatch(state,notes,juce::String::fromUTF8("Notes: Привет\n\"quoted\"\\path"));
    juce::String controls;
    for(int code=1;code<32;++code)controls+=juce::String::charToString(static_cast<juce::juce_wchar>(code));
    controls+=juce::String::fromUTF8("\"\\ Привет \xe2\x80\xa8 \xf0\x9f\x8e\xb8");
    for(const juce::var& value:juce::Array<juce::var>{controls,juce::JSON::parse("[null,true,1.25,{},[\"x\"]]")})
        for(std::size_t depth=0;depth<8;++depth)
            check(editJsonBytes(value,depth)==static_cast<std::int64_t>(juce::JSON::toString(value,
                juce::JSON::FormatOptions().withIndentLevel(static_cast<int>(depth*2))).getNumBytesAsUTF8()),
                "Fast size accounting matches JUCE for every control escape, Unicode, arrays and indentation");
    const auto beforeBytes=editJsonBytes(state);
    check(beforeBytes==static_cast<std::int64_t>(juce::JSON::toString(state).getNumBytesAsUTF8()),
          "Streaming JSON byte count matches the actual JUCE serializer");
    auto edited=patch.apply(state,true);
    check(find(state,"items",id)["notes"].toString().isEmpty(),"Patch never mutates its input/snapshot");
    check(edited["opaque"].getDynamicObject()==state["opaque"].getDynamicObject(),
          "Path copy retains the untouched opaque subtree");
    check(edited["channels"].getArray()==state["channels"].getArray(),"Path copy retains unrelated channel state");
    check(editJsonBytes(edited)-beforeBytes==patchByteDelta(state,patch,true),"Notes JSON delta includes UTF-8 and escaping exactly");
    check(sameShowValue(patch.apply(edited,false),state),"Undo returns to the exact original show");
    auto stale=replaceEditValue(state,notes,{true,"A concurrent edit"});bool rejected=false;
    try{patch.apply(stale,true);}catch(const std::exception&){rejected=true;}
    check(rejected&&find(stale,"items",id)["notes"].toString()=="A concurrent edit","Stale field cannot overwrite a newer value");
    const auto second=addItem(state,"Second");
    auto reordered=shallowEditCopy(state);
    auto items=*state["items"].getArray();items.swap(0,1);put(reordered,"items",items);
    edited=patch.apply(reordered,true);
    check(find(edited,"items",id)["notes"].toString()==patch.after.value.toString()&&
          find(edited,"items",second)["notes"].toString().isEmpty(),"Stable IDs survive array reordering");
    for(const auto& path:std::vector<EditPath>{{"performance","newMap","child","name"},
             {"performance","newMap"},{"items",EditStep::id(id),"optional"}}) {
        for(const juce::var& value:juce::Array<juce::var>{juce::var(),true,juce::JSON::parse("{\"rows\":[{\"name\":\"a\"},{\"name\":\"b\"}]}"),juce::Array<juce::var>{}}) {
            const auto change=valuePatch(state,path,value);const auto next=change.apply(state,true);
            check(editJsonBytes(next)-editJsonBytes(state)==patchByteDelta(state,change,true),
                  "Property creation delta includes keys, commas and nesting indentation");
            check(editJsonBytes(state)-editJsonBytes(next)==patchByteDelta(next,change,false),"Property removal delta is exact");
            check(sameShowValue(change.apply(next,false),state),"Undo removes newly-created parents and distinguishes absent/null");
        }
    }
    check(!valuePatch(state,notes,find(state,"items",id)["notes"]).changed(),"Unchanged input creates no edit");
    check(checkedEditBytes(maximumShowBytes-1,1)==maximumShowBytes,"Exact 64 MiB boundary is allowed");
    rejected=false;try{checkedEditBytes(maximumShowBytes,1);}catch(const std::exception&){rejected=true;}
    check(rejected,"Growing beyond 64 MiB is rejected");
    EditRevisions revisions;revisions.publish(EditImpact::notes);
    check(revisions.mutation==1&&revisions.timeline==0&&revisions.cards==0&&revisions.metadata==0,
          "Notes notification never invalidates layout or structural cards");
    revisions.publish(EditImpact::metadata|EditImpact::paint);
    check(revisions.metadata==1&&revisions.paint==1&&revisions.timeline==0&&revisions.cards==0,
          "Metadata notification repaints without rebuilding geometry/cards");
    EditByteBudget budget;EditStamp stamp;
    budget.remember(stamp,123);
    check(budget.get(stamp)==123,"Byte budget is available only for its exact stamp");
    for(int domain=0;domain<6;++domain) {
        auto changed=stamp;
        switch(domain){case 0:++changed.generation;break;case 1:++changed.structure;break;
            case 2:++changed.mix;break;case 3:++changed.fold;break;case 4:++changed.playback;break;default:++changed.patch;break;}
        check(!budget.get(changed),"Every persisted edit domain invalidates a stale byte count");
    }
    const auto containerPatch=valuePatch(state,{"performance","newContainer"},juce::JSON::parse("{\"value\":1}"));
    auto containerModel=containerPatch.apply(state,true);
    auto mutableLeaf=containerModel["performance"]["newContainer"];
    put(mutableLeaf,"value",2);
    check(static_cast<int>(containerPatch.after.value["value"])==1,"Mutable model leaves never alias Undo snapshots");
    saveItem(state,id);auto baseline=itemMetadataDirtyState(state,id);
    for(const char* key:{"name","key","notes"}) {
        const auto changed=valuePatch(state,{"items",EditStep::id(id),key},"Changed").apply(state,true);
        check(baseline.dirty(changed,id)==itemDirty(changed,id),"Bounded metadata dirty index agrees with canonical saved content");
    }
}
}
