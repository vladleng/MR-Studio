#pragma once
#include "LiveShowState.h"
#include <optional>
#include <vector>

namespace ui::live {
// Data-only message-thread edit protocol. A path contains object keys and
// stable array IDs; array positions are never retained across Undo/Redo.
struct EditStep {
    juce::String key;
    bool row{};
    EditStep(const char* name):key(name){}
    EditStep(juce::String name):key(std::move(name)){}
    bool operator==(const EditStep&)const=default;
    static EditStep id(juce::String value){EditStep step(std::move(value));step.row=true;return step;}
};
using EditPath=std::vector<EditStep>;
struct EditValue { bool present{}; juce::var value; };
inline bool sameEditValue(const EditValue& a,const EditValue& b) {
    return a.present==b.present&&(!a.present||sameShowValue(a.value,b.value));
}
inline EditValue editValue(const juce::var& root,const EditPath& path) {
    juce::var node=root;
    for(const auto& step:path) {
        if(step.row) {
            const auto* rows=node.getArray();bool found=false;
            if(rows)for(const auto& row:*rows)if(row["id"].toString()==step.key){node=row;found=true;break;}
            if(!found)throw std::runtime_error("Edit target no longer exists");
        }else {
            auto* object=node.getDynamicObject();
            if(!object||!object->hasProperty(juce::Identifier(step.key)))return {};
            node=object->getProperty(juce::Identifier(step.key));
        }
    }
    return {true,node};
}
inline juce::var shallowEditCopy(const juce::var& value) {
    if(const auto* rows=value.getArray())return juce::var(*rows);
    if(const auto* object=value.getDynamicObject()) {
        auto result=live::object();
        for(const auto& property:object->getProperties())put(result,property.name,property.value);
        return result;
    }
    throw std::runtime_error("Invalid edit container");
}
inline juce::var replaceEditValue(const juce::var& root,const EditPath& path,
                                  const EditValue& replacement,std::size_t at=0) {
    if(at==path.size()) {
        if(!replacement.present)throw std::runtime_error("Cannot remove edit root");
        return replacement.value;
    }
    auto result=shallowEditCopy(root);
    const auto& step=path[at];
    if(step.row) {
        auto* rows=result.getArray();bool found=false;
        if(rows)for(auto& row:*rows)if(row["id"].toString()==step.key){
            row=replaceEditValue(row,path,replacement,at+1);found=true;break;
        }
        if(!found)throw std::runtime_error("Edit row no longer exists");
    }else {
        auto* object=result.getDynamicObject();
        if(!object)throw std::runtime_error("Invalid edit object");
        const juce::Identifier key(step.key);
        if(at+1==path.size()) {
            if(replacement.present)object->setProperty(key,replacement.value);
            else object->removeProperty(key);
        }else {
            if(!object->hasProperty(key))throw std::runtime_error("Edit parent no longer exists");
            object->setProperty(key,replaceEditValue(object->getProperty(key),path,replacement,at+1));
        }
    }
    return result;
}
struct ValuePatch {
    EditPath path;
    EditValue before,after;
    bool changed()const{return !sameEditValue(before,after);}
    juce::var apply(const juce::var& state,bool forward)const {
        const auto& expected=forward?before:after;
        if(!sameEditValue(editValue(state,path),expected))
            throw std::runtime_error("Edit target changed; reopen its editor");
        const auto& target=forward?after:before;
        // Legacy structural/Core adapters may still mutate model nodes in place.
        // The model must never share a mutable leaf with the Undo snapshot.
        return replaceEditValue(state,path,{target.present,target.value.clone()});
    }
};
inline ValuePatch valuePatch(const juce::var& state,EditPath path,const juce::var& value) {
    if(path.empty())throw std::runtime_error("An edit needs an explicit target");
    // If a parent is absent, own the first missing property. Undo then removes
    // the newly created container as well, preserving absent vs null exactly.
    juce::var node=state;
    for(std::size_t i=0;i<path.size();++i) {
        EditPath prefix(path.begin(),path.begin()+static_cast<std::ptrdiff_t>(i+1));
        const auto current=editValue(state,prefix);
        if(!current.present) {
            if(path[i].row)throw std::runtime_error("Edit row no longer exists");
            juce::var nested=value.clone();
            for(std::size_t j=path.size();--j>i;) {
                if(path[j].row)throw std::runtime_error("Cannot invent an edit row");
                auto parent=object();put(parent,path[j].key,nested);nested=parent;
            }
            return {std::move(prefix),{}, {true,nested}};
        }
        node=current.value;
    }
    return {std::move(path),{true,node.clone()},{true,value.clone()}};
}
class JsonByteCounter final:public juce::OutputStream {
 public:
    bool write(const void*,std::size_t count)override{bytes+=static_cast<juce::int64>(count);return true;}
    juce::int64 getPosition()override{return bytes;}
    bool setPosition(juce::int64)override{return false;}
    void flush()override{}
 private:juce::int64 bytes{};
};
// Preserve the actual JUCE object/array/number formatting, but replace string
// VALUES with empty strings while counting their exact UTF-8/escape expansion.
// Opaque Base64 is scanned once without millions of virtual stream writes or
// allocating another encoded payload. Property keys still use JUCE verbatim.
inline juce::var editSizeShape(const juce::var& value,std::int64_t& stringBytes) {
    if(value.isString()) {
        const auto text=value.toString();
        const auto* bytes=reinterpret_cast<const unsigned char*>(text.toRawUTF8());
        for(;*bytes;++bytes) {
            ++stringBytes;
            if(*bytes=='"'||*bytes=='\\')++stringBytes;
            else if(*bytes<32)stringBytes+=(*bytes=='\b'||*bytes=='\f'||*bytes=='\t'||*bytes=='\r'||*bytes=='\n')?1:5;
        }
        return juce::String();
    }
    if(const auto* rows=value.getArray()) {
        juce::Array<juce::var> result;
        for(const auto& row:*rows)result.add(editSizeShape(row,stringBytes));
        return result;
    }
    if(const auto* source=value.getDynamicObject()) {
        auto result=object();
        for(const auto& property:source->getProperties())put(result,property.name,editSizeShape(property.value,stringBytes));
        return result;
    }
    return value;
}
inline std::int64_t editJsonBytes(const juce::var& value,std::size_t depth=0) {
    std::int64_t stringBytes{};
    const auto shape=editSizeShape(value,stringBytes);
    JsonByteCounter stream;
    juce::JSON::writeToStream(stream,shape,juce::JSON::FormatOptions().withIndentLevel(static_cast<int>(depth*2)));
    return stream.getPosition()+stringBytes;
}
inline std::int64_t patchByteDelta(const juce::var& state,const ValuePatch& patch,bool forward) {
    const auto& from=forward?patch.before:patch.after;
    const auto& to=forward?patch.after:patch.before;
    if(from.present&&to.present)
        return editJsonBytes(to.value,patch.path.size())-editJsonBytes(from.value,patch.path.size());
    // Adding/removing a property changes commas, keys and indentation. Measure
    // its parent using JUCE's actual serializer, not an estimated string delta.
    EditPath parent(patch.path.begin(),patch.path.end()-1);
    const auto old=editValue(state,parent).value;
    EditPath leaf{patch.path.back()};
    const auto next=replaceEditValue(old,leaf,to);
    return editJsonBytes(next,parent.size())-editJsonBytes(old,parent.size());
}
inline std::int64_t checkedEditBytes(std::int64_t bytes,std::int64_t delta) {
    if(bytes<0||delta>maximumShowBytes-bytes||bytes+delta<0)
        throw std::runtime_error("Show interface exceeds 64 MiB");
    return bytes+delta;
}
// One cache stamp covers structural, Core scalar, presentation and patch edits.
// Consumers cannot read a byte count without proving that its stamp is current.
struct EditStamp {
    std::uint64_t generation{},structure{},mix{},fold{},playback{},patch{};
    bool operator==(const EditStamp&)const=default;
};
class EditByteBudget {
 public:
    std::optional<std::int64_t> get(EditStamp current)const {
        return bytes&&current==stamp?bytes:std::nullopt;
    }
    void remember(EditStamp current,std::int64_t value){stamp=current;bytes=value;}
 private:EditStamp stamp;std::optional<std::int64_t> bytes;
};
// Conservative retained-payload estimate for UndoManager's bounded history.
// String backing may be shared, so this can overcount; it must not treat a
// 200-row task snapshot as the same constant size as a single bool.
inline std::size_t editUndoUnits(const juce::var& value) {
    std::size_t result=32;
    if(value.isString())return result+value.toString().getNumBytesAsUTF8();
    if(const auto* rows=value.getArray())for(const auto& row:*rows)result+=editUndoUnits(row);
    else if(const auto* object=value.getDynamicObject())
        for(const auto& property:object->getProperties())result+=32+editUndoUnits(property.value);
    return result;
}
enum class EditImpact:unsigned { none=0, notes=1, metadata=2, timeline=4, paint=8, cards=16, editor=32, fold=64, browser=128 };
inline EditImpact operator|(EditImpact a,EditImpact b){return static_cast<EditImpact>(static_cast<unsigned>(a)|static_cast<unsigned>(b));}
inline bool affects(EditImpact value,EditImpact bit){return (static_cast<unsigned>(value)&static_cast<unsigned>(bit))!=0;}
struct EditRevisions {
    std::uint64_t mutation{},metadata{},timeline{},paint{},cards{},editor{},browser{};
    void publish(EditImpact impact){
        ++mutation;
        if(affects(impact,EditImpact::metadata))++metadata;
        if(affects(impact,EditImpact::timeline))++timeline;
        if(affects(impact,EditImpact::paint))++paint;
        if(affects(impact,EditImpact::cards))++cards;
        if(affects(impact,EditImpact::editor))++editor;
        if(affects(impact,EditImpact::browser))++browser;
    }
};
} // namespace ui::live
