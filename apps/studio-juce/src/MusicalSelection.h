#pragma once
#include "Controls.h"
#include <mrs/core.hpp>
#include <set>
#include <algorithm>
namespace ui {
template<class T> mrs::Tick itemStart(const T& item){if constexpr(requires{item.tick;})return item.tick;else return item.start;}
template<class T> mrs::Tick itemEnd(const T& item){if constexpr(requires{item.tick;})return item.tick;else return item.end;}
template<class T> mrs::Tick groupStart(const std::vector<T>& values){mrs::Tick result=mrs::max_tick;for(const auto& v:values)result=std::min(result,itemStart(v));return result;}
template<class T> mrs::Tick groupEnd(const std::vector<T>& values){mrs::Tick result=0;for(const auto& v:values)result=std::max(result,itemEnd(v));return result;}
// UI-only selection/preview; project state always goes through Application commands.
template<class T> struct MusicalSelection {
    std::set<std::string> ids,base;
    std::vector<T> originals,previews;
    bool selecting{},valid{true};juce::Point<float> origin;juce::Rectangle<float> box;
    void set(const std::vector<mrs::Id>& values){ids.clear();for(const auto& id:values)ids.insert(id.value);}
    std::optional<mrs::Id> primary()const{return ids.empty()?std::nullopt:std::optional<mrs::Id>{mrs::Id{*ids.begin()}};}
    void toggle(const mrs::Id& id){if(ids.contains(id.value))ids.erase(id.value);else ids.insert(id.value);}
    std::vector<T> selected(const std::vector<T>& lane)const{std::vector<T> out;for(const auto& item:lane)if(ids.contains(item.id.value))out.push_back(item);return out;}
    void sync(const std::vector<T>& lane){std::erase_if(ids,[&](const auto& id){return std::none_of(lane.begin(),lane.end(),[&](const auto& item){return item.id.value==id;});});}
    void beginBox(juce::Point<float> p,bool add){if(!add)ids.clear();base=ids;origin=p;selecting=true;}
    template<class Rect> void boxDrag(juce::Point<float> p,const std::vector<T>& lane,Rect rect,juce::Rectangle<float> clip){box=juce::Rectangle<float>(origin,p).getIntersection(clip);ids=base;for(const auto& item:lane)if(rect(item).intersects(box))ids.insert(item.id.value);}
    void endBox(){selecting=false;base.clear();box={};}
    void cancel(){if(selecting)ids=base;endBox();originals.clear();previews.clear();valid=true;}
    void paintBox(juce::Graphics& g,juce::Colour colour=juce::Colour(accent))const{if(!selecting)return;g.setColour(colour.withAlpha(.2f));g.fillRect(box);g.setColour(colour);g.drawRect(box,1.f);}
};
template<class T,class X,class Snap> mrs::Tick groupDelta(const std::vector<T>& values,const T& lead,mrs::Tick raw,bool snap,const std::vector<T>& lane,bool copy,X x,Snap snapped){
    const auto first=groupStart(values),last=groupEnd(values),start=itemStart(lead);const auto clipped=std::clamp(raw,-first,mrs::max_tick-last);
    const auto target=start+clipped;auto delta=std::clamp((snap?snapped(target):target)-start,-first,mrs::max_tick-last);
    double distance=7.;for(const auto& other:lane)if(copy||std::none_of(values.begin(),values.end(),[&](const auto& v){return v.id==other.id;}))for(const auto candidate:{itemEnd(other)-first,itemStart(other)-last}){if(candidate<-first||candidate>mrs::max_tick-last)continue;const auto d=std::abs(x(start+clipped)-x(start+candidate));if(d<distance){distance=d;delta=candidate;}}
    return delta;
}
template<class T> bool groupValid(const std::vector<T>& values,const std::vector<T>& lane,const std::set<std::string>& ids,bool copy){
    if constexpr(requires(T t){t.tick;})return true;else{for(const auto& value:values)for(const auto& other:lane)if((copy||!ids.contains(other.id.value))&&value.start<other.end&&other.start<value.end)return false;return true;}
}
}
