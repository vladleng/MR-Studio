#include "PianoRoll.h"
#include "MusicalEdit.h"
#include <sstream>
#include <cmath>
namespace ui {
class MidiClipPanel final : public juce::Component {
public:
    MidiClipPanel(Desktop& d,mrs::Id clip):owner(d),id(std::move(clip)),roll(d,id){
        setLookAndFeel(&owner.theme);setName("Piano roll");
        addAndMakeVisible(viewport);viewport.setViewedComponent(&roll,false);viewport.setScrollBarsShown(true,true);
        addAndMakeVisible(snap);snap.addItem("Snap off",1);snap.addItem("1/4",2);snap.addItem("1/8",3);snap.addItem("1/16",4);snap.addItem("1/32",5);snap.setSelectedId(4);snap.setTitle("Piano roll snap");snap.onChange=[this]{const std::array<mrs::Tick,5> values{0,mrs::ppq,mrs::ppq/2,mrs::ppq/4,mrs::ppq/8};roll.grid=values[static_cast<std::size_t>(snap.getSelectedId()-1)];roll.repaint();};
        for(auto* b:{&zoomIn,&zoomOut,&copy,&paste,&selectAll,&musical})addAndMakeVisible(b);
        zoomIn.onClick=[this]{zoom(1.25f);};zoomOut.onClick=[this]{zoom(.8f);};copy.onClick=[this]{roll.copy();};paste.onClick=[this]{roll.paste();};selectAll.onClick=[this]{for(const auto& n:roll.notes)roll.selected.insert(n.id.value);roll.repaint();};
        musical.setComponentID("musical-edit-open");musical.onClick=[this]{roll.cancel();auto* musicalPanel=new MusicalEditPanel(owner.theme,[safe=juce::Component::SafePointer<MidiClipPanel>(this)](bool all,const mrs::NoteEdit& edit){if(!safe||!safe->isShowing())throw std::runtime_error("Piano roll is closed");const auto p=safe->owner.project();const auto& clip=*safe->current(*p).midi;std::vector<mrs::Id> selected;for(const auto& n:clip.notes)if(all?(n.start<clip.source_offset+clip.length&&n.start+n.length>clip.source_offset):safe->roll.selected.contains(n.id.value))selected.push_back(n.id);if(selected.empty())throw std::runtime_error("Select notes in piano roll or choose Whole clip");safe->owner.app.edit_midi_notes(safe->id,std::move(selected),edit);safe->owner.refresh();safe->roll.sync();safe->reload();});auto window=std::make_unique<EditorWindow>("Musical note edit",musicalPanel);window->getProperties().set("mrs-midi-clip",true);window->setOwner(owner);owner.windows.push_back(std::move(window));};
        roll.error=[this](auto text){hint.setText(text,juce::dontSendNotification);};roll.changed=[this]{reload();};
        addAndMakeVisible(selector);selector.setTitle("Select MIDI note");selector.onChange=[this]{loadNote();const int index=selector.getSelectedId()-1;if(index>=0&&index<static_cast<int>(notes.size())){roll.selected={notes[static_cast<std::size_t>(index)].id.value};roll.repaint();}};
        const std::array<juce::String,5> names{"Pitch (0..127)","Start (beats)","Length (beats)","Velocity (1..127)","Channel (1..16)"};
        for(std::size_t i=0;i<fields.size();++i){labels[i].setText(names[i],juce::dontSendNotification);labels[i].setFont(owner.theme.font(13));addAndMakeVisible(labels[i]);fields[i].setFont(owner.theme.font(13));fields[i].setTitle(names[i]);fields[i].setComponentID("midi-note-"+juce::String(static_cast<int>(i)));addAndMakeVisible(fields[i]);}
        for(auto* b:{&add,&apply,&remove})addAndMakeVisible(*b);
        add.setComponentID("midi-note-add");apply.setComponentID("midi-note-apply");remove.setComponentID("midi-note-delete");
        add.onClick=[this]{edit(0);};apply.onClick=[this]{edit(1);};remove.onClick=[this]{edit(2);};
        hint.setFont(owner.theme.font(13));hint.setJustificationType(juce::Justification::topLeft);addAndMakeVisible(hint);reload();setSize(1000,700);
    }
    ~MidiClipPanel() override{viewport.setViewedComponent(nullptr,false);setLookAndFeel(nullptr);}
    void resized() override{snap.setBounds(16,8,120,26);zoomIn.setBounds(144,8,72,26);zoomOut.setBounds(222,8,72,26);copy.setBounds(302,8,72,26);paste.setBounds(380,8,72,26);selectAll.setBounds(460,8,100,26);musical.setBounds(568,8,130,26);
        viewport.setBounds(16,42,getWidth()-32,getHeight()-244);selector.setBounds(16,getHeight()-194,getWidth()-32,26);const int width=(getWidth()-32)/5;for(std::size_t i=0;i<fields.size();++i){const int x=16+static_cast<int>(i)*width;labels[i].setBounds(x,getHeight()-164,width-8,22);fields[i].setBounds(x,getHeight()-140,width-8,26);}add.setBounds(16,getHeight()-108,140,26);apply.setBounds(168,getHeight()-108,140,26);remove.setBounds(320,getHeight()-108,140,26);hint.setBounds(16,getHeight()-76,getWidth()-32,70);if(firstLayout){firstLayout=false;viewport.setViewPosition(0,PianoRoll::ruler+(127-78)*static_cast<int>(roll.rowHeight));}}
    void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(background));}
private:
    Desktop& owner;mrs::Id id;
    PianoRoll roll;juce::Viewport viewport;juce::ComboBox snap;bool firstLayout{true};
    juce::TextButton zoomIn{"Zoom +"},zoomOut{"Zoom -"},copy{"Copy"},paste{"Paste"},selectAll{"Select all"},musical{"Musical edit..."};
    void zoom(float factor){const auto x=viewport.getViewPositionX();roll.cancel();roll.beatWidth=juce::jlimit(24.f,384.f,roll.beatWidth*factor);roll.sync();viewport.setViewPosition(static_cast<int>(x*factor),viewport.getViewPositionY());}
    juce::ComboBox selector;
    std::array<juce::Label,5> labels;std::array<juce::TextEditor,5> fields;
    juce::TextButton add{"Add note"},apply{"Apply note"},remove{"Delete note"};juce::Label hint;
    std::vector<mrs::MidiNote> notes;
    const mrs::Clip& current(const mrs::Project& p) const{auto i=std::find_if(p.clips.begin(),p.clips.end(),[&](const auto& c){return c.id==id;});if(i==p.clips.end()||!i->midi)throw std::runtime_error("MIDI clip no longer exists");return *i;}
    double number(std::size_t i) const{std::istringstream in(fields[i].getText().toStdString());in.imbue(std::locale::classic());double value{};if(!(in>>value)||!std::isfinite(value))throw std::runtime_error("Enter a valid number in every field");in>>std::ws;if(!in.eof())throw std::runtime_error("Unexpected characters in note field");return value;}
    void loadNote(){const int index=selector.getSelectedId()-1;mrs::MidiNote n;if(index>=0&&index<static_cast<int>(notes.size()))n=notes[static_cast<std::size_t>(index)];else{const auto p=owner.project();n.start=current(*p).midi->source_offset;}
        fields[0].setText(juce::String(n.pitch));fields[1].setText(juce::String(static_cast<double>(n.start)/mrs::ppq,6));fields[2].setText(juce::String(static_cast<double>(n.length)/mrs::ppq,6));fields[3].setText(juce::String(n.velocity));fields[4].setText(juce::String(n.channel+1));apply.setEnabled(index>=0);remove.setEnabled(index>=0);}
    void reload(int selected=1){const auto p=owner.project();const auto& c=current(*p);notes=c.midi->notes;selector.clear(juce::dontSendNotification);for(std::size_t i=0;i<notes.size();++i){const auto& n=notes[i];selector.addItem("Note "+juce::String(static_cast<int>(i)+1)+" | pitch "+juce::String(n.pitch)+" | beat "+juce::String(static_cast<double>(n.start)/mrs::ppq,3),static_cast<int>(i)+1);}selector.setTextWhenNothingSelected("Empty MIDI clip — add a note");selector.setSelectedId(juce::jlimit(0,static_cast<int>(notes.size()),selected),juce::dontSendNotification);loadNote();
        hint.setText("Start and length use quarter-note beats. Start is relative to the original clip source;\nDouble click: create. Ctrl/Shift click or drag empty space: select. Drag: move; right edge: length.\nAlt-drag: velocity. Ctrl+C/V/A/Z/Y: copy/paste/select/Undo/Redo. Right click/Delete: remove.",juce::dontSendNotification);}
    void edit(int operation){try{const auto p=owner.project();auto next=current(*p).midi->notes;const int index=selector.getSelectedId()-1;
        if(operation!=0&&(index<0||index>=static_cast<int>(notes.size())))throw std::runtime_error("Select a note");
        auto at=next.end();if(operation!=0){at=std::find_if(next.begin(),next.end(),[&](const auto& n){return n.id==notes[static_cast<std::size_t>(index)].id;});if(at==next.end())throw std::runtime_error("Note changed; reopen this editor");}
        int selected=index+1;
        if(operation==2){next.erase(at);}else{mrs::MidiNote n;n.id=operation==0?mrs::new_id():at->id;const auto pitch=number(0),start=number(1),length=number(2),velocity=number(3),channel=number(4);
            if(pitch<0||pitch>127||std::floor(pitch)!=pitch||velocity<1||velocity>127||std::floor(velocity)!=velocity||channel<1||channel>16||std::floor(channel)!=channel||start<0||start>static_cast<double>(mrs::max_tick)/mrs::ppq||length<=0||length>static_cast<double>(mrs::max_tick)/mrs::ppq)throw std::runtime_error("Note values are outside their allowed ranges");
            n.pitch=static_cast<int>(pitch);n.velocity=static_cast<int>(velocity);n.channel=static_cast<int>(channel)-1;n.start=static_cast<mrs::Tick>(std::llround(start*mrs::ppq));n.length=static_cast<mrs::Tick>(std::llround(length*mrs::ppq));if(operation==0){next.push_back(n);selected=static_cast<int>(next.size());}else *at=n;
        }
        owner.app.set_midi_notes(id,std::move(next));owner.refresh();roll.sync();reload(selected);
    }catch(const std::exception& e){hint.setText(juce::String::fromUTF8(e.what()),juce::dontSendNotification);}}
};
void Desktop::openMidiClip(mrs::Id id){for(auto& w:windows)if(w->isVisible()&&w->getProperties()["mrs-midi-id"].toString()==juce::String::fromUTF8(id.value.c_str())){w->toFront(true);return;}auto window=std::make_unique<EditorWindow>("Piano roll",new MidiClipPanel(*this,id));window->setResizable(true,false);window->setResizeLimits(720,500,1800,1200);window->getProperties().set("mrs-midi-clip",true);window->getProperties().set("mrs-midi-id",juce::String::fromUTF8(id.value.c_str()));window->setOwner(*this);windows.push_back(std::move(window));}
}
