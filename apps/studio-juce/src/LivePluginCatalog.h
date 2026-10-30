#pragma once
#include "Desktop.h"

namespace ui::live {
// A read-only sorted view of Desktop::catalog. Never probe modules, scan disks
// or re-index the underlying catalog on menu-open; class IDs remain authoritative.
inline juce::String pluginVendor(const mrs::processing::VstPlugin& plugin) {
    return plugin.vendor.empty()?juce::String::fromUTF8("Без вендора"):juce::String::fromUTF8(plugin.vendor.c_str());
}
inline std::vector<std::size_t> sortedPluginClasses(
    const std::vector<mrs::processing::VstPlugin>& catalog,bool instrument) {
    std::vector<std::size_t> order;
    for(std::size_t i=0;i<catalog.size();++i)
        if(catalog[i].instrument.has_value()&&*catalog[i].instrument==instrument)order.push_back(i);
    std::stable_sort(order.begin(),order.end(),[&](std::size_t a,std::size_t b) {
        const auto& x=catalog[a];const auto& y=catalog[b];
        if(const auto c=pluginVendor(x).compareIgnoreCase(pluginVendor(y));c!=0)return c<0;
        const auto nx=juce::String::fromUTF8(x.name.c_str()),ny=juce::String::fromUTF8(y.name.c_str());
        if(const auto c=nx.compareIgnoreCase(ny);c!=0)return c<0;
        if(x.class_id!=y.class_id)return x.class_id<y.class_id;
        return x.path<y.path;
    });
    return order;
}
inline const mrs::processing::VstPlugin* findPluginClass(
    const std::vector<mrs::processing::VstPlugin>& catalog,
    const std::string& path,const std::string& classId) {
    const auto match=std::find_if(catalog.begin(),catalog.end(),[&](const auto& p) {
        return p.path==path && p.class_id==classId;
    });
    return match==catalog.end()?nullptr:&*match;
}
} // namespace ui::live
