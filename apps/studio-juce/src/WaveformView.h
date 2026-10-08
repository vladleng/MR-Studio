#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <mrs/waveform.hpp>
#include <cmath>

namespace ui {
// Message-thread, viewport-bounded anti-aliased envelope. Paths replace the old
// disconnected one-pixel columns; no PCM decoding or audio-thread changes.
inline void drawWaveform(juce::Graphics& g,const mrs::audio::Waveform& wave,
                         juce::Rectangle<float> area,double sourceOffset,double samplesPerPixel) {
    const auto visible=area.getIntersection(g.getClipBounds().toFloat());
    if(visible.isEmpty()||area.getHeight()<=0||samplesPerPixel<=0)return;
    g.saveState();g.reduceClipRegion(area.getSmallestIntegerContainer());
    const int first=static_cast<int>(std::floor(visible.getX())),last=static_cast<int>(std::ceil(visible.getRight()));
    for(std::uint32_t channel=0;channel<wave.channels();++channel){
        const float lane=area.getHeight()/wave.channels(),centre=area.getY()+lane*(channel+.5f),scale=lane*.45f;
        auto bounds=[&](int x){const double begin=sourceOffset+(x-area.getX())*samplesPerPixel;
            const auto peak=wave.display(begin,begin+samplesPerPixel,channel);
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
}
