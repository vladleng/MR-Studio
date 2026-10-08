#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <mrs/waveform.hpp>
#include <mrs/recording.hpp>
#include <cmath>

namespace ui {
// Message-thread, viewport-bounded anti-aliased envelope. Paths replace the old
// disconnected one-pixel columns; no PCM decoding or audio-thread changes.
template<class Query>
inline void drawPeakEnvelope(juce::Graphics& g,std::uint32_t channels,Query query,
                            juce::Rectangle<float> area,double sourceOffset,double samplesPerPixel) {
    const auto visible=area.getIntersection(g.getClipBounds().toFloat());
    if(visible.isEmpty()||area.getHeight()<=0||samplesPerPixel<=0)return;
    g.saveState();g.reduceClipRegion(area.getSmallestIntegerContainer());
    const int first=static_cast<int>(std::floor(visible.getX())),last=static_cast<int>(std::ceil(visible.getRight()));
    for(std::uint32_t channel=0;channel<channels;++channel){
        const float lane=area.getHeight()/channels,centre=area.getY()+lane*(channel+.5f),scale=lane*.45f;
        auto bounds=[&](int x){const double begin=sourceOffset+(x-area.getX())*samplesPerPixel;
            const auto peak=query(begin,begin+samplesPerPixel,channel);
            const float top=centre-std::clamp(peak.maximum,-1.f,1.f)*scale,bottom=centre-std::clamp(peak.minimum,-1.f,1.f)*scale;
            const float middle=(top+bottom)*.5f;
            return std::pair<float,float>{std::min(top,middle-.5f),std::max(bottom,middle+.5f)};};
        juce::Path envelope;envelope.startNewSubPath(static_cast<float>(first),bounds(first).first);
        for(int x=first+1;x<=last;++x)envelope.lineTo(static_cast<float>(x),bounds(x).first);
        for(int x=last;x>=first;--x)envelope.lineTo(static_cast<float>(x),bounds(x).second);
        envelope.closeSubPath();g.fillPath(envelope);
    }
    g.restoreState();
}
inline void drawWaveform(juce::Graphics& g,const mrs::audio::Waveform& wave,
                         juce::Rectangle<float> area,double offset,double samplesPerPixel) {
    drawPeakEnvelope(g,wave.channels(),[&](double a,double b,std::uint32_t c){return wave.display(a,b,c);},area,offset,samplesPerPixel);
}
inline void drawRecordingWaveform(juce::Graphics& g,const mrs::audio::RecordPreview& wave,
                                  juce::Rectangle<float> area,double samplesPerPixel) {
    if(wave.peaks.empty()||!wave.channels||wave.bin_frames<=0)return;
    auto query=[&](double a,double b,std::uint32_t channel){
        if(b<=0||a>=wave.frames)return mrs::audio::Peak{};
        const auto count=wave.peaks.size()/wave.channels;
        const auto first=std::min(count-1,static_cast<std::size_t>(std::max(0.,a)/wave.bin_frames));
        const auto last=std::min(count,static_cast<std::size_t>(std::ceil(std::min(b,static_cast<double>(wave.frames))/wave.bin_frames)));
        auto peak=wave.peaks[first*wave.channels+channel];
        for(auto bin=first+1;bin<last;++bin){const auto other=wave.peaks[bin*wave.channels+channel];peak.minimum=std::min(peak.minimum,other.minimum);peak.maximum=std::max(peak.maximum,other.maximum);}
        return peak;};
    drawPeakEnvelope(g,wave.channels,query,area,0,samplesPerPixel);
}
}
