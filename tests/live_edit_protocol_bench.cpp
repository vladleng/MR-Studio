// Standalone manual JUCE Core data benchmark; no GUI, plugin or ASIO timing.
#include "../apps/studio-juce/src/LiveEditProtocol.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
using Clock=std::chrono::steady_clock;
double milliseconds(Clock::time_point from){return std::chrono::duration<double,std::milli>(Clock::now()-from).count();}
int main(){for(const int megabytes:{0,8}) {
    auto state=ui::live::empty();const auto id=ui::live::addItem(state,"Bench");
    auto payload=ui::live::object();ui::live::put(payload,"preset",juce::String::repeatedString("P",megabytes*1024*1024));
    ui::live::put(state,"opaque",payload);
    const ui::live::EditPath path{"items",ui::live::EditStep::id(id),"notes"};
    const auto begin=Clock::now();auto bytes=ui::live::editJsonBytes(state);const auto cold=milliseconds(begin);
    std::vector<double> whole,local;
    for(int i=0;i<12;++i) {
        const auto text="Notes update "+juce::String(i);
        auto start=Clock::now();auto copy=state.clone();ui::live::put(ui::live::require(copy,"items",id),"notes",text);
        volatile auto serialized=juce::JSON::toString(copy).getNumBytesAsUTF8();(void)serialized;
        whole.push_back(milliseconds(start));
        start=Clock::now();const auto patch=ui::live::valuePatch(state,path,text);
        bytes=ui::live::checkedEditBytes(bytes,ui::live::patchByteDelta(state,patch,true));state=patch.apply(state,true);
        local.push_back(milliseconds(start));
        if(state["opaque"].getDynamicObject()!=payload.getDynamicObject())return 2;
    }
    if(bytes!=ui::live::editJsonBytes(state))return 3;
    std::sort(whole.begin(),whole.end());std::sort(local.begin(),local.end());
    std::cout<<"opaque_MiB="<<megabytes<<" trials=12 cold_budget_ms="<<cold
        <<" whole_clone_json_p50_ms="<<whole[6]<<" whole_clone_json_max_ms="<<whole.back()
        <<" local_patch_p50_ms="<<local[6]<<" local_patch_max_ms="<<local.back()<<'\n';
}}
