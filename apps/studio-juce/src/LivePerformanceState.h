#pragma once
#include "LiveShowState.h"
#include <mrs/performance.hpp>

namespace ui::live {
inline mrs::PerformanceMode performanceMode(const juce::String& value){
    if(value.isEmpty()||value=="normal")return mrs::PerformanceMode::normal;
    if(value=="skip")return mrs::PerformanceMode::skip;
    if(value=="stop")return mrs::PerformanceMode::stop;
    if(value=="loop")return mrs::PerformanceMode::loop;
    throw std::runtime_error("Invalid performance mode");
}
inline juce::String performanceModeName(mrs::PerformanceMode mode){
    switch(mode){case mrs::PerformanceMode::normal:return "normal";case mrs::PerformanceMode::skip:return "skip";
        case mrs::PerformanceMode::stop:return "stop";case mrs::PerformanceMode::loop:return "loop";}
    throw std::runtime_error("Invalid performance mode");
}
inline juce::String performanceModeLabel(mrs::PerformanceMode mode){
    switch(mode){case mrs::PerformanceMode::normal:return juce::String::fromUTF8("Обычный");case mrs::PerformanceMode::skip:return juce::String::fromUTF8("Пропуск");
        case mrs::PerformanceMode::stop:return juce::String::fromUTF8("Доиграть и остановиться");case mrs::PerformanceMode::loop:return juce::String::fromUTF8("Зациклить");}
    return {};
}
inline mrs::PerformanceMode itemPerformanceMode(const juce::var& state,const juce::String& target){
    return performanceMode(state["performance"]["modes"][juce::Identifier(target)].toString());
}
inline void validatePerformance(const juce::var& state){
    const auto perf=state["performance"],modes=perf["modes"];
    if(!modes.isVoid()){
        if(!modes.isObject()||modes.getDynamicObject()->getProperties().size()>16384)throw std::runtime_error("Invalid performance modes");
        for(const auto& pair:modes.getDynamicObject()->getProperties()){
            if(!pair.value.isString()||pair.value.toString().isEmpty())throw std::runtime_error("Invalid performance mode");
            (void)performanceMode(pair.value.toString());
        }
    }
    for(const auto* key:{"backToStart","stopRecord"})if(!perf[key].isVoid()&&!perf[key].isBool())throw std::runtime_error("Invalid performance option");
}
inline void setPerformanceMode(juce::var& state,const juce::String& item,const juce::String& target,mrs::PerformanceMode mode){
    const auto source=find(state,"items",item);bool found=source.isObject()&&target==item;
    if(const auto* rows=source["sections"].getArray())for(const auto& row:*rows)found=found||row["id"].toString()==target;
    if(!found)throw std::runtime_error("Performance target was removed");
    if(mode==mrs::PerformanceMode::normal && !state["performance"]["modes"].hasProperty(juce::Identifier(target)))return;
    auto& modes=map(performance(state),"modes");
    if(mode==mrs::PerformanceMode::normal)modes.getDynamicObject()->removeProperty(juce::Identifier(target));
    else put(modes,target,performanceModeName(mode));
}
inline juce::String performanceKey(const juce::var& state,const juce::String& item){
    auto result=juce::String();const auto perf=state["performance"];
    result<<performanceModeName(itemPerformanceMode(state,item))<<":"<<perf["backToStart"].toString()<<":"<<perf["stopRecord"].toString();
    const auto source=find(state,"items",item);
    if(const auto* rows=source["sections"].getArray())for(const auto& row:*rows)result<<":"<<row["id"].toString()<<"="<<performanceModeName(itemPerformanceMode(state,row["id"].toString()));
    result<<":"<<source["bars"].toString()<<":"<<perf["arrangement"][juce::Identifier(item)]["length"].toString();return result;
}
inline double performanceEndBars(const juce::var& state,const juce::String& item,double renderedBars){
    const auto source=find(state,"items",item);if(!source.isObject())throw std::runtime_error("Performance item was removed");
    if(!std::isfinite(renderedBars)||renderedBars<0.)throw std::runtime_error("Invalid rendered performance extent");
    const auto explicitEnd=state["performance"]["arrangement"][juce::Identifier(item)]["length"];
    const double fallback=source["bars"].isVoid()?16.:static_cast<double>(source["bars"]);
    const double bars=explicitEnd.isVoid()?(std::min)(4096.,renderedBars>0.?renderedBars:(std::max)(1.,fallback)):static_cast<double>(explicitEnd);
    if(!std::isfinite(bars)||bars<=0.||bars>4096.)throw std::runtime_error("Invalid performance item end");
    return bars;
}
inline mrs::PerformancePlan preparePerformancePlan(const juce::var& state,const juce::String& item,
                                                  const mrs::TimeMap& time,const std::vector<mrs::ArrangerSection>& sections,
                                                  std::uint32_t rate,double renderedBars){
    validatePerformance(state);time.validate();if(rate<8000||rate>768000)throw std::runtime_error("Invalid performance sample rate");
    const auto& meter=time.meters.front();const double ticksPerBar=4.*mrs::ppq*meter.numerator/meter.denominator;
    const mrs::Timeline clock(time,rate);mrs::PerformancePlan result;
    result.end=clock.to_samples(static_cast<mrs::Tick>(std::llround(performanceEndBars(state,item,renderedBars)*ticksPerBar)));
    result.mode=itemPerformanceMode(state,item);
    result.back_to_start=static_cast<bool>(state["performance"]["backToStart"]);
    result.stop_record=state["performance"]["stopRecord"].isVoid()||static_cast<bool>(state["performance"]["stopRecord"]);
    for(const auto& section:sections){
        const auto start=clock.to_samples(section.start),end=clock.to_samples(section.end);
        if(start>=result.end)continue;
        auto mode=itemPerformanceMode(state,juce::String::fromUTF8(section.id.value.c_str()));
        // A shortened item wins over an unfinished section's stop/loop.
        if(end>result.end && mode!=mrs::PerformanceMode::skip)mode=mrs::PerformanceMode::normal;
        result.sections.push_back({start,(std::min)(end,result.end),mode});
    }
    result.validate();return result;
}
}
