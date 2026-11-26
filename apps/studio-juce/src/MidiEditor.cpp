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
        velocityHeight=owner.view.midiVelocityHeight;laneHeight=owner.view.midiControllerHeight;
        for(auto* b:{&drumMode,&drumMap,&expand,&collapse})b->setVisible(false);
        addAndMakeVisible(controllerLane);controllerLane.setComponentID("embedded-controllers");controllerLane.setEmbedded();controllerLane.setVisible(owner.view.controllersOpen);
        viewport.changed=[this]{alignControllers();velocity.repaint();};roll.viewChanged=[this]{
            if(roll.drums && viewport.getWidth()>0 && lastRollExtent!=juce::Point<int>{roll.getWidth(),roll.getHeight()})
                layoutEditorPanes(viewport.getX(),viewport.getWidth());
            alignControllers();velocity.repaint();
        };
        roll.followPosition=[this](float x){followPlayback(x);};
#if defined(MRS_DRUM_MAP_UI)
        if(!owner.standaloneLive){
            for(auto* b:{&drumMode,&drumMap,&expand,&collapse})sideBody.addAndMakeVisible(*b);
            drumMode.setComponentID("midi-view-mode");drumMap.setComponentID("midi-display-map");
            drumMode.onClick=[this]{owner.run([this]{setDrumMode(!roll.drums);});};drumMap.onClick=[this]{chooseMap();};
            expand.onClick=[this]{foldAll(false);};collapse.onClick=[this]{foldAll(true);};
            roll.toggleDrumGroup=[this](const auto& group){updateFold([&](auto& binding){const auto at=std::find(binding.collapsed.begin(),binding.collapsed.end(),group);if(at==binding.collapsed.end())binding.collapsed.push_back(group);else binding.collapsed.erase(at);});};
            addAndMakeVisible(velocity);velocity.setVisible(false);
        }
#endif
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
        controllerLane.toFront(false);addAndMakeVisible(divider);addAndMakeVisible(blockDivider);
        for(std::size_t i=0;i<quickCC.size();++i){auto& b=quickCC[i];const int cc=ccNumbers[i];divider.addAndMakeVisible(b);b.setButtonText("CC"+juce::String(cc));b.setTitle("Show controller "+juce::String(cc)+" lane");b.setComponentID("controller-cc"+juce::String(cc));b.getProperties().set("channelFont",true);b.onClick=[this,cc]{roll.cancel();controllerLane.selectCC(cc);};}
        controllerLane.laneChanged=[this]{updateQuickCC();};updateQuickCC();
        addAndMakeVisible(noteGrid);noteGrid.setState(noteGridFromView(),gridSnapEnabled());roll.setNoteGrid(noteGrid.spec(),noteGrid.snapEnabled());noteGrid.onClick=[this]{chooseNoteGrid();};
        for(auto* b:{&zoomIn,&zoomOut,&copy,&paste,&selectAll,&musical,&controllers,&attach,&details,&fullScreen})addAndMakeVisible(b);
        zoomIn.onClick=[this]{zoom(1.25f);};zoomOut.onClick=[this]{zoom(.8f);};copy.onClick=[this]{roll.copy();};paste.onClick=[this]{roll.paste();};selectAll.onClick=[this]{if(roll.drums)roll.selectVisible();else{for(const auto& n:roll.notes)roll.selected.insert(n.id.value);roll.repaint();}};
        musical.setComponentID("musical-edit-open");musical.onClick=[this]{roll.cancel();auto* musicalPanel=new MusicalEditPanel(owner.theme,[safe=juce::Component::SafePointer<MidiClipPanel>(this)](bool all,const mrs::NoteEdit& edit){if(!safe||!safe->isShowing())throw std::runtime_error("Piano roll is closed");safe->roll.cancel();safe->roll.sync();const auto p=safe->owner.project();const auto& clip=*safe->current(*p).midi;std::vector<mrs::Id> selected;for(const auto& n:clip.notes)if((!safe->roll.drums||safe->roll.noteVisible(n))&&(all?(n.start<clip.source_offset+clip.length&&n.start+n.length>clip.source_offset):safe->roll.selected.contains(n.id.value)))selected.push_back(n.id);if(selected.empty())throw std::runtime_error("Select visible notes or choose Whole clip");if(safe->roll.drums&&edit.kind==mrs::NoteEditKind::transpose){const auto next=mrs::transform_midi_notes(clip,selected,edit);std::set<std::string> targets;for(const auto& note:selected)targets.insert(note.value);for(const auto& n:next)if(targets.contains(n.id.value)&&!safe->roll.drumAddressVisible(n))throw std::runtime_error("Show the destination drum rows before transposing");}safe->owner.app.edit_midi_notes(safe->id,std::move(selected),edit);safe->owner.refresh();safe->roll.sync();safe->reload();});auto window=std::make_unique<EditorWindow>("Musical note edit",musicalPanel);window->getProperties().set("mrs-midi-clip",true);window->setOwner(owner);owner.windows.push_back(std::move(window));};
        controllers.setComponentID("controller-edit-open");controllers.onClick=[this]{roll.cancel();controllerLane.cancelGesture();controllerLane.setVisible(!controllerLane.isVisible());owner.view.controllersOpen=controllerLane.isVisible();owner.saveSettings();resized();};
        fullScreen.setComponentID("editor-fullscreen");fullScreen.setTitle("Toggle MIDI editor full screen (F11)");fullScreen.onClick=[this]{deferFullScreen();};attach.setComponentID("editor-attach");attach.onClick=[this]{deferAttach();};details.onClick=[this]{roll.cancel();controllerLane.cancelGesture();showDetails=!showDetails;resized();};
        roll.error=[this](auto text){hint.setText(text,juce::dontSendNotification);};roll.changed=[this]{reload();};
        for(auto* b:{&pointer,&split}){addAndMakeVisible(b);b->getProperties().set("mrSmallToolbarIcon",true);}
        pointer.getProperties().set("mrHostIcon","tool_pointer");split.getProperties().set("mrHostIcon","tool_split");
        pointer.setComponentID("piano-pointer");split.setComponentID("piano-split");pointer.setTitle("Pointer (1)");split.setTitle("Split notes (3)");pointer.setTooltip(pointer.getTitle());split.setTooltip("Split notes (3); S splits selected notes at the cursor");
        pointer.onClick=[this]{roll.setSplitTool(false);roll.grabKeyboardFocus();};split.onClick=[this]{roll.setSplitTool(true);roll.grabKeyboardFocus();};
        roll.toolChanged=[this]{pointer.setToggleState(!roll.splitTool,juce::dontSendNotification);split.setToggleState(roll.splitTool,juce::dontSendNotification);};roll.toolChanged();
        addAndMakeVisible(noteColors);noteColors.setComponentID("piano-note-colors");noteColors.setTitle("Note color mode");noteColors.setTooltip("Velocity, clip or drum group color");noteColors.addItem("Velocity colors",1);noteColors.addItem("Clip color",2);noteColors.addItem("Instrument color",3);noteColors.setSelectedId(owner.view.noteInstrumentColors?3:owner.view.noteClipColors?2:1,juce::dontSendNotification);
        noteColors.onChange=[this]{roll.cancel();owner.view.noteClipColors=noteColors.getSelectedId()==2;owner.view.noteInstrumentColors=noteColors.getSelectedId()==3;owner.saveSettings();roll.sync();};
        addAndMakeVisible(selector);selector.setTitle("Select MIDI note");selector.onChange=[this]{loadNote();const int index=selector.getSelectedId()-1;if(index>=0&&index<static_cast<int>(notes.size())){roll.selected={notes[static_cast<std::size_t>(index)].id.value};roll.repaint();}};
        const std::array<juce::String,5> names{"Pitch (0..127)","Start (beats)","Length (beats)","Velocity (1..127)","Channel (1..16)"};
        for(std::size_t i=0;i<fields.size();++i){labels[i].setText(names[i],juce::dontSendNotification);labels[i].setFont(owner.theme.font(13));addAndMakeVisible(labels[i]);fields[i].setFont(owner.theme.font(13));fields[i].setTitle(names[i]);fields[i].setComponentID("midi-note-"+juce::String(static_cast<int>(i)));addAndMakeVisible(fields[i]);}
        for(auto* b:{&add,&apply,&remove})addAndMakeVisible(*b);
        add.setComponentID("midi-note-add");apply.setComponentID("midi-note-apply");remove.setComponentID("midi-note-delete");
        add.onClick=[this]{edit(0);};apply.onClick=[this]{edit(1);};remove.onClick=[this]{edit(2);};
        hint.setFont(Theme::channelFont());hint.setJustificationType(juce::Justification::topLeft);addAndMakeVisible(hint);
        for(auto* c:std::array<juce::Component*,14>{&zoomIn,&zoomOut,&copy,&paste,&selectAll,&musical,&controllers,&attach,&details,&fullScreen,&selector,&add,&apply,&remove}){sideBody.addAndMakeVisible(c);c->getProperties().set("channelFont",true);}
        for(auto& l:labels){sideBody.addAndMakeVisible(l);l.setFont(Theme::channelFont());}
        for(auto& f:fields){sideBody.addAndMakeVisible(f);f.setFont(Theme::channelFont());}
        sideBody.addAndMakeVisible(hint);sideBody.addAndMakeVisible(controllerLane.controlsContent());
        details.setButtonText("Notes");details.setTitle("Show note details");
        const std::array<juce::String,5> compactNames{"Pitch","Start beat","Length","Velocity","Channel"};for(std::size_t i=0;i<labels.size();++i)labels[i].setText(compactNames[i],juce::dontSendNotification);
        for(auto* b:{&gridSnap,&playheadMode}){addAndMakeVisible(b);b->setWantsKeyboardFocus(false);}
        gridSnap.setComponentID("midi-grid-snap");gridSnap.setTitle("Snap grid");
        gridSnap.getProperties().set("mrHostIcon","action_snap");gridSnap.getProperties().set("mrSmallToolbarIcon",true);
        gridSnap.onClick=[this]{applyNoteGrid(noteGrid.spec(),!noteGrid.snapEnabled());};
        playheadMode.setComponentID("midi-playhead-mode");playheadMode.setTitle("Playback stop mode");
        playheadMode.onClick=[this]{
            owner.view.midiResumePlayback=!owner.view.midiResumePlayback;
            syncToolbarPreferences();owner.syncMidiEditorTools();owner.saveSettings();
            if(roll.isShowing())roll.grabKeyboardFocus();
        };
        if(!owner.standaloneLive)setupQuantizePanel();
        syncToolbarPreferences();
        addAndMakeVisible(follow);follow.setComponentID("midi-follow-playhead");follow.setClickingTogglesState(true);
        follow.setToggleState(owner.view.midiFollowPlayhead,juce::dontSendNotification);
        follow.onClick=[this]{
            owner.view.midiFollowPlayhead=follow.getToggleState();roll.refreshPlayhead();
            juce::Component::SafePointer<Desktop> safe(&owner);
            juce::MessageManager::callAsync([safe]{if(safe)safe->saveSettings();});
        };
        reload();setSize(1000,800);
        restoreDrumMode();
    }
    bool keyPressed(const juce::KeyPress& key) override{if(owner.workspaceShortcut(key))return true;if(key.getKeyCode()==juce::KeyPress::F11Key){deferFullScreen();return true;}for(auto* c=juce::Component::getCurrentlyFocusedComponent();c&&c!=this;c=c->getParentComponent())if(dynamic_cast<juce::TextEditor*>(c)||dynamic_cast<juce::ComboBox*>(c))return false;return roll.keyPressed(key);}
    bool keyStateChanged(bool isKeyDown) override{
        roll.releaseQuantizeKey();
        if(!juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey)){owner.spaceKey(false);return false;}
        for(auto* c=juce::Component::getCurrentlyFocusedComponent();c&&c!=this;c=c->getParentComponent())
            if(dynamic_cast<juce::TextEditor*>(c)||dynamic_cast<juce::ComboBox*>(c))return false;
        return roll.keyStateChanged(isKeyDown);
    }
    void activate(mrs::Id next){id=next;roll.activate(next);controllerLane.activate(next);showDetails=false;controllerLane.setVisible(owner.view.controllersOpen);restoreDrumMode();syncToolbarPreferences();reload();resized();}
    ~MidiClipPanel() override{controllerLane.laneChanged=nullptr;sideViewport.setViewedComponent(nullptr,false);viewport.setViewedComponent(nullptr,false);setLookAndFeel(nullptr);}
    void resized() override{
        const int sideWidth=juce::jlimit(156,196,getWidth()/4);
        sideViewport.setBounds(0,0,sideWidth,getHeight());
        const int width=sideWidth-14,w=width-12,half=(w-4)/2;
        const int drumTools=drumMode.isVisible()?(roll.drums?84:28):0;
        drumMode.setBounds(6,6,w,24);
        drumMap.setVisible(roll.drums);expand.setVisible(roll.drums);collapse.setVisible(roll.drums);
        drumMap.setBounds(6,34,w,24);expand.setBounds(6,62,half,24);collapse.setBounds(10+half,62,half,24);
        zoomIn.setBounds(6,6+drumTools,half,24);zoomOut.setBounds(10+half,6+drumTools,half,24);
        copy.setBounds(6,34+drumTools,half,24);paste.setBounds(10+half,34+drumTools,half,24);
        selectAll.setBounds(6,62+drumTools,w,24);musical.setBounds(6,90+drumTools,w,24);
        controllers.setBounds(6,118+drumTools,half,24);details.setBounds(10+half,118+drumTools,half,24);
        attach.setBounds(6,146+drumTools,half,24);fullScreen.setBounds(10+half,146+drumTools,half,24);
        fullScreen.setButtonText(fullScreenActive?"Restore":"Full screen");
        attach.setButtonText(static_cast<bool>(getProperties()["mrs-attached"])?"Detach":"Attach");
        controllers.setToggleState(controllerLane.isVisible(),juce::dontSendNotification);
        details.setToggleState(showDetails,juce::dontSendNotification);
        selector.setVisible(showDetails);for(auto& l:labels)l.setVisible(showDetails);
        for(auto& f:fields)f.setVisible(showDetails);
        for(auto* b:{&add,&apply,&remove})b->setVisible(showDetails);hint.setVisible(showDetails);
        int y=178+drumTools;selector.setBounds(6,y,w,24);y+=28;
        for(std::size_t i=0;i<fields.size();++i){labels[i].setBounds(6,y,76,24);fields[i].setBounds(82,y,w-76,24);y+=28;}
        add.setBounds(6,y,w,24);apply.setBounds(6,y+28,half,24);remove.setBounds(10+half,y+28,half,24);
        hint.setBounds(6,y+56,w,58);hint.setTooltip(hint.getText());
        if(!showDetails)y=178+drumTools;else y+=118;
        auto& form=controllerLane.controlsContent();form.setVisible(controllerLane.isVisible());
        controllerLane.layoutControls(width);form.setTopLeftPosition(0,y);
        sideBody.setSize(width,juce::jmax(getHeight(),y+(form.isVisible()?form.getHeight():0)));
        pointer.setBounds(sideWidth+4,2,32,28);split.setBounds(sideWidth+38,2,32,28);
        const int toolbarLeft=sideWidth+74;
        auto toolbarArea=juce::Rectangle<int>{toolbarLeft,2,std::max(0,getWidth()-toolbarLeft-8),28};
        const int available=toolbarArea.getWidth();
        const auto take=[&](int requested){auto cell=toolbarArea.removeFromLeft(std::min(requested,toolbarArea.getWidth()));toolbarArea.removeFromLeft(std::min(4,toolbarArea.getWidth()));return cell;};
        if(!owner.standaloneLive)quantizeButton.setBounds(take(32));
        gridSnap.setBounds(take(32));
        noteGrid.setBounds(take(juce::jlimit(72,104,available-300)));
        playheadMode.setBounds(take(juce::jlimit(70,94,available-270)));
        const int followWidth=juce::jlimit(62,118,available-320);
        noteColors.setBounds(take(std::max(0,std::min(156,toolbarArea.getWidth()-followWidth-4))).reduced(0,2));
        follow.setBounds(toolbarArea.removeFromLeft(std::min(followWidth,toolbarArea.getWidth())).reduced(0,2));
        follow.setButtonText(follow.getWidth()<100?"Follow":"Follow playhead");
        layoutQuantizePanel(sideWidth+4,juce::jmax(90,getWidth()-sideWidth-12));
        layoutEditorPanes(sideWidth+4,juce::jmax(90,getWidth()-sideWidth-12));alignControllers();
        if(firstLayout){firstLayout=false;viewport.setViewPosition(static_cast<int>(static_cast<double>(roll.clip.start)/mrs::ppq*roll.beatWidth),PianoRoll::ruler+(127-78)*static_cast<int>(roll.rowHeight));}
    }
    void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(background));}
private:
    friend void controllerScrollSmoke();
    friend void noteEditorLayoutSmoke();
    friend void pianoRoll214Smoke();
    friend void drumGridSmoke();
    friend void noteGridChecks(MidiClipPanel&,Desktop&);
    friend void noteGridSmoke();
    friend class Desktop;
    Desktop& owner;mrs::Id id;
    juce::TextButton drumMode{"Piano Roll / Drums"},drumMap{"Display map..."},expand{"Expand all"},collapse{"Collapse all"};
    int pianoY{},drumsY{};
    static int followedScrollX(int x,int left,int span,int maximum){
        return span>0 && (x<left || x>=left+span) ? std::clamp(x-span/8,0,maximum) : left;
    }
    void followPlayback(float timelineX){
        if(!follow.getToggleState() || !isShowing() || !roll.isEnabled() || velocity.grabbed)return;
        mrs::audio::RealtimeState state;
        if(!owner.app.engine()->try_state(state) || state.playback!=mrs::PlaybackState::playing)return;
        const auto visible=viewport.getViewArea();
        const int x=static_cast<int>(std::clamp(timelineX,0.f,static_cast<float>(roll.getWidth())));
        const auto next=followedScrollX(x,visible.getX(),visible.getWidth()-roll.railWidth(),std::max(0,roll.getWidth()-visible.getWidth()));
        if(next!=visible.getX())viewport.setViewPosition(next,visible.getY());
    }
    void restoreDrumMode(){
        if(!drumMode.isVisible())return;
        const auto p=owner.project();
        const auto clip=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id&&c.midi.has_value();});
        if(clip==p->clips.end())return;
        const auto track=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==clip->track;});
        if(track!=p->tracks.end()&&track->midi_drum_mode!=roll.drums)setDrumMode(track->midi_drum_mode,false);
    }
    void setDrumMode(bool active,bool persist=true){
        if(roll.drums==active)return;
        (roll.drums?drumsY:pianoY)=viewport.getViewPositionY();
        const auto x=viewport.getViewPositionX();
        roll.cancel();roll.sync();if(!roll.isEnabled())return;
        controllerLane.cancelGesture();
        if(persist)owner.app.set_track_midi_drum_mode(roll.trackId(),active);
        roll.setDrums(active);setName(active?"Drum Grid":"Piano roll");
        drumMode.setToggleState(active,juce::dontSendNotification);drumMode.setButtonText(active?"Drum Grid / Piano":"Piano Roll / Drums");
        velocity.setVisible(active);reload();resized();viewport.setViewPosition(x,active?drumsY:pianoY);
        if(persist&&roll.isShowing())roll.grabKeyboardFocus();
    }
    const mrs::Track* currentTrack(const mrs::Project& project) const{
        const auto at=std::find_if(project.tracks.begin(),project.tracks.end(),[&](const auto& t){return t.id==roll.trackId();});
        return at==project.tracks.end()?nullptr:&*at;
    }
    template<class Edit> void updateFold(Edit edit){try{
        roll.cancel();roll.sync();if(!roll.isEnabled())return;const auto p=owner.project();const auto* track=currentTrack(*p);if(!track||!track->drum_map)return;
        auto binding=*track->drum_map;edit(binding);
        owner.app.set_track_drum_map(track->id,std::move(binding));owner.refresh();roll.sync();reload();
    }catch(const std::exception& e){hint.setText(juce::String::fromUTF8(e.what()),juce::dontSendNotification);}}
    void foldAll(bool folded){updateFold([folded](auto& b){b.collapsed.clear();if(folded)for(const auto& c:b.snapshot.categories){b.collapsed.push_back(c.id);for(const auto& i:c.instruments)b.collapsed.push_back(i.id);}});}
    bool gridSnapEnabled() const{return owner.standaloneLive?owner.view.midiGridSnap:owner.app.midi_quantize_preferences().snap;}
    NoteGridSpec noteGridFromView() const{
        if(owner.standaloneLive)return canonicalNoteGrid({owner.view.midiGridDivisor,owner.view.midiGridSwing,static_cast<NoteGridSpec::Shape>(owner.view.midiGridShape)});
        const auto p=owner.app.midi_quantize_preferences();
        return canonicalNoteGrid({p.divisor,p.swing,static_cast<NoteGridSpec::Shape>(p.shape)});
    }
    void setupQuantizePanel(){
        addAndMakeVisible(quantizeButton);quantizeButton.setComponentID("midi-quantize-toggle");
        quantizeButton.setTitle("Quantize settings");quantizeButton.setTooltip("Show quantize settings (keyboard Q applies to selected notes)");
        quantizeButton.setWantsKeyboardFocus(false);
        quantizeButton.onClick=[this]{owner.view.midiQuantizeOpen=!owner.view.midiQuantizeOpen;owner.saveSettings();syncToolbarPreferences();owner.syncMidiEditorTools();if(roll.isShowing())roll.grabKeyboardFocus();};
        addChildComponent(quantizePanel);
        for(std::size_t i=0;i<quantizeDivisors.size();++i){
            auto& b=quantizeDivisors[i];quantizePanel.addAndMakeVisible(b);const int divisor=1<<i;
            b.setButtonText("1/"+juce::String(divisor));b.setWantsKeyboardFocus(false);
            b.onClick=[this,divisor]{auto spec=noteGridFromView();spec.divisor=divisor;applyNoteGrid(spec,gridSnapEnabled());};
        }
        const std::array<juce::String,4> shapes{"Straight","Triplet","Quintuplet","Septuplet"};
        for(std::size_t i=0;i<quantizeShapes.size();++i){
            auto& b=quantizeShapes[i];quantizePanel.addAndMakeVisible(b);b.setButtonText(shapes[i]);b.setWantsKeyboardFocus(false);
            b.onClick=[this,i]{auto spec=noteGridFromView();if(i==0&&spec.shape==NoteGridSpec::Shape::swing)return;spec.shape=static_cast<NoteGridSpec::Shape>(i);spec.swing=0;applyNoteGrid(spec,gridSnapEnabled());};
        }
        const std::array<juce::String,5> names{"Swing","Start","End","Range","Velocity"};
        for(std::size_t i=0;i<quantizeSliders.size();++i){
            auto& s=quantizeSliders[i];auto& label=quantizeLabels[i];
            quantizePanel.addAndMakeVisible(label);label.setText(names[i],juce::dontSendNotification);label.setFont(Theme::channelFont());
            quantizePanel.addAndMakeVisible(s);s.setName(names[i]);s.setSliderStyle(juce::Slider::LinearHorizontal);s.setRange(0,100,1);
            s.setTextBoxStyle(juce::Slider::TextBoxRight,false,44,20);s.setTextValueSuffix("%");
            s.onValueChange=[this,i]{
                if(i==4)return;
                auto p=owner.app.midi_quantize_preferences();const auto value=static_cast<int>(quantizeSliders[i].getValue());
                if(i==0){p.swing=value;p.shape=value?4:0;}else if(i==1)p.start=value;else if(i==2)p.end=value;else p.range=value;
                owner.app.set_midi_quantize_preferences(p);syncToolbarPreferences();owner.syncMidiEditorTools();
            };
        }
        quantizeSliders[4].setEnabled(false);quantizeLabels[4].setEnabled(false);
        quantizeSliders[4].setTooltip("Velocity is available with Groove Quantize");
    }
    void syncQuantizePanel(){
        if(owner.standaloneLive)return;
        const auto p=owner.app.midi_quantize_preferences();
        const bool visible=owner.view.midiQuantizeOpen,changed=quantizePanel.isVisible()!=visible;
        quantizePanel.setVisible(visible);quantizeButton.setToggleState(visible,juce::dontSendNotification);
        for(std::size_t i=0;i<quantizeDivisors.size();++i)quantizeDivisors[i].setToggleState(p.divisor==(1<<i),juce::dontSendNotification);
        for(std::size_t i=0;i<quantizeShapes.size();++i)quantizeShapes[i].setToggleState((p.shape==4?0:p.shape)==static_cast<int>(i),juce::dontSendNotification);
        const std::array<int,5> values{p.swing,p.start,p.end,p.range,0};
        for(std::size_t i=0;i<values.size();++i)quantizeSliders[i].setValue(values[i],juce::dontSendNotification);
        quantizeSliders[0].setEnabled(p.shape==0||p.shape==4);
        if(changed&&getWidth()>0)resized();
    }
    void layoutQuantizePanel(int x,int width){
        if(owner.standaloneLive||!quantizePanel.isVisible())return;
        quantizePanel.setBounds(x,rollTop,width,quantizePanelHeight);
        auto area=quantizePanel.getLocalBounds().reduced(2,1);
        auto distribute=[](auto& controls,juce::Rectangle<int> row){for(std::size_t i=0;i<controls.size();++i)controls[i].setBounds(row.removeFromLeft(row.getWidth()/static_cast<int>(controls.size()-i)).reduced(1,1));};
        distribute(quantizeDivisors,area.removeFromTop(24));
        auto second=area.removeFromTop(26);auto rhythm=second.removeFromLeft(second.getWidth()*3/4);
        distribute(quantizeShapes,rhythm);quantizeLabels[0].setBounds(second.removeFromLeft(40));quantizeSliders[0].setBounds(second.reduced(1));
        auto third=area.removeFromTop(26);
        for(std::size_t i=1;i<quantizeSliders.size();++i){auto cell=third.removeFromLeft(third.getWidth()/static_cast<int>(quantizeSliders.size()-i)).reduced(1);quantizeLabels[i].setBounds(cell.removeFromLeft(i==4?46:34));quantizeSliders[i].setBounds(cell);}
    }
    void chooseNoteGrid(){
        roll.cancel();controllerLane.cancelGesture();
        juce::Component::SafePointer<MidiClipPanel> safe(this);
        showNoteGridMenu(noteGrid,noteGrid.spec(),noteGrid.snapEnabled(),[safe](const NoteGridSpec& spec,bool snap){
            if(safe)safe->applyNoteGrid(spec,snap);
        });
    }
    // One project grid for both Piano Roll and Drum Grid; Live retains its prior preference path.
    void syncToolbarPreferences(){
        const auto spec=noteGridFromView();const bool enabled=gridSnapEnabled();
        if(roll.gridSpec!=spec||roll.snapGrid!=enabled){
            controllerLane.cancelGesture();velocity.cancel();roll.setNoteGrid(spec,enabled);
        }
        noteGrid.setState(spec,enabled);
        gridSnap.setToggleState(enabled,juce::dontSendNotification);
        gridSnap.setTooltip(enabled?"Snap enabled":"Snap disabled");
        const bool resume=owner.view.midiResumePlayback;
        playheadMode.setToggleState(resume,juce::dontSendNotification);
        playheadMode.setButtonText(resume?"Resume":"Return");
        playheadMode.setTooltip(resume?"Resume from the last stop position (click to return to start)":"Return to the last playback start (click to resume)");
        syncQuantizePanel();
    }
    void applyNoteGrid(const NoteGridSpec& spec,bool snap){
        const auto canonical=canonicalNoteGrid(spec);
        if(owner.standaloneLive){
            owner.view.midiGridDivisor=canonical.divisor;owner.view.midiGridShape=static_cast<int>(canonical.shape);
            owner.view.midiGridSwing=canonical.swing;owner.view.midiGridSnap=snap;owner.saveSettings();
        }else{
            auto p=owner.app.midi_quantize_preferences();p.divisor=canonical.divisor;p.shape=static_cast<int>(canonical.shape);p.swing=canonical.swing;p.snap=snap;
            owner.app.set_midi_quantize_preferences(p);
        }
        syncToolbarPreferences();owner.syncMidiEditorTools();
        if(roll.isShowing())roll.grabKeyboardFocus();
    }
#if defined(MRS_DRUM_MAP_UI)
    void chooseMap(){try{
        roll.cancel();roll.sync();if(!roll.isEnabled())return;const auto p=owner.project();const auto* track=currentTrack(*p);
        if(!track||track->kind!=mrs::TrackKind::instrument)throw std::runtime_error("Drum maps require an instrument track");
        const auto root=owner.prefsFile.parent_path()/"DrumMaps";const auto catalog=mrs::persistence::list_drum_maps(root);
        juce::PopupMenu menu;menu.setLookAndFeel(&owner.theme);
        menu.addItem(9999,"Open Drum Maps…");
        menu.addItem(1,"No display map",track->drum_map.has_value());
        if(track->drum_map)menu.addItem(2,"Project snapshot: "+juce::String::fromUTF8(track->drum_map->snapshot.name.c_str()),false,true);
        std::vector<mrs::Id> catalogIds;
        for(const auto& entry:catalog.entries){catalogIds.push_back(entry.id);menu.addItem(2+static_cast<int>(catalogIds.size()),juce::String::fromUTF8(entry.name.c_str()));}
        if(catalogIds.empty())menu.addItem(3,"No saved maps. Use Options > Drum Maps.",false);
        auto safe=juce::Component::SafePointer<MidiClipPanel>(this);const auto store=owner.app.services().projects;
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&drumMap),[safe,store,trackId=track->id,root,catalogIds=std::move(catalogIds)](int choice){
            if(!safe||!choice||!safe->roll.drums||safe->roll.trackId()!=trackId||safe->owner.app.services().projects!=store)return;
            try{
                if (choice == 9999) { safe->owner.openDrumMaps(trackId); return; }
                std::optional<mrs::DrumMapBinding> binding;
                if(choice>=3&&static_cast<std::size_t>(choice-3)<catalogIds.size()){
                    auto profile=mrs::persistence::load_drum_map(root,catalogIds[static_cast<std::size_t>(choice-3)]);
                    if(!profile)throw std::runtime_error("The map no longer exists");
                    const auto pNow=safe->owner.project();const auto* t=safe->currentTrack(*pNow);
                    if(t&&t->drum_map&&t->drum_map->snapshot==*profile)return;
                    binding=mrs::DrumMapBinding{std::move(*profile),{}};
                }else if(choice!=1)return;
                safe->owner.app.set_track_drum_map(trackId,std::move(binding));safe->owner.refresh();safe->roll.sync();safe->reload();
            }catch(const std::exception& e){juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Drum Grid",juce::String::fromUTF8(e.what()));}
        });
    }catch(const std::exception& e){juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Drum Grid",juce::String::fromUTF8(e.what()));}}
#endif
    struct VelocityPane final:juce::Component {
        MidiClipPanel& panel;
        std::optional<mrs::MidiNote> grabbed;
        std::shared_ptr<mrs::IProjectStore> store;
        mrs::Id clipId;
        int preview{}, delta{};
        double lastAudition{};
        std::unordered_map<std::string,int> originalVelocity;
        explicit VelocityPane(MidiClipPanel& p):panel(p){setTitle("Drum velocity");setComponentID("drum-velocity");}
        void paint(juce::Graphics& g) override{
            g.fillAll(juce::Colour(surface));g.setColour(juce::Colour(outline));g.drawHorizontalLine(0,0.f,static_cast<float>(getWidth()));
            g.setFont(panel.owner.theme.font(12));g.setColour(juce::Colours::lightgrey);g.drawText("Velocity",10,0,panel.roll.railWidth()-20,getHeight(),juce::Justification::centredLeft);
            const auto& clips=panel.roll.contextClips;
            const auto clip=std::find_if(clips.begin(),clips.end(),[this](const auto& c){return c.id==panel.id;});
            if(clip==clips.end() || !panel.roll.isEnabled())return;
            const auto left=panel.roll.railWidth(),right=panel.viewport.getViewWidth();
            for(const auto& n:panel.roll.notes){if(!panel.roll.noteDisplayed(n))continue;const auto x=panel.roll.displayedNoteRect(n).getCentreX()-panel.viewport.getViewPositionX();if(x<left||x>=right)continue;
                const auto value=originalVelocity.contains(n.id.value)?std::clamp(n.velocity+delta,1,127):n.velocity;
                const auto y=static_cast<float>(getHeight()-4)-value/127.f*std::max(1,getHeight()-9);
                g.setColour(panel.roll.displayedNoteColour(n,*clip,true,value));
                g.drawLine(x,y,x,static_cast<float>(getHeight()-3),1.f);g.fillEllipse(x-2.5f,y-2.5f,5.f,5.f);
                if(panel.roll.selected.contains(n.id.value)){g.setColour(juce::Colour(accent));g.drawEllipse(x-3.5f,y-3.5f,7.f,7.f,1.f);}
            }
        }
        void cancel() { grabbed.reset(); store.reset(); originalVelocity.clear(); delta=0; repaint(); }
        void begin(juce::Point<float> point,bool sameLevel=false) {
            cancel(); panel.roll.cancel(); panel.roll.sync();
            if (point.x < panel.roll.railWidth() || point.x >= panel.viewport.getViewWidth()) return;
            float best = 8.f;
            for (const auto& n : panel.roll.notes) {
                if (!panel.roll.noteDisplayed(n)) continue;
                const float x = panel.roll.displayedNoteRect(n).getCentreX()-panel.viewport.getViewPositionX();
                const float y = static_cast<float>(getHeight()-4)-n.velocity/127.f*std::max(1,getHeight()-9);
                const float distance = point.getDistanceFrom({x,y});
                if (distance < best) { best = distance; grabbed = n; }
            }
            if (grabbed) {
                preview=grabbed->velocity; lastAudition=-1000.;
                store=panel.owner.app.services().projects;clipId=panel.id;
                const bool selectedGroup=panel.roll.selected.contains(grabbed->id.value);
                for(const auto& n:panel.roll.notes)if(panel.roll.noteDisplayed(n) &&
                    (n.id==grabbed->id || (sameLevel?n.velocity==grabbed->velocity:selectedGroup&&panel.roll.selected.contains(n.id.value))))
                    originalVelocity.emplace(n.id.value,n.velocity);
                repaint();
            }
        }
        void drag(float deltaY) {
            if (!grabbed) return;
            delta=-static_cast<int>(std::lround(deltaY*127.f/std::max(1,getHeight()-9)));
            const int next=std::clamp(grabbed->velocity+delta,1,127);
            const auto now=juce::Time::getMillisecondCounterHiRes();
            if(next!=preview && now-lastAudition>=125. && store==panel.owner.app.services().projects && clipId==panel.id){
                const auto track=panel.roll.trackId();
                if(panel.owner.app.audition_note(track,grabbed->pitch,next,grabbed->channel))
                    panel.owner.app.audition_note(track,grabbed->pitch,next,grabbed->channel,false,PianoRoll::velocityPreviewTailMs);
                lastAudition=now;
            }
            preview=next;repaint();
        }
        void commit() {
            if (!grabbed) return;
            const bool current = store==panel.owner.app.services().projects && clipId==panel.id;
            const int offset=delta;auto targets=std::move(originalVelocity);cancel();
            if (!current || !offset) return;
            try {
                auto next = panel.current(*panel.owner.project()).midi->notes;
                std::set<std::string> selection;bool changed=false;
                for(auto& n:next)if(const auto at=targets.find(n.id.value);at!=targets.end()){
                    if(n.velocity!=at->second || !panel.roll.noteDisplayed(n))return;
                    selection.insert(n.id.value);const int nextValue=std::clamp(n.velocity+offset,1,127);
                    changed|=nextValue!=n.velocity;n.velocity=nextValue;
                }
                if(!changed || selection.size()!=targets.size())return;
                panel.owner.app.set_midi_notes(panel.id,std::move(next));
                panel.owner.refresh(); panel.roll.sync(); panel.roll.selected=std::move(selection); panel.reload();
            } catch (const std::exception& e) { panel.hint.setText(juce::String::fromUTF8(e.what()),juce::dontSendNotification); }
        }
        void mouseDown(const juce::MouseEvent& e) override { if(e.mods.isLeftButtonDown())begin(e.position,e.mods.isCtrlDown()&&e.mods.isShiftDown()); }
        void mouseDrag(const juce::MouseEvent& e) override { drag(static_cast<float>(e.getDistanceFromDragStartY())); }
        void mouseUp(const juce::MouseEvent&) override { commit(); }
        void visibilityChanged() override { if(!isShowing()) cancel(); }
    } velocity{*this};
    struct RollViewport : juce::Viewport {std::function<void()> changed;void visibleAreaChanged(const juce::Rectangle<int>&) override{if(changed)changed();}};
    PianoRoll roll;ControllerPanel controllerLane;RollViewport viewport;bool showDetails{};bool firstLayout{true};
    bool fullScreenActive{},returnAttached{};
    juce::TextButton pointer{""},split{""},follow{"Follow playhead"},gridSnap{""},playheadMode{"Resume"};juce::ComboBox noteColors;NoteGridButton noteGrid;
    juce::TextButton quantizeButton{"Q"};juce::Component quantizePanel;
    std::array<juce::TextButton,7> quantizeDivisors;
    std::array<juce::TextButton,4> quantizeShapes;
    std::array<juce::Label,5> quantizeLabels;
    std::array<juce::Slider,5> quantizeSliders;
    juce::Component sideBody;juce::Viewport sideViewport;
    static constexpr std::array<int,3> ccNumbers{1,11,64};
    std::array<juce::TextButton,3> quickCC;
    int laneHeight{180}, velocityHeight{62};
    juce::Point<int> lastRollExtent;
    struct Divider final : juce::Component {
        MidiClipPanel& owner;bool total{};int startHeight{}, screenY{};bool dragging{};
        explicit Divider(MidiClipPanel& p,bool whole=false):owner(p),total(whole){setComponentID(total?"midi-lanes-divider":"controller-divider");setName(total?"Resize MIDI lanes":"Resize controller pane");setMouseCursor(juce::MouseCursor::UpDownResizeCursor);setWantsKeyboardFocus(true);}
        void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(panel));g.setColour(juce::Colour(menuBorder));g.drawHorizontalLine(0,0.f,static_cast<float>(getWidth()));g.setColour(juce::Colours::lightgrey.withAlpha(isMouseOver()?.8f:.35f));const float x=(getWidth()+160.f)*.5f;g.drawLine(x-16,9,x+16,9,1);g.drawLine(x-16,13,x+16,13,1);}
        void mouseDown(const juce::MouseEvent& e) override{if(!e.mods.isLeftButtonDown())return;owner.roll.cancel();owner.controllerLane.cancelGesture();owner.velocity.cancel();startHeight=height();screenY=e.getScreenY();dragging=true;grabKeyboardFocus();}
        void mouseDrag(const juce::MouseEvent& e) override{if(dragging&&e.mods.isLeftButtonDown())apply(startHeight+screenY-e.getScreenY());}
        void mouseUp(const juce::MouseEvent&) override{if(dragging)owner.saveMidiLaneHeights();dragging=false;}
        void focusLost(FocusChangeType) override{if(dragging)owner.saveMidiLaneHeights();dragging=false;}
        bool keyPressed(const juce::KeyPress& k) override{if(k.getKeyCode()==juce::KeyPress::upKey||k.getKeyCode()==juce::KeyPress::downKey){owner.roll.cancel();owner.controllerLane.cancelGesture();apply(height()+(k.getKeyCode()==juce::KeyPress::upKey?16:-16));owner.saveMidiLaneHeights();return true;}if(k==juce::KeyPress::escapeKey&&dragging){dragging=false;apply(startHeight);return true;}return false;}
        int height() const {return total ? owner.velocity.getHeight()+owner.controllerLane.getHeight()*owner.controllerLane.isVisible() : owner.controllerLane.getHeight();}
        void apply(int h) { if(total) owner.setBlockHeight(h); else owner.setLaneHeight(h); }
    } divider{*this},blockDivider{*this,true};
    void updateQuickCC(){for(std::size_t i=0;i<quickCC.size();++i)quickCC[i].setToggleState(controllerLane.displayedCC()==ccNumbers[i],juce::dontSendNotification);}
    static constexpr int rollTop=36,dividerHeight=22,maxRowHeight=48,quantizePanelHeight=80;
    int editorRollTop() const{return rollTop+(quantizePanel.isVisible()&&!owner.standaloneLive?quantizePanelHeight:0);}
    int clampLaneHeight(int height) const{
        // Keep room for the ruler, a full pitch row even at maximum zoom, and the scrollbar.
        const int minRollHeight=PianoRoll::ruler+maxRowHeight+viewport.getScrollBarThickness();
        const int maxHeight=juce::jmax(0,getHeight()-editorRollTop()-dividerHeight-minRollHeight);
        return juce::jlimit(juce::jmin(90,maxHeight),maxHeight,height);
    }
    static constexpr int minVelocityHeight=24;
    int blockCapacity() const {
        const int minRollHeight=PianoRoll::ruler+maxRowHeight+viewport.getScrollBarThickness();
        return std::max(0,getHeight()-editorRollTop()-minRollHeight-dividerHeight*(controllerLane.isVisible()?2:1));
    }
    void saveMidiLaneHeights(){
        if(owner.view.midiVelocityHeight==velocityHeight && owner.view.midiControllerHeight==laneHeight)return;
        owner.view.midiVelocityHeight=velocityHeight;owner.view.midiControllerHeight=laneHeight;owner.saveSettings();
    }
    void setBlockHeight(int height) {
        const int cc=controllerLane.isVisible()?controllerLane.getHeight():0;
        if(velocity.getHeight()>velocityHeight && height<=cc+velocity.getHeight())return;
        velocityHeight=std::max(minVelocityHeight,std::min(height,blockCapacity())-cc);
        layoutEditorPanes(viewport.getX(),viewport.getWidth());
    }
    void setLaneHeight(int height){
        if (velocity.isVisible() && controllerLane.isVisible()) {
            const int sum=velocity.getHeight()+controllerLane.getHeight();
            const int autoFill=std::max(0,velocity.getHeight()-velocityHeight);
            laneHeight=std::clamp(height,std::min(90,std::max(0,sum-minVelocityHeight)),std::max(0,sum-minVelocityHeight));
            velocityHeight=std::max(minVelocityHeight,sum-laneHeight-autoFill);
        } else laneHeight=clampLaneHeight(height);
        layoutEditorPanes(viewport.getX(),viewport.getWidth());
    }
    void layoutEditorPanes(int x,int width){
        const bool showLane=controllerLane.isVisible(), showVelocity=velocity.isVisible();
        lastRollExtent={roll.getWidth(),roll.getHeight()};
        divider.setVisible(showLane); blockDivider.setVisible(showVelocity);
        const int top=editorRollTop();juce::Rectangle<int> rollArea{x,top,width,juce::jmax(0,getHeight()-top)};
        const int capacity=blockCapacity();
        const int ccHeight=showLane?(showVelocity?std::min(laneHeight,std::max(0,capacity-minVelocityHeight)):clampLaneHeight(laneHeight)):0;
        if(showLane){
            controllerLane.setBounds(rollArea.removeFromBottom(ccHeight));
            divider.setBounds(rollArea.removeFromBottom(dividerHeight));
            for(std::size_t i=0;i<quickCC.size();++i)quickCC[i].setBounds(4+static_cast<int>(i)*52,2,48,18);
        }
        if(showVelocity){
            const int scrollbar=roll.getWidth()>width?viewport.getScrollBarThickness():0;
            const int gridExtent=roll.getHeight()+scrollbar;
            const int spareHeight=rollArea.getHeight()-dividerHeight-gridExtent;
            const int effectiveVelocity=std::max(std::min(velocityHeight,std::max(0,capacity-ccHeight)),spareHeight);
            velocity.setBounds(rollArea.removeFromBottom(effectiveVelocity));
            blockDivider.setBounds(rollArea.removeFromBottom(dividerHeight));
        }
        if(viewport.getBounds()==rollArea)return;
        const bool atBottom=viewport.getViewPositionY()>0&&viewport.getViewArea().getBottom()>=roll.getHeight();
        viewport.setBounds(rollArea);
        if(atBottom)viewport.setViewPosition(viewport.getViewPositionX(),roll.getHeight());
    }
    juce::TextButton fullScreen{"Full screen"},zoomIn{"Zoom +"},zoomOut{"Zoom -"},copy{"Copy"},paste{"Paste"},selectAll{"Select all"},musical{"Musical edit..."},controllers{"Controllers"},attach{"Attach"},details{"Note details"};
    void deferFullScreen(){juce::Component::SafePointer<MidiClipPanel> safe(this);juce::MessageManager::callAsync([safe]{if(!safe)return;safe->roll.cancel();safe->controllerLane.cancelGesture();if(!safe->fullScreenActive){safe->returnAttached=static_cast<bool>(safe->getProperties()["mrs-attached"]);if(safe->returnAttached){safe->owner.attachClipEditor(safe,safe->id,false,false);safe->owner.view.editorAttached=true;}if(auto* w=safe->findParentComponentOfClass<EditorWindow>()){safe->fullScreenActive=true;w->onClose=[safe]{if(!safe)return;if(safe->fullScreenActive){if(auto* host=safe->findParentComponentOfClass<EditorWindow>())host->setFullScreen(false);safe->fullScreenActive=false;if(safe->returnAttached){safe->owner.attachClipEditor(safe,safe->id,true,false);safe->setVisible(false);}safe->resized();}safe->owner.setWorkspace(mrs::desktop::Workspace::arrange);};w->setFullScreen(true);safe->resized();}}else{if(auto* w=safe->findParentComponentOfClass<EditorWindow>())w->setFullScreen(false);safe->fullScreenActive=false;if(safe->returnAttached)safe->owner.attachClipEditor(safe,safe->id,true,false);safe->resized();}});}
    void deferAttach(){juce::Component::SafePointer<MidiClipPanel> safe(this);juce::MessageManager::callAsync([safe]{if(safe){safe->roll.cancel();safe->controllerLane.cancelGesture();if(safe->fullScreenActive){if(auto* w=safe->findParentComponentOfClass<EditorWindow>())w->setFullScreen(false);safe->fullScreenActive=false;}safe->owner.attachClipEditor(safe,safe->id,!static_cast<bool>(safe->getProperties()["mrs-attached"]));}});}
    void alignControllers(){if(viewport.getWidth()<=0)return;controllerLane.setTimeView(roll.clip.source_offset-roll.clip.start+static_cast<double>(viewport.getViewPositionX())/roll.beatWidth*mrs::ppq,juce::jmax(1,viewport.getWidth()-roll.railWidth()-ControllerPanel::timeViewRightInset)/roll.beatWidth*mrs::ppq,roll.railWidth());}
    void wheel(float delta,juce::ModifierKeys mods,float x,float y){roll.cancel();const auto vx=viewport.getViewPositionX(),vy=viewport.getViewPositionY();if(mods.isCtrlDown()){if(mods.isShiftDown()){const float before=roll.beatWidth;roll.beatWidth=juce::jlimit(24.f,384.f,before*std::pow(1.25f,delta*4));roll.sync();viewport.setViewPosition(juce::jmax(0,static_cast<int>((x-roll.railWidth())*roll.beatWidth/before-(x-vx-roll.railWidth()))),vy);}else{const float before=roll.rowHeight;roll.rowHeight=juce::jlimit(8.f,static_cast<float>(maxRowHeight),before+std::round(delta*24));roll.sync();viewport.setViewPosition(vx,juce::jmax(0,static_cast<int>((y-PianoRoll::ruler)*roll.rowHeight/before-(y-vy-PianoRoll::ruler))));}}else if(mods.isShiftDown())viewport.setViewPosition(juce::jmax(0,vx-static_cast<int>(delta*160)),vy);else viewport.setViewPosition(vx,juce::jmax(0,vy-static_cast<int>(delta*128)));alignControllers();}
    void zoom(float factor){const auto x=viewport.getViewPositionX();roll.cancel();roll.beatWidth=juce::jlimit(24.f,384.f,roll.beatWidth*factor);roll.sync();viewport.setViewPosition(static_cast<int>(x*factor),viewport.getViewPositionY());alignControllers();}
    juce::ComboBox selector;
    std::array<juce::Label,5> labels;std::array<juce::TextEditor,5> fields;
    juce::TextButton add{"Add note"},apply{"Apply note"},remove{"Delete note"};juce::Label hint;
    std::vector<mrs::MidiNote> notes;
    const mrs::Clip& current(const mrs::Project& p) const{auto i=std::find_if(p.clips.begin(),p.clips.end(),[&](const auto& c){return c.id==id;});if(i==p.clips.end()||!i->midi)throw std::runtime_error("MIDI clip no longer exists");return *i;}
    double number(std::size_t i) const{std::istringstream in(fields[i].getText().toStdString());in.imbue(std::locale::classic());double value{};if(!(in>>value)||!std::isfinite(value))throw std::runtime_error("Enter a valid number in every field");in>>std::ws;if(!in.eof())throw std::runtime_error("Unexpected characters in note field");return value;}
    void loadNote(){const int index=selector.getSelectedId()-1;mrs::MidiNote n;if(index>=0&&index<static_cast<int>(notes.size()))n=notes[static_cast<std::size_t>(index)];else{const auto p=owner.project();n.start=current(*p).midi->source_offset;}
        fields[0].setText(juce::String(n.pitch));fields[1].setText(juce::String(static_cast<double>(n.start)/mrs::ppq,6));fields[2].setText(juce::String(static_cast<double>(n.length)/mrs::ppq,6));fields[3].setText(juce::String(n.velocity));fields[4].setText(juce::String(n.channel+1));apply.setEnabled(index>=0);remove.setEnabled(index>=0);}
    void reload(int selected=1){const auto p=owner.project();const auto& c=current(*p);notes=c.midi->notes;if(roll.drums)std::erase_if(notes,[&](const auto& n){return !roll.noteVisible(n);});velocity.repaint();selector.clear(juce::dontSendNotification);for(std::size_t i=0;i<notes.size();++i){const auto& n=notes[i];selector.addItem("Note "+juce::String(static_cast<int>(i)+1)+" | pitch "+juce::String(n.pitch)+" | beat "+juce::String(static_cast<double>(n.start)/mrs::ppq,3),static_cast<int>(i)+1);}selector.setTextWhenNothingSelected("Empty MIDI clip — add a note");selector.setSelectedId(juce::jlimit(0,static_cast<int>(notes.size()),selected),juce::dontSendNotification);loadNote();
        hint.setText("Start and length use quarter-note beats. Start is relative to the original clip source;\nDouble click: create/remove. Ctrl/Shift click or drag empty space: select. Drag: move; both edges: trim. Alt-drag: copy; Ctrl+Shift-drag: velocity.\nD: duplicate; S: split; Shift+Up/Down: octave; Ctrl+Shift: octave copy. Ctrl+C/V/A/Z/Y; Delete.",juce::dontSendNotification);}
    void edit(int operation){try{if(roll.drums){roll.cancel();roll.sync();}const auto p=owner.project();auto next=current(*p).midi->notes;const int index=selector.getSelectedId()-1;
        if(operation!=0&&(index<0||index>=static_cast<int>(notes.size())))throw std::runtime_error("Select a note");
        auto at=next.end();if(operation!=0){at=std::find_if(next.begin(),next.end(),[&](const auto& n){return n.id==notes[static_cast<std::size_t>(index)].id;});if(at==next.end()||(roll.drums&&!roll.noteVisible(*at)))throw std::runtime_error("Note is no longer visible; reopen its drum group");}
        int selected=index+1;
        if(operation==2){next.erase(at);}else{mrs::MidiNote n;n.id=operation==0?mrs::new_id():at->id;const auto pitch=number(0),start=number(1),length=number(2),vel=number(3),channel=number(4);
            if(pitch<0||pitch>127||std::floor(pitch)!=pitch||vel<1||vel>127||std::floor(vel)!=vel||channel<1||channel>16||std::floor(channel)!=channel||start<0||start>static_cast<double>(mrs::max_tick)/mrs::ppq||length<=0||length>static_cast<double>(mrs::max_tick)/mrs::ppq)throw std::runtime_error("Note values are outside their allowed ranges");
            n.pitch=static_cast<int>(pitch);n.channel=static_cast<int>(channel)-1;if(roll.drums&&!roll.drumAddressVisible(n))throw std::runtime_error("Choose a visible drum row and channel");n.velocity=static_cast<int>(vel);n.start=static_cast<mrs::Tick>(std::llround(start*mrs::ppq));n.length=static_cast<mrs::Tick>(std::llround(length*mrs::ppq));if(operation==0){next.push_back(n);selected=static_cast<int>(next.size());}else *at=n;
        }
        owner.app.set_midi_notes(id,std::move(next));owner.refresh();roll.sync();reload(selected);
    }catch(const std::exception& e){hint.setText(juce::String::fromUTF8(e.what()),juce::dontSendNotification);}}
};

// Note grid: complete value list, swing displacement, palette hit testing and
// the shared piano roll / drum grid projection of the chosen grid.
void noteGridChecks(MidiClipPanel& editor,Desktop& d){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    auto& roll=editor.roll;
    auto mouse=[](juce::Component& c,juce::Point<float> p,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(mods),1,0,0,0,0,&c,&c,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,true);};
    const NoteGridSpec defaultGrid,tripletEighth{8,0,NoteGridSpec::Shape::triplet},swingEighth{8,60,NoteGridSpec::Shape::swing};
    const NoteGridSpec septupletEighth{8,0,NoteGridSpec::Shape::septuplet},lastGrid{16,100,NoteGridSpec::Shape::swing};
    const auto& choices=noteGridChoices();
    check(choices.size()==27&&choices.front().divisor==1&&choices.back()==lastGrid,"note grid lists every size including swing");
    check(NoteGridSpec{16,0,NoteGridSpec::Shape::straight}.snap(mrs::ppq/4-30)==mrs::ppq/4,"straight grid snaps to the nearest sixteenth");
    check(tripletEighth.position(3)==mrs::ppq&&tripletEighth.snap(mrs::ppq-40)==mrs::ppq,"triplet grid keeps three lines per quarter");
    check(swingEighth.position(1)==mrs::ppq/2+96&&swingEighth.snap(swingEighth.position(1)+20)==swingEighth.position(1),"swing displaces the weak eighth and snaps to it");
    check(swingEighth.position(-1)==-mrs::ppq/2+96&&swingEighth.snap(-300)==swingEighth.position(-1),"negative drag deltas continue the same grid pattern");
    check(septupletEighth.position(7)==4*mrs::ppq,"7:8 septuplet returns to the shared PPQ boundary");
    check(swingEighth.snap(swingEighth.position(501)+20)==swingEighth.position(501),"swing snap remains aligned late in the song");
    check(canonicalNoteGrid({16,37,NoteGridSpec::Shape::swing}).swing==37,"exact swing percentage is retained");
    {
        mrs::MidiClip clip;clip.start=mrs::ppq+30;clip.source_offset=120;clip.length=4*mrs::ppq;
        mrs::MidiNote a;a.id=mrs::new_id();a.start=350;a.length=100;a.pitch=64;a.velocity=73;a.channel=3;
        mrs::MidiNote b=a;b.id=mrs::new_id();b.start=700;
        clip.notes={a,b};const std::set<std::string> selected{a.id.value};
        const auto full=quantizeSelectedNotes(clip,selected,defaultGrid,{});
        auto expected=a;expected.start=330;
        check(full[0]==expected&&full[1]==b,"quantize selected start in project ticks preserves duration and metadata");
        expected.start=340;
        check(quantizeSelectedNotes(clip,selected,defaultGrid,{50,0,100})[0]==expected,"half strength moves halfway");
        check(quantizeSelectedNotes(clip,selected,defaultGrid,{100,0,10})==clip.notes,"range excludes distant start");
        check(quantizeSelectedNotes(clip,{},defaultGrid,{})==clip.notes,"empty selection is a no-op");
        check(quantizeSelectedNotes(clip,selected,defaultGrid,{0,0,100})==clip.notes,"zero strength is a no-op");
        expected.start=330;expected.length=1;
        check(quantizeSelectedNotes(clip,selected,defaultGrid,{100,100,100})[0]==expected,"end quantize never creates a zero-length note");
        auto edge=clip;edge.notes[0].start=edge.source_offset+edge.length-100;
        const auto bounded=quantizeSelectedNotes(edge,selected,defaultGrid,{});
        check(bounded[0].start+bounded[0].length<=edge.source_offset+edge.length&&bounded[0].length==100,"start quantize stays inside clip");
    }
    NoteGridSpec chosen;bool chosenSnap=true;NoteGridMenu palette{defaultGrid,true};
    palette.choose=[&](const NoteGridSpec& spec,bool snap){chosen=spec;chosenSnap=snap;};
    palette.mouseDown(mouse(palette,{90.f,72.f},juce::ModifierKeys::leftButtonModifier));
    check(chosen==tripletEighth&&chosenSnap,"grid palette selects the triplet eighth cell");
    palette.mouseDown(mouse(palette,{202.f,203.f},juce::ModifierKeys::leftButtonModifier));
    check(!chosenSnap&&chosen==defaultGrid,"grid palette turns snap off from the footer");
    check(editor.noteGrid.getComponentID()=="piano-note-grid"&&editor.noteGrid.getBounds().getRight()<=editor.getWidth()&&
          editor.noteGrid.getBounds().getY()<PianoRoll::ruler+8,"grid menu button sits on the piano roll top toolbar");
    editor.applyNoteGrid(tripletEighth,true);
    check(roll.gridSpec==tripletEighth&&roll.snapGrid&&roll.grid==mrs::ppq/3&&d.app.midi_quantize_preferences().divisor==8&&d.app.midi_quantize_preferences().shape==1,"grid selection drives the piano roll and settings");
    // Painted lines follow the same grid: the triplet line is only inked when selected.
    const auto gridPixels=[&](mrs::Tick tick){
        const int x=roll.railWidth()+static_cast<int>(static_cast<double>(tick)/mrs::ppq*roll.beatWidth);
        const auto image=roll.createComponentSnapshot({x-2,static_cast<int>(PianoRoll::ruler+roll.rowHeight)+6,5,1},true,1.f,juce::SoftwareImageType{});
        juce::Array<juce::uint32> values;for(int i=0;i<5;++i)values.add(image.getPixelAt(i,0).getARGB());
        return values;
    };
    const auto tripletLine=gridPixels(tripletEighth.position(1));
    editor.applyNoteGrid(defaultGrid,true);
    const auto sixteenthLine=gridPixels(tripletEighth.position(1));
    check(tripletLine!=sixteenthLine,"piano roll grid lines follow the selected note grid");
    editor.applyNoteGrid(defaultGrid,false);
    check(!roll.snapGrid&&roll.grid==mrs::ppq/4&&!d.app.midi_quantize_preferences().snap,"snap off keeps the nominal step in ticks");
    const auto settings=d.app.midi_quantize_preferences();
    const auto persisted=mrs::persistence::decode_project(mrs::persistence::encode(d.app.snapshot())).project;
    (void)persisted;
    check(settings.divisor==16&&settings.shape==0&&settings.swing==0&&!settings.snap,"note grid is stored on the active project");
    const auto document=mrs::persistence::decode_project(mrs::persistence::encode(d.app.snapshot()));
    check(std::any_of(document.extensions.begin(),document.extensions.end(),[](const auto& e){return e.tag=="MQTZ"&&e.bytes=="1 16 0 0 100 0 100 0";}),"project quantize settings survive archive round trip");
    const bool oldPanel=d.view.midiQuantizeOpen;editor.quantizeButton.onClick();
    check(editor.quantizePanel.isVisible()==!oldPanel&&ViewSettings::decode(d.view.encode()).midiQuantizeOpen==!oldPanel,"quantize panel visibility persists");
    editor.quantizeButton.onClick();
    editor.applyNoteGrid(defaultGrid,true);
    check(roll.snapGrid&&d.app.midi_quantize_preferences().snap,"snap stays on after the grid checks");
    editor.gridSnap.onClick();
    check(!roll.snapGrid&&!d.app.midi_quantize_preferences().snap&&!editor.gridSnap.getToggleState()&&editor.noteGrid.spec()==defaultGrid,"direct Snap button disables the existing grid without changing resolution");
    editor.gridSnap.onClick();
    check(roll.snapGrid&&editor.gridSnap.getToggleState(),"direct Snap button restores snapping");
    const auto previousResume=d.view.midiResumePlayback;
    editor.playheadMode.onClick();
    check(d.view.midiResumePlayback!=previousResume&&editor.playheadMode.getToggleState()==d.view.midiResumePlayback&&ViewSettings::decode(d.view.encode()).midiResumePlayback==d.view.midiResumePlayback,"playback mode toggle and persisted setting");
    editor.playheadMode.onClick();
    const auto originalSize=editor.getBounds();
    editor.setSize(720,300);
    check(editor.gridSnap.getWidth()>0&&editor.playheadMode.getWidth()>0&&editor.follow.getRight()<=editor.getWidth()&&!editor.gridSnap.getBounds().intersects(editor.noteGrid.getBounds())&&!editor.playheadMode.getBounds().intersects(editor.noteColors.getBounds()),"MIDI toolbar controls fit the small editor");
    editor.setSize(originalSize.getWidth(),originalSize.getHeight());
}
void noteGridSmoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    Desktop d(true);d.app.new_project();
    const auto track=d.app.add_instrument_track("Note grid fixture");
    const auto clip=d.app.create_midi_clip(track,0,16*mrs::ppq);
    MidiClipPanel editor(d,clip);editor.setSize(1000,800);editor.setVisible(true);
    noteGridChecks(editor,d);
    // Snapping itself follows the chosen grid, including displaced swing lines.
    auto& roll=editor.roll;
    const auto tickX=[&](mrs::Tick tick){
        return editor.viewport.getViewPositionX()+roll.railWidth()+static_cast<float>(tick)/mrs::ppq*roll.beatWidth;
    };
    const auto doubleClick=[&](mrs::Tick tick){
        const auto point=juce::Point<float>{tickX(tick),static_cast<float>(editor.viewport.getViewPositionY()+PianoRoll::ruler)+6.f};
        roll.mouseDoubleClick(juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&roll,&roll,
            juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,true));
    };
    editor.applyNoteGrid(NoteGridSpec{8,0,NoteGridSpec::Shape::triplet},true);
    doubleClick(2*mrs::ppq+30);
    check(roll.notes.size()==1&&roll.notes.front().start==2*mrs::ppq,"triplet double click snaps the new note to a triplet line");
    d.app.undo();roll.sync();
    const NoteGridSpec swing{8,60,NoteGridSpec::Shape::swing};
    editor.applyNoteGrid(swing,true);
    doubleClick(swing.position(1)+40);
    check(roll.notes.size()==1&&roll.notes.front().start==swing.position(1),"swing double click snaps to the displaced eighth line");
    d.app.undo();roll.sync();
    editor.applyNoteGrid(NoteGridSpec{},true);
    // Exercise actual ruler mouse events and transport, not just grid arithmetic.
    struct RulerClock final:mrs::audio::IAudioDevice{
        std::vector<mrs::audio::DeviceInfo> enumerate()override{return {{0,"MIDI ruler fixture",{}, {"L","R"},16,8192,128,1}};}
        void control_panel(int)override{}
        void open(const mrs::audio::DeviceConfig&,std::shared_ptr<mrs::audio::AudioEngine>)override{}
        void start()override{}void stop()override{}void close()noexcept override{}
        mrs::audio::DeviceStatus status()override{return {mrs::audio::DevicePhase::running,48000};}
    };
    d.app.connect(std::make_unique<RulerClock>(),{0,48000,128,{}, {0,1}});
    std::array<float,256> audio{};const auto render=[&]{d.app.engine()->process(nullptr,audio.data(),128);};
    const auto sampleAt=[&](mrs::Tick tick){const auto p=d.project();return mrs::Timeline(p->time,p->sample_rate).to_samples(tick);};
    const auto seekRuler=[&](mrs::Tick raw,mrs::Tick expected){
        const auto before=d.app.services().projects->state().revision;
        const juce::Point<float> point{roll.railWidth()+static_cast<float>(raw)/mrs::ppq*roll.beatWidth,
            static_cast<float>(editor.viewport.getViewPositionY())+8.f};
        const auto event=juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&roll,&roll,
            juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,true);
        roll.mouseDown(event);render();
        check(d.message.isEmpty()&&d.app.engine()->state().sample==sampleAt(expected),"MIDI ruler seeks to the expected global grid or free position");
        check(d.app.services().projects->state().revision==before,"ruler seek is not a project edit");
    };
    const auto pressSpace=[&]{
        d.spaceKey(false);check(roll.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)),"MIDI editor consumes Space");render();
    };
    for(bool drumView:{false,true}){
        editor.setDrumMode(drumView);check(roll.drums==drumView,"ruler fixture editor mode");
        editor.viewport.setViewPosition(0,0);
        editor.applyNoteGrid(NoteGridSpec{},true);
        seekRuler(2*mrs::ppq+80,2*mrs::ppq);
        editor.gridSnap.onClick();seekRuler(2*mrs::ppq+80,2*mrs::ppq+80);
        editor.gridSnap.onClick();
        const NoteGridSpec triplet{8,0,NoteGridSpec::Shape::triplet};
        editor.applyNoteGrid(triplet,true);seekRuler(triplet.position(4)+40,triplet.position(4));
        editor.applyNoteGrid(swing,true);seekRuler(swing.position(1)+40,swing.position(1));
        // A scrolled timeline still uses project ticks, not viewport-relative ticks.
        editor.applyNoteGrid(NoteGridSpec{},true);
        editor.viewport.setViewPosition(100,0);
        seekRuler(4*mrs::ppq+80,4*mrs::ppq);
        editor.viewport.setViewPosition(0,0);
        if(!d.view.midiResumePlayback)editor.playheadMode.onClick();
        seekRuler(2*mrs::ppq+80,2*mrs::ppq);
        pressSpace();check(d.app.engine()->state().playback==mrs::PlaybackState::playing,"MIDI editor starts at snapped position");
        pressSpace();const auto pausedAt=d.app.engine()->state().sample;
        check(d.app.engine()->state().playback==mrs::PlaybackState::paused&&pausedAt>sampleAt(2*mrs::ppq),"Resume preserves the last pause");
        roll.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey));render();
        check(d.app.engine()->state().playback==mrs::PlaybackState::paused,"held Space does not restart playback");
        pressSpace();check(d.app.engine()->state().sample>pausedAt,"Resume continues at the pause point");
        d.action(31);render();d.spaceKey(false);
        editor.playheadMode.onClick();seekRuler(2*mrs::ppq+80,2*mrs::ppq);
        pressSpace();pressSpace();
        check(d.app.engine()->state().playback==mrs::PlaybackState::stopped&&d.app.engine()->state().sample==sampleAt(2*mrs::ppq),"Return stops at the snapped playback start");
        d.spaceKey(false);editor.playheadMode.onClick();
    }
    editor.setDrumMode(false);d.app.disconnect();
    const auto folder=juce::File::getCurrentWorkingDirectory();
    auto save=[&](juce::Component& component,const juce::String& name,float scale){
        auto stream=folder.getChildFile(name+"-"+juce::String(static_cast<int>(scale*100))+".png").createOutputStream();
        if(!stream)throw std::runtime_error("Note grid preview stream");
        stream->setPosition(0);stream->truncate();
        if(!juce::PNGImageFormat().writeImageToStream(component.createComponentSnapshot(component.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*stream))
            throw std::runtime_error("Note grid preview PNG");
    };
    NoteGridMenu menu{NoteGridSpec{},true};
    for(float scale:{1.f,1.5f})save(menu,"note-grid-menu",scale);
    save(editor,"note-grid-toolbar",1.f);
}

#if defined(MRS_DRUM_MAP_UI)
void drumGridSmoke(){
    const auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    Desktop d(true);d.app.new_project();const auto track=d.app.add_instrument_track("Drums");
    const auto id=d.app.create_midi_clip(track,0,16*mrs::ppq);
    mrs::DrumMapProfile map;map.id={"map"};map.name="Test kit";
    const mrs::DrumMapInstrument rack{{"rack"},"Rack Tom",{{{"rack-center"},"Center",48,{},{}},{{"rack-rim"},"Rimshot",82,1,{}}}};
    const mrs::DrumMapInstrument floor{{"floor"},"Floor Tom",{{{"floor-center"},"Center",43,{},{}}}};
    const mrs::DrumMapInstrument ride{{"ride"},"Ride",{{{"ride-bell"},"Bell",53,{},{}}}};
    map.categories={{{"toms"},"Toms",{rack,floor}},{{"cymbals"},"Cymbals",{ride}}};
    {
        auto compactMap=map;
        const mrs::DrumMapInstrument hh{{"hh"},"HH",{{{"closed"},"Closed",42,{},{}}}};
        compactMap.categories.push_back({{"hi-hat"},"Hi-Hat",{hh}});
        mrs::DrumMapBinding binding{compactMap,{}};DrumGridRows rows;rows.rebuild(&binding,{});
        const auto at=rows.index(42,0);
        check(at>0&&rows.rows[static_cast<std::size_t>(at)].depth==1&&rows.rows[static_cast<std::size_t>(at-1)].group==mrs::Id{"hi-hat"},"single Hi-Hat layer keeps note assignment");
        check(rows.rows[static_cast<std::size_t>(rows.index(53,0))].depth==2,"Cymbals keep nested instruments");
        binding.collapsed={{"hi-hat"}};rows.rebuild(&binding,{});
        check(rows.index(42,0)<0&&rows.index(53,0)>=0,"compact group folds independently");
    }
    map.categories.front().instruments.front().articulations.front().alternate_notes = {50};
    map.categories.front().color = 0xf07468;
    map.categories.front().instruments.back().color=0x53bce4;
    const mrs::MidiNote a{{"a"},0,240,50,100,0},b{{"b"},480,240,43,86,0},c{{"c"},960,240,53,110,0},u{{"u"},1440,240,99,86,0};
    d.app.set_midi_notes(id,{a,b,c,u});d.app.set_track_drum_map(track,mrs::DrumMapBinding{map,{}});
    MidiClipPanel editor(d,id);editor.setSize(1100,650);editor.setDrumMode(true);
    auto& roll=editor.roll;const auto original=d.project()->clips.front().midi->notes;
    check(!roll.noteRect(a).isEmpty()&&!roll.noteRect(u).isEmpty(),"mapped and unmapped notes visible");
    check(roll.drumLayout().index(48,0)==roll.drumLayout().index(50,0),"default and alternate share one row");
    editor.noteColors.setSelectedId(3,juce::sendNotificationSync);
    check(roll.noteColour(a,d.project()->clips.front()).getARGB()==0xfff07468u && ViewSettings::decode(d.view.encode()).noteInstrumentColors,"third mode inherits category RGB and persists");
    check(roll.noteColour(b,d.project()->clips.front()).getARGB()==0xff53bce4u && editor.currentTrack(*d.project())->midi_drum_mode,"instrument overrides category and drum mode persists on track");
    roll.selectVisible();check(roll.selected.size()==4,"select all visible");
    roll.toggleDrumGroup({"rack"});check(roll.noteRect(a).isEmpty()&&!roll.noteRect(b).isEmpty()&&!roll.noteRect(c).isEmpty(),"independent instrument fold");
    check(roll.selected.contains("a")&&d.project()->clips.front().midi->notes==original,"fold keeps displayed selection and events");
    const int foldedRow=roll.drumLayout().summaryIndex(50,0);
    check(foldedRow>=0 && roll.drumLayout().rows[static_cast<std::size_t>(foldedRow)].group==mrs::Id{"rack"},"folded alternate note projects onto instrument header only");
    d.resetDevice();
    const auto foldedPoint=roll.displayedNoteRect(a).getCentre();
    const auto foldedEvent=[&](int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),foldedPoint,juce::ModifierKeys(mods),1,0,0,0,0,&roll,&roll,juce::Time::getCurrentTime(),foldedPoint,juce::Time::getCurrentTime(),1,false);};
    roll.selected.clear();roll.mouseDown(foldedEvent(juce::ModifierKeys::leftButtonModifier));
    check(roll.selected.contains(a.id.value)&&roll.auditionPitch()==a.pitch,"folded diamond selects and auditions the actual alternate note");
    roll.mouseUp(foldedEvent(0));roll.selected={a.id.value,b.id.value};
    editor.velocity.begin({foldedPoint.x-editor.viewport.getViewPositionX(),static_cast<float>(editor.velocity.getHeight()-4)-a.velocity/127.f*std::max(1,editor.velocity.getHeight()-9)});
    check(editor.velocity.originalVelocity.size()==2&&editor.velocity.originalVelocity.contains(a.id.value),"folded diamond has a velocity handle and joins the selected group");
    editor.velocity.drag(-4.f);editor.velocity.commit();
    const auto editedFold=d.project()->clips.front().midi->notes;
    check(editedFold[0].velocity>a.velocity&&editedFold[1].velocity>b.velocity&&editedFold[2]==c,"mixed folded and open selection edits velocity only");
    check(d.app.undo(),"undo folded velocity group");roll.sync();check(roll.notes==original,"undo restores folded group velocities");
    roll.selectVisible();
    roll.erase();check(d.project()->clips.front().midi->notes==std::vector<mrs::MidiNote>{a},"delete does not touch hidden notes");
    check(d.app.undo(),"undo visible delete");roll.sync();check(roll.notes==original&&roll.noteRect(a).isEmpty(),"undo retains fold");
    roll.toggleDrumGroup({"toms"});roll.toggleDrumGroup({"toms"});check(roll.noteRect(a).isEmpty()&&!roll.noteRect(b).isEmpty(),"parent retains nested fold state");
    editor.foldAll(true);check(roll.noteRect(c).isEmpty()&&!roll.noteRect(u).isEmpty(),"collapse all retains unmapped access");
    editor.foldAll(false);check(!roll.noteRect(a).isEmpty()&&!roll.noteRect(c).isEmpty(),"expand all returns mapped notes");
    for(const auto& note:original){
        const auto tickX=roll.railWidth()+static_cast<float>(roll.clip.start+note.start-roll.clip.source_offset)/mrs::ppq*roll.beatWidth;
        check(std::abs(roll.noteRect(note).getCentreX()-tickX)<.01f,"diamond centre aligns with the MIDI onset");
    }
    roll.selected.clear();
    const juce::Point<float> velocityHandle{roll.noteRect(b).getCentreX()-editor.viewport.getViewPositionX(),static_cast<float>(editor.velocity.getHeight()-4)-b.velocity/127.f*std::max(1,editor.velocity.getHeight()-9)};
    editor.velocity.begin(velocityHandle);
    check(editor.velocity.grabbed && editor.velocity.preview==b.velocity && d.project()->clips.front().midi->notes==original,"grabbing does not jump or edit MIDI");
    editor.velocity.drag(-8.f);
    check(editor.velocity.preview>b.velocity && d.project()->clips.front().midi->notes==original,"velocity drag remains a local preview");
    editor.velocity.commit();
    check(d.project()->clips.front().midi->notes[1].velocity!=b.velocity&&d.project()->clips.front().midi->notes[0]==a,"velocity edit is isolated");
    check(d.app.undo(),"undo velocity");roll.sync();check(roll.notes==original,"undo restores original events");
    editor.velocity.begin(velocityHandle,true); editor.velocity.drag(-6.f); editor.velocity.commit();
    const auto grouped=d.project()->clips.front().midi->notes;
    check(grouped[1].velocity>b.velocity && grouped[1].velocity==grouped[3].velocity && grouped[0]==a && grouped[2]==c,"Shift drag applies only to original velocity peers");
    check(d.app.undo(),"undo grouped velocity in one step");roll.sync();check(roll.notes==original,"group undo restores all events");
    d.view.midiFollowPlayhead=true;
    { const auto savedView=ViewSettings::decode(d.view.encode()); MidiClipPanel reopened(d,id);
      check(savedView.midiFollowPlayhead && reopened.roll.drums && reopened.follow.getToggleState(),"editor restores drum mode and follow toggle"); }
    editor.controllerLane.setVisible(true); editor.resized();
    const auto preferredVelocity=editor.velocityHeight;
    const auto preferredControllers=editor.laneHeight;
    editor.foldAll(true);
    const int expectedGridHeight=roll.getHeight()+editor.viewport.getScrollBarThickness();
    check(editor.viewport.getHeight()==expectedGridHeight && editor.blockDivider.getY()==editor.viewport.getBottom() &&
          editor.velocity.getHeight()>preferredVelocity && editor.velocityHeight==preferredVelocity && editor.laneHeight==preferredControllers,
          "short folded grid fills spare height with Velocity without changing saved sizes");
    editor.foldAll(false);
    check(editor.viewport.getHeight()>expectedGridHeight && editor.velocityHeight==preferredVelocity && editor.laneHeight==preferredControllers,
          "unfolding immediately recovers room for drum rows");
    const int beforeTop=editor.blockDivider.getY(), beforeCC=editor.controllerLane.getHeight();
    editor.setBlockHeight(editor.velocity.getHeight()+beforeCC+32);
    check(editor.blockDivider.getY()==beforeTop-32 && editor.controllerLane.getHeight()==beforeCC,"top splitter grows total lanes upwards");
    const int resizedTop=editor.blockDivider.getY();
    editor.setLaneHeight(beforeCC+16);
    check(editor.blockDivider.getY()==resizedTop && editor.controllerLane.getHeight()==beforeCC+16,"internal splitter redistributes fixed total height");
    editor.saveMidiLaneHeights();
    const auto savedSizes=ViewSettings::decode(d.view.encode());
    { MidiClipPanel reopened(d,id);reopened.setSize(1100,650);
      check(reopened.velocity.getHeight()==editor.velocity.getHeight() && savedSizes.midiVelocityHeight==editor.velocity.getHeight() &&
            savedSizes.midiControllerHeight==editor.controllerLane.getHeight(),"saved lane sizes survive reopening and preference round-trip"); }
    auto image=editor.createComponentSnapshot(editor.getLocalBounds(),true,1.f,juce::SoftwareImageType{});
    auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("drum-grid-smoke.png").createOutputStream();
    check(stream&&stream->setPosition(0)&&stream->truncate().wasOk()&&juce::PNGImageFormat().writeImageToStream(image,*stream),"write Drum Grid preview");
    editor.setDrumMode(false);check(roll.railWidth()==PianoRoll::keys&&roll.notes==original,"Piano Roll round trip");
    editor.setDrumMode(true);editor.foldAll(true);
    const auto bytes=mrs::persistence::encode(d.app.snapshot());const auto restored=mrs::persistence::decode_project(bytes);
    check(restored.project==*d.project()&&restored.project.clips.front().midi->notes==original,"project round trip preserves folds and events");
    editor.foldAll(false);
    const auto click=[&](juce::Point<float> point,int modifiers){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(modifiers),1,0,0,0,0,&roll,&roll,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,false);};
    constexpr int velocityMods=juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::ctrlModifier|juce::ModifierKeys::shiftModifier;
    roll.selected={a.id.value,b.id.value};
    const auto grab=roll.noteRect(b).getCentre();
    roll.mouseDown(click(grab,velocityMods));roll.mouseDrag(click(grab.translated(3.f,-12.f),velocityMods));
    roll.mouseUp(click(grab.translated(3.f,-12.f),0));
    auto expected=original;expected[0].velocity+=8;expected[1].velocity+=8;
    check(roll.notes==expected,"Ctrl+Shift changes selected velocities without moving note starts or pitches");
    check(d.app.undo(),"undo Ctrl+Shift velocity");roll.sync();
    roll.selected={b.id.value};roll.keyPressed(juce::KeyPress(juce::KeyPress::upKey));
    auto rowMove=original;rowMove[1].pitch=82;rowMove[1].channel=1;
    check(roll.notes==rowMove,"Up follows the preceding articulation across an instrument header and takes its MIDI channel");
    check(d.app.undo(),"undo Up articulation move");roll.sync();
    roll.selected={a.id.value};roll.keyPressed(juce::KeyPress(juce::KeyPress::downKey));
    rowMove=original;rowMove[0].pitch=82;rowMove[0].channel=1;
    check(roll.notes==rowMove,"Down follows the next articulation rather than the next MIDI pitch");
    check(d.app.undo(),"undo Down articulation move");roll.sync();
    check(MidiClipPanel::followedScrollX(105,100,100,500)==100 && MidiClipPanel::followedScrollX(200,100,100,500)==188 &&
          MidiClipPanel::followedScrollX(0,100,100,500)==0 && MidiClipPanel::followedScrollX(700,500,100,500)==500,"follow page and loop wrap clamp without redundant scroll");
    const auto p=roll.noteRect(a).getCentre().withX(roll.railWidth()+4*roll.beatWidth);
    roll.mouseDoubleClick(click(p,juce::ModifierKeys::leftButtonModifier));
    check(roll.notes.size()==5&&std::any_of(roll.notes.begin(),roll.notes.end(),[](const auto& note){return note.pitch==48&&note.start==4*mrs::ppq;}),"double click creates mapped note at snap grid");
    check(d.app.undo(),"undo creation");roll.sync();
    auto dense=original;for(int i=0;i<4096;++i)dense.push_back({{"dense-"+std::to_string(i)},i*3,3,i%128,84,0});
    d.app.set_midi_notes(id,std::move(dense));roll.sync();roll.selectVisible();
    check(roll.selected.size()==4100,"dense clip complete selection");editor.foldAll(true);
    check(roll.selected.size()==4100&&roll.notes.size()==4100,"dense fold keeps all displayed hits selected");
    editor.viewport.setViewPosition(200,480);editor.alignControllers();
    check(editor.viewport.getViewPositionX()>0&&editor.viewport.getViewPositionY()>0,"dense grid scrolls on both axes");
    // Same panel is reused by both the attached and separate editor.
    const auto keys=d.app.add_instrument_track("Keys");
    const auto keysClip=d.app.create_midi_clip(keys,20*mrs::ppq,2*mrs::ppq);
    const auto otherDrumClip=d.app.create_midi_clip(track,24*mrs::ppq,2*mrs::ppq);
    editor.activate(keysClip);check(!roll.drums,"new track defaults to Piano Roll, not previous track mode");
    editor.activate(otherDrumClip);check(roll.drums,"another clip on the same drum track restores Drum Grid");
    editor.activate(keysClip);check(!roll.drums,"track switch restores Piano Roll");
    editor.setDrumMode(true);check(roll.drums,"explicit grid on another track");
    editor.setDrumMode(false);check(!roll.drums,"explicit Piano Roll on second track");
    check(d.app.undo(),"undo track presentation");editor.restoreDrumMode();check(roll.drums,"undo restores active editor mode");
    check(d.app.redo(),"redo track presentation");editor.restoreDrumMode();check(!roll.drums,"redo restores active editor mode");
    editor.activate(id);check(roll.drums,"changing the second track does not affect original drum track");
    const auto views=mrs::persistence::decode_project(mrs::persistence::encode(d.app.snapshot())).project;
    const auto mode=[&](const auto& trackId){return std::find_if(views.tracks.begin(),views.tracks.end(),[&](const auto& t){return t.id==trackId;})->midi_drum_mode;};
    check(mode(track)&&!mode(keys),"project archive restores independent track presentations");
    d.app.remove_clips({id});editor.setDrumMode(false);check(!roll.isEnabled(),"switch safely refused after clip deletion");
}
#endif

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
    mrs::MidiNote n;n.id=mrs::new_id();n.start=2*mrs::ppq;n.pitch=60;n.length=mrs::ppq;
    auto lowest=n;lowest.id=mrs::new_id();lowest.pitch=0;
    d.app.set_midi_notes(id,{n,lowest});
    std::vector<mrs::MidiChannelEvent> events;for(int cc:{1,11,64}){mrs::MidiChannelEvent e;e.id=mrs::new_id();e.kind=2;e.data1=cc;e.data2=80;e.start=n.start;events.push_back(e);}d.app.set_midi_events(id,events);
    d.view.controllersOpen=true;MidiClipPanel editor(d,id);editor.setSize(1000,800);editor.setVisible(true);
    const auto before=*d.project();const auto revision=d.app.services().projects->state().revision;
    auto& lane=editor.controllerLane;auto& vp=editor.viewport;auto& roll=editor.roll;
    check(lane.channel.getSelectedId()==1&&lane.channel.getText()=="1","compact caption preserves MIDI channel selection");
    auto mouse=[](juce::Component& c,juce::Point<float> p,juce::Point<float> origin,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(mods),1,0,0,0,0,&c,&c,juce::Time::getCurrentTime(),origin,juce::Time::getCurrentTime(),1,true);};
    const auto checkBottomDock=[&]{
        const auto& scrollbar=vp.getHorizontalScrollBar();
        check(lane.getBottom()==editor.getHeight()&&lane.getY()==editor.divider.getBottom(),
              "controller pane reaches the editor bottom without a gap");
        check(vp.getBottom()==editor.divider.getY(),"piano roll ends at the divider without overlap or a gap");
        if(scrollbar.isVisible()){
            const auto point=vp.getPosition()+scrollbar.getBounds().getCentre();
            check(editor.getComponentAt(point)==&scrollbar,"horizontal scrollbar stays reachable above the divider");
        }
    };
    const auto bounds=vp.getBounds();const auto scroll=vp.getViewPosition();
    const auto lowestPoint=[&]{return editor.getLocalPoint(&roll,roll.noteRect(lowest).getCentre().toInt());};
    vp.setViewPosition(0,roll.getHeight());
    check(editor.getComponentAt(lowestPoint())==&roll,"lowest MIDI note is reachable with Controllers open");
    vp.setViewPosition(scroll);
    check(vp.getHorizontalScrollBar().isVisible(),"layout fixture has horizontal overflow");
    checkBottomDock();
    const float beatWidth=roll.beatWidth;
    roll.beatWidth=24.f;roll.sync();
    check(!vp.getHorizontalScrollBar().isVisible(),"zooming out removes horizontal overflow");
    checkBottomDock();
    roll.beatWidth=beatWidth;roll.sync();vp.setViewPosition(scroll);
    checkBottomDock();
    const auto origin=juce::Point<float>{300,10};
    editor.divider.mouseDown(mouse(editor.divider,origin,origin,juce::ModifierKeys::leftButtonModifier));
    editor.divider.mouseDrag(mouse(editor.divider,origin.translated(0,-120),origin,juce::ModifierKeys::leftButtonModifier));
    check(lane.getHeight()==300&&vp.getBounds()==bounds.withTrimmedBottom(120)&&vp.getViewPosition()==scroll,"growing the controller pane gives up roll height without changing its scroll origin");
    checkBottomDock();
    editor.divider.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));check(lane.getHeight()==180&&vp.getBounds()==bounds,"Escape restores both pane heights");
    editor.divider.keyPressed(juce::KeyPress(juce::KeyPress::upKey));check(lane.getHeight()==196&&vp.getBounds()==bounds.withTrimmedBottom(16),"keyboard resize updates both panes");
    vp.setViewPosition(0,roll.getHeight());
    editor.setLaneHeight(10000);
    check(editor.getComponentAt(lowestPoint())==&roll,"resizing at the bottom keeps the lowest note in view");
    editor.setLaneHeight(-100);check(lane.getHeight()==90,"controller pane minimum graph height");
    vp.setViewPosition(scroll);
    editor.setLaneHeight(220);
    for(std::size_t i=0;i<editor.quickCC.size();++i){editor.quickCC[i].onClick();check(lane.displayedCC()==editor.ccNumbers[i]&&lane.matches(events[i]),"quick CC selects correct lane");for(std::size_t j=0;j<editor.quickCC.size();++j)check(editor.quickCC[j].getToggleState()==(i==j),"CC button state follows lane");}
    lane.kind.setSelectedId(5,juce::sendNotificationSync);for(auto& b:editor.quickCC)check(!b.getToggleState(),"pitch bend clears CC toggle");
    lane.setEnabled(false);check(!lane.controlsContent().isEnabled(),"reparented controller form follows disabled state");lane.setEnabled(true);
    lane.selectCC(1);auto p=lane.point(events.front());lane.graph.mouseDown(mouse(lane.graph,p,p,juce::ModifierKeys::leftButtonModifier));lane.graph.mouseDrag(mouse(lane.graph,p.translated(50,-10),p,juce::ModifierKeys::leftButtonModifier));editor.quickCC[1].onClick();lane.graph.mouseUp(mouse(lane.graph,p,p,0));check(!lane.dragging&&lane.events==events,"switch CC cancels controller preview without commit");
    lane.selectCC(1);p=lane.point(events.front());lane.graph.mouseDown(mouse(lane.graph,p,p,juce::ModifierKeys::leftButtonModifier));lane.graph.mouseDrag(mouse(lane.graph,p.translated(40,-8),p,juce::ModifierKeys::leftButtonModifier));lane.controlsContent().keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));lane.graph.mouseUp(mouse(lane.graph,p,p,0));check(!lane.dragging&&lane.events==events,"reparented form retains controller Escape route");
    const auto graphPoint=lane.getPosition()+lane.graph.getPosition()+lane.plot().getCentre().toInt();
    check(editor.getComponentAt(graphPoint)==&lane.graph,"automation pane retains hit testing");
    const auto exposed=juce::Point<int>{vp.getX()+100,vp.getY()+80};check(editor.getComponentAt(exposed)==&roll,"piano roll retains hit testing");
    const auto ccPoint=editor.divider.getPosition()+editor.quickCC.front().getBounds().getCentre();check(editor.getComponentAt(ccPoint)==&editor.quickCC.front(),"CC buttons own their hit region");
    const auto splitBounds=vp.getBounds();
    editor.details.onClick();check(editor.showDetails&&lane.isVisible(),"note fields coexist with controller lane");check(vp.getBounds()==splitBounds,"details do not shrink roll");
    editor.controllers.onClick();check(!lane.isVisible()&&!editor.divider.isVisible()&&vp.getBottom()==editor.getHeight(),"hiding automation gives the roll the full height");
    editor.controllers.onClick();check(vp.getBounds()==splitBounds,"reopening automation restores the split");
    editor.setSize(720,300);check(editor.sideViewport.getHeight()==300&&editor.sideBody.getHeight()>300,"small editor scrolls all sidebar controls");
    editor.sideViewport.setViewPosition(0,10000);auto& form=lane.controlsContent();check(editor.sideViewport.getViewPositionY()>0&&form.getBottom()<=editor.sideBody.getHeight(),"controller controls reachable at sidebar bottom");
    roll.rowHeight=static_cast<float>(MidiClipPanel::maxRowHeight);roll.sync();vp.setViewPosition(0,roll.getHeight());
    editor.setLaneHeight(10000);
    checkBottomDock();
    const auto lowBounds=editor.getLocalArea(&roll,roll.noteRect(lowest).getSmallestIntegerContainer());
    check(lowBounds.getY()>=vp.getY()+PianoRoll::ruler&&editor.getComponentAt(lowestPoint())==&roll,
          "small editor at maximum zoom and lane height still exposes the full lowest pitch row");
    roll.rowHeight=18.f;roll.sync();
    editor.setSize(1000,800);editor.details.onClick();editor.sideViewport.setViewPosition(0,0);editor.setLaneHeight(220);
    vp.setViewPosition(0,static_cast<int>(roll.noteRect(n).getY())-100);editor.alignControllers();lane.selectCC(64);
    check(*d.project()==before&&d.app.services().projects->state().revision==revision,"layout/CC/resize preserve MIDI model and Undo revision");
    noteGridChecks(editor,d);
    for(float scale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-note-layout-"+juce::String(static_cast<int>(scale*100))+"-preview.png").createOutputStream();check(stream!=nullptr,"note layout preview stream");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*stream),"note layout preview");}
    editor.setSize(720,300);editor.details.onClick();editor.sideViewport.setViewPosition(0,10000);
    vp.setViewPosition(0,roll.getHeight());
    auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-note-layout-small-preview.png").createOutputStream();check(stream!=nullptr,"small layout preview stream");stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds(),true,1.f,juce::SoftwareImageType{}),*stream);
    auto* target=editor.getComponentAt(lowestPoint());
    check(target==&roll,"lowest note receives pointer input in the small editor");
    const auto from=roll.noteRect(lowest).getCentre(),to=from.translated(roll.beatWidth/4,0);
    const int left=juce::ModifierKeys::leftButtonModifier;
    target->mouseDown(mouse(*target,from,from,left));
    target->mouseDrag(mouse(*target,to,from,left));
    target->mouseUp(mouse(*target,to,from,0));
    check(std::any_of(roll.notes.begin(),roll.notes.end(),[&](const auto& note){return note.id==lowest.id&&note.pitch==0&&note.start==lowest.start+mrs::ppq/4;}),
          "lowest note can be moved with the controller pane open");
    d.app.undo();check(*d.project()==before,"Undo restores the lowest note and preserves controllers");
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
    auto sharp=base.front();sharp.pitch=61;
    const auto y=roll.noteRect(sharp).getCentreY();
    const auto originalScroll=vp.getViewPosition();
    vp.setViewPosition(0,static_cast<int>(y)-80);
    juce::Image initialKeyboard;
    for(const auto x:{0,173}){
        vp.setViewPosition(x,vp.getViewPositionY());
        check(vp.getViewPositionX()==x,"keyboard fixture can pan the timeline");
        const auto area=vp.getLocalBounds().withWidth(PianoRoll::keys).withTrimmedTop(PianoRoll::ruler).withHeight(108);
        const auto keyboard=vp.createComponentSnapshot(area,true,1.f,juce::SoftwareImageType{});
        const int keyY=static_cast<int>(y)-vp.getViewPositionY()-PianoRoll::ruler;
        check(keyboard.getPixelAt(8,keyY).getBrightness()<.35f&&keyboard.getPixelAt(38,keyY+3).getBrightness()>.8f,
              "a dark accidental covers only the left side of the white keys");
        if(x==0)initialKeyboard=keyboard;
        else for(int py=0;py<keyboard.getHeight();++py)for(int px=0;px<keyboard.getWidth();++px)
            check(keyboard.getPixelAt(px,py)==initialKeyboard.getPixelAt(px,py),"timeline scrolling never shows through the keyboard");
        const juce::Point<float> black{vp.getViewPositionX()+8.f,y},white{vp.getViewPositionX()+38.f,y+3.f};
        roll.mouseDown(mouse(black,black,left));check(roll.auditionPitch()==61,"short black key auditions C sharp");
        roll.mouseUp(mouse(black,black,0));
        roll.mouseDown(mouse(white,white,left));check(roll.auditionPitch()==60,"white key beside the black tip auditions C natural");
        roll.mouseUp(mouse(white,white,0));check(roll.auditionPitch()==-1,"keyboard mouse up releases the audition");
    }
    check(PianoRoll::keys==43,"keyboard width is one third shorter");
    vp.setViewPosition(originalScroll);
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
void Desktop::syncMidiEditorTools(){
    if(standaloneLive)return;
    if(auto* midiEditor=dynamic_cast<MidiClipPanel*>(dockedEditor.get()))midiEditor->syncToolbarPreferences();
    for(const auto& window:windows)
        if(auto* midiEditor=dynamic_cast<MidiClipPanel*>(window->getContentComponent()))midiEditor->syncToolbarPreferences();
}
void Desktop::syncMidiEditorModes(){
    if(standaloneLive)return;
    if(dockedEditor&&dockedEditor->isVisible())
        if(auto* midiEditor=dynamic_cast<MidiClipPanel*>(dockedEditor.get()))midiEditor->restoreDrumMode();
    for(const auto& window:windows)if(window->isVisible())
        if(auto* midiEditor=dynamic_cast<MidiClipPanel*>(window->getContentComponent()))midiEditor->restoreDrumMode();
}
std::optional<mrs::Id> Desktop::activeMidiEditorTrack() const {
    if (dockedEditor && dockedEditor->isShowing())
        if (const auto* midiPanel = dynamic_cast<const MidiClipPanel*>(dockedEditor.get())) return midiPanel->roll.trackId();
    for (const auto& window : windows) if (window->isVisible())
        if (const auto* midiPanel = dynamic_cast<const MidiClipPanel*>(window->getContentComponent())) return midiPanel->roll.trackId();
    return {};
}
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
