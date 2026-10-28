#pragma once
#include "../apps/studio-juce/src/LivePlaybackState.h"
namespace ui::live::test {
inline void performancePolicies(){
    const auto check=[](bool pass,const char* message){if(!pass)throw std::runtime_error(message);};
    auto state=empty();const auto item=addItem(state,"Performance");
    auto section=object();put(section,"id","part");put(section,"name","Intro");put(section,"start",0.);put(section,"length",4.);put(section,"color","blue");
    put(require(state,"items",item),"sections",juce::Array<juce::var>{section});
    {const auto newPlan=preparePerformancePlan(state,item,mrs::TimeMap{}, {},48000,0.);
        check(newPlan.end==96000,"A new item without bars has the same one-bar fallback as the timeline");}
    put(require(state,"items",item),"bars",8.);
    check(itemPerformanceMode(state,item)==mrs::PerformanceMode::normal,"Missing legacy mode defaults to normal");
    saveItem(state,item);setPerformanceMode(state,item,"part",mrs::PerformanceMode::loop);
    check(itemDirty(state,item),"Section mode belongs to saved item content");
    const auto saved=itemVersion(state,item);
    check(saved["performance"]["modes"]["part"].toString()=="loop","Browser version retains section mode");
    setPerformanceMode(state,item,item,mrs::PerformanceMode::stop);
    auto reopened=juce::JSON::parse(juce::JSON::toString(state));validatePerformance(reopened);
    check(itemPerformanceMode(reopened,item)==mrs::PerformanceMode::stop && itemPerformanceMode(reopened,"part")==mrs::PerformanceMode::loop,
          "Item/section policy round-trips without normalising saved show");
    setPerformanceMode(state,item,item,mrs::PerformanceMode::normal);
    check(!state["performance"]["modes"].hasProperty(juce::Identifier(item)),"Returning to normal removes redundant override");
    bool rejected=false;try{setPerformanceMode(state,item,"removed",mrs::PerformanceMode::stop);}catch(...){rejected=true;}
    check(rejected,"A stale context target cannot receive a mode");
    auto invalid=state.clone();put(map(performance(invalid),"modes"),item,"invalid");
    rejected=false;try{validatePerformance(invalid);}catch(...){rejected=true;}check(rejected,"Unknown persisted mode is rejected");
    {auto order=empty();const auto first=addItem(order,"First"),skipped=addItem(order,"Skip"),last=addItem(order,"Last");
        setPerformanceMode(order,skipped,skipped,mrs::PerformanceMode::skip);put(order,"currentItem",first);
        auto list=activeSetlist(order);auto& entries=array(list,"entries");
        for(const auto& target:{first,skipped,last}){auto entry=object();put(entry,"id",target+"-entry");put(entry,"itemId",target);entries.add(entry);}
        const auto initial=entries[0]["id"].toString();
        check(nextPerformanceEntry(order,initial)["itemId"].toString()==last,"Auto next skips mode skip entries");
        auto repeated=object();put(repeated,"id","repeat-first");put(repeated,"itemId",first);entries.add(repeated);
        put(order,"currentItem",last);
        check(nextPerformanceEntry(order,entries[2]["id"].toString())["id"].toString()=="repeat-first","Repeated item is resolved by entry identity");
        put(order,"currentItem",first);
        check(nextPerformanceEntry(order,"repeat-first").isVoid(),"Last repeated entry does not wrap into an infinite show");
        setPerformanceMode(order,last,last,mrs::PerformanceMode::skip);
        check(nextPerformanceEntry(order,initial)["id"].toString()=="repeat-first","Skipped successors still reach the next distinct entry");
    }
    mrs::TimeMap time;time.tempos={{0,120.}};time.meters={{1,4,4}};
    std::vector<mrs::ArrangerSection> parts{{{"part"},"Intro",0,16*mrs::ppq,{}}};
    auto plan=preparePerformancePlan(state,item,time,parts,48000,12.);
    check(plan.end==1152000 && plan.sections[0].end==384000,"Implicit item end follows longer real rendered extent through Core Timeline");
    put(map(map(performance(state),"arrangement"),item),"length",2.);
    plan=preparePerformancePlan(state,item,time,parts,48000,12.);
    check(plan.end==192000 && plan.sections[0].mode==mrs::PerformanceMode::normal,
          "Explicit shortened end wins over unfinished section loop without deleting the authored section");
    check(static_cast<double>(find(state,"items",item)["sections"][0]["length"])==4.,"Policy preparation never rewrites hidden content");
    check(mrs::performanceDecision(plan,plan.end).action==mrs::PerformanceAction::next,"Normal end describes next-item intent, not fake UI playback");
    setPerformanceMode(state,item,item,mrs::PerformanceMode::stop);plan=preparePerformancePlan(state,item,time,parts,48000,0.);
    check(mrs::performanceDecision(plan,plan.end).action==mrs::PerformanceAction::stop,"Stop item holds exact visible end");
    setPerformanceMode(state,item,item,mrs::PerformanceMode::loop);plan=preparePerformancePlan(state,item,time,parts,48000,0.);
    check(mrs::performanceDecision(plan,plan.end).target==0 && mrs::performanceDecision(plan,plan.end).action==mrs::PerformanceAction::loop,"Loop item returns to musical origin");
    mrs::PerformancePlan boundaries;boundaries.end=1000;boundaries.sections={{100,200,mrs::PerformanceMode::skip},{300,400,mrs::PerformanceMode::stop},{500,600,mrs::PerformanceMode::loop}};boundaries.validate();
    check(mrs::nextPerformanceBoundary(boundaries,0)==100 && mrs::performanceDecision(boundaries,150).target==200,"Seek into skip resolves to its end and future entry is a boundary");
    check(mrs::performanceDecision(boundaries,400).action==mrs::PerformanceAction::stop && mrs::performanceDecision(boundaries,600).target==500,"Section stop/loop use the exact end sample");
    check(mrs::performanceDecision(boundaries,450).action==mrs::PerformanceAction::none,"Seeking beyond a finished section does not fire a stale action");
    check(mrs::nextPerformanceBoundary(boundaries,100,mrs::LoopRange{50,900})==900 &&
          mrs::performanceDecision(boundaries,150,mrs::LoopRange{50,900}).action==mrs::PerformanceAction::none &&
          mrs::performanceDecision(boundaries,900,mrs::LoopRange{50,900}).target==50,"Valid explicit manual loop overrides section policy");
    check(mrs::performanceDecision(boundaries,1000,mrs::LoopRange{0,2000}).action==mrs::PerformanceAction::next,"Manual loop outside item extent cannot override its hard end");
    auto touching=boundaries;touching.sections={{0,100,mrs::PerformanceMode::loop},{100,200,mrs::PerformanceMode::skip}};
    check(mrs::performanceDecision(touching,100).action==mrs::PerformanceAction::loop &&
          mrs::performanceDecision(touching,100,{},false).action==mrs::PerformanceAction::skip,
          "Natural loop completion wins at a touching seam, while explicit seek enters the skipped section");
    boundaries.mode=mrs::PerformanceMode::skip;
    check(mrs::performanceDecision(boundaries,0).action==mrs::PerformanceAction::next,"Skipped item requests sequential advance");
}
}
