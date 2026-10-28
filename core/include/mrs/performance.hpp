#pragma once
#include <mrs/core.hpp>
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace mrs {
// Immutable policy prepared outside audio processing; AudioEngine consumes it
// on exact sample boundaries within the device callback.
enum class PerformanceMode { normal, skip, stop, loop };
struct PerformanceSection {
    Sample start{}, end{};
    PerformanceMode mode{PerformanceMode::normal};
};
struct PerformancePlan {
    Sample end{};
    PerformanceMode mode{PerformanceMode::normal};
    bool back_to_start{}, stop_record{true};
    std::vector<PerformanceSection> sections;
    void validate() const {
        if(end<=0 || end>max_sample || sections.size()>128)throw std::invalid_argument("Invalid performance extent");
        const auto valid=[](PerformanceMode value){return value>=PerformanceMode::normal && value<=PerformanceMode::loop;};
        if(!valid(mode))throw std::invalid_argument("Invalid item performance mode");
        Sample previous{};
        for(const auto& row:sections){
            if(!valid(row.mode)||row.start<previous||row.end<=row.start||row.end>end)throw std::invalid_argument("Invalid performance section");
            previous=row.end;
        }
    }
};
enum class PerformanceAction { none, skip, stop, loop, next };
struct PerformanceDecision { PerformanceAction action{PerformanceAction::none}; Sample target{}; };
inline bool performanceManualLoop(const PerformancePlan& plan,const std::optional<LoopRange>& loop) noexcept {
    return loop && loop->start>=0 && loop->start<loop->end && loop->end<=plan.end;
}
// Bounded, allocation-free queries over a validated, immutable plan. They do
// not seek or publish state, and are not a UI-timer transport implementation.
inline PerformanceDecision performanceDecision(const PerformancePlan& plan,Sample at,
                                                std::optional<LoopRange> manualLoop={},bool arrivedThroughPlayback=true) noexcept {
    if(at<0)return {};
    if(plan.mode==PerformanceMode::skip)return {PerformanceAction::next,0};
    if(performanceManualLoop(plan,manualLoop))
        return at>=manualLoop->end?PerformanceDecision{PerformanceAction::loop,manualLoop->start}:PerformanceDecision{};
    if(at>=plan.end){
        if(plan.mode==PerformanceMode::loop)return {PerformanceAction::loop,0};
        return {plan.mode==PerformanceMode::stop?PerformanceAction::stop:PerformanceAction::next,plan.end};
    }
    // A naturally completed loop/stop wins over the next section's entry,
    // matching the accepted show workflow. Explicit seek does not re-fire it.
    if(arrivedThroughPlayback)for(const auto& row:plan.sections)if(at==row.end){
        if(row.mode==PerformanceMode::loop)return {PerformanceAction::loop,row.start};
        if(row.mode==PerformanceMode::stop)return {PerformanceAction::stop,row.end};
    }
    for(const auto& row:plan.sections)
        if(row.mode==PerformanceMode::skip && row.start<=at && at<row.end)return {PerformanceAction::skip,row.end};
    return {};
}
inline Sample nextPerformanceBoundary(const PerformancePlan& plan,Sample at,
                                       std::optional<LoopRange> manualLoop={},bool arrivedThroughPlayback=true) noexcept {
    if(performanceDecision(plan,at,manualLoop,arrivedThroughPlayback).action!=PerformanceAction::none)return at;
    if(performanceManualLoop(plan,manualLoop))return manualLoop->end;
    Sample next=plan.end;
    for(const auto& row:plan.sections){
        if(row.mode==PerformanceMode::skip && row.start>at)next=(std::min)(next,row.start);
        if((row.mode==PerformanceMode::loop || row.mode==PerformanceMode::stop) && row.end>at)next=(std::min)(next,row.end);
    }
    return next;
}
} // namespace mrs
