#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <juce_core/juce_core.h>
#include <map>
#include <set>
#include <stdexcept>

// Show authoring data only. No transport, audio, plugin or device implementation.
namespace ui::live {
inline juce::var object() { return juce::var(new juce::DynamicObject); }
inline juce::var &property(juce::var &value, const juce::Identifier &key) {
    auto *o = value.getDynamicObject();
    if (!o)
        throw std::runtime_error("Expected show object");
    if (!o->hasProperty(key))
        o->setProperty(key, juce::var());
    return *o->getProperties().getVarPointer(key);
}
inline void put(juce::var &value, const juce::Identifier &key, juce::var entry) {
    if (!value.isObject())
        value = object();
    value.getDynamicObject()->setProperty(key, std::move(entry));
}
inline juce::var &map(juce::var &value, const juce::Identifier &key) {
    auto &entry = property(value, key);
    if (!entry.isObject())
        entry = object();
    return entry;
}
inline juce::Array<juce::var> &array(juce::var &value, const juce::Identifier &key) {
    auto &entry = property(value, key);
    if (!entry.isArray())
        entry = juce::Array<juce::var>();
    return *entry.getArray();
}
inline juce::var find(const juce::var &value, const juce::Identifier &key, const juce::String &id) {
    if (const auto *entries = value[juce::Identifier(key)].getArray())
        for (const auto &entry : *entries)
            if (entry["id"].toString() == id)
                return entry;
    return {};
}
inline juce::var &require(juce::var &value, const juce::Identifier &key, const juce::String &id) {
    for (auto &entry : array(value, key))
        if (entry["id"].toString() == id)
            return entry;
    throw std::runtime_error("Show item no longer exists");
}
inline juce::String freshId() { return juce::Uuid().toString(); }
inline bool named(const juce::var &value, int limit = 120) {
    return value.isString() && value.toString().trim().isNotEmpty() && value.toString().length() <= limit;
}
inline bool number(const juce::var &value) {
    return (value.isInt() || value.isInt64() || value.isDouble()) && std::isfinite(static_cast<double>(value));
}
inline bool status(const juce::String &value) {
    return value == "Todo" || value == "Wip" || value == "Wait" || value == "Done";
}
inline juce::var sorted(const juce::var &value) {
    if (const auto *source = value.getDynamicObject()) {
        auto result = object();
        juce::StringArray keys;
        for (const auto &pair : source->getProperties())
            keys.add(pair.name.toString());
        keys.sort(false);
        for (const auto &key : keys)
            put(result, key, sorted(value[juce::Identifier(key)]));
        return result;
    }
    if (const auto *entries = value.getArray()) {
        juce::Array<juce::var> result;
        for (const auto &entry : *entries)
            result.add(sorted(entry));
        return result;
    }
    return value;
}
inline juce::String canonical(const juce::var &value) { return juce::JSON::toString(sorted(value), true, 17); }
// Equality on the in-memory show tree, preserving all fields (including
// opaque VST3 state) without repeatedly JSON-encoding multi-megabyte strings.
// This is used for UI dirty checks and no-op detection, not for file hashing.
inline bool sameShowValue(const juce::var& left,const juce::var& right) {
    // JUCE arrays also report isObject(); handle arrays before DynamicObject.
    if(left.isArray()||right.isArray()) {
        const auto* a=left.getArray(),*b=right.getArray();
        if(!a||!b||a->size()!=b->size())return false;
        for(int i=0;i<a->size();++i)
            if(!sameShowValue(a->getReference(i),b->getReference(i)))return false;
        return true;
    }
    if(left.isObject()||right.isObject()) {
        const auto* a=left.getDynamicObject(),*b=right.getDynamicObject();
        if(!a||!b||a->getProperties().size()!=b->getProperties().size())return false;
        for(const auto& property:a->getProperties())
            if(!b->hasProperty(property.name)||
               !sameShowValue(property.value,b->getProperty(property.name)))return false;
        return true;
    }
    // JUCE var::clone() may normalize the storage type of a scalar while
    // preserving its JSON value. Strict equalsWithSameType can then mark a
    // newly cloned clean show dirty and prevent Undo from returning to clean.
    // Use var value equality for leaves, retaining recursive exact property
    // and array comparisons above (including opaque VST3 Base64 strings).
    return left==right;
}

// Diagnostics for preserving saved-show equality without re-encoding large VST3
// state blobs. Returns only the first differing path and short leaf summaries.
inline juce::String firstShowDifference(const juce::var& a,const juce::var& b,
                                        const juce::String& path="$") {
    if(a.isArray()||b.isArray()) {
        const auto* aa=a.getArray(),*bb=b.getArray();
        if(!aa||!bb)return path+" (array type differs: current="+
            juce::String(a.isArray()?"array":a.isObject()?"object":a.isVoid()?"void":"scalar")+
            ", saved="+juce::String(b.isArray()?"array":b.isObject()?"object":b.isVoid()?"void":"scalar")+")";
        if(aa->size()!=bb->size())
            return path+" (array count "+juce::String(aa->size())+" vs "+juce::String(bb->size())+")";
        for(int i=0;i<aa->size();++i) {
            const auto result=firstShowDifference(aa->getReference(i),bb->getReference(i),
                                                  path+"["+juce::String(i)+"]");
            if(result.isNotEmpty())return result;
        }
        return {};
    }
    if(a.isObject()||b.isObject()) {
        const auto* ao=a.getDynamicObject(),*bo=b.getDynamicObject();
        if(!ao||!bo)return path+" (object type differs: current="+
            juce::String(a.isArray()?"array":a.isObject()?"object":a.isVoid()?"void":"scalar")+
            ", saved="+juce::String(b.isArray()?"array":b.isObject()?"object":b.isVoid()?"void":"scalar")+")";
        if(ao->getProperties().size()!=bo->getProperties().size())
            return path+" (property count "+juce::String(ao->getProperties().size())+
                   " vs "+juce::String(bo->getProperties().size())+")";
        for(const auto& prop:ao->getProperties()) {
            if(!bo->hasProperty(prop.name))return path+"."+prop.name.toString()+" (missing)";
            const auto result=firstShowDifference(prop.value,bo->getProperty(prop.name),
                                                  path+"."+prop.name.toString());
            if(result.isNotEmpty())return result;
        }
        return {};
    }
    if(!(a==b))return path+" (scalar "+a.toString().substring(0,35)+" vs "+
                      b.toString().substring(0,35)+")";
    return {};
}
inline juce::var performance(const juce::var &state) { return state["performance"]; }
inline juce::var &performance(juce::var &state) { return map(state, "performance"); }
inline juce::var workflow(const juce::var &state, const juce::String &id) {
    auto value = state["performance"]["itemWorkflow"][juce::Identifier(id)];
    if (!value.isObject())
        value = object();
    else
        value = value.clone();
    if (!value.hasProperty("status"))
        put(value, "status", "Todo");
    if (!value.hasProperty("priority"))
        put(value, "priority", "P4");
    if (!value.hasProperty("progress"))
        put(value, "progress", 0);
    return value;
}
inline juce::var tasks(const juce::var &state, const juce::String &id) {
    auto value = state["performance"]["itemTasks"][juce::Identifier(id)];
    return value.isArray() ? value : juce::var(juce::Array<juce::var>());
}
inline double taskProgress(const juce::var &entries, const juce::var &parent = {}, int depth = 0) {
    if (depth > 8)
        return 0;
    double sum = 0;
    int count = 0;
    if (const auto *list = entries.getArray()) {
        for (const auto &entry : *list)
            if (entry["parent"] == parent) {
                sum += taskProgress(entries, entry["id"], depth + 1);
                ++count;
            }
        if (count)
            return juce::roundToInt(sum / count);
        for (const auto &entry : *list)
            if (entry["id"] == parent)
                return entry["status"].toString() == "Done" ? 100. : static_cast<double>(entry["progress"]);
    }
    return 0;
}
inline int progress(const juce::var &state, const juce::String &id) {
    const auto entries = tasks(state, id);
    return entries.size() ? juce::roundToInt(taskProgress(entries))
                          : juce::roundToInt(static_cast<double>(workflow(state, id)["progress"]));
}
// #289 A - channel patch library. A patch is a frozen snapshot of the channel's
// Live insert chain. Opaque VST3 component/controller state lives once in the
// show-wide `patchStates` map and is referenced by hash, so identical plugin
// state is never duplicated across patches or saved Browser versions.
inline constexpr int maximumChannelPatches = 64;
inline constexpr int maximumPatchInserts = 8;
inline constexpr int maximumSubPatches = 32;
inline constexpr int maximumSubPatchOverrides = 128;
inline constexpr std::size_t maximumPatchStates = 512;
inline constexpr std::size_t maximumPatchStateBase64 = ((16u * 1024u * 1024u + 2u) / 3u) * 4u;
inline constexpr std::array<const char *, 6> patchColours{"blue", "violet", "teal", "graphite", "amber", "red"};
inline bool patchColour(const juce::String &value) {
    return std::any_of(patchColours.begin(), patchColours.end(),
                       [&](const auto *colour) { return value == juce::String(colour); });
}
inline bool hexHash(const juce::String &value) {
    if (value.length() != 32)
        return false;
    for (int i = 0; i < 32; ++i) {
        const auto c = value[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}
// Older demo shows stored an engine description instead of a captured chain.
// Such patches stay visible but cannot be applied until their chain is captured.
inline bool legacyPatch(const juce::var &patch) {
    return patch.isObject() && !patch["inserts"].isArray();
}
inline juce::var channelItemTrack(const juce::var &state, const juce::String &item,
                                  const juce::String &channel) {
    const auto row = find(state, "items", item);
    if (const auto *tracks = row["tracks"].getArray())
        for (const auto &track : *tracks)
            if (track["channelId"].toString() == channel)
                return track;
    return {};
}
// #289 C: free patch/sub-patch block geometry on an item track. Times are in
// bars relative to the item start; sub-patch spans are relative to their parent
// patch. One gesture produces one new spans object, committed as one Undo.
inline double blockStart(const juce::var& span,double fallback=0.) {
    return number(span["start"])?static_cast<double>(span["start"]):fallback;
}
inline double blockLength(const juce::var& span,double fallback=.25) {
    return number(span["length"])?juce::jmax(.25,static_cast<double>(span["length"])):fallback;
}
inline juce::var blockSpan(double start,double length) {
    auto span=object();
    put(span,"start",juce::jmax(0.,start));
    put(span,"length",juce::jmax(.25,length));
    return span;
}
// Authored timeline blocks have their own IDs. Older shows used the
// library Patch/Sub-patch ID as the span key; that remains valid. Scissors
// allocate a NEW block key while retaining a reference to the same library
// sound, without copying any opaque VST3 state.
inline juce::String patchLibraryId(const juce::String& blockId,const juce::var& span) {
    const auto id=span["patchId"].toString();
    return id.isNotEmpty()?id:blockId;
}
inline juce::String subPatchLibraryId(const juce::String& blockId,const juce::var& span) {
    const auto id=span["subPatchId"].toString();
    return id.isNotEmpty()?id:blockId;
}
struct PatchBlockSplit {
    juce::var spans,order;
    juce::String rightId;
};
inline PatchBlockSplit splitPatchBlock(const juce::var& spans,const juce::var& order,
                                       const juce::String& blockId,double cutBar) {
    const auto original=spans[juce::Identifier(blockId)];
    if(!original.isObject() || !order.isArray())
        throw std::runtime_error("Cannot split missing Patch block");
    const double start=blockStart(original),length=blockLength(original),end=start+length;
    if(!std::isfinite(cutBar) || cutBar-start<.25-1.e-7 || end-cutBar<.25-1.e-7)
        throw std::runtime_error("Cut must leave at least one quarter-bar on each side");
    auto result=spans.clone();
    auto newOrder=order.clone();
    auto* ids=newOrder.getArray();
    int at=-1;
    for(int i=0;i<ids->size();++i)
        if((*ids)[i].toString()==blockId){at=i;break;}
    if(at<0)throw std::runtime_error("Patch block order no longer matches geometry");
    auto left=original.clone(),right=original.clone();
    put(left,"length",cutBar-start);
    put(right,"start",cutBar);
    put(right,"length",end-cutBar);
    put(right,"patchId",patchLibraryId(blockId,original));
    if(const auto* children=original["subs"].getDynamicObject()) {
        auto leftSubs=object(),rightSubs=object();
        const double splitOffset=cutBar-start;
        for(const auto& pair:children->getProperties()) {
            if(!pair.value.isObject())continue;
            auto subLeft=pair.value.clone(),subRight=pair.value.clone();
            const double subStart=blockStart(pair.value);
            const double subEnd=subStart+blockLength(pair.value);
            if(subStart<splitOffset-1.e-7) {
                if(subEnd>splitOffset+1.e-7)
                    put(subLeft,"length",splitOffset-subStart);
                put(leftSubs,pair.name,subLeft);
            }
            if(subEnd>splitOffset+1.e-7) {
                put(subRight,"start",juce::jmax(0.,subStart-splitOffset));
                if(subStart<splitOffset-1.e-7)
                    put(subRight,"length",subEnd-splitOffset);
                put(rightSubs,pair.name,subRight);
            }
            if((subStart<splitOffset-1.e-7 && subEnd>splitOffset+1.e-7) &&
               ((splitOffset-subStart)<.25-1.e-7 || (subEnd-splitOffset)<.25-1.e-7))
                throw std::runtime_error("Move scissors outside the last quarter-bar of a Sub-patch");
        }
        put(left,"subs",leftSubs);put(right,"subs",rightSubs);
    }
    if(const auto* cuts=original["baseCuts"].getArray()) {
        juce::Array<juce::var> leftCuts,rightCuts;
        const double splitOffset=cutBar-start;
        for(const auto& mark:*cuts) {
            const auto at=static_cast<double>(mark);
            if(at<splitOffset-1.e-7)leftCuts.add(at);
            else if(at>splitOffset+1.e-7)rightCuts.add(at-splitOffset);
        }
        put(left,"baseCuts",juce::var(leftCuts));
        put(right,"baseCuts",juce::var(rightCuts));
    }
    const auto rightId=freshId();
    put(result,juce::Identifier(blockId),left);
    put(result,juce::Identifier(rightId),right);
    ids->insert(at+1,rightId);
    return {result,newOrder,rightId};
}
struct SubPatchBlockSplit {
    juce::var spans;
    juce::String rightId;
};
inline SubPatchBlockSplit splitSubPatchBlock(const juce::var& spans,const juce::String& patchBlockId,
                                             const juce::String& subBlockId,double cutOffset) {
    const auto span=spans[juce::Identifier(patchBlockId)];
    const auto original=span["subs"][juce::Identifier(subBlockId)];
    if(!span.isObject()||!original.isObject())
        throw std::runtime_error("Cannot split missing Sub-patch strip");
    const double start=blockStart(original),end=start+blockLength(original);
    if(!std::isfinite(cutOffset)||cutOffset-start<.25-1.e-7||end-cutOffset<.25-1.e-7)
        throw std::runtime_error("Cut must leave at least one quarter-bar on each side");
    auto result=spans.clone();
    auto parent=result[juce::Identifier(patchBlockId)];
    auto subs=parent["subs"].clone();
    auto left=original.clone(),right=original.clone();
    put(left,"length",cutOffset-start);
    put(right,"start",cutOffset);
    put(right,"length",end-cutOffset);
    put(right,"subPatchId",subPatchLibraryId(subBlockId,original));
    const auto rightId=freshId();
    put(subs,juce::Identifier(subBlockId),left);
    put(subs,juce::Identifier(rightId),right);
    put(parent,"subs",subs);
    put(result,juce::Identifier(patchBlockId),parent);
    return {result,rightId};
}
inline std::vector<std::pair<juce::String,juce::var>> orderedSpans(const juce::var& spans) {
    std::vector<std::pair<juce::String,juce::var>> rows;
    if(const auto* values=spans.getDynamicObject())
        for(const auto& pair:values->getProperties()) {
            auto value=pair.value;
            if(value.isObject())rows.emplace_back(pair.name.toString(),value);
        }
    std::sort(rows.begin(),rows.end(),[&](const auto& left,const auto& right){
        return blockStart(left.second)<blockStart(right.second);});
    return rows;
}
// The unoccupied part of the embedded Sub-patch rail always uses the
// parent Patch's Base parameters. It is derived, never stored in the Show.
inline std::vector<std::pair<double,double>> subPatchBaseGaps(const juce::var& patchSpan) {
    std::vector<std::pair<double,double>> gaps;
    const double parentLength=blockLength(patchSpan);
    double cursor=0.;
    for(const auto& child:orderedSpans(patchSpan["subs"])) {
        const double start=juce::jlimit(0.,parentLength,blockStart(child.second));
        if(start>cursor+1.e-7)gaps.emplace_back(cursor,start-cursor);
        cursor=juce::jmax(cursor,juce::jmin(parentLength,start+blockLength(child.second)));
    }
    if(cursor<parentLength-1.e-7)gaps.emplace_back(cursor,parentLength-cursor);
    return gaps;
}
// Base is a derived, state-free segment. Optional split markers merely persist
// editorial divisions; no VST3 state, graph or extra sound is created.
inline juce::String baseSegmentId(double start) {
    return "$base:"+juce::String(start,6);
}
inline bool isBaseSegmentId(const juce::String& id) {
    return id.startsWith("$base:");
}
struct BaseSegment {
    double start{},length{};
    juce::String id;
};
inline std::vector<BaseSegment> subPatchBaseSegments(const juce::var& parent) {
    std::vector<BaseSegment> result;
    std::vector<double> cuts;
    if(const auto* entries=parent["baseCuts"].getArray())
        for(const auto& cut:*entries)
            if(number(cut))cuts.push_back(static_cast<double>(cut));
    std::sort(cuts.begin(),cuts.end());
    for(const auto& gap:subPatchBaseGaps(parent)) {
        const double end=gap.first+gap.second;
        double cursor=gap.first;
        for(const double cut:cuts)
            if(cut>cursor+1.e-7 && cut<end-1.e-7) {
                result.push_back({cursor,cut-cursor,baseSegmentId(cursor)});
                cursor=cut;
            }
        if(end>cursor+1.e-7)
            result.push_back({cursor,end-cursor,baseSegmentId(cursor)});
    }
    return result;
}
inline juce::var splitBaseSegment(const juce::var& spans,const juce::String& patchId,double offset) {
    const auto parent=spans[juce::Identifier(patchId)];
    if(!parent.isObject() || !std::isfinite(offset))
        throw std::runtime_error("Cannot split missing Base segment");
    bool found=false;
    for(const auto& base:subPatchBaseSegments(parent))
        if(offset-base.start>=.25-1.e-7 &&
           base.start+base.length-offset>=.25-1.e-7){found=true;break;}
    if(!found)throw std::runtime_error("Move scissors inside Base; keep 1/4 bar on each side");
    auto result=spans.clone();
    auto updated=result[juce::Identifier(patchId)].clone();
    juce::Array<juce::var> cuts;
    if(const auto* existing=updated["baseCuts"].getArray())
        cuts=*existing;
    cuts.add(offset);
    put(updated,"baseCuts",juce::var(cuts));
    put(result,juce::Identifier(patchId),updated);
    return result;
}
inline juce::var movedBaseCut(const juce::var& spans,const juce::String& patchId,
                              double oldOffset,double newOffset) {
    auto result=spans.clone();
    auto parent=result[juce::Identifier(patchId)].clone();
    const auto* oldCuts=parent["baseCuts"].getArray();
    if(!oldCuts)throw std::runtime_error("Base cut no longer exists");
    auto cuts=*oldCuts;
    int target=-1;
    for(int i=0;i<cuts.size();++i)
        if(std::abs(static_cast<double>(cuts[i])-oldOffset)<1.e-7){target=i;break;}
    if(target<0)throw std::runtime_error("Base cut no longer exists");
    // Never move through an existing Sub-patch or leave a sliver below 1/4 bar.
    auto draft=parent.clone();
    auto without=cuts;
    without.remove(target);
    put(draft,"baseCuts",juce::var(without));
    bool allowed=false;
    for(const auto& base:subPatchBaseSegments(draft))
        if(oldOffset>base.start+1.e-7 && oldOffset<base.start+base.length-1.e-7 &&
           newOffset>=base.start+.25-1.e-7 &&
           newOffset<=base.start+base.length-.25+1.e-7){allowed=true;break;}
    if(!allowed)return spans.clone();
    cuts.set(target,newOffset);
    put(parent,"baseCuts",juce::var(cuts));
    put(result,juce::Identifier(patchId),parent);
    return result;
}
inline juce::var replaceBaseSegment(const juce::var& spans,const juce::String& patchId,
                                    const juce::String& baseId,const juce::String& subLibraryId) {
    auto result=spans.clone();
    auto parent=result[juce::Identifier(patchId)].clone();
    if(!parent.isObject() || subLibraryId.isEmpty())
        throw std::runtime_error("Missing Base segment or Sub-patch library reference");
    auto subs=parent["subs"].isObject()?parent["subs"].clone():object();
    for(const auto& segment:subPatchBaseSegments(parent))
        if(segment.id==baseId) {
            const auto key=subs[juce::Identifier(subLibraryId)].isObject()?freshId():subLibraryId;
            auto child=blockSpan(segment.start,segment.length);
            if(key!=subLibraryId)put(child,"subPatchId",subLibraryId);
            put(subs,juce::Identifier(key),child);
            put(parent,"subs",subs);
            put(result,juce::Identifier(patchId),parent);
            return result;
        }
    throw std::runtime_error("Selected Base segment no longer exists");
}
inline juce::var replaceSubPatchSegment(const juce::var& spans,const juce::String& patchId,
                                        const juce::String& instanceId,const juce::String& libraryId) {
    auto result=spans.clone();
    auto parent=result[juce::Identifier(patchId)].clone();
    auto subs=parent["subs"].clone();
    const auto source=subs[juce::Identifier(instanceId)];
    if(!source.isObject() || libraryId.isEmpty())
        throw std::runtime_error("Selected Sub-patch strip no longer exists");
    auto updated=source.clone();
    put(updated,"subPatchId",libraryId);
    put(subs,juce::Identifier(instanceId),updated);
    put(parent,"subs",subs);
    put(result,juce::Identifier(patchId),parent);
    return result;
}
// Same-chain Sub-patches own their shared boundary independently of the
// parent Patch and its Base intervals. One local spans edit = one Undo.
inline juce::var joinedSubPatchSpans(const juce::var& spans,const juce::String& patchId,
                                     const juce::String& leftId,const juce::String& rightId,
                                     double atOffset) {
    const auto parent=spans[juce::Identifier(patchId)];
    if(!parent.isObject()||!std::isfinite(atOffset))
        throw std::runtime_error("Missing shared Sub-patch boundary");
    const auto subs=parent["subs"];
    const auto rows=orderedSpans(subs);
    const auto left=std::find_if(rows.begin(),rows.end(),
        [&](const auto& row){return row.first==leftId;});
    if(left==rows.end()||std::next(left)==rows.end()||
       std::next(left)->first!=rightId)
        throw std::runtime_error("Sub-patch strips are not adjacent");
    const auto& first=left->second;
    const auto& second=std::next(left)->second;
    const double start=blockStart(first);
    const double boundary=start+blockLength(first);
    const double end=blockStart(second)+blockLength(second);
    if(std::abs(blockStart(second)-boundary)>1.e-7)
        throw std::runtime_error("Sub-patch strips do not touch");
    const double next=juce::jlimit(start+.25,end-.25,
                                  std::round(atOffset*4.)/4.);
    if(std::abs(next-boundary)<1.e-7)return spans.clone();
    auto result=spans.clone();
    auto updated=result[juce::Identifier(patchId)].clone();
    auto children=updated["subs"].clone();
    auto firstNext=children[juce::Identifier(leftId)].clone();
    auto secondNext=children[juce::Identifier(rightId)].clone();
    put(firstNext,"length",next-start);
    put(secondNext,"start",next);
    put(secondNext,"length",end-next);
    put(children,juce::Identifier(leftId),firstNext);
    put(children,juce::Identifier(rightId),secondNext);
    put(updated,"subs",children);
    put(result,juce::Identifier(patchId),updated);
    return result;
}
// Place one Sub-patch at the click (1 bar by default); shorten to the free
// interval. UUID instance keys allow repeated Rhythm/Solo occurrences while
// keeping one shared library override state.
inline juce::var addedSubPatchSpanAt(const juce::var& subs,double parentLength,
                                     const juce::String& libraryId,double preferredStart,
                                     double preferredLength=1.) {
    if(libraryId.isEmpty() || !std::isfinite(preferredStart) ||
       !std::isfinite(parentLength) || parentLength<.25 ||
       !std::isfinite(preferredLength) || preferredLength<.25)
        throw std::runtime_error("Invalid Sub-patch insertion");
    auto next=subs.isObject()?subs.clone():object();
    auto parent=blockSpan(0.,parentLength);
    put(parent,"subs",next);
    const auto gaps=subPatchBaseGaps(parent);
    const double snapped=std::round(preferredStart*4.)/4.;
    struct Slot { double start{},length{}; bool atClick{}; };
    std::vector<Slot> slots;
    for(const auto& gap:gaps) {
        const double begin=std::ceil((gap.first-1.e-7)*4.)/4.;
        const double end=std::floor((gap.first+gap.second-.25+1.e-7)*4.)/4.;
        if(end<begin-1.e-7)continue;
        slots.push_back({begin,end, snapped>=begin-1.e-7&&snapped<=gap.first+gap.second-.25+1.e-7});
    }
    if(slots.empty())return next;
    const Slot* selected=nullptr;
    for(const auto& slot:slots)
        if(slot.atClick){selected=&slot;break;}
    if(selected==nullptr)
        for(const auto& slot:slots)
            if(slot.start>=snapped-1.e-7){selected=&slot;break;}
    if(selected==nullptr)selected=&slots.front();
    const double start=juce::jlimit(selected->start,selected->length,snapped);
    const double gapEnd=[&]{
        for(const auto& gap:gaps)
            if(start>=gap.first-1.e-7&&start<gap.first+gap.second-1.e-7)
                return gap.first+gap.second;
        return start;
    }();
    const double length=juce::jmin(preferredLength,gapEnd-start);
    if(length<.25-1.e-7)return next;
    const auto instance=next[juce::Identifier(libraryId)].isObject()?freshId():libraryId;
    auto span=blockSpan(start,length);
    if(instance!=libraryId)put(span,"subPatchId",libraryId);
    put(next,juce::Identifier(instance),span);
    return next;
}
// Group assignment rebinds each *instance* to a library Patch. Geometry,
// order and VST3 snapshots never change. The caller commits this object in
// one local Undo batch, even when selected blocks span several Items.
inline juce::var assignedPatchBlocks(const juce::var& spans,
                                     const std::vector<juce::String>& blockIds,
                                     const juce::String& replacementId) {
    if(!spans.isObject() || replacementId.isEmpty() || blockIds.empty())
        throw std::runtime_error("Select one or more Patch blocks and a captured library Patch");
    auto updatedSpans=spans.clone();
    for(const auto& blockId:blockIds) {
        const auto existing=updatedSpans[juce::Identifier(blockId)];
        if(!existing.isObject())
            throw std::runtime_error("Selected Patch block no longer exists");
        if(patchLibraryId(blockId,existing)==replacementId)continue;
        auto span=existing.clone();
        // Sub-strips belong to the previous base sound; never silently keep
        // incompatible Sub-patch overrides after a group re-assignment.
        if(auto* object=span.getDynamicObject())object->removeProperty("subs");
        put(span,"patchId",replacementId);
        put(updatedSpans,juce::Identifier(blockId),span);
    }
    return updatedSpans;
}
// Small pixel-based magnetic snap to adjacent block boundaries. The geometry
// remains a *single* block edit; neighbours never move as a side effect.
struct PatchMagnet {
    double start{},length{};
    bool snapped{};
};
inline PatchMagnet magneticPatchEdge(const juce::var& spans,
                                     const juce::String& id,double start,double length,
                                     bool moveBody,bool leftEdge,double toleranceBars) {
    PatchMagnet result{start,length,false};
    const auto rows=orderedSpans(spans);
    auto at=std::find_if(rows.begin(),rows.end(),
        [&](const auto& row){return row.first==id;});
    if(at==rows.end()||toleranceBars<=0.)return result;
    const auto index=static_cast<std::size_t>(std::distance(rows.begin(),at));
    double best=toleranceBars;
    const auto consider=[&](double delta) {
        if(std::abs(delta)>best+1.e-9)return;
        best=std::abs(delta);
        result.snapped=true;
        if(moveBody)result.start=start+delta;
        else if(leftEdge) {
            result.start=start+delta;
            result.length=length-delta;
        } else result.length=length+delta;
    };
    if(index>0&&(moveBody||leftEdge)) {
        const auto& previous=rows[index-1].second;
        const double end=blockStart(previous)+blockLength(previous);
        consider(end-start);
    }
    if(index+1<rows.size()&&(moveBody||!leftEdge)) {
        const double nextStart=blockStart(rows[index+1].second);
        consider(nextStart-(start+length));
    }
    return result;
}
// Moves/resizes one patch block. Siblings keep order: a body move is clamped to
// the free space between neighbours; resizing with adjustNeighbours pushes the
// touched neighbour, matching the accepted browser-demo rules. A parent resize
// scales nested sub-patch spans proportionally. One call produces the complete
// next spans object for a single Undo commit.
inline juce::var editedPatchSpans(const juce::var& spans,double itemBars,const juce::String& patchId,
                                  double start,double length,bool adjustNeighbours,bool moveBody=false) {
    auto result=spans.isObject()?spans.clone():object();
    auto* values=result.getDynamicObject();
    if(values==nullptr||!values->hasProperty(juce::Identifier(patchId)))
        return result;
    auto rows=orderedSpans(result);
    std::size_t index=0;
    for(;index<rows.size();++index)if(rows[index].first==patchId)break;
    if(index>=rows.size())return result;
    auto& current=rows[index].second;
    const double originalLength=blockLength(current);
    const double itemEnd=juce::jmax(.25,itemBars);
    length=juce::jmax(.25,length);
    if(moveBody) {
        start=juce::jlimit(0.,juce::jmax(0.,itemEnd-length),start);
        if(index>0)
            start=juce::jmax(start,blockStart(rows[index-1].second)+blockLength(rows[index-1].second));
        if(index+1<rows.size())
            start=juce::jmin(start,blockStart(rows[index+1].second)-length);
        if(start<0.)start=0.;
    } else {
        start=juce::jlimit(0.,juce::jmax(0.,itemEnd-.25),start);
        if(start+length>itemEnd)length=itemEnd-start;
        if(adjustNeighbours) {
            if(index>0) {
                auto& previous=rows[index-1].second;
                const double previousEnd=blockStart(previous)+blockLength(previous);
                if(start<previousEnd) {
                    const double newLength=juce::jmax(.25,start-blockStart(previous));
                    put(previous,"start",juce::jmax(0.,start-newLength));
                    put(previous,"length",newLength);
                }
            }
            if(index+1<rows.size()) {
                auto& next=rows[index+1].second;
                const double nextEnd=blockStart(next)+blockLength(next);
                if(start+length>blockStart(next)) {
                    put(next,"start",start+length);
                    put(next,"length",juce::jmax(.25,nextEnd-(start+length)));
                }
            }
        } else {
            if(index>0)
                start=juce::jmax(start,blockStart(rows[index-1].second)+blockLength(rows[index-1].second));
            if(index+1<rows.size())
                length=juce::jmin(length,blockStart(rows[index+1].second)-start);
            if(length<.25)length=.25;
            if(start+length>itemEnd)length=itemEnd-start;
        }
    }
    put(current,"start",start);
    put(current,"length",length);
    auto subs=current["subs"].isObject()?current["subs"].clone():object();
    if(auto* subValues=subs.getDynamicObject())
        if(!moveBody&&originalLength>.25&&std::abs(length-originalLength)>1.e-9) {
            // NamedValueSet iteration is read-only in JUCE, so scale into copies
            // first and publish them after the traversal.
            std::vector<std::pair<juce::Identifier,juce::var>> scaled;
            const double ratio=length/originalLength;
            for(const auto& pair:subValues->getProperties()) {
                auto sub=pair.value;
                if(!sub.isObject())continue;
                put(sub,"start",juce::jlimit(0.,juce::jmax(0.,length-.25),blockStart(sub)*ratio));
                put(sub,"length",juce::jlimit(.25,juce::jmax(.25,length),blockLength(sub)*ratio));
                scaled.emplace_back(pair.name,sub);
            }
            for(const auto& entry:scaled)subValues->setProperty(entry.first,entry.second);
        }
    put(current,"subs",subs);
    if(!moveBody&&originalLength>.25&&std::abs(length-originalLength)>1.e-9)
        if(const auto* cuts=current["baseCuts"].getArray()) {
            juce::Array<juce::var> scaled;
            for(const auto& mark:*cuts)
                scaled.add(static_cast<double>(mark)*length/originalLength);
            put(current,"baseCuts",juce::var(scaled));
        }
    for(const auto& row:rows)
        if(row.first!=patchId)values->setProperty(juce::Identifier(row.first),row.second);
    values->setProperty(juce::Identifier(patchId),current);
    return result;
}
// #374 Upd1: the lower-half shared seam between two adjacent Patch blocks
// is a *single* gesture. It preserves their combined extent and every library
// reference. Sub-patch strips stay proportional to their parent; the minimum
// allowed parent length also protects each child from becoming < 1/4 bar.
inline juce::var joinedPatchSpans(const juce::var& spans,const juce::String& leftId,
                                  const juce::String& rightId,double atBar) {
    const auto ordered=orderedSpans(spans);
    const auto at=std::find_if(ordered.begin(),ordered.end(),[&](const auto& row) {
        return row.first==leftId;});
    if(at==ordered.end() || std::next(at)==ordered.end() ||
       std::next(at)->first!=rightId)
        throw std::runtime_error("Shared Patch boundary no longer exists");
    const auto& leftOrig=at->second;
    const auto& rightOrig=std::next(at)->second;
    const double start=blockStart(leftOrig);
    const double originalLeft=blockLength(leftOrig);
    const double boundary=start+originalLeft;
    const double oldRight=blockLength(rightOrig);
    const double end=blockStart(rightOrig)+oldRight;
    if(std::abs(blockStart(rightOrig)-boundary)>1.e-7)
        throw std::runtime_error("Patch blocks must touch to edit their shared boundary");
    const auto minimum=[&](const juce::var& parent) {
        const double parentLength=blockLength(parent);
        double required=.25;
        if(const auto* children=parent["subs"].getDynamicObject())
            for(const auto& child:children->getProperties()) {
                if(!child.value.isObject())continue;
                const auto childLength=blockLength(child.value);
                if(childLength<=0.)throw std::runtime_error("Invalid Sub-patch strip");
                required=juce::jmax(required,parentLength*(.25/childLength));
            }
        return std::ceil((required-1.e-7)*4.)/4.;
    };
    const double lower=start+minimum(leftOrig);
    const double upper=end-minimum(rightOrig);
    if(lower>upper+1.e-7)
        throw std::runtime_error("Sub-patch strips prevent resizing this shared boundary");
    const double nextBoundary=juce::jlimit(lower,upper,std::round(atBar*4.)/4.);
    if(std::abs(nextBoundary-boundary)<1.e-7)return spans.clone();
    auto result=spans.clone();
    const auto updatedParent=[&](const juce::var& source,double parentStart,double newLength) {
        auto updated=source.clone();
        const double ratio=newLength/blockLength(source);
        put(updated,"start",parentStart);
        put(updated,"length",newLength);
        if(auto* sourceChildren=source["subs"].getDynamicObject()) {
            auto children=object();
            for(const auto& child:sourceChildren->getProperties()) {
                if(!child.value.isObject())continue;
                auto current=child.value.clone();
                put(current,"start",blockStart(child.value)*ratio);
                put(current,"length",blockLength(child.value)*ratio);
                put(children,child.name,current);
            }
            put(updated,"subs",children);
        }
        if(const auto* marks=source["baseCuts"].getArray()) {
            juce::Array<juce::var> cuts;
            for(const auto& mark:*marks)
                cuts.add(static_cast<double>(mark)*ratio);
            put(updated,"baseCuts",juce::var(cuts));
        }
        return updated;
    };
    put(result,juce::Identifier(leftId),updatedParent(leftOrig,start,nextBoundary-start));
    put(result,juce::Identifier(rightId),updatedParent(rightOrig,nextBoundary,end-nextBoundary));
    return result;
}
inline juce::var editedSubPatchSpan(const juce::var& spans,const juce::String& patchId,
                                    const juce::String& subId,double start,double length,
                                    bool moveBody=false) {
    auto result=spans.isObject()?spans.clone():object();
    auto* values=result.getDynamicObject();
    if(values==nullptr||!values->hasProperty(juce::Identifier(patchId)))return result;
    auto* patch=values->getProperty(juce::Identifier(patchId)).getDynamicObject();
    if(patch==nullptr)return result;
    auto subs=patch->getProperty("subs");
    if(!subs.isObject()){subs=object();patch->setProperty("subs",subs);}
    auto* subValues=subs.getDynamicObject();
    const double parentLength=blockLength(values->getProperty(juce::Identifier(patchId)));
    auto rows=orderedSpans(subs);
    int index=-1;
    for(int i=0;i<static_cast<int>(rows.size());++i)if(rows[i].first==subId){index=i;break;}
    const double previousEnd=index>0?blockStart(rows[static_cast<std::size_t>(index-1)].second)+
        blockLength(rows[static_cast<std::size_t>(index-1)].second):0.;
    const double nextStart=index>=0&&index+1<static_cast<int>(rows.size())?
        blockStart(rows[static_cast<std::size_t>(index+1)].second):parentLength;
    if(moveBody) {
        length=juce::jmax(.25,length);
        start=juce::jlimit(0.,juce::jmax(0.,parentLength-length),start);
        start=juce::jmax(start,previousEnd);
        start=juce::jmin(start,juce::jmax(previousEnd,nextStart-length));
    } else {
        start=juce::jlimit(0.,juce::jmax(0.,parentLength-.25),start);
        length=juce::jlimit(.25,juce::jmax(.25,parentLength-start),length);
        start=juce::jmax(start,previousEnd);
        length=juce::jmin(length,juce::jmax(.25,nextStart-start));
        if(start+length>parentLength)length=parentLength-start;
    }
    if(index>=0) {
        // Keep instance->library metadata after a cut. Replacing this row
        // with blockSpan() would drop subPatchId on generated UUID instances.
        auto updated=subValues->getProperty(juce::Identifier(subId)).clone();
        put(updated,"start",start);
        put(updated,"length",length);
        subValues->setProperty(juce::Identifier(subId),updated);
    }
    return result;
}
inline juce::var removedPatchSpan(const juce::var& spans,const juce::String& patchId) {
    auto result=spans.isObject()?spans.clone():object();
    if(auto* values=result.getDynamicObject())
        values->removeProperty(juce::Identifier(patchId));
    return result;
}
inline juce::var addedPatchSpan(const juce::var& spans,double itemBars,const juce::String& patchId,
                                double preferredLength) {
    auto result=spans.isObject()?spans.clone():object();
    auto* values=result.getDynamicObject();
    if(values==nullptr)return result;
    const double itemEnd=juce::jmax(.25,itemBars);
    const double preferred=juce::jmax(.25,preferredLength);
    double start=0.;
    // Placement uses the first gap that fits a block; the requested length is
    // shortened to that gap instead of rejecting the insert.
    const auto place=[&](double gapLength) {
        if(gapLength<.25)return false;
        values->setProperty(juce::Identifier(patchId),
                            blockSpan(start,juce::jmin(preferred,gapLength)));
        return true;
    };
    for(const auto& row:orderedSpans(result)) {
        const double rowStart=juce::jmax(0.,blockStart(row.second));
        if(place(rowStart-start))return result;
        start=juce::jmax(start,rowStart+blockLength(row.second));
    }
    place(itemEnd-start); // no room: explicit no-op, caller reports
    return result;
}
// #289 C follow-up: insert at the playhead instead of the first free gap. Falls back to the
// first gap that fits when the playhead sits inside an existing block.
inline juce::var addedPatchSpanAt(const juce::var& spans,double itemBars,const juce::String& patchId,
                                  double preferredStart,double preferredLength) {
    auto result=spans.isObject()?spans.clone():object();
    auto* values=result.getDynamicObject();
    if(values==nullptr)return result;
    const double itemEnd=juce::jmax(.25,itemBars);
    const double want=juce::jmax(.25,preferredLength);
    std::vector<std::pair<double,double>> gaps;
    double cursor=0.;
    for(const auto& row:orderedSpans(result)) {
        const double rowStart=juce::jmax(0.,blockStart(row.second));
        if(rowStart-cursor>=.25)gaps.emplace_back(cursor,rowStart);
        cursor=juce::jmax(cursor,rowStart+blockLength(row.second));
    }
    if(itemEnd-cursor>=.25)gaps.emplace_back(cursor,itemEnd);
    int chosen=-1;
    for(std::size_t i=0;i<gaps.size();++i)
        if(preferredStart>=gaps[i].first&&preferredStart<=gaps[i].second){chosen=static_cast<int>(i);break;}
    if(chosen<0)
        for(std::size_t i=0;i<gaps.size();++i)
            if(gaps[i].second-gaps[i].first>=.25){chosen=static_cast<int>(i);break;}
    if(chosen<0)return result; // no room: explicit no-op, caller reports
    const auto& gap=gaps[static_cast<std::size_t>(chosen)];
    double start=juce::jlimit(gap.first,juce::jmax(gap.first,gap.second-.25),preferredStart);
    double length=juce::jmin(want,juce::jmax(.25,gap.second-start));
    if(length<.25){start=gap.first;length=juce::jmin(want,gap.second-gap.first);}
    values->setProperty(juce::Identifier(patchId),blockSpan(start,length));
    return result;
}
// #289 A-Upd1: channel patches an item asks for when it becomes current.
// Legacy demo patches have no captured chain and are never auto-applied.
inline std::vector<std::pair<juce::String, juce::String>> itemDefaultPatches(
    const juce::var &state, const juce::String &item) {
    std::vector<std::pair<juce::String, juce::String>> result;
    const auto row = find(state, "items", item);
    if (!row.isObject())
        return result;
    if (const auto *tracks = row["tracks"].getArray())
        for (const auto &track : *tracks) {
            const auto channel = track["channelId"].toString();
            const auto patchId = track["patchId"].toString();
            if (channel.isEmpty() || patchId.isEmpty())
                continue;
            const auto channelRow = find(state, "channels", channel);
            if (channelRow["type"].toString() == "Playback")
                continue;
            const auto patch = find(channelRow, "patches", patchId);
            if (!patch.isObject() || legacyPatch(patch))
                continue;
            result.emplace_back(channel, patchId);
        }
    return result;
}
inline void validatePatchStates(const juce::var &state) {
    const auto store = state["patchStates"];
    if (store.isVoid())
        return;
    if (!store.isObject())
        throw std::runtime_error("Invalid channel patch state store");
    const auto *values = store.getDynamicObject();
    if (static_cast<std::size_t>(values->getProperties().size()) > maximumPatchStates)
        throw std::runtime_error("Too many channel patch states");
    for (const auto &pair : values->getProperties()) {
        if (!hexHash(pair.name.toString()) || !pair.value.isObject())
            throw std::runtime_error("Invalid channel patch state");
        for (const auto *key : {"component", "controller"}) {
            const auto blob = pair.value[key];
            if (!blob.isString() ||
                static_cast<std::size_t>(blob.toString().length()) > maximumPatchStateBase64)
                throw std::runtime_error("Invalid channel patch state encoding");
        }
    }
}
inline void validatePatchInserts(const juce::var &state, const juce::var &patch) {
    const auto *inserts = patch["inserts"].getArray();
    if (!inserts || inserts->size() > maximumPatchInserts)
        throw std::runtime_error("Invalid channel patch insert chain");
    std::set<juce::String> insertIds;
    for (const auto &insert : *inserts) {
        if (!insert.isObject() || !named(insert["id"], 128) ||
            !insertIds.insert(insert["id"].toString()).second || !insert["bypass"].isBool())
            throw std::runtime_error("Invalid channel patch insert");
        const auto kind = insert["kind"].toString();
        if (kind == "gain") {
            if (!number(insert["gain"]))
                throw std::runtime_error("Invalid channel patch gain insert");
            continue;
        }
        if (kind != "vst3" || !named(insert["path"], 4096) || !named(insert["classId"], 256) ||
            !insert["name"].isString() || insert["name"].toString().length() > 500 ||
            !hexHash(insert["state"].toString()))
            throw std::runtime_error("Invalid channel patch VST3 insert");
        const auto *store = state["patchStates"].getDynamicObject();
        if (store == nullptr ||
            !store->hasProperty(juce::Identifier(insert["state"].toString())))
            throw std::runtime_error("Invalid or missing channel patch VST3 state");
        if (!insert["parameters"].isArray() || insert["parameters"].size() > 2048)
            throw std::runtime_error("Invalid channel patch parameters");
        for (const auto &parameter : *insert["parameters"].getArray()) {
            if (!parameter.isObject() || !number(parameter["id"]) || !number(parameter["value"]))
                throw std::runtime_error("Invalid channel patch parameter");
            const auto id = static_cast<double>(parameter["id"]);
            const auto level = static_cast<double>(parameter["value"]);
            if (!std::isfinite(id) || id < 0 || id > 4294967295. || std::floor(id) != id ||
                !std::isfinite(level) || level < 0 || level > 1)
                throw std::runtime_error("Invalid channel patch parameter range");
        }
    }
}
// #289 B: sub-patches are state variants of the SAME captured chain. They never
// carry engine/plugin structure of their own; only the opaque plugin state and
// native parameters differ from the base patch.
inline void validateSubPatches(const juce::var &state, const juce::var &patch) {
    // The baseline is read-only metadata used for Sub-patch comparison.
    // Vendor component state remains authoritative for Base restoration.
    const auto baseline=patch["parameterBaseline"];
    if(!baseline.isVoid()) {
        if(!baseline.isArray()||baseline.size()>maximumPatchInserts)
            throw std::runtime_error("Invalid Patch parameter baseline");
        std::set<juce::String> seenInserts;
        for(const auto& insert:*baseline.getArray()) {
            const auto id=insert["insertId"].toString();
            const auto* params=insert["parameters"].getArray();
            if(!insert.isObject() || !named(insert["insertId"],128) ||
               !seenInserts.insert(id).second || !params || params->size()>4096)
                throw std::runtime_error("Invalid Patch VST3 parameter baseline");
            std::set<juce::int64> paramIds;
            for(const auto& param:*params) {
                if(!number(param["id"])||!number(param["value"]))
                    throw std::runtime_error("Invalid Patch VST3 parameter baseline");
                const double key=static_cast<double>(param["id"]);
                const double value=static_cast<double>(param["value"]);
                if(key<0.||key>4294967295.||std::floor(key)!=key ||
                   !std::isfinite(value)||value<0.||value>1.||
                   !paramIds.insert(static_cast<juce::int64>(key)).second)
                    throw std::runtime_error("Invalid Patch VST3 parameter baseline");
            }
        }
    }
    const auto subs = patch["subPatches"];
    if(subs.isVoid())return;
    if(!subs.isArray() || subs.size()>maximumSubPatches)
        throw std::runtime_error("Invalid channel sub-patch list");
    const auto* baseInserts=patch["inserts"].getArray();
    if(baseInserts==nullptr)throw std::runtime_error("Channel sub-patches require a captured Patch");
    std::set<juce::String> ids,names;
    for(const auto& sub:*subs.getArray()) {
        if(!sub.isObject() || !named(sub["id"]) || !ids.insert(sub["id"].toString()).second ||
           !named(sub["name"]) || !names.insert(sub["name"].toString()).second)
            throw std::runtime_error("Invalid channel sub-patch");
        if(!sub["color"].isVoid() && !patchColour(sub["color"].toString()))
            throw std::runtime_error("Invalid channel sub-patch colour");
        if(!sub["overrides"].isVoid()) {
            const auto* overrides=sub["overrides"].getArray();
            if(!overrides||overrides->isEmpty()||overrides->size()>maximumSubPatchOverrides||
               sub["inserts"].isArray())
                throw std::runtime_error("Invalid parameter-only sub-patch");
            std::set<std::pair<juce::String,juce::int64>> seen;
            for(const auto& entry:*overrides) {
                const auto insertId=entry["insertId"].toString();
                if(!entry.isObject()||!named(entry["insertId"],128)||
                   !number(entry["parameterId"])||!number(entry["value"]))
                    throw std::runtime_error("Invalid sub-patch parameter override");
                const double parameterId=static_cast<double>(entry["parameterId"]);
                const double value=static_cast<double>(entry["value"]);
                if(parameterId<0.||parameterId>4294967295.||std::floor(parameterId)!=parameterId||
                   !std::isfinite(value)||value<0.||value>1. ||
                   !seen.insert({insertId,static_cast<juce::int64>(parameterId)}).second)
                    throw std::runtime_error("Invalid sub-patch parameter ID/value");
                bool matches=false;
                for(const auto& insert:*baseInserts)
                    if(insert["id"].toString()==insertId && insert["kind"].toString()=="vst3")
                        if(const auto* params=insert["parameters"].getArray())
                            for(const auto& parameter:*params)
                                if(static_cast<double>(parameter["id"])==parameterId)
                                    matches=true;
                if(!matches && baseline.isArray())
                    for(const auto& row:*baseline.getArray())
                        if(row["insertId"].toString()==insertId)
                            if(const auto* params=row["parameters"].getArray())
                                for(const auto& param:*params)
                                    if(static_cast<double>(param["id"])==parameterId)
                                        matches=true;
                if(!matches)
                    throw std::runtime_error("Sub-patch override references a missing VST3 parameter");
            }
            continue;
        }
        // Previous accepted shows may contain full same-chain opaque snapshots.
        // Validate and preserve them until a user explicitly updates this Sub-patch.
        if(!sub["inserts"].isArray() || sub["inserts"].size()!=baseInserts->size())
            throw std::runtime_error("Sub-patch chain differs from its base patch");
        validatePatchInserts(state,sub);
        for(int i=0;i<baseInserts->size();++i) {
            const auto& base=(*baseInserts)[i];
            const auto& variant=sub["inserts"][i];
            if(variant["id"]!=base["id"] || variant["kind"]!=base["kind"] ||
               variant["path"]!=base["path"] || variant["classId"]!=base["classId"])
                throw std::runtime_error("Sub-patch chain differs from its base patch");
        }
    }
}
// Removes every reference to a deleted patch from one item track: the default
// assignment, the canonical patchOrder entry and the stored span geometry.
inline bool removeChannelPatchReference(juce::var &track, const juce::String &channel,
                                        const juce::String &patchId) {
    if(!track.isObject() || track["channelId"].toString()!=channel)return false;
    bool changed=false;
    if(track["patchId"].toString()==patchId) {
        track.getDynamicObject()->removeProperty("patchId");
        track.getDynamicObject()->removeProperty("subPatchId");
        changed=true;
    }
    auto spans=track["spans"].clone();
    juce::StringArray removed;
    if(const auto* values=spans.getDynamicObject())
        for(const auto& pair:values->getProperties())
            if(patchLibraryId(pair.name.toString(),pair.value)==patchId)
                removed.add(pair.name.toString());
    if(!removed.isEmpty()) {
        for(const auto& id:removed)
            spans.getDynamicObject()->removeProperty(juce::Identifier(id));
        put(track,"spans",spans);
        changed=true;
    }
    if(auto* order=track["patchOrder"].getArray())
        for(int i=order->size();--i>=0;)
            if(removed.contains((*order)[i].toString()) ||
               (removed.isEmpty() && (*order)[i].toString()==patchId)) {
                order->remove(i);changed=true;
            }
    return changed;
}
inline void clearChannelPatchReferences(juce::var &state, const juce::String &channel,
                                        const juce::String &patchId) {
    for (auto &item : array(state, "items"))
        if (auto *tracks = item["tracks"].getArray())
            for (auto &track : *tracks)
                removeChannelPatchReference(track, channel, patchId);
}
// Browser saved item versions are JSON strings; a deleted patch must not be
// resurrectable from them as a dangling reference either. The property set is
// read-only during iteration, so updates are applied afterwards.
inline void clearLibraryPatchReferences(juce::var &state, const juce::String &channel,
                                        const juce::String &patchId) {
    auto *library = state["library"].getDynamicObject();
    if (library == nullptr)
        return;
    std::vector<std::pair<juce::String, juce::String>> updates;
    for (const auto &pair : library->getProperties()) {
        // Cheap prefilter: most saved versions do not reference this patch.
        if (!pair.value.isString() || !pair.value.toString().contains(patchId))
            continue;
        juce::var snapshot;
        if (juce::JSON::parse(pair.value.toString(), snapshot).failed())
            continue;
        bool changed = false;
        if (auto *items = snapshot["items"].getArray())
            for (auto &item : *items)
                if (auto *tracks = item["tracks"].getArray())
                    for (auto &track : *tracks)
                        changed = removeChannelPatchReference(track, channel, patchId) || changed;
        if (changed)
            updates.emplace_back(pair.name.toString(), juce::JSON::toString(snapshot, true, 17));
    }
    for (const auto &update : updates)
        library->setProperty(juce::Identifier(update.first), update.second);
}
// #289 B: deleting a sub-patch clears the item selection that pointed at it.
inline bool removeChannelSubPatchReference(juce::var &track, const juce::String &channel,
                                           const juce::String &patchId,
                                           const juce::String &subPatchId) {
    if(!track.isObject() || track["channelId"].toString()!=channel)return false;
    bool changed=false;
    if(track["patchId"].toString()==patchId && track["subPatchId"].toString()==subPatchId) {
        track.getDynamicObject()->removeProperty("subPatchId");
        changed=true;
    }
    auto spans=track["spans"].clone();
    std::vector<std::pair<juce::Identifier,juce::var>> updates;
    if(const auto* values=spans.getDynamicObject())
        for(const auto& pair:values->getProperties()) {
            if(!pair.value.isObject()||
               patchLibraryId(pair.name.toString(),pair.value)!=patchId)continue;
            auto subs=pair.value["subs"].clone();
            juce::StringArray removed;
            if(const auto* entries=subs.getDynamicObject())
                for(const auto& sub:entries->getProperties())
                    if(subPatchLibraryId(sub.name.toString(),sub.value)==subPatchId)
                        removed.add(sub.name.toString());
            if(removed.isEmpty())continue;
            for(const auto& id:removed)
                subs.getDynamicObject()->removeProperty(juce::Identifier(id));
            auto updated=pair.value.clone();
            put(updated,"subs",subs);
            updates.emplace_back(pair.name,updated);
        }
    for(const auto& update:updates)put(spans,update.first,update.second);
    if(!updates.empty())changed=true;
    if(changed)put(track,"spans",spans);
    return changed;
}
inline void clearSubPatchReferences(juce::var &state, const juce::String &channel,
                                    const juce::String &patchId, const juce::String &subPatchId) {
    for (auto &item : array(state, "items"))
        if (auto *tracks = item["tracks"].getArray())
            for (auto &track : *tracks)
                removeChannelSubPatchReference(track, channel, patchId, subPatchId);
}
inline void clearLibrarySubPatchReferences(juce::var &state, const juce::String &channel,
                                           const juce::String &patchId,
                                           const juce::String &subPatchId) {
    auto *library = state["library"].getDynamicObject();
    if (library == nullptr)
        return;
    std::vector<std::pair<juce::String, juce::String>> updates;
    for (const auto &pair : library->getProperties()) {
        if (!pair.value.isString() || !pair.value.toString().contains(subPatchId))
            continue;
        juce::var snapshot;
        if (juce::JSON::parse(pair.value.toString(), snapshot).failed())
            continue;
        bool changed = false;
        if (auto *items = snapshot["items"].getArray())
            for (auto &item : *items)
                if (auto *tracks = item["tracks"].getArray())
                    for (auto &track : *tracks)
                        changed = removeChannelSubPatchReference(track, channel, patchId, subPatchId) || changed;
        if (changed)
            updates.emplace_back(pair.name.toString(), juce::JSON::toString(snapshot, true, 17));
    }
    for (const auto &update : updates)
        library->setProperty(juce::Identifier(update.first), update.second);
}
// Drop blobs that no patch references any more; identical chains share one blob.
inline void prunePatchStates(juce::var &state) {
    std::set<juce::String> used;
    for (const auto &channel : array(state, "channels"))
        if (const auto *bank = channel["patches"].getArray())
            for (const auto &patch : *bank) {
                const auto keepInserts=[&](const juce::var& source) {
                    if(const auto* inserts=source["inserts"].getArray())
                        for(const auto& insert:*inserts)
                            if(insert["state"].isString())
                                used.insert(insert["state"].toString());
                };
                keepInserts(patch);
                if(const auto* subs=patch["subPatches"].getArray())
                    for(const auto& sub:*subs)keepInserts(sub); // 0.9.2 legacy variants
            }
    auto &store = map(state, "patchStates");
    juce::StringArray remove;
    if (const auto *values = store.getDynamicObject())
        for (const auto &pair : values->getProperties())
            if (!used.contains(pair.name.toString()))
                remove.add(pair.name.toString());
    for (const auto &name : remove)
        store.getDynamicObject()->removeProperty(juce::Identifier(name));
    if (remove.size() && store.getDynamicObject() &&
        store.getDynamicObject()->getProperties().isEmpty())
        state.getDynamicObject()->removeProperty("patchStates");
}
inline juce::var empty(const juce::var &defaults = {}) {
    auto state = object();
    put(state, "items", juce::Array<juce::var>());
    put(state, "folders", juce::Array<juce::var>());
    put(state, "channels", juce::Array<juce::var>());
    put(state, "library", object());
    auto list = object();
    put(list, "id", "setlist-main");
    put(list, "name", "Setlist");
    put(list, "entries", juce::Array<juce::var>());
    juce::Array<juce::var> lists;
    lists.add(list);
    put(state, "setlists", lists);
    put(state, "activeSetlist", "setlist-main");
    put(state, "currentItem", {});
    put(state, "viewRecordTracks", false);
    auto perf = object();
    put(perf, "showOrganizer", defaults.hasProperty("showOrganizer") ? defaults["showOrganizer"] : juce::var(true));
    if (defaults["itemCardLayout"].isObject())
        put(perf, "itemCardLayout", defaults["itemCardLayout"].clone());
    put(state, "performance", perf);
    return state;
}
inline void validateLayout(const juce::var &layout) {
    if (layout.isVoid())
        return;
    if (!layout.isObject() || static_cast<int>(layout["revision"]) != 2 || !number(layout["height"]) ||
        static_cast<double>(layout["height"]) < 48 || static_cast<double>(layout["height"]) > 240 ||
        !layout["fields"].isObject())
        throw std::runtime_error("Invalid item card layout");
    for (const auto *key : {"name", "metadata", "progress", "priority", "status", "saved"}) {
        const auto field = layout["fields"][key];
        for (const auto *axis : {"x", "y", "w", "h"})
            if (!number(field[axis]))
                throw std::runtime_error("Invalid card field geometry");
        const double x = field["x"], y = field["y"], w = field["w"], h = field["h"];
        if (!field["visible"].isBool() || x < 0 || y < 0 || w < 2 || h < 4 || x + w > 100.001 ||
            y + h > static_cast<double>(layout["height"]) + .001)
            throw std::runtime_error("Card field exceeds item bounds");
    }
    if (!static_cast<bool>(layout["fields"]["name"]["visible"]))
        throw std::runtime_error("Item name must stay visible");
}
inline constexpr std::int64_t maximumShowBytes=64ll*1024ll*1024ll;
inline void validate(const juce::var &state) {
    if (!state.isObject() || juce::JSON::toString(state).getNumBytesAsUTF8() > maximumShowBytes)
        throw std::runtime_error("Show interface exceeds 64 MiB");
    std::set<juce::String> ids, items, folders, lists;
    for (const auto *key : {"items", "folders", "setlists"})
        if (!state[key].isArray() || state[key].size() > 500)
            throw std::runtime_error("Invalid show collection (limit 500)");
    const auto unique = [&](const juce::var &entry) {
        const auto id = entry["id"];
        if (!named(id) || !ids.insert(id.toString()).second)
            throw std::runtime_error("Invalid or repeated show ID");
    };
    for (const auto &folder : *state["folders"].getArray()) {
        unique(folder);
        folders.insert(folder["id"].toString());
        if (!named(folder["name"]))
            throw std::runtime_error("Invalid folder name");
    }
    for (const auto &folder : *state["folders"].getArray()) {
        std::set<juce::String> visited{folder["id"].toString()};
        auto parent = folder["parent"];
        while (!parent.isVoid()) {
            if (!parent.isString() || !folders.contains(parent.toString()) ||
                !visited.insert(parent.toString()).second || visited.size() > 32)
                throw std::runtime_error("Invalid folder hierarchy");
            parent = find(state, "folders", parent.toString())["parent"];
        }
    }
    for (const auto &item : *state["items"].getArray()) {
        unique(item);
        items.insert(item["id"].toString());
        if (!named(item["name"]) || (!item["folder"].isVoid() && !folders.contains(item["folder"].toString())) ||
            !number(item["tempo"]) || static_cast<double>(item["tempo"]) < 20 ||
            static_cast<double>(item["tempo"]) > 400 || !named(item["meter"], 12) || !item["key"].isString() ||
            item["key"].toString().length() > 24 || !number(item["duration"]) ||
            static_cast<double>(item["duration"]) < 0 || static_cast<double>(item["duration"]) > 86400 ||
            !item["notes"].isString() || item["notes"].toString().length() > 4000)
            throw std::runtime_error("Invalid item properties");
    }
    if (!state["setlists"].size())
        throw std::runtime_error("A show needs a setlist");
    for (const auto &list : *state["setlists"].getArray()) {
        unique(list);
        lists.insert(list["id"].toString());
        if (!named(list["name"]) || !list["entries"].isArray() || list["entries"].size() > 1000)
            throw std::runtime_error("Invalid setlist");
        for (const auto &entry : *list["entries"].getArray()) {
            unique(entry);
            if (entry["type"].toString() == "divider") {
                if (!named(entry["name"]))
                    throw std::runtime_error("Invalid divider");
            } else if (!items.contains(entry["itemId"].toString()))
                throw std::runtime_error("Missing item reference");
        }
    }
    if (!lists.contains(state["activeSetlist"].toString()) ||
        (!state["currentItem"].isVoid() && !items.contains(state["currentItem"].toString())))
        throw std::runtime_error("Invalid show selection");
    if (!state["performance"].isObject() || !state["channels"].isArray() || state["channels"].size() > 128)
        throw std::runtime_error("Invalid show interface state");
    const auto perf = state["performance"];
    for (const auto &item : *state["items"].getArray()) {
        const auto gain = item["playbackGroupGain"];
        if (!gain.isVoid() && (!number(gain) || static_cast<double>(gain) < -60 || static_cast<double>(gain) > 12))
            throw std::runtime_error("Item Playback level must be -60..12 dB");
    }
    std::set<juce::String> channelIds;
    for (const auto &channel : *state["channels"].getArray()) {
        if (!named(channel["id"]) || !channelIds.insert(channel["id"].toString()).second)
            throw std::runtime_error("Invalid or duplicate channel ID");
        for (const auto *key : {"gain", "pan"}) {
            const auto v = channel[key];
            const bool pan = juce::String(key) == "pan";
            if (!v.isVoid() && (!number(v) || static_cast<double>(v) < (pan ? -100. : -60.) ||
                               static_cast<double>(v) > (pan ? 100. : 12.)))
                throw std::runtime_error("Invalid channel mix");
        }
        for (const auto *key : {"mute", "solo"})
            if (!channel[key].isVoid() && !channel[key].isBool()) throw std::runtime_error("Invalid channel switch");
        // #289 A: channel-owned patch library. Captured patches carry a frozen
        // insert chain; legacy demo patches stay readable but cannot be applied.
        if (!channel["patches"].isVoid()) {
            if (!channel["patches"].isArray() || channel["patches"].size() > maximumChannelPatches)
                throw std::runtime_error("Invalid channel patch library");
            std::set<juce::String> patchIds, patchNames;
            for (const auto &patch : *channel["patches"].getArray()) {
                if (!patch.isObject() || !named(patch["id"]) ||
                    !patchIds.insert(patch["id"].toString()).second || !named(patch["name"]))
                    throw std::runtime_error("Invalid channel patch");
                if (legacyPatch(patch))
                    continue; // Legacy demo colours/names stay accepted as-is.
                if (!patchNames.insert(patch["name"].toString()).second)
                    throw std::runtime_error("Duplicate channel patch name");
                if (!patch["color"].isVoid() && !patchColour(patch["color"].toString()))
                    throw std::runtime_error("Invalid channel patch colour");
                if (!patch["version"].isInt() || static_cast<int>(patch["version"]) != 1)
                    throw std::runtime_error("Unsupported channel patch version");
                validatePatchInserts(state, patch);
                validateSubPatches(state, patch);
            }
        }
    }

    // #289 A: saved item versions now carry the channel patch assignment, so a
    // dangling reference must fail explicitly instead of silently rendering a
    // patch that no longer exists.
    validatePatchStates(state);
    for (const auto &item : *state["items"].getArray())
        if (const auto *tracks = item["tracks"].getArray())
            for (const auto &track : *tracks) {
                const auto channelId = track["channelId"].toString();
                if (channelId.isEmpty() || !channelIds.contains(channelId))
                    continue;
                const auto patchId = track["patchId"];
                // A track authoring free patch blocks may have no default patch at all, so the
                // reference checks below must not gate the #289 C block geometry validation.
                const bool noDefaultPatch = patchId.isVoid() ||
                    (patchId.isString() && patchId.toString().isEmpty());
                // External editors may write "" for "no default patch".
                if (!noDefaultPatch) {
                    if (!named(patchId) ||
                        !find(find(state, "channels", channelId), "patches", patchId.toString()).isObject())
                        throw std::runtime_error("Missing item channel patch reference");
                    const auto subPatchId = track["subPatchId"];
                    if (!(subPatchId.isVoid() ||
                          (subPatchId.isString() && subPatchId.toString().isEmpty()))) {
                        if (!named(subPatchId) ||
                            !find(find(find(state, "channels", channelId), "patches", patchId.toString()),
                                  "subPatches", subPatchId.toString()).isObject())
                            throw std::runtime_error("Missing item channel sub-patch reference");
                    }
                }
                // #289 C: authored free blocks must stay ordered, inside the item
                // and consistent with the channel patch library.
                const auto channelRow = find(state, "channels", channelId);
                const double itemBars = number(item["bars"]) ? static_cast<double>(item["bars"]) : 0.;
                std::set<juce::String> ordered;
                if (const auto *order = track["patchOrder"].getArray())
                    for (const auto &entry : *order) {
                        if (!named(entry) || !ordered.insert(entry.toString()).second)
                            throw std::runtime_error("Invalid item patch order");
                    }
                const auto spans = track["spans"];
                std::vector<std::pair<double, double>> ranges;
                if (const auto *values = spans.getDynamicObject())
                    for (const auto &pair : values->getProperties()) {
                        if (!ordered.contains(pair.name.toString()) || !pair.value.isObject() ||
                            (!pair.value["patchId"].isVoid() && !named(pair.value["patchId"])) ||
                            !find(channelRow,"patches",patchLibraryId(pair.name.toString(),pair.value)).isObject())
                            throw std::runtime_error("Invalid item patch span");
                        if (!number(pair.value["start"]) || !number(pair.value["length"]))
                            throw std::runtime_error("Invalid item patch span bounds");
                        const double start = static_cast<double>(pair.value["start"]);
                        const double length = static_cast<double>(pair.value["length"]);
                        // Imported/demo shows may carry blocks longer than the declared item
                        // length; that is repairable, while a block starting outside the item is
                        // not. Never reject a whole show for recoverable legacy geometry.
                        if (start < 0. || length < .25 || (itemBars > 0. && start >= itemBars - 1.e-6))
                            throw std::runtime_error("Item patch span exceeds item bounds: patch=" +
                                pair.name.toString().toStdString() + " start=" + std::to_string(start) +
                                " length=" + std::to_string(length) + " bars=" + std::to_string(itemBars));
                        ranges.emplace_back(start, start + length);
                    }
                std::sort(ranges.begin(), ranges.end());
                for (std::size_t i = 1; i < ranges.size(); ++i)
                    if (ranges[i].first < ranges[i - 1].second - 1.e-6)
                        throw std::runtime_error("Item patch spans overlap");
                if (const auto *values = spans.getDynamicObject())
                    for (const auto &pair : values->getProperties()) {
                        const auto patch = find(channelRow, "patches",
                                                patchLibraryId(pair.name.toString(),pair.value));
                        const auto subs = pair.value["subs"];
                        const double parentLength = static_cast<double>(pair.value["length"]);
                        const auto markers=pair.value["baseCuts"];
                        if(!markers.isVoid()) {
                            if(!markers.isArray())
                                throw std::runtime_error("Invalid Base split markers");
                            std::vector<double> cuts;
                            for(const auto& marker:*markers.getArray()) {
                                if(!number(marker))
                                    throw std::runtime_error("Invalid Base split marker");
                                const auto offset=static_cast<double>(marker);
                                if(!std::isfinite(offset)||offset<=0.||offset>=parentLength)
                                    throw std::runtime_error("Base split marker exceeds parent Patch");
                                cuts.push_back(offset);
                            }
                            std::sort(cuts.begin(),cuts.end());
                            for(std::size_t i=1;i<cuts.size();++i)
                                if(cuts[i]-cuts[i-1]<1.e-7)
                                    throw std::runtime_error("Duplicate Base split marker");
                        }
                        if (subs.isVoid())
                            continue;
                        if (!subs.isObject())
                            throw std::runtime_error("Invalid item sub-patch spans");
                        std::vector<std::pair<double, double>> subRanges;
                        if (const auto *subValues = subs.getDynamicObject())
                            for (const auto &sub : subValues->getProperties()) {
                                if (!sub.value.isObject() ||
                                    (!sub.value["subPatchId"].isVoid() && !named(sub.value["subPatchId"])) ||
                                    !find(patch, "subPatches",
                                          subPatchLibraryId(sub.name.toString(),sub.value)).isObject() ||
                                    !number(sub.value["start"]) || !number(sub.value["length"]))
                                    throw std::runtime_error("Invalid item sub-patch span");
                                const double start = static_cast<double>(sub.value["start"]);
                                const double length = static_cast<double>(sub.value["length"]);
                                if (start < 0. || length < .25 || start + length > parentLength + 1.e-6)
                                    throw std::runtime_error("Item sub-patch span exceeds its patch");
                                subRanges.emplace_back(start, start + length);
                            }
                        std::sort(subRanges.begin(), subRanges.end());
                        for (std::size_t i = 1; i < subRanges.size(); ++i)
                            if (subRanges[i].first < subRanges[i - 1].second - 1.e-6)
                                throw std::runtime_error("Item sub-patch spans overlap");

                    }
            }

    // Optional legacy-compatible stable root ID for reorderable Playback channels.
    const auto pinnedPlayback=perf["primaryPlaybackChannel"];
    if(!pinnedPlayback.isVoid()) {
        bool valid=false;
        if(pinnedPlayback.isString())
            for(const auto& channel:*state["channels"].getArray())
                if(channel["id"]==pinnedPlayback && channel["type"].toString()=="Playback")
                    valid=true;
        if(!valid)throw std::runtime_error("Invalid primary Playback channel");
    }

    validateLayout(perf["itemCardLayout"]);
    if (!perf["showOrganizer"].isVoid() && !perf["showOrganizer"].isBool())
        throw std::runtime_error("Invalid organizer visibility");
    if (const auto *workflows = perf["itemWorkflow"].getDynamicObject())
        for (const auto &pair : workflows->getProperties()) {
            const auto value = pair.value;
            if (!items.contains(pair.name.toString()) || !status(value["status"].toString()) ||
                !(value["priority"].toString() == "P1" || value["priority"].toString() == "P2" ||
                  value["priority"].toString() == "P3" || value["priority"].toString() == "P4") ||
                !number(value["progress"]) || static_cast<double>(value["progress"]) < 0 ||
                static_cast<double>(value["progress"]) > 100)
                throw std::runtime_error("Invalid item workflow");
        }
    if (const auto *taskMap = perf["itemTasks"].getDynamicObject())
        for (const auto &pair : taskMap->getProperties()) {
            if (!items.contains(pair.name.toString()) || !pair.value.isArray() || pair.value.size() > 200)
                throw std::runtime_error("Invalid organizer task list");
            std::map<juce::String, juce::var> nodes;
            for (const auto &task : *pair.value.getArray()) {
                if (!named(task["id"]) || !nodes.emplace(task["id"].toString(), task).second || !named(task["title"]) ||
                    !status(task["status"].toString()) || !number(task["progress"]) ||
                    static_cast<double>(task["progress"]) < 0 || static_cast<double>(task["progress"]) > 100)
                    throw std::runtime_error("Invalid organizer task");
            }
            for (const auto &[key, task] : nodes) {
                std::set<juce::String> visited{key};
                auto parent = task["parent"];
                while (!parent.isVoid()) {
                    const auto found = nodes.find(parent.toString());
                    if (!parent.isString() || found == nodes.end() || !visited.insert(parent.toString()).second ||
                        visited.size() > 8)
                        throw std::runtime_error("Invalid organizer hierarchy");
                    parent = found->second["parent"];
                }
            }
        }
    if (const auto *library = state["library"].getDynamicObject())
        for (const auto &pair : library->getProperties()) {
            if (!items.contains(pair.name.toString()) || !pair.value.isString())
                throw std::runtime_error("Invalid Browser saved version");
            juce::var saved;
            const auto parsed = juce::JSON::parse(pair.value.toString(), saved);
            if (parsed.failed() || !saved.isObject() || !saved["items"].isArray() || saved["items"].size() != 1 ||
                saved["items"][0]["id"].toString() != pair.name.toString())
                throw std::runtime_error("Invalid saved item snapshot");
        }
}
inline juce::String addChannel(juce::var &state, const juce::String &type, const juce::String &name) {
    if (state["channels"].size() >= 128) throw std::runtime_error("Show channel limit reached");
    auto channel = object(); const auto id = juce::Uuid().toString();
    put(channel, "id", id); put(channel, "name", name); put(channel, "type", type);
    put(channel, "gain", 0.); put(channel, "pan", 0.);
    for (const auto *key : {"mute", "solo", "armed", "monitor"}) put(channel, key, false);
    put(channel, "patches", juce::Array<juce::var>()); put(channel, "archivedPatches", juce::Array<juce::var>());
    property(state, "channels").getArray()->add(channel);
    for (auto &item : *property(state, "items").getArray()) {
        auto track = object(); put(track, "id", juce::Uuid().toString()); put(track, "channelId", id);
        for (const auto *key : {"patchId", "subPatchId", "media", "recordTrack"}) put(track, key, {});
        put(track, "patchOrder", juce::Array<juce::var>()); put(track, "spans", object()); put(track, "playbackGain", 0.);
        if (!item["tracks"].isArray()) put(item, "tracks", juce::Array<juce::var>());
        property(item, "tracks").getArray()->add(track);
    }
    return id;
}
// #289 A: before this slice the item version did not store the channel patch
// assignment. Older saved Browser versions therefore have no patchId; adopt the
// current assignment (and the referenced patch descriptor) once so opening a
// pre-#289 show is not reported as modified. The file itself is not rewritten
// until the user saves.
inline void migrateSavedPatchAssignments(juce::var &state) {
    auto *library = state["library"].getDynamicObject();
    if (library == nullptr)
        return;
    juce::StringArray keys;
    for (const auto &pair : library->getProperties())
        if (pair.value.isString())
            keys.add(pair.name.toString());
    std::vector<std::pair<juce::String, juce::String>> updates;
    for (const auto &key : keys) {
        juce::var snapshot;
        if (juce::JSON::parse(library->getProperty(juce::Identifier(key)).toString(), snapshot).failed() ||
            !snapshot["items"].isArray() || snapshot["items"].size() != 1)
            continue;
        const auto item = find(state, "items", key);
        if (!item.isObject())
            continue;
        bool changed = false;
        auto *savedTracks = snapshot["items"][0]["tracks"].getArray();
        auto *liveTracks = item["tracks"].getArray();
        if (savedTracks != nullptr && liveTracks != nullptr)
            for (auto &savedTrack : *savedTracks) {
                if (!savedTrack.isObject())
                    continue;
                const auto channelId = savedTrack["channelId"].toString();
                for (const auto &liveTrack : *liveTracks) {
                    if (liveTrack["channelId"].toString() != channelId)
                        continue;
                    auto *savedObject = savedTrack.getDynamicObject();
                    for (const auto *field : {"patchId", "subPatchId"}) {
                        const auto fieldKey = juce::Identifier(field);
                        const bool savedHasField = savedObject->hasProperty(fieldKey);
                        const auto savedValue =
                            savedHasField ? savedObject->getProperty(fieldKey).toString() : juce::String();
                        const auto liveValue = liveTrack[field];
                        if (savedValue.isNotEmpty())
                            continue;
                        if (liveValue.toString().isNotEmpty()) {
                            put(savedTrack, field, liveValue.clone());
                            changed = true;
                        } else if (!savedHasField) {
                            // Old writers always stored the field as null; absent
                            // and null must compare equal in the saved content.
                            put(savedTrack, field, {});
                            changed = true;
                        }
                    }
                    break;
                }
            }
        if (changed && savedTracks != nullptr)
            if (auto *savedChannels = snapshot["channels"].getArray())
                for (auto &savedChannel : *savedChannels) {
                    if (!savedChannel.isObject())
                        continue;
                    const auto liveChannel = find(state, "channels", savedChannel["id"].toString());
                    if (!liveChannel.isObject())
                        continue;
                    for (const auto &savedTrack : *savedTracks)
                        if (savedTrack["channelId"].toString() == savedChannel["id"].toString()) {
                            const auto patchId = savedTrack["patchId"].toString();
                            if (patchId.isEmpty() || find(savedChannel, "patches", patchId).isObject())
                                continue;
                            const auto patch = find(liveChannel, "patches", patchId);
                            if (patch.isObject())
                                array(savedChannel, "patches").add(patch.clone());
                            break;
                        }
                }
        if (changed)
            updates.emplace_back(key, juce::JSON::toString(snapshot, true, 17));
    }
    for (const auto &update : updates)
        library->setProperty(juce::Identifier(update.first), update.second);
}
inline juce::var importDemo(const juce::String &bytes) {
    if (bytes.getNumBytesAsUTF8() > 1024 * 1024)
        throw std::runtime_error("Editor show exceeds 1 MiB");
    juce::var state;
    if (juce::JSON::parse(bytes, state).failed() || state["schema"].toString() != "mr-live-demo-show" ||
        static_cast<int>(state["version"]) != 3)
        throw std::runtime_error("Expected editor show v3 JSON");
    migrateSavedPatchAssignments(state);
    validate(state);
    state.getDynamicObject()->removeProperty("schema");
    state.getDynamicObject()->removeProperty("version");
    return state;
}
inline juce::var pick(const juce::var &source, const std::set<juce::String> &ids) {
    auto result = object();
    if (const auto *values = source.getDynamicObject())
        for (const auto &pair : values->getProperties())
            if (ids.contains(pair.name.toString()))
                put(result, pair.name, pair.value.clone());
    return result;
}
inline juce::var itemVersion(const juce::var &state, const juce::String &id) {
    auto item = find(state, "items", id).clone();
    if (!item.isObject())
        throw std::runtime_error("Missing item");
    std::set<juce::String> trackIds, recordIds, targets{id};
    if (auto *entries = property(item, "tracks").getArray())
        for (auto &track : *entries) {
            trackIds.insert(track["id"].toString());
            if (track["recordTrack"].isObject())
                recordIds.insert(track["recordTrack"]["id"].toString());
            // #289 A: the per-item channel patch assignment is part of the saved
            // item version (it is not transport/runtime state).
        }
    if (const auto *sections = item["sections"].getArray())
        for (const auto &section : *sections)
            targets.insert(section["id"].toString());
    auto snapshot = object();
    juce::Array<juce::var> itemArray;
    itemArray.add(item);
    put(snapshot, "items", itemArray);
    put(snapshot, "channels", state["channels"].clone());
    auto perf = object();
    put(perf, "stopRecord", true);
    const auto source = performance(state);
    put(perf, "modes", pick(source["modes"], targets));
    put(perf, "takes", pick(source["takes"], recordIds));
    put(perf, "clips", pick(source["clips"], trackIds));
    for (const auto *key : {"chords", "itemColors", "itemWorkflow"})
        put(perf, key, pick(source[key], {id}));
    if (source["arrangement"][juce::Identifier(id)].hasProperty("length") || source["arrangement"][juce::Identifier(id)].hasProperty("start")) {
        auto row = object();
        put(row, "scale", 16);
        put(row, "lead", 0);
        for(const auto* key:{"start","length"})
            if(source["arrangement"][juce::Identifier(id)].hasProperty(key))put(row,key,source["arrangement"][juce::Identifier(id)][key]);
        auto arrangement = object();
        put(arrangement, id, row);
        put(perf, "arrangement", arrangement);
    }
    if (source["nativePlayback"][juce::Identifier(id)].isObject())
        put(perf, "nativePlayback", pick(source["nativePlayback"], {id}));
    put(snapshot, "performance", perf);
    return snapshot;
}
inline juce::var content(const juce::var &snapshot) {
    auto result = object();
    auto item = snapshot["items"][0].clone();
    if (!item.isObject())
        return {};
    item.getDynamicObject()->removeProperty("folder");
    // Browser placement is show organization, not the saved musical item version.
    item.getDynamicObject()->removeProperty("browserOrder");
    juce::Array<juce::var> channels;
    if (const auto *entries = snapshot["channels"].getArray())
        for (const auto &channel : *entries) {
            auto selected = object();
            put(selected, "id", channel["id"]);
            put(selected, "type", channel["type"]);
            juce::Array<juce::var> patches;
            juce::var track;
            if (const auto *tracks = item["tracks"].getArray())
                for (const auto &candidate : *tracks)
                    if (candidate["channelId"] == channel["id"])
                        track = candidate;
            const auto defaultPatch = track["patchId"].toString();
            if (const auto *bank = channel["patches"].getArray())
                for (const auto &patch : *bank) {
                    const auto patchId = patch["id"].toString();
                    const auto *order = track["patchOrder"].getArray();
                    // Saved Browser versions include blocks shown on the track and
                    // the item's default channel patch (#289 A).
                    if ((order && order->contains(patch["id"])) ||
                        (defaultPatch.isNotEmpty() && defaultPatch == patchId))
                        patches.add(patch.clone());
                }
            put(selected, "patches", patches);
            channels.add(selected);
        }
    auto perf = snapshot["performance"].clone();
    auto progressOnly = object();
    if (const auto *workflows = perf["itemWorkflow"].getDynamicObject())
        for (const auto &pair : workflows->getProperties())
            if (static_cast<double>(pair.value["progress"]) != 0) {
                auto row = object();
                put(row, "progress", pair.value["progress"]);
                put(progressOnly, pair.name, row);
            }
    put(perf, "itemWorkflow", progressOnly);
    put(result, "item", item);
    put(result, "channels", channels);
    put(result, "performance", perf);
    return result;
}
inline bool itemDirty(const juce::var &state, const juce::String &id) {
    const auto saved = state["library"][juce::Identifier(id)];
    if (!saved.isString())
        return true;
    const auto parsed = juce::JSON::parse(saved.toString());
    return canonical(content(itemVersion(state, id))) != canonical(content(parsed));
}
// Cache opaque/non-metadata content once when cards are created. Notes,
// names, musical labels/colors and progress then compare only bounded data.
inline juce::var itemMetadata(const juce::var& item,const juce::var& perf,const juce::String& id,const juce::var* modeMetadata=nullptr,const juce::var* clipMetadata=nullptr) {
    auto result=object();
    for(const auto* key:{"name","key","notes","sections"})put(result,key,item[key]);
    put(result,"chords",perf["chords"][juce::Identifier(id)]);
    put(result,"nativePlayback",perf["nativePlayback"][juce::Identifier(id)]);
    if(clipMetadata)put(result,"clips",*clipMetadata);
    else {
        std::set<juce::String> tracks;
        if(const auto* rows=item["tracks"].getArray())for(const auto& track:*rows)tracks.insert(track["id"].toString());
        put(result,"clips",pick(perf["clips"],tracks));
    }
    if(modeMetadata)put(result,"modes",*modeMetadata);
    else {
        std::set<juce::String> targets{id};
        if(const auto* sections=item["sections"].getArray())for(const auto& section:*sections)
            targets.insert(section["id"].toString());
        put(result,"modes",pick(perf["modes"],targets));
    }
    auto range=object();
    for(const auto* key:{"start","length"})if(perf["arrangement"][juce::Identifier(id)].hasProperty(key))put(range,key,perf["arrangement"][juce::Identifier(id)][key]);
    put(result,"range",range);
    put(result,"progress",static_cast<double>(perf["itemWorkflow"][juce::Identifier(id)]["progress"]));
    return result;
}
struct ItemMetadataDirtyState {
    bool otherChanges{true};
    juce::var savedMetadata;
    // Persistent UI patches retain these subtrees on Notes/name edits. Cache
    // the bounded mode subset so typing never rescans the show's modes map.
    mutable juce::var modeSource,modeSections,cachedModeMetadata;
    mutable bool modesCached{};
    mutable juce::var clipSource,clipTracks,cachedClipMetadata;
    mutable bool clipsCached{},cachedClipsDirty{};
    mutable juce::var nativeSource;
    mutable bool nativeCached{},cachedNativeDirty{};
    bool dirty(const juce::var& state,const juce::String& id)const {
        if(otherChanges)return true;
        const auto item=find(state,"items",id),perf=state["performance"];
        const auto modes=perf["modes"],sections=item["sections"];
        const auto clips=perf["clips"],tracks=item["tracks"];
        const bool modeChanged=!modesCached||modes.getDynamicObject()!=modeSource.getDynamicObject()||sections.getArray()!=modeSections.getArray();
        const bool clipsChanged=!clipsCached||clips.getDynamicObject()!=clipSource.getDynamicObject()||tracks.getArray()!=clipTracks.getArray();
        if(modeChanged||clipsChanged){
            const auto metadata=itemMetadata(item,perf,id,modeChanged?nullptr:&cachedModeMetadata,clipsChanged?nullptr:&cachedClipMetadata);
            if(modeChanged){cachedModeMetadata=metadata["modes"];modeSource=modes;modeSections=sections;modesCached=true;}
            if(clipsChanged){cachedClipMetadata=metadata["clips"];clipSource=clips;clipTracks=tracks;clipsCached=true;
                cachedClipsDirty=!sameShowValue(savedMetadata["clips"],cachedClipMetadata);}
        }
        const auto native=perf["nativePlayback"][juce::Identifier(id)];
        if(!nativeCached||native.getDynamicObject()!=nativeSource.getDynamicObject()){
            nativeSource=native;nativeCached=true;cachedNativeDirty=!sameShowValue(savedMetadata["nativePlayback"],native);
        }
        if(cachedClipsDirty||cachedNativeDirty)return true;
        // Already compared unchanged audio domains once. Alias their saved
        // values so Notes do not walk hundreds of unchanged regions per card.
        const auto savedClips=savedMetadata["clips"];
        auto metadata=itemMetadata(item,perf,id,&cachedModeMetadata,&savedClips);
        put(metadata,"nativePlayback",savedMetadata["nativePlayback"]);
        return !sameShowValue(savedMetadata,metadata);
    }
};
inline ItemMetadataDirtyState itemMetadataDirtyState(const juce::var& state,const juce::String& id) {
    ItemMetadataDirtyState result;
    const auto saved=state["library"][juce::Identifier(id)];
    if(!saved.isString())return result;
    auto stored=content(juce::JSON::parse(saved.toString()));
    auto current=content(itemVersion(state,id));
    const auto currentModes=current["performance"]["modes"];
    const auto currentClips=current["performance"]["clips"];
    const auto currentNative=current["performance"]["nativePlayback"][juce::Identifier(id)];
    result.savedMetadata=itemMetadata(stored["item"],stored["performance"],id);
    if(stored["item"].isObject()&&current["item"].isObject()) {
        for(auto* value:{&stored,&current}) {
            for(const auto* key:{"name","key","notes","sections"})
                (*value)["item"].getDynamicObject()->removeProperty(key);
            for(const auto* key:{"chords","itemWorkflow","arrangement","modes","clips","nativePlayback"})
                (*value)["performance"].getDynamicObject()->removeProperty(key);
        }
        result.otherChanges=!sameShowValue(current,stored);
        if(!result.otherChanges){
            result.modeSource=state["performance"]["modes"];
            result.modeSections=find(state,"items",id)["sections"];
            result.cachedModeMetadata=currentModes;result.modesCached=true;
            result.clipSource=state["performance"]["clips"];
            result.clipTracks=find(state,"items",id)["tracks"];
            result.cachedClipMetadata=currentClips;result.clipsCached=true;
            result.cachedClipsDirty=!sameShowValue(result.savedMetadata["clips"],currentClips);
            result.nativeSource=state["performance"]["nativePlayback"][juce::Identifier(id)];result.nativeCached=true;
            result.cachedNativeDirty=!sameShowValue(result.savedMetadata["nativePlayback"],currentNative);
        }
    }
    return result;
}
// Kept for the existing Notes regression entry point; the same cache now
// supports every metadata field that uses the local edit protocol.
using ItemNotesDirtyState=ItemMetadataDirtyState;
inline ItemMetadataDirtyState itemNotesDirtyState(const juce::var& state,const juce::String& id) {
    return itemMetadataDirtyState(state,id);
}
inline void saveItem(juce::var &state, const juce::String &id) {
    put(map(state, "library"), id, juce::JSON::toString(itemVersion(state, id), true, 17));
}
inline juce::var libraryItem(const juce::var &state, const juce::String &id) {
    const auto saved = state["library"][juce::Identifier(id)];
    if (saved.isString()) {
        const auto snapshot = juce::JSON::parse(saved.toString());
        if (snapshot["items"].isArray() && snapshot["items"].size() == 1)
            return snapshot["items"][0];
    }
    return find(state, "items", id);
}
// Browser uses its own authored sibling ordering, independently from the
// Setlist. Legacy shows without browserOrder retain folders-first/name order.
struct BrowserSibling {
    juce::String id, name;
    bool folder{}, ordered{};
    double order{};
};
inline std::vector<BrowserSibling> browserSiblings(const juce::var &state, const juce::var &parent) {
    std::vector<BrowserSibling> siblings;
    const auto append=[&](const juce::var& row, bool folder) {
        const auto rank=row["browserOrder"];
        const auto id=row["id"].toString();
        const auto display=folder?row:libraryItem(state,id);
        siblings.push_back({id,display["name"].toString(),folder,number(rank),number(rank)?static_cast<double>(rank):0.});
    };
    if(const auto* folders=state["folders"].getArray())
        for(const auto& row:*folders)if(row["parent"]==parent)append(row,true);
    if(const auto* items=state["items"].getArray())
        for(const auto& row:*items)if(row["folder"]==parent)append(row,false);
    std::stable_sort(siblings.begin(),siblings.end(),[](const auto& a,const auto& b){
        if(a.ordered!=b.ordered)return a.ordered;
        if(a.ordered&&a.order!=b.order)return a.order<b.order;
        if(!a.ordered&&a.folder!=b.folder)return a.folder;
        return a.name.compareNatural(b.name)<0;
    });
    return siblings;
}
enum class BrowserDropMode { before, after, into, appendRoot };
// The same validation is used by the visible drop indicator and by the commit.
inline bool browserDropAllowed(const juce::var& state,const juce::String& source,
                               bool sourceFolder,const juce::String& target,BrowserDropMode mode) {
    if(!find(state,sourceFolder?"folders":"items",source).isObject())return false;
    if(mode!=BrowserDropMode::appendRoot) {
        if(source==target)return false;
        if(!find(state,"folders",target).isObject() && !find(state,"items",target).isObject())return false;
        if(mode==BrowserDropMode::into && !find(state,"folders",target).isObject())return false;
    }
    const auto destination=mode==BrowserDropMode::appendRoot?juce::var():
        mode==BrowserDropMode::into?juce::var(target):
        find(state,"folders",target).isObject()?find(state,"folders",target)["parent"]:
                                                find(state,"items",target)["folder"];
    if(!destination.isVoid() && !find(state,"folders",destination.toString()).isObject())return false;
    if(!sourceFolder)return true;
    // A folder cannot be moved inside itself or any of its descendants.
    auto cursor=destination;
    std::set<juce::String> seen;
    while(!cursor.isVoid()) {
        const auto id=cursor.toString();
        if(id==source||!seen.insert(id).second)return false;
        const auto row=find(state,"folders",id);
        if(!row.isObject())return false;
        cursor=row["parent"];
    }
    return true;
}
inline bool moveBrowserRow(juce::var& state,const juce::String& source,bool sourceFolder,
                           const juce::String& target,BrowserDropMode mode) {
    if(!browserDropAllowed(state,source,sourceFolder,target,mode))return false;
    const auto targetFolder=find(state,"folders",target);
    const auto parent=mode==BrowserDropMode::appendRoot?juce::var():
        mode==BrowserDropMode::into?juce::var(target):
        targetFolder.isObject()?targetFolder["parent"]:find(state,"items",target)["folder"];
    auto siblings=browserSiblings(state,parent);
    siblings.erase(std::remove_if(siblings.begin(),siblings.end(),
                                 [&](const auto& row){return row.id==source;}),siblings.end());
    std::size_t position=siblings.size();
    if(mode!=BrowserDropMode::appendRoot && mode!=BrowserDropMode::into) {
        const auto found=std::find_if(siblings.begin(),siblings.end(),
                                      [&](const auto& row){return row.id==target;});
        if(found==siblings.end())return false;
        position=static_cast<std::size_t>(std::distance(siblings.begin(),found));
        if(mode==BrowserDropMode::after)++position;
    }
    siblings.insert(siblings.begin()+static_cast<std::ptrdiff_t>(position),
                    {source,{},sourceFolder,false,0.});
    auto& moving=require(state,sourceFolder?"folders":"items",source);
    put(moving,sourceFolder?"parent":"folder",parent);
    for(std::size_t i=0;i<siblings.size();++i) {
        auto& entry=require(state,siblings[i].folder?"folders":"items",siblings[i].id);
        put(entry,"browserOrder",static_cast<int>(i));
    }
    return true;
}
inline juce::var activeSetlist(const juce::var &state) {
    return find(state, "setlists", state["activeSetlist"].toString());
}
inline juce::var &activeEntries(juce::var &state) {
    return property(require(state, "setlists", state["activeSetlist"].toString()), "entries");
}
inline void insertEntry(juce::var &state, const juce::String &itemId, const juce::String &target = {},
                        bool after = false) {
    if (!find(state, "items", itemId).isObject())
        throw std::runtime_error("Missing item");
    auto &entries = *activeEntries(state).getArray();
    int at = entries.size();
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i]["id"].toString() == target) {
            at = i + (after ? 1 : 0);
            break;
        }
    auto entry = object();
    put(entry, "id", freshId());
    put(entry, "itemId", itemId);
    entries.insert(at, entry);
}
inline void reorderEntry(juce::var &state, const juce::String &source, const juce::String &target, bool after) {
    if (source == target)
        return;
    auto &entries = *activeEntries(state).getArray();
    int index = -1;
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i]["id"].toString() == source)
            index = i;
    if (index < 0)
        throw std::runtime_error("Missing setlist entry");
    const auto moved = entries[index];
    entries.remove(index);
    int at = entries.size();
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i]["id"].toString() == target) {
            at = i + (after ? 1 : 0);
            break;
        }
    entries.insert(at, moved);
}
inline juce::String addItem(juce::var &state, const juce::String &name, const juce::var &folder = {}) {
    auto item = object();
    const auto id = freshId();
    put(item, "id", id);
    put(item, "name", name.trim());
    put(item, "folder", folder);
    put(item, "tempo", 120);
    put(item, "meter", "4/4");
    put(item, "key", "");
    put(item, "duration", 0);
    put(item, "bars", 16.);
    put(item, "notes", "");
    juce::Array<juce::var> tracks;
    if (const auto *channels = state["channels"].getArray()) for (const auto &channel : *channels) {
        auto track = object(); put(track, "id", freshId()); put(track, "channelId", channel["id"]);
        for (const auto *key : {"patchId", "subPatchId", "media", "recordTrack"}) put(track, key, {});
        put(track, "patchOrder", juce::Array<juce::var>()); put(track, "spans", object()); put(track, "playbackGain", 0.);
        tracks.add(track);
    }
    put(item, "tracks", tracks);
    put(item, "sections", juce::Array<juce::var>());
    array(state, "items").add(item);
    saveItem(state, id);
    return id;
}
inline void setWorkflow(juce::var &state, const juce::String &id, const juce::String &itemStatus,
                        const juce::String &priority, int percent) {
    auto value = workflow(state, id);
    put(value, "status", itemStatus);
    put(value, "priority", priority);
    put(value, "progress", percent);
    put(map(performance(state), "itemWorkflow"), id, value);
}
inline juce::String addTask(juce::var &state, const juce::String &itemId, const juce::var &parent,
                            const juce::String &name) {
    auto task = object();
    const auto id = freshId();
    put(task, "id", id);
    put(task, "parent", parent);
    put(task, "title", name.trim());
    put(task, "status", "Todo");
    put(task, "progress", 0);
    array(map(performance(state), "itemTasks"), itemId).add(task);
    return id;
}
inline void removeTask(juce::var &state, const juce::String &itemId, const juce::String &taskId) {
    auto &entries = array(map(performance(state), "itemTasks"), itemId);
    std::set<juce::String> removed{taskId};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto &task : entries)
            if (removed.contains(task["parent"].toString()))
                changed = removed.insert(task["id"].toString()).second || changed;
    }
    for (int i = entries.size(); --i >= 0;)
        if (removed.contains(entries[i]["id"].toString()))
            entries.remove(i);
}
} // namespace ui::live
