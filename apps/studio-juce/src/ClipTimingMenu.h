#pragma once
#include "MRControls.h"
#include <mrs/arrangement.hpp>
namespace ui {
// Buffered menu controls: a wheel burst is one command, not one render per tick.
class ClipTimingMenu final : public juce::PopupMenu::CustomComponent, private juce::Timer {
    friend void clipStretchSmoke();
    struct Field final : juce::MouseListener {
        ui::kit::MRValueField editor;
        double wheelStep{};
        Field(ui::kit::ParameterSpec spec,double step):editor(std::move(spec)),wheelStep(step){editor.addMouseListener(this,false);}
        ~Field()override{editor.removeMouseListener(this);}
        void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& d)override{
            if(!editor.isEnabled()||d.deltaY==0||d.isInertial)return;
            const auto direction=d.deltaY>0?1.:-1.;
            const auto value=std::clamp(editor.value()+direction*wheelStep*(e.mods.isCtrlDown()?.1:1.),editor.spec().minimum,editor.spec().maximum);
            if(value==editor.value())return;
            editor.setAuthoritativeValue(value);if(editor.onCommit)editor.onCommit(value);
        }
    };
public:
    using Apply=std::function<std::optional<mrs::Clip>(const mrs::Clip&,mrs::stretch::Parameters,std::optional<double>,bool)>;
    struct Pending {
        mrs::Clip expected;mrs::stretch::Parameters parameters;std::optional<double> tempo;Apply submit;bool dirty{};
        void flush(){if(!dirty)return;dirty=false;if(const auto result=submit(expected,parameters,tempo,false))expected=*result;}
    };
    std::function<void()> finish()const{const auto state=pending;return [state]{state->flush();};}
    ClipTimingMenu(Theme& theme,mrs::Clip clip,Apply apply):CustomComponent(false),expected(std::move(clip)),submit(std::move(apply)),
        tempo({"file-tempo","File Tempo",ui::kit::Unit::none,0,999,0,.01,ui::kit::Mapping::linear,2,true},1),
        speed({"speedup","Speedup",ui::kit::Unit::none,.125,8,1,.001,ui::kit::Mapping::linear,3,true},.01),
        transpose({"transpose","Transpose",ui::kit::Unit::none,-24,24,0,.1,ui::kit::Mapping::bipolar,1,true},1),
        tune({"tune","Tune (cents)",ui::kit::Unit::none,-100,100,0,.1,ui::kit::Mapping::bipolar,1,true},1){
        pending=std::make_shared<Pending>();pending->expected=expected;pending->submit=submit;
        setLookAndFeel(&theme);setWantsKeyboardFocus(true);
        name.setText(juce::String::fromUTF8(expected.name.c_str()),juce::dontSendNotification);name.setFont(Theme::channelFont());addAndMakeVisible(name);
        std::array<Field*,4> fields{&tempo,&speed,&transpose,&tune};
        const std::array<juce::String,4> labels{"File Tempo","Speedup","Transpose","Tune (cents)"};
        for(std::size_t i=0;i<fields.size();++i){addAndMakeVisible(fields[i]->editor);captions[i].setText(labels[i],juce::dontSendNotification);captions[i].setFont(Theme::detailFont());addAndMakeVisible(captions[i]);fields[i]->editor.onCommit=[this,i](double){speedEdited=speedEdited||i==1;transposeEdited=transposeEdited||i==2;tuneEdited=tuneEdited||i==3;dirty=true;updatePending();startTimer(350);};}
        tempo.editor.setTooltip("Source BPM; 0 = unknown. Metadata only until Fit to project tempo.");
        speed.editor.setTooltip("Playback speed; 2 = twice as fast. Wheel, Ctrl+wheel for fine steps.");
        transpose.editor.setTooltip("Semitones. Wheel, Ctrl+wheel for fine steps.");tune.editor.setTooltip("Cents. Wheel, Ctrl+wheel for fine steps.");
        fit.setButtonText("Fit to project tempo");fit.onClick=[this]{flush(true);};addAndMakeVisible(fit);
        status.setFont(Theme::detailFont());status.setText("Wheel to change; Ctrl = fine. 0 BPM = unknown.",juce::dontSendNotification);addAndMakeVisible(status);refresh();
    }
    ~ClipTimingMenu()override{stopTimer();setLookAndFeel(nullptr);}
    void getIdealSize(int& w,int& h)override{w=370;h=142;}
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&)override{} // Numeric wheels must not scroll an overflowing popup.
    void resized()override{
        name.setBounds(10,4,getWidth()-20,22);
        std::array<Field*,4> fields{&tempo,&speed,&transpose,&tune};
        for(int i=0;i<4;++i){const int x=10+(i%2)*180,y=30+(i/2)*28;captions[static_cast<std::size_t>(i)].setBounds(x,y,102,24);fields[static_cast<std::size_t>(i)]->editor.setBounds(x+103,y,64,24);}
        fit.setBounds(10,90,170,24);status.setBounds(10,116,getWidth()-20,22);
    }
    void flush(bool fitTempo=false){
        stopTimer();if(!dirty&&!fitTempo)return;
        auto parameters=editedParameters();
        const double bpm=tempo.editor.value();
        if(fitTempo&&bpm<1){status.setText("Set File Tempo before fitting.",juce::dontSendNotification);return;}
        if(fitTempo)parameters.enabled=true;
        const auto result=submit(expected,parameters,bpm==0?std::nullopt:std::optional<double>{bpm},fitTempo);
        dirty=false;speedEdited=transposeEdited=tuneEdited=false;pending->dirty=false;if(result){expected=*result;refresh();status.setText("Applied. Undo restores the previous gesture.",juce::dontSendNotification);}else{refresh();status.setText("Could not apply; check the error and retry.",juce::dontSendNotification);}
    }
private:
    mrs::stretch::Parameters editedParameters()const{
        auto parameters=expected.stretch?expected.stretch->parameters:mrs::stretch::Parameters{};
        if(speedEdited)parameters.duration_ratio=1/speed.editor.value();
        if(transposeEdited)parameters.semitones=transpose.editor.value();
        if(tuneEdited)parameters.cents=tune.editor.value();
        if(speedEdited||transposeEdited||tuneEdited)parameters.enabled=true;
        return parameters;
    }
    void updatePending(){
        pending->expected=expected;pending->parameters=editedParameters();pending->tempo=tempo.editor.value()==0?std::nullopt:std::optional<double>{tempo.editor.value()};pending->dirty=true;
    }
    void refresh(){tempo.editor.setAuthoritativeValue(expected.file_tempo.value_or(0));speed.editor.setAuthoritativeValue(1/(expected.stretch?expected.stretch->parameters.duration_ratio:1.));transpose.editor.setAuthoritativeValue(expected.stretch?expected.stretch->parameters.semitones:0);tune.editor.setAuthoritativeValue(expected.stretch?expected.stretch->parameters.cents:0);}
    void timerCallback()override{flush();}
    mrs::Clip expected;Apply submit;bool dirty{},speedEdited{},transposeEdited{},tuneEdited{};std::shared_ptr<Pending> pending;
    Field tempo,speed,transpose,tune;juce::Label name,status;std::array<juce::Label,4> captions;juce::TextButton fit;
};
}
