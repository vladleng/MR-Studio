#include "Desktop.h"
#include "J1Smoke.h"
#include <windows.h>
namespace ui {
void j2Smoke(Desktop& d,const juce::File& fixture){
    auto check=[](bool b,const char* text){if(!b)throw std::runtime_error(text);};
    d.app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});
    const auto first=d.project()->tracks.front().id;
    d.action(20);check(d.project()->tracks.size()==2,"add track binding");
    auto second=d.project()->tracks.back().id;d.app.rename_track(second,"Stereo guitar");
    d.action(21);auto bus=d.project()->tracks.back().id;
    d.app.set_track_output(first,bus);d.app.set_track_sends(second,{{bus,.5f,true}});d.refresh(true);
    check(d.mixer.size()==3&&d.arrangement->rows.size()==3,"multiple channel views");
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
    mrs::NativeInsert eq;eq.id=mrs::new_id();eq.kind=mrs::InsertKind::channel_eq;d.applyChain(second,{eq});d.openInsert(second,eq.id);
    check(!d.windows.empty()&&d.windows.back()->isVisible(),"native editor bridge");d.closeEditors();
    d.openInsert(second,eq.id);auto* editor=d.windows.back()->getContentComponent();
    auto eqEvent=[&](juce::Point<float> p,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(mods),1,0,0,0,0,editor,editor,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,true);};
    const auto eqRevision=d.app.services().projects->state().revision;d.app.play();render();
    const float midX=24+static_cast<float>(std::log(1000./20)/std::log(1000.))*660;
    editor->mouseDown(eqEvent({midX,193},juce::ModifierKeys::leftButtonModifier));editor->mouseDrag(eqEvent({midX+30,173},juce::ModifierKeys::leftButtonModifier));
    check(d.app.services().projects->state().revision==eqRevision,"live EQ preview");editor->mouseUp(eqEvent({midX+30,173},0));render();
    check(d.chain(second).front().bands[2].gain>0&&d.app.services().projects->state().revision==eqRevision+1,"live EQ commit");
    d.app.stop();render();d.closeEditors();
    if(fixture!=juce::File()){
        d.catalog=mrs::processing::probe_vst3(std::string(fixture.getFullPathName().toUTF8()));check(!d.catalog.empty(),"fixture probe");
        juce::DragAndDropTarget::SourceDetails drop("vst3:0",d.browser.get(),{50,50});
        const auto revision=d.app.services().projects->state().revision;d.mixer[1]->itemDropped(drop);
        check(d.app.services().projects->state().revision==revision+1,"plugin drop commits once");
        auto plugin=d.chain(second).back();d.openInsert(second,plugin.id);check(d.windows.back()->isVisible(),"VST3 HWND bridge");
        auto* content=d.windows.back()->getContentComponent();check(content->getWidth()==320&&content->getHeight()==120,"native plugin dimensions");
        const auto count=d.windows.size();d.openInsert(second,plugin.id);check(d.windows.size()==count,"editor repeated-click reuse");
        d.closeEditors();d.app.set_plugin_parameter(second,plugin.id,100,.25f);render();
    }
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mrs-juce-j2","",false);folder.createDirectory();
    const auto file=folder.getChildFile("roundtrip.mrsproject");
    const auto wav=folder.getChildFile("stereo.wav");
    {mrs::audio::Recorder recorder(std::filesystem::path(wav.getFullPathName().toWideCharPointer()),48000,0,{0,1});std::array<float,1024> samples{};
        for(std::size_t i=0;i<samples.size();i+=2){samples[i]=.25f;samples[i+1]=-.5f;}recorder.capture(samples.data(),2,512,0);recorder.finish();}
    d.importFiles({wav.getFullPathName()});check(d.project()->tracks.size()==4,"WAV import track");
    const auto imported=d.project()->clips.back();
    d.saveFile(file);const auto saved=mrs::persistence::load_project(d.app.path()).project;d.app.rename_track(second,"Changed");d.openFile(juce::File(juce::String(d.app.path().wstring().c_str())));
    check(*d.project()==saved,"project roundtrip");
    std::string source;for(const auto& c:d.project()->clips)if(c.name==imported.name)source=c.source;
    for(int i=0;i<500&&!d.app.waveform(source);++i){d.app.poll();juce::Thread::sleep(1);}
    for(const auto& c:d.project()->clips)if(c.name=="stereo.wav"){const auto* wave=d.app.waveform(c.source);check(wave&&wave->channels()==2,"stereo waveform channels");check(wave->range(0,512,0).maximum>.2f&&wave->range(0,512,1).minimum<-.4f,"independent L/R waveform");}
    d.app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});d.app.seek(4800);render();d.action(30);render();d.action(32);render();
    check(d.app.engine()->state().sample==4800,"stop returns to start");
    d.spaceKey(true);render();check(d.app.engine()->state().playback==mrs::PlaybackState::playing,"space starts");d.spaceKey(true);render();check(d.app.engine()->state().playback==mrs::PlaybackState::playing,"space repeat rejected");
    d.spaceKey(false);d.spaceKey(true);render();check(d.app.engine()->state().playback==mrs::PlaybackState::stopped,"space stops");d.spaceKey(false);
    d.selectedClip=d.project()->clips.front().id;const auto clips=d.project()->clips.size();d.action(25);check(d.project()->clips.size()+1==clips,"delete clip");d.action(10);
    d.setSize(1200,700);check(d.arrangeArea.getWidth()>0&&d.mixArea.getHeight()>0,"minimum layout");d.setSize(1400,850);d.arrangement->fit();
    d.app.play();for(int i=0;i<8;++i)render();d.app.stop();render();
    const auto meter=d.app.engine()->take_meters();d.peaks=meter.tracks;d.masterPeak=meter.master;
    for(const auto& clip:d.project()->clips){for(int i=0;i<1000&&!d.app.waveform(clip.source);++i){d.app.poll();juce::Thread::sleep(1);}check(d.app.waveform(clip.source)!=nullptr,"visible waveform prepared");}
    for(const auto& clip:d.project()->clips)if(clip.source=="mrs:demo-tone")check(d.app.waveform(clip.source)->range(0,48000,0).maximum>.05f,"demo waveform data");
    // Fixture is under a unique temporary directory created by this test only.
    check(folder.getParentDirectory()==juce::File::getSpecialLocation(juce::File::tempDirectory)&&folder.getFileName().startsWith("mrs-juce-j2"),"fixture cleanup scope");folder.deleteRecursively();
}
void j2PluginSmoke(Desktop& d,const juce::File& module){
    auto check=[](bool b,const char* text){if(!b)throw std::runtime_error(text);};
    const auto log=juce::File::getCurrentWorkingDirectory().getChildFile("juce-plugin-smoke.log");log.replaceWithText(module.getFullPathName()+"\n");
    auto stage=[&](const char* s){log.appendText(juce::String(s)+"\n");};
    d.app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});
    stage("probe");
    d.catalog=mrs::processing::probe_vst3(module.getFullPathName().toStdString());check(!d.catalog.empty(),"external probe");
    stage("load");auto track=d.project()->tracks.front().id;d.addPlugin(track,0);check(d.chain(track).size()==1,"external load");auto slot=d.chain(track).front().id;
    stage("open editor");
    d.openInsert(track,slot);check(!d.windows.empty()&&static_cast<bool>(d.windows.back()->getProperties()["mrs-native-editor"]),"external native editor");
    stage("reuse and close editor");const auto count=d.windows.size();d.openInsert(track,slot);check(d.windows.size()==count,"external editor reuse");d.closeEditors();
    auto pump=[] {const auto until=juce::Time::getMillisecondCounter()+80;while(juce::Time::getMillisecondCounter()<until){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}juce::Thread::sleep(1);}};
    pump();
    std::array<float,256> audio{};d.app.play();for(int i=0;i<4;++i){d.app.engine()->process(nullptr,audio.data(),128);d.app.poll();}d.app.stop();d.app.engine()->process(nullptr,audio.data(),128);d.app.poll();
    check(std::all_of(audio.begin(),audio.end(),[](float v){return std::isfinite(v);}),"external finite process");
    stage("capture project");const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);auto folder=temp.getNonexistentChildFile("mrs-juce-plugin","",false);folder.createDirectory();d.saveFile(folder.getChildFile("plugin.mrsproject"));
    const auto saved=mrs::persistence::load_project(d.app.path()).project;check(!saved.tracks.front().inserts.front().component_state.empty(),"opaque component capture");
    pump();stage("reopen project");d.openFile(juce::File(d.app.path().wstring().c_str()));check(d.project()->tracks.front().inserts.front().component_state==saved.tracks.front().inserts.front().component_state,"opaque component reopen");
    stage("reopen editor");
    d.app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});d.openInsert(track,slot);check(static_cast<bool>(d.windows.back()->getProperties()["mrs-native-editor"]),"external reopened editor");d.closeEditors();
    check(folder.getParentDirectory()==temp&&folder.getFileName().startsWith("mrs-juce-plugin"),"external cleanup scope");folder.deleteRecursively();
    stage("passed");
}
}
