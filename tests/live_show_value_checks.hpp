#pragma once
#include "../apps/studio-juce/src/LiveShowState.h"

namespace ui::live::test {
inline void showValueEquality() {
    const auto check=[](bool pass,const char* message){if(!pass)throw std::runtime_error(message);};
    const auto saved=juce::JSON::parse(R"({"items":[{"id":"song","notes":"Before","tasks":[{"done":false}]}],"empty":[],"pluginState":"AQIDBA=="})");
    // JUCE 9 arrays also report isObject(), but have no DynamicObject.
    check(saved["items"].isArray()&&saved["items"].isObject()&&saved["items"].getDynamicObject()==nullptr,
          "Regression fixture exercises JUCE array/object type overlap");
    check(sameShowValue(saved,saved),"An unchanged show must compare equal to itself, including arrays");
    check(sameShowValue(saved,saved.clone()),"Deep-cloned saved show starts clean");
    const auto reopened=juce::JSON::parse(juce::JSON::toString(saved));
    check(sameShowValue(saved,reopened)&&firstShowDifference(saved,reopened).isEmpty(),
          "JSON Save/Open preserves clean equality and has no differing path");
    auto edited=saved.clone();
    put(edited["items"].getArray()->getReference(0),"notes","After");
    check(!sameShowValue(edited,saved)&&firstShowDifference(edited,saved).startsWith("$.items[0].notes"),
          "Notes edit inside an array is dirty and reports its actual leaf path");
    put(edited["items"].getArray()->getReference(0),"notes","Before");
    check(sameShowValue(edited,saved)&&firstShowDifference(edited,saved).isEmpty(),
          "Restoring Notes returns to clean without rewriting the saved baseline");
    auto reordered=juce::JSON::parse("[1,2]");
    check(!sameShowValue(reordered,juce::JSON::parse("[2,1]")),"Array ordering remains significant");
    check(!sameShowValue(juce::Array<juce::var>{},object()),"Empty array and empty object remain distinct");
    check(firstShowDifference(juce::Array<juce::var>{},object()).contains("array type differs"),
          "Array/object mismatch diagnostics describe the actual container type");
    edited=saved.clone();put(edited,"pluginState","AQIDBQ==");
    check(!sameShowValue(edited,saved),"Opaque plugin state changes remain dirty");
}
inline void notesDirtyRoundTrip() {
    const auto check=[](bool pass,const char* message){if(!pass)throw std::runtime_error(message);};
    auto state=empty();const auto id=addItem(state,"Notes dirty fixture");
    auto item=require(state,"items",id);put(item,"notes","Saved note");
    // Opaque content must remain significant without being compared on each key.
    put(item,"opaqueState",juce::String::repeatedString("opaque-",4096));
    saveItem(state,id);
    const auto clean=itemNotesDirtyState(state,id);
    check(!clean.dirty(state,id),"Notes baseline starts clean");
    for(const auto* text:{"A","A longer note","Saved note"}) {
        put(item,"notes",text);
        check(clean.dirty(state,id)==itemDirty(state,id),
              "Cached Notes dirty dot equals canonical item comparison, including return to saved text");
    }
    put(item,"name","Other changed content");
    const auto changed=itemNotesDirtyState(state,id);
    put(item,"notes","Draft note");
    check(changed.dirty(state,id),"Notes do not clear another dirty field");
    put(item,"notes","Saved note");
    check(changed.dirty(state,id)&&itemDirty(state,id),"Notes return cannot hide an unrelated item edit");
    saveItem(state,id);
    const auto resaved=itemNotesDirtyState(state,id);
    check(!resaved.dirty(state,id),"Saving rebuilds the Notes dirty baseline");
    put(item,"opaqueState","Changed opaque state");
    check(itemNotesDirtyState(state,id).dirty(state,id),"Opaque state remains part of non-Notes comparison");
    auto unsaved=empty();const auto fresh=addItem(unsaved,"Unsaved");
    map(unsaved,"library").getDynamicObject()->removeProperty(juce::Identifier(fresh));
    check(itemNotesDirtyState(unsaved,fresh).dirty(unsaved,fresh),"Missing saved item remains dirty");
}

}
