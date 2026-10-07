#include "PianoRoll.h"
#include "J1Smoke.h"
#include "PluginPreset.h"
#include <windows.h>
#include <thread>
#include <chrono>
namespace ui {
void midi4bSmoke(Desktop& d,const juce::File& fixture){
    auto check=[](bool b,const char* text){if(!b)throw std::runtime_error(text);};
    if(!fixture.existsAsFile())return; auto stage=[](const char* text){juce::File::getCurrentWorkingDirectory().getChildFile("midi-clips-smoke-stage.txt").replaceWithText(text);};stage("begin");
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);const auto folder=temp.getNonexistentChildFile("mrs-midi4b","",false);folder.createDirectory();const auto plugin=folder.getChildFile("instrument.vst3");check(fixture.copyFileTo(plugin),"MIDI playback fixture copy");
    const bool peer=d.getPeer()==nullptr;if(peer)d.addToDesktop(0);
    d.closeEditors();d.app.new_project();const auto track=d.app.add_instrument_track("MIDI clip playback");auto info=mrs::processing::probe_vst3(plugin.getFullPathName().toStdString()).front();mrs::NativeInsert fx;fx.id=mrs::new_id();fx.kind=mrs::InsertKind::vst3;fx.plugin_path=info.path;fx.class_id=info.class_id;fx.plugin_name=info.name;d.app.set_inserts(track,{fx});d.app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1},2});d.selectedTrack=track;d.refresh(true);stage("create editor");d.action(43);
    check(d.message.isEmpty()&&d.selectedClip&&d.project()->clips.size()==1&&!d.windows.empty(),"MIDI clip menu and editor");const auto clip=*d.selectedClip;auto* notePanel=d.windows.back()->getContentComponent();
    auto control=[&](const char* id)->juce::Component*{for(auto* child:notePanel->getChildren())if(child->getComponentID()==id)return child;throw std::runtime_error("MIDI field missing");};
    auto* add=dynamic_cast<juce::TextButton*>(control("midi-note-add"));check(add!=nullptr,"MIDI add action");stage("add note");add->onClick();check(d.project()->clips.front().midi->notes.size()==1,"MIDI note enters shared model");const auto note=d.project()->clips.front().midi->notes.front().id;
    dynamic_cast<juce::TextEditor*>(control("midi-note-0"))->setText("67");dynamic_cast<juce::TextButton*>(control("midi-note-apply"))->onClick();check(d.project()->clips.front().midi->notes.front().pitch==67&&d.project()->clips.front().midi->notes.front().id==note,"MIDI form keeps note ID");
    stage("edit validation");const auto revision=d.app.services().projects->state().revision;dynamic_cast<juce::TextEditor*>(control("midi-note-3"))->setText("0");dynamic_cast<juce::TextButton*>(control("midi-note-apply"))->onClick();check(d.app.services().projects->state().revision==revision,"invalid MIDI form is atomic");dynamic_cast<juce::TextEditor*>(control("midi-note-3"))->setText("100");dynamic_cast<juce::TextButton*>(control("midi-note-apply"))->onClick();
    stage("piano roll gestures");PianoRoll* roll=nullptr;for(auto* child:notePanel->getChildren())if(auto* viewport=dynamic_cast<juce::Viewport*>(child))roll=dynamic_cast<PianoRoll*>(viewport->getViewedComponent());check(roll!=nullptr,"piano roll viewport");
    const auto savedNotes=d.project()->clips.front().midi->notes;const auto origin=roll->noteRect(roll->notes.front()).getCentre();
    auto mouse=[&](juce::Point<float> point,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,roll,roll,juce::Time::getCurrentTime(),origin,juce::Time::getCurrentTime(),1,true);};
    const auto rev=d.app.services().projects->state().revision;roll->mouseDown(mouse(origin,juce::ModifierKeys::leftButtonModifier));roll->mouseDrag(mouse(origin.translated(24,-18),juce::ModifierKeys::leftButtonModifier));check(d.app.services().projects->state().revision==rev,"piano drag is preview only");roll->mouseUp(mouse(origin.translated(24,-18),0));check(d.project()->clips.front().midi->notes.front().pitch==68&&d.project()->clips.front().midi->notes.front().start==mrs::ppq/4,"piano snapped move");
    check(roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0)),"piano Undo shortcut");check(d.project()->clips.front().midi->notes==savedNotes,"piano shared Undo restores gesture");
    roll->sync();roll->selected={note.value};roll->copy();roll->cursor=2*mrs::ppq;roll->paste();check(d.project()->clips.front().midi->notes.size()==2,"piano paste creates note");check(d.project()->clips.front().midi->notes.back().id!=note,"piano paste fresh ID");
    roll->selected.clear();for(const auto& n:roll->notes)roll->selected.insert(n.id.value);const auto groupBefore=roll->notes;auto point=roll->noteRect(roll->notes.front()).getCentre();roll->mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));roll->mouseDrag(mouse(point.translated(24,-36),juce::ModifierKeys::leftButtonModifier));roll->mouseUp(mouse(point.translated(24,-36),0));check(d.project()->clips.front().midi->notes.front().pitch==69&&d.project()->clips.front().midi->notes.back().pitch==69,"piano group move");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));check(d.project()->clips.front().midi->notes==groupBefore,"one Undo restores whole group");
    roll->sync();point=roll->noteRect(roll->notes.front()).getCentre();roll->mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));roll->mouseDrag(mouse(point.translated(80,-100),juce::ModifierKeys::leftButtonModifier));roll->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));check(d.project()->clips.front().midi->notes==groupBefore&&roll->notes==groupBefore,"Escape cancels preview");
    roll->selected={d.project()->clips.front().midi->notes.back().id.value};roll->erase();check(d.project()->clips.front().midi->notes==savedNotes,"piano selected delete");
    roll->sync();point=roll->noteRect(roll->notes.front()).withWidth(roll->noteRect(roll->notes.front()).getWidth()-1).getTopRight().translated(0,8);roll->mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));roll->mouseDrag(mouse(point.translated(24,0),juce::ModifierKeys::leftButtonModifier));roll->mouseUp(mouse(point.translated(24,0),0));check(d.project()->clips.front().midi->notes.front().length==mrs::ppq+mrs::ppq/4,"piano right-edge resize");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));
    roll->sync();point=roll->noteRect(roll->notes.front()).getCentre();roll->mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier));roll->mouseDrag(mouse(point.translated(0,-20),juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier));roll->mouseUp(mouse(point.translated(0,-20),0));check(d.project()->clips.front().midi->notes.front().velocity==114,"piano velocity gesture");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));
    roll->sync();point=roll->noteRect(roll->notes.front()).getCentre();roll->mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));roll->mouseDrag(mouse(point.translated(-10000,-10000),juce::ModifierKeys::leftButtonModifier));roll->mouseUp(mouse(point.translated(-10000,-10000),0));check(d.project()->clips.front().midi->notes.front().start==0&&d.project()->clips.front().midi->notes.front().pitch==127,"piano edge clamp");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));
    stage("upd1 gestures");const auto beforeZoom=roll->rowHeight;roll->wheel(.5f,juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),origin.x,origin.y);check(roll->rowHeight>beforeZoom,"Ctrl wheel vertical piano zoom");roll->wheel(-.5f,juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),origin.x,origin.y);const auto beatBefore=roll->beatWidth;roll->wheel(.25f,juce::ModifierKeys(juce::ModifierKeys::ctrlModifier|juce::ModifierKeys::shiftModifier),origin.x,origin.y);check(roll->beatWidth>beatBefore,"Ctrl Shift wheel horizontal piano zoom");roll->wheel(-.25f,juce::ModifierKeys(juce::ModifierKeys::ctrlModifier|juce::ModifierKeys::shiftModifier),origin.x,origin.y);roll->sync();for(auto* child:notePanel->getChildren())if(auto* viewport=dynamic_cast<juce::Viewport*>(child))viewport->setViewPosition(0,static_cast<int>(roll->noteRect(roll->notes.front()).getY())-60);roll->mouseDoubleClick(mouse(roll->noteRect(roll->notes.front()).getCentre(),juce::ModifierKeys::leftButtonModifier));check(d.project()->clips.front().midi->notes.empty(),"double left click removes note");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));check(d.project()->clips.front().midi->notes==savedNotes,"double click delete Undo");
    stage("fix1 right click and audition");roll->sync();auto audiblePoint=roll->noteRect(roll->notes.front()).getCentre();const auto rightRevision=d.app.services().projects->state().revision;roll->mouseDown(mouse(audiblePoint,juce::ModifierKeys::rightButtonModifier));check(d.app.services().projects->state().revision==rightRevision&&d.project()->clips.front().midi->notes==savedNotes,"right click does not delete");std::array<float,256> auditionAudio{};d.app.engine()->process(nullptr,auditionAudio.data(),128);roll->mouseDown(mouse(audiblePoint,juce::ModifierKeys::leftButtonModifier));d.app.engine()->process(nullptr,auditionAudio.data(),128);const auto originalAudition=auditionAudio.back();check(originalAudition>0&&roll->auditionPitch()==67,"original drag note audition");roll->mouseDrag(mouse(audiblePoint.translated(0,-18),juce::ModifierKeys::leftButtonModifier));d.app.engine()->process(nullptr,auditionAudio.data(),128);check(roll->auditionPitch()==68&&std::abs(auditionAudio.back()-originalAudition)<.01f,"drag retunes one voice without retaining old note");roll->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));d.app.engine()->process(nullptr,auditionAudio.data(),128);check(auditionAudio.back()==0&&d.project()->clips.front().midi->notes==savedNotes,"Escape releases retuned audition");
    stage("musical edit dialog");roll->selected={note.value};auto* music=dynamic_cast<juce::TextButton*>(control("musical-edit-open"));check(music!=nullptr,"musical edit entry");music->onClick();auto* musicalPanel=d.windows.back()->getContentComponent();auto musicalControl=[&](const char* name)->juce::Component*{for(auto* child:musicalPanel->getChildren())if(child->getComponentID()==name)return child;throw std::runtime_error("musical control missing");};
    auto* op=dynamic_cast<juce::ComboBox*>(musicalControl("musical-operation"));op->setSelectedId(2,juce::sendNotificationSync);dynamic_cast<juce::TextEditor*>(musicalControl("musical-value"))->setText("12");auto* musicalApply=dynamic_cast<juce::TextButton*>(musicalControl("musical-apply"));musicalApply->onClick();check(d.project()->clips.front().midi->notes.front().pitch==79,"musical transpose UI applies command");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));check(d.project()->clips.front().midi->notes==savedNotes,"musical UI single Undo");
    dynamic_cast<juce::TextEditor*>(musicalControl("musical-value"))->setText("oops");const auto musicalRevision=d.app.services().projects->state().revision;musicalApply->onClick();check(d.app.services().projects->state().revision==musicalRevision,"musical invalid input atomic");
    roll->selected.clear();dynamic_cast<juce::TextEditor*>(musicalControl("musical-value"))->setText("-12");musicalApply->onClick();check(d.app.services().projects->state().revision==musicalRevision,"empty selection does not target whole clip");dynamic_cast<juce::ComboBox*>(musicalControl("musical-scope"))->setSelectedId(2,juce::sendNotificationSync);musicalApply->onClick();check(d.project()->clips.front().midi->notes.front().pitch==55,"whole clip musical scope");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));const auto noOpRevision=d.app.services().projects->state().revision;
    op->setSelectedId(1,juce::sendNotificationSync);dynamic_cast<juce::TextEditor*>(musicalControl("musical-strength"))->setText("0");musicalApply->onClick();check(d.app.services().projects->state().revision==noOpRevision,"zero strength UI no-op");
    for(float scale:{1.f,1.5f}){auto image=musicalPanel->createComponentSnapshot(musicalPanel->getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-musical-100-preview.png":"juce-musical-150-preview.png").createOutputStream();check(stream!=nullptr,"musical preview stream");stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);}
    stage("controller event editor");auto* controllerPanel=control("embedded-controllers");check(controllerPanel->isShowing(),"controllers integrated below piano roll");auto controllerControl=[&](const char* name)->juce::Component*{for(auto* child:controllerPanel->getChildren())if(child->getComponentID()==name)return child;throw std::runtime_error("controller UI missing");};auto* eventAdd=dynamic_cast<juce::TextButton*>(controllerControl("controller-add"));dynamic_cast<juce::TextEditor*>(controllerControl("controller-value"))->setText("127");eventAdd->onClick();check(d.project()->clips.front().midi->events.size()==1&&d.project()->clips.front().midi->events.front().data1==64,"controller sustain add");const auto sustainId=d.project()->clips.front().midi->events.front().id;
    dynamic_cast<juce::TextEditor*>(controllerControl("controller-position"))->setText("0.5");dynamic_cast<juce::TextEditor*>(controllerControl("controller-value"))->setText("0");dynamic_cast<juce::TextButton*>(controllerControl("controller-apply"))->onClick();check(d.project()->clips.front().midi->events.front().start==mrs::ppq/2&&d.project()->clips.front().midi->events.front().id==sustainId,"controller move retains ID");controllerPanel->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));check(d.project()->clips.front().midi->events.front().data2==127,"controller shared Undo");
    dynamic_cast<juce::ComboBox*>(controllerControl("controller-kind"))->setSelectedId(5,juce::sendNotificationSync);dynamic_cast<juce::TextEditor*>(controllerControl("controller-value"))->setText("8191");eventAdd->onClick();check(d.project()->clips.front().midi->events.size()==2&&d.project()->clips.front().midi->events.back().data1==127&&d.project()->clips.front().midi->events.back().data2==127,"14-bit bend UI");const auto controllerRevision=d.app.services().projects->state().revision;dynamic_cast<juce::TextEditor*>(controllerControl("controller-value"))->setText("8192");dynamic_cast<juce::TextButton*>(controllerControl("controller-apply"))->onClick();check(d.app.services().projects->state().revision==controllerRevision,"invalid bend atomic");
    dynamic_cast<juce::ComboBox*>(controllerControl("controller-kind"))->setSelectedId(3,juce::sendNotificationSync);dynamic_cast<juce::TextEditor*>(controllerControl("controller-value"))->setText("12");eventAdd->onClick();check(d.project()->clips.front().midi->events.back().kind==3&&d.project()->clips.front().midi->events.back().data1==12,"Program UI");dynamic_cast<juce::TextButton*>(controllerControl("controller-delete"))->onClick();check(d.project()->clips.front().midi->events.size()==2,"controller delete");
    dynamic_cast<juce::ComboBox*>(controllerControl("controller-selector"))->setSelectedId(1,juce::sendNotificationSync);auto* lane=controllerControl("controller-lane");const juce::Point<float> eventPoint{64,26};auto controllerMouse=[&](juce::Point<float> point,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,lane,lane,juce::Time::getCurrentTime(),eventPoint,juce::Time::getCurrentTime(),1,true);};const auto eventBaseline=d.project()->clips.front().midi->events;const auto dragRevision=d.app.services().projects->state().revision;lane->mouseDown(controllerMouse(eventPoint,juce::ModifierKeys::leftButtonModifier));lane->mouseDrag(controllerMouse(eventPoint.translated(200,50),juce::ModifierKeys::leftButtonModifier));check(d.app.services().projects->state().revision==dragRevision,"controller drag preview only");lane->mouseUp(controllerMouse(eventPoint.translated(200,50),0));check(d.project()->clips.front().midi->events.front().start>0&&d.project()->clips.front().midi->events.front().data2<127,"controller gesture position/value");controllerPanel->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));check(d.project()->clips.front().midi->events==eventBaseline,"one Undo restores controller drag");lane->mouseDown(controllerMouse(eventPoint,juce::ModifierKeys::leftButtonModifier));lane->mouseDrag(controllerMouse(eventPoint.translated(250,80),juce::ModifierKeys::leftButtonModifier));lane->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));lane->mouseUp(controllerMouse(eventPoint.translated(250,80),0));check(d.project()->clips.front().midi->events==eventBaseline,"controller Escape cancels gesture");
    dynamic_cast<juce::ComboBox*>(controllerControl("controller-kind"))->setSelectedId(2,juce::sendNotificationSync);dynamic_cast<juce::TextEditor*>(controllerControl("controller-number"))->setText("1");for(const auto [beat,val]:std::array<std::pair<int,int>,3>{{{0,10},{1,127},{2,20}}}){dynamic_cast<juce::TextEditor*>(controllerControl("controller-position"))->setText(juce::String(beat));dynamic_cast<juce::TextEditor*>(controllerControl("controller-value"))->setText(juce::String(val));eventAdd->onClick();}check(d.project()->clips.front().midi->events.back().data1==1,"continuous CC1 lane data");
    for(float scale:{1.f,1.5f}){auto image=controllerPanel->createComponentSnapshot(controllerPanel->getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-controller-100-preview.png":"juce-controller-150-preview.png").createOutputStream();check(stream!=nullptr,"controller preview stream");stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);}
    d.app.set_midi_events(clip,{});d.refresh();
    stage("pump timer");const auto until=juce::Time::getMillisecondCounter()+100;while(juce::Time::getMillisecondCounter()<until){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}check(notePanel->isShowing(),"MIDI editor survives engine rebuild timer");
    stage("playback");d.app.play();std::array<float,256> audio{};d.app.engine()->process(nullptr,audio.data(),128);check(audio.back()>0,"authored MIDI plays through VST3");d.app.pause();d.app.engine()->process(nullptr,audio.data(),128);d.app.poll();check(audio.back()==0,"MIDI pause releases notes");
    d.setWorkspace(mrs::desktop::Workspace::arrange);d.arrangement->pixelsPerSecond=240;d.refresh(true);auto rect=d.arrangement->clipRect(d.project()->clips.front());check(rect.getWidth()>400,"MIDI clip tick range visible");
    stage("snapshots");for(float scale:{1.f,1.5f}){auto image=d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-midi-clips-100-preview.png":"juce-midi-clips-150-preview.png").createOutputStream();check(stream!=nullptr,"MIDI arrange preview stream");stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);auto editorImage=notePanel->createComponentSnapshot(notePanel->getLocalBounds(),true,scale,juce::SoftwareImageType{});auto editorStream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-midi-notes-100-preview.png":"juce-midi-notes-150-preview.png").createOutputStream();check(editorStream!=nullptr,"MIDI notes preview stream");editorStream->setPosition(0);editorStream->truncate();juce::PNGImageFormat().writeImageToStream(editorImage,*editorStream);}
    stage("dock editor");d.selectedClip=clip;d.openSelectedEditor();d.attachClipEditor(notePanel,clip,true);check(d.dockedEditor.get()==notePanel&&d.app.workspace()==mrs::desktop::Workspace::edit,"Edit dock replaces mixer");check(roll->clip.notes==d.project()->clips.front().midi->notes,"dock preserves editor instance");d.setWorkspace(mrs::desktop::Workspace::mix);check(!notePanel->isVisible()&&d.master->isVisible(),"Mix replaces attached editor");d.setWorkspace(mrs::desktop::Workspace::edit);check(notePanel->isVisible()&&!d.master->isVisible(),"Edit restores attached panel");for(float scale:{1.f,1.5f}){auto image=d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-edit-dock-100-preview.png":"juce-edit-dock-150-preview.png").createOutputStream();stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);}d.attachClipEditor(notePanel,clip,false);check(!d.dockedEditor&&notePanel->isShowing(),"Detach retains editor instance");
    stage("fix1 global timeline");d.app.move_clip(clip,track,mrs::Timeline(d.project()->time,d.project()->sample_rate).to_samples(2*mrs::ppq));const auto ghost=d.app.create_midi_clip(track,10*mrs::ppq,2*mrs::ppq);mrs::MidiNote ghostNote;ghostNote.id=mrs::new_id();ghostNote.pitch=67;ghostNote.start=0;ghostNote.length=mrs::ppq;d.app.set_midi_notes(ghost,{ghostNote});d.refresh();roll->sync();check(roll->contextClips.size()==2&&roll->clip.start==2*mrs::ppq,"same-track clips in global piano timeline");check(std::abs(roll->noteRect(roll->notes.front()).getX()-(PianoRoll::keys+2*roll->beatWidth))<.01f,"active note global offset");const auto ghostRevision=d.app.services().projects->state().revision;roll->mouseDoubleClick(mouse({PianoRoll::keys+10.5f*roll->beatWidth,roll->noteRect(roll->notes.front()).getCentreY()},juce::ModifierKeys::leftButtonModifier));check(d.app.services().projects->state().revision==ghostRevision,"inactive clip stays read-only");d.app.seek(72000);d.app.engine()->process(nullptr,audio.data(),128);dynamic_cast<juce::TextButton*>(control("editor-play"))->onClick();d.app.engine()->process(nullptr,audio.data(),128);check(d.app.engine()->state().playback==mrs::PlaybackState::playing&&roll->playheadTick()==mrs::Timeline(d.project()->time,d.project()->sample_rate).to_ticks(d.app.engine()->state().sample),"editor Play and playhead share global transport");dynamic_cast<juce::TextButton*>(control("editor-pause"))->onClick();d.app.engine()->process(nullptr,audio.data(),128);check(d.app.engine()->state().playback==mrs::PlaybackState::paused,"editor Pause shares transport");roll->wheel(-.5f,juce::ModifierKeys(juce::ModifierKeys::ctrlModifier|juce::ModifierKeys::shiftModifier),0,0);for(auto* child:notePanel->getChildren())if(auto* viewport=dynamic_cast<juce::Viewport*>(child))viewport->setViewPosition(0,static_cast<int>(roll->noteRect(roll->notes.front()).getY())-80);for(float scale:{1.f,1.5f}){auto image=notePanel->createComponentSnapshot(notePanel->getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-editor-timeline-100-preview.png":"juce-editor-timeline-150-preview.png").createOutputStream();stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);}dynamic_cast<juce::TextButton*>(control("editor-stop"))->onClick();d.app.engine()->process(nullptr,audio.data(),128);d.app.remove_clip(ghost);d.app.move_clip(clip,track,0);d.refresh();roll->sync();
    stage("duplicate");d.closeEditors();d.action(45);check(d.project()->clips.size()==2&&d.project()->clips.back().midi->start==4*mrs::ppq,"duplicate MIDI menu");d.action(10);check(d.project()->clips.size()==1,"MIDI duplicate undo");d.selectedClip=clip;d.action(46);d.app.engine()->process(nullptr,audio.data(),128);check(d.app.engine()->state().loop.has_value(),"MIDI clip loop menu");
    stage("audio Edit");d.closeEditors();d.app.disconnect();d.app.new_project();const auto wav=folder.getChildFile("audio.wav");{juce::FileOutputStream stream(wav);stream.write("RIFF",4);stream.writeInt(36+192000);stream.write("WAVEfmt ",8);stream.writeInt(16);stream.writeShort(1);stream.writeShort(1);stream.writeInt(48000);stream.writeInt(96000);stream.writeShort(2);stream.writeShort(16);stream.write("data",4);stream.writeInt(192000);for(int i=0;i<96000;++i)stream.writeShort(static_cast<short>(10000*std::sin(i*.05)));}d.app.import_wavs({std::filesystem::path(wav.getFullPathName().toStdString())});d.refresh(true);d.selectedClip=d.project()->clips.front().id;d.openSelectedEditor();auto* audioEditor=d.windows.back()->getContentComponent();check(audioEditor->getComponentID()=="audio-clip-editor","Edit opens audio editor");juce::TextButton* audioApply=nullptr;for(auto* child:audioEditor->getChildren()){if(child->getComponentID()=="audio-trim-start")dynamic_cast<juce::TextEditor*>(child)->setText("0.25");if(child->getComponentID()=="audio-trim-apply")audioApply=dynamic_cast<juce::TextButton*>(child);}check(audioApply!=nullptr,"audio trim action");const auto audioBefore=d.project()->clips.front();audioApply->onClick();check(d.project()->clips.front().start==12000,"audio editor trims through shared command");d.app.undo();d.refresh();check(d.project()->clips.front().start==audioBefore.start&&d.project()->clips.front().length==audioBefore.length,"audio editor shared Undo");d.attachClipEditor(audioEditor,*d.selectedClip,true);check(d.dockedEditor.get()==audioEditor,"audio Attach");for(float scale:{1.f,1.5f}){auto image=d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-audio-edit-100-preview.png":"juce-audio-edit-150-preview.png").createOutputStream();stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);}d.attachClipEditor(audioEditor,*d.selectedClip,false);
    stage("cleanup");d.closeEditors();d.app.disconnect();d.app.new_project();d.refresh(true);check(folder.getParentDirectory()==temp&&folder.getFileName().startsWith("mrs-midi4b"),"MIDI smoke cleanup scope");folder.deleteRecursively();if(peer)d.removeFromDesktop();stage("passed");
}
void midi4aSmoke(Desktop& d,const juce::File& fixture){
    auto stage=[](const char* text){juce::File::getCurrentWorkingDirectory().getChildFile("midi-fix1-smoke-stage.txt").replaceWithText(text);};stage("begin");
    auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    if(fixture==juce::File())return;
    const bool addedPeer=!d.getPeer();if(addedPeer)d.addToDesktop(0);
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mrs-midi4a","",false);folder.createDirectory();
    const auto generator=folder.getChildFile("instrument.vst3");check(fixture.copyFileTo(generator),"instrument fixture copied");
    d.closeEditors();d.app.new_project();d.refresh(true);d.action(28);
    check(d.project()->tracks.size()==1&&d.project()->tracks.front().kind==mrs::TrackKind::instrument,"instrument track menu");
    const auto id=d.project()->tracks.front().id;
    d.app.disconnect(); // reproduce the user's disconnected editor path
    stage("add disconnected instrument");
    auto original=d.catalog;d.catalog=mrs::processing::probe_vst3(generator.getFullPathName().toStdString());d.addPlugin(id,0);
    check(d.message.isEmpty()&&!d.windows.empty(),"instrument opens from disconnected state");
    auto* editor=d.windows.back().get();check(static_cast<bool>(editor->getProperties()["mrs-native-editor"]),"disconnected instrument gets native GUI, not generic fallback");
    stage("native owner");
    const auto editorHwnd=static_cast<HWND>(editor->getPeer()->getNativeHandle());const auto dawHwnd=GetAncestor(static_cast<HWND>(d.getPeer()->getNativeHandle()),GA_ROOT);
    check(GetWindow(editorHwnd,GW_OWNER)==dawHwnd,"instrument editor owned by DAW");
    auto host=reinterpret_cast<HWND>(static_cast<std::intptr_t>(static_cast<juce::int64>(editor->getProperties()["mrs-native-host"])));check(IsWindow(GetWindow(host,GW_CHILD)),"instrument native child attached");
    for(int i=0;i<editor->getContentComponent()->getNumChildComponents();++i)if(auto* b=dynamic_cast<juce::TextButton*>(editor->getContentComponent()->getChildComponent(i));b&&b->getButtonText()=="Parameters")b->onClick();
    stage("generic owner");
    auto* generic=d.windows.back().get();check(generic!=editor&&GetWindow(static_cast<HWND>(generic->getPeer()->getNativeHandle()),GW_OWNER)==dawHwnd,"generic parameters also owned by DAW");
    d.toFront(false);check(editor->isVisible()&&generic->isVisible()&&IsWindow(GetWindow(host,GW_CHILD)),"DAW focus retains both editor views");d.closeEditors();
    stage("manual device");
    d.app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1},1});
    d.app.set_midi_input(id,"unavailable-fixture-port",-1,true);d.refresh(true);
    check(d.app.engine()->enqueue_live_midi({d.app.engine()->midi_generation(),0,{0,mrs::processing::MidiKind::note_on,0,60,127}}),"live note ingress");
    // Let route reconfiguration's panic settle before the deterministic manual callback.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));std::array<float,256> audio{};d.app.engine()->process(nullptr,audio.data(),128);
    d.app.engine()->enqueue_live_midi({d.app.engine()->midi_generation(),0,{0,mrs::processing::MidiKind::note_on,0,60,127}});d.app.engine()->process(nullptr,audio.data(),128);
    check(audio.back()>0,"instrument sounds while transport stopped");d.action(42);d.app.engine()->process(nullptr,audio.data(),128);check(audio.back()==0,"panic menu");
    d.app.set_midi_input(id,"",-1,false);d.refresh(true);d.arrangement->rows.front()->refreshMidiStatus();
    stage("disconnected MIDI status");
    d.app.disconnect();d.app.set_midi_input(id,"winmm:0:0:Komplete Kontrol A49 regression:0",2,true);d.app.poll();
    const auto status=d.app.midi_status(id);check(status.find("Komplete Kontrol A49 regression")!=std::string::npos&&status.find("audio disconnected")!=std::string::npos&&status!="MIDI: off","selected MIDI port retained while audio disconnected");d.refresh(true);
    d.setWorkspace(mrs::desktop::Workspace::mix);d.selectedTrack=id;d.refresh(true);
    stage("snapshots");
    for(float scale:{1.f,1.5f}){auto snapshot=d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{});check(snapshot.getWidth()==juce::roundToInt(d.getWidth()*scale),"MIDI UI DPI snapshot");auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-midi-100-preview.png":"juce-midi-150-preview.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(snapshot,*stream);}}
    d.catalog=original;d.app.disconnect();d.app.new_project();d.refresh(true);check(folder.getParentDirectory()==juce::File::getSpecialLocation(juce::File::tempDirectory)&&folder.getFileName().startsWith("mrs-midi4a"),"MIDI fixture cleanup scope");folder.deleteRecursively();
    stage("passed");
    if(addedPeer)d.removeFromDesktop();
}
void j2Smoke(Desktop& d,const juce::File& fixture){
    auto check=[](bool b,const char* text){if(!b)throw std::runtime_error(text);};
    d.app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});
    const auto first=d.project()->tracks.front().id;
    d.action(20);check(d.project()->tracks.size()==2,"add track binding");
    auto second=d.project()->tracks.back().id;d.app.rename_track(second,"Stereo guitar");
    d.action(21);auto bus=d.project()->tracks.back().id;
    d.app.set_track_output(first,bus);d.app.set_track_sends(second,{{bus,.5f,true}});d.refresh(true);
    check(d.mixer.size()==3&&d.arrangement->rows.size()==3,"multiple channel views");
    auto pump=[] {for(int i=0;i<10;++i){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}};
    const auto beforeReorder=*d.project();
    juce::DragAndDropTarget::SourceDetails trackDrop("mrs-track:"+juce::String(first.value),d.arrangement->rows[0].get(),{130,300});
    check(d.mixer[2]->isInterestedInDragSource(trackDrop),"mixer accepts track drag from arrangement");d.mixer[2]->itemDropped(trackDrop);pump();
    check(d.project()->tracks.back().id==first&&d.mixer.back()->target==first&&d.arrangement->rows.back()->target==first,"reorder synchronizes both views");
    check(d.project()->clips==beforeReorder.clips,"reorder retains clip identities and positions");d.action(10);check(*d.project()==beforeReorder,"reorder undo");
    juce::DragAndDropTarget::SourceDetails reverseDrop("mrs-track:"+juce::String(bus.value),d.mixer[2].get(),{400,70});
    check(d.arrangement->isInterestedInDragSource(reverseDrop),"arrangement accepts mixer track drag");d.arrangement->itemDropped(reverseDrop);pump();
    check(d.project()->tracks.front().id==bus&&d.mixer.front()->target==bus&&d.arrangement->rows.front()->target==bus,"reverse reorder synchronizes views");d.action(10);
    const auto reorderRevision=d.app.services().projects->state().revision;d.dropTrack("mrs-track:"+juce::String(first.value),1);pump();check(d.app.services().projects->state().revision==reorderRevision,"same position drag is no-op");
    d.app.set_track_armed(second,true);d.app.set_track_input(second,-1);d.refresh(true);
    check(d.app.track_armed(second),"arm binding");
    auto& fader=d.mixer[1]->gain;const auto initial=d.app.services().projects->state();const auto point=fader.knob().getCentre();
    auto event=[&](juce::Point<float> position,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),position,juce::ModifierKeys(mods),1,0,0,0,0,&fader,&fader,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,true);};
    fader.mouseDown(event(point,juce::ModifierKeys::leftButtonModifier));fader.mouseDrag(event(point.translated(0,10),juce::ModifierKeys::leftButtonModifier));
    check(d.app.services().projects->state().revision==initial.revision,"mixer preview history");fader.mouseUp(event(point.translated(0,10),0));
    check(d.app.services().projects->state().revision==initial.revision+1,"mixer one undo");d.action(10);
    check(d.project()->tracks[1].mix==initial.project->tracks[1].mix,"shared strip undo");
    auto beforeHeight=d.arrangement->trackHeight;d.arrangement->wheel(.25f,juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),500);
    check(d.arrangement->trackHeight>beforeHeight,"vertical zoom");
    const double scale=d.arrangement->pixelsPerSecond;d.arrangement->wheel(.25f,juce::ModifierKeys(juce::ModifierKeys::ctrlModifier|juce::ModifierKeys::shiftModifier),500);
    check(d.arrangement->pixelsPerSecond>scale,"horizontal zoom");
    d.arrangement->wheel(-.25f,juce::ModifierKeys(juce::ModifierKeys::shiftModifier),500);check(d.arrangement->horizontal>0,"horizontal scroll");
    d.setWorkspace(mrs::desktop::Workspace::arrange);check(!d.master->isVisible(),"arrange hides docked mixer");
    d.setWorkspace(mrs::desktop::Workspace::mix);check(d.master->isVisible(),"mix shows docked mixer");
    d.sidebar=false;d.resized();int width=d.arrangeArea.getWidth();d.sidebar=true;d.resized();check(d.arrangeArea.getWidth()<width,"browser resizes arrangement");
    check(d.browser->tree.getRootItem()->getNumSubItems()==0,"empty browser");
    d.catalog={{"missing.vst3","00000000000000000000000000000001","Example","Example vendor","1"}};d.browser->rebuild();
    check(!d.browser->tree.getRootItem()->getSubItem(0)->isOpen(),"vendors initially collapsed");d.catalog.clear();d.browser->rebuild();
    auto originalClip=d.project()->clips.front();d.app.move_clip(originalClip.id,second,4800);d.selectedClip=originalClip.id;d.app.seek(10000);
    std::array<float,256> audio{};auto render=[&]{d.app.engine()->process(nullptr,audio.data(),128);d.app.poll();};render();d.action(24);
    check(d.project()->clips.size()==2,"split binding");d.action(10);check(d.project()->clips.size()==1,"split undo");
    const auto emptyFaderHeight=d.mixer[1]->gain.getHeight();const auto emptyMixerHeight=d.mixArea.getHeight();
    mrs::NativeInsert eq;eq.id=mrs::new_id();eq.kind=mrs::InsertKind::channel_eq;d.applyChain(second,{eq});
    check(d.mixer[1]->gain.getHeight()==emptyFaderHeight&&d.mixArea.getHeight()>emptyMixerHeight,"inserts grow header without shrinking faders");
    std::vector<mrs::NativeInsert> longChain{eq};for(int i=0;i<7;++i){auto effect=eq;effect.id=mrs::new_id();longChain.push_back(effect);}d.applyChain(second,longChain);
    check(d.mixer[1]->gain.getHeight()==emptyFaderHeight&&d.mixer[0]->gain.getHeight()==emptyFaderHeight,"long insert chain retains equal fixed fader heights");d.applyChain(second,{eq});d.openInsert(second,eq.id);
    check(!d.windows.empty()&&d.windows.back()->isVisible(),"native editor bridge");d.closeEditors();
    d.openInsert(second,eq.id);auto* editor=d.windows.back()->getContentComponent();
    auto eqEvent=[&](juce::Point<float> p,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(mods),1,0,0,0,0,editor,editor,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,true);};
    const auto eqRevision=d.app.services().projects->state().revision;d.app.play();render();
    const float midX=24+static_cast<float>(std::log(1000./20)/std::log(1000.))*660;
    editor->mouseDown(eqEvent({midX,193},juce::ModifierKeys::leftButtonModifier));editor->mouseDrag(eqEvent({midX+30,173},juce::ModifierKeys::leftButtonModifier));
    check(d.app.services().projects->state().revision==eqRevision,"live EQ preview");editor->mouseUp(eqEvent({midX+30,173},0));render();
    check(d.chain(second).front().bands[2].gain>0&&d.app.services().projects->state().revision==eqRevision+1,"live EQ commit");
    d.app.stop();render();d.closeEditors();
    auto effects=d.chain(second);mrs::NativeInsert legacy;legacy.id=mrs::new_id();legacy.kind=mrs::InsertKind::eq;legacy.gain=4;effects.push_back(legacy);d.applyChain(second,effects);d.openInsert(second,legacy.id);
    auto* legacyEditor=d.windows.back()->getContentComponent();auto* valueEditor=dynamic_cast<juce::TextEditor*>(legacyEditor->findChildWithID("native-gain"));auto* applyButton=dynamic_cast<juce::TextButton*>(legacyEditor->findChildWithID("apply-native"));
    check(valueEditor&&applyButton&&valueEditor->getText().getFloatValue()==4,"legacy EQ dB display");valueEditor->setText("6",false);applyButton->onClick();check(d.chain(second).back().gain==6,"legacy EQ dB commit");d.closeEditors();effects.pop_back();d.applyChain(second,effects);
    if(fixture!=juce::File()){
        d.catalog=mrs::processing::probe_vst3(std::string(fixture.getFullPathName().toUTF8()));check(!d.catalog.empty(),"fixture probe");
        juce::DragAndDropTarget::SourceDetails drop("vst3:0",d.browser.get(),{50,50});
        const auto revision=d.app.services().projects->state().revision;d.mixer[1]->itemDropped(drop);
        check(d.app.services().projects->state().revision==revision+1,"plugin drop commits once");
        d.closeEditors();d.resetDevice();d.applyChain(second,d.chain(second));
        auto plugin=d.chain(second).back();d.openInsert(second,plugin.id);check(d.windows.back()->isVisible()&&static_cast<bool>(d.windows.back()->getProperties()["mrs-native-editor"]),"VST3 HWND bridge after offline insert rebuild");
        auto* pluginWindow=d.windows.back().get();
        RECT client{};GetClientRect(reinterpret_cast<HWND>(static_cast<std::intptr_t>(static_cast<juce::int64>(pluginWindow->getProperties()["mrs-native-host"]))),&client);
        check(std::abs(client.right-320)<=1&&std::abs(client.bottom-120)<=1,"native plugin physical dimensions");
        for(const double editorScale:{1.,1.5,2.}){pluginWindow->getPeer()->setCustomPlatformScaleFactor(editorScale);pluginWindow->fitNativeEditor(320,120);
            // A forced peer scale does not change Windows non-client metrics. Check the
            // JUCE content conversion here; real HWND client dimensions above use OS DPI.
            auto* content=pluginWindow->getContentComponent();if(std::abs(content->getWidth()*editorScale-320)>1||std::abs(content->getHeight()*editorScale-120)>1)throw std::runtime_error("native editor content DPI conversion "+std::to_string(editorScale)+" "+std::to_string(content->getWidth())+","+std::to_string(content->getHeight()));}
        pluginWindow->getPeer()->setCustomPlatformScaleFactor({});pluginWindow->fitNativeEditor(320,120);
        const auto count=d.windows.size();d.openInsert(second,plugin.id);check(d.windows.size()==count,"editor repeated-click reuse");
        d.closeEditors();
        auto fixStage=[](const char* text){juce::File::getCurrentWorkingDirectory().getChildFile("fix1-editor-stage.txt").replaceWithText(text);};fixStage("prepare second plugin");
        auto peerPlugin=plugin;peerPlugin.id=mrs::new_id();d.applyChain(std::nullopt,{peerPlugin});
        if(!d.getPeer())d.addToDesktop(0);
        fixStage("open pin");
        d.openInsert(second,plugin.id);auto* pinned=d.windows.back().get();
        auto button=[&](EditorWindow* w,const juce::String& id){for(int i=0;i<w->getContentComponent()->getNumChildComponents();++i)if(auto* b=dynamic_cast<juce::TextButton*>(w->getContentComponent()->getChildComponent(i));b&&b->getComponentID()==id)return b;return static_cast<juce::TextButton*>(nullptr);};
        fixStage("pin button");
        check(GetWindow(static_cast<HWND>(pinned->getPeer()->getNativeHandle()),GW_OWNER)==static_cast<HWND>(d.getPeer()->getNativeHandle()),"plugin editor is owned by MR Studio HWND");
        auto* pin=button(pinned,"plugin-pin");check(pin!=nullptr,"native editor exposes Pin");pin->setToggleState(true,juce::dontSendNotification);pin->onClick();
        {auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-plugin-pin-preview.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(pinned->getContentComponent()->createComponentSnapshot(pinned->getContentComponent()->getLocalBounds(),true,1.f,juce::SoftwareImageType{}),*stream);}}
        fixStage("other editor");
        d.openInsert(std::nullopt,peerPlugin.id);auto* other=d.windows.back().get();
        auto hostOf=[](EditorWindow* w){return reinterpret_cast<HWND>(static_cast<std::intptr_t>(static_cast<juce::int64>(w->getProperties()["mrs-native-host"])));};
        check(pinned->isVisible()&&other->isVisible()&&IsWindow(GetWindow(hostOf(pinned),GW_CHILD))&&IsWindow(GetWindow(hostOf(other),GW_CHILD)),"Pin permits two independent native editors");
        fixStage("bypass loops");
        for(int i=0;i<4;++i){const bool bypassBefore=d.chain(second).back().bypass;button(pinned,"plugin-bypass")->onClick();check(d.chain(second).back().bypass!=bypassBefore,"bypass button commits project state");render();pump();
            check(d.message.isEmpty()&&pinned->isVisible()&&other->isVisible()&&IsWindow(GetWindow(hostOf(pinned),GW_CHILD))&&IsWindow(GetWindow(hostOf(other),GW_CHILD)),"bypass reconnects both native views without white host");}
        fixStage("close other");
        other->closeButtonPressed();check(pinned->isVisible()&&IsWindow(GetWindow(hostOf(pinned),GW_CHILD)),"closing one editor retains pinned native view");
        fixStage("hover");
        SendMessageW(hostOf(pinned),WM_MOUSEMOVE,0,MAKELPARAM(5,5));SendMessageW(hostOf(pinned),WM_MOUSELEAVE,0,0);d.toFront(false);pump();
        check(pinned->isVisible()&&IsWindow(GetWindow(hostOf(pinned),GW_CHILD)),"hover and focus changes retain editor");
        fixStage("unpin");
        pin->setToggleState(false,juce::dontSendNotification);pin->onClick();d.openInsert(std::nullopt,peerPlugin.id);
        check(!pinned->isVisible(),"unpin restores close on opening another plugin");
        fixStage("cleanup");
        d.closeEditors();d.applyChain(std::nullopt,{});
        auto savedPreset=d.app.capture_insert(second,plugin.id);d.app.load_insert_preset(second,savedPreset);d.openInsert(second,plugin.id);
        auto* presetWindow=d.windows.back().get();auto host=reinterpret_cast<HWND>(static_cast<std::intptr_t>(static_cast<juce::int64>(presetWindow->getProperties()["mrs-native-host"])));
        auto child=GetWindow(host,GW_CHILD);check(child!=nullptr,"preset fixture native child");
        SendMessageW(child,WM_COMMAND,MAKEWPARAM(1,BN_CLICKED),0);d.app.poll();
        check(d.app.capture_insert(second,plugin.id).component_state!=savedPreset.component_state,"fixture edits DSP before preset load");
        const auto presetFile=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mrs-preset-load",".mrspreset",false);
        publishSettings(presetFile,encodePreset(savedPreset));d.refreshPresetLists();
        check(presetWindow->isVisible()&&IsWindow(child),"preset list refresh keeps native editor alive");
        const auto beforeLoad=d.app.services().projects->state().revision;d.loadPresetFile(second,plugin.id,presetFile);
        check(d.message.isEmpty()&&d.app.services().projects->state().revision==beforeLoad,"same snapshot preset restores DSP without a redundant history entry");
        check(presetWindow->isVisible()&&IsWindow(child)&&d.windows.back().get()==presetWindow,"preset load preserves editor HWND and window identity");
        check(d.app.capture_insert(second,plugin.id).component_state==savedPreset.component_state,"preset equal to project snapshot replaces edited live DSP");
        auto badPreset=savedPreset;badPreset.component_state={std::byte{0},std::byte{0},std::byte{0},std::byte{0x40}}; // fixture rejects gain 2.0
        publishSettings(presetFile,encodePreset(badPreset));d.loadPresetFile(second,plugin.id,presetFile);
        check(!d.message.isEmpty()&&IsWindow(child)&&d.app.capture_insert(second,plugin.id).component_state==savedPreset.component_state,"failed in-place preset restores previous DSP and retains editor");d.message.clear();
        SendMessageW(child,WM_COMMAND,MAKEWPARAM(1,BN_CLICKED),0);d.app.poll();const auto liveBeforeBuffer=d.app.capture_insert(second,plugin.id);
        check(liveBeforeBuffer.component_state!=savedPreset.component_state,"buffer regression starts with unsaved native editor edits");
        d.closeEditors();d.app.connect(std::make_unique<ManualDevice>(),{0,48000,256,{}, {0,1}});
        const auto liveAfterBuffer=d.app.capture_insert(second,plugin.id);
        check(liveAfterBuffer.component_state==liveBeforeBuffer.component_state&&liveAfterBuffer.controller_state==liveBeforeBuffer.controller_state,"buffer reconnect preserves live component/controller state");
        check(d.chain(second).back().component_state==liveBeforeBuffer.component_state,"buffer reconnect updates project plugin snapshot");
        presetFile.deleteFile();d.app.set_plugin_parameter(second,plugin.id,100,.25f);render();
    }
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mrs-juce-j2","",false);folder.createDirectory();
    const auto file=folder.getChildFile("roundtrip.mrsproject");
    const auto wav=folder.getChildFile("stereo.wav");
    {mrs::audio::Recorder recorder(std::filesystem::path(wav.getFullPathName().toWideCharPointer()),48000,0,{0,1});std::array<float,1024> samples{};
        for(std::size_t i=0;i<samples.size();i+=2){samples[i]=.25f;samples[i+1]=-.5f;}recorder.capture(samples.data(),2,512,0);recorder.finish();}
    d.browser->showFiles(true);d.browser->navigate(folder);
    check(!d.arrangement->isInterestedInFileDrag({folder.getFullPathName()})&&d.arrangement->isInterestedInFileDrag({wav.getFullPathName()}),"sample browser rejects folders and accepts WAV");
    auto* fileTree=dynamic_cast<juce::FileTreeComponent*>(d.browser->getChildComponent(d.browser->getNumChildComponents()-1));
    for(int i=0;i<d.browser->getNumChildComponents();++i)if(auto* candidate=dynamic_cast<juce::FileTreeComponent*>(d.browser->getChildComponent(i)))fileTree=candidate;
    check(fileTree!=nullptr,"file browser tree exists");
    for(int i=0;i<500&&d.browser->selectedSamples().isEmpty();++i){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}fileTree->setSelectedFile(wav);juce::Thread::sleep(1);}
    juce::DragAndDropTarget::SourceDetails sampleDrop("mrs-sample-files",fileTree,{400,120});
    check(d.arrangement->isInterestedInDragSource(sampleDrop),"internal WAV browser drag accepted");
    const auto tracksBeforeDrop=d.project()->tracks.size();const auto clipsBeforeDrop=d.project()->clips.size();const auto revisionBeforeDrop=d.app.services().projects->state().revision;
    d.arrangement->itemDropped(sampleDrop);
    check(d.project()->tracks.size()==tracksBeforeDrop&&d.project()->clips.size()==clipsBeforeDrop+1,"sample drop uses existing audio track");
    check(d.project()->clips.back().track==first&&d.project()->clips.back().start==d.arrangement->sampleAt(400),"sample drop follows track and cursor time");
    check(d.app.services().projects->state().revision==revisionBeforeDrop+1,"existing track import is one command");
    d.action(10);check(d.project()->clips.size()==clipsBeforeDrop&&d.project()->tracks.size()==tracksBeforeDrop,"sample import undo restores clips without removing track");
    d.arrangement->filesDropped({wav.getFullPathName()},400,70+static_cast<int>(tracksBeforeDrop)*d.arrangement->trackHeight+4);
    fileTree->setSelectedFile(wav);
    {const auto until=juce::Time::getMillisecondCounter()+2000;while(!d.browser->samplePreviewReady()&&juce::Time::getMillisecondCounter()<until){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}}
    check(d.browser->samplePreviewReady(),"background WAV metadata and waveform preview completes");
    {auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-files-preview.png").createOutputStream();juce::PNGImageFormat format;if(stream){stream->setPosition(0);stream->truncate();format.writeImageToStream(d.createComponentSnapshot(d.getLocalBounds(),true,1.f,juce::SoftwareImageType{}),*stream);}}
    d.browser->showFiles(false);check(d.project()->tracks.size()==4,"WAV import track");
    const auto imported=d.project()->clips.back();
    d.saveFile(file);const auto saved=mrs::persistence::load_project(d.app.path()).project;d.app.rename_track(second,"Changed");d.openFile(juce::File(juce::String(d.app.path().wstring().c_str())));
    check(*d.project()==saved,"project roundtrip");
    check(d.projectTitle.getText().startsWith("roundtrip"),"saved filename displayed in menu bar");
    auto other=mrs::persistence::load_project(d.app.path());other.project.title="Different session";other.project.tracks[0].name="Different track";
    const auto alternate=juce::File(juce::String(d.app.path().wstring().c_str())).getSiblingFile("other.mrsproject");mrs::persistence::save_project(std::filesystem::path(alternate.getFullPathName().toWideCharPointer()),other);
    auto originalPath=d.app.path();d.openFile(alternate);
    check(d.projectTitle.getText()=="other"&&d.arrangement->rows.front()->gain.getTitle().contains("Different track"),"same track IDs in another project refresh title and channel views");
    d.openFile(juce::File(juce::String(originalPath.wstring().c_str())));
    std::string source;for(const auto& c:d.project()->clips)if(c.name==imported.name)source=c.source;
    for(int i=0;i<500&&!d.app.waveform(source);++i){d.app.poll();juce::Thread::sleep(1);}
    for(const auto& c:d.project()->clips)if(c.name=="stereo.wav"){const auto* wave=d.app.waveform(c.source);check(wave&&wave->channels()==2,"stereo waveform channels");check(wave->range(0,512,0).maximum>.2f&&wave->range(0,512,1).minimum<-.4f,"independent L/R waveform");}
    d.app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});d.app.seek(4800);render();d.action(30);render();d.action(32);render();
    check(d.app.engine()->state().sample==4800,"stop returns to start");
    d.spaceKey(true);render();check(d.app.engine()->state().playback==mrs::PlaybackState::playing,"space starts");d.spaceKey(true);render();check(d.app.engine()->state().playback==mrs::PlaybackState::playing,"space repeat rejected");
    d.spaceKey(false);d.spaceKey(true);render();check(d.app.engine()->state().playback==mrs::PlaybackState::stopped,"space stops");d.spaceKey(false);
    d.selectedClip=d.project()->clips.front().id;const auto clips=d.project()->clips.size();check(d.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey)),"Backspace binding handled");check(d.project()->clips.size()+1==clips,"Backspace deletes selected clip");d.action(10);
    d.setSize(1200,700);check(d.arrangeArea.getWidth()>0&&d.mixArea.getHeight()>0,"minimum layout");d.setSize(1400,850);d.arrangement->fit();
    d.app.play();for(int i=0;i<8;++i)render();d.app.stop();render();
    const auto meter=d.app.engine()->take_meters();d.peaks=meter.tracks;d.masterPeak=meter.master;
    for(const auto& clip:d.project()->clips){for(int i=0;i<1000&&!d.app.waveform(clip.source);++i){d.app.poll();juce::Thread::sleep(1);}check(d.app.waveform(clip.source)!=nullptr,"visible waveform prepared");}
    for(const auto& clip:d.project()->clips)if(clip.source=="mrs:demo-tone")check(d.app.waveform(clip.source)->range(0,48000,0).maximum>.05f,"demo waveform data");
    // Fixture is under a unique temporary directory created by this test only.
    check(folder.getParentDirectory()==juce::File::getSpecialLocation(juce::File::tempDirectory)&&folder.getFileName().startsWith("mrs-juce-j2"),"fixture cleanup scope");folder.deleteRecursively();
}
void j3Smoke(Desktop& d){
    auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    mrs::audio::DeviceStatus performance{mrs::audio::DevicePhase::running,48000,2.7,5.4,.5,{}};
    d.updatePerformance(performance,true);check(d.cpuReadout.getText()=="CPU 50.0%"&&d.audioCpu==.5&&d.latencyReadout.getText()=="Latency I 2.70 / O 5.40 ms","driver load and latency readouts");
    performance.cpu_load=1.25;d.updatePerformance(performance,true);check(d.cpuReadout.getText()=="CPU 125.0%"&&d.audioCpu==1.,"overload readout exceeds 100 while bar saturates");
    d.updatePerformance(performance,false);check(d.cpuReadout.getText()=="CPU --%"&&d.latencyReadout.getText().contains("--")&&d.audioCpu==0,"offline clock does not claim measured hardware CPU or latency");
    j3AudioSmoke(d);profilingSmoke(d);
    auto dividerEvent=[&](juce::Point<float> point){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&d.browserDivider,&d.browserDivider,juce::Time::getCurrentTime(),{4,20},juce::Time::getCurrentTime(),1,true);};
    d.mouseDown(dividerEvent({4,20}));d.mouseDrag(dividerEvent({-336,20}));d.mouseUp(dividerEvent({4,20}));check(d.browserWidth==604,"browser divider drag uses local coordinates");
    d.resizeBrowser(600);check(d.browserWidth==600&&d.browserDivider.getBounds().getRight()==d.browser->getX(),"browser widens with usable divider");
    d.resizeBrowser(2000);check(d.browserWidth<=700&&d.arrangeArea.getWidth()>=680,"browser maximum keeps arrangement usable");d.resizeBrowser(260);
    const auto mixerBefore=d.mixArea.getHeight();const auto mixerSetting=d.view.mixerHeight;
    auto mixerEvent=[&](juce::Point<float> point){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&d.mixerDivider,&d.mixerDivider,juce::Time::getCurrentTime(),{30,2},juce::Time::getCurrentTime(),1,true);};
    d.mouseDown(mixerEvent({30,2}));d.mouseDrag(mixerEvent({30,-48}));d.mouseUp(mixerEvent({30,2}));
    check(d.mixArea.getHeight()>mixerBefore&&d.arrangeArea.getHeight()>=140,"mixer divider grows dock upward while retaining arrangement");d.resizeMixer(mixerSetting);

    auto oldCatalog=d.catalog;d.catalog={{"a.vst3","00000000000000000000000000000001","A","Vendor A","1"},{"b.vst3","00000000000000000000000000000002","B","Vendor A","1"},{"c.vst3","00000000000000000000000000000003","C","Vendor B","1"}};
    auto vendorMenu=d.pluginMenu();check(vendorMenu.getNumItems()==2,"insert menu grouped by vendor");d.browser->rebuild();
    {auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-vendors-preview.png").createOutputStream();juce::PNGImageFormat format;if(stream){stream->setPosition(0);stream->truncate();format.writeImageToStream(d.browser->createComponentSnapshot(d.browser->getLocalBounds(),true,1.f,juce::SoftwareImageType{}),*stream);}}
    d.catalog=oldCatalog;d.browser->rebuild();
    for(const auto& track:d.project()->tracks)for(const auto& fx:track.inserts){
        if(fx.kind!=mrs::InsertKind::channel_eq&&fx.kind!=mrs::InsertKind::vst3)continue;
        auto captured=d.app.capture_insert(track.id,fx.id);auto text=encodePreset(captured);auto loaded=decodePreset(text,fx);
        check(loaded.id==fx.id&&loaded.component_state==captured.component_state&&loaded.bands==captured.bands,"native/VST preset state roundtrip");
        auto another=fx;another.id=mrs::new_id();another.bypass=!fx.bypass;auto remapped=decodePreset(text,another);
        check(remapped.id==another.id&&remapped.bypass==another.bypass,"preset slot identity and bypass preserved");
        if(fx.kind==mrs::InsertKind::vst3){another.class_id="00000000000000000000000000000000";bool rejected=false;try{decodePreset(text,another);}catch(...){rejected=true;}check(rejected,"wrong plugin preset rejected");}
    }
    {
        auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);auto folder=temp.getNonexistentChildFile("mrs-preset-library","",false);
        mrs::NativeInsert effect;effect.id=mrs::new_id();effect.kind=mrs::InsertKind::gain;effect.gain=.25f;
        auto pluginFolder=presetFolder(effect,folder);check(pluginFolder.createDirectory().wasOk(),"preset library folder created");
        publishSettings(pluginFolder.getChildFile("Clean.mrspreset"),encodePreset(effect));
        auto files=presetFiles(effect,folder);check(files.size()==1&&decodePreset(files[0].loadFileAsString(),effect).gain==.25f,"saved preset discoverable and reloadable");
        auto other=effect;other.kind=mrs::InsertKind::channel_eq;check(presetFiles(other,folder).isEmpty(),"preset library separates processors");
        check(folder.getParentDirectory()==temp&&folder.getFileName().startsWith("mrs-preset-library"),"preset cleanup scope");folder.deleteRecursively();
    }
    d.addToDesktop(0);
    auto* handler=d.mixer.front()->gain.getAccessibilityHandler();check(handler&&handler->getRole()==juce::AccessibilityRole::slider&&!handler->getTitle().isEmpty(),"named accessible fader");
    auto* value=handler->getValueInterface();check(value&&value->getRange().isValid(),"accessible fader range");
    auto history=d.app.services().projects->state().revision;value->setValue(.7);check(d.app.services().projects->state().revision==history+1,"UI Automation fader uses one shared command");d.action(10);
    juce::TextEditor text;d.addChildComponent(text);auto count=d.project()->tracks.size();check(!d.shortcutAllowedFor(&text),"text input blocks global shortcuts");
    juce::Component external;check(!d.shortcutAllowedFor(&external),"editor window blocks arrangement shortcuts");check(d.shortcutAllowedFor(&d.mixer.front()->gain),"mixer allows transport shortcuts");d.removeChildComponent(&text);
    check(!d.keyPressed(juce::KeyPress('R',juce::ModifierKeys::ctrlModifier,0))&&d.project()->tracks.size()==count,"unassigned modified shortcuts ignored");
    ViewSettings settings;settings.sidebar=false;settings.snap=true;settings.browserWidth=440;settings.trackHeight=320;settings.mixerHeight=440;settings.pixelsPerSecond=1200;settings.inputs="1,2";
    auto decoded=ViewSettings::decode(settings.encode());check(!decoded.sidebar&&decoded.snap&&decoded.browserWidth==440&&decoded.trackHeight==320&&decoded.pixelsPerSecond==1200&&decoded.inputs=="1,2","view settings roundtrip");
    check(decoded.mixerHeight==440,"mixer dock height settings roundtrip");
    bool rejected=false;try{ViewSettings::decode("{broken}");}catch(...){rejected=true;}check(rejected,"malformed settings rejected");
    auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mrs-j3-settings",".json",false);
    publishSettings(file,settings.encode());publishSettings(file,settings.encode());check(ViewSettings::decode(file.loadFileAsString()).inputs=="1,2","atomic settings publication");file.deleteFile();
    for(const auto scale:{1.f,1.5f,2.f}){auto image=d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{});check(image.getWidth()==juce::roundToInt(d.getWidth()*scale)&&image.getHeight()==juce::roundToInt(d.getHeight()*scale),"DPI snapshot dimensions");}
    d.setSize(1200,700);d.sidebar=true;d.browserWidth=450;d.resized();check(d.arrangeArea.getWidth()>250&&d.master->getBounds().getRight()<=d.arrangeArea.getRight(),"minimum layout with widest browser");
    d.browserWidth=260;d.setSize(1400,850);d.resized();d.removeFromDesktop();
}
void thuDiagnostic(const juce::File& file){
    auto project=mrs::persistence::load_project(std::filesystem::path(file.getFullPathName().toWideCharPointer())).project;
    std::vector<mrs::NativeInsert> effects=project.master_inserts;for(const auto& track:project.tracks)effects.insert(effects.end(),track.inserts.begin(),track.inserts.end());
    auto found=std::find_if(effects.begin(),effects.end(),[](const auto& n){return n.kind==mrs::InsertKind::vst3&&n.plugin_name=="TH-U";});if(found==effects.end())throw std::runtime_error("TH-U insert not found in project");
    auto graph=mrs::processing::insert_graph(std::array{*found});auto node=graph.nodes.front();
    auto run=[&](bool activeRestore){auto plugin=mrs::processing::vst3_factory(node);plugin->prepare({project.sample_rate,2,128});
        if(activeRestore){plugin->warm();plugin->restore(node.plugin);}else {plugin->restore(node.plugin);plugin->warm();}
        auto captured=plugin->capture();juce::String result="component bytes="+juce::String(static_cast<int>(captured.component.size()))+" exact="+juce::String(captured.component==node.plugin.component?1:0);
        std::array<float,256> audio{};mrs::processing::MidiBuffer midi;double energy=0;std::vector<float> rendered;rendered.reserve(256*256);
        for(int block=0;block<256;++block){for(int sample=0;sample<128;++sample){const auto time=static_cast<float>(block*128+sample);audio[sample*2]=audio[sample*2+1]=.035f*std::sin(time*.0288f)+.015f*std::sin(time*.087f);}
            plugin->process({audio,128,2,{}, {},midi,block*128,true,120,0});for(auto value:audio){if(!std::isfinite(value))throw std::runtime_error("TH-U non-finite audio");energy+=value*value;rendered.push_back(value);}}
        result+=" RMS="+juce::String(std::sqrt(energy/rendered.size()),9);return std::pair{result,rendered};};
    auto before=run(false),repeatBefore=run(false),after=run(true),repeatAfter=run(true);
    auto difference=[](const auto& left,const auto& right){double error=0,reference=0;for(std::size_t i=0;i<left.second.size();++i){auto diff=left.second[i]-right.second[i];error+=diff*diff;reference+=right.second[i]*right.second[i];}return std::sqrt(error/juce::jmax(1.e-20,reference));};
    auto log="Normal prepare / restore / warm: "+before.first+"\nExplicit restore after warm: "+after.first+"\nRelative audio difference: "+juce::String(difference(before,after),9)+"\nRepeat restore-first difference: "+juce::String(difference(before,repeatBefore),9)+"\nRepeat activate-first difference: "+juce::String(difference(after,repeatAfter),9)+"\n";
    juce::File::getCurrentWorkingDirectory().getChildFile("thu-audio-diagnostic.log").replaceWithText(log);
}
void j2PluginSmoke(Desktop& d,const juce::File& module){
    auto check=[](bool b,const char* text){if(!b)throw std::runtime_error(text);};
    const auto log=juce::File::getCurrentWorkingDirectory().getChildFile("juce-plugin-smoke.log");log.replaceWithText(module.getFullPathName()+"\n");
    auto stage=[&](const char* s){log.appendText(juce::String(s)+"\n");};
    d.app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});
    stage("probe");
    d.catalog=mrs::processing::probe_vst3(module.getFullPathName().toStdString());check(!d.catalog.empty(),"external probe");
    d.resetDevice();
    stage("load offline");auto track=d.project()->tracks.front().id;d.addPlugin(track,0);check(d.chain(track).size()==1,"external load");auto slot=d.chain(track).front().id;
    stage("open editor");
    d.openInsert(track,slot);check(!d.windows.empty()&&static_cast<bool>(d.windows.back()->getProperties()["mrs-native-editor"]),"external native editor");
    stage("reuse and close editor");const auto count=d.windows.size();d.openInsert(track,slot);check(d.windows.size()==count,"external editor reuse");
    auto pump=[] {const auto until=juce::Time::getMillisecondCounter()+80;while(juce::Time::getMillisecondCounter()<until){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}};
    pump();check(d.windows.back()->isVisible()&&static_cast<bool>(d.windows.back()->getProperties()["mrs-native-editor"]),"external editor survives offline UI timer");
    d.closeEditors();
    std::array<float,256> audio{};d.app.play();for(int i=0;i<4;++i){d.app.engine()->process(nullptr,audio.data(),128);d.app.poll();}d.app.stop();d.app.engine()->process(nullptr,audio.data(),128);d.app.poll();
    check(std::all_of(audio.begin(),audio.end(),[](float v){return std::isfinite(v);}),"external finite process");
    stage("capture project");const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);auto folder=temp.getNonexistentChildFile("mrs-juce-plugin","",false);folder.createDirectory();d.saveFile(folder.getChildFile("plugin.mrsproject"));
    const auto saved=mrs::persistence::load_project(d.app.path()).project;check(!saved.tracks.front().inserts.front().component_state.empty(),"opaque component capture");
    pump();stage("reopen project");d.openFile(juce::File(d.app.path().wstring().c_str()));check(d.project()->tracks.front().inserts.front().component_state==saved.tracks.front().inserts.front().component_state,"opaque component reopen");
    stage("reopen editor");
    d.openInsert(track,slot);check(static_cast<bool>(d.windows.back()->getProperties()["mrs-native-editor"]),"external reopened editor offline");pump();check(d.windows.back()->isVisible(),"external reopened editor remains visible");d.closeEditors();
    check(folder.getParentDirectory()==temp&&folder.getFileName().startsWith("mrs-juce-plugin"),"external cleanup scope");folder.deleteRecursively();
    stage("passed");
}
}
