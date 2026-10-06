#include "Desktop.h"
#include <sstream>
#include <cmath>
namespace ui {
class MidiClipPanel final : public juce::Component {
public:
    MidiClipPanel(Desktop& d,mrs::Id clip):owner(d),id(std::move(clip)){
        setLookAndFeel(&owner.theme);setSize(720,285);setName("MIDI clip notes");
        addAndMakeVisible(selector);selector.setTitle("Select MIDI note");selector.onChange=[this]{loadNote();};
        const std::array<juce::String,5> names{"Pitch (0..127)","Start (beats)","Length (beats)","Velocity (1..127)","Channel (1..16)"};
        for(std::size_t i=0;i<fields.size();++i){labels[i].setText(names[i],juce::dontSendNotification);labels[i].setFont(owner.theme.font(13));addAndMakeVisible(labels[i]);fields[i].setFont(owner.theme.font(13));fields[i].setTitle(names[i]);fields[i].setComponentID("midi-note-"+juce::String(static_cast<int>(i)));addAndMakeVisible(fields[i]);}
        for(auto* b:{&add,&apply,&remove})addAndMakeVisible(*b);
        add.setComponentID("midi-note-add");apply.setComponentID("midi-note-apply");remove.setComponentID("midi-note-delete");
        add.onClick=[this]{edit(0);};apply.onClick=[this]{edit(1);};remove.onClick=[this]{edit(2);};
        hint.setFont(owner.theme.font(13));hint.setJustificationType(juce::Justification::topLeft);addAndMakeVisible(hint);reload();
    }
    ~MidiClipPanel() override{setLookAndFeel(nullptr);}
    void resized() override{selector.setBounds(16,16,getWidth()-32,30);const int width=(getWidth()-32)/5;for(std::size_t i=0;i<fields.size();++i){const int x=16+static_cast<int>(i)*width;labels[i].setBounds(x,60,width-8,28);fields[i].setBounds(x,90,width-8,30);}add.setBounds(16,140,140,30);apply.setBounds(168,140,140,30);remove.setBounds(320,140,140,30);hint.setBounds(16,185,getWidth()-32,84);}
    void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(background));}
private:
    Desktop& owner;mrs::Id id;
    juce::ComboBox selector;
    std::array<juce::Label,5> labels;std::array<juce::TextEditor,5> fields;
    juce::TextButton add{"Add note"},apply{"Apply note"},remove{"Delete note"};juce::Label hint;
    std::vector<mrs::MidiNote> notes;
    const mrs::Clip& current(const mrs::Project& p) const{auto i=std::find_if(p.clips.begin(),p.clips.end(),[&](const auto& c){return c.id==id;});if(i==p.clips.end()||!i->midi)throw std::runtime_error("MIDI clip no longer exists");return *i;}
    double number(std::size_t i) const{std::istringstream in(fields[i].getText().toStdString());in.imbue(std::locale::classic());double value{};if(!(in>>value)||!std::isfinite(value))throw std::runtime_error("Enter a valid number in every field");in>>std::ws;if(!in.eof())throw std::runtime_error("Unexpected characters in note field");return value;}
    void loadNote(){const int index=selector.getSelectedId()-1;mrs::MidiNote n;if(index>=0&&index<static_cast<int>(notes.size()))n=notes[static_cast<std::size_t>(index)];else{const auto p=owner.project();n.start=current(*p).midi->source_offset;}
        fields[0].setText(juce::String(n.pitch));fields[1].setText(juce::String(static_cast<double>(n.start)/mrs::ppq,6));fields[2].setText(juce::String(static_cast<double>(n.length)/mrs::ppq,6));fields[3].setText(juce::String(n.velocity));fields[4].setText(juce::String(n.channel+1));apply.setEnabled(index>=0);remove.setEnabled(index>=0);}
    void reload(int selected=1){const auto p=owner.project();const auto& c=current(*p);notes=c.midi->notes;selector.clear(juce::dontSendNotification);for(std::size_t i=0;i<notes.size();++i){const auto& n=notes[i];selector.addItem("Note "+juce::String(static_cast<int>(i)+1)+" | pitch "+juce::String(n.pitch)+" | beat "+juce::String(static_cast<double>(n.start)/mrs::ppq,3),static_cast<int>(i)+1);}selector.setTextWhenNothingSelected("Empty MIDI clip — add a note");selector.setSelectedId(juce::jlimit(0,static_cast<int>(notes.size()),selected),juce::dontSendNotification);loadNote();
        hint.setText("Start and length use quarter-note beats. Start is relative to the original clip source;\ntrimming preserves notes. Pause or stop playback before editing. Changes can be undone.",juce::dontSendNotification);}
    void edit(int operation){try{const auto p=owner.project();auto next=current(*p).midi->notes;const int index=selector.getSelectedId()-1;
        if(operation!=0&&(index<0||index>=static_cast<int>(notes.size())))throw std::runtime_error("Select a note");
        auto at=next.end();if(operation!=0){at=std::find_if(next.begin(),next.end(),[&](const auto& n){return n.id==notes[static_cast<std::size_t>(index)].id;});if(at==next.end())throw std::runtime_error("Note changed; reopen this editor");}
        int selected=index+1;
        if(operation==2){next.erase(at);}else{mrs::MidiNote n;n.id=operation==0?mrs::new_id():at->id;const auto pitch=number(0),start=number(1),length=number(2),velocity=number(3),channel=number(4);
            if(pitch<0||pitch>127||std::floor(pitch)!=pitch||velocity<1||velocity>127||std::floor(velocity)!=velocity||channel<1||channel>16||std::floor(channel)!=channel||start<0||start>static_cast<double>(mrs::max_tick)/mrs::ppq||length<=0||length>static_cast<double>(mrs::max_tick)/mrs::ppq)throw std::runtime_error("Note values are outside their allowed ranges");
            n.pitch=static_cast<int>(pitch);n.velocity=static_cast<int>(velocity);n.channel=static_cast<int>(channel)-1;n.start=static_cast<mrs::Tick>(std::llround(start*mrs::ppq));n.length=static_cast<mrs::Tick>(std::llround(length*mrs::ppq));if(operation==0){next.push_back(n);selected=static_cast<int>(next.size());}else *at=n;
        }
        owner.app.set_midi_notes(id,std::move(next));owner.refresh();reload(selected);
    }catch(const std::exception& e){hint.setText(juce::String::fromUTF8(e.what()),juce::dontSendNotification);}}
};
void Desktop::openMidiClip(mrs::Id id){for(auto& w:windows)if(w->isVisible()&&w->getProperties()["mrs-midi-id"].toString()==juce::String::fromUTF8(id.value.c_str())){w->toFront(true);return;}auto window=std::make_unique<EditorWindow>("MIDI clip notes",new MidiClipPanel(*this,id));window->getProperties().set("mrs-midi-clip",true);window->getProperties().set("mrs-midi-id",juce::String::fromUTF8(id.value.c_str()));window->setOwner(*this);windows.push_back(std::move(window));}
}
