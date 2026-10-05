#include "Desktop.h"
#include "J1Smoke.h"
#include "PluginPreset.h"
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
    j3AudioSmoke(d);
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
