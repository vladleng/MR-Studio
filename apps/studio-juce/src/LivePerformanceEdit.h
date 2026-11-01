#pragma once
#include "LivePerformanceState.h"
#include "LiveEditProtocol.h"

namespace ui::live {
inline void requirePerformanceTarget(const juce::var& state,const juce::String& item,const juce::String& target) {
    const auto source=find(state,"items",item);
    bool found=source.isObject()&&target==item;
    if(const auto* rows=source["sections"].getArray())for(const auto& row:*rows)
        found=found||row["id"].toString()==target;
    if(!found)throw std::runtime_error("Performance target was removed");
}
inline std::optional<ValuePatch> performanceModePatch(const juce::var& state,const juce::String& item,
                                                     const juce::String& target,mrs::PerformanceMode mode) {
    requirePerformanceTarget(state,item,target);
    const auto name=performanceModeName(mode); // Validate enum even for a no-op.
    EditPath path{"performance","modes",target};
    if(mode!=mrs::PerformanceMode::normal){
        auto patch=valuePatch(state,std::move(path),name);
        if(patch.changed())return patch;
    }else{
        const auto old=editValue(state,path);
        if(old.present)return ValuePatch{std::move(path),{true,old.value.clone()},{}};
    }
    return std::nullopt;
}
struct PerformanceModeChange {juce::String item,target;bool effective{};};
// Recheck on perform AND Undo/Redo. The patch may own a newly created modes
// container, but it must change exactly one valid Item/Section scalar.
inline PerformanceModeChange validatePerformanceModeChange(const juce::var& before,const juce::var& after) {
    validatePerformance(after);
    const auto old=before["performance"]["modes"],next=after["performance"]["modes"];
    PerformanceModeChange change;
    const auto changed=[&](const juce::String& key){
        if(change.target.isNotEmpty())throw std::runtime_error("A performance edit must change exactly one target");
        change.target=key;
    };
    if(const auto* object=next.getDynamicObject())for(const auto& property:object->getProperties())
        if(!sameShowValue(property.value,old[property.name]))changed(property.name.toString());
    if(const auto* object=old.getDynamicObject())for(const auto& property:object->getProperties())
        if(!next.hasProperty(property.name))changed(property.name.toString());
    if(change.target.isEmpty())throw std::runtime_error("Performance target did not change");
    if(const auto* items=after["items"].getArray())for(const auto& item:*items){
        const auto id=item["id"].toString();bool found=id==change.target;
        if(const auto* sections=item["sections"].getArray())for(const auto& section:*sections)
            found=found||section["id"].toString()==change.target;
        if(found){change.item=id;break;}
    }
    if(change.item.isEmpty())throw std::runtime_error("Performance target was removed");
    change.effective=itemPerformanceMode(before,change.target)!=itemPerformanceMode(after,change.target);
    return change;
}
} // namespace ui::live
