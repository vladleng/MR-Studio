#pragma once
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
inline juce::var importDemo(const juce::String &bytes) {
    if (bytes.getNumBytesAsUTF8() > 1024 * 1024)
        throw std::runtime_error("Editor show exceeds 1 MiB");
    juce::var state;
    if (juce::JSON::parse(bytes, state).failed() || state["schema"].toString() != "mr-live-demo-show" ||
        static_cast<int>(state["version"]) != 3)
        throw std::runtime_error("Expected editor show v3 JSON");
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
            put(track, "patchId", {});
            put(track, "subPatchId", {});
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
    if (source["arrangement"][juce::Identifier(id)].hasProperty("length")) {
        auto row = object();
        put(row, "scale", 16);
        put(row, "lead", 0);
        put(row, "length", source["arrangement"][juce::Identifier(id)]["length"]);
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
            if (const auto *bank = channel["patches"].getArray())
                for (const auto &patch : *bank)
                    if (const auto *order = track["patchOrder"].getArray(); order && order->contains(patch["id"]))
                        patches.add(patch.clone());
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
