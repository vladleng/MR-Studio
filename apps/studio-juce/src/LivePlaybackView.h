#pragma once
#include "Desktop.h"
#include "LiveWaveformCache.h"
#include "PlaybackWaveform.h"

namespace ui::live {
inline PlaybackWaveformSource playbackWaveform(Desktop &owner, const LiveWaveformCache &cache,
                                               const juce::var &state, const juce::String &item, std::size_t index) {
    const auto rows = playbackRows(state, item);
    if (index >= rows.size()) return {};
    const auto &row = rows[index];
    PlaybackWaveformSource result;
    if (owner.livePlaybackReady() && owner.livePlaybackItem == item && index < owner.app.playback_sources().size()) {
        const auto project=owner.project();const auto& clip=owner.app.playback_sources()[index];
        if(const auto* wave=owner.app.clip_waveform(clip))result={wave,static_cast<double>(project->sample_rate),
            static_cast<double>(clip.start)/project->sample_rate,static_cast<double>(clip.source_offset)/project->sample_rate,
            static_cast<double>(clip.length)/project->sample_rate};
    }
    if(!result.wave){
        const auto* cached=cache.find(row["file"].toString());
        if(!cached || !cached->wave || !cached->rate)return {};
        const double start=static_cast<double>(row["trimStart"]);
        const double end=row["trimEnd"].isVoid()?static_cast<double>(cached->wave->frames())/cached->rate:static_cast<double>(row["trimEnd"]);
        result={cached->wave.get(),static_cast<double>(cached->rate),start,start,(std::max)(0.,end-start)};
    }
    const auto regions=playbackRegions(state,item,row);
    if(regions.isArray()){
        const double seconds=playbackSecondsPerBar(state,item);result.hasRegions=true;result.start=0.;result.length=0.;
        for(const auto& region:*regions.getArray()){
            const double start=static_cast<double>(region["start"])*seconds,offset=static_cast<double>(region["offset"])*seconds,
                length=static_cast<double>(region["length"])*seconds;
            result.regions.push_back({start,offset,length});result.length=(std::max)(result.length,start+length);
        }
    }
    return result;
}
} // namespace ui::live
