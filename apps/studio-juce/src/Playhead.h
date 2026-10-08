#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <mrs/audio.hpp>
namespace ui {
// Message-thread display only. Interpolate towards an observed audio position,
// never beyond it; a stalled device cannot make the cursor run away.
class VisualTransport {
public:
    void reset(){valid=false;}
    double sample(const mrs::audio::RealtimeState& state,double rate,double now){
        const double raw=static_cast<double>(state.sample);
        const auto elapsed=now-observedAt;
        const bool jump=valid&&(raw<target||raw-target>rate*std::max(.1,elapsed*.002+ .05));
        if(!valid||state.playback!=mrs::PlaybackState::playing||playback!=state.playback||state.loop!=loop||jump||now<observedAt||elapsed>250.){
            from=target=raw;observedAt=now;duration=0;valid=true;
        }else if(raw!=target){
            from=value(now);duration=std::clamp(elapsed,1.,50.);target=raw;observedAt=now;
        }
        playback=state.playback;loop=state.loop;return value(now);
    }
private:
    double value(double now) const{return duration>0?from+(target-from)*std::clamp((now-observedAt)/duration,0.,1.):target;}
    bool valid{};double from{},target{},observedAt{},duration{};
    mrs::PlaybackState playback{mrs::PlaybackState::stopped};std::optional<mrs::LoopRange> loop;
};
// Separate transparent child: moving it invalidates only the old/new narrow
// cursor strips, not an entire piano roll, its model or controller event list.
class PlayheadLine final : public juce::Component,private juce::Timer {
public:
    using Position=std::function<juce::Rectangle<float>()>;
    explicit PlayheadLine(Position fn):position(std::move(fn)){setInterceptsMouseClicks(false,false);setAccessible(false);startTimerHz(60);}
    void update(){if(!getParentComponent())return;const auto r=position();const auto bounds=r.expanded(1.f,0.f).getSmallestIntegerContainer();const float next=r.getX()-bounds.getX();const bool moved=getBounds()!=bounds;offset=next;if(moved)setBounds(bounds);else repaint();toFront(false);}
    void paint(juce::Graphics& g) override{g.setColour(juce::Colours::white);g.fillRect(offset,0.f,1.f,static_cast<float>(getHeight()));}
private:
    void timerCallback() override{if(isShowing())update();}
    Position position;float offset{};
};
}
