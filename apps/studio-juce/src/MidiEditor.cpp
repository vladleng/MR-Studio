#include "PianoRoll.h"
#include "MusicalEdit.h"
#include "ControllerEditor.h"
#include "WaveformView.h"
#include "ChordTrack.h"
#include "MarkerTrack.h"
#include <sstream>
#include <cmath>
#include <mrs/vst3.hpp>
namespace ui {
class MidiClipPanel final : public juce::Component {
public:
    MidiClipPanel(Desktop& d,mrs::Id clip):owner(d),id(std::move(clip)),roll(d,id),controllerLane(d,id){
        setLookAndFeel(&owner.theme);setName("Piano roll");
        addAndMakeVisible(controllerLane);controllerLane.setComponentID("embedded-controllers");controllerLane.setEmbedded();controllerLane.setVisible(owner.view.controllersOpen);
        viewport.changed=[this]{alignControllers();};roll.viewChanged=[this]{alignControllers();};
        roll.wheel=[this](float delta,juce::ModifierKeys mods,float x,float y){wheel(delta,mods,x,y);};
        controllerLane.wheel=[this](const juce::MouseEvent& e,const juce::MouseWheelDetails& w){
            // Translate lane coordinates into the shared roll, including scroll offset.
            // Vertical zoom uses the visible roll centre, not an offscreen lane Y.
            const auto translated=e.getEventRelativeTo(&roll);
            const auto anchored=translated.withNewPosition(juce::Point<float>{translated.position.x,viewport.getViewPositionY()+viewport.getViewHeight()*.5f});
            roll.mouseWheelMove(anchored,w);
        };
        addAndMakeVisible(sideViewport);sideViewport.setViewedComponent(&sideBody,false);sideViewport.setScrollBarsShown(true,false);
        addAndMakeVisible(viewport);viewport.setViewedComponent(&roll,false);viewport.setScrollBarsShown(true,true);
        controllerLane.toFront(false);addAndMakeVisible(divider);
        for(std::size_t i=0;i<quickCC.size();++i){auto& b=quickCC[i];const int cc=ccNumbers[i];divider.addAndMakeVisible(b);b.setButtonText("CC"+juce::String(cc));b.setTitle("Show controller "+juce::String(cc)+" lane");b.setComponentID("controller-cc"+juce::String(cc));b.getProperties().set("channelFont",true);b.onClick=[this,cc]{roll.cancel();controllerLane.selectCC(cc);};}
        controllerLane.laneChanged=[this]{updateQuickCC();};updateQuickCC();
        addAndMakeVisible(snap);snap.addItem("Snap off",1);snap.addItem("1/4",2);snap.addItem("1/8",3);snap.addItem("1/16",4);snap.addItem("1/32",5);snap.setSelectedId(4);snap.setTitle("Piano roll snap");snap.onChange=[this]{const std::array<mrs::Tick,5> values{0,mrs::ppq,mrs::ppq/2,mrs::ppq/4,mrs::ppq/8};roll.grid=values[static_cast<std::size_t>(snap.getSelectedId()-1)];roll.repaint();};
        for(auto* b:{&zoomIn,&zoomOut,&copy,&paste,&selectAll,&musical,&controllers,&attach,&details,&fullScreen})addAndMakeVisible(b);
        zoomIn.onClick=[this]{zoom(1.25f);};zoomOut.onClick=[this]{zoom(.8f);};copy.onClick=[this]{roll.copy();};paste.onClick=[this]{roll.paste();};selectAll.onClick=[this]{for(const auto& n:roll.notes)roll.selected.insert(n.id.value);roll.repaint();};
        musical.setComponentID("musical-edit-open");musical.onClick=[this]{roll.cancel();auto* musicalPanel=new MusicalEditPanel(owner.theme,[safe=juce::Component::SafePointer<MidiClipPanel>(this)](bool all,const mrs::NoteEdit& edit){if(!safe||!safe->isShowing())throw std::runtime_error("Piano roll is closed");const auto p=safe->owner.project();const auto& clip=*safe->current(*p).midi;std::vector<mrs::Id> selected;for(const auto& n:clip.notes)if(all?(n.start<clip.source_offset+clip.length&&n.start+n.length>clip.source_offset):safe->roll.selected.contains(n.id.value))selected.push_back(n.id);if(selected.empty())throw std::runtime_error("Select notes in piano roll or choose Whole clip");safe->owner.app.edit_midi_notes(safe->id,std::move(selected),edit);safe->owner.refresh();safe->roll.sync();safe->reload();});auto window=std::make_unique<EditorWindow>("Musical note edit",musicalPanel);window->getProperties().set("mrs-midi-clip",true);window->setOwner(owner);owner.windows.push_back(std::move(window));};
        controllers.setComponentID("controller-edit-open");controllers.onClick=[this]{roll.cancel();controllerLane.cancelGesture();controllerLane.setVisible(!controllerLane.isVisible());owner.view.controllersOpen=controllerLane.isVisible();owner.saveSettings();resized();};
        fullScreen.setComponentID("editor-fullscreen");fullScreen.setTitle("Toggle MIDI editor full screen (F11)");fullScreen.onClick=[this]{deferFullScreen();};attach.setComponentID("editor-attach");attach.onClick=[this]{deferAttach();};details.onClick=[this]{roll.cancel();controllerLane.cancelGesture();showDetails=!showDetails;resized();};
        roll.error=[this](auto text){hint.setText(text,juce::dontSendNotification);};roll.changed=[this]{reload();};
        for(auto* b:{&pointer,&split}){addAndMakeVisible(b);b->getProperties().set("mrSmallToolbarIcon",true);}
        pointer.getProperties().set("mrHostIcon","tool_pointer");split.getProperties().set("mrHostIcon","tool_split");
        pointer.setComponentID("piano-pointer");split.setComponentID("piano-split");pointer.setTitle("Pointer (1)");split.setTitle("Split notes (3)");pointer.setTooltip(pointer.getTitle());split.setTooltip("Split notes (3); S splits selected notes at the cursor");
        pointer.onClick=[this]{roll.setSplitTool(false);roll.grabKeyboardFocus();};split.onClick=[this]{roll.setSplitTool(true);roll.grabKeyboardFocus();};
        roll.toolChanged=[this]{pointer.setToggleState(!roll.splitTool,juce::dontSendNotification);split.setToggleState(roll.splitTool,juce::dontSendNotification);};roll.toolChanged();
        addAndMakeVisible(noteColors);noteColors.setComponentID("piano-note-colors");noteColors.setTitle("Note color mode");noteColors.setTooltip("Velocity colors or clip color; note fill always shows velocity");noteColors.addItem("Velocity colors",1);noteColors.addItem("Clip color",2);noteColors.setSelectedId(owner.view.noteClipColors?2:1,juce::dontSendNotification);
        noteColors.onChange=[this]{roll.cancel();owner.view.noteClipColors=noteColors.getSelectedId()==2;owner.saveSettings();roll.repaint();};
        addAndMakeVisible(selector);selector.setTitle("Select MIDI note");selector.onChange=[this]{loadNote();const int index=selector.getSelectedId()-1;if(index>=0&&index<static_cast<int>(notes.size())){roll.selected={notes[static_cast<std::size_t>(index)].id.value};roll.repaint();}};
        const std::array<juce::String,5> names{"Pitch (0..127)","Start (beats)","Length (beats)","Velocity (1..127)","Channel (1..16)"};
        for(std::size_t i=0;i<fields.size();++i){labels[i].setText(names[i],juce::dontSendNotification);labels[i].setFont(owner.theme.font(13));addAndMakeVisible(labels[i]);fields[i].setFont(owner.theme.font(13));fields[i].setTitle(names[i]);fields[i].setComponentID("midi-note-"+juce::String(static_cast<int>(i)));addAndMakeVisible(fields[i]);}
        for(auto* b:{&add,&apply,&remove})addAndMakeVisible(*b);
        add.setComponentID("midi-note-add");apply.setComponentID("midi-note-apply");remove.setComponentID("midi-note-delete");
        add.onClick=[this]{edit(0);};apply.onClick=[this]{edit(1);};remove.onClick=[this]{edit(2);};
        hint.setFont(Theme::channelFont());hint.setJustificationType(juce::Justification::topLeft);addAndMakeVisible(hint);
        for(auto* c:std::array<juce::Component*,15>{&snap,&zoomIn,&zoomOut,&copy,&paste,&selectAll,&musical,&controllers,&attach,&details,&fullScreen,&selector,&add,&apply,&remove}){sideBody.addAndMakeVisible(c);c->getProperties().set("channelFont",true);}
        for(auto& l:labels){sideBody.addAndMakeVisible(l);l.setFont(Theme::channelFont());}
        for(auto& f:fields){sideBody.addAndMakeVisible(f);f.setFont(Theme::channelFont());}
        sideBody.addAndMakeVisible(hint);sideBody.addAndMakeVisible(controllerLane.controlsContent());
        details.setButtonText("Notes");details.setTitle("Show note details");
        const std::array<juce::String,5> compactNames{"Pitch","Start beat","Length","Velocity","Channel"};for(std::size_t i=0;i<labels.size();++i)labels[i].setText(compactNames[i],juce::dontSendNotification);
        reload();setSize(1000,800);
    }
    bool keyPressed(const juce::KeyPress& key) override{if(owner.workspaceShortcut(key))return true;if(key.getKeyCode()==juce::KeyPress::F11Key){deferFullScreen();return true;}for(auto* c=juce::Component::getCurrentlyFocusedComponent();c&&c!=this;c=c->getParentComponent())if(dynamic_cast<juce::TextEditor*>(c)||dynamic_cast<juce::ComboBox*>(c))return false;return roll.keyPressed(key);}
    void activate(mrs::Id next){id=next;roll.activate(next);controllerLane.activate(next);showDetails=false;controllerLane.setVisible(owner.view.controllersOpen);reload();resized();}
    ~MidiClipPanel() override{controllerLane.laneChanged=nullptr;sideViewport.setViewedComponent(nullptr,false);viewport.setViewedComponent(nullptr,false);setLookAndFeel(nullptr);}
    void resized() override{
        const int sideWidth=juce::jlimit(156,196,getWidth()/4);
        sideViewport.setBounds(0,0,sideWidth,getHeight());
        const int width=sideWidth-14,w=width-12,half=(w-4)/2;
        snap.setBounds(6,6,w,24);
        zoomIn.setBounds(6,34,half,24);zoomOut.setBounds(10+half,34,half,24);
        copy.setBounds(6,62,half,24);paste.setBounds(10+half,62,half,24);
        selectAll.setBounds(6,90,w,24);musical.setBounds(6,118,w,24);
        controllers.setBounds(6,146,half,24);details.setBounds(10+half,146,half,24);
        attach.setBounds(6,174,half,24);fullScreen.setBounds(10+half,174,half,24);
        fullScreen.setButtonText(fullScreenActive?"Restore":"Full screen");
        attach.setButtonText(static_cast<bool>(getProperties()["mrs-attached"])?"Detach":"Attach");
        controllers.setToggleState(controllerLane.isVisible(),juce::dontSendNotification);
        details.setToggleState(showDetails,juce::dontSendNotification);
        selector.setVisible(showDetails);for(auto& l:labels)l.setVisible(showDetails);
        for(auto& f:fields)f.setVisible(showDetails);
        for(auto* b:{&add,&apply,&remove})b->setVisible(showDetails);hint.setVisible(showDetails);
        int y=206;selector.setBounds(6,y,w,24);y+=28;
        for(std::size_t i=0;i<fields.size();++i){labels[i].setBounds(6,y,76,24);fields[i].setBounds(82,y,w-76,24);y+=28;}
        add.setBounds(6,y,w,24);apply.setBounds(6,y+28,half,24);remove.setBounds(10+half,y+28,half,24);
        hint.setBounds(6,y+56,w,58);hint.setTooltip(hint.getText());
        if(!showDetails)y=206;else y+=118;
        auto& form=controllerLane.controlsContent();form.setVisible(controllerLane.isVisible());
        controllerLane.layoutControls(width);form.setTopLeftPosition(0,y);
        sideBody.setSize(width,juce::jmax(getHeight(),y+(form.isVisible()?form.getHeight():0)));
        // Roll always owns the full content area; only the foreground overlay changes.
        pointer.setBounds(sideWidth+4,2,32,28);split.setBounds(sideWidth+38,2,32,28);noteColors.setBounds(sideWidth+82,4,juce::jmin(156,juce::jmax(90,getWidth()-sideWidth-94)),24);
        viewport.setBounds(sideWidth+4,36,juce::jmax(90,getWidth()-sideWidth-12),juce::jmax(90,getHeight()-44));
        layoutOverlay();alignControllers();
        if(firstLayout){firstLayout=false;viewport.setViewPosition(static_cast<int>(static_cast<double>(roll.clip.start)/mrs::ppq*roll.beatWidth),PianoRoll::ruler+(127-78)*static_cast<int>(roll.rowHeight));}
    }
    void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(background));}
private:
    friend void controllerScrollSmoke();
    friend void noteEditorLayoutSmoke();
    friend void pianoRoll214Smoke();
    Desktop& owner;mrs::Id id;
    struct RollViewport : juce::Viewport {std::function<void()> changed;void visibleAreaChanged(const juce::Rectangle<int>&) override{if(changed)changed();}};
    PianoRoll roll;ControllerPanel controllerLane;RollViewport viewport;bool showDetails{};juce::ComboBox snap;bool firstLayout{true};
    bool fullScreenActive{},returnAttached{};
    juce::TextButton pointer{""},split{""};juce::ComboBox noteColors;
    juce::Component sideBody;juce::Viewport sideViewport;
    static constexpr std::array<int,3> ccNumbers{1,11,64};
    std::array<juce::TextButton,3> quickCC;
    int overlayHeight{180};
    struct Divider final : juce::Component {
        MidiClipPanel& owner;int startHeight{};bool dragging{};
        explicit Divider(MidiClipPanel& p):owner(p){setComponentID("controller-divider");setName("Resize controller overlay");setMouseCursor(juce::MouseCursor::UpDownResizeCursor);setWantsKeyboardFocus(true);}
        void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(panel));g.setColour(juce::Colour(menuBorder));g.drawHorizontalLine(0,0.f,static_cast<float>(getWidth()));g.setColour(juce::Colours::lightgrey.withAlpha(isMouseOver()?.8f:.35f));const float x=(getWidth()+160.f)*.5f;g.drawLine(x-16,9,x+16,9,1);g.drawLine(x-16,13,x+16,13,1);}
        void mouseDown(const juce::MouseEvent& e) override{if(!e.mods.isLeftButtonDown())return;owner.roll.cancel();owner.controllerLane.cancelGesture();startHeight=owner.controllerLane.getHeight();dragging=true;grabKeyboardFocus();}
        void mouseDrag(const juce::MouseEvent& e) override{if(dragging&&e.mods.isLeftButtonDown())owner.setOverlayHeight(startHeight-e.getDistanceFromDragStartY());}
        void mouseUp(const juce::MouseEvent&) override{dragging=false;}
        void focusLost(FocusChangeType) override{dragging=false;}
        bool keyPressed(const juce::KeyPress& k) override{if(k.getKeyCode()==juce::KeyPress::upKey||k.getKeyCode()==juce::KeyPress::downKey){owner.roll.cancel();owner.controllerLane.cancelGesture();owner.setOverlayHeight(owner.controllerLane.getHeight()+(k.getKeyCode()==juce::KeyPress::upKey?16:-16));return true;}if(k==juce::KeyPress::escapeKey&&dragging){dragging=false;owner.setOverlayHeight(startHeight);return true;}return false;}
    } divider{*this};
    void updateQuickCC(){for(std::size_t i=0;i<quickCC.size();++i)quickCC[i].setToggleState(controllerLane.displayedCC()==ccNumbers[i],juce::dontSendNotification);}
    void setOverlayHeight(int height){overlayHeight=juce::jlimit(90,juce::jmax(90,viewport.getHeight()-70),height);layoutOverlay();}
    void layoutOverlay(){
        divider.setVisible(controllerLane.isVisible());
        const int h=juce::jlimit(90,juce::jmax(90,viewport.getHeight()-70),overlayHeight);
        controllerLane.setBounds(viewport.getX(),viewport.getBottom()-14-h,viewport.getWidth(),h);
        divider.setBounds(viewport.getX(),controllerLane.getY()-22,viewport.getWidth(),22);
        for(std::size_t i=0;i<quickCC.size();++i)quickCC[i].setBounds(4+static_cast<int>(i)*52,2,48,18);
    }
    juce::TextButton fullScreen{"Full screen"},zoomIn{"Zoom +"},zoomOut{"Zoom -"},copy{"Copy"},paste{"Paste"},selectAll{"Select all"},musical{"Musical edit..."},controllers{"Controllers"},attach{"Attach"},details{"Note details"};
    void deferFullScreen(){juce::Component::SafePointer<MidiClipPanel> safe(this);juce::MessageManager::callAsync([safe]{if(!safe)return;safe->roll.cancel();safe->controllerLane.cancelGesture();if(!safe->fullScreenActive){safe->returnAttached=static_cast<bool>(safe->getProperties()["mrs-attached"]);if(safe->returnAttached){safe->owner.attachClipEditor(safe,safe->id,false,false);safe->owner.view.editorAttached=true;}if(auto* w=safe->findParentComponentOfClass<EditorWindow>()){safe->fullScreenActive=true;w->onClose=[safe]{if(!safe)return;if(safe->fullScreenActive){if(auto* host=safe->findParentComponentOfClass<EditorWindow>())host->setFullScreen(false);safe->fullScreenActive=false;if(safe->returnAttached){safe->owner.attachClipEditor(safe,safe->id,true,false);safe->setVisible(false);}safe->resized();}safe->owner.setWorkspace(mrs::desktop::Workspace::arrange);};w->setFullScreen(true);safe->resized();}}else{if(auto* w=safe->findParentComponentOfClass<EditorWindow>())w->setFullScreen(false);safe->fullScreenActive=false;if(safe->returnAttached)safe->owner.attachClipEditor(safe,safe->id,true,false);safe->resized();}});}
    void deferAttach(){juce::Component::SafePointer<MidiClipPanel> safe(this);juce::MessageManager::callAsync([safe]{if(safe){safe->roll.cancel();safe->controllerLane.cancelGesture();if(safe->fullScreenActive){if(auto* w=safe->findParentComponentOfClass<EditorWindow>())w->setFullScreen(false);safe->fullScreenActive=false;}safe->owner.attachClipEditor(safe,safe->id,!static_cast<bool>(safe->getProperties()["mrs-attached"]));}});}
    void alignControllers(){if(viewport.getWidth()<=0)return;controllerLane.setTimeView(roll.clip.source_offset-roll.clip.start+static_cast<double>(viewport.getViewPositionX())/roll.beatWidth*mrs::ppq,juce::jmax(1,viewport.getWidth()-PianoRoll::keys-16)/roll.beatWidth*mrs::ppq);}
    void wheel(float delta,juce::ModifierKeys mods,float x,float y){roll.cancel();const auto vx=viewport.getViewPositionX(),vy=viewport.getViewPositionY();if(mods.isCtrlDown()){if(mods.isShiftDown()){const float before=roll.beatWidth;roll.beatWidth=juce::jlimit(24.f,384.f,before*std::pow(1.25f,delta*4));roll.sync();viewport.setViewPosition(juce::jmax(0,static_cast<int>((x-PianoRoll::keys)*roll.beatWidth/before-(x-vx-PianoRoll::keys))),vy);}else{const float before=roll.rowHeight;roll.rowHeight=juce::jlimit(8.f,48.f,before+std::round(delta*24));roll.sync();viewport.setViewPosition(vx,juce::jmax(0,static_cast<int>((y-PianoRoll::ruler)*roll.rowHeight/before-(y-vy-PianoRoll::ruler))));}}else if(mods.isShiftDown())viewport.setViewPosition(juce::jmax(0,vx-static_cast<int>(delta*160)),vy);else viewport.setViewPosition(vx,juce::jmax(0,vy-static_cast<int>(delta*128)));alignControllers();}
    void zoom(float factor){const auto x=viewport.getViewPositionX();roll.cancel();roll.beatWidth=juce::jlimit(24.f,384.f,roll.beatWidth*factor);roll.sync();viewport.setViewPosition(static_cast<int>(x*factor),viewport.getViewPositionY());alignControllers();}
    juce::ComboBox selector;
    std::array<juce::Label,5> labels;std::array<juce::TextEditor,5> fields;
    juce::TextButton add{"Add note"},apply{"Apply note"},remove{"Delete note"};juce::Label hint;
    std::vector<mrs::MidiNote> notes;
    const mrs::Clip& current(const mrs::Project& p) const{auto i=std::find_if(p.clips.begin(),p.clips.end(),[&](const auto& c){return c.id==id;});if(i==p.clips.end()||!i->midi)throw std::runtime_error("MIDI clip no longer exists");return *i;}
    double number(std::size_t i) const{std::istringstream in(fields[i].getText().toStdString());in.imbue(std::locale::classic());double value{};if(!(in>>value)||!std::isfinite(value))throw std::runtime_error("Enter a valid number in every field");in>>std::ws;if(!in.eof())throw std::runtime_error("Unexpected characters in note field");return value;}
    void loadNote(){const int index=selector.getSelectedId()-1;mrs::MidiNote n;if(index>=0&&index<static_cast<int>(notes.size()))n=notes[static_cast<std::size_t>(index)];else{const auto p=owner.project();n.start=current(*p).midi->source_offset;}
        fields[0].setText(juce::String(n.pitch));fields[1].setText(juce::String(static_cast<double>(n.start)/mrs::ppq,6));fields[2].setText(juce::String(static_cast<double>(n.length)/mrs::ppq,6));fields[3].setText(juce::String(n.velocity));fields[4].setText(juce::String(n.channel+1));apply.setEnabled(index>=0);remove.setEnabled(index>=0);}
    void reload(int selected=1){const auto p=owner.project();const auto& c=current(*p);notes=c.midi->notes;selector.clear(juce::dontSendNotification);for(std::size_t i=0;i<notes.size();++i){const auto& n=notes[i];selector.addItem("Note "+juce::String(static_cast<int>(i)+1)+" | pitch "+juce::String(n.pitch)+" | beat "+juce::String(static_cast<double>(n.start)/mrs::ppq,3),static_cast<int>(i)+1);}selector.setTextWhenNothingSelected("Empty MIDI clip — add a note");selector.setSelectedId(juce::jlimit(0,static_cast<int>(notes.size()),selected),juce::dontSendNotification);loadNote();
        hint.setText("Start and length use quarter-note beats. Start is relative to the original clip source;\nDouble click: create/remove. Ctrl/Shift click or drag empty space: select. Drag: move; both edges: trim. Alt-drag: copy; Ctrl+Alt-drag: velocity.\nD: duplicate; S: split; Shift+Up/Down: octave; Ctrl+Shift: octave copy. Ctrl+C/V/A/Z/Y; Delete.",juce::dontSendNotification);}
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

void controllerScrollSmoke(){
    auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    Desktop d(true);d.app.new_project();const auto track=d.app.add_instrument_track("Scroll fixture");
    const auto id=d.app.create_midi_clip(track,4*mrs::ppq,32*mrs::ppq);
    mrs::MidiNote note;note.id=mrs::new_id();note.start=8*mrs::ppq;note.length=mrs::ppq;note.pitch=60;d.app.set_midi_notes(id,{note});
    mrs::MidiChannelEvent point;point.id=mrs::new_id();point.kind=2;point.data1=64;point.data2=100;point.start=note.start;d.app.set_midi_events(id,{point});
    const mrs::Timeline timeline(d.project()->time,d.project()->sample_rate);
    d.app.trim_clip(id,timeline.to_samples(6*mrs::ppq),timeline.to_samples(36*mrs::ppq));
    d.view.controllersOpen=true;MidiClipPanel editor(d,id);editor.setSize(1000,800);editor.resized();
    auto& lane=editor.controllerLane;auto& graph=lane.graph;auto& viewport=editor.viewport;auto& roll=editor.roll;
    const auto before=*d.project();const auto revision=d.app.services().projects->state().revision;
    auto mouse=[](juce::Component& c,juce::Point<float> p,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(mods),1,0,0,0,0,&c,&c,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,false);};
    auto scroll=[&](float dx,float dy,int mods){juce::MouseWheelDetails w{};w.deltaX=dx;w.deltaY=dy;graph.mouseWheelMove(mouse(graph,lane.plot().getCentre(),mods),w);};
    auto aligned=[&]{const auto noteX=roll.getX()+editor.viewport.getX()+roll.noteRect(note).getX();const auto pointX=lane.getX()+graph.getX()+lane.point(point).x;check(std::abs(noteX-pointX)<.1f,"controller/note pixels share timeline");};
    aligned();const int x=viewport.getViewPositionX(),y=viewport.getViewPositionY();scroll(0,-.5f,juce::ModifierKeys::shiftModifier);check(viewport.getViewPositionX()==x+80&&viewport.getViewPositionY()==y,"lane Shift-wheel pans shared time");aligned();
    scroll(0,.5f,juce::ModifierKeys::shiftModifier);check(viewport.getViewPositionX()==x,"lane wheel pans both directions");scroll(-.5f,0,0);check(viewport.getViewPositionX()==x+80,"native horizontal wheel pans shared time");aligned();
    const float timeScale=roll.beatWidth;const double anchor=lane.viewFirst+lane.viewLength*.5;scroll(0,.25f,juce::ModifierKeys::ctrlModifier|juce::ModifierKeys::shiftModifier);check(roll.beatWidth>timeScale,"lane Ctrl-Shift-wheel time zoom");check(std::abs(anchor-lane.viewFirst-lane.viewLength*.5)<=mrs::ppq/roll.beatWidth,"time zoom keeps pointer anchor");aligned();
    const int oldY=viewport.getViewPositionY(),oldX=viewport.getViewPositionX();scroll(0,-.25f,0);check(viewport.getViewPositionX()==oldX&&viewport.getViewPositionY()>oldY,"plain lane wheel keeps vertical scroll");
    const float rows=roll.rowHeight;scroll(0,.25f,juce::ModifierKeys::ctrlModifier);check(roll.rowHeight>rows,"lane Ctrl-wheel keeps vertical zoom");aligned();
    scroll(0,1000,juce::ModifierKeys::shiftModifier);check(viewport.getViewPositionX()==0,"left scroll boundary");scroll(0,-1000,juce::ModifierKeys::shiftModifier);check(viewport.getViewPositionX()<=roll.getWidth()-viewport.getViewWidth(),"right scroll boundary");aligned();
    viewport.setViewPosition(x,y);editor.alignControllers();const auto p=lane.point(point);graph.mouseDown(mouse(graph,p,juce::ModifierKeys::leftButtonModifier));graph.mouseDrag(mouse(graph,p.translated(60,-10),juce::ModifierKeys::leftButtonModifier));scroll(0,-.25f,juce::ModifierKeys::shiftModifier);graph.mouseUp(mouse(graph,p,0));check(!lane.dragging&&lane.events==before.clips.front().midi->events,"scroll cancels point preview without commit");
    check(*d.project()==before&&d.app.services().projects->state().revision==revision,"scroll/zoom leave notes/controllers/history unchanged");
    viewport.setViewPosition(static_cast<int>(roll.noteRect(note).getX())-400,static_cast<int>(roll.noteRect(note).getY())-80);editor.alignControllers();aligned();
    for(float scale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-controller-scroll-"+juce::String(static_cast<int>(scale*100))+"-preview.png").createOutputStream();check(stream!=nullptr,"controller scroll preview stream");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*stream),"controller scroll preview");}
}

void noteEditorLayoutSmoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    Desktop d(true);d.app.new_project();const auto track=d.app.add_instrument_track("Layout fixture");
    const auto id=d.app.create_midi_clip(track,0,16*mrs::ppq);
    mrs::MidiNote n;n.id=mrs::new_id();n.start=2*mrs::ppq;n.pitch=60;n.length=mrs::ppq;d.app.set_midi_notes(id,{n});
    std::vector<mrs::MidiChannelEvent> events;for(int cc:{1,11,64}){mrs::MidiChannelEvent e;e.id=mrs::new_id();e.kind=2;e.data1=cc;e.data2=80;e.start=n.start;events.push_back(e);}d.app.set_midi_events(id,events);
    d.view.controllersOpen=true;MidiClipPanel editor(d,id);editor.setSize(1000,800);editor.setVisible(true);
    const auto before=*d.project();const auto revision=d.app.services().projects->state().revision;
    auto& lane=editor.controllerLane;auto& vp=editor.viewport;auto& roll=editor.roll;
    check(lane.channel.getSelectedId()==1&&lane.channel.getText()=="1","compact caption preserves MIDI channel selection");
    auto mouse=[](juce::Component& c,juce::Point<float> p,juce::Point<float> origin,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(mods),1,0,0,0,0,&c,&c,juce::Time::getCurrentTime(),origin,juce::Time::getCurrentTime(),1,true);};
    const auto bounds=vp.getBounds();const auto scroll=vp.getViewPosition();
    const auto origin=juce::Point<float>{300,10};
    editor.divider.mouseDown(mouse(editor.divider,origin,origin,juce::ModifierKeys::leftButtonModifier));
    editor.divider.mouseDrag(mouse(editor.divider,origin.translated(0,-120),origin,juce::ModifierKeys::leftButtonModifier));
    check(lane.getHeight()==300&&vp.getBounds()==bounds&&vp.getViewPosition()==scroll,"controller overlay expands without shrinking/moving roll");
    editor.divider.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));check(lane.getHeight()==180,"overlay Escape restores height");
    editor.divider.keyPressed(juce::KeyPress(juce::KeyPress::upKey));check(lane.getHeight()==196&&vp.getBounds()==bounds,"overlay keyboard resize");
    editor.setOverlayHeight(10000);check(editor.divider.getY()>=vp.getY()+34,"overlay leaves roll header reachable");
    editor.setOverlayHeight(-100);check(lane.getHeight()==90,"overlay minimum graph height");
    editor.setOverlayHeight(220);
    for(std::size_t i=0;i<editor.quickCC.size();++i){editor.quickCC[i].onClick();check(lane.displayedCC()==editor.ccNumbers[i]&&lane.matches(events[i]),"quick CC selects correct lane");for(std::size_t j=0;j<editor.quickCC.size();++j)check(editor.quickCC[j].getToggleState()==(i==j),"CC button state follows lane");}
    lane.kind.setSelectedId(5,juce::sendNotificationSync);for(auto& b:editor.quickCC)check(!b.getToggleState(),"pitch bend clears CC toggle");
    lane.setEnabled(false);check(!lane.controlsContent().isEnabled(),"reparented controller form follows disabled state");lane.setEnabled(true);
    lane.selectCC(1);auto p=lane.point(events.front());lane.graph.mouseDown(mouse(lane.graph,p,p,juce::ModifierKeys::leftButtonModifier));lane.graph.mouseDrag(mouse(lane.graph,p.translated(50,-10),p,juce::ModifierKeys::leftButtonModifier));editor.quickCC[1].onClick();lane.graph.mouseUp(mouse(lane.graph,p,p,0));check(!lane.dragging&&lane.events==events,"switch CC cancels controller preview without commit");
    lane.selectCC(1);p=lane.point(events.front());lane.graph.mouseDown(mouse(lane.graph,p,p,juce::ModifierKeys::leftButtonModifier));lane.graph.mouseDrag(mouse(lane.graph,p.translated(40,-8),p,juce::ModifierKeys::leftButtonModifier));lane.controlsContent().keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));lane.graph.mouseUp(mouse(lane.graph,p,p,0));check(!lane.dragging&&lane.events==events,"reparented form retains controller Escape route");
    const auto graphPoint=lane.getPosition()+lane.graph.getPosition()+lane.plot().getCentre().toInt();
    check(editor.getComponentAt(graphPoint)==&lane.graph,"overlay intercepts covered roll");
    const auto exposed=juce::Point<int>{vp.getX()+100,vp.getY()+80};check(editor.getComponentAt(exposed)==&roll,"uncovered roll retains hit testing");
    const auto ccPoint=editor.divider.getPosition()+editor.quickCC.front().getBounds().getCentre();check(editor.getComponentAt(ccPoint)==&editor.quickCC.front(),"CC buttons own their hit region");
    editor.details.onClick();check(editor.showDetails&&lane.isVisible(),"note fields coexist with controller lane");check(vp.getBounds()==bounds,"details do not shrink roll");
    editor.setSize(720,300);check(editor.sideViewport.getHeight()==300&&editor.sideBody.getHeight()>300,"small editor scrolls all sidebar controls");
    editor.sideViewport.setViewPosition(0,10000);auto& form=lane.controlsContent();check(editor.sideViewport.getViewPositionY()>0&&form.getBottom()<=editor.sideBody.getHeight(),"controller controls reachable at sidebar bottom");
    check(lane.getBounds().getIntersection(vp.getBounds())==lane.getBounds()&&editor.divider.getY()>=vp.getY(),"small window clamps overlay");
    editor.setSize(1000,800);editor.details.onClick();editor.sideViewport.setViewPosition(0,0);editor.setOverlayHeight(220);
    vp.setViewPosition(0,static_cast<int>(roll.noteRect(n).getY())-100);editor.alignControllers();lane.selectCC(64);
    check(*d.project()==before&&d.app.services().projects->state().revision==revision,"layout/CC/resize preserve MIDI model and Undo revision");
    for(float scale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-note-layout-"+juce::String(static_cast<int>(scale*100))+"-preview.png").createOutputStream();check(stream!=nullptr,"note layout preview stream");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*stream),"note layout preview");}
    editor.setSize(720,300);editor.details.onClick();editor.sideViewport.setViewPosition(0,10000);
    auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-note-layout-small-preview.png").createOutputStream();check(stream!=nullptr,"small layout preview stream");stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds(),true,1.f,juce::SoftwareImageType{}),*stream);
}

void pianoRoll214Smoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    Desktop d(true);d.app.new_project(48000);
    const auto folder=juce::File::getCurrentWorkingDirectory(),fixture=folder.getChildFile("../../Release/mrs_vst3_fixture.vst3"),instrumentFile=folder.getChildFile("instrument.vst3");
    check(fixture.existsAsFile()&&fixture.copyFileTo(instrumentFile),"Isolated VST3 instrument fixture");
    const auto descriptor=mrs::processing::probe_vst3(instrumentFile.getFullPathName().toStdString()).front();
    mrs::NativeInsert synth;synth.id=mrs::new_id();synth.kind=mrs::InsertKind::vst3;synth.plugin_path=descriptor.path;synth.class_id=descriptor.class_id;synth.plugin_name=descriptor.name;
    const auto track=d.app.add_instrument_track("Piano / Ноты"),audio=d.app.add_audio_track("Native FX");d.app.set_inserts(track,{synth});
    mrs::NativeInsert gain;gain.id=mrs::new_id();gain.kind=mrs::InsertKind::gain;d.app.set_inserts(audio,{gain});
    const auto clipId=d.app.create_midi_clip(track,4*mrs::ppq,16*mrs::ppq);
    const std::vector<mrs::MidiNote> base{{mrs::new_id(),4*mrs::ppq,2*mrs::ppq,60,32,2},{mrs::new_id(),5*mrs::ppq,2*mrs::ppq,64,96,5},{mrs::new_id(),8*mrs::ppq,mrs::ppq,67,127,0}};
    d.app.set_midi_notes(clipId,base);
    const mrs::Timeline time(d.project()->time,d.project()->sample_rate);d.app.trim_clip(clipId,time.to_samples(6*mrs::ppq),time.to_samples(20*mrs::ppq));
    mrs::MidiChannelEvent cc;cc.id=mrs::new_id();cc.start=4*mrs::ppq;cc.kind=2;cc.data1=1;cc.data2=60;d.app.set_midi_events(clipId,{cc});d.app.set_clip_colors({clipId},0xb45df0);
    d.refresh(true);d.openInsert(audio,gain.id);check(!d.windows.empty(),"Native FX window");juce::Component::SafePointer<EditorWindow> native(d.windows.back().get());native->getProperties().set("mrs-pinned",true);
    d.openInsert(track,synth.id);check(d.windows.size()==2,"Synthetic instrument window");juce::Component::SafePointer<EditorWindow> instrument(d.windows.back().get());instrument->getProperties().set("mrs-pinned",true);
    const auto generation=d.app.insert_generation();
    MidiClipPanel editor(d,clipId);editor.setSize(1000,700);editor.setVisible(true);editor.resized();auto& roll=editor.roll;auto& vp=editor.viewport;
    const auto state=[&](){for(const auto& c:d.project()->clips)if(c.id==clipId)return c.midi->notes;throw std::runtime_error("Fixture clip missing");};
    const auto rev=[&](){return d.app.services().projects->state().revision;};
    const auto lifetime=[&](){check(d.app.insert_generation()==generation&&native&&native->isVisible()&&instrument&&instrument->isVisible(),"Note edit/history retains exact native/VST3 editors and DSP generation");for(const auto& c:d.project()->clips)if(c.id==clipId)check(c.midi->events==std::vector<mrs::MidiChannelEvent>{cc},"Note edits preserve controller events");};
    const auto selectGroup=[&](){roll.sync();roll.selected={base[0].id.value,base[1].id.value};};
    const auto undo=[&](const std::vector<mrs::MidiNote>& expected){check(d.app.history_preserves_plugins(false),"Toolbar Undo retains plugins");d.action(10);roll.sync();check(state()==expected,"One Undo restores complete note group");lifetime();};
    const auto mouse=[&](juce::Point<float> p,juce::Point<float> from,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(mods),1,0,0,0,0,&roll,&roll,juce::Time::getCurrentTime(),from,juce::Time::getCurrentTime(),1,true);};
    const auto drag=[&](juce::Point<float> from,juce::Point<float> to,int mods){const auto revision=rev();roll.mouseDown(mouse(from,from,mods));roll.mouseDrag(mouse(to,from,mods));check(rev()==revision,"Drag remains local preview");roll.mouseUp(mouse(to,from,0));};
    const int left=juce::ModifierKeys::leftButtonModifier,shift=juce::ModifierKeys::shiftModifier,ctrl=juce::ModifierKeys::ctrlModifier,alt=juce::ModifierKeys::altModifier;
    check(roll.clip.source_offset==2*mrs::ppq,"Trimmed-source fixture");
    check(PianoRoll::noteName(60)=="C4"&&PianoRoll::noteName(61)=="C#4"&&PianoRoll::noteName(127)=="G9","Pitch labels use C4=60");
    auto point=roll.noteRect(base[0]).getCentre();roll.selected={base[2].id.value};const auto selectionRevision=rev();
    roll.mouseDown(mouse(point,point,left|shift));roll.mouseUp(mouse(point,point,0));check(roll.selected.size()==2&&roll.selected.contains(base[0].id.value),"Shift click adds note");
    roll.mouseDown(mouse(point,point,left|shift));check(roll.selected.size()==2,"Shift removal deferred until release");roll.mouseUp(mouse(point,point,0));check(roll.selected==std::set<std::string>{base[2].id.value},"Shift click removes note");
    const auto boxStart=roll.noteRect(base[0]).getTopLeft().withY(roll.noteRect(base[1]).getY()-3).translated(-8,0),boxEnd=roll.noteRect(base[1]).getBottomRight().withY(roll.noteRect(base[0]).getBottom()+3).translated(3,0);
    roll.mouseDown(mouse(boxStart,boxStart,left|shift));roll.mouseDrag(mouse(boxEnd,boxStart,left|shift));roll.mouseUp(mouse(boxEnd,boxStart,0));check(roll.selected.size()==3&&rev()==selectionRevision,"Shift rectangle adds group without history");
    selectGroup();drag(point,point.translated(24,-roll.rowHeight),left);auto moved=state();check(moved[0].start==base[0].start+mrs::ppq/4&&moved[1].pitch==base[1].pitch+1&&moved[2]==base[2],"Snapped group move preserves relative positions and unrelated note");lifetime();undo(base);
    selectGroup();point=roll.noteRect(base[0]).getCentre();const auto copyRevision=rev();roll.mouseDown(mouse(point,point,left|alt));roll.mouseDrag(mouse(point.translated(96,-roll.rowHeight),point,left|alt));
    check(rev()==copyRevision&&state()==base&&roll.notes.size()==5,"Alt group copy preview leaves model unchanged");const auto previewIds=roll.selected;roll.mouseDrag(mouse(point.translated(120,-roll.rowHeight),point,left|alt));check(roll.selected==previewIds,"Copy preview IDs stable");
    roll.mouseUp(mouse(point.translated(120,-roll.rowHeight),point,0));const auto copies=state();check(copies.size()==5&&std::equal(base.begin(),base.end(),copies.begin())&&rev()==copyRevision+1,"Alt copy commits once and retains originals");
    check(copies[3].id!=base[0].id&&copies[3].pitch==61&&copies[3].velocity==32&&copies[3].channel==2,"Copy preserves note metadata");undo(base);d.action(11);roll.sync();check(state()==copies,"Redo restores committed copy IDs");undo(base);
    selectGroup();point=roll.noteRect(base[0]).getCentre();const auto cancelRevision=rev();roll.mouseDown(mouse(point,point,left|alt));roll.mouseDrag(mouse(point.translated(96,0),point,left|alt));roll.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));roll.mouseUp(mouse(point,point,0));check(state()==base&&rev()==cancelRevision&&roll.selected.size()==2,"Escape cancels copied group and selection");
    roll.mouseDown(mouse(point,point,left));roll.mouseDrag(mouse(point.translated(96,0),point,left));roll.focusLost(juce::Component::focusChangedDirectly);roll.mouseUp(mouse(point,point,0));check(state()==base&&rev()==cancelRevision,"Focus loss cancels preview");
    roll.mouseDown(mouse(point,point,left|alt));roll.mouseUp(mouse(point,point,0));check(state()==base&&rev()==cancelRevision,"Alt click no-op has no copy/history");
    selectGroup();auto edge=roll.noteRect(base[0]).getCentre().withX(roll.noteRect(base[0]).getX()+1);roll.mouseMove(mouse(edge,edge,0));check(roll.getMouseCursor()==noteTrimCursor(false),"Left bracket cursor");
    drag(edge,edge.translated(24,0),left);auto trimmed=state();check(trimmed[0].start==base[0].start+mrs::ppq/4&&trimmed[0].start+trimmed[0].length==base[0].start+base[0].length&&trimmed[1].start+trimmed[1].length==base[1].start+base[1].length,"Left trim group preserves note ends");undo(base);
    selectGroup();edge=roll.noteRect(base[0]).getCentre().withX(roll.noteRect(base[0]).getRight()-1);roll.mouseMove(mouse(edge,edge,0));check(roll.getMouseCursor()==noteTrimCursor(true),"Right bracket cursor");drag(edge,edge.translated(24,0),left);check(state()[0].length==base[0].length+mrs::ppq/4&&state()[0].start==base[0].start,"Right trim group");undo(base);
    selectGroup();drag(edge,edge.translated(-10000,0),left);check(state()[0].length==1&&state()[1].length==1,"Group trim clamps to one tick");undo(base);
    selectGroup();point=roll.noteRect(base[0]).getCentre();drag(point,point.translated(0,-20),left|alt|ctrl);check(state().size()==3&&state()[0].velocity==46&&state()[1].velocity==110,"Ctrl Alt edits group velocity");undo(base);
    selectGroup();roll.keyPressed(juce::KeyPress(juce::KeyPress::upKey,juce::ModifierKeys(shift),0));check(state()[0].pitch==72&&state()[1].pitch==76&&state()[0].start==base[0].start,"Shift Up moves group one octave");undo(base);
    selectGroup();roll.keyPressed(juce::KeyPress(juce::KeyPress::downKey,juce::ModifierKeys(shift|ctrl),0));check(state().size()==5&&state()[3].pitch==48&&state()[4].pitch==52&&state()[3].id!=base[0].id,"Ctrl Shift Down copies group one octave");undo(base);
    auto bounded=base;bounded[1].pitch=125;d.app.set_midi_notes(clipId,bounded);roll.sync();roll.selected={bounded[0].id.value,bounded[1].id.value};const auto boundRevision=rev();roll.transpose(12,true);check(state()==bounded&&rev()==boundRevision&&roll.selected.size()==2,"Octave copy rejects out of range group atomically");d.app.set_midi_notes(clipId,base);
    selectGroup();roll.duplicate();check(state().size()==5&&state()[3].start==7*mrs::ppq&&state()[4].start==8*mrs::ppq,"D duplicate follows selected group span");undo(base);
    selectGroup();roll.copy();roll.cursor=10*mrs::ppq;roll.paste();check(state().size()==5&&state()[3].start==10*mrs::ppq&&state()[4].start==11*mrs::ppq,"Clipboard paste keeps relative note positions");undo(base);
    selectGroup();roll.cursor=15*mrs::ppq;const auto pasteRevision=rev();roll.paste();check(state()==base&&rev()==pasteRevision&&roll.selected.size()==2,"Out of bounds paste rejects whole group");
    selectGroup();roll.keyPressed(juce::KeyPress('3'));check(roll.splitTool&&editor.split.getToggleState(),"Split tool shortcut and toolbar agree");
    const auto cut=11*mrs::ppq/2;point=roll.noteRect(base[0]).getCentre().withX(PianoRoll::keys+static_cast<float>(roll.clip.start+cut-roll.clip.source_offset)/mrs::ppq*roll.beatWidth);const auto splitRevision=rev();roll.mouseDown(mouse(point,point,left));roll.mouseUp(mouse(point,point,0));const auto halves=state();
    check(halves.size()==5&&halves[0].length==cut-base[0].start&&halves[3].start==cut&&halves[3].velocity==base[0].velocity&&halves[4].channel==base[1].channel&&rev()==splitRevision+1,"Split tool cuts selected group once, retaining velocity/channel");
    undo(base);d.action(11);roll.sync();check(state()==halves,"Split Redo retains new IDs");undo(base);roll.keyPressed(juce::KeyPress('1'));check(!roll.splitTool&&editor.pointer.getToggleState(),"Pointer shortcut");
    selectGroup();roll.cursor=base[0].start;const auto boundaryRevision=rev();roll.selected={base[0].id.value};roll.splitAt(roll.cursor);check(state()==base&&rev()==boundaryRevision,"Split at note boundary is no-op");
    const auto visualRevision=rev();d.view.noteClipColors=false;const auto p=d.project();const auto source=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==clipId;});check(roll.noteColour(base[0],*source)!=roll.noteColour(base[1],*source),"Velocity color gradation");
    editor.noteColors.setSelectedId(2,juce::sendNotificationSync);check(d.view.noteClipColors&&roll.noteColour(base[0],*source)==roll.noteColour(base[1],*source)&&rev()==visualRevision,"Clip color mode changes view only");check(ViewSettings::decode(d.view.encode()).noteClipColors,"Note color preference roundtrip");
    check(roll.noteColour(base[0],*source)==juce::Colour(0xffb45df0),"Exact clip RGB mode");roll.selected.clear();
    {juce::Image image(juce::Image::ARGB,240,40,true,juce::SoftwareImageType{});{juce::Graphics g(image);auto n=base[0];n.velocity=64;roll.drawNote(g,n,*source,{10,6,200,24},true);}auto opacityOut=folder.getChildFile("piano214-opacity.png").createOutputStream();check(opacityOut!=nullptr,"Opacity preview");opacityOut->setPosition(0);opacityOut->truncate();juce::PNGImageFormat().writeImageToStream(image,*opacityOut);check(std::abs(image.getPixelAt(70,26).getAlpha()-204)<=1&&std::abs(image.getPixelAt(190,26).getAlpha()-204)<=1,"Both velocity regions have 80 percent body opacity");check(image.getPixelAt(70,26)!=image.getPixelAt(190,26),"Full height velocity fill has visible tail");}
    std::vector<mrs::MidiNote> gallery;for(int i=0;i<5;++i){auto n=base[0];n.id=mrs::new_id();n.start=(4+i)*mrs::ppq;n.length=4*mrs::ppq;n.pitch=68-i*2;n.velocity=std::array{1,32,64,96,127}[static_cast<std::size_t>(i)];gallery.push_back(n);}d.app.set_midi_notes(clipId,gallery);roll.sync();roll.selected={gallery[2].id.value};editor.setSize(1000,700);editor.resized();vp.setViewPosition(static_cast<int>(6*roll.beatWidth),static_cast<int>(roll.noteRect(gallery.front()).getY())-60);
    const auto save=[&](juce::String name){for(float scale:{1.f,1.25f,1.5f,2.f}){auto out=folder.getChildFile(name+"-"+juce::String(juce::roundToInt(scale*100))+".png").createOutputStream();check(out!=nullptr,"Piano preview file");out->setPosition(0);out->truncate();check(juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*out),"Piano preview PNG");}};
    editor.noteColors.setSelectedId(1,juce::sendNotificationSync);save("piano214-velocity");editor.noteColors.setSelectedId(2,juce::sendNotificationSync);save("piano214-clip");
    roll.rowHeight=32;roll.beatWidth=144;roll.sync();vp.setViewPosition(static_cast<int>(6*roll.beatWidth),static_cast<int>(roll.noteRect(gallery.front()).getY())-60);save("piano214-labels");editor.setSize(720,300);editor.controllerLane.setVisible(false);editor.resized();save("piano214-minimum");lifetime();
    const auto file=folder.getChildFile("piano214-"+juce::String::fromUTF8(d.project()->id.value.c_str())+".mrsproject");d.app.save_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));const auto saved=mrs::persistence::load_project(std::filesystem::path(file.getFullPathName().toWideCharPointer())).project;
    roll.sync();roll.selected={gallery[0].id.value};point=roll.noteRect(gallery[0]).getCentre();roll.mouseDown(mouse(point,point,left));roll.mouseDrag(mouse(point.translated(48,0),point,left));d.closeEditors();d.app.open_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));roll.mouseUp(mouse(point,point,0));check(*d.project()==saved&&roll.gesture==0,"Store replacement cancels stale drag even with matching clip IDs/notes");
    controllerScrollSmoke();noteEditorLayoutSmoke();
    folder.getChildFile("piano214-results.txt").replaceWithText("PASS #214: group selection/move/copy/trim/velocity/octave/duplicate/paste/split, one Undo/Redo and stable IDs, boundaries/Escape/focus/stale Store, native+VST3 editors/DSP retained, controller events retained, color mode/preferences/80 percent fill, SaveOpen; controller scroll and editor layout regressions. Software100/125/150/200/minimum720x300. Physical ASIO/listening/MIDI keyboard/DPI NOT TESTED.\n");
}
class AudioClipPanel final : public juce::Component, private juce::Timer {
public:
    AudioClipPanel(Desktop& d,mrs::Id clip):owner(d),id(std::move(clip)),wave(*this){setLookAndFeel(&owner.theme);setName("Audio editor");setComponentID("audio-clip-editor");for(auto* c:std::array<juce::Component*,6>{&wave,&attach,&start,&end,&apply,&hint})addAndMakeVisible(c);start.setTitle("Clip start (seconds)");start.setComponentID("audio-trim-start");end.setComponentID("audio-trim-end");end.setTitle("Clip end (seconds)");apply.setTitle("Apply audio trim");attach.setComponentID("editor-attach");apply.setComponentID("audio-trim-apply");attach.onClick=[this]{juce::Component::SafePointer<AudioClipPanel> safe(this);juce::MessageManager::callAsync([safe]{if(safe)safe->owner.attachClipEditor(safe,safe->id,!static_cast<bool>(safe->getProperties()["mrs-attached"]));});};apply.onClick=[this]{owner.run([&]{const auto p=owner.project();const auto parse=[](const juce::TextEditor& f){std::istringstream in(f.getText().toStdString());in.imbue(std::locale::classic());double n{};if(!(in>>n)||!std::isfinite(n)||n<0||n>static_cast<double>(mrs::max_sample)/192000)throw std::runtime_error("Enter valid trim seconds");in>>std::ws;if(!in.eof())throw std::runtime_error("Unexpected trim characters");return n;};owner.app.trim_clip(id,static_cast<mrs::Sample>(std::llround(parse(start)*p->sample_rate)),static_cast<mrs::Sample>(std::llround(parse(end)*p->sample_rate)));reload();});};setSize(1000,600);reload();startTimer(150);}
    bool keyPressed(const juce::KeyPress& key) override{return owner.workspaceShortcut(key);}
    ~AudioClipPanel() override{setLookAndFeel(nullptr);}
    void resized() override{attach.setBounds(getWidth()-130,8,114,26);attach.setButtonText(static_cast<bool>(getProperties()["mrs-attached"])?"Detach":"Attach");wave.setBounds(16,44,getWidth()-32,getHeight()-132);start.setBounds(16,getHeight()-76,150,26);end.setBounds(174,getHeight()-76,150,26);apply.setBounds(332,getHeight()-76,160,26);hint.setBounds(16,getHeight()-40,getWidth()-32,28);}
    void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(background));g.setColour(juce::Colours::white);g.drawText("Audio editor - "+juce::String::fromUTF8(clip.name.c_str()),16,8,getWidth()-160,26,juce::Justification::left);}
private:
    struct Wave final:juce::Component {AudioClipPanel& owner;explicit Wave(AudioClipPanel& p):owner(p){}void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(surface));if(const auto* waveform=owner.owner.app.waveform(owner.clip.source)){g.setColour(juce::Colours::skyblue);drawWaveform(g,*waveform,getLocalBounds().toFloat(),static_cast<double>(owner.clip.source_offset),static_cast<double>(owner.clip.length)/juce::jmax(1,getWidth()));}else{g.setColour(juce::Colours::lightgrey);g.drawText("Waveform unavailable",getLocalBounds(),juce::Justification::centred);}}};
    Desktop& owner;mrs::Id id;mrs::Clip clip;Wave wave;juce::TextButton attach{"Attach"},apply{"Apply trim"};juce::TextEditor start,end;juce::Label hint;
    void reload(){const auto p=owner.project();const auto it=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});if(it==p->clips.end()||it->midi){setEnabled(false);return;}clip=*it;start.setText(juce::String(static_cast<double>(clip.start)/p->sample_rate,6));end.setText(juce::String(static_cast<double>(clip.start+clip.length)/p->sample_rate,6));hint.setText("Start / end in project seconds. Apply trim: one Undo. Ctrl+Z/Y in main window.",juce::dontSendNotification);wave.repaint();}
    void timerCallback() override{const auto p=owner.project();const auto it=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});if(it==p->clips.end()){setEnabled(false);return;}if(it->start!=clip.start||it->length!=clip.length||it->source_offset!=clip.source_offset)reload();wave.repaint();}
};
void Desktop::openMidiClip(mrs::Id id){openClipEditor(std::move(id));}
bool Desktop::clipEditorOpen() const{if(dockedEditor&&dockedClip&&dockedEditor->isVisible()&&!static_cast<bool>(dockedEditor->getProperties()["mrs-ara-editor"]))return true;for(const auto& w:windows)if(w->isVisible()&&static_cast<bool>(w->getProperties()["mrs-midi-id"].isString())&&!w->getProperties()["mrs-midi-id"].toString().isEmpty())return true;return false;}
void Desktop::activateClip(mrs::Id id){arrangement->chords->clearSelection();arrangement->markers->clearSelection();selectedClip=id;selectedClips={id.value};const bool opened=clipEditorOpen();if(opened)openClipEditor(id);refresh();}
void Desktop::toggleEditor(){if(dockedEditor&&dockedClip&&static_cast<bool>(dockedEditor->getProperties()["mrs-ara-editor"])) {setWorkspace(dockedEditor->isVisible()?mrs::desktop::Workspace::arrange:mrs::desktop::Workspace::edit);return;}if(clipEditorOpen()){if(dockedEditor)dockedEditor->setVisible(false);for(auto& w:windows)if(!w->getProperties()["mrs-midi-id"].toString().isEmpty())w->closeButtonPressed();setWorkspace(mrs::desktop::Workspace::arrange);}else openSelectedEditor();}
void Desktop::openClipEditor(mrs::Id id){
    arrangement->chords->clearSelection();arrangement->markers->clearSelection();
    const auto p=project();const auto clip=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});if(clip==p->clips.end())throw std::runtime_error("Select an audio or MIDI clip");selectedClip=id;selectedTrack=clip->track;
    const auto create=[&]()->juce::Component*{return clip->midi?static_cast<juce::Component*>(new MidiClipPanel(*this,id)):static_cast<juce::Component*>(new AudioClipPanel(*this,id));};
    const bool fullscreenEditor=std::any_of(windows.begin(),windows.end(),[](const auto& w){return w->isVisible()&&w->isFullScreen()&&!w->getProperties()["mrs-midi-id"].toString().isEmpty();});
    if(view.editorAttached&&!fullscreenEditor){if(auto* midiPanel=dynamic_cast<MidiClipPanel*>(dockedEditor.get());midiPanel&&clip->midi)midiPanel->activate(id);else if(dockedClip!=id||!dockedEditor)dockedEditor.reset(create());dockedClip=id;dockedEditor->getProperties().set("mrs-attached",true);addAndMakeVisible(dockedEditor.get());setWorkspace(mrs::desktop::Workspace::edit);return;}
    for(auto& w:windows)if(w->isVisible()&&!w->getProperties()["mrs-midi-id"].toString().isEmpty()){if(auto* midiPanel=dynamic_cast<MidiClipPanel*>(w->getContentComponent());midiPanel&&clip->midi)midiPanel->activate(id);else w->setContentOwned(create(),true);w->getProperties().set("mrs-midi-id",juce::String::fromUTF8(id.value.c_str()));w->setName(clip->midi?"Piano roll":"Audio editor");w->toFront(true);setWorkspace(mrs::desktop::Workspace::edit);return;}
    auto window=std::make_unique<EditorWindow>(clip->midi?"Piano roll":"Audio editor",create());window->setResizable(true,false);window->setResizeLimits(720,550,1800,1400);window->getProperties().set("mrs-midi-clip",true);window->getProperties().set("mrs-midi-id",juce::String::fromUTF8(id.value.c_str()));window->setOwner(*this);window->onClose=[this]{setWorkspace(mrs::desktop::Workspace::arrange);};windows.push_back(std::move(window));setWorkspace(mrs::desktop::Workspace::edit);
}
void Desktop::openSelectedEditor(){const auto p=project();if(!selectedClip){const auto head=app.engine()->state().sample;const mrs::Timeline time(p->time,p->sample_rate);for(const auto& c:p->clips)if((!selectedTrack||c.track==*selectedTrack)&&mrs::clip_start(c,time)<=head&&mrs::clip_end(c,time)>head){selectedClip=c.id;break;}}if(!selectedClip)throw std::runtime_error("Select an audio or MIDI clip, then press Edit");setWorkspace(mrs::desktop::Workspace::edit);openClipEditor(*selectedClip);}
void Desktop::attachClipEditor(juce::Component* content,mrs::Id id,bool attach,bool persist){
    view.editorAttached=attach;if(persist)saveSettings();
    if(attach){for(auto& w:windows)if(w->getContentComponent()==content){w->setContentNonOwned(content,false);w->clearContentComponent();w->setVisible(false);dockedEditor.reset(content);dockedClip=id;content->getProperties().set("mrs-attached",true);addAndMakeVisible(content);setWorkspace(mrs::desktop::Workspace::edit);return;}}
    else if(dockedEditor.get()==content){auto* editorContent=dockedEditor.release();dockedClip.reset();removeChildComponent(editorContent);editorContent->getProperties().set("mrs-attached",false);editorContent->setSize(1000,800);auto window=std::make_unique<EditorWindow>(editorContent->getName(),editorContent);window->setResizable(true,false);window->setResizeLimits(720,550,1800,1400);window->getProperties().set("mrs-midi-clip",true);window->getProperties().set("mrs-midi-id",juce::String::fromUTF8(id.value.c_str()));window->setOwner(*this);window->onClose=[this]{setWorkspace(mrs::desktop::Workspace::arrange);};windows.push_back(std::move(window));setWorkspace(mrs::desktop::Workspace::edit);}
}
}
