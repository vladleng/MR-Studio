#pragma once
#include "DrumMapPresentation.h"
#include <algorithm>
#include <array>
#include <set>

namespace ui {
// Presentation only: MIDI events are still authored and stored by the Piano Roll.
class DrumGridRows {
public:
    struct Row {
        std::string name;
        mrs::Id group;
        int depth{};
        bool header{}, closed{};
        std::optional<std::uint32_t> color;
        int pitch{-1}, channel{-1}; // preferred creation address; -1 pitch means no writable assignment
    };
    using Address = std::pair<int,int>;
    std::vector<Row> rows;
    int index(int pitch, int channel) const { return lookup_[key(pitch,channel)]; }
    int summaryIndex(int pitch,int channel) const{return summaries_[key(pitch,channel)];}
    std::optional<std::uint32_t> color(int pitch,int channel) const{return colors_[key(pitch,channel)];}
    void rebuild(const mrs::DrumMapBinding* binding, const std::vector<mrs::Clip>& clips) {
        rows.clear(); lookup_.fill(-1); summaries_.fill(-1); colors_.fill({});
        std::array<const mrs::DrumMapArticulation*,128*16> owners{};
        std::array<bool,128*16> used{};
        for(const auto& clip:clips) for(const auto& n:clip.midi->notes) used[key(n.pitch,n.channel)]=true;
        std::set<std::string> closed;
        if(binding) {
            for(const auto& id:binding->collapsed)closed.insert(id.value);
            // Specific channel takes precedence; authored order resolves duplicate assignments.
            for(bool explicitChannel:{true,false})
                for(const auto& c:binding->snapshot.categories)for(const auto& i:c.instruments)for(const auto& a:i.articulations)
                    if(a.note&&a.channel.has_value()==explicitChannel)for(int ch=0;ch<16;++ch)if(!a.channel||*a.channel==ch){
                        const auto claim = [&](int pitch) { auto& entry = owners[key(pitch,ch)]; if (!entry) { entry = &a; colors_[key(pitch,ch)] = i.color ? i.color : c.color; } };
                        claim(*a.note);
                        for (int alt : a.alternate_notes) claim(alt);
                    }
            for(const auto& c:binding->snapshot.categories){
                const bool cClosed=closed.contains(c.id.value), compact=compactDrumCategory(c);
                const int categoryRow=static_cast<int>(rows.size());
                rows.push_back({c.name,c.id,0,true,cClosed,c.color});
                for(const auto& i:c.instruments){
                    const bool iClosed=!compact && closed.contains(i.id.value);
                    int summary=cClosed?categoryRow:-1;
                    if(!cClosed && !compact){
                        if(iClosed) summary=static_cast<int>(rows.size());
                        rows.push_back({i.name,i.id,1,true,iClosed,i.color ? i.color : c.color});
                    }
                    for(const auto& a:i.articulations)if(a.note){
                        const int row=static_cast<int>(rows.size());int channel=-1;
                        for(int ch=0;ch<16;++ch){
                            const auto add=[&](int pitch){const auto address=key(pitch,ch);if(owners[address]!=&a)return;
                                if(summary>=0)summaries_[address]=summary;
                                else {lookup_[address]=row;if(pitch==*a.note&&channel<0)channel=ch;}
                            };
                            add(*a.note);for(int alt:a.alternate_notes)add(alt);
                        }
                        if(summary<0)rows.push_back({a.name,{},compact?1:2,false,false,i.color ? i.color : c.color,channel>=0?*a.note:-1,channel});
                    }
                }
            }
        }
        bool unmappedHeader=false;
        for(int pitch=0;pitch<128;++pitch){
            int channel=-1;
            for(int ch=0;ch<16;++ch)if(!owners[key(pitch,ch)]&&used[key(pitch,ch)]){channel=ch;break;}
            if(channel<0)continue;
            if(!unmappedHeader){rows.push_back({"Unmapped",{},0,true,false});unmappedHeader=true;}
            const int row=static_cast<int>(rows.size());
            for(int ch=0;ch<16;++ch)if(!owners[key(pitch,ch)])lookup_[key(pitch,ch)]=row;
            rows.push_back({"MIDI " + std::to_string(pitch),{},1,false,false,{},pitch,channel});
        }
    }
private:
    static std::size_t key(int pitch,int channel){return static_cast<std::size_t>(pitch*16+channel);}
    std::array<int,128*16> lookup_{}, summaries_{};
    std::array<std::optional<std::uint32_t>,128*16> colors_{};
};
} // namespace ui
