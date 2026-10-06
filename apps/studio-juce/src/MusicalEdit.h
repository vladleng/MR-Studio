#pragma once
#include "Controls.h"
#include <mrs/midi_clips.hpp>
#include <sstream>
namespace ui {
class MusicalEditPanel final : public juce::Component {
public:
    MusicalEditPanel(Theme& theme,std::function<void(bool,const mrs::NoteEdit&)> commit):applyEdit(std::move(commit)){
        setLookAndFeel(&theme);setName("Musical note edit");setSize(540,370);
        operation.setComponentID("musical-operation");scope.setComponentID("musical-scope");grid.setComponentID("musical-grid");policy.setComponentID("musical-policy");mode.setComponentID("musical-mode");strength.setComponentID("musical-strength");value.setComponentID("musical-value");apply.setComponentID("musical-apply");
        operation.addItem("Quantize",1);operation.addItem("Transpose",2);operation.addItem("Velocity",3);operation.addItem("Length",4);operation.setSelectedId(1);
        scope.addItem("Selected notes",1);scope.addItem("Whole clip (audible notes)",2);scope.setSelectedId(1);
        grid.addItem("1/4",1);grid.addItem("1/8",2);grid.addItem("1/16",3);grid.addItem("1/32",4);grid.addItem("1/8 triplet",5);grid.addItem("1/16 triplet",6);grid.setSelectedId(3);
        policy.addItem("Start only",1);policy.addItem("Length only",2);policy.addItem("Start and length",3);policy.setSelectedId(1);
        mode.addItem("Set",1);mode.addItem("Add",2);mode.addItem("Scale %",3);mode.setSelectedId(1);strength.setText("100");value.setText("0");
        const std::array<juce::String,7> names{"Operation","Scope","Grid","Quantize target","Strength %","Value","Value mode"};
        for(std::size_t i=0;i<labels.size();++i){labels[i].setText(names[i],juce::dontSendNotification);labels[i].setFont(theme.font(13));addAndMakeVisible(labels[i]);}
        for(auto* c:std::array<juce::Component*,9>{&operation,&scope,&grid,&policy,&strength,&value,&mode,&apply,&hint})addAndMakeVisible(c);
        operation.setTitle("Musical edit operation");scope.setTitle("Notes affected");grid.setTitle("Quantize grid");policy.setTitle("Quantize start / length");strength.setTitle("Quantize strength 0 to 100 percent");value.setTitle("Musical edit value");mode.setTitle("Set / add / scale");
        strength.setFont(theme.font(13));value.setFont(theme.font(13));hint.setFont(theme.font(12));hint.setJustificationType(juce::Justification::topLeft);
        operation.onChange=[this]{value.setText(operation.getSelectedId()==3?"100":operation.getSelectedId()==4?"1":"0");update();};mode.onChange=[this]{update();};
        apply.onClick=[this]{try{mrs::NoteEdit edit;edit.kind=static_cast<mrs::NoteEditKind>(operation.getSelectedId()-1);edit.mode=static_cast<mrs::NoteValueMode>(mode.getSelectedId()-1);const std::array<mrs::Tick,6> grids{mrs::ppq,mrs::ppq/2,mrs::ppq/4,mrs::ppq/8,mrs::ppq/3,mrs::ppq/6};edit.grid=grids[static_cast<std::size_t>(grid.getSelectedId()-1)];
            if(edit.kind==mrs::NoteEditKind::quantize){edit.strength=number(strength);edit.starts=policy.getSelectedId()!=2;edit.lengths=policy.getSelectedId()!=1;}
            else {edit.value=number(value);if(edit.kind==mrs::NoteEditKind::length&&edit.mode!=mrs::NoteValueMode::scale)edit.value=std::round(edit.value*mrs::ppq);}
            applyEdit(scope.getSelectedId()==2,edit);hint.setText("Applied. Undo/Redo in piano roll reverses the entire operation.",juce::dontSendNotification);
        }catch(const std::exception& e){hint.setText(juce::String::fromUTF8(e.what()),juce::dontSendNotification);}};update();
    }
    ~MusicalEditPanel() override{setLookAndFeel(nullptr);}
    void resized() override{std::array<juce::Component*,7> controls{&operation,&scope,&grid,&policy,&strength,&value,&mode};for(std::size_t i=0;i<controls.size();++i){const int y=12+static_cast<int>(i)*36;labels[i].setBounds(16,y,150,26);controls[i]->setBounds(176,y,getWidth()-192,26);}apply.setBounds(176,270,160,28);hint.setBounds(16,310,getWidth()-32,52);}
    void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(background));}
private:
    std::function<void(bool,const mrs::NoteEdit&)> applyEdit;
    juce::ComboBox operation,scope,grid,policy,mode;juce::TextEditor strength,value;std::array<juce::Label,7> labels;juce::TextButton apply{"Apply"};juce::Label hint;
    static double number(const juce::TextEditor& field){std::istringstream input(field.getText().toStdString());input.imbue(std::locale::classic());double v{};if(!(input>>v)||!std::isfinite(v))throw std::invalid_argument("Enter a finite number");input>>std::ws;if(!input.eof())throw std::invalid_argument("Unexpected characters in value");return v;}
    void update(){const bool quantize=operation.getSelectedId()==1;grid.setEnabled(quantize);policy.setEnabled(quantize);strength.setEnabled(quantize);mode.setEnabled(operation.getSelectedId()>2);value.setEnabled(!quantize);const bool percent=mode.getSelectedId()==3&&operation.getSelectedId()>2;labels[5].setText(percent?"Value %":operation.getSelectedId()==2?"Semitones":operation.getSelectedId()==4?"Length (beats)":"Velocity",juce::dontSendNotification);hint.setText("Pause/Stop before editing. Group transpose preserves intervals.\nValues and note edges are clamped; no zero-length notes.",juce::dontSendNotification);}
};
}
