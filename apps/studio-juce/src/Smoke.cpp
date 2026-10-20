#include "MeterTrack.h"
#include "MusicalGrid.h"
#include "PianoRoll.h"
#include "ChordTrack.h"
#include "SectionTrack.h"
#include "TempoTrack.h"
#include "MarkerTrack.h"
#include "ControllerEditor.h"
#include "../../../tests/audio_state_test_access.hpp"
#include "J1Smoke.h"
#include "PluginPreset.h"
#include "ProjectHome.h"
#include "WaveformView.h"
#include <windows.h>
#include <thread>
#include <chrono>
namespace ui {
namespace {
void referencePreview(juce::Component& component,const juce::String& name){
    for(float scale:{1.f,1.5f}){auto output=juce::File::getCurrentWorkingDirectory().getChildFile("juce-"+name+"-"+juce::String(static_cast<int>(scale*100))+"-preview.png").createOutputStream();
        if(!output||!output->setPosition(0)||output->truncate().failed()||!juce::PNGImageFormat().writeImageToStream(component.createComponentSnapshot(component.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*output))throw std::runtime_error("reference preview export");}
}
}

void arrangeFinalGesturesSmoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    struct Clock final:mrs::audio::IAudioDevice{
        std::vector<mrs::audio::DeviceInfo> enumerate()override{return {{0,"Arrange fixture",{}, {"L","R"},16,8192,128,1}};}
        void control_panel(int)override{}void open(const mrs::audio::DeviceConfig&,std::shared_ptr<mrs::audio::AudioEngine>)override{}
        void start()override{}void stop()override{}void close()noexcept override{}mrs::audio::DeviceStatus status()override{return {mrs::audio::DevicePhase::running,48000};}
    };
    Desktop d(true);d.app.new_project();const auto instrument=d.app.add_instrument_track("Gesture MIDI");const auto midi=d.app.create_midi_clip(instrument,0,4*mrs::ppq);
    d.app.connect(std::make_unique<Clock>(),{0,48000,128,{}, {0,1}});d.setWorkspace(mrs::desktop::Workspace::arrange);d.refresh(true);d.setSize(1200,700);d.setVisible(true);d.snap=false;
    auto& a=*d.arrangement;a.pixelsPerSecond=100.;
    auto mouse=[&](juce::Point<float> point,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,&a,&a,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,true);};
    std::array<float,256> out{};auto flush=[&]{d.app.engine()->process(nullptr,out.data(),128);};
    a.mouseDown(mouse({450,8},juce::ModifierKeys::leftButtonModifier));flush();const auto rulerSample=d.app.engine()->state().sample;check(rulerSample==a.seekSampleAt(450),"ruler click seeks");
    a.mouseDrag(mouse({480,8},juce::ModifierKeys::leftButtonModifier));flush();check(d.app.engine()->state().sample==a.seekSampleAt(480),"ruler drag seeks");
    const auto beforeBody=d.app.engine()->state().sample;a.mouseDrag(mouse({550,100},juce::ModifierKeys::leftButtonModifier));a.mouseUp(mouse({550,100},0));flush();check(d.app.engine()->state().sample==beforeBody,"ruler drag outside ruler does not seek");
    for(float y:{32.f,52.f,230.f}){a.mouseDown(mouse({750,y},juce::ModifierKeys::leftButtonModifier));a.mouseUp(mouse({750,y},0));flush();check(d.app.engine()->state().sample==beforeBody,"section/chord/empty body clicks do not seek");}
    auto stored=[&](mrs::Id id){const auto p=d.project();return *std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});};
    auto resize=[&](mrs::Id id,bool right,float shift,bool cancel=false){
        d.selectedClip=id;d.selectedClips={id.value};const auto c=stored(id);const auto r=a.clipRect(c);const juce::Point<float> point{right?r.getRight()-2:r.getX()+2,r.getCentreY()};
        check(a.edgeAt(c,point)==(right?1:-1),"audio/MIDI edge hit area");a.mouseMove(mouse(point,0));check(!(a.getMouseCursor()==juce::MouseCursor(juce::MouseCursor::NormalCursor)),"bracket hover replaces normal pointer");
        const auto revision=d.app.services().projects->state().revision;a.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));a.mouseDrag(mouse(point.translated(shift,0),juce::ModifierKeys::leftButtonModifier));check(d.app.services().projects->state().revision==revision,"edge drag is preview only");
        if(cancel)a.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));a.mouseUp(mouse(point.translated(shift,0),0));
    };
    const auto initial=stored(midi);resize(midi,true,900);if(stored(midi).midi->length<=initial.midi->length*2)throw std::runtime_error("MIDI resize length "+std::to_string(stored(midi).midi->length)+", initial "+std::to_string(initial.midi->length)+", message: "+d.message.toStdString());d.action(10);check(stored(midi)==initial,"one Undo restores MIDI edge trim");
    resize(midi,true,-50,true);check(stored(midi)==initial,"Escape cancels MIDI edge preview");
    d.app.move_clip(midi,instrument,mrs::Timeline(d.project()->time,48000).to_samples(4*mrs::ppq));d.refresh();resize(midi,false,-1000);
    check(stored(midi).midi->start==0&&stored(midi).midi->length==8*mrs::ppq,"MIDI left edge extends beyond source toward timeline zero");d.action(10);d.action(10);check(stored(midi)==initial,"MIDI left extension and move each have one Undo");
    d.selectedClip=midi;d.selectedClips={midi.value};d.app.seek(mrs::Timeline(d.project()->time,48000).to_samples(mrs::ppq));flush();d.action(24);const auto midiRight=*d.selectedClip;
    check(midiRight!=midi&&d.selectedClips==std::set<std::string>{midiRight.value},"MIDI split selects only right clip");d.action(10);
    d.app.demo();d.app.connect(std::make_unique<Clock>(),{0,48000,128,{}, {0,1}});d.refresh(true);a.pixelsPerSecond=8.;a.horizontal=0;const auto audio=d.project()->clips.front().id;const auto original=stored(audio);
    resize(audio,true,-40);check(stored(audio).length<original.length,"audio edge shortens");resize(audio,true,900);check(stored(audio)==original&&d.message.isEmpty(),"audio extension clamps to original source without error");
    resize(audio,false,32);check(stored(audio).source_offset>0,"audio left trim advances source offset");resize(audio,false,-900);check(stored(audio)==original,"audio left edge restores original source only");
    const auto other=d.app.add_instrument_track("Other selected");const auto otherClip=d.app.create_midi_clip(other,0);d.refresh(true);d.selectedClip=audio;d.selectedClips={audio.value,otherClip.value};
    const auto edge=a.clipRect(stored(audio)).getTopRight().translated(-2,30);a.mouseDown(mouse(edge,juce::ModifierKeys::leftButtonModifier));check(d.selectedClips==std::set<std::string>{audio.value,otherClip.value},"edge trim retains group selection");a.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
    d.selectedClips={audio.value};d.selectedClip=audio;d.app.seek(48000);flush();d.action(24);const auto audioRight=*d.selectedClip;check(audioRight!=audio&&d.selectedClips==std::set<std::string>{audioRight.value},"audio split selects only right clip");
    const auto before=stored(audioRight);resize(audioRight,true,-24);const auto shortened=stored(audioRight);check(shortened.length<before.length,"split audio right trim");d.action(10);check(stored(audioRight)==before,"one Undo restores audio trim");d.action(11);check(stored(audioRight)==shortened,"Redo restores audio trim");
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory),folder=temp.getNonexistentChildFile("mrs-upd8-gestures","",false);const auto file=folder.getChildFile("Gestures.mrsproject");
    d.app.save_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));d.app.open_project(d.app.path());d.refresh(true);check(stored(audioRight)==shortened,"trim survives Save/Open");d.app.disconnect();d.app.new_project();
    check(folder.getParentDirectory()==temp&&folder.getFileName().startsWith("mrs-upd8-gestures"),"gesture cleanup scope");folder.deleteRecursively();
}
void arrangeAudioSmoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    Desktop d(true);d.app.disconnect();d.app.new_project();auto initial=d.project()->time;initial.tempos={{0,120}};initial.meters={{1,4,4}};d.app.set_time_map(initial);d.app.add_audio_track("Audio");d.refresh(true);d.setSize(1200,700);d.setVisible(true);
    auto& a=*d.arrangement;const mrs::Timeline time(d.project()->time,d.project()->sample_rate);
    for(double zoom:{30.,123.45,2400.})for(double scroll:{0.,37.25}){
        a.pixelsPerSecond=zoom;a.horizontal=scroll;const auto bar=time.to_samples(4*mrs::ppq);
        const int x=250+static_cast<int>(static_cast<double>(bar)/d.project()->sample_rate*zoom-scroll);
        d.snap=false;for(int dx:{-6,-3,0,3,6})check(a.seekSampleAt(static_cast<float>(x+dx))==bar,"Snap off retains six-pixel bar capture");
        d.snap=true;for(int dx:{-6,-3,0,3,6}){const auto target=time.to_ticks(a.seekSampleAt(static_cast<float>(x+dx)));check(a.snapTick(target)==target,"Smart ruler captures a displayed grid tick");}check(a.seekSampleAt(static_cast<float>(x))==bar,"Smart ruler bar pixel remains exact");
    }
    a.pixelsPerSecond=240.;a.horizontal=0;d.snap=false;
    check(a.seekSampleAt(279.5f)==5900,"Snap off exact off-grid sample instead of truncation to ticks");
    d.snap=true;check(a.seekSampleAt(279.5f)==6000,"Smart Snap chooses nearest visible division");
    auto map=d.project()->time;map.tempos={{0,120},{4*mrs::ppq,90}};map.meters={{1,4,4},{3,3,4}};d.app.set_time_map(map);
    const mrs::Timeline changed(map,d.project()->sample_rate);const auto tick=changed.to_ticks(mrs::MusicalPosition{4,1,0}),sample=changed.to_samples(tick);
    a.horizontal=777.25;const int pixel=250+static_cast<int>(static_cast<double>(sample)/d.project()->sample_rate*a.pixelsPerSecond-a.horizontal);
    d.snap=false;check(a.seekSampleAt(static_cast<float>(pixel-5))==sample&&a.seekSampleAt(static_cast<float>(pixel+5))==sample,"Snap off bar capture follows tempo and meter changes");
    d.snap=true;for(int dx:{-5,0,5}){const auto snapped=changed.to_ticks(a.seekSampleAt(static_cast<float>(pixel+dx)));check(a.snapTick(snapped)==snapped,"Smart ruler follows changed tempo and meter grid");}
    a.horizontal=0;a.resized();const auto image=a.createComponentSnapshot(a.getLocalBounds(),true,1.,juce::SoftwareImageType{});
    check(image.getPixelAt(250,5)==juce::Colours::white&&image.getPixelAt(250,48)==juce::Colours::white,"playhead spans ruler and chord track");
    check(a.getComponentAt(250,5)==&a&&a.getComponentAt(250,80)==a.chords.get()&&a.getComponentAt(250,24)==a.markers.get(),
        "extended playhead is transparent to ruler and editable chord lane");
    d.app.demo();d.selectedClip.reset();d.selectedClips.clear();d.setWorkspace(mrs::desktop::Workspace::arrange);d.refresh(true);a.pixelsPerSecond=240.;a.horizontal=0;
    const auto source=d.project()->clips.front().source;
    for(int attempt=0;attempt<200&&!d.app.waveform(source);++attempt){std::this_thread::sleep_for(std::chrono::milliseconds(5));d.app.poll();}
    check(d.app.waveform(source)!=nullptr,"arrangement waveform prepared before visual check");
    const auto clip=a.clipRect(d.project()->clips.front());const auto translucent=a.createComponentSnapshot(a.getLocalBounds(),true,1.,juce::SoftwareImageType{});
    const auto fill=juce::Colour(arrangementCanvas).overlaidWith(juce::Colour(0xff294d7e).withAlpha(.72f));
    const auto actual=translucent.getPixelAt(static_cast<int>(clip.getX()+17),static_cast<int>(clip.getY()+21));
    if(std::abs(actual.getRed()-fill.getRed())>2||std::abs(actual.getGreen()-fill.getGreen())>2||std::abs(actual.getBlue()-fill.getBlue())>2)throw std::runtime_error("audio clip opacity: got "+actual.toString().toStdString()+", expected "+fill.toString().toStdString());
    for(float scale:{1.f,1.5f}){auto picture=d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-arrange-upd8-"+juce::String(static_cast<int>(scale*100))+"-preview.png").createOutputStream();check(stream!=nullptr,"arrangement preview stream");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(picture,*stream),"arrangement snapshot");}
    struct Preview final:juce::Component{
        mrs::audio::Waveform peaks;Preview():peaks(mrs::audio::AudioData{48000,1,[]{std::vector<float> s(512);for(int i=0;i<512;++i)s[i]=static_cast<float>(std::sin(i*.03)*(.2+i/700.));return s;}()}){}
        void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(arrangementCanvas));g.setColour(juce::Colours::skyblue);drawWaveform(g,peaks,getLocalBounds().toFloat(),0,.5);}
    } preview;preview.setSize(1024,180);
    for(float scale:{1.f,1.5f}){auto picture=preview.createComponentSnapshot(preview.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-waveform-smooth-"+juce::String(static_cast<int>(scale*100))+"-preview.png").createOutputStream();check(stream!=nullptr,"smooth waveform preview stream");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(picture,*stream),"smooth waveform snapshot");}
}
void tempo83Smoke(){
    const auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    struct Clock final:mrs::audio::IAudioDevice{std::vector<mrs::audio::DeviceInfo> enumerate()override{return {{0,"Tempo GUI fixture",{}, {"L","R"},16,8192,128,1}};}void control_panel(int)override{}void open(const mrs::audio::DeviceConfig&,std::shared_ptr<mrs::audio::AudioEngine>)override{}void start()override{}void stop()override{}void close()noexcept override{}mrs::audio::DeviceStatus status()override{return {mrs::audio::DevicePhase::running,48000};}};
    Desktop d(true);d.app.new_project();d.app.add_instrument_track("Tempo MIDI");d.app.connect(std::make_unique<Clock>(),{0,48000,128,{}, {0,1}});d.setVisible(true);d.refresh(true);auto& lane=*d.arrangement->tempos;d.arrangement->pixelsPerSecond=120;
    auto pump=[](){for(int n=0;n<10;++n){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}};
    auto field=[&](juce::String name){auto* form=lane.popupEditor();check(form!=nullptr,"tempo popup opens");for(auto* child:form->getChildren())if(auto* editor=dynamic_cast<juce::TextEditor*>(child))if(editor->getName()==name)return editor;throw std::runtime_error("tempo field missing");};
    auto save=[&]{for(auto* child:lane.popupEditor()->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()=="Apply"){button->onClick();pump();return;}throw std::runtime_error("tempo Apply missing");};
    lane.edit(mrs::TempoPoint{0,120});check(field("Position")->isReadOnly(),"tempo anchor cannot move");field("BPM")->setText("125");save();check(d.project()->time.tempos.front().bpm==125&&d.message.isEmpty(),"tempo anchor BPM editable");d.action(10);check(d.project()->time.tempos.front().bpm==120,"tempo BPM Undo");
    lane.edit({},4*mrs::ppq);field("BPM")->setText("90");save();lane.edit({},8*mrs::ppq);field("BPM")->setText("140");save();check(d.project()->time.tempos==std::vector<mrs::TempoPoint>{{0,120},{4*mrs::ppq,90},{8*mrs::ppq,140}},"120/90/140 GUI map");
    const auto map=d.project()->time;const auto revision=d.app.services().projects->state().revision;lane.edit(map.tempos[1]);field("Position")->setText("1:1:0");save();check(d.project()->time==map&&d.app.services().projects->state().revision==revision&&!d.message.isEmpty(),"tempo duplicate atomic");
    lane.edit(map.tempos[1]);referencePreview(*lane.popupEditor()->getTopLevelComponent(),"tempo83-editor");field("BPM")->setText("0");save();check(d.project()->time==map,"invalid BPM atomic");
    auto mouse=[&](juce::Point<float> p,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(mods),1,0,0,0,0,&lane,&lane,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,true);};
    const auto point=juce::Point<float>{lane.xAt(4*mrs::ppq)+2,12};lane.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse(point.translated(24,0),juce::ModifierKeys::leftButtonModifier));check(d.project()->time==map&&lane.dragging(),"tempo gesture preview only");lane.mouseUp(mouse(point.translated(24,0),0));check(d.project()->time!=map&&d.message.isEmpty(),"tempo point drag commits");d.action(10);check(d.project()->time==map,"tempo drag Undo");
    lane.mouseDown(mouse({lane.xAt(0)+2,12},juce::ModifierKeys::leftButtonModifier));lane.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey));check(d.project()->time==map&&!d.message.isEmpty(),"anchor delete refused");
    d.message.clear();d.refresh();referencePreview(d,"tempo83");
}
void meter84Smoke(){
    const auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    struct Clock final:mrs::audio::IAudioDevice{std::vector<mrs::audio::DeviceInfo> enumerate()override{return {{0,"Meter GUI fixture",{}, {"L","R"},16,8192,128,1}};}void control_panel(int)override{}void open(const mrs::audio::DeviceConfig&,std::shared_ptr<mrs::audio::AudioEngine>)override{}void start()override{}void stop()override{}void close()noexcept override{}mrs::audio::DeviceStatus status()override{return {mrs::audio::DevicePhase::running,48000};}};
    Desktop d(true);d.app.new_project();d.app.add_instrument_track("Meter MIDI");d.app.connect(std::make_unique<Clock>(),{0,48000,128,{}, {0,1}});d.setVisible(true);d.refresh(true);auto& lane=*d.arrangement->meters;d.arrangement->pixelsPerSecond=120;
    auto pump=[](){for(int n=0;n<10;++n){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}};
    auto field=[&](juce::String name){auto* form=lane.popupEditor();check(form!=nullptr,"meter popup opens");for(auto* child:form->getChildren())if(auto* editor=dynamic_cast<juce::TextEditor*>(child))if(editor->getName()==name)return editor;throw std::runtime_error("meter field missing");};
    auto save=[&]{for(auto* child:lane.popupEditor()->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()=="Apply"){button->onClick();pump();return;}throw std::runtime_error("meter Apply missing");};
    lane.edit(mrs::MeterPoint{1,4,4});check(field("Bar")->isReadOnly(),"meter anchor cannot move");field("Numerator")->setText("3");save();check(d.project()->time.meters.front().numerator==3&&d.message.isEmpty(),"meter anchor signature editable");d.action(10);check(d.project()->time.meters.front().numerator==4,"meter signature Undo");
    lane.edit({},3);field("Numerator")->setText("6");field("Denominator")->setText("8");save();lane.edit({},5);field("Numerator")->setText("3");field("Denominator")->setText("4");save();check(d.project()->time.meters==std::vector<mrs::MeterPoint>{{1,4,4},{3,6,8},{5,3,4}},"4/4 6/8 3/4 GUI map");
    const auto map=d.project()->time;const auto revision=d.app.services().projects->state().revision;lane.edit(map.meters[1]);field("Bar")->setText("1");save();check(d.project()->time==map&&d.app.services().projects->state().revision==revision&&!d.message.isEmpty(),"meter duplicate atomic");
    lane.edit(map.meters[1]);referencePreview(*lane.popupEditor()->getTopLevelComponent(),"meter84-editor");field("Denominator")->setText("3");save();check(d.project()->time==map,"invalid denominator atomic");
    auto mouse=[&](juce::Point<float> p,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(mods),1,0,0,0,0,&lane,&lane,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,true);};
    const auto point=juce::Point<float>{lane.xAt(3)+2,12};lane.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse(point.translated(180,0),juce::ModifierKeys::leftButtonModifier));check(d.project()->time==map&&lane.dragging(),"meter gesture preview only");lane.mouseUp(mouse(point.translated(180,0),0));check(d.project()->time.meters[1].bar==4&&d.message.isEmpty(),"meter point drag commits at bar start");d.action(10);check(d.project()->time==map,"meter drag Undo");
    lane.mouseDown(mouse({lane.xAt(1)+2,12},juce::ModifierKeys::leftButtonModifier));lane.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey));check(d.project()->time==map&&!d.message.isEmpty(),"anchor delete refused");
    std::vector<mrs::MusicalPosition> beats;visitMeterGrid(map,8*mrs::ppq,11*mrs::ppq-1,1,[&](mrs::Tick tick,mrs::MusicalPosition pos){beats.push_back(pos);check(tick==8*mrs::ppq+(pos.beat-1)*mrs::ppq/2,"6/8 ruler has eighth-note beats");});check(beats.size()==6&&beats.back().beat==6,"all six denominator beats visible");
    d.message.clear();d.refresh();referencePreview(d,"meter84");
}

void section81Smoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    struct Clock final:mrs::audio::IAudioDevice{std::vector<mrs::audio::DeviceInfo> enumerate()override{return {{0,"Sections fixture",{}, {"L","R"},16,8192,128,1}};}void control_panel(int)override{}void open(const mrs::audio::DeviceConfig&,std::shared_ptr<mrs::audio::AudioEngine>)override{}void start()override{}void stop()override{}void close()noexcept override{}mrs::audio::DeviceStatus status()override{return {mrs::audio::DevicePhase::running,48000};}};
    Desktop d(true);d.app.new_project();const auto fixtureTrack=d.app.add_audio_track("Section editor retention");mrs::NativeInsert fx;fx.id=mrs::new_id();d.app.set_inserts(fixtureTrack,{fx});d.app.connect(std::make_unique<Clock>(),{0,48000,128,{}, {0,1}});d.setVisible(true);d.refresh(true);d.arrangement->pixelsPerSecond=120;d.snap=true;auto& lane=*d.arrangement->sections;
    auto pump=[](){for(int n=0;n<15;++n){MSG msg{};int limit=0;while(limit++<200&&PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}};
    auto form=[&](){pump();auto* f=lane.popupEditor();check(f!=nullptr&&f->isShowing(),"section popup opens");check(dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent())==nullptr,"section editor is a popup, not AlertWindow");return f;};
    auto field=[&](const juce::String& name){for(auto* child:form()->getChildren())if(auto* e=dynamic_cast<juce::TextEditor*>(child))if(e->getName()==name)return e;throw std::runtime_error("section popup field missing");};
    auto button=[&](const juce::String& name){for(auto* child:form()->getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(child))if(b->getButtonText()==name||b->getName()==name){b->onClick();pump();return;}throw std::runtime_error("section popup button missing");};
    lane.edit({},0);field("Name")->setText("Intro");field("End")->setText("3:1:0");button("Apply");check(d.message.isEmpty()&&d.project()->sections.size()==1&&d.project()->sections[0].end==8*mrs::ppq,"section precise create");auto first=d.project()->sections[0];
    d.app.add_section("Припев",12*mrs::ppq,16*mrs::ppq,0xD5B63E);d.refresh();lane.edit(first);referencePreview(*form()->getParentComponent(),"sections81-editor");button("Teal");check(d.project()->sections[0].color==0x287B89u,"palette square commits RGB");first=d.project()->sections[0];const auto revision=d.app.services().projects->state().revision;
    lane.edit(first);field("End")->setText("5:1:0");button("Apply");check(d.app.services().projects->state().revision==revision&&!d.message.isEmpty(),"overlap popup atomic");lane.edit(first);field("RGB")->setText("FFFFFF");juce::PopupMenu::dismissAllActiveMenus();pump();check(d.app.services().projects->state().revision==revision,"popup dismissal no history");
    auto mouse=[&](juce::Point<float> p,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(mods),1,0,0,0,0,&lane,&lane,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,true);};
    lane.mouseDown(mouse(lane.sectionRect(first).getCentre(),juce::ModifierKeys::rightButtonModifier));
    check(!field("Name")->isReadOnly(),"right click opens editable name");field("Name")->setText(juce::String::fromUTF8("ВступлениеX"));field("Name")->setCaretPosition(field("Name")->getTotalNumChars());field("Name")->keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey));check(d.project()->sections.size()==2,"Backspace in name field never deletes section");button("Apply");
    if(d.project()->sections[0].name!="Вступление"||d.project()->sections[0].id!=first.id)throw std::runtime_error("rename retains section ID: name="+d.project()->sections[0].name+" message="+d.message.toStdString());d.action(10);check(d.project()->sections[0]==first,"rename Undo exact");
    lane.mouseDown(mouse(lane.sectionRect(first).getCentre(),juce::ModifierKeys::leftButtonModifier));lane.mouseUp(mouse(lane.sectionRect(first).getCentre(),0));lane.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey));
    check(d.project()->sections.size()==1&&d.project()->sections[0].id!=first.id,"Backspace deletes selected section only");d.action(10);check(d.project()->sections[0]==first,"delete Undo exact");d.action(11);check(d.project()->sections.size()==1,"delete Redo");d.action(10);
    lane.mouseDown(mouse(lane.sectionRect(first).getCentre(),juce::ModifierKeys::leftButtonModifier));lane.clearSelection();lane.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey));check(d.project()->sections.size()==2,"Backspace without selection does not delete");
    const auto center=lane.sectionRect(first).getCentre();lane.mouseDown(mouse(center,juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse(center.translated(30,0),juce::ModifierKeys::leftButtonModifier));
    check(d.project()->sections[0]==first&&lane.dragging(),"section body move is preview only");lane.mouseUp(mouse(center.translated(30,0),0));
    const auto moved=d.project()->sections[0];check(moved.start>first.start&&moved.end-moved.start==first.end-first.start,"section body drag preserves duration");d.action(10);check(d.project()->sections[0]==first,"section move Undo");
    lane.mouseDoubleClick(mouse(center,juce::ModifierKeys::leftButtonModifier));check(lane.renameEditor()!=nullptr,"section double click inline rename");lane.renameEditor()->setText("Verse");lane.renameEditor()->onEscapeKey();pump();check(d.project()->sections[0]==first,"inline rename Escape");
    lane.mouseDoubleClick(mouse(center,juce::ModifierKeys::leftButtonModifier));lane.renameEditor()->setText(juce::String::fromUTF8("Куплет"));lane.renameEditor()->onReturnKey();pump();check(d.project()->sections[0].name=="Куплет","section inline rename commit");d.action(10);
    const auto gestureRevision=d.app.services().projects->state().revision;
    d.openInsert(fixtureTrack,fx.id);check(!d.windows.empty(),"native insert editor open");auto* pluginWindow=d.windows.back().get();
    const auto right=lane.sectionRect(first).getTopRight().translated(-1,10);lane.mouseDown(mouse(right,juce::ModifierKeys::leftButtonModifier));check(lane.dragging(),"section edge hit");lane.mouseDrag(mouse(right.translated(50,0),juce::ModifierKeys::leftButtonModifier));check(d.app.services().projects->state().revision==gestureRevision,"section drag preview only");lane.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));lane.mouseUp(mouse(right.translated(50,0),0));check(d.app.services().projects->state().revision==gestureRevision,"Escape cancels drag");
    lane.mouseDown(mouse(right,juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse(right.translated(50,0),juce::ModifierKeys::leftButtonModifier));lane.mouseUp(mouse(right.translated(50,0),0));check(d.project()->sections[0].end>first.end&&d.app.services().projects->state().revision==gestureRevision+1,"edge drag single commit");d.action(10);check(d.project()->sections[0]==first&&pluginWindow->isVisible(),"edge Undo exact and keeps plugin editor");d.action(11);d.closeEditors();
    first=d.project()->sections[0];const auto left=lane.sectionRect(first).getTopLeft().translated(1,10);lane.mouseDown(mouse(left,juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse(left.translated(35,0),juce::ModifierKeys::leftButtonModifier));lane.mouseUp(mouse(left.translated(35,0),0));check(d.project()->sections[0].start>0,"left edge moves");d.action(10);
    const auto offGrid=d.app.add_section("Off-grid",18*mrs::ppq+mrs::ppq/2,20*mrs::ppq,0x487D52);d.refresh();
    for(double zoom:{90.,123.45,180.})for(double scroll:{0.,37.25,500.}){
        d.arrangement->pixelsPerSecond=zoom;d.arrangement->horizontal=scroll;
        const auto item=*std::find_if(d.project()->sections.begin(),d.project()->sections.end(),[&](const auto& item){return item.id==offGrid;});
        const auto r=lane.sectionRect(item);const mrs::Timeline t(d.project()->time,d.project()->sample_rate);
        check(std::abs(r.getX()-(Arrangement::left+static_cast<double>(t.to_samples(item.start))/d.project()->sample_rate*zoom-scroll))<.001,"section geometry preserves fractional start");
        check(r.getY()==1&&r.getBottom()==31,"section taller block geometry");
        if(r.getCentreX()>=Arrangement::left)check(lane.sectionAt(r.getCentre())->id==offGrid,"off-grid hit and paint agree");
    }
    d.arrangement->horizontal=40;for(double zoom:{90.,180.}){d.arrangement->pixelsPerSecond=zoom;lane.sync();const auto r=lane.sectionRect(d.project()->sections[0]);check(lane.sectionAt(r.getCentre()).has_value(),"zoom scroll section hit matches paint");}d.arrangement->horizontal=0;d.arrangement->pixelsPerSecond=120;d.refresh();referencePreview(d,"sections81");for(float scale:{2.f}){auto output=juce::File::getCurrentWorkingDirectory().getChildFile("juce-sections81-200-preview.png").createOutputStream();check(output&&output->setPosition(0)&&output->truncate().wasOk()&&juce::PNGImageFormat().writeImageToStream(d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*output),"sections200 preview");}
}

void arranger80Smoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};Desktop d(true);d.app.new_project();const auto track=d.app.add_instrument_track("Keys");d.app.create_midi_clip(track,0,4*mrs::ppq);d.app.add_section("Куплет",0,mrs::ppq,0x7050A0);d.refresh(true);d.setVisible(true);d.arrangement->pixelsPerSecond=120;d.snap=true;
    auto& a=*d.arrangement;auto& lane=*a.sections;const auto initial=*d.project();const auto source=initial.sections.front();
    const auto mouse=[&](juce::Point<float> p,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(mods),1,0,0,0,0,&lane,&lane,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,true);};
    const auto center=lane.sectionRect(source).getCentre();const auto alt=juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier;
    lane.mouseDown(mouse(center,alt));lane.mouseDrag(mouse(center.translated(120,0),alt));check(*d.project()==initial&&lane.dragging(),"Alt section copy preview only");referencePreview(d,"arranger80-copy-preview");lane.mouseUp(mouse(center.translated(120,0),0));
    check(d.message.isEmpty()&&d.project()->sections.size()==2&&d.project()->sections[0]==source&&d.project()->sections[1].id!=source.id&&d.project()->clips==initial.clips,"Alt copy commits section only");d.action(10);check(*d.project()==initial,"copy Undo exact");d.action(11);check(d.project()->sections.size()==2,"copy Redo");d.action(10);
    lane.mouseDown(mouse(center,alt));lane.mouseDrag(mouse(center.translated(120,0),alt));lane.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));lane.mouseUp(mouse(center.translated(120,0),0));check(*d.project()==initial,"copy Escape exact");
    lane.mouseDown(mouse(center,alt));lane.mouseDrag(mouse(center.translated(5,0),alt));lane.mouseUp(mouse(center.translated(5,0),0));check(*d.project()==initial,"overlap copy rejected");
    lane.mouseDown(mouse(center,alt));lane.mouseDrag(mouse(center.translated(-1000,0),alt));lane.mouseUp(mouse(center.translated(-1000,0),0));check(*d.project()==initial,"copy before timeline clamps safely without commit");
    lane.mouseDown(mouse(center,juce::ModifierKeys::leftButtonModifier));lane.mouseUp(mouse(center,0));d.refresh();bool duplicateEnabled=false;for(auto* child:d.getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(child))if(b->getButtonText()=="Duplicate")duplicateEnabled=b->isEnabled();check(duplicateEnabled,"Duplicate enabled for sections");d.action(45);check(d.project()->sections.size()==2&&d.project()->sections[1].start==source.end,"toolbar duplicate adjoining section");d.action(10);
    const auto second=d.app.add_section("Chorus",3*mrs::ppq,4*mrs::ppq,0x487D52);d.refresh();const auto paired=*d.project();
    const auto chorus=paired.sections.back();
    lane.mouseDown(mouse(center,juce::ModifierKeys::leftButtonModifier));lane.mouseUp(mouse(center,0));
    lane.mouseDown(mouse(lane.sectionRect(chorus).getCentre(),juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::shiftModifier));lane.mouseUp(mouse(center,0));check(lane.selectionCount()==2,"Shift adds section to group");
    lane.mouseDown(mouse(lane.sectionRect(chorus).getCentre(),juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::shiftModifier));check(lane.selectionCount()==1,"Shift toggles section out of group");
    lane.clearSelection();lane.mouseDown(mouse({Arrangement::left+300.f,2.f},juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse({Arrangement::left+1.f,30.f},juce::ModifierKeys::leftButtonModifier));check(lane.selectionCount()==2&&*d.project()==paired,"marquee selects sections without history");lane.mouseUp(mouse({Arrangement::left+1.f,30.f},0));
    lane.mouseDown(mouse(center,alt));lane.mouseDrag(mouse(center.translated(242,0),alt));referencePreview(d,"arranger80-upd1-group-shadow");lane.mouseUp(mouse(center.translated(242,0),0));
    check(d.project()->sections.size()==4&&lane.selectionCount()==2&&d.project()->sections[2].start==chorus.end,"group Alt copy captures touching boundary");check(d.project()->clips==paired.clips,"group copy leaves clips untouched");d.action(10);check(*d.project()==paired,"group copy one Undo");d.action(11);check(d.project()->sections.size()==4,"group Redo");d.action(10);
    lane.clearSelection();lane.mouseDown(mouse(center,alt));lane.mouseDrag(mouse(center.translated(59,0),alt));lane.mouseUp(mouse(center.translated(59,0),0));check(d.project()->sections.size()==3&&d.project()->sections[1].start==source.end,"single Alt copy exact adjacency despite pixel rounding");d.action(10);
    lane.clearSelection();lane.mouseDown(mouse(center,alt));lane.mouseDrag(mouse(center.translated(180,0),alt));referencePreview(d,"arranger80-upd1-invalid-shadow");lane.mouseUp(mouse(center.translated(180,0),0));check(*d.project()==paired&&!d.message.isEmpty(),"invalid copy remains visible and commits nothing");d.message.clear();
    lane.clearSelection();lane.mouseDown(mouse({Arrangement::left+300.f,2.f},juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse({Arrangement::left+1.f,30.f},juce::ModifierKeys::leftButtonModifier));lane.mouseUp(mouse({Arrangement::left+1.f,30.f},0));lane.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey));check(d.project()->sections.empty(),"group Backspace atomic delete");d.action(10);check(*d.project()==paired,"group delete one Undo");d.app.remove_section(chorus);d.refresh();check(*d.project()==initial,"temporary group fixture restored");(void)second;
    for(auto* component:{static_cast<juce::Component*>(a.markers.get()),static_cast<juce::Component*>(a.sections.get()),static_cast<juce::Component*>(a.chords.get())}){bool plus=false;for(auto* child:component->getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(child))if(b->getButtonText()=="+"){plus=true;check(b->getX()==208&&b->getWidth()==34,"uniform lane add button bounds");}check(plus,"uniform plus button");}
    // A touching edge overrides coarse Snap using the raw pointer, including off-grid bounds.
    d.app.add_section("Off grid",2*mrs::ppq+80,3*mrs::ppq+80);d.refresh();const auto offGrid=d.project()->sections.back();a.pixelsPerSecond=600;d.snap=true;lane.clearSelection();const auto largeCenter=lane.sectionRect(source).getCentre();lane.mouseDown(mouse(largeCenter,alt));lane.mouseDrag(mouse(largeCenter.translated(325,0),alt));lane.mouseUp(mouse(largeCenter.translated(325,0),0));check(d.project()->sections.size()==3&&d.project()->sections[1].end==offGrid.start,"off-grid touching edge overrides coarse Snap");d.action(10);d.app.remove_section(offGrid);a.pixelsPerSecond=120;d.refresh();check(*d.project()==initial,"off-grid fixture restored");
    const auto revision=d.app.services().projects->state().revision;
    constexpr std::array<int,5> heights{24,33,24,32,32};const std::array<juce::Component*,5> lanes{a.markers.get(),a.sections.get(),a.chords.get(),a.tempos.get(),a.meters.get()};
    for(int mask=0;mask<32;++mask){for(int i=0;i<5;++i)a.setLaneVisible(i,(mask&(1<<i))!=0);int y=20;for(int i=0;i<5;++i){check(lanes[i]->isVisible()==((mask&(1<<i))!=0)&&a.laneButtons[i]->getToggleState()==lanes[i]->isVisible(),"lane toggle visibility");if(lanes[i]->isVisible()){check(lanes[i]->getY()==y&&lanes[i]->getHeight()==heights[i],"lane compact geometry");y+=heights[i];}}
        check(a.headerHeight()==y&&a.rowsBody.getY()==y,"dynamic rows header");const auto clip=a.clipRect(d.project()->clips.front());check(clip.getY()==y+1&&a.trackAt(clip.getCentreY())==0&&a.clipAt(clip.getCentre())==d.project()->clips.front().id,"clip paint/hit follows hidden lanes");check(ViewSettings::decode(d.view.encode()).musicalLanes==mask,"lane visibility preferences roundtrip");
    }
    check(d.app.services().projects->state().revision==revision&&*d.project()==initial,"visibility never edits project/history");
    a.laneButtons[0]->onClick();check(!a.markers->isVisible(),"icon click toggles lane");a.laneButtons[0]->onClick();
    lane.mouseDoubleClick(mouse(center,juce::ModifierKeys::leftButtonModifier));check(lane.renameEditor()!=nullptr,"rename before hiding");a.setLaneVisible(1,false);check(lane.renameEditor()==nullptr&&!lane.hasSelection(),"hide cancels inline rename/selection");referencePreview(d,"arranger80-lanes-hidden");a.setLaneVisible(1,true);referencePreview(d,"arranger80-lanes");
    auto old=juce::JSON::parse(d.view.encode());old.getDynamicObject()->removeProperty("musicalLanes");check(ViewSettings::decode(juce::JSON::toString(old)).musicalLanes==31,"legacy view settings show all lanes");
    bool rejected=false;old.getDynamicObject()->setProperty("musicalLanes",32);try{ViewSettings::decode(juce::JSON::toString(old));}catch(...){rejected=true;}check(rejected,"invalid lane mask rejected");
}

void unifiedLaneUpd2Smoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};Desktop d(true);d.app.new_project();d.setVisible(true);d.refresh(true);auto& a=*d.arrangement;a.pixelsPerSecond=120;d.snap=false;
    d.app.add_chord("Am",0,mrs::ppq);d.app.add_chord("C",3*mrs::ppq,4*mrs::ppq);d.app.add_marker("A",0);d.app.add_marker("B",3*mrs::ppq);d.refresh();const auto initial=*d.project();
    const auto test=[&](auto& lane,auto rect,auto values){
        const auto mouse=[&](juce::Point<float> p,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(mods),1,0,0,0,0,&lane,&lane,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,true);};
        const int left=juce::ModifierKeys::leftButtonModifier,shift=left|juce::ModifierKeys::shiftModifier,alt=left|juce::ModifierKeys::altModifier;const auto first=rect(values.front()).getCentre(),second=rect(values.back()).getCentre();
        lane.mouseDown(mouse(first,left));lane.mouseUp(mouse(first,0));lane.mouseDown(mouse(second,shift));lane.mouseUp(mouse(second,0));check(lane.selectionCount()==2,"unified Shift group selection");lane.mouseDown(mouse(second,shift));check(lane.selectionCount()==1,"unified Shift toggle off");
        lane.clearSelection();lane.mouseDown(mouse({Arrangement::left+450.f,1.f},left));lane.mouseDrag(mouse({Arrangement::left+1.f,23.f},left));lane.mouseUp(mouse({Arrangement::left+1.f,23.f},0));check(lane.selectionCount()==2,"unified marquee selection");
        lane.mouseDown(mouse(first,alt));lane.mouseDrag(mouse(first.translated(360,0),alt));check(*d.project()==initial&&lane.dragging(),"unified copy preview no publication");referencePreview(d,std::is_same_v<std::decay_t<decltype(values.front())>,mrs::Chord>?"unified-chord-shadow":"unified-marker-shadow");lane.mouseUp(mouse(first.translated(360,0),0));check(d.message.isEmpty()&&lane.selectionCount()==2,"unified group Alt-copy selects new IDs");d.action(10);check(*d.project()==initial,"unified group copy one Undo");d.action(11);d.action(10);
        lane.clearSelection();lane.mouseDown(mouse(first,alt));lane.mouseDrag(mouse(first.translated(-1000,0),alt));lane.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));lane.mouseUp(mouse(first.translated(-1000,0),0));check(*d.project()==initial,"unified negative/Escape safe");
        lane.mouseDown(mouse(first,left));lane.mouseUp(mouse(first,0));lane.mouseDown(mouse(second,shift));lane.mouseUp(mouse(second,0));d.action(45);check(lane.selectionCount()==2,"unified toolbar Duplicate group");d.action(10);check(*d.project()==initial,"unified Duplicate Undo");
        lane.clearSelection();lane.mouseDown(mouse(first,left));lane.mouseUp(mouse(first,0));lane.mouseDown(mouse(second,shift));lane.mouseUp(mouse(second,0));lane.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey));d.action(10);check(*d.project()==initial,"unified group Delete Undo");
    };
    test(*a.chords,[&](const auto& c){return a.chords->chordRect(c);},initial.chords);
    test(*a.markers,[&](const auto& m){return juce::Rectangle<float>{a.markers->xAt(m.tick),1,6,22};},initial.markers);
    const auto closeId=d.app.add_marker("Close",mrs::ppq);d.refresh();auto& markers=*a.markers;markers.clearSelection();const auto markerMouse=[&](juce::Point<float> p,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(mods),1,0,0,0,0,&markers,&markers,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,true);};markers.mouseDown(markerMouse({900,23},juce::ModifierKeys::leftButtonModifier));markers.mouseDrag(markerMouse({315,1},juce::ModifierKeys::leftButtonModifier));markers.mouseUp(markerMouse({315,1},0));check(markers.selectionCount()==2,"marquee uses visible shortened marker badge bounds");markers.clearSelection();const auto close=*std::find_if(d.project()->markers.begin(),d.project()->markers.end(),[&](const auto& m){return m.id==closeId;});d.app.remove_marker(close);d.refresh();
    auto& lane=*a.chords;auto chord=initial.chords.front();const auto p=lane.chordRect(chord).getCentre();const auto mouse=[&](juce::Point<float> point,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,&lane,&lane,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,true);};
    lane.mouseDown(mouse(p,juce::ModifierKeys::rightButtonModifier));const auto pump=[](){for(int n=0;n<15;++n){MSG msg{};int limit=0;while(limit++<200&&PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}};pump();check(lane.colorMenu()!=nullptr,"chord popup palette exists");referencePreview(*lane.colorMenu()->getTopLevelComponent(),"unified-chord-colors");bool clicked=false;for(auto* child:lane.colorMenu()->getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(child))if(b->getName()=="Teal"){b->onClick();clicked=true;break;}pump();check(clicked&&d.project()->chords.front().color==0x287B89u,"chord palette stores RGB");d.action(10);check(!d.project()->chords.front().color,"chord color Undo exact");d.action(11);referencePreview(d,"unified-lanes-upd2");
}

void midiImport149Smoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};Desktop d(true);d.app.new_project();d.refresh(true);
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mrs-midi-ui","",false);check(root.createDirectory().wasOk(),"MIDI UI temp directory");
    struct Clean{juce::File file;~Clean(){file.deleteRecursively();}}clean{root};const auto file=root.getChildFile("Keys.mid");
    const unsigned char raw[]={0x4d,0x54,0x68,0x64,0,0,0,6,0,0,0,1,1,0xe0,0x4d,0x54,0x72,0x6b,0,0,0,13,0,0x90,60,100,0x83,0x60,0x80,60,0,0,0xff,0x2f,0};check(file.replaceWithData(raw,sizeof(raw)),"MIDI UI fixture");
    juce::StringArray files{file.getFullPathName()};check(d.arrangement->isInterestedInFileDrag(files),"MIDI external drop accepted");
    d.browser->navigate(root);d.browser->showFiles(true);
    juce::FileTreeComponent* fileTree=nullptr;for(auto* child:d.browser->getChildren())if(auto* tree=dynamic_cast<juce::FileTreeComponent*>(child))fileTree=tree;check(fileTree!=nullptr,"Files tree available");fileTree->setSelectedFile(file);
    for(int dispatch=0;dispatch<100&&d.browser->selectedSamples().isEmpty();++dispatch){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(10);}
    check(d.browser->selectedSamples()==files,"Files browser displays and selects MIDI");
    d.arrangement->fileDragMove(files,500,Arrangement::header+20);const auto rects=d.arrangement->midiDropRects();check(rects.size()==1&&rects[0].getWidth()>2,"MIDI ghost has real duration");referencePreview(d,"midi-drop-shadow");
    const auto before=*d.project();d.arrangement->filesDropped(files,500,Arrangement::header+20);check(d.message.isEmpty()&&d.project()->tracks.size()==1&&d.project()->clips.size()==1&&d.project()->clips[0].midi&&d.arrangement->midiDropRects().empty(),"MIDI drop creates track and clears ghost");
    const auto assigned=*d.project();d.action(10);check(*d.project()==before,"MIDI UI single Undo");d.action(11);check(*d.project()==assigned,"MIDI UI Redo");
    d.arrangement->fileDragMove(files,650,Arrangement::header+20);check(d.arrangement->midiDropRects()[0].getY()==rects[0].getY(),"MIDI ghost targets existing instrument row");d.arrangement->filesDropped(files,650,Arrangement::header+20);check(d.project()->tracks.size()==1&&d.project()->clips.size()==2,"MIDI drop reuses existing track");
    d.arrangement->fileDragExit(files);check(d.arrangement->midiDropRects().empty(),"MIDI drag exit clears ghost");
    const auto revision=d.app.services().projects->state().revision;auto details=juce::DragAndDropTarget::SourceDetails("mrs-sample-files",d.browser.get(),{50,50});check(d.mixer.front()->isInterestedInDragSource(details),"Mixer channel accepts MIDI browser drag");d.mixer.front()->itemDropped(details);
    for(int dispatch=0;dispatch<10;++dispatch){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}
    check(d.project()->tracks.size()==1&&d.project()->clips.size()==3&&d.app.services().projects->state().revision==revision+1,"Mixer MIDI drop imports at project start");

}

void navigation86Smoke(){
    auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    struct Clock final:mrs::audio::IAudioDevice{
        std::vector<mrs::audio::DeviceInfo> enumerate()override{return {{0,"Navigation fixture",{"Mic"},{"L","R"},16,8192,128,1}};}
        void control_panel(int)override{}void open(const mrs::audio::DeviceConfig&,std::shared_ptr<mrs::audio::AudioEngine>)override{}
        void start()override{}void stop()override{}void close()noexcept override{}mrs::audio::DeviceStatus status()override{return {mrs::audio::DevicePhase::running,48000};}
    };
    Desktop d(true);d.app.connect(std::make_unique<Clock>(),{0,48000,128,{0},{0,1}});d.setSize(1200,900);d.setVisible(true);d.refresh(true);
    std::array<float,256> output{};std::array<float,128> input{};auto flush=[&]{d.app.engine()->process(input.data(),output.data(),128);d.app.poll();d.refresh();};
    const auto original=d.displayedMusical;d.action(35);check(d.displayedMusical==original,"navigation UI waits for ack");flush();check(d.displayedMusical.current_section&&d.displayedMusical.current_section->id==mrs::Id{"section-verse"},"section arrow ack");
    check(d.displayState().playback==mrs::PlaybackState::stopped,"navigation does not start playback");
    d.navigationMode.setSelectedId(2,juce::sendNotificationSync);
    for(auto* child:d.getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(child))if(b->getButtonText()=="Section >")b->onClick();
    flush();check(d.displayedMusical.current_marker&&d.displayedMusical.current_marker->id==mrs::Id{"cue-chorus"},"marker mode arrow");
    d.app.play();flush();d.action(48);flush();check(d.displayState().playback==mrs::PlaybackState::playing&&d.displayedMusical.current_marker->id==mrs::Id{"cue-chorus"},"marker previous restarts current anchor during play");
    d.app.musical().seek_marker({"cue-vocal"});flush();check(d.displayState().playback==mrs::PlaybackState::playing&&d.displayedMusical.current_marker->id==mrs::Id{"cue-vocal"},"marker ID seek preserves play");
    d.app.pause();flush();d.action(49);flush();check(d.displayState().playback==mrs::PlaybackState::paused,"marker seek preserves pause");
    const auto before=d.displayedMusical;while(d.app.engine()->enqueue({mrs::audio::ControlKind::pause})){}d.action(34);check(d.message.contains("queue full")&&d.displayedMusical==before,"queue full leaves UI at confirmed position");flush();
    referencePreview(d,"navigation86");
    d.app.stop();flush();d.app.set_track_armed(d.project()->tracks.front().id,true);auto settings=d.app.click_settings();settings.count_bars=1;d.app.set_click_settings(settings);flush();
    const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mrs-navigation-"+juce::String(mrs::new_id().value)+".wav");
    struct Cleanup{juce::File file;~Cleanup(){file.deleteFile();}} cleanup{file};
    d.app.start_recording(std::filesystem::path(file.getFullPathName().toWideCharPointer()));flush();d.refresh();check(d.app.recording()&&!d.navigationMode.isEnabled(),"record count disables navigation UI");
    bool refused=false;const auto position=d.displayState().sample;try{d.app.musical().seek_marker({"cue-chorus"});}catch(const std::exception&){refused=true;}check(refused&&d.displayState().sample==position,"shared command recording guard");d.app.stop_recording();d.app.stop();flush();
}

void upd14Smoke(){
    auto check=[](bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);};
    Desktop d(true,true);d.action(1);d.setSize(1200,900);d.setVisible(true);d.setWorkspace(mrs::desktop::Workspace::mix);
    for(auto* child:d.getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()=="+ Track"||button->getButtonText()=="+ Bus"||button->getButtonText()=="Del clip")check(!button->isVisible(),"upd14 toolbar controls removed");
    d.action(20);d.action(21);d.action(29);const auto fx=*d.selectedTrack;
    auto role=[&]{return d.project()->tracks.back().fx_channel;};check(role(),"FX role created");
    check(mrs::deserialize(mrs::serialize(*d.project()))==*d.project(),"FX role round trip");
    d.action(10);check(d.project()->tracks.size()==2,"FX add Undo");d.action(11);check(role(),"FX add Redo role");
    auto menu=d.channelMenu(fx);juce::PopupMenu::MenuItemIterator items(menu);bool del=false;while(items.next())if(items.getItem().itemID==23)del=items.getItem().text=="Del FX channel";check(del,"typed FX delete menu");
    d.selectedTrack=fx;d.action(23);check(d.project()->tracks.size()==2,"FX deletion");d.action(10);check(role(),"FX deletion Undo");
    d.app.set_track_output(d.project()->tracks.front().id,fx);d.selectedTrack=fx;d.action(23);d.action(10);check(d.project()->tracks.front().output==fx,"delete Undo restores routing");
    referencePreview(d,"upd14-mixer");
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);const auto fixtureRoot=temp.getChildFile("mrs-upd14-"+juce::String(mrs::new_id().value));check(fixtureRoot.getParentDirectory()==temp&&!fixtureRoot.exists()&&fixtureRoot.createDirectory().wasOk(),"upd14 fixture scope");auto savedFile=fixtureRoot.getChildFile("Close.mrsproject");
    struct RemoveFixture{juce::File folder;~RemoveFixture(){folder.deleteRecursively();}} cleanup{fixtureRoot};
    d.saveFile(savedFile);savedFile=juce::File(juce::String(d.app.path().wstring().c_str()));d.app.rename_track(fx,"Dirty FX");

    d.action(8);check(juce::ModalComponentManager::getInstance()->getNumModalComponents()==1,"dirty Close asks save/discard/cancel");
    auto* alert=dynamic_cast<juce::AlertWindow*>(juce::ModalComponentManager::getInstance()->getModalComponent(0));check(alert!=nullptr,"close confirmation");alert->exitModalState(0);
    {const auto deadline=juce::Time::getMillisecondCounter()+30;while(juce::Time::getMillisecondCounter()<deadline){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}}check(!d.projectHomeVisible()&&d.project()->tracks.size()==3,"Close Cancel preserves project");
    d.action(8);alert=dynamic_cast<juce::AlertWindow*>(juce::ModalComponentManager::getInstance()->getModalComponent(0));check(alert!=nullptr,"close discard confirmation");alert->exitModalState(2);
    {const auto deadline=juce::Time::getMillisecondCounter()+30;while(juce::Time::getMillisecondCounter()<deadline){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}}check(d.projectHomeVisible()&&!d.canReturnToProject()&&d.project()->tracks.empty()&&d.app.path().empty(),"Close Discard releases project and goes home");
    d.openFile(savedFile);check(d.project()->tracks.back().fx_channel,"FX role actual file reopen");d.app.rename_track(fx,"Saved FX");
    d.action(8);alert=dynamic_cast<juce::AlertWindow*>(juce::ModalComponentManager::getInstance()->getModalComponent(0));check(alert!=nullptr,"close save confirmation");alert->exitModalState(1);
    {const auto deadline=juce::Time::getMillisecondCounter()+40;while(juce::Time::getMillisecondCounter()<deadline){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}}
    check(d.projectHomeVisible()&&!d.canReturnToProject(),"Close Save goes home");d.openFile(savedFile);check(d.project()->tracks.back().name=="Saved FX","Close Save persists changes");d.action(8);check(d.projectHomeVisible()&&!d.canReturnToProject(),"clean Close no prompt");

}

void projectHomeSmoke(){
    arrangeFinalGesturesSmoke();
    arrangeAudioSmoke();
    auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    juce::Component canvas;canvas.setSize(100,40);canvas.setVisible(true);float cursorX=10.25f;PlayheadLine line([&]{return juce::Rectangle<float>{cursorX,5.f,1.f,20.f};});canvas.addAndMakeVisible(line);line.update();auto first=canvas.createComponentSnapshot(canvas.getLocalBounds(),true,1.f,juce::SoftwareImageType{});check(first.getPixelAt(10,15).getAlpha()>0,"fractional cursor renders");cursorX=30.75f;line.update();auto moved=canvas.createComponentSnapshot(canvas.getLocalBounds(),true,1.f,juce::SoftwareImageType{});check(moved.getPixelAt(10,15).getAlpha()==0,"moving cursor clears old stripe");check(moved.getPixelAt(31,15).getAlpha()>0,"moving cursor paints new stripe");check(canvas.getComponentAt(31,15)==&canvas,"cursor does not intercept gestures");
    for(double rate:{44100.,48000.,96000.}){
        VisualTransport visual;mrs::audio::RealtimeState state;state.playback=mrs::PlaybackState::playing;
        check(visual.sample(state,rate,0)==0,"visual starts at audio head");state.sample=static_cast<mrs::Sample>(rate*.02);visual.sample(state,rate,20);
        const auto middle=visual.sample(state,rate,30);check(middle>0&&middle<state.sample,"visual interpolates between observed samples");
        check(visual.sample(state,rate,200)==state.sample,"stalled audio does not extrapolate");state.playback=mrs::PlaybackState::paused;state.sample=123;check(visual.sample(state,rate,201)==123,"Pause exact sample");
        state.playback=mrs::PlaybackState::playing;visual.sample(state,rate,202);state.sample=120;check(visual.sample(state,rate,203)==120,"backward Seek snaps");
        state.sample=static_cast<mrs::Sample>(rate*10);check(visual.sample(state,rate,204)==state.sample,"forward Seek snaps");state.loop=mrs::LoopRange{0,1000};state.sample=990;visual.sample(state,rate,205);state.sample=10;check(visual.sample(state,rate,206)==10,"loop wrap snaps instead of sweeping backward");
        state.playback=mrs::PlaybackState::stopped;state.sample=0;check(visual.sample(state,rate,207)==0,"Stop exact return");
        for(int frames:{128,256,2048}){visual.reset();state={};state.playback=mrs::PlaybackState::playing;double previous=0;for(int frame=0;frame<180;++frame){const double now=frame*1000./60;state.sample=static_cast<mrs::Sample>(std::floor(now*.001*rate/frames))*frames;const auto head=visual.sample(state,rate,now);check(std::isfinite(head)&&head>=previous&&head<=state.sample,"60Hz visual bounded monotonic for callback size");previous=head;}}
    }
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mrs-home-upd6","",false);check(folder.createDirectory().wasOk(),"home temporary root");
    Desktop home(true,true);check(home.projectHomeVisible()&&home.project()->tracks.empty()&&!home.arrangement->isVisible(),"startup home without demo tracks");bool initialClose=false;home.confirmDiscard([&]{initialClose=true;});check(initialClose,"initial home has no invisible-project save prompt");
    home.app.new_project(48000,"First project");const auto a=folder.getChildFile("Projects/First/First.mrsproject");home.app.save_project(std::filesystem::path(a.getFullPathName().toWideCharPointer()));
    home.app.new_project(48000,"Duplicate name");const auto b=folder.getChildFile("Projects/Second/First.MRSPROJECT");home.app.save_project(std::filesystem::path(b.getFullPathName().toWideCharPointer()));
    check(folder.getChildFile("directory.mrsproject").createDirectory().wasOk(),"folder with project suffix");check(folder.getChildFile("notes.txt").replaceWithText("not a project"),"home unrelated file fixture");
    auto found=findHomeProjects(folder);check(found.error.isEmpty()&&found.files.size()==2&&found.files[0].relative!=found.files[1].relative,"recursive case-insensitive files only, duplicates retained");
    home.home->scanRoot(folder);for(int attempt=0;attempt<200&&home.home->loading();++attempt){std::this_thread::sleep_for(std::chrono::milliseconds(5));home.home->pollScan();}check(!home.home->loading()&&home.home->projectCount()==2,"asynchronous library scan");
    for(auto* c:home.home->getChildren())if(c->getComponentID()=="home-search"){auto* search=dynamic_cast<juce::TextEditor*>(c);search->setText("Second",false);search->onTextChange();check(home.home->projectCount()==1,"home filter matches relative path");search->clear();search->onTextChange();}
    for(auto size:std::array<juce::Point<int>,2>{juce::Point<int>{1200,700},juce::Point<int>{1400,850}}){home.setSize(size.x,size.y);for(float scale:{1.f,1.5f}){auto image=home.createComponentSnapshot(home.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-home-"+juce::String(size.x)+"-"+juce::String(static_cast<int>(scale*100))+"-preview.png").createOutputStream();check(stream!=nullptr,"home preview stream");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(image,*stream),"home snapshot");}}
    juce::ListBox* homeList=nullptr;for(auto* child:home.home->getChildren())if(child->getComponentID()=="home-project-list")homeList=dynamic_cast<juce::ListBox*>(child);
    check(homeList&&homeList->getMouseCursor()==juce::MouseCursor(juce::MouseCursor::NormalCursor),"home list blank area has normal cursor");
    auto* firstRow=homeList->getComponentForRowNumber(0);check(firstRow!=nullptr,"home custom name row");
    auto* nameLink=dynamic_cast<juce::TextButton*>(firstRow->getChildComponent(0));
    check(nameLink&&nameLink->getComponentID()=="home-project-name"&&nameLink->getMouseCursor()==juce::MouseCursor(juce::MouseCursor::PointingHandCursor)&&nameLink->getWidth()<firstRow->getWidth()/2,"hand cursor is scoped to actual project name");
    const auto normalName=nameLink->createComponentSnapshot(nameLink->getLocalBounds(),true,1.f,juce::SoftwareImageType{});nameLink->setState(juce::Button::buttonOver);
    const auto hoverName=nameLink->createComponentSnapshot(nameLink->getLocalBounds(),true,1.f,juce::SoftwareImageType{});bool highlighted=false;
    for(int y=0;y<normalName.getHeight();++y)for(int x=0;x<normalName.getWidth();++x)highlighted=highlighted||normalName.getPixelAt(x,y)!=hoverName.getPixelAt(x,y);
    check(highlighted,"home filename highlights on hover");
    for(const float hoverScale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(hoverScale==1.f?"juce-home-hover-100-preview.png":"juce-home-hover-150-preview.png").createOutputStream();check(stream!=nullptr,"hover snapshot stream");stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(home.createComponentSnapshot(home.getLocalBounds(),true,hoverScale,juce::SoftwareImageType{}),*stream);}nameLink->setState(juce::Button::buttonNormal);
    nameLink->onClick();const auto until=juce::Time::getMillisecondCounter()+30;while(juce::Time::getMillisecondCounter()<until){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}check(!home.projectHomeVisible()&&!home.app.path().empty()&&home.arrangement->isVisible(),"single filename click opens project via shared loading");
    home.action(6);check(home.projectHomeVisible()&&home.canReturnToProject(),"File Project home returns to library");for(auto* c:home.home->getChildren())if(c->getComponentID()=="home-resume"){check(c->isVisible(),"Resume available for loaded project");dynamic_cast<juce::TextButton*>(c)->onClick();}check(!home.projectHomeVisible(),"Resume returns to same project");home.action(6);const auto previous=home.app.path();home.run([&]{home.openFile(folder.getChildFile("missing.mrsproject"));});check(home.projectHomeVisible()&&home.app.path()==previous&&!home.message.isEmpty(),"missing project preserves session and library error");
    const auto bad=folder.getChildFile("corrupt.mrsproject");check(bad.replaceWithText("not a project"),"corrupt fixture");home.run([&]{home.openFile(bad);});check(home.projectHomeVisible()&&home.app.path()==previous&&!home.message.isEmpty(),"corrupt project preserves loaded session");
    home.action(1);check(!home.projectHomeVisible()&&home.app.path().empty()&&home.project()->tracks.empty(),"New project leaves home with empty workspace");
    auto map=home.project()->time;map.tempos={{0,120},{mrs::ppq,60},{2*mrs::ppq,180}};home.app.set_time_map(map);home.app.seek(36025);std::array<float,256> silence{};home.app.engine()->process(nullptr,silence.data(),128);check(std::abs(home.visualTick()-1200.5)<.001,"visual MIDI position keeps fractional ticks across tempo change");
    home.home.reset();check(folder.deleteRecursively(),"home fixture cleanup");
}
void midi4bSmoke(Desktop& d,const juce::File& fixture){
    auto check=[](bool b,const char* text){if(!b)throw std::runtime_error(text);};
    if(!fixture.existsAsFile())return; auto stage=[](const char* text){juce::File::getCurrentWorkingDirectory().getChildFile("midi-clips-smoke-stage.txt").replaceWithText(text);};stage("begin");
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);const auto folder=temp.getNonexistentChildFile("mrs-midi4b","",false);folder.createDirectory();const auto plugin=folder.getChildFile("instrument.vst3");check(fixture.copyFileTo(plugin),"MIDI playback fixture copy");
    const bool peer=d.getPeer()==nullptr;if(peer)d.addToDesktop(0);
    d.closeEditors();d.view.editorAttached=false;d.view.controllersOpen=true;d.app.new_project();const auto track=d.app.add_instrument_track("MIDI clip playback");auto info=mrs::processing::probe_vst3(plugin.getFullPathName().toStdString()).front();mrs::NativeInsert fx;fx.id=mrs::new_id();fx.kind=mrs::InsertKind::vst3;fx.plugin_path=info.path;fx.class_id=info.class_id;fx.plugin_name=info.name;d.app.set_inserts(track,{fx});d.app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1},2});d.selectedTrack=track;d.refresh(true);stage("create editor");d.action(43);
    check(d.message.isEmpty()&&d.selectedClip&&d.project()->clips.size()==1&&!d.windows.empty(),"MIDI clip menu and editor");const auto clip=*d.selectedClip;auto* notePanel=d.windows.back()->getContentComponent();
    auto control=[&](const char* id)->juce::Component*{std::function<juce::Component*(juce::Component&)> find=[&](juce::Component& parent)->juce::Component*{if(parent.getComponentID()==id)return &parent;for(auto* child:parent.getChildren())if(auto* match=find(*child))return match;return nullptr;};if(auto* result=find(*notePanel))return result;throw std::runtime_error("MIDI field missing");};
    auto* add=dynamic_cast<juce::TextButton*>(control("midi-note-add"));check(add!=nullptr,"MIDI add action");stage("add note");add->onClick();check(d.project()->clips.front().midi->notes.size()==1,"MIDI note enters shared model");const auto note=d.project()->clips.front().midi->notes.front().id;
    dynamic_cast<juce::TextEditor*>(control("midi-note-0"))->setText("67");dynamic_cast<juce::TextButton*>(control("midi-note-apply"))->onClick();check(d.project()->clips.front().midi->notes.front().pitch==67&&d.project()->clips.front().midi->notes.front().id==note,"MIDI form keeps note ID");
    stage("edit validation");const auto revision=d.app.services().projects->state().revision;dynamic_cast<juce::TextEditor*>(control("midi-note-3"))->setText("0");dynamic_cast<juce::TextButton*>(control("midi-note-apply"))->onClick();check(d.app.services().projects->state().revision==revision,"invalid MIDI form is atomic");dynamic_cast<juce::TextEditor*>(control("midi-note-3"))->setText("100");dynamic_cast<juce::TextButton*>(control("midi-note-apply"))->onClick();
    stage("piano roll gestures");PianoRoll* roll=nullptr;for(auto* child:notePanel->getChildren())if(auto* viewport=dynamic_cast<juce::Viewport*>(child))if(auto* piano=dynamic_cast<PianoRoll*>(viewport->getViewedComponent()))roll=piano;check(roll!=nullptr,"piano roll viewport");for(auto* child:notePanel->getChildren())check(child->getComponentID()!="editor-play"&&child->getComponentID()!="editor-pause"&&child->getComponentID()!="editor-stop","no duplicate editor transport buttons");
    const auto settingsBefore=d.app.click_settings();
    auto toolbar=[&](const char* title){for(auto* child:d.getChildren())if(child->getTitle()==title)if(auto* button=dynamic_cast<juce::TextButton*>(child)){button->onClick();return;}throw std::runtime_error("Toolbar toggle missing");};
    toolbar("Toggle metronome");check(d.app.click_settings().playback==!(settingsBefore.playback||settingsBefore.recording)&&d.app.click_settings().recording==d.app.click_settings().playback,"Click toggles without popup");
    toolbar("Toggle metronome");d.action(513);toolbar("Toggle recording count-in");check(d.app.click_settings().count_bars==0,"Count toolbar disables count");toolbar("Toggle recording count-in");check(d.app.click_settings().count_bars==3,"Count restores chosen bars without popup");d.app.set_click_settings(settingsBefore);
    const auto savedNotes=d.project()->clips.front().midi->notes;const auto origin=roll->noteRect(roll->notes.front()).getCentre();
    auto mouse=[&](juce::Point<float> point,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,roll,roll,juce::Time::getCurrentTime(),origin,juce::Time::getCurrentTime(),1,true);};
    const auto insertGeneration=d.app.insert_generation();
    const auto rev=d.app.services().projects->state().revision;roll->mouseDown(mouse(origin,juce::ModifierKeys::leftButtonModifier));roll->mouseDrag(mouse(origin.translated(24,-18),juce::ModifierKeys::leftButtonModifier));check(d.app.services().projects->state().revision==rev,"piano drag is preview only");roll->mouseUp(mouse(origin.translated(24,-18),0));check(d.project()->clips.front().midi->notes.front().pitch==68&&d.project()->clips.front().midi->notes.front().start==mrs::ppq/4,"piano snapped move");
    check(d.app.insert_generation()==insertGeneration,"piano move retains instrument instance");
    check(roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0)),"piano Undo shortcut");check(d.project()->clips.front().midi->notes==savedNotes,"piano shared Undo restores gesture");
    const auto stableInstrument=d.app.insert_generation();std::array<float,256> dragAudio{};
    for(int repeat=0;repeat<32;++repeat){roll->sync();const auto from=roll->noteRect(roll->notes.front()).getCentre();const float direction=repeat%2?-1.f:1.f;const auto to=from.translated(direction*24,-direction*18);roll->mouseDown(mouse(from,juce::ModifierKeys::leftButtonModifier));d.app.engine()->process(nullptr,dragAudio.data(),128);roll->mouseDrag(mouse(to,juce::ModifierKeys::leftButtonModifier));d.app.engine()->process(nullptr,dragAudio.data(),128);roll->mouseUp(mouse(to,0));d.app.engine()->process(nullptr,dragAudio.data(),128);check(d.app.insert_generation()==stableInstrument,"repeated drag retains VST3 lifetime");}
    check(d.project()->clips.front().midi->notes==savedNotes,"32 auditioned drags return notes unchanged");
    roll->sync();roll->selected={note.value};roll->copy();roll->cursor=2*mrs::ppq;roll->paste();check(d.project()->clips.front().midi->notes.size()==2,"piano paste creates note");check(d.project()->clips.front().midi->notes.back().id!=note,"piano paste fresh ID");
    roll->selected.clear();for(const auto& n:roll->notes)roll->selected.insert(n.id.value);const auto groupBefore=roll->notes;auto point=roll->noteRect(roll->notes.front()).getCentre();roll->mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));roll->mouseDrag(mouse(point.translated(24,-36),juce::ModifierKeys::leftButtonModifier));roll->mouseUp(mouse(point.translated(24,-36),0));check(d.project()->clips.front().midi->notes.front().pitch==69&&d.project()->clips.front().midi->notes.back().pitch==69,"piano group move");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));check(d.project()->clips.front().midi->notes==groupBefore,"one Undo restores whole group");
    roll->sync();point=roll->noteRect(roll->notes.front()).getCentre();roll->mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));roll->mouseDrag(mouse(point.translated(80,-100),juce::ModifierKeys::leftButtonModifier));roll->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));check(d.project()->clips.front().midi->notes==groupBefore&&roll->notes==groupBefore,"Escape cancels preview");
    roll->selected={d.project()->clips.front().midi->notes.back().id.value};roll->erase();check(d.project()->clips.front().midi->notes==savedNotes,"piano selected delete");
    roll->sync();point=roll->noteRect(roll->notes.front()).withWidth(roll->noteRect(roll->notes.front()).getWidth()-1).getTopRight().translated(0,8);roll->mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));roll->mouseDrag(mouse(point.translated(24,0),juce::ModifierKeys::leftButtonModifier));roll->mouseUp(mouse(point.translated(24,0),0));check(d.project()->clips.front().midi->notes.front().length==mrs::ppq+mrs::ppq/4,"piano right-edge resize");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));
    roll->sync();point=roll->noteRect(roll->notes.front()).getCentre();roll->mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier|juce::ModifierKeys::ctrlModifier));roll->mouseDrag(mouse(point.translated(0,-20),juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier|juce::ModifierKeys::ctrlModifier));roll->mouseUp(mouse(point.translated(0,-20),0));check(d.project()->clips.front().midi->notes.front().velocity==114,"piano velocity gesture");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));
    roll->sync();point=roll->noteRect(roll->notes.front()).getCentre();roll->mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));roll->mouseDrag(mouse(point.translated(-10000,-10000),juce::ModifierKeys::leftButtonModifier));roll->mouseUp(mouse(point.translated(-10000,-10000),0));check(d.project()->clips.front().midi->notes.front().start==0&&d.project()->clips.front().midi->notes.front().pitch==127,"piano edge clamp");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));
    stage("upd1 gestures");const auto beforeZoom=roll->rowHeight;roll->wheel(.5f,juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),origin.x,origin.y);check(roll->rowHeight>beforeZoom,"Ctrl wheel vertical piano zoom");roll->wheel(-.5f,juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),origin.x,origin.y);const auto beatBefore=roll->beatWidth;roll->wheel(.25f,juce::ModifierKeys(juce::ModifierKeys::ctrlModifier|juce::ModifierKeys::shiftModifier),origin.x,origin.y);check(roll->beatWidth>beatBefore,"Ctrl Shift wheel horizontal piano zoom");roll->wheel(-.25f,juce::ModifierKeys(juce::ModifierKeys::ctrlModifier|juce::ModifierKeys::shiftModifier),origin.x,origin.y);roll->sync();for(auto* child:notePanel->getChildren())if(auto* viewport=dynamic_cast<juce::Viewport*>(child);viewport&&dynamic_cast<PianoRoll*>(viewport->getViewedComponent()))viewport->setViewPosition(0,static_cast<int>(roll->noteRect(roll->notes.front()).getY())-60);roll->mouseDoubleClick(mouse(roll->noteRect(roll->notes.front()).getCentre(),juce::ModifierKeys::leftButtonModifier));check(d.project()->clips.front().midi->notes.empty(),"double left click removes note");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));check(d.project()->clips.front().midi->notes==savedNotes,"double click delete Undo");
    stage("fix1 right click and audition");roll->sync();auto audiblePoint=roll->noteRect(roll->notes.front()).getCentre();const auto rightRevision=d.app.services().projects->state().revision;roll->mouseDown(mouse(audiblePoint,juce::ModifierKeys::rightButtonModifier));check(d.app.services().projects->state().revision==rightRevision&&d.project()->clips.front().midi->notes==savedNotes,"right click does not delete");std::array<float,256> auditionAudio{};d.app.engine()->process(nullptr,auditionAudio.data(),128);roll->mouseDown(mouse(audiblePoint,juce::ModifierKeys::leftButtonModifier));d.app.engine()->process(nullptr,auditionAudio.data(),128);const auto originalAudition=auditionAudio.back();check(originalAudition>0&&roll->auditionPitch()==67,"original drag note audition");roll->mouseDrag(mouse(audiblePoint.translated(0,-18),juce::ModifierKeys::leftButtonModifier));d.app.engine()->process(nullptr,auditionAudio.data(),128);check(roll->auditionPitch()==68&&std::abs(auditionAudio.back()-originalAudition)<.01f,"drag retunes one voice without retaining old note");roll->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));d.app.engine()->process(nullptr,auditionAudio.data(),128);check(auditionAudio.back()==0&&d.project()->clips.front().midi->notes==savedNotes,"Escape releases retuned audition");
    stage("upd4 note movement");auto offGrid=savedNotes;offGrid.front().start=73;d.app.set_midi_notes(clip,offGrid);d.refresh();roll->sync();roll->selected={note.value};auto noteOrigin=roll->noteRect(roll->notes.front()).getCentre();roll->mouseDown(mouse(noteOrigin,juce::ModifierKeys::leftButtonModifier));roll->mouseDrag(mouse(noteOrigin.translated(8,-roll->rowHeight),juce::ModifierKeys::leftButtonModifier));roll->mouseUp(mouse(noteOrigin.translated(8,-roll->rowHeight),0));check(roll->notes.front().start==73&&roll->notes.front().pitch==68,"vertical drag preserves off-grid time");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));roll->sync();noteOrigin=roll->noteRect(roll->notes.front()).getCentre();roll->mouseDown(mouse(noteOrigin,juce::ModifierKeys::leftButtonModifier));roll->mouseDrag(mouse(noteOrigin.translated(24,0),juce::ModifierKeys::leftButtonModifier));roll->mouseUp(mouse(noteOrigin.translated(24,0),0));check(roll->notes.front().start%mrs::Tick(roll->grid)==0&&roll->notes.front().pitch==67,"horizontal drag snaps note start");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));roll->keyPressed(juce::KeyPress(juce::KeyPress::upKey));check(roll->notes.front().pitch==68&&roll->notes.front().start==73,"Up transposes selected note one semitone");roll->keyPressed(juce::KeyPress(juce::KeyPress::downKey));check(roll->notes==offGrid,"Down restores pitch without shifting time");roll->sync();noteOrigin=roll->noteRect(roll->notes.front()).getCentre();roll->mouseDown(mouse(noteOrigin,juce::ModifierKeys::leftButtonModifier));roll->mouseDrag(mouse(noteOrigin.translated(7,0),juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::shiftModifier));roll->mouseUp(mouse(noteOrigin.translated(7,0),0));check(roll->notes.front().start==73+static_cast<mrs::Tick>(std::llround(7/roll->beatWidth*mrs::ppq))&&roll->grid==mrs::ppq/4,"Shift note drag bypasses snap and small-horizontal deadzone without changing grid");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys::ctrlModifier,0));roll->sync();noteOrigin=roll->noteRect(roll->notes.front()).getCentre();roll->mouseDown(mouse(noteOrigin,juce::ModifierKeys::leftButtonModifier));roll->mouseDrag(mouse(noteOrigin.translated(7,0),juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::shiftModifier));roll->mouseDrag(mouse(noteOrigin.translated(24,0),juce::ModifierKeys::leftButtonModifier));roll->mouseUp(mouse(noteOrigin.translated(24,0),0));check(roll->notes.front().start%roll->grid==0,"releasing Shift during note drag resumes piano snap");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys::ctrlModifier,0));d.app.set_midi_notes(clip,savedNotes);d.refresh();roll->sync();
    stage("musical edit dialog");roll->selected={note.value};auto* music=dynamic_cast<juce::TextButton*>(control("musical-edit-open"));check(music!=nullptr,"musical edit entry");music->onClick();auto* musicalPanel=d.windows.back()->getContentComponent();auto musicalControl=[&](const char* name)->juce::Component*{for(auto* child:musicalPanel->getChildren())if(child->getComponentID()==name)return child;throw std::runtime_error("musical control missing");};
    auto* op=dynamic_cast<juce::ComboBox*>(musicalControl("musical-operation"));op->setSelectedId(2,juce::sendNotificationSync);dynamic_cast<juce::TextEditor*>(musicalControl("musical-value"))->setText("12");auto* musicalApply=dynamic_cast<juce::TextButton*>(musicalControl("musical-apply"));musicalApply->onClick();check(d.project()->clips.front().midi->notes.front().pitch==79,"musical transpose UI applies command");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));check(d.project()->clips.front().midi->notes==savedNotes,"musical UI single Undo");
    dynamic_cast<juce::TextEditor*>(musicalControl("musical-value"))->setText("oops");const auto musicalRevision=d.app.services().projects->state().revision;musicalApply->onClick();check(d.app.services().projects->state().revision==musicalRevision,"musical invalid input atomic");
    roll->selected.clear();dynamic_cast<juce::TextEditor*>(musicalControl("musical-value"))->setText("-12");musicalApply->onClick();check(d.app.services().projects->state().revision==musicalRevision,"empty selection does not target whole clip");dynamic_cast<juce::ComboBox*>(musicalControl("musical-scope"))->setSelectedId(2,juce::sendNotificationSync);musicalApply->onClick();check(d.project()->clips.front().midi->notes.front().pitch==55,"whole clip musical scope");roll->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));const auto noOpRevision=d.app.services().projects->state().revision;
    op->setSelectedId(1,juce::sendNotificationSync);dynamic_cast<juce::TextEditor*>(musicalControl("musical-strength"))->setText("0");musicalApply->onClick();check(d.app.services().projects->state().revision==noOpRevision,"zero strength UI no-op");
    for(float scale:{1.f,1.5f}){auto image=musicalPanel->createComponentSnapshot(musicalPanel->getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-musical-100-preview.png":"juce-musical-150-preview.png").createOutputStream();check(stream!=nullptr,"musical preview stream");stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);}
    stage("controller event editor");auto* controllerPanel=control("embedded-controllers");check(controllerPanel->isShowing(),"controllers integrated below piano roll");auto controllerControl=[&](const char* name)->juce::Component*{return control(name);};auto* eventAdd=dynamic_cast<juce::TextButton*>(controllerControl("controller-add"));dynamic_cast<juce::TextEditor*>(controllerControl("controller-value"))->setText("127");eventAdd->onClick();check(d.project()->clips.front().midi->events.size()==1&&d.project()->clips.front().midi->events.front().data1==64,"controller sustain add");const auto sustainId=d.project()->clips.front().midi->events.front().id;
    dynamic_cast<juce::TextEditor*>(controllerControl("controller-position"))->setText("0.5");dynamic_cast<juce::TextEditor*>(controllerControl("controller-value"))->setText("0");dynamic_cast<juce::TextButton*>(controllerControl("controller-apply"))->onClick();check(d.project()->clips.front().midi->events.front().start==mrs::ppq/2&&d.project()->clips.front().midi->events.front().id==sustainId,"controller move retains ID");controllerPanel->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));check(d.project()->clips.front().midi->events.front().data2==127,"controller shared Undo");
    dynamic_cast<juce::ComboBox*>(controllerControl("controller-kind"))->setSelectedId(5,juce::sendNotificationSync);dynamic_cast<juce::TextEditor*>(controllerControl("controller-value"))->setText("8191");eventAdd->onClick();check(d.project()->clips.front().midi->events.size()==2&&d.project()->clips.front().midi->events.back().data1==127&&d.project()->clips.front().midi->events.back().data2==127,"14-bit bend UI");const auto controllerRevision=d.app.services().projects->state().revision;dynamic_cast<juce::TextEditor*>(controllerControl("controller-value"))->setText("8192");dynamic_cast<juce::TextButton*>(controllerControl("controller-apply"))->onClick();check(d.app.services().projects->state().revision==controllerRevision,"invalid bend atomic");
    dynamic_cast<juce::ComboBox*>(controllerControl("controller-kind"))->setSelectedId(3,juce::sendNotificationSync);dynamic_cast<juce::TextEditor*>(controllerControl("controller-value"))->setText("12");eventAdd->onClick();check(d.project()->clips.front().midi->events.back().kind==3&&d.project()->clips.front().midi->events.back().data1==12,"Program UI");dynamic_cast<juce::TextButton*>(controllerControl("controller-delete"))->onClick();check(d.project()->clips.front().midi->events.size()==2,"controller delete");
    dynamic_cast<juce::ComboBox*>(controllerControl("controller-selector"))->setSelectedId(1,juce::sendNotificationSync);auto* lane=controllerControl("controller-lane");const juce::Point<float> eventPoint{64,26};auto controllerMouse=[&](juce::Point<float> point,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,lane,lane,juce::Time::getCurrentTime(),eventPoint,juce::Time::getCurrentTime(),1,true);};const auto eventBaseline=d.project()->clips.front().midi->events;const auto dragRevision=d.app.services().projects->state().revision;lane->mouseDown(controllerMouse(eventPoint,juce::ModifierKeys::leftButtonModifier));lane->mouseDrag(controllerMouse(eventPoint.translated(200,50),juce::ModifierKeys::leftButtonModifier));check(d.app.services().projects->state().revision==dragRevision,"controller drag preview only");lane->mouseUp(controllerMouse(eventPoint.translated(200,50),0));check(d.project()->clips.front().midi->events.front().start>0&&d.project()->clips.front().midi->events.front().data2<127,"controller gesture position/value");controllerPanel->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));check(d.project()->clips.front().midi->events==eventBaseline,"one Undo restores controller drag");lane->mouseDown(controllerMouse(eventPoint,juce::ModifierKeys::leftButtonModifier));lane->mouseDrag(controllerMouse(eventPoint.translated(250,80),juce::ModifierKeys::leftButtonModifier));lane->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));lane->mouseUp(controllerMouse(eventPoint.translated(250,80),0));check(d.project()->clips.front().midi->events==eventBaseline,"controller Escape cancels gesture");
    dynamic_cast<juce::ComboBox*>(controllerControl("controller-kind"))->setSelectedId(2,juce::sendNotificationSync);dynamic_cast<juce::TextEditor*>(controllerControl("controller-number"))->setText("1");for(const auto [beat,val]:std::array<std::pair<int,int>,3>{{{0,10},{1,127},{2,20}}}){dynamic_cast<juce::TextEditor*>(controllerControl("controller-position"))->setText(juce::String(beat));dynamic_cast<juce::TextEditor*>(controllerControl("controller-value"))->setText(juce::String(val));eventAdd->onClick();}check(d.project()->clips.front().midi->events.back().data1==1,"continuous CC1 lane data");
    stage("upd4 controller group");{auto ccBefore=d.project()->clips.front().midi->events;auto controllerPlot=juce::Rectangle<float>(64,26,static_cast<float>(lane->getWidth())-80,static_cast<float>(lane->getHeight())-52);auto cp=[&](const mrs::MidiChannelEvent& event){return juce::Point<float>{controllerPlot.getX()+static_cast<float>(static_cast<double>(event.start)/((notePanel->getWidth()-32-PianoRoll::keys-16)/roll->beatWidth*mrs::ppq))*controllerPlot.getWidth(),controllerPlot.getBottom()-mrs::midi_event_value(event)/127.f*controllerPlot.getHeight()};};std::vector<mrs::MidiChannelEvent> cc;for(const auto& event:ccBefore)if(event.kind==2&&event.data1==1)cc.push_back(event);check(cc.size()==3,"CC group fixture");const auto boxStart=controllerPlot.getTopLeft().translated(-4,-4),boxEnd=controllerPlot.getBottomRight().translated(4,4);lane->mouseDown(controllerMouse(boxStart,juce::ModifierKeys::leftButtonModifier));lane->mouseDrag(controllerMouse(boxEnd,juce::ModifierKeys::leftButtonModifier));lane->mouseUp(controllerMouse(boxEnd,0));const auto groupRevision=d.app.services().projects->state().revision;const auto anchorPoint=cp(cc.front());lane->mouseDown(controllerMouse(anchorPoint,juce::ModifierKeys::leftButtonModifier));lane->mouseDrag(controllerMouse(anchorPoint.translated(80,0),juce::ModifierKeys::leftButtonModifier));check(d.app.services().projects->state().revision==groupRevision,"CC group preview does not edit model");lane->mouseUp(controllerMouse(anchorPoint.translated(80,0),0));auto ccAfter=d.project()->clips.front().midi->events;mrs::Tick shift=-1;for(const auto& original:cc){const auto found=std::find_if(ccAfter.begin(),ccAfter.end(),[&](const auto& event){return event.id==original.id;});check(found!=ccAfter.end(),"group keeps event ID");if(shift<0)shift=found->start-original.start;check(shift>0&&found->start-original.start==shift&&mrs::midi_event_value(*found)==mrs::midi_event_value(original),"CC group preserves timing and values");}controllerPanel->keyPressed(juce::KeyPress('Z',juce::ModifierKeys(juce::ModifierKeys::ctrlModifier),0));check(d.project()->clips.front().midi->events==ccBefore,"single Undo restores CC group");lane->mouseDown(controllerMouse(anchorPoint,juce::ModifierKeys::leftButtonModifier));lane->mouseDrag(controllerMouse(anchorPoint.translated(100,30),juce::ModifierKeys::leftButtonModifier));lane->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));lane->mouseUp(controllerMouse(anchorPoint,0));check(d.project()->clips.front().midi->events==ccBefore,"Escape cancels CC group drag");}
    for(float scale:{1.f,1.5f}){auto image=controllerPanel->createComponentSnapshot(controllerPanel->getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-controller-100-preview.png":"juce-controller-150-preview.png").createOutputStream();check(stream!=nullptr,"controller preview stream");stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);}
    d.app.set_midi_events(clip,{});d.refresh();
    stage("pump timer");const auto until=juce::Time::getMillisecondCounter()+100;while(juce::Time::getMillisecondCounter()<until){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}check(notePanel->isShowing(),"MIDI editor survives engine rebuild timer");
    stage("playback");d.app.play();std::array<float,256> audio{};d.app.engine()->process(nullptr,audio.data(),128);check(audio.back()>0,"authored MIDI plays through VST3");d.app.pause();d.app.engine()->process(nullptr,audio.data(),128);d.app.poll();check(audio.back()==0,"MIDI pause releases notes");
    d.setWorkspace(mrs::desktop::Workspace::arrange);d.arrangement->pixelsPerSecond=240;d.refresh(true);auto rect=d.arrangement->clipRect(d.project()->clips.front());check(rect.getWidth()>400,"MIDI clip tick range visible");
    stage("snapshots");for(float scale:{1.f,1.5f}){auto image=d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-midi-clips-100-preview.png":"juce-midi-clips-150-preview.png").createOutputStream();check(stream!=nullptr,"MIDI arrange preview stream");stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);auto editorImage=notePanel->createComponentSnapshot(notePanel->getLocalBounds(),true,scale,juce::SoftwareImageType{});auto editorStream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-midi-notes-100-preview.png":"juce-midi-notes-150-preview.png").createOutputStream();check(editorStream!=nullptr,"MIDI notes preview stream");editorStream->setPosition(0);editorStream->truncate();juce::PNGImageFormat().writeImageToStream(editorImage,*editorStream);}
    stage("upd4 full screen");auto pump=[](){const auto end=juce::Time::getMillisecondCounter()+80;while(juce::Time::getMillisecondCounter()<end){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}};auto* full=dynamic_cast<juce::TextButton*>(control("editor-fullscreen"));full->onClick();pump();check(notePanel->findParentComponentOfClass<EditorWindow>()->isFullScreen(),"floating MIDI full screen");full->onClick();pump();check(!notePanel->findParentComponentOfClass<EditorWindow>()->isFullScreen(),"full screen restore");
    stage("dock editor");d.selectedClip=clip;d.openSelectedEditor();d.attachClipEditor(notePanel,clip,true);check(d.dockedEditor.get()==notePanel&&d.app.workspace()==mrs::desktop::Workspace::edit,"Edit dock replaces mixer");check(roll->clip.notes==d.project()->clips.front().midi->notes,"dock preserves editor instance");const auto originalMixHeight=d.view.mixerHeight;const auto originalEditHeight=d.view.editorHeight;d.resizeMixer(620);check(d.view.editorHeight>=500&&d.view.mixerHeight==originalMixHeight,"editor resize does not resize mixer");d.setWorkspace(mrs::desktop::Workspace::mix);const auto keptEditHeight=d.view.editorHeight;d.resizeMixer(340);check(d.view.mixerHeight>=340&&d.view.mixerHeight<=380&&d.view.editorHeight==keptEditHeight,"mixer resize does not resize editor");d.setWorkspace(mrs::desktop::Workspace::edit);d.resizeMixer(originalEditHeight);d.setWorkspace(mrs::desktop::Workspace::mix);d.resizeMixer(originalMixHeight);d.setWorkspace(mrs::desktop::Workspace::edit);full->onClick();pump();check(!d.dockedEditor&&d.view.editorAttached&&notePanel->findParentComponentOfClass<EditorWindow>()->isFullScreen(),"attached editor full screen preserves Attach preference and same instance");full->onClick();pump();check(d.dockedEditor.get()==notePanel&&d.view.editorAttached,"full screen restores Attach");d.setWorkspace(mrs::desktop::Workspace::mix);check(!notePanel->isVisible()&&d.master->isVisible(),"Mix replaces attached editor");d.setWorkspace(mrs::desktop::Workspace::edit);check(notePanel->isVisible()&&!d.master->isVisible(),"Edit restores attached panel");for(float scale:{1.f,1.5f}){auto image=d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-edit-dock-100-preview.png":"juce-edit-dock-150-preview.png").createOutputStream();stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);}d.attachClipEditor(notePanel,clip,false);check(!d.dockedEditor&&notePanel->isShowing(),"Detach retains editor instance");
    stage("fix1 global timeline");d.app.move_clip(clip,track,mrs::Timeline(d.project()->time,d.project()->sample_rate).to_samples(2*mrs::ppq));const auto ghost=d.app.create_midi_clip(track,10*mrs::ppq,2*mrs::ppq);mrs::MidiNote ghostNote;ghostNote.id=mrs::new_id();ghostNote.pitch=67;ghostNote.start=0;ghostNote.length=mrs::ppq;d.app.set_midi_notes(ghost,{ghostNote});d.refresh();roll->sync();check(roll->contextClips.size()==2&&roll->clip.start==2*mrs::ppq,"same-track clips in global piano timeline");check(std::abs(roll->noteRect(roll->notes.front()).getX()-(PianoRoll::keys+2*roll->beatWidth))<.01f,"active note global offset");const auto ghostRevision=d.app.services().projects->state().revision;roll->mouseDoubleClick(mouse({PianoRoll::keys+10.5f*roll->beatWidth,roll->noteRect(roll->notes.front()).getCentreY()},juce::ModifierKeys::leftButtonModifier));check(d.app.services().projects->state().revision==ghostRevision,"inactive clip stays read-only");roll->mouseDown(mouse({PianoRoll::keys+10.5f*roll->beatWidth,roll->noteRect(roll->notes.front()).getCentreY()},juce::ModifierKeys::leftButtonModifier));check(d.selectedClip==ghost&&roll->clip.start==10*mrs::ppq,"click ghost activates same editor");dynamic_cast<juce::TextButton*>(control("controller-edit-open"))->onClick();check(!d.view.controllersOpen&&!control("embedded-controllers")->isVisible(),"Controllers closed setting");d.activateClip(clip);check(roll->clip.start==2*mrs::ppq&&!control("embedded-controllers")->isVisible(),"Controllers state follows active clip");dynamic_cast<juce::TextButton*>(control("controller-edit-open"))->onClick();d.app.seek(72000);d.app.engine()->process(nullptr,audio.data(),128);check(roll->keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)),"Space starts shared transport");d.app.engine()->process(nullptr,audio.data(),128);check(d.app.engine()->state().playback==mrs::PlaybackState::playing&&roll->playheadTick()==mrs::Timeline(d.project()->time,d.project()->sample_rate).to_ticks(d.app.engine()->state().sample),"editor Play and playhead share global transport");check(roll->keyPressed(juce::KeyPress(juce::KeyPress::spaceKey)),"Space pauses shared transport");d.app.engine()->process(nullptr,audio.data(),128);check(d.app.engine()->state().playback==mrs::PlaybackState::paused,"editor Pause shares transport");roll->wheel(-.5f,juce::ModifierKeys(juce::ModifierKeys::ctrlModifier|juce::ModifierKeys::shiftModifier),0,0);for(auto* child:notePanel->getChildren())if(auto* viewport=dynamic_cast<juce::Viewport*>(child);viewport&&dynamic_cast<PianoRoll*>(viewport->getViewedComponent()))viewport->setViewPosition(0,static_cast<int>(roll->noteRect(roll->notes.front()).getY())-80);for(float scale:{1.f,1.5f}){auto image=notePanel->createComponentSnapshot(notePanel->getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-editor-timeline-100-preview.png":"juce-editor-timeline-150-preview.png").createOutputStream();stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);}d.action(32);d.app.engine()->process(nullptr,audio.data(),128);d.app.remove_clip(ghost);d.app.move_clip(clip,track,0);d.refresh();roll->sync();
    stage("upd2 editor preferences");d.attachClipEditor(notePanel,clip,true);check(d.view.editorAttached,"Attach remembered");d.toggleEditor();check(!d.clipEditorOpen(),"Edit toggles attached panel closed");d.toggleEditor();check(d.clipEditorOpen()&&d.dockedClip==clip&&d.dockedEditor.get()==notePanel,"Edit reopens attached panel");d.attachClipEditor(notePanel,clip,false);check(!d.view.editorAttached,"Detach remembered");d.toggleEditor();check(!d.clipEditorOpen(),"Edit toggles floating editor closed");d.toggleEditor();check(d.clipEditorOpen()&&!d.dockedEditor,"Edit reopens detached editor");
    stage("duplicate");d.closeEditors();d.action(45);check(d.project()->clips.size()==2&&d.project()->clips.back().midi->start==4*mrs::ppq,"duplicate MIDI menu");d.action(10);check(d.project()->clips.size()==1,"MIDI duplicate undo");d.selectedClip=clip;d.action(46);d.app.engine()->process(nullptr,audio.data(),128);check(d.app.engine()->state().loop.has_value(),"MIDI clip loop menu");
    stage("audio Edit");d.closeEditors();d.app.disconnect();d.app.new_project();const auto wav=folder.getChildFile("audio.wav");{juce::FileOutputStream stream(wav);stream.write("RIFF",4);stream.writeInt(36+192000);stream.write("WAVEfmt ",8);stream.writeInt(16);stream.writeShort(1);stream.writeShort(1);stream.writeInt(48000);stream.writeInt(96000);stream.writeShort(2);stream.writeShort(16);stream.write("data",4);stream.writeInt(192000);for(int i=0;i<96000;++i)stream.writeShort(static_cast<short>(10000*std::sin(i*.05)));}d.app.import_wavs({std::filesystem::path(wav.getFullPathName().toStdString())});d.refresh(true);d.selectedClip=d.project()->clips.front().id;d.openSelectedEditor();auto* audioEditor=d.windows.back()->getContentComponent();check(audioEditor->getComponentID()=="audio-clip-editor","Edit opens audio editor");juce::TextButton* audioApply=nullptr;for(auto* child:audioEditor->getChildren()){if(child->getComponentID()=="audio-trim-start")dynamic_cast<juce::TextEditor*>(child)->setText("0.25");if(child->getComponentID()=="audio-trim-apply")audioApply=dynamic_cast<juce::TextButton*>(child);}check(audioApply!=nullptr,"audio trim action");const auto audioBefore=d.project()->clips.front();audioApply->onClick();check(d.project()->clips.front().start==12000,"audio editor trims through shared command");d.app.undo();d.refresh();check(d.project()->clips.front().start==audioBefore.start&&d.project()->clips.front().length==audioBefore.length,"audio editor shared Undo");d.attachClipEditor(audioEditor,*d.selectedClip,true);check(d.dockedEditor.get()==audioEditor,"audio Attach");for(float scale:{1.f,1.5f}){auto image=d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-audio-edit-100-preview.png":"juce-audio-edit-150-preview.png").createOutputStream();stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);}d.attachClipEditor(audioEditor,*d.selectedClip,false);
    stage("upd2 mixed clip selection");d.closeEditors();const auto audioId=d.project()->clips.front().id;const auto selectionTrack=d.app.add_instrument_track("Selection keys");const auto selectionMidi=d.app.create_midi_clip(selectionTrack,0,2*mrs::ppq);d.refresh(true);d.arrangement->pixelsPerSecond=180;d.arrangement->horizontal=0;d.arrangement->vertical=0;d.arrangement->setSize(1200,650);d.arrangement->resized();auto arrangementMouse=[&](juce::Point<float> point,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,d.arrangement.get(),d.arrangement.get(),juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,true);};d.selectedClip=audioId;d.openSelectedEditor();const auto midiRect=d.arrangement->clipRect(d.project()->clips.back());d.arrangement->mouseDown(arrangementMouse(midiRect.getCentre(),juce::ModifierKeys::leftButtonModifier));d.arrangement->mouseUp(arrangementMouse(midiRect.getCentre(),0));check(d.selectedClip==selectionMidi&&d.clipEditorOpen(),"Arrange click changes open editor audio to MIDI");const auto sampleBeforeSelection=d.app.engine()->state().sample;const auto selectionRevision=d.app.services().projects->state().revision;d.arrangement->mouseDown(arrangementMouse({900,600},juce::ModifierKeys::leftButtonModifier));d.arrangement->mouseDrag(arrangementMouse({251,71},juce::ModifierKeys::leftButtonModifier));for(float scale:{1.f,1.5f}){auto image=d.arrangement->createComponentSnapshot(d.arrangement->getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-clip-selection-100-preview.png":"juce-clip-selection-150-preview.png").createOutputStream();juce::PNGImageFormat().writeImageToStream(image,*stream);}d.arrangement->mouseUp(arrangementMouse({251,71},0));check(d.selectedClips.size()==2&&d.selectedClips.contains(audioId.value)&&d.selectedClips.contains(selectionMidi.value)&&d.app.engine()->state().sample==sampleBeforeSelection&&d.app.services().projects->state().revision==selectionRevision,"left drag selects audio and MIDI without seek or mutation");const auto clipsBeforeMove=d.project()->clips;const auto groupPoint=d.arrangement->clipRect(clipsBeforeMove.front()).getCentre();d.arrangement->mouseDown(arrangementMouse(groupPoint,juce::ModifierKeys::leftButtonModifier));d.arrangement->mouseDrag(arrangementMouse(groupPoint.translated(90,0),juce::ModifierKeys::leftButtonModifier));check(d.project()->clips==clipsBeforeMove,"group move preview does not mutate model");for(float scale:{1.f,1.5f}){auto image=d.arrangement->createComponentSnapshot(d.arrangement->getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-clip-group-100-preview.png":"juce-clip-group-150-preview.png").createOutputStream();juce::PNGImageFormat().writeImageToStream(image,*stream);}d.arrangement->mouseUp(arrangementMouse(groupPoint.translated(90,0),0));const mrs::Timeline groupTime(d.project()->time,d.project()->sample_rate);for(std::size_t i=0;i<clipsBeforeMove.size();++i)check(mrs::clip_start(d.project()->clips[i],groupTime)==mrs::clip_start(clipsBeforeMove[i],groupTime)+24000,"group move preserves relative audio MIDI position");d.action(10);check(d.project()->clips==clipsBeforeMove,"one Undo restores group move");d.action(11);check(d.project()->clips!=clipsBeforeMove,"group move Redo");d.action(10);d.arrangement->mouseDown(arrangementMouse(groupPoint,juce::ModifierKeys::leftButtonModifier));d.arrangement->mouseDrag(arrangementMouse(groupPoint.translated(90,0),juce::ModifierKeys::leftButtonModifier));check(d.arrangement->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),"Escape cancels group gesture");d.arrangement->mouseUp(arrangementMouse(groupPoint.translated(90,0),0));check(d.project()->clips==clipsBeforeMove,"cancelled group move does not commit");const auto groupRevision=d.app.services().projects->state().revision;bool badGroup=false;try{d.app.move_clips({audioId,selectionMidi},-1);}catch(const std::exception&){badGroup=true;}check(badGroup&&d.app.services().projects->state().revision==groupRevision&&d.project()->clips==clipsBeforeMove,"negative group move rejected atomically");d.action(25);check(d.project()->clips.empty(),"Delete selection removes both clips");d.action(10);check(d.project()->clips.size()==2,"one Undo restores selected clips");
    stage("cleanup");d.closeEditors();d.app.disconnect();d.app.new_project();d.refresh(true);check(folder.getParentDirectory()==temp&&folder.getFileName().startsWith("mrs-midi4b"),"MIDI smoke cleanup scope");folder.deleteRecursively();if(peer)d.removeFromDesktop();stage("passed");
}
void midi4aSmoke(Desktop& d,const juce::File& fixture){
    auto stage=[](const char* text){juce::File::getCurrentWorkingDirectory().getChildFile("midi-fix1-smoke-stage.txt").replaceWithText(text);};stage("begin");
    auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    if(fixture==juce::File())return;
    const bool addedPeer=!d.getPeer();if(addedPeer)d.addToDesktop(0);
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mrs-midi4a","",false);folder.createDirectory();
    const auto generator=folder.getChildFile("instrument.vst3");check(fixture.copyFileTo(generator),"instrument fixture copied");
    {
        Desktop dropUI(true);dropUI.app.new_project();dropUI.app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1},2});dropUI.refresh(true);
        dropUI.catalog=mrs::processing::probe_vst3(generator.getFullPathName().toStdString());
        auto details=[&](juce::String text,juce::Point<int> point){return juce::DragAndDropTarget::SourceDetails(text,dropUI.browser.get(),point);};
        auto drop=details("vst3:0",{500,Arrangement::header+10});const auto empty=*dropUI.project();const auto revision=dropUI.app.services().projects->state().revision;
        check(dropUI.arrangement->isInterestedInDragSource(drop),"empty Arrange accepts VST3 drop");
        check(!dropUI.arrangement->isInterestedInDragSource(details("vst3:999",{500,Arrangement::header+10}))&&!dropUI.arrangement->isInterestedInDragSource(details("vst3:x",{500,Arrangement::header+10})),"invalid browser payload rejected");
        dropUI.setVisible(true);
        // JUCE findTarget passes SourceDetails before it computes target-local cursor coordinates.
        const auto unresolved=juce::DragAndDropTarget::SourceDetails("vst3:0",dropUI.browser.get(),{});
        auto discover=[&](juce::Point<int> point){for(auto* hit=dropUI.getComponentAt(point);hit;hit=hit->getParentComponent())if(auto* target=dynamic_cast<juce::DragAndDropTarget*>(hit))if(target->isInterestedInDragSource(unresolved))return target;return static_cast<juce::DragAndDropTarget*>(nullptr);};
        check(discover(dropUI.arrangeArea.getTopLeft()+drop.localPosition)==dropUI.arrangement.get(),"JUCE finds empty Arrange target before local cursor is supplied");
        check(discover(dropUI.mixArea.getCentre())==static_cast<juce::DragAndDropTarget*>(&dropUI),"JUCE finds empty Mix target before local cursor is supplied");
        dropUI.arrangement->itemDropped(details("vst3:0",{500,10}));check(*dropUI.project()==empty,"ruler drop still leaves project unchanged");
        dropUI.arrangement->itemDropped(drop);dropUI.closeEditors();
        check(dropUI.project()->tracks.size()==1&&dropUI.project()->tracks.back().kind==mrs::TrackKind::instrument&&dropUI.project()->tracks.back().inserts.size()==1&&dropUI.selectedTrack==dropUI.project()->tracks.back().id,"one drop creates selected assigned instrument track");
        check(dropUI.app.services().projects->state().revision==revision+1,"create assign is one command");
        check(dropUI.arrangement->isInterestedInDragSource(drop),"instrument timeline accepts replacement drop");
        const auto assigned=*dropUI.project();dropUI.action(10);check(*dropUI.project()==empty,"single drop Undo removes complete track");dropUI.action(11);check(*dropUI.project()==assigned,"drop Redo restores track and plugin");
        dropUI.setSize(1400,1200);dropUI.catalog=mrs::processing::probe_vst3(fixture.getFullPathName().toStdString());auto below=details("vst3:0",{500,Arrangement::header+dropUI.arrangement->trackHeight+20});
        const auto errorRevision=dropUI.app.services().projects->state().revision;dropUI.arrangement->itemDropped(below);
        check(*dropUI.project()==assigned&&dropUI.app.services().projects->state().revision==errorRevision&&!dropUI.message.isEmpty(),"effect drop reports error without partial track");
        auto existing=details("vst3:0",{50,50});check(dropUI.arrangement->rows.front()->isInterestedInDragSource(existing),"compact Arrange strip accepts plugin drops");dropUI.mixer.front()->itemDropped(existing);
        // Mixer drops commit after JUCE releases its drag source/target pointers.
        for(int dispatch=0;dispatch<10;++dispatch){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}
        dropUI.closeEditors();
        check(dropUI.project()->tracks.size()==1&&dropUI.project()->tracks.front().inserts.size()==2,"existing mixer channel drop preserves insert behavior");dropUI.action(10);
        dropUI.catalog=mrs::processing::probe_vst3(generator.getFullPathName().toStdString());dropUI.arrangement->itemDropped(below);dropUI.closeEditors();
        check(dropUI.project()->tracks.size()==2&&dropUI.project()->tracks.back().kind==mrs::TrackKind::instrument,"blank area below existing tracks creates one more instrument");
        const auto instrument=dropUI.project()->tracks.back().id;
        auto effects=dropUI.chain(instrument);mrs::NativeInsert gain;gain.id=mrs::new_id();effects.push_back(gain);dropUI.app.set_inserts(instrument,effects);dropUI.refresh(true);
        const auto original=*dropUI.project();const auto originalRevision=dropUI.app.services().projects->state().revision;
        dropUI.dropPlugin(instrument,0);
        juce::AlertWindow* replace=nullptr;for(int n=0;n<juce::Component::getNumCurrentlyModalComponents();++n)if(auto* a=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent(n)))replace=a;
        check(replace!=nullptr&&replace->getButton(0)->getButtonText()=="Replace"&&replace->getButton(1)->getButtonText()=="Do not touch","instrument replacement dialog choices");
        replace->exitModalState(0);for(int dispatch=0;dispatch<10;++dispatch){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}
        check(*dropUI.project()==original&&dropUI.app.services().projects->state().revision==originalRevision,"Do not touch leaves state unchanged");
        dropUI.dropPlugin(instrument,0);replace=nullptr;for(int n=0;n<juce::Component::getNumCurrentlyModalComponents();++n)if(auto* a=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent(n)))replace=a;check(replace!=nullptr,"second replacement dialog");replace->exitModalState(1);
        for(int dispatch=0;dispatch<10;++dispatch){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}
        dropUI.closeEditors();check(dropUI.chain(instrument).size()==2&&dropUI.chain(instrument).back()==gain&&dropUI.chain(instrument).front().id!=original.tracks.back().inserts.front().id,"replacement keeps downstream effects");dropUI.action(10);check(*dropUI.project()==original,"replacement single Undo");dropUI.action(11);
        dropUI.setSize(1400,1100);auto leftBlank=details("vst3:0",{10,Arrangement::header+static_cast<int>(dropUI.project()->tracks.size())*dropUI.arrangement->trackHeight+10});check(dropUI.arrangement->isInterestedInDragSource(leftBlank),"blank left header accepts VSTi");dropUI.arrangement->itemDropped(leftBlank);dropUI.closeEditors();check(dropUI.project()->tracks.size()==3,"left blank creates instrument");dropUI.action(10);
        auto mixBlank=details("vst3:0",dropUI.mixArea.getCentre());check(dropUI.isInterestedInDragSource(mixBlank),"empty mixer accepts VSTi");dropUI.itemDropped(mixBlank);for(int dispatch=0;dispatch<10;++dispatch){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}dropUI.closeEditors();check(dropUI.project()->tracks.size()==3,"empty mixer creates instrument");dropUI.action(10);
        const auto save=folder.getChildFile("drop.mrsproject");const auto savePath=std::filesystem::path(save.getFullPathName().toWideCharPointer());dropUI.app.save_project(savePath);dropUI.app.open_project(savePath);dropUI.refresh(true);
        check(dropUI.project()->tracks.size()==2&&dropUI.project()->tracks.back().inserts.front().class_id==dropUI.catalog.front().class_id,"drop Save Open retains instrument assignment");
        for(float scale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-instrument-drop-"+juce::String(static_cast<int>(scale*100))+"-preview.png").createOutputStream();check(stream!=nullptr,"drop preview stream");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(dropUI.createComponentSnapshot(dropUI.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*stream),"drop preview");}
        dropUI.app.disconnect();
    }
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
    check(d.mixer.front()->pluginInsertionAt(d.mixer.front()->pluginDropArea().getTopLeft())==1,"effect drop preserves instrument first slot");
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
    auto mixerInsert=[&](juce::Component& strip,const mrs::Id& slot){
        std::function<MixerInsert*(juce::Component&)> find=[&](juce::Component& node)->MixerInsert*{if(node.getComponentID()=="mixer-insert:"+juce::String(slot.value))return dynamic_cast<MixerInsert*>(&node);for(auto* child:node.getChildren())if(auto* found=find(*child))return found;return nullptr;};
        auto* row=find(strip);check(row!=nullptr,"mixer insert row found by stable slot identity");return row;
    };
    const auto beforeReorder=*d.project();
    const auto targetBounds=d.mixer[2]->getLocalBounds();
    juce::DragAndDropTarget::SourceDetails outsideDrop("mrs-track:"+juce::String(first.value),d.arrangement->rows[0].get(),{targetBounds.getRight()+1,targetBounds.getCentreY()});
    d.mixer[2]->itemDropped(outsideDrop);pump();check(*d.project()==beforeReorder,"outside mixer drop cannot reorder tracks");
    juce::DragAndDropTarget::SourceDetails trackDrop("mrs-track:"+juce::String(first.value),d.arrangement->rows[0].get(),{targetBounds.getRight()-2,targetBounds.getCentreY()});
    check(d.mixer[2]->isInterestedInDragSource(trackDrop),"mixer accepts track drag from arrangement");d.mixer[2]->itemDropped(trackDrop);pump();
    check(d.project()->tracks.back().id==first&&d.mixer.back()->target==first&&d.arrangement->rows.back()->target==first,"reorder synchronizes both views");
    check(d.project()->clips==beforeReorder.clips,"reorder retains clip identities and positions");d.action(10);check(*d.project()==beforeReorder,"reorder undo");
    juce::DragAndDropTarget::SourceDetails reverseDrop("mrs-track:"+juce::String(bus.value),d.mixer[2].get(),{400,Arrangement::header});
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
    // An ARA-enabled build always has an empty ARA 2 category, even when no
    // plugins are scanned. Only vendor/plugin entries must be absent here.
    auto* emptyRoot=d.browser->tree.getRootItem();
#ifdef MRS_HAS_ARA
    check(emptyRoot->getNumSubItems()==1&&emptyRoot->getSubItem(0)->getUniqueName()=="ARA 2"&&emptyRoot->getSubItem(0)->getNumSubItems()==0,"empty ARA browser category");
#else
    check(emptyRoot->getNumSubItems()==0,"empty browser");
#endif
    d.catalog={{"missing.vst3","00000000000000000000000000000001","Example","Example vendor","1"}};d.browser->rebuild();
    auto* populatedRoot=d.browser->tree.getRootItem();
    check(populatedRoot->getNumSubItems()>0&&!populatedRoot->getSubItem(populatedRoot->getNumSubItems()-1)->isOpen(),"vendors initially collapsed");d.catalog.clear();d.browser->rebuild();
    auto originalClip=d.project()->clips.front();d.app.move_clip(originalClip.id,second,4800);d.selectedClip=originalClip.id;d.app.seek(10000);
    std::array<float,256> audio{};auto render=[&]{d.app.engine()->process(nullptr,audio.data(),128);d.app.poll();};render();d.action(24);
    check(d.project()->clips.size()==2,"split binding");d.action(10);check(d.project()->clips.size()==1,"split undo");
    const auto emptyFaderHeight=d.mixer[1]->gain.getHeight();const auto emptyMixerHeight=d.mixArea.getHeight();
    // A channel colour swatch is a deliberately custom-painted TextButton,
    // not a flat mixer button. All ordinary buttons still share flat chrome.
    auto checkFlatButtons=[&](Strip& strip,const char* failure){
        check(strip.colorField.getComponentID()=="channel-color","Channel custom swatch identity");
        for(auto* child:strip.getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child)){
            if(button==&strip.colorField)continue;
            check(static_cast<bool>(button->getProperties()["flatMixerButton"]),failure);
        }
    };
    for(const auto& strip:d.mixer)checkFlatButtons(*strip,"channel buttons use flat mixer style");
    checkFlatButtons(*d.master,"Master buttons use flat mixer style");
    {juce::TextButton flat;flat.setLookAndFeel(&d.theme);flat.getProperties().set("flatMixerButton",true);flat.setSize(80,28);const auto picture=flat.createComponentSnapshot(flat.getLocalBounds(),true,1.f,juce::SoftwareImageType{});const auto expected=juce::Colour(raised);check(picture.getPixelAt(4,8)==expected&&picture.getPixelAt(4,20)==expected,"flat mixer button uses reference raised fill without bevel");flat.setLookAndFeel(nullptr);}
    for(auto* strip:std::array<Strip*,3>{d.mixer[0].get(),d.mixer[1].get(),d.master.get()}){
        const auto snapshot=strip->createComponentSnapshot(strip->getLocalBounds(),true,1.f,juce::SoftwareImageType{});
        check(snapshot.getPixelAt(strip->getWidth()-2,35)==juce::Colour(panel),"Mix/Master background matches track channel");
    }
    {juce::Image image(juce::Image::ARGB,100,60,true,juce::SoftwareImageType{});{juce::Graphics g(image);d.theme.drawPopupMenuBackground(g,100,60);}
        check(std::abs(image.getPixelAt(20,20).getFloatAlpha()-.7f)<.01f&&image.getPixelAt(0,20)==juce::Colour(menuBorder),"menu browser surface and subtle border");}
    {
        auto menu=d.pluginMenu();menu.setLookAndFeel(&d.theme);menu.addSeparator();menu.addItem(9001,"Disabled item",false);menu.addItem(9002,"Selected item",true,true);
        if(!d.getPeer())d.addToDesktop(0);d.setVisible(true);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&d).withParentComponent(&d).withMinimumWidth(280),[](int){});
        auto* popup=d.getChildComponent(d.getNumChildComponents()-1);check(popup!=nullptr,"plugin popup renders with shared theme");
        for(float previewScale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-plugin-menu-"+juce::String(static_cast<int>(previewScale*100))+"-preview.png").createOutputStream();check(stream!=nullptr,"plugin menu snapshot stream");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(popup->createComponentSnapshot(popup->getLocalBounds(),true,previewScale,juce::SoftwareImageType{}),*stream),"plugin menu snapshot");}
        juce::PopupMenu::dismissAllActiveMenus();
    }
    mrs::NativeInsert eq;eq.id=mrs::new_id();eq.kind=mrs::InsertKind::channel_eq;d.applyChain(second,{eq});
    check(d.mixer[1]->gain.getHeight()==emptyFaderHeight&&d.mixArea.getHeight()>emptyMixerHeight,"inserts grow header without shrinking faders");
    {
        const auto generation=d.app.insert_generation();const auto revision=d.app.services().projects->state().revision;
        auto* row=mixerInsert(*d.mixer[1],eq.id);const auto visibleEditors=std::count_if(d.windows.begin(),d.windows.end(),[](const auto& w){return w->isVisible();});
        check(row->getComponentAt(row->bypass.getBounds().getCentre())==&row->bypass&&row->getComponentAt(row->editor.getBounds().getCentre())==&row->editor,"mixer bypass/editor hit regions are separate");
        check(row->bypass.getParentComponent()==&row->editor&&row->editor.getBounds()==row->getLocalBounds()&&row->bypass.getRight()<17,"power lies inside shared plugin surface before text inset");
        check(row->bypass.getWidth()==13&&row->bypass.getHeight()==13&&row->bypass.getButtonText().isEmpty()&&row->bypass.iconColour()==juce::Colour(0xff3098ee)&&row->bypass.iconColour().getPerceivedBrightness()>juce::Colour(accent).getPerceivedBrightness(),"compact transparent power icon is brighter blue when active");
        row->bypass.triggerClick();pump();render();
        check(d.chain(second).front().bypass&&d.app.services().projects->state().revision==revision+1&&d.app.insert_generation()==generation,"native mixer bypass single Undo without graph rebuild");
        check(mixerInsert(*d.mixer[1],eq.id)->bypass.getToggleState()&&std::count_if(d.windows.begin(),d.windows.end(),[](const auto& w){return w->isVisible();})==visibleEditors,"mixer bypass reflects model and does not open editor");
        check(mixerInsert(*d.mixer[1],eq.id)->bypass.iconColour()==juce::Colour(0xffbdc6cf),"bypassed power icon is brighter neutral grey");
        check(mrs::deserialize(mrs::serialize(*d.project()))==*d.project(),"mixer bypass project serialization round trip");
        d.action(10);check(!d.chain(second).front().bypass&&!mixerInsert(*d.mixer[1],eq.id)->bypass.getToggleState(),"mixer bypass Undo refresh");
        d.action(11);check(d.chain(second).front().bypass&&mixerInsert(*d.mixer[1],eq.id)->bypass.getToggleState(),"mixer bypass Redo refresh");d.action(10);
        d.app.play();render();mixerInsert(*d.mixer[1],eq.id)->bypass.triggerClick();pump();render();
        check(d.chain(second).front().bypass&&d.app.engine()->state().playback==mrs::PlaybackState::playing,"native mixer bypass during playback");d.app.stop();render();d.action(10);
        auto busFx=eq;busFx.id=mrs::new_id();auto masterFx=eq;masterFx.id=mrs::new_id();d.applyChain(bus,{busFx});d.applyChain(std::nullopt,{masterFx});
        mixerInsert(*d.mixer[2],busFx.id)->bypass.triggerClick();pump();mixerInsert(*d.master,masterFx.id)->bypass.triggerClick();pump();render();
        check(d.chain(bus).front().bypass&&d.chain(std::nullopt).front().bypass&&!d.chain(second).front().bypass,"bus/Master mixer bypass targets isolated");
        for(const float bypassScale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(bypassScale==1.f?"juce-mixer-bypass-100-preview.png":"juce-mixer-bypass-150-preview.png").createOutputStream();check(stream!=nullptr,"mixer bypass preview stream");stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(d.createComponentSnapshot(d.getLocalBounds(),true,bypassScale,juce::SoftwareImageType{}),*stream);}
        d.applyChain(bus,{});d.applyChain(std::nullopt,{});
        // A queued click must not mutate a slot removed before dispatch.
        mixerInsert(*d.mixer[1],eq.id)->bypass.onClick();d.applyChain(second,{});const auto retiredRevision=d.app.services().projects->state().revision;pump();
        check(d.app.services().projects->state().revision==retiredRevision&&d.message.isEmpty(),"retired mixer insert queued action is harmless");d.applyChain(second,{eq});
    }
    std::vector<mrs::NativeInsert> longChain{eq};for(int i=0;i<7;++i){auto effect=eq;effect.id=mrs::new_id();longChain.push_back(effect);}d.applyChain(second,longChain);
    check(d.mixer[1]->gain.getHeight()==emptyFaderHeight&&d.mixer[0]->gain.getHeight()==emptyFaderHeight,"long insert chain retains equal fixed fader heights");
    const auto originalMixerSetting=d.view.mixerHeight;const auto originalEditorSetting=d.view.editorHeight;
    d.resizeMixer(10000);check(d.mixArea.getHeight()==d.getHeight()-292-20,"mixer maximum follows window, not insert count");
    const auto largestDock=d.mixArea.getHeight();d.resizeMixer(260);
    check(d.mixArea.getHeight()<largestDock&&d.mixer[1]->gain.getHeight()==emptyFaderHeight&&d.master->gain.getHeight()==emptyFaderHeight,"dock contraction does not change track or Master faders");
    const auto insertTop=d.mixer[1]->getLocalPoint(mixerInsert(*d.mixer[1],eq.id),juce::Point<int>{});
    d.resizeMixer(10000);check(d.mixer[1]->getLocalPoint(mixerInsert(*d.mixer[1],eq.id),juce::Point<int>{})==insertTop,"insert rows stay pinned to top while empty area expands");d.applyChain(second,{eq});
    d.action(10);check(d.mixer[1]->gain.getHeight()==emptyFaderHeight,"insert Undo keeps fixed fader");d.action(11);
    d.resizeMixer(10000);check(d.mixArea.getHeight()==largestDock,"short chain permits same free expansion as long chain");
    auto otherChain=longChain;for(auto& effect:otherChain)effect.id=mrs::new_id();
    d.applyChain(bus,otherChain);d.resizeMixer(10000);check(d.mixArea.getHeight()==largestDock,"Bus does not impose content cap");d.applyChain(bus,{});
    for(auto& effect:otherChain)effect.id=mrs::new_id();
    d.applyChain(std::nullopt,otherChain);d.resizeMixer(10000);check(d.mixArea.getHeight()==largestDock,"Master does not impose content cap");
    const auto savedWindow=d.getBounds();d.setSize(1200,700);d.resizeMixer(10000);
    check(d.arrangeArea.getHeight()>=140&&d.master->gain.getHeight()==emptyFaderHeight&&d.master->gain.getY()>50&&d.master->gain.getBottom()<d.master->getHeight(),"small window keeps fixed faders and arrangement usable");
    for(const float heightScale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(heightScale==1.f?"juce-mixer-height-small-100-preview.png":"juce-mixer-height-small-150-preview.png").createOutputStream();check(stream!=nullptr,"mixer height snapshot stream");stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(d.createComponentSnapshot(d.getLocalBounds(),true,heightScale,juce::SoftwareImageType{}),*stream);}
    d.setBounds(savedWindow);d.applyChain(std::nullopt,{});d.applyChain(second,{});d.resizeMixer(10000);
    check(d.mixArea.getHeight()==largestDock,"empty inserts area freely expands to window limit");
    d.applyChain(second,{eq});d.resizeMixer(originalMixerSetting);
    check(d.view.editorHeight==originalEditorSetting,"mixer limit does not alter saved Edit height");d.openInsert(second,eq.id);
    check(!d.windows.empty()&&d.windows.back()->isVisible(),"native editor bridge");d.closeEditors();
    d.openInsert(second,eq.id);auto* editor=d.windows.back()->getContentComponent();referencePreview(*editor,"native-eq-theme");
    auto eqEvent=[&](juce::Point<float> p,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(mods),1,0,0,0,0,editor,editor,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,true);};
    const auto eqRevision=d.app.services().projects->state().revision;d.app.play();render();
    const float midX=24+static_cast<float>(std::log(1000./20)/std::log(1000.))*660;
    editor->mouseDown(eqEvent({midX,193},juce::ModifierKeys::leftButtonModifier));editor->mouseDrag(eqEvent({midX+30,173},juce::ModifierKeys::leftButtonModifier));
    check(d.app.services().projects->state().revision==eqRevision,"live EQ preview");editor->mouseUp(eqEvent({midX+30,173},0));render();
    check(d.chain(second).front().bands[2].gain>0&&d.app.services().projects->state().revision==eqRevision+1,"live EQ commit");
    d.app.stop();render();d.closeEditors();
    auto effects=d.chain(second);mrs::NativeInsert legacy;legacy.id=mrs::new_id();legacy.kind=mrs::InsertKind::eq;legacy.gain=4;effects.push_back(legacy);d.applyChain(second,effects);d.openInsert(second,legacy.id);
    auto* legacyEditor=d.windows.back()->getContentComponent();auto* valueEditor=dynamic_cast<juce::TextEditor*>(legacyEditor->findChildWithID("native-gain"));auto* applyButton=dynamic_cast<juce::TextButton*>(legacyEditor->findChildWithID("apply-native"));
    check(valueEditor&&applyButton&&valueEditor->getText().getFloatValue()==4,"legacy EQ dB display");valueEditor->setText("6",false);applyButton->onClick();check(d.chain(second).back().gain==6,"legacy EQ dB commit");
    {
        auto* effectWindow=d.windows.back().get();effectWindow->toFront(true);legacyEditor->grabKeyboardFocus();
        const auto root=static_cast<HWND>(effectWindow->getPeer()->getNativeHandle());SetFocus(root);d.app.stop();render();
        auto space=[&](bool repeat=false){PostMessageW(root,WM_KEYDOWN,VK_SPACE,repeat?(LPARAM{1}<<30):1);};
        space();space(true);PostMessageW(root,WM_KEYUP,VK_SPACE,LPARAM{1}<<31);pump();render();
        check(d.app.engine()->state().playback==mrs::PlaybackState::playing,"audio effect parameter editor Space starts once");
        space();PostMessageW(root,WM_KEYUP,VK_SPACE,LPARAM{1}<<31);pump();render();
        check(d.app.engine()->state().playback==mrs::PlaybackState::stopped,"audio effect parameter editor Space stops");
        valueEditor->grabKeyboardFocus();valueEditor->setCaretPosition(valueEditor->getText().length());space();PostMessageW(root,WM_CHAR,L' ',1);PostMessageW(root,WM_KEYUP,VK_SPACE,LPARAM{1}<<31);pump();render();
        check(valueEditor->getText().containsChar(' ')&&d.app.engine()->state().playback==mrs::PlaybackState::stopped,"audio parameter text Space preserved");
        legacyEditor->grabKeyboardFocus();space();MSG queued{};check(PeekMessageW(&queued,root,WM_KEYDOWN,WM_KEYDOWN,PM_REMOVE)!=0,"queued effect Space fixture");
        d.closeEditors();pump();render();check(d.app.engine()->state().playback==mrs::PlaybackState::stopped,"retired audio editor cancels queued Space");
    }
    effects.pop_back();d.applyChain(second,effects);
    if(fixture!=juce::File()){
        d.catalog=mrs::processing::probe_vst3(std::string(fixture.getFullPathName().toUTF8()));check(!d.catalog.empty(),"fixture probe");
        juce::DragAndDropTarget::SourceDetails drop("vst3:0",d.browser.get(),{50,50});
        const auto revision=d.app.services().projects->state().revision;d.mixer[1]->itemDropped(drop);pump();
        check(d.app.services().projects->state().revision==revision+1,"plugin drop commits once");
        d.closeEditors();d.resetDevice();
        // resetDevice starts the threaded Offline clock. All render() calls in
        // this regression must instead have a single, manually driven owner.
        d.app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});
        d.applyChain(second,d.chain(second));
        auto plugin=d.chain(second).back();d.openInsert(second,plugin.id);check(d.windows.back()->isVisible()&&static_cast<bool>(d.windows.back()->getProperties()["mrs-native-editor"]),"VST3 HWND bridge after offline insert rebuild");
        auto* pluginWindow=d.windows.back().get();
        {
            const auto host=reinterpret_cast<HWND>(static_cast<std::intptr_t>(static_cast<juce::int64>(pluginWindow->getProperties()["mrs-native-host"])));
            const auto child=GetWindow(host,GW_CHILD);check(child!=nullptr,"Space fixture native child");
            pluginWindow->toFront(true);SetFocus(child);d.app.stop();render();
            auto postSpace=[&](HWND target,bool repeat=false){PostMessageW(target,WM_KEYDOWN,VK_SPACE,repeat?(LPARAM{1}<<30):1);};
            postSpace(child);postSpace(child,true);PostMessageW(child,WM_KEYUP,VK_SPACE,LPARAM{1}<<31);pump();render();
            check(d.app.engine()->state().playback==mrs::PlaybackState::playing,"native editor Space starts once, repeat ignored");
            postSpace(child);PostMessageW(child,WM_KEYUP,VK_SPACE,LPARAM{1}<<31);pump();render();
            check(d.app.engine()->state().playback==mrs::PlaybackState::stopped,"native editor Space uses shared stop policy");
            auto edit=CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE,0,0,100,20,host,nullptr,GetModuleHandleW(nullptr),nullptr);check(edit!=nullptr,"native text fixture");SetFocus(edit);
            postSpace(edit);PostMessageW(edit,WM_CHAR,L' ',1);PostMessageW(edit,WM_KEYUP,VK_SPACE,LPARAM{1}<<31);pump();render();wchar_t text[8]{};GetWindowTextW(edit,text,8);
            check(text[0]==L' '&&d.app.engine()->state().playback==mrs::PlaybackState::stopped,"native text field keeps Space without transport");DestroyWindow(edit);SetFocus(child);
            BYTE keys[256]{};GetKeyboardState(keys);auto modified=std::array<BYTE,256>{};std::copy_n(keys,256,modified.begin());modified[VK_CONTROL]=0x80;SetKeyboardState(modified.data());postSpace(child);PostMessageW(child,WM_KEYUP,VK_SPACE,LPARAM{1}<<31);pump();SetKeyboardState(keys);render();
            check(d.app.engine()->state().playback==mrs::PlaybackState::stopped,"modified native Space is not transport");
            juce::Component modal;modal.enterModalState(false);postSpace(child);PostMessageW(child,WM_KEYUP,VK_SPACE,LPARAM{1}<<31);pump();render();
            check(d.app.engine()->state().playback==mrs::PlaybackState::stopped,"modal native Space is not transport");modal.exitModalState(0);pump();
            const auto outside=CreateWindowW(L"STATIC",L"Unrelated fixture window",WS_POPUP,0,0,20,20,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);SetFocus(outside);postSpace(outside);PostMessageW(outside,WM_KEYUP,VK_SPACE,LPARAM{1}<<31);pump();render();
            check(d.app.engine()->state().playback==mrs::PlaybackState::stopped,"unrelated native window is untouched");DestroyWindow(outside);SetFocus(child);
        }
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
        referencePreview(*pinned->getContentComponent(),"host-editor-theme");fixStage("other editor");
        d.openInsert(std::nullopt,peerPlugin.id);auto* other=d.windows.back().get();
        auto hostOf=[](EditorWindow* w){return reinterpret_cast<HWND>(static_cast<std::intptr_t>(static_cast<juce::int64>(w->getProperties()["mrs-native-host"])));};
        check(pinned->isVisible()&&other->isVisible()&&IsWindow(GetWindow(hostOf(pinned),GW_CHILD))&&IsWindow(GetWindow(hostOf(other),GW_CHILD)),"Pin permits two independent native editors");
        {
            const auto original=d.chain(second);auto reordered=original;std::reverse(reordered.begin(),reordered.end());
            const auto generation=d.app.insert_generation();const auto child=GetWindow(hostOf(pinned),GW_CHILD);const auto otherChild=GetWindow(hostOf(other),GW_CHILD);
            const auto store=d.app.services().projects;const auto dropRevision=store->state().revision;d.insertDrag=Desktop::InsertDrag{store,dropRevision,second,plugin.id};
            const auto area=d.mixer[1]->pluginDropArea();juce::DragAndDropTarget::SourceDetails details(juce::var("mrs-insert:"+juce::String::fromUTF8(plugin.id.value.c_str())),mixerInsert(*d.mixer[1],plugin.id),{area.getX()+2,area.getY()+2});
            d.mixer[1]->itemDragEnter(details);check(store->state().revision==dropRevision,"#210 drop preview does not mutate project");
            {auto image=d.mixer[1]->createComponentSnapshot(d.mixer[1]->getLocalBounds(),true,1.f,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-insert210-drop.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);}}
            d.mixer[1]->itemDropped(details);pump();render();check(d.chain(second)==reordered,"#210 Mixer drop commits at preview boundary");d.action(10);render();
            d.insertDrag->revision=dropRevision;check(!d.insertDropAllowed(second),"#210 stale drag dropRevision rejected");d.insertDrag.reset();
            d.applyChain(second,reordered);render();pump();check(d.message.isEmpty()&&d.app.insert_generation()==generation&&GetWindow(hostOf(pinned),GW_CHILD)==child&&GetWindow(hostOf(other),GW_CHILD)==otherChild,"#210 reorder retains exact VST3 editor and unrelated chain");
            d.action(10);render();check(d.chain(second)==original&&pinned->isVisible()&&GetWindow(hostOf(pinned),GW_CHILD)==child,"#210 reorder Undo retains native view");
            d.action(11);render();check(d.chain(second)==reordered&&GetWindow(hostOf(pinned),GW_CHILD)==child,"#210 reorder Redo retains native view");d.action(10);render();
            auto mixed=d.chain(second);mixed.front().bypass=true;d.applyChain(second,mixed);render();
            d.toggleInsertPower(second);render();const auto disabled=d.chain(second);check(std::all_of(disabled.begin(),disabled.end(),[](const auto& fx){return fx.bypass;}),"#210 group power disables FX");
            d.toggleInsertPower(second);render();check(d.chain(second)==mixed&&GetWindow(hostOf(pinned),GW_CHILD)==child,"#210 group power restores mixed mask and native view");
            d.action(10);render();check(pinned->isVisible()&&GetWindow(hostOf(pinned),GW_CHILD)==child,"#210 power Undo retains editor");d.action(11);render();d.applyChain(second,original);render();
            const auto encoded=ViewSettings::decode(d.view.encode());check(encoded.nativeEditorViews.at(juce::String::fromUTF8(plugin.id.value.c_str())).pinned,"#210 pin persists in encoded preferences");
            pinned->closeButtonPressed();d.openInsert(second,plugin.id);pinned=d.windows.back().get();pin=button(pinned,"plugin-pin");check(pin->getToggleState()&&static_cast<bool>(pinned->getProperties()["mrs-pinned"]),"#210 pin survives close/reopen");d.openInsert(std::nullopt,peerPlugin.id);other=d.windows.back().get();
        }
        fixStage("bypass loops");
        const auto bypassGeneration=d.app.insert_generation();const auto bypassChild=GetWindow(hostOf(pinned),GW_CHILD);
        for(int i=0;i<4;++i){const bool bypassBefore=d.chain(second).back().bypass;button(pinned,"plugin-bypass")->onClick();check(d.chain(second).back().bypass!=bypassBefore,"bypass button commits project state");render();pump();
            check(d.message.isEmpty()&&pinned->isVisible()&&other->isVisible()&&IsWindow(GetWindow(hostOf(pinned),GW_CHILD))&&IsWindow(GetWindow(hostOf(other),GW_CHILD)),"bypass reconnects both native views without white host");}
        {
            const auto windowsBefore=d.windows.size();const auto latency=d.app.engine()->compensation().output;
            check(d.app.insert_generation()==bypassGeneration&&GetWindow(hostOf(pinned),GW_CHILD)==bypassChild,"bypass retains graph and exact editor child");
            check(latency>=14,"fixture track plus Master PDC active");
            mixerInsert(*d.mixer[1],plugin.id)->bypass.triggerClick();pump();render();
            check(d.chain(second).back().bypass&&d.app.engine()->compensation().output==latency,"mixer VST3 bypass rebuilds PDC");
            check(d.windows.size()==windowsBefore&&pinned->isVisible()&&other->isVisible()&&IsWindow(GetWindow(hostOf(pinned),GW_CHILD))&&IsWindow(GetWindow(hostOf(other),GW_CHILD)),"mixer bypass reconnects existing native views without opening editor");
            mixerInsert(*d.mixer[1],plugin.id)->bypass.triggerClick();pump();render();check(!d.chain(second).back().bypass&&d.app.engine()->compensation().output==latency,"mixer VST3 enable restores PDC");
            d.app.play();render();const auto playingRevision=d.app.services().projects->state().revision;
            mixerInsert(*d.mixer[1],plugin.id)->bypass.triggerClick();pump();render();
            check(d.chain(second).back().bypass&&mixerInsert(*d.mixer[1],plugin.id)->bypass.getToggleState()&&d.app.services().projects->state().revision==playingRevision+1&&d.message.isEmpty(),"playing VST3 bypass keeps processor and PDC");
            d.message.clear();d.app.stop();render();
        }
        {
            auto control=[&](Strip& strip,const char* id){for(auto* child:strip.getChildren())if(child->getComponentID()==id)return dynamic_cast<juce::TextButton*>(child);return static_cast<juce::TextButton*>(nullptr);};
            d.app.set_track_armed(second,false);d.app.set_track_monitoring(second,false);render();
            const auto generation=d.app.insert_generation();const auto child=GetWindow(hostOf(pinned),GW_CHILD);
            d.app.set_track_armed(second,true);d.refresh(true);render();pump();
            auto* r=control(*d.mixer[1],"track-arm");auto* i=control(*d.mixer[1],"track-monitor");
            check(r&&i&&r->isVisible()&&i->isVisible()&&r->getToggleState()&&i->getToggleState(),"mixer R enables I and reflects arrangement state");
            check(r->findColour(juce::TextButton::buttonOnColourId)==juce::Colour(0xffd94747)&&r->getY()>d.mixer[1]->gain.getBottom(),"armed R is red below M/S");
            for(float irPreviewScale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-mixer-ir-"+juce::String(static_cast<int>(irPreviewScale*100))+"-preview.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(d.createComponentSnapshot(d.getLocalBounds(),true,irPreviewScale,juce::SoftwareImageType{}),*stream);}}
            d.app.set_track_monitoring(second,false);d.refresh(true);render();d.action(10);render();d.action(11);render();
            d.app.set_track_armed(second,false);d.app.set_track_monitoring(second,true);render();d.app.set_track_monitoring(second,false);render();d.refresh(true);pump();
            check(d.app.insert_generation()==generation&&pinned->isVisible()&&GetWindow(hostOf(pinned),GW_CHILD)==child,"I/R and monitor history retain exact native editor and processor");
            auto* plus=control(*d.mixer[1],"mixer-add-insert");check(plus&&plus->getY()<24&&plus->getX()>=d.mixer[1]->nameBounds().getRight(),"add insert lives to right of channel title");
            for(auto* c:d.mixer[1]->getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(c))check(!b->getButtonText().startsWith("Inserts"),"obsolete Inserts button removed");
            const auto area=d.mixer[1]->pluginDropArea();const auto middle=juce::Point<int>(area.getCentreX(),area.getY()+20);
            check(d.mixer[1]->pluginInsertionAt(middle)==1,"drop between first and second insert");
            juce::DragAndDropTarget::SourceDetails insertDrop("vst3:0",d.browser.get(),middle);
            auto noDropImage=d.mixer[1]->createComponentSnapshot(d.mixer[1]->getLocalBounds(),true,1.f,juce::SoftwareImageType{});
            d.mixer[1]->itemDragEnter(insertDrop);auto image=d.mixer[1]->createComponentSnapshot(d.mixer[1]->getLocalBounds(),true,1.f,juce::SoftwareImageType{});
            check(image.getPixelAt(area.getX()+2,area.getY()+20)==juce::Colour(accent),"drop insertion line visible over child buttons");
            check(image.getPixelAt(area.getX()+2,area.getBottom()-5)==noDropImage.getPixelAt(area.getX()+2,area.getBottom()-5),"drop leaves insert area unfilled");
            d.browser->rebuild();auto* vendorNode=d.browser->tree.getRootItem()->getSubItem(0);check(vendorNode!=nullptr,"fixture vendor in browser");vendorNode->setOpen(true);auto* pluginNode=vendorNode->getSubItem(0);check(pluginNode!=nullptr,"fixture plugin in browser");pluginNode->setSelected(true,true);
            check(pluginNode->isSelected(),"VST3 browser selection state");referencePreview(*d.browser,"vst3-theme");
            juce::Image selectedNode(juce::Image::ARGB,200,24,true);{juce::Graphics selectedGraphics(selectedNode);pluginNode->paintItem(selectedGraphics,200,24);}
            auto selectedStream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-browser-selected-preview.png").createOutputStream();if(selectedStream)juce::PNGImageFormat().writeImageToStream(selectedNode,*selectedStream);
            check(selectedNode.getPixelAt(198,12)==juce::Colour(accent),"VST3 browser selected name has accent background");
            for(float irPreviewScale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-mixer-drop-"+juce::String(static_cast<int>(irPreviewScale*100))+"-preview.png").createOutputStream();if(stream){stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(d.mixer[1]->createComponentSnapshot(d.mixer[1]->getLocalBounds(),true,irPreviewScale,juce::SoftwareImageType{}),*stream);}}

            d.mixer[1]->itemDragExit(insertDrop);
            const auto original=d.chain(second);d.mixer[1]->itemDropped(insertDrop);pump();
            const auto added=d.chain(second);check(added.size()==original.size()+1&&added.front().id==original.front().id&&added[2].id==original[1].id,"browser drop inserts at highlighted boundary");
            d.closeEditors();d.action(10);check(d.chain(second)==original,"positional plugin insert Undo");
            d.openInsert(second,plugin.id);pinned=d.windows.back().get();pin=button(pinned,"plugin-pin");pin->setToggleState(true,juce::dontSendNotification);pin->onClick();d.openInsert(std::nullopt,peerPlugin.id);other=d.windows.back().get();
        }
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
        check(d.message.isEmpty()&&d.app.services().projects->state().revision==beforeLoad+1,"same snapshot preset restores DSP and records newly selected name once");
        auto* presetSelector=dynamic_cast<juce::ComboBox*>(presetWindow->getContentComponent()->findChildWithID("preset-library"));
        check(presetSelector&&presetSelector->getWidth()==300&&presetSelector->getHeight()==24&&presetSelector->getText()==presetLabel(presetFile,presetFolder(savedPreset)),"compact selector displays current preset");
        const auto namedRevision=d.app.services().projects->state().revision;d.loadPresetFile(second,plugin.id,presetFile);
        check(d.message.isEmpty()&&d.app.services().projects->state().revision==namedRevision,"same named preset retains no-op history");
        for(float previewScale:{1.f,1.5f}){auto output=juce::File::getCurrentWorkingDirectory().getChildFile(previewScale==1.f?"juce-preset-selector-100-preview.png":"juce-preset-selector-150-preview.png").createOutputStream();if(output){output->setPosition(0);output->truncate();juce::PNGImageFormat format;format.writeImageToStream(presetSelector->createComponentSnapshot(presetSelector->getLocalBounds(),true,previewScale,juce::SoftwareImageType{}),*output);}}
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
    juce::DragAndDropTarget::SourceDetails sampleDrop("mrs-sample-files",fileTree,{400,Arrangement::header+19});
    check(d.arrangement->isInterestedInDragSource(sampleDrop),"internal WAV browser drag accepted");
    const auto tracksBeforeDrop=d.project()->tracks.size();const auto clipsBeforeDrop=d.project()->clips.size();const auto revisionBeforeDrop=d.app.services().projects->state().revision;
    d.arrangement->itemDropped(sampleDrop);
    check(d.project()->tracks.size()==tracksBeforeDrop&&d.project()->clips.size()==clipsBeforeDrop+1,"sample drop uses existing audio track");
    check(d.project()->clips.back().track==first&&d.project()->clips.back().start==d.arrangement->sampleAt(400),"sample drop follows track and cursor time");
    check(d.app.services().projects->state().revision==revisionBeforeDrop+1,"existing track import is one command");
    d.action(10);check(d.project()->clips.size()==clipsBeforeDrop&&d.project()->tracks.size()==tracksBeforeDrop,"sample import undo restores clips without removing track");
    d.arrangement->filesDropped({wav.getFullPathName()},400,Arrangement::header+static_cast<int>(tracksBeforeDrop)*d.arrangement->trackHeight+4);
    fileTree->setSelectedFile(wav);
    {const auto until=juce::Time::getMillisecondCounter()+2000;while(!d.browser->samplePreviewReady()&&juce::Time::getMillisecondCounter()<until){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}}
    check(d.browser->samplePreviewReady(),"background WAV metadata and waveform preview completes");
    {auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-files-preview.png").createOutputStream();juce::PNGImageFormat format;if(stream){stream->setPosition(0);stream->truncate();format.writeImageToStream(d.createComponentSnapshot(d.getLocalBounds(),true,1.f,juce::SoftwareImageType{}),*stream);}}
    referencePreview(d,"files-theme");d.browser->showFiles(false);check(d.project()->tracks.size()==4,"WAV import track");
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
    // Leave the second-track waveform probe inside Arrange with the additional Tempo lane.
    d.setSize(1400,900);d.resizeMixer(260);
    // Fixture is under a unique temporary directory created by this test only.
    check(folder.getParentDirectory()==juce::File::getSpecialLocation(juce::File::tempDirectory)&&folder.getFileName().startsWith("mrs-juce-j2"),"fixture cleanup scope");folder.deleteRecursively();
}
void stateBusySmoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    Desktop d(true);d.app.new_project();const auto track=d.app.add_instrument_track("Snapshot fixture");const auto clip=d.app.create_midi_clip(track,0,4*mrs::ppq);
    d.app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});
    auto* engine=d.app.engine().get();std::array<float,256> out{};
    d.app.seek(4800);engine->process(nullptr,out.data(),128);d.refresh(true);d.setSize(1200,700);d.setVisible(true);
    PianoRoll roll(d,clip);ControllerPanel controllers(d,clip);roll.setSize(800,600);controllers.setSize(800,280);
    const auto expected=d.displayState();const auto visual=d.visualSample();const auto revision=d.app.services().projects->state().revision;
    {
        mrs::audio::AudioEngineTestAccess::Busy busy(*engine);
        bool oldReadThrows=false;try{(void)engine->state();}catch(const std::runtime_error&){oldReadThrows=true;}
        check(oldReadThrows,"fixture reproduces old throwing state read");
        check(d.displayState().sample==expected.sample&&d.visualSample()==visual,"busy display retains coherent last head");
        juce::Image image(juce::Image::ARGB,1200,700,true);juce::Graphics graphics(image);d.paint(graphics);d.refresh();d.timerCallback();
        check(roll.playheadTick()==mrs::Timeline(d.project()->time,48000).to_ticks(expected.sample),"busy piano playhead holds previous head");
        check(!roll.editable(),"busy state cannot authorize note editing");
        roll.gesture=4;roll.timerCallback();check(!roll.gesture,"busy piano timer cancels gesture safely");
        controllers.selecting=true;controllers.timerCallback();check(!controllers.selecting,"busy controller timer cancels gesture safely");
        d.spaceKey(true);d.spaceKey(false);check(!d.message.isEmpty(),"busy Space is reported without escaping event callback");
        roll.keyPressed(juce::KeyPress(juce::KeyPress::spaceKey));
        check(d.app.services().projects->state().revision==revision,"busy UI does not modify project or Undo");
    }
    d.app.seek(9600);engine->process(nullptr,out.data(),128);check(d.displayState().sample==9600,"UI recovers on next coherent publication");
    d.resetDevice();check(d.lastDisplayState.sample==0,"new session clears display cache");
}
void clipCopySmoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    struct Clock final:mrs::audio::IAudioDevice{
        std::vector<mrs::audio::DeviceInfo> enumerate()override{return {{0,"Copy fixture",{}, {"L","R"},16,8192,128,1}};}
        void control_panel(int)override{}void open(const mrs::audio::DeviceConfig&,std::shared_ptr<mrs::audio::AudioEngine>)override{}
        void start()override{}void stop()override{}void close()noexcept override{}mrs::audio::DeviceStatus status()override{return {mrs::audio::DevicePhase::running,48000};}
    };
    Desktop d(true);d.app.demo();d.app.connect(std::make_unique<Clock>(),{0,48000,128,{}, {0,1}});
    const auto keys=d.app.add_instrument_track("Copy MIDI"),midi=d.app.create_midi_clip(keys,mrs::ppq/3,mrs::ppq);
    d.app.set_midi_notes(midi,{{mrs::new_id(),0,mrs::ppq/2,60,90,0}});
    d.setWorkspace(mrs::desktop::Workspace::arrange);d.refresh(true);d.setSize(1200,900);d.setVisible(true);d.snap=false;
    auto& a=*d.arrangement;a.pixelsPerSecond=30.;
    auto mouse=[&](juce::Point<float> point,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,&a,&a,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,true);};
    auto stored=[&](mrs::Id id){const auto p=d.project();return *std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});};
    check(d.duplicate.isVisible()&&d.duplicate.getComponentID()=="duplicate-clip","Duplicate toolbar available");
    const auto audio=d.project()->clips.front().id;
    {
        const auto initialRevision=d.app.services().projects->state().revision;const auto audioRect=a.clipRect(stored(audio)),midiRect=a.clipRect(stored(midi));
        a.mouseDown(mouse(audioRect.getCentre(),juce::ModifierKeys::leftButtonModifier));a.mouseUp(mouse(audioRect.getCentre(),0));
        a.mouseDown(mouse(midiRect.getCentre(),juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::shiftModifier));a.mouseUp(mouse(midiRect.getCentre(),0));check(d.selectedClips.contains(audio.value)&&d.selectedClips.contains(midi.value),"#211 Shift adds mixed clip to selection");
        a.mouseDown(mouse(midiRect.getCentre(),juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::shiftModifier));a.mouseUp(mouse(midiRect.getCentre(),0));check(d.selectedClips.size()==1&&d.selectedClips.contains(audio.value)&&d.app.services().projects->state().revision==initialRevision,"#211 Shift repeat removes without history");
        const auto original=stored(audio);const juce::Point<float> handle{audioRect.getX()+6,audioRect.getY()+26},target{audioRect.getX()+audioRect.getWidth()*.2f,audioRect.getY()+26};
        a.mouseDown(mouse(handle,juce::ModifierKeys::leftButtonModifier));a.mouseDrag(mouse(target,juce::ModifierKeys::leftButtonModifier));check(stored(audio)==original,"#211 fade preview is not project state");
        const bool hadAssets=d.assetSkin()!=nullptr;d.setAssetDesign(true);
        for(const auto scale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"clip211-fade-100.png":"clip211-fade-150.png").createOutputStream();check(stream!=nullptr,"#211 preview stream");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*stream),"#211 preview PNG");}d.setAssetDesign(hadAssets);
        a.mouseUp(mouse(target,0));check(d.message.isEmpty()&&stored(audio).fade_in>0,"#211 fade handle commits once");d.action(10);check(stored(audio)==original,"#211 fade Undo");d.action(11);const auto faded=stored(audio);
        const auto beforeCancel=d.app.services().projects->state().revision;const auto fadeHandle=juce::Point<float>{audioRect.getX()+static_cast<float>(static_cast<double>(faded.fade_in)/faded.length*audioRect.getWidth()),audioRect.getY()+26};a.mouseDown(mouse(fadeHandle,juce::ModifierKeys::leftButtonModifier));a.mouseDrag(mouse(fadeHandle.translated(15,0),juce::ModifierKeys::leftButtonModifier));a.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));a.mouseUp(mouse(fadeHandle,0));check(stored(audio)==faded&&d.app.services().projects->state().revision==beforeCancel,"#211 Escape cancels fade");d.action(10);check(stored(audio)==original,"#211 original restored");
        if(!d.mixer.empty()){const auto* power=d.mixer.front()->findChildWithID("mixer-inserts-power");check(power&&power->getWidth()==13&&power->getRight()<=d.mixer.front()->nameBounds().getX(),"#210 follow-up power is slot-sized and left of channel name");}
    }
    for(const auto id:{audio,midi}){
        d.activateClip(id);const auto original=stored(id);const auto count=d.project()->clips.size();
        d.duplicate.onClick();check(d.message.isEmpty()&&d.project()->clips.size()==count+1&&d.selectedClip!=id,"Duplicate button copies audio/MIDI and selects copy");
        d.action(10);check(d.project()->clips.size()==count&&stored(id)==original,"Duplicate one Undo");
        d.selectedClip=id;d.selectedClips={id.value};const auto r=a.clipRect(original);const auto point=r.getCentre();const auto moved=point.translated(180,0);
        const auto revision=d.app.services().projects->state().revision;
        a.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier));a.mouseDrag(mouse(moved,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier));
        check(stored(id)==original&&d.app.services().projects->state().revision==revision,"Alt drag keeps source/model unchanged");
        const auto preview=a.createComponentSnapshot(a.getLocalBounds(),true,1.,juce::SoftwareImageType{});
        check(preview.getPixelAt(static_cast<int>(r.getX()+181),static_cast<int>(r.getCentreY())).getBrightness()>.7f,"Alt copy ghost white outline is visible");
        for(float scale:{1.f,1.5f}){const auto image=d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-clip-copy-"+juce::String(id==midi?"midi":"audio")+"-"+juce::String(static_cast<int>(scale*100))+"-preview.png").createOutputStream();check(stream!=nullptr,"copy preview stream");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(image,*stream),"copy preview image");}
        a.mouseUp(mouse(moved,0));check(d.message.isEmpty()&&d.project()->clips.size()==count+1&&stored(id)==original,"Alt release commits copy only");
        const auto copy=stored(*d.selectedClip);check(copy.id!=id&&copy.track==original.track,"Alt copied selection and track");
        d.action(10);check(d.project()->clips.size()==count,"Alt one Undo");d.action(11);check(stored(copy.id)==copy,"Alt Redo stable IDs");d.action(10);
        d.selectedClip=id;d.selectedClips={id.value};a.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier));a.mouseDrag(mouse(moved,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier));a.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));a.mouseUp(mouse(moved,0));check(d.project()->clips.size()==count,"Escape cancels copy");
        a.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier));a.mouseUp(mouse(point,0));check(d.project()->clips.size()==count,"Alt click without movement makes no copy");
    }
    d.snap=true;a.pixelsPerSecond=60.;a.horizontal=0;
    for(const auto id:{audio,midi}){d.activateClip(id);const auto original=stored(id);const mrs::Timeline time(d.project()->time,d.project()->sample_rate);const auto start=mrs::clip_start(original,time);const auto point=a.clipRect(original).getCentre().roundToInt().toFloat();const auto freePoint=point.translated(7,0);
        a.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));a.mouseDrag(mouse(freePoint,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::shiftModifier));check(stored(id)==original,"Shift clip drag remains preview only");a.mouseUp(mouse(freePoint,0));
        const auto expected=original.midi?time.to_samples(time.to_ticks(start+5600)):start+5600;check(mrs::clip_start(stored(id),time)==expected&&d.snap,"Shift after mouse-down bypasses clip grid without changing Snap setting");d.action(10);check(stored(id)==original,"Shift clip drag one Undo");d.action(11);check(mrs::clip_start(stored(id),time)==expected,"Shift clip drag Redo");d.action(10);
        const auto resumed=point.translated(21,0);a.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));a.mouseDrag(mouse(freePoint,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::shiftModifier));a.mouseDrag(mouse(resumed,juce::ModifierKeys::leftButtonModifier));a.mouseUp(mouse(resumed,0));check(mrs::clip_start(stored(id),time)==a.snapSample(start+16800)&&d.snap,"releasing Shift during clip drag resumes Smart Snap");d.action(10);
    }
    d.activateClip(midi);const auto before=*d.project();check(d.keyPressed(juce::KeyPress('D')),"D Duplicate shortcut");const auto copy=*d.selectedClip;check(d.project()->clips.size()==before.clips.size()+1,"D duplicates clip");
    d.selectedClips={midi.value,copy.value};check(d.keyPressed(juce::KeyPress('G')),"G Merge shortcut");const auto joined=*d.project();check(joined.clips.size()==before.clips.size()&&stored(*d.selectedClip).midi.has_value(),"G merges selected MIDI clips");
    d.action(10);check(d.project()->clips.size()==before.clips.size()+1,"Merge one Undo");d.action(11);check(*d.project()==joined,"Merge Redo stable IDs");d.action(10);d.action(10);check(*d.project()==before,"Duplicate/merge history restores original");
    d.addToDesktop(0);d.toFront(true);juce::TextEditor text;d.addAndMakeVisible(text);text.setBounds(20,20,180,30);text.grabKeyboardFocus();check(text.hasKeyboardFocus(true),"keyboard guard fixture owns real focus");const auto revision=d.app.services().projects->state().revision;check(!d.keyPressed(juce::KeyPress('D'))&&!d.keyPressed(juce::KeyPress('G'))&&d.app.services().projects->state().revision==revision,"D/G guarded while typing");d.removeChildComponent(&text);a.grabKeyboardFocus();d.removeFromDesktop();
    for(bool snapping:{false,true})for(double zoom:{5.,12.,24.,30.,37.5,90.,2400.}){d.snap=snapping;a.pixelsPerSecond=zoom;a.horizontal=37.25;juce::Image grid(juce::Image::RGB,1200,100,true,juce::SoftwareImageType{});{juce::Graphics graphics(grid);a.paintGrid(graphics,20,100,false);}std::set<int> bars;const auto gridProject=d.project();const mrs::Timeline gridTime(gridProject->time,gridProject->sample_rate);visitDisplayGrid(gridProject->time,gridTime.to_ticks(a.sampleAt(static_cast<float>(Arrangement::left))),gridTime.to_ticks(a.sampleAt(static_cast<float>(a.getWidth()))),a.displaySpacing(),[&](auto tick,auto pos){if(pos.beat==1&&pos.tick==0)bars.insert(Arrangement::left+juce::roundToInt(static_cast<double>(gridTime.to_samples(tick))/gridProject->sample_rate*zoom-a.horizontal));});
        int previous=-100,count=0;for(int x=Arrangement::left;x<1200;++x)if(grid.getPixelAt(x,80)!=juce::Colours::black){check(x-previous>=10||bars.contains(x)||bars.contains(previous),"only mandatory bar boundaries may shorten nominal grid spacing");previous=x;++count;}if(!count)throw std::runtime_error("zoom grid visible: zoom="+std::to_string(zoom)+" width="+std::to_string(a.getWidth())+" height="+std::to_string(a.getHeight()));
        if(!snapping&&(zoom==24.||zoom==30.))for(float scale:{1.f,1.5f}){auto image=a.createComponentSnapshot(a.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-midi-grid-"+juce::String(static_cast<int>(zoom))+"-"+juce::String(static_cast<int>(scale*100))+".png").createOutputStream();juce::PNGImageFormat().writeImageToStream(image,*stream);}}
    for(mrs::Tick spacing:{1,30,240,241,480,481,960,961,1920,1921,3840,3841}){const auto step=displayGridStep(mrs::ppq,4*mrs::ppq,spacing);check(step>=mrs::ppq/4&&step>=spacing,"grid never finer than sixteenth and satisfies pixel threshold");}
    check(displayGridStep(mrs::ppq,3*mrs::ppq,1000)==2*mrs::ppq&&displayGridStep(mrs::ppq,3*mrs::ppq,2000)==3*mrs::ppq,"odd meter half then measure");
    mrs::TimeMap odd;odd.meters={{1,3,4}};std::vector<mrs::Tick> oddLines;visitDisplayGrid(odd,0,6*mrs::ppq,1000,[&](auto tick,auto){oddLines.push_back(tick);});check(oddLines==std::vector<mrs::Tick>{0,2*mrs::ppq,3*mrs::ppq,5*mrs::ppq,6*mrs::ppq},"half-note grid retains odd-meter bar boundaries");
    const auto oddNeighbours=displayGridNeighbours(odd,3*mrs::ppq-1,1000);check(oddNeighbours.first==2*mrs::ppq&&oddNeighbours.second==3*mrs::ppq,"odd-meter snap shares half-note/bar lines");
    check(displayGridStep(60,180,1)==360,"short meters use whole bars without going below sixteenth minimum");
    auto map=d.project()->time;map.tempos={{0,80}};map.meters={{1,4,4}};d.app.set_time_map(map);d.snap=true;a.horizontal=0;
    for(const auto [zoom,step]:{std::pair{2400.,mrs::ppq/4},std::pair{60.,mrs::ppq/4},std::pair{32.,mrs::ppq/2},std::pair{30.,mrs::ppq/2},std::pair{24.,mrs::ppq},std::pair{12.,2*mrs::ppq},std::pair{7.,2*mrs::ppq},std::pair{6.,4*mrs::ppq},std::pair{2.,8*mrs::ppq}}){a.pixelsPerSecond=zoom;check(displayGridStep(mrs::ppq,4*mrs::ppq,a.displaySpacing())==step,"Smart grid sixteenth/eighth/quarter/half/bar progression");
        const mrs::Timeline time(d.project()->time,d.project()->sample_rate);for(mrs::Tick tick=0;tick<12*mrs::ppq;tick+=137){const auto target=a.snapTick(tick);check(target%step==0,"Smart Snap only visible grid ticks");const auto sample=time.to_samples(tick);const auto x=static_cast<float>(Arrangement::left+static_cast<double>(sample)/d.project()->sample_rate*zoom);check(time.to_ticks(a.sampleAt(x))%step==0&&time.to_ticks(a.seekSampleAt(x))%step==0,"clips/drops/ruler share visible grid");}
        if(zoom==60.||zoom==30.||zoom==24.||zoom==12.||zoom==7.||zoom==6.){d.refresh();referencePreview(a,"smart-grid-"+juce::String(static_cast<int>(zoom)));}}
    a.pixelsPerSecond=12.;a.horizontal=37.25;const float zoomCursor=497.f;const auto anchorSeconds=(zoomCursor-Arrangement::left+a.horizontal)/a.pixelsPerSecond;
    a.wheel(1.f,juce::ModifierKeys(juce::ModifierKeys::ctrlModifier|juce::ModifierKeys::shiftModifier),zoomCursor);check(std::abs((zoomCursor-Arrangement::left+a.horizontal)/a.pixelsPerSecond-anchorSeconds)<1./d.project()->sample_rate,"Smart zoom preserves raw cursor anchor when resolution changes");
    check(a.snapTick(-1)==0&&a.snapTick(mrs::max_tick+1)<=mrs::max_tick,"Smart Snap clamps outside project tick limits");
    map.tempos={{0,80},{3*mrs::ppq,140}};map.meters={{1,7,8},{4,3,4},{8,5,16}};d.app.set_time_map(map);a.pixelsPerSecond=15.;
    std::vector<mrs::Tick> painted;visitDisplayGrid(map,0,50*mrs::ppq,a.displaySpacing(),[&](auto tick,auto){painted.push_back(tick);});
    for(mrs::Tick tick=0;tick<40*mrs::ppq;tick+=137){const auto target=a.snapTick(tick);check(std::find(painted.begin(),painted.end(),target)!=painted.end(),"tempo/meter Smart Snap belongs to painted grid");check(a.snapTick(target)==target,"Smart grid snap idempotent");}
    mrs::TimeMap meters;meters.meters={{1,7,8},{4,3,4},{8,5,16}};std::vector<mrs::Tick> ticks;visitDisplayGrid(meters,0,40*mrs::ppq,1000,[&](auto tick,auto){ticks.push_back(tick);});check(std::is_sorted(ticks.begin(),ticks.end())&&std::adjacent_find(ticks.begin(),ticks.end())==ticks.end(),"meter display grid monotonic unique");
    d.app.disconnect();
}
void externalMidiSmoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    Desktop d(true);d.app.new_project();const auto track=d.app.add_instrument_track("Внешний синтезатор");
    d.app.set_midi_output(track,"fixture-missing-external-output",4);d.setWorkspace(mrs::desktop::Workspace::arrange);d.refresh(true);d.setSize(1200,700);d.setVisible(true);
    for(int n=0;n<200&&d.app.midi_output_status(track).find("missing")==std::string::npos;++n){d.app.poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    check(d.app.midi_output_status(track).find("missing")!=std::string::npos,"missing external output status instead of choose VST3");
    d.refresh();bool visibleStatus=false;for(auto& row:d.arrangement->rows){row->refreshMidiStatus();for(auto* child:row->getChildren())if(child->getComponentID()=="midi-input-status"){auto* button=dynamic_cast<juce::TextButton*>(child);visibleStatus=button&&button->isVisible()&&button->getButtonText().contains("MIDI out: missing");}}
    check(visibleStatus,"external MIDI status is visible in Arrange");
    const auto before=*d.project();d.app.set_midi_output(track,{},-1);d.refresh();check(d.app.undo()&&*d.project()==before,"external MIDI setting Undo");
    for(int n=0;n<200&&d.app.midi_output_status(track).find("missing")==std::string::npos;++n){d.app.poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));}d.refresh();for(auto& row:d.arrangement->rows)row->refreshMidiStatus();
    for(float scale:{1.f,1.5f}){auto image=d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-external-midi-"+juce::String(static_cast<int>(scale*100))+"-preview.png").createOutputStream();check(stream!=nullptr,"external MIDI preview stream");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(image,*stream),"external MIDI preview");}
    d.app.midi_panic();d.app.disconnect();
}
void trackRenameSmoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    auto pump=[](){for(int pass=0;pass<12;++pass){MSG msg{};int limit=0;while(limit++<200&&PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}};
    Desktop d(true);d.app.new_project();const auto audio=d.app.add_audio_track("Original"),keys=d.app.add_instrument_track("Keys"),bus=d.app.add_bus("Bus");
    mrs::NativeInsert effect;effect.id=mrs::new_id();effect.kind=mrs::InsertKind::channel_eq;d.app.set_inserts(audio,{effect});
    d.setSize(1200,800);d.setWorkspace(mrs::desktop::Workspace::mix);d.refresh(true);d.addToDesktop(0);d.setVisible(true);
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mrs-rename-ui","",false);check(folder.createDirectory(),"rename fixture directory");
    const auto file=std::filesystem::path(folder.getChildFile("Rename.mrsproject").getFullPathName().toWideCharPointer());
    auto name=[&](mrs::Id id){const auto p=d.project();return std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==id;})->name;};
    auto event=[](Strip& strip,juce::Point<float> point,int mods,int clicks){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,&strip,&strip,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),clicks,false);};
    juce::Component::SafePointer<juce::TextEditor> editor;
    auto finish=[&](juce::String text,bool accept){check(editor&&editor->isVisible(),"inline editor active");const auto revision=d.app.services().projects->state().revision;editor->setText(text);check(d.app.services().projects->state().revision==revision,"typing does not edit model");check(static_cast<juce::Component*>(editor.getComponent())->keyPressed(juce::KeyPress(accept?juce::KeyPress::returnKey:juce::KeyPress::escapeKey)),"rename Enter/Escape handled");pump();check(!editor||!editor->isVisible(),"inline editor closes");};
    auto open=[&](Strip& strip){const auto e=event(strip,strip.nameBounds().getCentre().toFloat(),juce::ModifierKeys::leftButtonModifier,2);strip.mouseDown(e);strip.mouseDoubleClick(e);check(!d.isDragAndDropActive(),"double click does not reorder track");editor=dynamic_cast<juce::TextEditor*>(strip.findChildWithID("track-name-editor"));check(editor&&editor->isVisible()&&editor->getBounds()==strip.nameBounds(),"inline editor stays in name field");check(!d.shortcutAllowedFor(editor.getComponent()),"typing blocks transport/delete shortcuts");check(!juce::ModalComponentManager::getInstance()->getNumModalComponents(),"rename opens no dialog");};
    const auto initial=d.app.services().projects->state().revision;auto& first=*d.arrangement->rows[0];first.mouseDown(event(first,first.nameBounds().getCentre().toFloat(),juce::ModifierKeys::leftButtonModifier,1));check(d.selectedTrack==audio&&!juce::ModalComponentManager::getInstance()->getNumModalComponents(),"single click only selects track");first.mouseDoubleClick(event(first,first.nameBounds().getCentre().toFloat(),juce::ModifierKeys::rightButtonModifier,2));check(!juce::ModalComponentManager::getInstance()->getNumModalComponents(),"right double click ignored");open(first);check(editor->getText()=="Original","current name initialized");finish("Ignored",false);check(name(audio)=="Original"&&d.app.services().projects->state().revision==initial,"Escape leaves model/history intact");
    open(*d.arrangement->rows[0]);finish(juce::String::fromUTF8("Дорожка — длинное имя 123456789"),true);check(name(audio)!= "Original"&&d.app.services().projects->state().revision==initial+1,"Enter commits one shared rename command");check(d.mixer[0]->gain.getTitle().contains(juce::String::fromUTF8("Дорожка")),"Mix follows Arrange rename");
    d.action(10);check(name(audio)=="Original","rename Undo");d.action(11);const auto renamed=name(audio);check(renamed!="Original","rename Redo");
    open(*d.mixer[1]);finish("New Keys",true);check(name(keys)=="New Keys"&&d.arrangement->rows[1]->gain.getTitle().contains("New Keys"),"Arrange follows Mix instrument rename");open(*d.mixer[2]);finish("New Bus",true);check(name(bus)=="New Bus","bus name editable");
    const auto unchanged=d.app.services().projects->state().revision;open(*d.mixer[2]);finish("New Bus",true);check(d.app.services().projects->state().revision==unchanged,"unchanged rename is no-op");
    open(*d.mixer[2]);finish("  ",true);check(name(bus)=="New Bus"&&d.app.services().projects->state().revision==unchanged,"empty name cancels");
    d.renameTrack(bus);editor=dynamic_cast<juce::TextEditor*>(d.mixer[2]->findChildWithID("track-name-editor"));check(editor&&editor->isVisible(),"menu also edits inline");editor->setText("Focus Bus");editor->onFocusLost();pump();check(name(bus)=="Focus Bus","click outside commits");d.action(10);
    for(auto* strip:{d.master.get(),d.mixer[0].get()}){const auto p=strip==d.master.get()?strip->nameBounds().getCentre().toFloat():juce::Point<float>{8,60};strip->mouseDoubleClick(event(*strip,p,juce::ModifierKeys::leftButtonModifier,2));check(!juce::ModalComponentManager::getInstance()->getNumModalComponents(),"Master/body double click does not rename");}
    auto fonts=[&](Strip& strip){for(auto* child:strip.getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child)){const auto f=d.theme.getTextButtonFont(*button,button->getHeight());check(f.getHeight()==Theme::stripFont().getHeight()&&f.getTypefaceName()==Theme::stripFont().getTypefaceName(),"all channel buttons match insert font");}};
    for(auto& strip:d.mixer)fonts(*strip);for(auto& strip:d.arrangement->rows)fonts(*strip);fonts(*d.master);check(Theme::stripFont().getHeight()==12.f,"reference body logical font size");
    d.app.save_project(file);d.app.open_project(file);d.refresh(true);check(name(audio)==renamed&&name(keys)=="New Keys"&&name(bus)=="New Bus","renames survive actual Save/Open");
    open(*d.mixer[0]);for(float scale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-track-inline-"+juce::String(static_cast<int>(scale*100))+"-preview.png").createOutputStream();check(stream!=nullptr,"channel font preview stream");check(juce::PNGImageFormat().writeImageToStream(d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*stream),"channel font preview");}finish("Ignored",false);
    open(*d.mixer[0]);editor->setText("Stale editor");editor->onReturnKey();d.app.new_project();d.refresh(true);const auto fresh=*d.project();pump();check(*d.project()==fresh,"old inline edit cannot rename replacement project");d.removeFromDesktop();check(folder.getParentDirectory()==juce::File::getSpecialLocation(juce::File::tempDirectory)&&folder.getFileName().startsWith("mrs-rename-ui"),"rename cleanup scope");folder.deleteRecursively();
}
void mixerSendsSmoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    Desktop d(true);d.app.demo();const auto source=d.project()->tracks.front().id;
    const auto first=d.app.add_bus("Room"),second=d.app.add_bus("Delay"),third=d.app.add_bus("Alternate");
    d.app.set_track_sends(source,{{first,.5f,true},{second,.25f,false}});
    d.app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});
    d.setWorkspace(mrs::desktop::Workspace::mix);d.refresh(true);d.setSize(1400,850);d.addToDesktop(0);d.setVisible(true);
    auto row=[&](mrs::Id bus)->SendLevel&{
        auto* viewport=dynamic_cast<juce::Viewport*>(d.mixer.front()->findChildWithID("mixer-sends"));check(viewport!=nullptr,"send viewport exists");
        auto* control=dynamic_cast<SendLevel*>(viewport->getViewedComponent()->findChildWithID("mixer-send:"+juce::String(bus.value)));check(control!=nullptr,"named send row exists");return *control;
    };
    auto sends=[&]{return d.project()->tracks.front().sends;};
    auto event=[](SendLevel& c,float x,float y,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),{x,y},juce::ModifierKeys(mods),1,0,0,0,0,&c,&c,juce::Time::getCurrentTime(),{x,y},juce::Time::getCurrentTime(),1,true);};
    std::array<float,256> out{};auto render=[&]{d.app.engine()->process(nullptr,out.data(),128);};
    check(d.mixer.front()->findChildWithID("mixer-sends")->getHeight()>0,"default mixer reserves space for sends even without inserts");
    auto& control=row(second);check(control.destination=="Delay"&&control.gain==.25f&&SendLevel::level(0)==0&&SendLevel::level(1)==16,"destination names and full send range");
    check(control.rail().getHeight()==5,"send rail visibly thicker");control.mouseDown(event(control,40,10,juce::ModifierKeys::leftButtonModifier));check(control.dragging(),"expanded send hit region");control.focusLost(juce::Component::focusChangedDirectly);
    d.app.play();render();const auto sendGeneration=d.app.insert_generation();
    std::size_t powerFirst{},powerSecond{},powerIndex{};for(const auto& track:d.project()->tracks)if(track.kind!=mrs::TrackKind::midi){if(track.id==first)powerFirst=powerIndex;if(track.id==second)powerSecond=powerIndex;++powerIndex;}
    const auto beforePower=d.app.services().projects->state().revision;control.power.onClick();
    const auto powerDeadline=juce::Time::getMillisecondCounter()+1000;
    while(!sends()[1].bypass&&juce::Time::getMillisecondCounter()<powerDeadline){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}
    render();render();d.app.engine()->take_meters();render();const auto bypassMeters=d.app.engine()->take_meters();
    check(sends()[1].bypass&&sends()[1].gain==.25f&&control.power.getToggleState()&&d.app.services().projects->state().revision==beforePower+1,"send power commits bypass preserving level");
    check(bypassMeters.tracks[powerSecond].left==0&&bypassMeters.tracks[powerFirst].left>0&&d.app.insert_generation()==sendGeneration,"power silences only selected return without DSP rebuild");
    check(d.app.preview_send_gain(source,second,1),"bypassed send preview accepted");render();render();d.app.engine()->take_meters();render();check(d.app.engine()->take_meters().tracks[powerSecond].left==0,"level preview cannot unmute bypass");d.app.cancel_mix_preview();
    d.action(10);render();check(!sends()[1].bypass&&sends()[1].gain==.25f,"send power Undo during playback");d.action(11);render();check(sends()[1].bypass,"send power Redo during playback");d.action(10);render();
    d.app.pause();render();
    const auto original=sends();const auto revision=d.app.services().projects->state().revision;d.app.play();render();
    std::size_t firstIndex=0,secondIndex=0,index=0;for(const auto& track:d.project()->tracks)if(track.kind!=mrs::TrackKind::midi){if(track.id==first)firstIndex=index;if(track.id==second)secondIndex=index;++index;}
    d.app.engine()->take_meters();check(d.app.preview_send_gain(source,second,0),"send preview accepted by engine");render();render();d.app.engine()->take_meters();render();const auto silent=d.app.engine()->take_meters();
    check(silent.tracks[secondIndex].left==0&&silent.tracks[firstIndex].left>0,"preview silences only selected return DSP");
    d.app.cancel_mix_preview();render();const auto restored=d.app.engine()->take_meters();check(restored.tracks[secondIndex].left>0,"preview cancellation restores actual send DSP");
    control.mouseDown(event(control,40,2,juce::ModifierKeys::leftButtonModifier));control.mouseDrag(event(control,85,2,juce::ModifierKeys::leftButtonModifier));render();
    check(sends()==original&&d.app.services().projects->state().revision==revision&&d.busyGesture(),"send drag previews without history during playback");
    const auto previewGain=control.gain;control.mouseUp(event(control,85,2,0));render();
    check(d.message.isEmpty()&&sends()[0]==original[0]&&sends()[1].gain==previewGain&&d.app.services().projects->state().revision==revision+1,"one mouse-up edits only matching send");
    d.action(10);render();check(sends()==original,"send gain one Undo during playback");d.action(11);render();check(sends()[1].gain==previewGain,"send gain Redo during playback");
    control.mouseDown(event(control,5,2,juce::ModifierKeys::leftButtonModifier));control.mouseDrag(event(control,65,2,juce::ModifierKeys::leftButtonModifier));check(control.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)),"Escape handles send preview");render();control.mouseUp(event(control,65,2,0));check(sends()[1].gain==previewGain&&!d.busyGesture(),"Escape restores committed gain without Undo");
    control.mouseDown(event(control,25,2,juce::ModifierKeys::leftButtonModifier));control.focusLost(juce::Component::focusChangedDirectly);render();check(sends()[1].gain==previewGain&&!d.busyGesture(),"focus loss cancels preview");
    control.keyPressed(juce::KeyPress(juce::KeyPress::homeKey));render();check(sends()[1].gain==0,"keyboard exact silence");control.keyPressed(juce::KeyPress(juce::KeyPress::endKey));render();check(sends()[1].gain==16,"keyboard maximum");
    d.app.pause();render();d.action(10);d.action(10);render();
    const auto bodyRevision=d.app.services().projects->state().revision;control.mouseDown(event(control,45,15,juce::ModifierKeys::leftButtonModifier));control.mouseUp(event(control,45,15,0));check(d.app.services().projects->state().revision==bodyRevision,"button caption click does not change level");
    control.mouseDown(event(control,45,15,juce::ModifierKeys::rightButtonModifier));juce::PopupMenu::dismissAllActiveMenus();
    std::vector<mrs::Id> choices;auto menu=d.sendMenu(source,second,choices);bool duplicateDisabled=false,currentChecked=false;
    juce::PopupMenu::MenuItemIterator items(menu);while(items.next()){const auto& item=items.getItem();if(item.itemID>=10){const auto id=choices[static_cast<std::size_t>(item.itemID-10)];if(id==first)duplicateDisabled=!item.isEnabled;if(id==second)currentChecked=item.isTicked;}}
    check(duplicateDisabled&&currentChecked,"menu marks selected bus and rejects duplicate destinations");
    const auto choice=10+static_cast<int>(std::find(choices.begin(),choices.end(),third)-choices.begin());d.run([&]{d.applySendChoice(source,second,choice,choices);});
    check(sends()[1].bus==third&&sends()[1].gain==previewGain&&!sends()[1].pre_fader,"destination replacement keeps gain/pre/post");
    const auto staleRevision=d.app.services().projects->state().revision;d.run([&]{d.applySendChoice(source,second,3,choices);});check(d.app.services().projects->state().revision==staleRevision,"stale menu cannot remove a different send");
    d.run([&]{d.applySendChoice(source,third,2,choices);});check(sends()[1].pre_fader,"pre/post menu edits shared model");d.action(10);d.action(10);check(sends()[1].bus==second,"routing Undo restores destination");
    d.app.rename_track(second,"Long renamed return destination");d.refresh();check(row(second).destination=="Long renamed return destination","send follows bus rename");
    std::vector<mrs::NativeInsert> inserts;for(int i=0;i<8;++i)inserts.push_back({mrs::new_id(),mrs::InsertKind::gain});d.app.set_inserts(source,inserts);
    auto all=sends();for(int i=0;i<6;++i){const auto bus=d.app.add_bus("Return "+std::to_string(i+3));all.push_back({bus,.1f,false});}d.app.set_track_sends(source,all);d.refresh(true);
    const auto faderHeight=d.mixer.front()->gain.getHeight();for(int height:{700,1050}){
        d.setSize(1200,height);d.resizeMixer(height);d.resized();auto& strip=*d.mixer.front();auto* sendView=dynamic_cast<juce::Viewport*>(strip.findChildWithID("mixer-sends"));auto* insertView=dynamic_cast<juce::Viewport*>(strip.findChildWithID("mixer-inserts"));
        check(sendView&&insertView&&sendView->getY()>=insertView->getBottom()&&sendView->getBottom()<strip.gain.getY()&&strip.gain.getHeight()==faderHeight,"sends below inserts above fixed faders");
        if(height==700)check(sendView->getViewedComponent()->getHeight()>sendView->getHeight()&&insertView->getViewedComponent()->getHeight()>insertView->getHeight(),"both lists scroll when mixer has overflow");
    }
    auto& splitStrip=*d.mixer.front();auto* divider=splitStrip.findChildWithID("mixer-send-divider");auto* splitSend=splitStrip.findChildWithID("mixer-sends");auto* splitInsert=splitStrip.findChildWithID("mixer-inserts");
    const auto splitRevision=d.app.services().projects->state().revision;const int beforeHeight=splitSend->getHeight();
    auto splitMouse=[&](float y){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),{4,y},juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,divider,divider,juce::Time::getCurrentTime(),{4,2},juce::Time::getCurrentTime(),1,true);};
    splitStrip.mouseDown(splitMouse(2));splitStrip.mouseDrag(splitMouse(-28));splitStrip.mouseUp(splitMouse(2));
    check(splitSend->getHeight()>beforeHeight&&splitInsert->getHeight()>=24&&d.app.services().projects->state().revision==splitRevision,"divider expands sends upward without project mutation");
    for(const auto& strip:d.mixer)check(strip->findChildWithID("mixer-send-divider")->getY()==divider->getY(),"divider aligned across mixer channels");
    d.resizeSendArea(100000);check(splitInsert->getHeight()==24,"send expansion retains minimum insert height");d.resizeSendArea(-100000);check(splitSend->getHeight()==24,"send collapse retains minimum send height");d.resizeSendArea(beforeHeight+30);
    const auto menuChain=d.chain(source);const auto menuSlot=menuChain[1].id;const auto menuGeneration=d.app.insert_generation();
    auto* menuRow=dynamic_cast<MixerInsert*>(dynamic_cast<juce::Viewport*>(splitInsert)->getViewedComponent()->findChildWithID("mixer-insert:"+juce::String(menuSlot.value)));check(menuRow&&menuRow->menu,"insert supports context menu");
    d.insertContextMenu(source,menuSlot,splitStrip.localPointToGlobal(juce::Point<int>{15,50}));juce::PopupMenu::dismissAllActiveMenus();
    d.run([&]{d.applyInsertChoice(source,menuSlot,3);});check(d.chain(source)[1].bypass&&d.app.insert_generation()==menuGeneration,"context bypass retains instances");d.action(10);check(d.chain(source)==menuChain,"context bypass Undo");
    d.run([&]{d.applyInsertChoice(source,menuSlot,2);});check(!d.windows.empty(),"context Editor opens existing window");
    d.run([&]{d.applyInsertChoice(source,menuSlot,1);});check(d.chain(source).size()==7&&std::none_of(d.windows.begin(),d.windows.end(),[](const auto& window){return window->isVisible();}),"context Remove closes editors and removes chosen slot");
    const auto staleInsertRevision=d.app.services().projects->state().revision;d.run([&]{d.applyInsertChoice(source,menuSlot,1);});check(d.app.services().projects->state().revision==staleInsertRevision,"stale insert menu cannot remove neighbour");d.action(10);check(d.chain(source)==menuChain,"context Remove Undo restores chain");
    for(float scale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-mixer-sends-"+juce::String(static_cast<int>(scale*100))+"-preview.png").createOutputStream();check(stream!=nullptr,"send preview stream");check(juce::PNGImageFormat().writeImageToStream(d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*stream),"send preview image");}
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mrs-send-ui","",false);const auto file=folder.getChildFile("Sends.mrsproject");
    d.app.save_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));const auto saved=*d.project();d.app.open_project(d.app.path());d.refresh(true);check(*d.project()==saved&&row(second).gain==previewGain,"send UI gain/routing Save/Open");
    d.app.disconnect();d.removeFromDesktop();check(folder.getParentDirectory()==juce::File::getSpecialLocation(juce::File::tempDirectory)&&folder.getFileName().startsWith("mrs-send-ui"),"send cleanup scope");folder.deleteRecursively();
}
void chordTrackSmoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    struct Clock final:mrs::audio::IAudioDevice{
        std::vector<mrs::audio::DeviceInfo> enumerate()override{return {{0,"Chord fixture",{}, {"L","R"},16,8192,128,1}};}
        void control_panel(int)override{}void open(const mrs::audio::DeviceConfig&,std::shared_ptr<mrs::audio::AudioEngine>)override{}
        void start()override{}void stop()override{}void close()noexcept override{}mrs::audio::DeviceStatus status()override{return {mrs::audio::DevicePhase::running,48000};}
    };
    Desktop d(true);d.app.new_project();const auto track=d.app.add_instrument_track("Keys");
    d.app.create_midi_clip(track,0,4*mrs::ppq);d.app.connect(std::make_unique<Clock>(),{0,48000,128,{}, {0,1}});
    d.setSize(1200,800);d.setWorkspace(mrs::desktop::Workspace::arrange);d.setVisible(true);d.refresh(true);
    d.addToDesktop(juce::ComponentPeer::windowIsTemporary);
    auto& a=*d.arrangement;auto& lane=*a.chords;a.pixelsPerSecond=120;a.horizontal=0;d.snap=false;
    auto pump=[](){for(int pass=0;pass<15;++pass){MSG msg{};int limit=0;while(limit++<200&&PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}};
    auto form=[&](){auto* alert=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());check(alert!=nullptr,"chord form opens");return alert;};
    auto save=[&](juce::String symbol,mrs::Tick start,mrs::Tick end,int result=1){
        auto* alert=form();alert->getTextEditor("symbol")->setText(symbol);
        alert->getTextEditor("start")->setText(juce::String(start));alert->getTextEditor("end")->setText(juce::String(end));
        check(!d.shortcutAllowedFor(alert->getTextEditor("symbol")),"chord form guards transport/delete shortcuts");
        alert->exitModalState(result);pump();
    };
    lane.edit({},0);save("Am",0,4*mrs::ppq);check(d.message.isEmpty()&&d.project()->chords.size()==1,"form creates chord");
    lane.edit({},8*mrs::ppq);save("C/G",8*mrs::ppq,12*mrs::ppq);
    auto stored=[&](mrs::Id id){for(const auto& c:d.project()->chords)if(c.id==id)return c;throw std::runtime_error("chord missing");};
    const auto c=d.project()->chords.front();const auto clips=d.project()->clips;
    lane.edit(c,c.start);save(juce::String::fromUTF8("Авторский аккорд"),0,3*mrs::ppq);
    check(stored(c.id).symbol=="Авторский аккорд"&&stored(c.id).end==3*mrs::ppq,"form exact UTF-8/time edit keeps ID");
    check(d.app.undo()&&stored(c.id)==c&&d.app.redo(),"form Undo/Redo");d.app.undo();d.refresh(true);
    const auto revision=d.app.services().projects->state().revision;
    lane.edit(c,c.start);save("Cancel",0,mrs::ppq,0);check(stored(c.id)==c&&d.app.services().projects->state().revision==revision,"form Cancel no mutation");
    lane.edit(c,c.start);save("",0,mrs::ppq);check(!d.message.isEmpty()&&stored(c.id)==c,"form invalid symbol rejected");
    lane.edit(c,c.start);save("Overlap",0,9*mrs::ppq);check(!d.message.isEmpty()&&stored(c.id)==c,"form overlap rejected");
    auto mouse=[&](juce::Point<float> point,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,&lane,&lane,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,true);};
    const juce::Point<float> empty{610,12}; // tick 5760, midway through the gap
    lane.mouseDoubleClick(mouse(empty,juce::ModifierKeys::leftButtonModifier));
    check(juce::Component::getCurrentlyModalComponent()==nullptr&&d.project()->chords.size()==3,"empty double click creates without a form");
    const auto created=d.project()->chords[1];
    check(created.symbol=="Am"&&created.start==6*mrs::ppq&&created.end==8*mrs::ppq,"direct creation stops at next chord");
    lane.mouseDoubleClick(mouse(lane.chordRect(created).getCentre(),juce::ModifierKeys::leftButtonModifier));
    save("Cancelled edit",created.start,created.end,0);check(stored(created.id)==created,"chord double click opens cancellable editor");
    check(d.app.undo()&&d.project()->chords.size()==2,"direct create one Undo");
    check(d.app.redo()&&stored(created.id)==created,"direct create Redo retains ID");d.app.undo();d.refresh(true);
    auto copyPoint=lane.chordRect(c).getCentre();auto copyTarget=copyPoint.translated(240,0);
    const auto copyRevision=d.app.services().projects->state().revision;
    lane.mouseDown(mouse(copyPoint,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier));
    lane.mouseDrag(mouse(copyTarget,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier));
    check(lane.copyDrag&&d.app.services().projects->state().revision==copyRevision&&stored(c.id)==c,"Alt-copy ghost does not mutate project");
    lane.mouseUp(mouse(copyTarget,0));
    check(d.message.isEmpty()&&d.project()->chords.size()==3&&stored(c.id)==c,"Alt-drag copies and preserves source");
    const auto copied=d.project()->chords[1];
    check(copied.id!=c.id&&copied.start==c.end&&copied.end==8*mrs::ppq&&copied.symbol==c.symbol,"Alt-copy fresh ID and exact duration");
    check(d.app.undo()&&d.project()->chords.size()==2&&d.app.redo()&&stored(copied.id)==copied,"Alt-copy single Undo/Redo");
    d.app.undo();d.refresh(true);
    lane.mouseDown(mouse(copyPoint,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier));
    lane.mouseDrag(mouse(copyPoint.translated(60,0),juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier));
    const auto invalidCopyRevision=d.app.services().projects->state().revision;
    lane.mouseUp(mouse(copyPoint.translated(60,0),0));
    check(!d.message.isEmpty()&&d.app.services().projects->state().revision==invalidCopyRevision&&d.project()->chords.size()==2,"overlapping copy fails atomically");
    lane.mouseDown(mouse(copyPoint,juce::ModifierKeys::leftButtonModifier));lane.mouseUp(mouse(copyPoint,0));
    d.action(45);check(d.message.isEmpty()&&d.project()->chords.size()==3&&stored(c.id)==c,"toolbar Duplicate targets selected chord");
    d.action(25);check(d.project()->chords.size()==2&&d.project()->clips==clips,"toolbar deletion targets chord rather than clip");
    d.app.undo();d.app.undo();d.refresh(true);
    auto point=lane.chordRect(c).getCentre();auto moved=point.translated(120,0);
    lane.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));
    lane.mouseDrag(mouse(moved,juce::ModifierKeys::leftButtonModifier));
    check(stored(c.id)==c&&lane.dragging()&&d.busyGesture(),"chord gesture is preview only");
    lane.mouseUp(mouse(moved,0));check(d.message.isEmpty()&&stored(c.id).start==2*mrs::ppq,"chord move commits tick displacement");
    check(d.app.undo()&&stored(c.id)==c,"one Undo restores chord drag");check(d.app.redo()&&stored(c.id).start==2*mrs::ppq,"move Redo keeps ID");d.app.undo();d.refresh(true);
    point={lane.chordRect(c).getRight()-2,12};moved=point.translated(-60,0);
    lane.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse(moved,juce::ModifierKeys::leftButtonModifier));lane.mouseUp(mouse(moved,0));
    check(stored(c.id).end==3*mrs::ppq,"right edge resizes duration");d.app.undo();d.refresh(true);
    point={lane.chordRect(c).getX()+2,12};moved=point.translated(60,0);
    lane.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse(moved,juce::ModifierKeys::leftButtonModifier));lane.mouseUp(mouse(moved,0));
    check(stored(c.id).start==mrs::ppq&&stored(c.id).end==c.end,"left edge resizes duration");d.app.undo();d.refresh(true);
    point=lane.chordRect(c).getCentre();moved=point.translated(30,0);
    for(bool escape:{true,false}){
        lane.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse(moved,juce::ModifierKeys::leftButtonModifier));
        if(escape)lane.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));else lane.focusLost(juce::Component::focusChangedDirectly);
        lane.mouseUp(mouse(moved,0));check(stored(c.id)==c,"Escape/focus loss cancel chord preview");
    }
    d.snap=true;moved=point.translated(17,0);lane.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));
    lane.mouseDrag(mouse(moved,juce::ModifierKeys::leftButtonModifier));lane.mouseUp(mouse(moved,0));
    check(d.arrangement->snapTick(stored(c.id).start)==stored(c.id).start,"chord drag uses Arrange snap");d.app.undo();d.snap=false;d.refresh(true);
    auto offgrid=c;offgrid.start=15;d.app.update_chord(c,offgrid);d.refresh(true);d.snap=true;
    point=lane.chordRect(offgrid).getCentre();moved=point.translated(17,0);
    lane.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse(moved,juce::ModifierKeys::leftButtonModifier));lane.mouseUp(mouse(moved,0));
    check(d.arrangement->snapTick(stored(c.id).start)==stored(c.id).start,"snap moves an off-grid chord onto the grid");
    d.app.undo();d.app.undo();d.snap=false;d.refresh(true);point=lane.chordRect(c).getCentre();moved=point.translated(30,0);
    lane.edit(c,c.start);auto newer=c;newer.symbol="Newer";d.app.update_chord(c,newer);
    save("Stale",0,c.end);check(stored(c.id)==newer&&!d.message.isEmpty(),"stale form cannot overwrite updated chord");d.app.undo();d.refresh(true);
    lane.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse(moved,juce::ModifierKeys::leftButtonModifier));
    d.app.update_chord(c,newer);lane.mouseUp(mouse(moved,0));check(stored(c.id)==newer&&!d.message.isEmpty(),"stale gesture fails atomically");d.app.undo();d.refresh(true);
    std::array<float,256> out{};
    auto position=[&](mrs::Tick tick){const mrs::Timeline time(d.project()->time,d.project()->sample_rate);d.app.seek(time.to_samples(tick));d.app.engine()->process(nullptr,out.data(),128);d.app.poll();d.refresh();};
    position(c.start);check(lane.currentChord()==c.id,"current highlight at inclusive start");
    position(c.end);check(!lane.currentChord(),"gap has no current highlight");
    position(8*mrs::ppq);check(lane.currentChord()==d.project()->chords.back().id,"next chord inclusive start");
    for(double zoom:{30.,120.,300.})for(double scroll:{0.,150.}){
        a.pixelsPerSecond=zoom;a.horizontal=scroll;const auto r=lane.chordRect(c);
        if(r.getCentreX()>=Arrangement::left)check(lane.chordAt(r.getCentre())->id==c.id,"zoom/scroll hit-test matches drawing");
    }
    a.pixelsPerSecond=120;a.horizontal=0;position(0);
    lane.grabKeyboardFocus();const auto laneImage=lane.createComponentSnapshot(lane.getLocalBounds(),true,1.f,juce::SoftwareImageType{});
    check(laneImage.getPixelAt(551,0)==juce::Colour(arrangementCanvas),"focused chord lane has no region outline");
    check(laneImage.getPixelAt(551,3)==juce::Colour(arrangementCanvas),"chord lane shares Arrange canvas colour");
    check(laneImage.getPixelAt(550,3)!=laneImage.getPixelAt(551,3),"shared grid is visible in chord gaps");
    check(laneImage.getPixelAt(310,3)!=laneImage.getPixelAt(311,3),"translucent chord reveals underlying grid");
    d.message.clear();d.refresh();
    for(float scale:{1.f,1.5f}){
        auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-chord-track-"+juce::String(static_cast<int>(scale*100))+"-preview.png").createOutputStream();
        check(stream&&stream->setPosition(0)&&stream->truncate().wasOk(),"replace previous chord preview rather than append PNG data");
        check(stream&&juce::PNGImageFormat().writeImageToStream(d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*stream),"chord preview export");
    }
    lane.mouseDown(mouse(lane.chordRect(c).getCentre(),juce::ModifierKeys::leftButtonModifier));lane.mouseUp(mouse(lane.chordRect(c).getCentre(),0));
    lane.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey));check(d.project()->chords.size()==1,"Backspace chord, not clip");
    check(d.app.undo()&&stored(c.id)==c&&d.project()->clips==clips,"deletion Undo and clips unchanged");
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mrs-chords-"+juce::Uuid().toString());folder.createDirectory();
    const auto file=folder.getChildFile("Chords.mrsproject");const auto all=d.project()->chords;
    d.app.stop();d.app.engine()->process(nullptr,out.data(),128);
    d.app.save_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));d.app.open_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));d.refresh(true);
    check(d.project()->chords==all&&!lane.selected,"save/open preserves chords and clears transient selection");
    lane.edit(d.project()->chords.front(),0);d.app.new_project();d.refresh(true);save("Other project",0,mrs::ppq);
    check(d.project()->chords.empty()&&!d.message.isEmpty(),"old project form cannot write to replacement");
    d.app.disconnect();d.removeFromDesktop();check(folder.getParentDirectory()==juce::File::getSpecialLocation(juce::File::tempDirectory)&&folder.getFileName().startsWith("mrs-chords-"),"chord fixture cleanup scope");folder.deleteRecursively();
}
void markerTrackSmoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    Desktop d(true);d.app.new_project();d.setSize(1200,800);d.setWorkspace(mrs::desktop::Workspace::arrange);d.setVisible(true);d.refresh(true);
    d.addToDesktop(juce::ComponentPeer::windowIsTemporary);auto& a=*d.arrangement;auto& lane=*a.markers;a.pixelsPerSecond=120;a.horizontal=0;d.snap=false;
    auto pump=[](){for(int pass=0;pass<15;++pass){MSG msg{};int limit=0;while(limit++<200&&PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}};
    auto form=[&](){auto* alert=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());check(alert!=nullptr,"marker form opens");return alert;};
    auto save=[&](juce::String name,mrs::Tick tick,int kind,int result=1){auto* alert=form();alert->getTextEditor("name")->setText(name);alert->getTextEditor("tick")->setText(juce::String(tick));
        alert->getComboBoxComponent("kind")->setSelectedItemIndex(kind);check(!d.shortcutAllowedFor(alert->getTextEditor("name")),"marker form guards project shortcuts");alert->exitModalState(result);pump();};
    auto stored=[&](mrs::Id id){for(const auto& m:d.project()->markers)if(m.id==id)return m;throw std::runtime_error("marker missing");};
    for(int kind=0;kind<6;++kind){lane.edit({},2*mrs::ppq);save("Marker "+juce::String(kind),2*mrs::ppq,kind);}
    check(d.message.isEmpty()&&d.project()->markers.size()==6&&lane.list.getNumItems()==6,"all kinds and coincident markers in selector");
    const auto all=d.project()->markers;
    for(int i=0;i<6;++i){lane.list.setSelectedId(i+1,juce::sendNotificationSync);check(lane.selected==all[static_cast<std::size_t>(i)].id,"each coincident ID selectable");}
    auto mouse=[&](juce::Point<float> point,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,&lane,&lane,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,true);};
    const auto source=all[2];lane.select(source.id);auto point=juce::Point<float>{lane.xAt(source.tick),12};
    lane.mouseDoubleClick(mouse(point,juce::ModifierKeys::leftButtonModifier));check(lane.renameEditor()!=nullptr,"marker double click inline rename");lane.renameEditor()->setText(juce::String::fromUTF8("Русский маркер"));lane.renameEditor()->onReturnKey();pump();
    auto edited=stored(source.id);check(edited.name=="Русский маркер"&&edited.kind==source.kind&&d.project()->markers[2].id==source.id,"form keeps ID and tie order");
    check(d.app.undo()&&stored(source.id)==source&&d.app.redo()&&stored(source.id)==edited,"marker form Undo/Redo");
    const auto revision=d.app.services().projects->state().revision;lane.edit(edited,edited.tick);save("Cancel",0,0,0);
    check(d.app.services().projects->state().revision==revision,"marker Cancel no mutation");
    lane.edit(edited,edited.tick);save("",0,0);check(stored(source.id)==edited&&!d.message.isEmpty(),"invalid name rejected");
    lane.edit(edited,edited.tick);save("Negative",-1,0);check(stored(source.id)==edited&&!d.message.isEmpty(),"negative tick rejected");
    lane.edit(edited,edited.tick);save("No op",edited.tick,3,0);
    lane.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse(point.translated(60,0),juce::ModifierKeys::leftButtonModifier));
    check(stored(source.id)==edited&&d.busyGesture(),"marker drag preview only");lane.mouseUp(mouse(point.translated(60,0),0));
    check(d.message.isEmpty()&&stored(source.id).tick==3*mrs::ppq,"marker drag commits tick position");
    check(d.app.undo()&&stored(source.id)==edited&&d.app.redo()&&stored(source.id).tick==3*mrs::ppq,"marker gesture one Undo/Redo");d.app.undo();d.refresh();
    for(bool escape:{true,false}){lane.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse(point.translated(30,0),juce::ModifierKeys::leftButtonModifier));
        if(escape)lane.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));else lane.focusLost(juce::Component::focusChangedDirectly);
        lane.mouseUp(mouse(point.translated(30,0),0));check(stored(source.id)==edited,"marker Escape/focus cancels preview");}
    d.snap=true;lane.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse(point.translated(17,0),juce::ModifierKeys::leftButtonModifier));lane.mouseUp(mouse(point.translated(17,0),0));
    check(stored(source.id).tick%(mrs::ppq/4)==0,"marker uses Arrange snap");d.app.undo();d.snap=false;d.refresh();
    lane.edit(edited,edited.tick);auto newer=edited;newer.name="Newer";d.app.update_marker(edited,newer);save("Stale",edited.tick,3);
    check(stored(source.id)==newer&&!d.message.isEmpty(),"stale marker form rejects overwrite");d.app.undo();d.refresh();
    lane.mouseDown(mouse(point,juce::ModifierKeys::leftButtonModifier));lane.mouseDrag(mouse(point.translated(30,0),juce::ModifierKeys::leftButtonModifier));d.app.update_marker(edited,newer);lane.mouseUp(mouse(point.translated(30,0),0));
    check(stored(source.id)==newer&&!d.message.isEmpty(),"stale marker gesture rejects overwrite");d.app.undo();d.refresh();
    for(double zoom:{30.,120.,300.})for(double scroll:{0.,70.}){a.pixelsPerSecond=zoom;a.horizontal=scroll;const auto x=lane.xAt(source.tick);if(x>=Arrangement::left)check(lane.markersAt(x).size()==6,"marker zoom/scroll hit test keeps ties");}
    a.pixelsPerSecond=120;a.horizontal=0;lane.select(source.id);lane.keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey));check(d.project()->markers.size()==5,"Backspace deletes selected marker only");check(d.app.undo()&&stored(source.id)==edited,"marker delete Undo restores order");d.refresh();
    lane.mouseDoubleClick(mouse({lane.xAt(5*mrs::ppq),12},juce::ModifierKeys::leftButtonModifier));save("Navigation",5*mrs::ppq,5);check(d.project()->markers.size()==7,"empty marker double click creates via form");
    lane.select(source.id);lane.mouseDown(mouse(point,juce::ModifierKeys::rightButtonModifier));check(lane.paletteEditor!=nullptr,"marker right click palette");
    referencePreview(*lane.paletteEditor->getTopLevelComponent(),"marker-menu-upd2");bool paletteClicked=false;for(auto* child:lane.paletteEditor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getName()=="Teal"){button->onClick();paletteClicked=true;break;}pump();
    check(paletteClicked&&stored(source.id).color==0x287B89u,"marker color square commits");check(d.app.undo()&&stored(source.id)==edited&&d.app.redo()&&stored(source.id).color==0x287B89u,"marker color Undo/Redo");
    d.message.clear();d.refresh();for(float scale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-marker-track-"+juce::String(static_cast<int>(scale*100))+"-preview.png").createOutputStream();
        check(stream&&stream->setPosition(0)&&stream->truncate().wasOk()&&juce::PNGImageFormat().writeImageToStream(d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*stream),"marker preview export");}
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mrs-markers-"+juce::Uuid().toString());folder.createDirectory();const auto file=folder.getChildFile("Markers.mrsproject");const auto expected=d.project()->markers;
    d.app.save_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));d.app.open_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));d.refresh(true);
    check(d.project()->markers==expected&&!lane.selected,"marker Save/Open and transient selection reset");
    lane.edit(expected.front(),expected.front().tick);d.app.new_project();d.refresh(true);save("Other project",0,0);check(d.project()->markers.empty()&&!d.message.isEmpty(),"replacement project rejects old marker form");
    d.removeFromDesktop();check(folder.getParentDirectory()==juce::File::getSpecialLocation(juce::File::tempDirectory)&&folder.getFileName().startsWith("mrs-markers-"),"marker cleanup scope");folder.deleteRecursively();
}
void musicalContextSmoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    struct Clock final:mrs::audio::IAudioDevice{
        std::vector<mrs::audio::DeviceInfo> enumerate()override{return {{0,"Context fixture",{}, {"L","R"},16,8192,128,1}};}
        void control_panel(int)override{}void open(const mrs::audio::DeviceConfig&,std::shared_ptr<mrs::audio::AudioEngine>)override{}
        void start()override{}void stop()override{}void close()noexcept override{}mrs::audio::DeviceStatus status()override{return {mrs::audio::DevicePhase::running,48000};}
    };
    Desktop d(true);d.app.new_project();const auto upper=d.app.add_instrument_track("Upper");const auto lower=d.app.add_instrument_track("Lower");d.app.create_midi_clip(upper,0,8*mrs::ppq);d.app.create_midi_clip(lower,0,8*mrs::ppq);
    auto p=*d.project();p.chords={{{"ctx-a"},"Am",0,4*mrs::ppq},{{"ctx-b"},"C/G",5*mrs::ppq,8*mrs::ppq}};
    p.sections={{{"ctx-intro"},"Вступление",0,4*mrs::ppq,0x4056d6},{{"ctx-verse"},"Verse",5*mrs::ppq,8*mrs::ppq,0x7050a0}};
    p.markers={{{"ctx-first"},"First",4*mrs::ppq,mrs::MarkerKind::cue},{{"ctx-tie"},"Последний",4*mrs::ppq,mrs::MarkerKind::action},{{"ctx-last"},"End",8*mrs::ppq,mrs::MarkerKind::navigation}};
    d.app.services().projects->execute(mrs::SetMusicalData{p.time,p.chords,p.sections,p.markers});
    d.app.connect(std::make_unique<Clock>(),{0,48000,128,{}, {0,1}});d.setWorkspace(mrs::desktop::Workspace::arrange);d.refresh(true);d.setSize(1200,700);d.setVisible(true);
    const mrs::Timeline clock(p.time,p.sample_rate);std::array<float,256> output{};
    auto acknowledge=[&]{d.app.engine()->process(nullptr,output.data(),128);d.app.poll();d.refresh();};
    check(d.contextLabels[0].getText()==juce::String::fromUTF8("Chord: Am  →  C/G"),"context initial chord");
    const auto before=d.displayedMusical;d.app.seek(clock.to_samples(4*mrs::ppq));d.refresh();
    check(d.displayedMusical==before,"context queued seek is not optimistic");acknowledge();
    check(!d.displayedMusical.current_chord&&!d.displayedMusical.current_section&&d.displayedMusical.next_chord->symbol=="C/G","context exact ends and gap");
    check(d.visualSample()==static_cast<double>(d.displayedMusical.transport.sample),"visual head resets interpolation at musical boundary");
    check(d.displayedMusical.current_marker->id==mrs::Id{"ctx-tie"}&&d.displayedMusical.next_marker->id==mrs::Id{"ctx-last"},"context tied marker order");
    {mrs::audio::AudioEngineTestAccess::Busy busy(*d.app.engine());d.refresh();check(d.displayedMusical.tick==4*mrs::ppq,"context busy retains acknowledged position");}
    d.app.seek(clock.to_samples(5*mrs::ppq));acknowledge();check(d.arrangement->chords->currentChord()==mrs::Id{"ctx-b"},"lane uses same context snapshot");
    const auto current=*d.displayedMusical.current_chord;auto edited=current;edited.symbol="Доминанта";const auto generation=d.app.insert_generation();
    d.app.update_chord(current,edited);d.refresh();check(d.contextLabels[0].getText().contains(juce::String::fromUTF8("Доминанта")),"context edit refresh");
    d.app.undo();d.refresh();check(d.displayedMusical.current_chord->symbol=="C/G","context Undo");d.app.redo();d.refresh();
    check(d.app.insert_generation()==generation,"context has no DSP rebuild");
    d.app.musical().loop_section({"ctx-verse"});d.app.seek(clock.to_samples(8*mrs::ppq)-64);d.app.play();acknowledge();
    check(d.displayedMusical.current_section->id==mrs::Id{"ctx-verse"}&&d.displayedMusical.tick>=5*mrs::ppq&&d.displayedMusical.tick<8*mrs::ppq,"context loop wrap");
    d.app.stop();acknowledge();d.app.musical().clear_loop();acknowledge();
    const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mrs-context",".mrsproject",false);
    d.app.save_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));d.app.new_project();d.refresh(true);check(!d.displayedMusical.current_chord&&!d.displayedMusical.next_marker,"replacement clears context");
    d.app.open_project(std::filesystem::path(file.getFullPathName().toWideCharPointer()));d.app.connect(std::make_unique<Clock>(),{0,48000,128,{}, {0,1}});d.refresh(true);check(d.displayedMusical.current_chord->symbol=="Am","Open context restored");file.deleteFile();
    d.app.seek(clock.to_samples(8*mrs::ppq));acknowledge();check(!d.displayedMusical.current_chord&&!d.displayedMusical.next_chord&&!d.displayedMusical.current_section&&!d.displayedMusical.next_section&&!d.displayedMusical.next_marker,"end context clears fields");
    mrs::Clip top,bottom;top.track=d.project()->tracks[0].id;bottom.track=d.project()->tracks[1].id;top.length=bottom.length=48000;
    for(const int height:{40,56,128,200}){d.arrangement->trackHeight=height;d.arrangement->vertical=17;const auto a=d.arrangement->clipRect(top),b=d.arrangement->clipRect(bottom);check(b.getY()-a.getBottom()==2.f,"adjacent clip gap exactly two logical pixels, matching Mixer");}
    for(auto* child:d.getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))check(button->getButtonText()!="Arrange"&&button->getButtonText()!="Audio settings","obsolete toolbar buttons removed");
    d.arrangement->trackHeight=128;d.arrangement->vertical=0;d.app.seek(clock.to_samples(5*mrs::ppq));acknowledge();
    for(auto* child:d.arrangement->getChildren())if(auto* line=dynamic_cast<PlayheadLine*>(child))line->update();
    for(const float scale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(scale==1.f?"juce-context-100-preview.png":"juce-context-150-preview.png").createOutputStream();check(stream!=nullptr,"context preview stream");stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*stream);}
}
void backlog150Smoke(){
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};Desktop d(true);d.app.new_project();const auto track=d.app.add_instrument_track("0.4 MIDI");const auto first=d.app.create_midi_clip(track,0,4*mrs::ppq),second=d.app.create_midi_clip(track,0,4*mrs::ppq);d.refresh(true);d.setSize(1400,850);d.setVisible(true);d.snap=false;auto& a=*d.arrangement;a.pixelsPerSecond=120.;
    auto mouse=[&](juce::Point<float> point,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,&a,&a,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,true);};
    const auto before=*d.project();d.selectedClip=second;d.selectedClips={first.value,second.value};const auto r=a.clipRect(before.clips.back());const juce::Point<float> edge{r.getRight()-2,r.getCentreY()};a.mouseDown(mouse(edge,juce::ModifierKeys::leftButtonModifier));a.mouseDrag(mouse(edge.translated(-24,0),juce::ModifierKeys::leftButtonModifier));check(*d.project()==before,"group length gesture is preview only");a.mouseUp(mouse(edge.translated(-24,0),0));check(d.project()->clips[0].midi->length==d.project()->clips[1].midi->length&&d.project()->clips[0].midi->length<4*mrs::ppq,"group length edits both MIDI clips");d.action(10);check(*d.project()==before,"group length one Undo");
    d.selectedClips.clear();d.selectedClip.reset();check(d.keyPressed(juce::KeyPress('3'))&&d.knifeMode,"3 selects knife");a.mouseDown(mouse({r.getX()+r.getWidth()*.5f,r.getCentreY()},juce::ModifierKeys::leftButtonModifier));check(d.project()->clips.size()==3&&d.message.isEmpty(),"knife splits clicked MIDI clip");check(d.keyPressed(juce::KeyPress('1'))&&!d.knifeMode,"1 restores pointer");d.action(10);check(*d.project()==before,"knife Undo exact");
    auto color=std::make_shared<std::uint32_t>(0);MenuColourPalette palette(d.theme,0,color,true);int w{},h{};palette.getIdealSize(w,h);palette.setSize(w,h);check(palette.getNumChildComponents()==14,"clip palette has twice marker colors");referencePreview(palette,"backlog150-clip-palette");
    ViewSettings settings;settings.sectionTemplates={{"Припев",0xF15E62}};check(ViewSettings::decode(settings.encode()).sectionTemplates==settings.sectionTemplates,"section template settings round trip");
    auto section=d.app.add_section("Verse",0,4*mrs::ppq,0x4056D6);d.refresh();const auto original=*d.project();a.sections->edit(d.project()->sections.front());auto* form=a.sections->popupEditor();check(form!=nullptr,"section template popup");
    for(auto* child:form->getChildren())if(auto* input=dynamic_cast<juce::TextEditor*>(child)){if(input->getName()=="Name")input->setText("Template150");if(input->getName()=="RGB")input->setText("F15E62");}
    for(auto* child:form->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getName()=="Save section template")button->onClick();
    check(d.view.sectionTemplates.size()==1&&d.view.sectionTemplates.front().name=="Template150"&&*d.project()==original,"saving template changes settings only");
    for(auto* child:form->getChildren())if(auto* input=dynamic_cast<juce::TextEditor*>(child))if(input->getName()=="Name")input->setText("Temporary");
    for(auto* child:form->getChildren())if(auto* combo=dynamic_cast<juce::ComboBox*>(child))combo->setSelectedId(1,juce::sendNotificationSync);
    bool restored=false;for(auto* child:form->getChildren())if(auto* input=dynamic_cast<juce::TextEditor*>(child))if(input->getName()=="Name")restored=input->getText()=="Template150";check(restored&&d.project()->sections.front().id==section,"template selection fills draft without changing project");referencePreview(*form->getTopLevelComponent(),"backlog150-section-template");juce::PopupMenu::dismissAllActiveMenus();
    d.app.set_clip_colors({first,second},0x23C9CF);d.refresh();referencePreview(d,"backlog150-main");
}
void j3Smoke(Desktop& d){
    backlog150Smoke();
    musicalContextSmoke();
    markerTrackSmoke();
    chordTrackSmoke();
    controllerScrollSmoke();
    noteEditorLayoutSmoke();
    trackRenameSmoke();
    mixerSendsSmoke();
    {
        const auto original=d.getBounds();
        auto button=[&](juce::String text)->juce::TextButton*{for(auto* child:d.getChildren())if(auto* b=dynamic_cast<juce::TextButton*>(child);b&&b->getButtonText()==text)return b;throw std::runtime_error("footer control missing");};
        const auto play=button("Play"),brows=button("BROWS"),loop=button("Loop section"),edit=button("Edit");
        for(int width:{1200,1400,1920}){d.setSize(width,800);d.resized();
            if(play->getBottom()!=d.getHeight()-4||brows->getBottom()!=d.getHeight()-4||
                d.latencyReadout.getX()!=d.audioCpuBar.getRight()+6||
                d.latencyReadout.getBounds().intersects(play->getBounds())||loop->getRight()>edit->getX())throw std::runtime_error("compact footer geometry/overlap");
            if(d.master&&d.app.workspace()==mrs::desktop::Workspace::mix&&d.master->getBottom()!=d.mixArea.getBottom())throw std::runtime_error("mixer reaches bottom of dock without gap");
            for(float scale:{1.f,1.5f}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-footer-"+juce::String(width)+"-"+juce::String(static_cast<int>(scale*100))+"-preview.png").createOutputStream();if(!stream||!juce::PNGImageFormat().writeImageToStream(d.createComponentSnapshot(d.getLocalBounds(),true,scale,juce::SoftwareImageType{}),*stream))throw std::runtime_error("footer preview");}
        }
        if(!d.footerStatus().contains("PDC"))throw std::runtime_error("footer diagnostics retained");
        d.setBounds(original);d.resized();
    }
    externalMidiSmoke();
    clipCopySmoke();
    stateBusySmoke();
    projectHomeSmoke();
    upd14Smoke();
    navigation86Smoke();
    midiImport149Smoke();
    section81Smoke();arranger80Smoke();unifiedLaneUpd2Smoke();
    tempo83Smoke();meter84Smoke();
    auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    const auto fileMenu=d.getMenuForIndex(0,"File");bool diagnosticEntry=false;juce::PopupMenu::MenuItemIterator menuItems(fileMenu);
    while(menuItems.next())if(menuItems.getItem().itemID==7)diagnosticEntry=menuItems.getItem().isEnabled&&menuItems.getItem().text=="Open diagnostics folder";
    check(diagnosticEntry,"File diagnostics entry");
    if(!mrs::diagnostics::folder().empty()){const auto revision=d.app.services().projects->state().revision;d.action(7);check(d.message.isEmpty()&&d.app.services().projects->state().revision==revision,"diagnostics action is not a project edit");mrs::diagnostics::event("gui_diagnostics_menu_smoke");}
    mrs::audio::DeviceStatus performance{mrs::audio::DevicePhase::running,48000,2.7,5.4,.5,{}};d.cpuMeasured=false;
    d.updatePerformance(performance,true);check(d.cpuReadout.getText()=="CPU 50.0%"&&d.audioCpu==.5&&d.latencyReadout.getText()=="Latency I 2.70 / O 5.40 ms","driver load and latency readouts");
    performance.cpu_load=1.25;d.cpuUpdateMs-=100;d.updatePerformance(performance,true);check(d.displayedCpu>.5&&d.displayedCpu<1.25,"CPU display smooths jumps");d.cpuMeasured=false;d.updatePerformance(performance,true);check(d.cpuReadout.getText()=="CPU 125.0%"&&d.audioCpu==1.,"overload readout exceeds 100 while bar saturates");
    d.updatePerformance(performance,false);check(d.cpuReadout.getText()=="CPU --%"&&d.latencyReadout.getText().contains("--")&&d.audioCpu==0,"offline clock does not claim measured hardware CPU or latency");
    j3AudioSmoke(d);profilingSmoke(d);
    auto dividerEvent=[&](juce::Point<float> point){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&d.browserDivider,&d.browserDivider,juce::Time::getCurrentTime(),{4,20},juce::Time::getCurrentTime(),1,true);};
    d.mouseDown(dividerEvent({4,20}));d.mouseDrag(dividerEvent({-336,20}));d.mouseUp(dividerEvent({4,20}));check(d.browserWidth==604,"browser divider drag uses local coordinates");
    d.resizeBrowser(600);check(d.browserWidth==600&&d.browserDivider.getBounds().getRight()==d.browser->getX(),"browser widens with usable divider");
    d.resizeBrowser(2000);check(d.browserWidth<=700&&d.arrangeArea.getWidth()>=680,"browser maximum keeps arrangement usable");d.resizeBrowser(260);
    const auto mixerSetting=d.view.mixerHeight;d.resizeMixer(260);const auto mixerBefore=d.mixArea.getHeight();const auto fixedFader=d.master->gain.getHeight();
    auto mixerEvent=[&](juce::Point<float> point){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&d.mixerDivider,&d.mixerDivider,juce::Time::getCurrentTime(),{30,2},juce::Time::getCurrentTime(),1,true);};
    d.mouseDown(mixerEvent({30,2}));d.mouseDrag(mixerEvent({30,-48}));d.mouseUp(mixerEvent({30,2}));
    check(d.mixArea.getHeight()>mixerBefore&&d.arrangeArea.getHeight()>=140&&d.master->gain.getHeight()==fixedFader,"mixer divider freely grows insert area without stretching faders");d.resizeMixer(mixerSetting);

    d.grabKeyboardFocus();d.setWorkspace(mrs::desktop::Workspace::arrange);check(d.keyPressed(juce::KeyPress(juce::KeyPress::F3Key))&&d.app.workspace()==mrs::desktop::Workspace::mix,"F3 opens mixer");check(d.keyPressed(juce::KeyPress(juce::KeyPress::F3Key))&&d.app.workspace()==mrs::desktop::Workspace::arrange,"F3 closes mixer");const bool browserBeforeKey=d.sidebar;check(d.keyPressed(juce::KeyPress(juce::KeyPress::F5Key))&&d.sidebar!=browserBeforeKey,"F5 toggles browser");d.keyPressed(juce::KeyPress(juce::KeyPress::F5Key));d.selectedClip=d.project()->clips.front().id;check(d.keyPressed(juce::KeyPress(juce::KeyPress::F2Key))&&d.clipEditorOpen(),"F2 opens selected editor");d.grabKeyboardFocus();check(d.keyPressed(juce::KeyPress(juce::KeyPress::F2Key))&&!d.clipEditorOpen(),"F2 closes editor");d.setWorkspace(mrs::desktop::Workspace::mix);
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
        auto nested=presetChild(pluginFolder,"Keys/Warm/Clean",true);check(nested.getParentDirectory().createDirectory().wasOk(),"nested preset folder created");
        auto alternative=effect;alternative.gain=.75f;publishSettings(nested,encodePreset(alternative));
        auto empty=presetChild(pluginFolder,"Empty",false);check(empty.createDirectory().wasOk(),"empty preset folder created");
        files=presetFiles(effect,folder);check(files.size()==2,"root and nested preset files discovered");
        check(presetLabel(nested,pluginFolder)=="Keys/Warm/Clean","relative nested preset label");
        auto menu=presetMenu(files,pluginFolder,"Keys/Warm/Clean");int leaves=0,ticked=0;for(juce::PopupMenu::MenuItemIterator it(menu,true);it.next();){auto& item=it.getItem();if(item.itemID>0&&item.isEnabled){++leaves;check(item.text=="Clean","duplicate basenames remain separate leaves");if(item.isTicked){++ticked;check(files[item.itemID-1]==nested,"nested selected menu maps to exact file");}}}check(leaves==2&&ticked==1,"folder menu exact mapping and current checkmark");
        {
            PresetMenuTheme popupTheme;menu.setLookAndFeel(&popupTheme);
            if(!d.getPeer())d.addToDesktop(0);d.setVisible(true);const auto countBefore=d.getNumChildComponents();
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&d).withParentComponent(&d).withMinimumWidth(280).withStandardItemHeight(22).withMaximumNumColumns(1),[](int){});
            auto pumpMenu=[](){for(int pass=0;pass<10;++pass){MSG msg{};int limit=0;while(limit++<200&&PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}std::this_thread::sleep_for(std::chrono::milliseconds(1));}};
            check(d.getNumChildComponents()>countBefore,"preset popup renders");
            auto* popup=d.getChildComponent(d.getNumChildComponents()-1);check(popup&&popup->getWidth()>=280&&popup->getWidth()<380,"compact preset popup width");
            for(float previewScale:{1.f,1.5f}){auto output=juce::File::getCurrentWorkingDirectory().getChildFile(previewScale==1.f?"juce-preset-menu-100-preview.png":"juce-preset-menu-150-preview.png").createOutputStream();if(output){output->setPosition(0);output->truncate();juce::PNGImageFormat format;format.writeImageToStream(popup->createComponentSnapshot(popup->getLocalBounds(),true,previewScale,juce::SoftwareImageType{}),*output);}}
            juce::PopupMenu::dismissAllActiveMenus();pumpMenu();
        }
        for(const auto& bad:{"../escape","C:/escape","/escape","A//B","A/../B","CON","LPT1","A/Bad."}){bool rejected=false;try{presetChild(pluginFolder,bad,true);}catch(...){rejected=true;}check(rejected,"unsafe preset path rejected");}
        check(presetChild(pluginFolder,"Clean.1",true).getFileName()=="Clean.1.mrspreset","dots in preset names retained");
        d.closeEditors();auto target=d.project()->tracks.front().id;auto chain=d.chain(target);auto namedEffect=effect;namedEffect.id=mrs::new_id();chain.push_back(namedEffect);d.app.set_inserts(target,chain);
        const auto generation=d.app.insert_generation();d.app.name_insert_preset(target,namedEffect.id,"Keys/Warm/Clean");
        check(d.app.insert_generation()==generation&&d.chain(target).back().preset_name=="Keys/Warm/Clean","preset naming is metadata only");
        check(mrs::deserialize(mrs::serialize(*d.project())).tracks.front().inserts.back().preset_name=="Keys/Warm/Clean","preset label project restore");
        d.app.undo();check(d.chain(target).back().preset_name.empty(),"preset name Undo");d.app.redo();check(d.chain(target).back().preset_name=="Keys/Warm/Clean","preset name Redo");
        presetSelectorSmoke(d,target,namedEffect.id,pluginFolder);
        d.openInsert(target,namedEffect.id);auto* selector=dynamic_cast<juce::ComboBox*>(d.windows.back()->getContentComponent()->findChildWithID("preset-library"));check(selector&&selector->getText()=="Keys/Warm/Clean","reopened editor displays durable preset name");d.closeEditors();chain.pop_back();d.app.set_inserts(target,chain);
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
    ViewSettings settings;settings.sidebar=false;settings.snap=true;settings.browserWidth=440;settings.trackHeight=320;settings.mixerHeight=440;settings.editorHeight=620;settings.pixelsPerSecond=1200;settings.inputs="1,2";settings.controllersOpen=false;settings.editorAttached=true;
    auto decoded=ViewSettings::decode(settings.encode());check(!decoded.sidebar&&decoded.snap&&decoded.browserWidth==440&&decoded.trackHeight==320&&decoded.pixelsPerSecond==1200&&decoded.inputs=="1,2"&&!decoded.controllersOpen&&decoded.editorAttached,"view settings roundtrip");
    check(decoded.mixerHeight==440&&decoded.editorHeight==620,"independent editor/mixer height settings roundtrip");
    settings.mixerHeight=1800;check(ViewSettings::decode(settings.encode()).mixerHeight==1800,"tall-window mixer height survives settings reload");
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
