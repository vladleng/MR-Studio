#pragma once
#include "Controls.h"
#include <mrs/desktop.hpp>
#include <mrs/offline_device.hpp>
namespace ui {
class Surface final : public juce::Component, private juce::Timer {
public:
    Surface() {
        setLookAndFeel(&theme); setWantsKeyboardFocus(true);
        app.demo(); app.connect(mrs::audio::make_offline_device(),{0,48000,128,{}, {0,1}});
        app.workspace(mrs::desktop::Workspace::mix);
        for(auto* b : {&play,&stop,&mute,&solo,&undo,&redo,&brows}) addAndMakeVisible(*b);
        addAndMakeVisible(gain); addAndMakeVisible(pan);
        gain.setTitle("Channel gain"); pan.setTitle("Channel pan");
        gain.setDescription("Drag handle; Ctrl for fine adjustment; arrows change gain; Escape cancels");
        pan.setDescription("Drag handle; Ctrl for fine adjustment; arrows change pan; Escape cancels");
        play.onClick=[this]{app.play();}; stop.onClick=[this]{app.stop();};
        mute.onClick=[this]{ auto m=mix(); m.mute=!m.mute; app.set_track_mix(id(),m); sync();};
        solo.onClick=[this]{ auto m=mix(); m.solo=!m.solo; app.set_track_mix(id(),m); sync();};
        undo.onClick=[this]{app.undo();sync();}; redo.onClick=[this]{app.redo();sync();};
        brows.onClick=[this]{sidebar=!sidebar;repaint();};
        gain.preview=[this](double v){preview(true,v);}; pan.preview=[this](double v){preview(false,v);};
        gain.commit=[this](double v){change(true,v);}; pan.commit=[this](double v){change(false,v);};
        gain.cancel=pan.cancel=[this]{pending.reset();app.cancel_mix_preview();sync();};
        sync(); setSize(900,600); startTimerHz(30);
    }
    ~Surface() override {stopTimer(); setLookAndFeel(nullptr);}
    void resized() override {
        play.setBounds(20,getHeight()-48,80,30); stop.setBounds(108,getHeight()-48,80,30);
        undo.setBounds(260,20,70,30);redo.setBounds(338,20,70,30);
        gain.setBounds(128,200,62,juce::jmax(100,getHeight()-320));
        pan.setBounds(35,132,155,48); mute.setBounds(34,getHeight()-102,70,28);
        solo.setBounds(112,getHeight()-102,70,28); brows.setBounds(getWidth()-98,getHeight()-48,78,30);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(background)); g.setFont(theme.font(18));g.setColour(juce::Colours::whitesmoke);
        g.drawText("Moon River Studio 0.1n / JUCE J1",20,65,getWidth()-40,30,juce::Justification::left);
        g.setColour(juce::Colour(panel)); g.fillRect(20,110,184,getHeight()-175);
        g.setColour(juce::Colour(accent));g.fillRect(20,110,4,getHeight()-175);
        g.setColour(juce::Colours::whitesmoke);g.setFont(theme.font(14));
        g.drawText("Audio 1",34,111,145,22,juce::Justification::left);
        g.drawText(juce::String(-60+gain.value*72,1)+" dB",32,175,145,24,juce::Justification::left);
        const auto h=juce::jmax(100,getHeight()-320);
        for(int i=0;i<2;++i) { const int x=38+i*28;
            g.setColour(juce::Colour(0xff121619));g.fillRect(x,200,18,h);
            const float p=i==0 ? peak.left : peak.right;
            const auto level=juce::jlimit(0.f,1.f,(20.f*std::log10(juce::jmax(.000001f,p))+60.f)/60.f);
            g.setColour(p>.95f ? juce::Colours::orange : juce::Colour(0xff45d899));
            const int height=static_cast<int>(level*static_cast<float>(h));g.fillRect(x,200+h-height,18,height);
            g.setColour(juce::Colours::lightgrey);g.drawText(i==0 ? "L" : "R",x,202+h,18,22,juce::Justification::centred);
        }
        g.setColour(juce::Colours::lightgrey);
        g.drawText("J1: shared engine / one channel / custom components",228,120,getWidth()-250,25,juce::Justification::left);
        g.drawText("Offline clock: signal is rendered, hardware output is disabled",228,154,getWidth()-250,25,juce::Justification::left);
        g.drawText("Space: Play / Stop    Ctrl+Z: Undo    Ctrl+Y: Redo",228,188,getWidth()-250,25,juce::Justification::left);
        g.drawText("Arrangement, browser and plugin editors follow in J2",228,222,getWidth()-250,25,juce::Justification::left);
        g.drawText(juce::String(static_cast<double>(app.engine()->state().sample)/48000.,2)+" s",204,getHeight()-48,120,30,juce::Justification::left);
        if(sidebar) {g.setColour(juce::Colour(panel));g.fillRect(getWidth()-240,285,220,140);
            g.setColour(juce::Colours::whitesmoke);g.drawText("Project: Foundation demo",getWidth()-230,300,200,25,juce::Justification::left);
            g.drawText("VST3 browser reserved for J2",getWidth()-230,335,200,25,juce::Justification::left);}
    }
    bool keyPressed(const juce::KeyPress& k) override {
        if(k.getModifiers().isCtrlDown() && (k.getTextCharacter()=='z' || k.getTextCharacter()=='y')) {
            if(gain.dragging() || pan.dragging()) return true;
            if(k.getTextCharacter()=='z')app.undo();else app.redo();sync();return true;
        }
        return k==juce::KeyPress::spaceKey; // handled on transition to avoid key repeat
    }
    bool keyStateChanged(bool) override {
        const bool down=juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey);
        if(down && !spaceHeld) {if(app.engine()->state().playback==mrs::PlaybackState::playing)app.stop();else app.play();}
        spaceHeld=down;return down;
    }
    void focusLost(FocusChangeType) override {spaceHeld=false;}
    void sync() {
        const auto m=mix();if(!gain.dragging())gain.value=juce::jlimit(0.,1.,(20*std::log10(juce::jmax(.001f,m.gain))+60)/72.);
        if(!pan.dragging())pan.value=(m.pan+1)/2.;
        mute.setToggleState(m.mute,juce::dontSendNotification);solo.setToggleState(m.solo,juce::dontSendNotification);
        undo.setEnabled(app.services().projects->state().can_undo);redo.setEnabled(app.services().projects->state().can_redo);
        gain.repaint();pan.repaint();repaint();
    }
    mrs::desktop::Application app;
    Handle gain{true},pan{false};
private:
    mrs::Id id() const {return app.services().projects->state().project->tracks.front().id;}
    mrs::Track::Mix mix() const {return app.services().projects->state().project->tracks.front().mix;}
    mrs::Track::Mix adjusted(bool volume,double v) const {auto m=mix();if(volume)m.gain=static_cast<float>(std::pow(10.,(-60+v*72)/20));else m.pan=static_cast<float>(v*2-1);return m;}
    void preview(bool volume,double v) {pending=adjusted(volume,v);app.preview_mix(id(),*pending,app.services().projects->state().project->master_gain);}
    void change(bool volume,double v) {pending.reset();app.set_track_mix(id(),adjusted(volume,v));sync();}
    void timerCallback() override {app.poll();if(pending && (gain.dragging()||pan.dragging()))app.preview_mix(id(),*pending,app.services().projects->state().project->master_gain);
        else pending.reset();const auto meters=app.engine()->take_meters();peak.left=juce::jmax(meters.tracks[0].left,peak.left*.86f);peak.right=juce::jmax(meters.tracks[0].right,peak.right*.86f);repaint();}
    Theme theme;
    juce::TextButton play{"Play"},stop{"Stop"},mute{"M"},solo{"S"},undo{"Undo"},redo{"Redo"},brows{"BROWS"};
    mrs::audio::StereoPeak peak;
    std::optional<mrs::Track::Mix> pending;
    bool sidebar{},spaceHeld{};
};

class ManualDevice final : public mrs::audio::IAudioDevice {
public:
    std::vector<mrs::audio::DeviceInfo> enumerate() override {return {{0,"J1 test",{}, {"L","R"},32,2048,128,-1}};}
    void control_panel(int) override {}
    void open(const mrs::audio::DeviceConfig&,std::shared_ptr<mrs::audio::AudioEngine>) override {phase=mrs::audio::DevicePhase::open;}
    void start() override {phase=mrs::audio::DevicePhase::running;}
    void stop() override {phase=mrs::audio::DevicePhase::stopped;}
    void close() noexcept override {phase=mrs::audio::DevicePhase::closed;}
    mrs::audio::DeviceStatus status() override {return {phase,48000,0,0,0,{}};}
private:
    mrs::audio::DevicePhase phase{mrs::audio::DevicePhase::closed};
};
inline void smoke(Surface& s) {
    auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    s.app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});
    auto event=[&](juce::Point<float> point,int mods,bool dragged){return juce::MouseEvent(
        juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),
        1,0,0,0,0,&s.gain,&s.gain,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,dragged);};
    const auto initial=s.app.services().projects->state();const auto v=s.gain.value;
    s.gain.mouseDown(event({2,2},juce::ModifierKeys::leftButtonModifier,false));
    s.gain.mouseDrag(event({2,60},juce::ModifierKeys::leftButtonModifier,true));
    s.gain.mouseUp(event({2,60},0,true));
    require(s.gain.value==v && s.app.services().projects->state().revision==initial.revision,"rail rejection");
    auto point=s.gain.knob().getCentre();
    s.gain.mouseDown(event(point,juce::ModifierKeys::leftButtonModifier,false));
    s.gain.mouseDrag(event(point.translated(0,-12),juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::ctrlModifier,true));
    require(s.gain.value>v && s.gain.value-v<.02,"fine relative drag");
    require(s.app.services().projects->state().revision==initial.revision,"preview does not commit");
    s.gain.mouseUp(event(point.translated(0,-12),0,true));
    require(s.app.services().projects->state().revision==initial.revision+1,"one undo command");
    s.app.undo();s.sync();require(s.app.services().projects->state().project->tracks.front().mix==initial.project->tracks.front().mix,"gesture undo");
    point=s.gain.knob().getCentre();s.gain.mouseDown(event(point,juce::ModifierKeys::leftButtonModifier,false));
    s.gain.mouseDrag(event(point.translated(0,20),juce::ModifierKeys::leftButtonModifier,true));
    require(s.gain.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),"escape handled");
    require(s.gain.value==v,"escape restores handle");
    point=s.gain.knob().getCentre();s.gain.mouseDown(event(point,juce::ModifierKeys::leftButtonModifier,false));
    s.gain.mouseDrag(event(point.translated(0,15),juce::ModifierKeys::leftButtonModifier,true));
    s.gain.focusLost(juce::Component::focusChangedDirectly);require(s.gain.value==v,"focus cancels");
    require(s.pan.keyPressed(juce::KeyPress(juce::KeyPress::rightKey)),"pan keyboard");
    require(s.app.services().projects->state().project->tracks.front().mix.pan>0,"pan binding");
    s.app.undo();s.sync();
    auto click=[&](const juce::String& label){for(auto* child:s.getChildren())
        if(auto* button=dynamic_cast<juce::TextButton*>(child);button && button->getButtonText()==label){button->onClick();return;}
        throw std::runtime_error("missing button");};
    click("S");require(s.app.services().projects->state().project->tracks.front().mix.solo,"solo binding");
    click("Undo");
    std::array<float,256> output{};auto render=[&]{s.app.engine()->process(nullptr,output.data(),128);s.app.poll();};
    s.app.seek(4800);render();s.app.play();render();
    require(s.app.engine()->state().playback==mrs::PlaybackState::playing,"play");
    auto peak=s.app.engine()->take_meters().tracks[0];require(peak.left>0 && peak.right>0,"stereo meters");
    s.app.stop();render();require(s.app.engine()->state().sample==4800,"stop return");
    click("M");s.app.play();for(int n=0;n<32;++n)render();
    require(std::all_of(output.begin(),output.end(),[](float f){return f==0;}),"mute audio");
    s.app.stop();render();s.app.undo();s.sync();
    s.setSize(720,500);require(s.getLocalBounds().contains(s.gain.getBounds()),"minimum layout");
    s.setSize(1350,900);require(s.getLocalBounds().contains(s.gain.getBounds()),"large layout");
    s.setSize(900,600);
}
}
