#include "Desktop.h"
#include "ProjectHome.h"
#include "WaveformView.h"
#include <fstream>
#include <sstream>

namespace ui {
namespace {
juce::String label(const std::string& s){return juce::String::fromUTF8(s.c_str());}
std::filesystem::path path(const juce::File& f){return std::filesystem::path(f.getFullPathName().toWideCharPointer());}
juce::String insertName(const mrs::NativeInsert& n){
    if(n.kind==mrs::InsertKind::vst3)return label(n.plugin_name);
    if(n.kind==mrs::InsertKind::channel_eq)return "Channel EQ";
    if(n.kind==mrs::InsertKind::cab_ir)return "Cab IR";
    if(n.kind==mrs::InsertKind::highpass)return "High pass";
    if(n.kind==mrs::InsertKind::lowpass)return "Low pass";
    return n.kind==mrs::InsertKind::eq ? "EQ" : "Gain";
}
juce::PopupMenu::Options popup(juce::Component* c){return juce::PopupMenu::Options().withTargetComponent(c);}
}
EditorWindow::EditorWindow(juce::String name,juce::Component* content,bool show)
    :DocumentWindow(name,juce::Colour(background),closeButton){
    setUsingNativeTitleBar(true);setContentOwned(content,true);centreWithSize(content->getWidth(),content->getHeight());
    setVisible(show);
}
void EditorWindow::closeButtonPressed(){if(onClose)onClose();setVisible(false);}
void EditorWindow::fitNativeEditor(int width,int height){
    const double scale=getPeer()?getPeer()->getPlatformScaleFactor():1.;
    if(width<1||height<1||width>4096||height>4096||!std::isfinite(scale)||scale<=0)
        throw std::runtime_error("Invalid native editor dimensions");
    setResizeLimits(1,1,8192,8192);
    setContentComponentSize(juce::jmax(1,juce::roundToInt(width/scale)),juce::jmax(1,juce::roundToInt(height/scale)));
}

Desktop::Desktop(bool test,bool startAtHome):testing(test){
    setLookAndFeel(&theme);setWantsKeyboardFocus(true);setTitle("Moon River Studio workspace");setFocusContainerType(FocusContainerType::keyboardFocusContainer);
    const auto config=juce::File(juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA",juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getFullPathName())).getChildFile("MoonRiverStudio");
    viewFile=path(config.getChildFile("juce-view.json"));prefsFile=path(config.getChildFile("juce-preferences.json"));cacheFile=path(config.getChildFile("vst3.cache"));
    if(!testing)try{const auto readPrefs=std::filesystem::exists(prefsFile)?prefsFile:path(config.getChildFile("desktop.cfg"));std::ifstream in(readPrefs,std::ios::binary);if(in){std::string bytes((std::istreambuf_iterator<char>(in)),{});prefs=mrs::desktop::decode_preferences(bytes);}
        catalog=mrs::processing::load_vst3_cache(cacheFile);}catch(const std::exception& e){message=label(e.what());}
    if(!testing)try{juce::File file(juce::String(viewFile.wstring().c_str()));if(file.existsAsFile())view=ViewSettings::decode(file.loadFileAsString());}catch(const std::exception& e){message=label(e.what());}
    sidebar=view.sidebar;browserWidth=view.browserWidth;snap=view.snap;
    projectSessionStarted=testing&&!startAtHome;if(projectSessionStarted)app.demo();else app.new_project();resetDevice();
    app.set_click_settings(prefs.click);
    arrangement=std::make_unique<Arrangement>(*this);browser=std::make_unique<Browser>(*this);
    addAndMakeVisible(browserDivider);browserDivider.setTitle("Resize browser");browserDivider.setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);browserDivider.addMouseListener(this,false);
    addAndMakeVisible(mixerDivider);mixerDivider.setTitle("Resize mixer");mixerDivider.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);mixerDivider.addMouseListener(this,false);
    for(auto* c:{static_cast<juce::Component*>(&menu),static_cast<juce::Component*>(arrangement.get()),static_cast<juce::Component*>(browser.get()),static_cast<juce::Component*>(&mixerViewport)})addAndMakeVisible(c);
    mixerViewport.setViewedComponent(&mixerBody,false);mixerViewport.setScrollBarsShown(false,true);
    addAndMakeVisible(projectTitle);projectTitle.setJustificationType(juce::Justification::centred);projectTitle.setInterceptsMouseClicks(false,false);projectTitle.setFont(theme.font(13));
    for(auto* component:{static_cast<juce::Component*>(&audioCpuBar),static_cast<juce::Component*>(&cpuReadout),static_cast<juce::Component*>(&latencyReadout)})addAndMakeVisible(component);
    cpuReadout.setFont(theme.font(12));latencyReadout.setFont(theme.font(12));
    cpuReadout.setTitle("Audio CPU load percent");audioCpuBar.setTitle("Audio processing budget usage");latencyReadout.setTitle("Device input and output latency in milliseconds");
    cpuReadout.setDescription("Audio callback CPU load as a percentage of the buffer processing deadline; not whole-computer CPU usage.");
    latencyReadout.setDescription("Driver-reported input/output latency. Plugin delay compensation is shown separately as PDC.");
    audioCpuBar.setPercentageDisplay(false);audioCpuBar.setColour(juce::ProgressBar::backgroundColourId,juce::Colour(0xff121619));
    updatePerformance(app.device_status(),false);
    for(auto* b:{&play,&pause,&stop,&record,&previous,&next,&loop,&arrangeButton,&editButton,&mixButton,&brows,&addTrack,&addBus,&undo,&redo,&split,&remove,&zoomIn,&zoomOut,&fit,&audio,&snapButton})addAndMakeVisible(b);
    auto bind=[this](juce::TextButton& b,int cmd){b.onClick=[this,cmd]{action(cmd);};};
    bind(play,30);bind(pause,31);bind(stop,32);bind(record,33);bind(previous,34);bind(next,35);bind(loop,36);
    bind(addTrack,20);bind(addBus,21);bind(undo,10);bind(redo,11);bind(split,24);bind(remove,25);bind(audio,40);
    for(auto* b:{&clickButton,&countButton}){addAndMakeVisible(b);b->setTitle(b==&clickButton?"Toggle metronome":"Toggle recording count-in");b->setTooltip("Toggle on/off. Settings: Transport → Metronome / count-in");}
    clickButton.onClick=[this]{action(503);};countButton.onClick=[this]{action(504);};
    arrangeButton.onClick=[this]{setWorkspace(mrs::desktop::Workspace::arrange);};
    editButton.onClick=[this]{run([&]{toggleEditor();});};
    mixButton.onClick=[this]{setWorkspace(app.workspace()==mrs::desktop::Workspace::mix?mrs::desktop::Workspace::arrange:mrs::desktop::Workspace::mix);};
    brows.onClick=[this]{sidebar=!sidebar;resized();repaint();};
    zoomIn.onClick=[this]{arrangement->zoom(1.25);};zoomOut.onClick=[this]{arrangement->zoom(.8);};fit.onClick=[this]{arrangement->fit();};
    snapButton.setButtonText(snap ? "Snap on" : "Snap off");
    arrangement->trackHeight=view.trackHeight;arrangement->pixelsPerSecond=view.pixelsPerSecond;
    int focus=1;for(auto* b:{&undo,&redo,&addTrack,&addBus,&split,&remove,&zoomIn,&zoomOut,&fit,&snapButton,&audio,&play,&pause,&stop,&record,&previous,&next,&loop,&arrangeButton,&editButton,&mixButton,&brows}){b->setTitle(b->getButtonText());b->setTooltip(b->getButtonText());b->setExplicitFocusOrder(focus++);}
    snapButton.onClick=[this]{snap=!snap;snapButton.setButtonText(snap ? "Snap on" : "Snap off");};
    setSize(1400,850);refresh(true);setWorkspace(testing?mrs::desktop::Workspace::mix:prefs.workspace==mrs::desktop::Workspace::live?mrs::desktop::Workspace::arrange:prefs.workspace);
    home=std::make_unique<ProjectHome>(*this);addChildComponent(*home);if(!testing||startAtHome){showProjectHome(true);if(!testing)home->rescan();}startTimerHz(30);
}
Desktop::~Desktop(){try{saveSettings();}catch(...){}stopTimer();*scanCancel=true;chooser.reset();closeEditors();dockedEditor.reset();home.reset();
    if(scanner.valid())scanner.wait();setLookAndFeel(nullptr);mixerViewport.setViewedComponent(nullptr,false);}
void Desktop::run(std::function<void()> f){try{f();message.clear();if(!app.recording()){recordingPreview.clear();audioRecordingPreview.clear();}else if(recordingPreview.empty()&&audioRecordingPreview.empty())updateRecordingPreview();refresh();}catch(const std::exception& e){mrs::diagnostics::event(std::string("ui_error ")+e.what());refresh();message=label(e.what());repaint();}}
void Desktop::resetDevice(){visualTransport.reset();lastDisplayState={};lastVisualSample=0;app.connect(mrs::audio::make_offline_device(),{0,project()->sample_rate,128,{}, {0,1}});}
mrs::audio::RealtimeState Desktop::displayState() const noexcept{app.engine()->try_state(lastDisplayState);return lastDisplayState;}
double Desktop::visualSample(){const auto* engine=app.engine().get();if(engine!=visualEngine){visualEngine=engine;visualTransport.reset();}mrs::audio::RealtimeState state;if(!engine->try_state(state))return lastVisualSample;return lastVisualSample=visualTransport.sample(state,project()->sample_rate,juce::Time::getMillisecondCounterHiRes());}
double Desktop::visualTick(){const auto p=project();double sample=visualSample(),tick=0;for(std::size_t i=0;i<p->time.tempos.size();++i){const auto& tempo=p->time.tempos[i];const double perTick=60.*p->sample_rate/(tempo.bpm*mrs::ppq);if(i+1==p->time.tempos.size())return tick+sample/perTick;const double length=static_cast<double>(p->time.tempos[i+1].tick-tempo.tick),frames=length*perTick;if(sample<frames)return tick+sample/perTick;sample-=frames;tick+=length;}return 0;}
void Desktop::showProjectHome(bool visible){if(!home)return;homeVisible=visible;if(!visible)projectSessionStarted=true;for(auto* child:getChildren())if(child!=home.get())child->setVisible(!visible);home->setVisible(visible);if(visible){closeEditors();home->toFront(false);}resized();repaint();}
void Desktop::saveSettings(){if(testing)return;
    prefs.workspace=app.workspace();view.sidebar=sidebar;view.browserWidth=browserWidth;view.snap=snap;
    if(arrangement){view.trackHeight=arrangement->trackHeight;view.pixelsPerSecond=arrangement->pixelsPerSecond;}
    publishSettings(juce::File(juce::String(prefsFile.wstring().c_str())),label(mrs::desktop::encode_preferences(prefs)));
    publishSettings(juce::File(juce::String(viewFile.wstring().c_str())),view.encode());
}
void Desktop::remember(){if(testing)return;mrs::desktop::remember_project(prefs,app.path());saveSettings();}
void Desktop::reconnectDevice(bool session){if(testing||(!session&&!prefs.reconnect_audio)||prefs.device_name.empty())return;
#ifdef MRS_HAS_ASIO
    auto device=mrs::audio::make_asio_device();auto infos=device->enumerate();
    auto found=std::find_if(infos.begin(),infos.end(),[&](const auto& info){return info.name==prefs.device_name;});
    if(found==infos.end())throw std::runtime_error("Saved ASIO device unavailable; running offline. Review Audio settings.");
    if(!view.deviceChannels.isEmpty()&&view.deviceChannels!=deviceLayout(*found))
        throw std::runtime_error("ASIO channel names changed; running offline. Review Audio settings.");
    mrs::audio::DeviceConfig config{found->index,project()->sample_rate,prefs.buffer,{},prefs.outputs,prefs.processing_workers,prefs.process_buffer_frames};
    if(!view.inputs.trim().isEmpty())config.inputs=mrs::desktop::parse_outputs(view.inputs.toStdString());
    else if(prefs.monitor_input>=0)config.inputs={prefs.monitor_input};
    try{app.connect(std::move(device),config);prefs.rate=config.sample_rate;saveSettings();}catch(...){resetDevice();throw;}
#endif
}
void Desktop::openFile(const juce::File& f){mrs::diagnostics::event("project_open_begin "+f.getFullPathName().toStdString());const bool session=app.audio_name()!="Offline clock (no sound)"&&app.audio_name()!="Disconnected";closeEditors();cancelPreview();app.open_project(path(f));resetDevice();selectedClip.reset();selectedTrack.reset();remember();refresh(true);arrangement->fit();showProjectHome(false);reconnectDevice(session);mrs::diagnostics::event("project_open_end");}
void Desktop::saveFile(const juce::File& f){auto destination=path(f);if(destination!=app.path())destination=mrs::desktop::project_folder_file(destination);app.save_project(destination);remember();refresh();}
void Desktop::importFiles(const juce::StringArray& files){closeEditors();std::vector<std::filesystem::path> paths;for(const auto& f:files)paths.push_back(path(juce::File(f)));app.import_wavs(paths);refresh(true);}
void Desktop::choose(int chooserOptions,std::function<void(const juce::File&)> callback,juce::String pattern){
    const auto studio=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MR Studio").getChildFile("Projects");
    chooser=std::make_unique<juce::FileChooser>("Moon River Studio",app.path().empty()?studio:juce::File(juce::String(app.path().wstring().c_str())),pattern,true);
    juce::Component::SafePointer<Desktop> safe(this);
    chooser->launchAsync(chooserOptions,[safe,callback=std::move(callback)](const juce::FileChooser& result){
        if(safe && result.getResult()!=juce::File())safe->run([&]{callback(result.getResult());});
    });
}
void Desktop::textDialog(juce::String title,juce::String initial,std::function<void(juce::String)> callback){
    auto* alert=new juce::AlertWindow(title,"",juce::MessageBoxIconType::NoIcon);
    alert->addTextEditor("value",initial);alert->addButton("OK",1,juce::KeyPress(juce::KeyPress::returnKey));alert->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<Desktop> safe(this);
    alert->enterModalState(true,juce::ModalCallbackFunction::create([safe,alert,callback](int r){if(safe && r==1)safe->run([&]{callback(alert->getTextEditorContents("value"));});}),true);
}
void Desktop::confirmDiscard(std::function<void()> nextAction){if(app.recording()){run([&]{app.pause();});if(app.recording()||!message.isEmpty())return;}if(!projectSessionStarted||!app.dirty()){nextAction();return;}
    auto* alert=new juce::AlertWindow("Save project changes?",label(project()->title),juce::MessageBoxIconType::NoIcon);
    alert->addButton("Save",1);alert->addButton("Discard",2);alert->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<Desktop> safe(this);alert->enterModalState(true,juce::ModalCallbackFunction::create([safe,nextAction](int n){if(!safe)return;
        if(n==2)nextAction();else if(n==1)safe->run([&]{if(!safe->app.path().empty()){safe->saveFile(juce::File(juce::String(safe->app.path().wstring().c_str())));nextAction();}
            else safe->choose(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe,nextAction](auto f){if(safe){safe->saveFile(f.withFileExtension("mrsproject"));nextAction();}},"*.mrsproject");});
    }),true);
}
juce::StringArray Desktop::getMenuBarNames(){return {"File","Edit","Track","Transport"};}
juce::PopupMenu Desktop::getMenuForIndex(int n,const juce::String&){juce::PopupMenu m;
    if(n==0){m.addItem(6,"Project home");m.addItem(1,"New project");m.addItem(2,"Open project...");m.addItem(3,"Save");m.addItem(4,"Save as...");m.addItem(5,"Import WAV...");
        juce::PopupMenu recent;for(std::size_t i=0;i<prefs.recent_projects.size();++i)recent.addItem(1000+static_cast<int>(i),label(prefs.recent_projects[i]));m.addSubMenu("Open recent project",recent);m.addSeparator();m.addItem(7,"Open diagnostics folder");}
    if(n==1){m.addItem(10,"Undo",app.services().projects->state().can_undo);m.addItem(11,"Redo",app.services().projects->state().can_redo);m.addItem(24,"Split selected clip (S)");m.addItem(25,"Delete selected clip (Backspace)");m.addItem(43,"Create MIDI clip at cursor",selectedTrack.has_value());m.addItem(44,"Edit MIDI clip notes",selectedClip.has_value());m.addItem(45,"Duplicate selected clip",selectedClip.has_value());m.addItem(46,"Loop selected clip",selectedClip.has_value());}
    if(n==2){m.addItem(20,"Add audio track");m.addItem(28,"Add instrument track");m.addItem(21,"Add bus");m.addItem(22,"Rename track");m.addItem(23,"Delete track");m.addItem(26,"Move track up");m.addItem(27,"Move track down");}
    if(n==3){m.addItem(30,"Play");m.addItem(31,"Pause");m.addItem(32,"Stop");m.addItem(33,"Record");m.addItem(34,"Previous section");m.addItem(35,"Next section");m.addItem(36,"Loop section");m.addItem(40,"Audio settings...");m.addItem(41,"Engine profiling...");m.addItem(42,"MIDI panic / all notes off");m.addItem(47,"Project tempo...");
        const auto c=app.click_settings();juce::PopupMenu click,count,level;const bool enabled=!app.recording();
        click.addItem(500,"During playback",enabled,c.playback);click.addItem(501,"During recording",enabled,c.recording);click.addItem(502,"Accent first beat",enabled,c.accent);
        for(const auto value:{10,20,35,50,75,100})level.addItem(600+value,juce::String(value)+"%",enabled,c.level==value);click.addSubMenu("Level",level);
        for(int bars=0;bars<=4;++bars)count.addItem(510+bars,bars?juce::String(bars)+" bar(s)":"Off",enabled,c.count_bars==bars);
        click.addSubMenu("Count-in before recording",count);m.addSubMenu("Metronome / count-in",click);}
    return m;
}
void Desktop::menuItemSelected(int n,int){action(n);}
void Desktop::action(int n){if(busyGesture())return;mrs::diagnostics::event("ui_action="+std::to_string(n));run([&]{
    if(n==7){const auto logs=mrs::diagnostics::folder();if(logs.empty())throw std::runtime_error("Diagnostics unavailable; see package README");if(!testing)juce::File(juce::String(logs.wstring().c_str())).revealToUser();return;}
    if(n>=500&&n<=700){auto c=app.click_settings();if(n==500)c.playback=!c.playback;else if(n==501)c.recording=!c.recording;else if(n==502)c.accent=!c.accent;else if(n==503)c.playback=c.recording=!(c.playback||c.recording);else if(n==504){if(c.count_bars){lastCountBars=c.count_bars;c.count_bars=0;}else c.count_bars=lastCountBars;}else if(n>=510&&n<=514)c.count_bars=n-510;else if(n>=600)c.level=n-600;else return;app.set_click_settings(c);if(c.count_bars)lastCountBars=c.count_bars;prefs.click=c;saveSettings();return;}
    if(n==6){app.pause();showProjectHome(true);home->rescan();return;}
    if(n>=1000){auto at=static_cast<std::size_t>(n-1000);if(at<prefs.recent_projects.size()){const auto f=juce::File(label(prefs.recent_projects[at]));confirmDiscard([this,f]{run([&]{openFile(f);});});}return;}
    if(n==1 || n==2){confirmDiscard([this,n]{run([&]{if(n==1){const bool session=app.audio_name()!="Offline clock (no sound)"&&app.audio_name()!="Disconnected";closeEditors();app.new_project(session?prefs.rate:48000);resetDevice();selectedClip.reset();selectedTrack.reset();refresh(true);showProjectHome(false);reconnectDevice(session);}else choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](const auto& f){openFile(f);},"*.mrsproject");});});}
    else if(n==3){if(app.path().empty())action(4);else saveFile(juce::File(juce::String(app.path().wstring().c_str())));}
    else if(n==4)choose(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[this](const auto& f){saveFile(f.withFileExtension("mrsproject"));},"*.mrsproject");
    else if(n==5)choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](const auto& f){importFiles({f.getFullPathName()});},"*.wav");
    else if(n==10 || n==11){closeEditors(true);if(n==10)app.undo();else app.redo();refresh(true);}
    else if(n==20)selectedTrack=app.add_audio_track("Audio "+std::to_string(project()->tracks.size()+1));
    else if(n==28)selectedTrack=app.add_instrument_track("Instrument "+std::to_string(project()->tracks.size()+1));
    else if(n==42)app.midi_panic();
    else if(n==21)selectedTrack=app.add_bus("Bus "+std::to_string(project()->tracks.size()+1));
    else if(n==22 && selectedTrack){auto id=*selectedTrack;for(const auto& t:project()->tracks)if(t.id==id)textDialog("Rename track",label(t.name),[this,id](auto s){app.rename_track(id,s.toStdString());});}
    else if(n==23 && selectedTrack){closeEditors();app.remove_track(*selectedTrack);selectedTrack.reset();selectedClip.reset();}
    else if((n==26 || n==27) && selectedTrack){const auto p=project();for(std::size_t i=0;i<p->tracks.size();++i)if(p->tracks[i].id==selectedTrack && ((n==26&&i>0)||(n==27&&i+1<p->tracks.size())))app.reorder_track(*selectedTrack,n==26?i-1:i+1);}
    else if(n==47){textDialog("Project tempo (BPM at project start)",juce::String(project()->time.tempos.front().bpm),[this](juce::String text){std::size_t used{};const auto value=std::stod(text.toStdString(),&used);if(used!=static_cast<std::size_t>(text.length()))throw std::runtime_error("Enter a valid BPM number");auto time=project()->time;time.tempos.front().bpm=value;app.set_time_map(std::move(time));});}
    else if(n==43 && selectedTrack){const mrs::Timeline t(project()->time,project()->sample_rate);selectedClip=app.create_midi_clip(*selectedTrack,t.to_ticks(app.engine()->state().sample));openMidiClip(*selectedClip);}
    else if(n==44 && selectedClip){openMidiClip(*selectedClip);}
    else if(n==45 && selectedClip){selectedClip=app.duplicate_clip(*selectedClip);}
    else if(n==46 && selectedClip){if(app.recording())throw std::runtime_error("Stop recording before changing loop");const auto p=project();const mrs::Timeline t(p->time,p->sample_rate);auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip){return clip.id==*selectedClip;});if(c!=p->clips.end())app.services().transport->set_loop(mrs::LoopRange{mrs::clip_start(*c,t),mrs::clip_end(*c,t)});}
    else if(n==24 && selectedClip){auto id=app.split_clip(*selectedClip,app.engine()->state().sample);activateClip(id);}
    else if(n==25 && selectedClip){std::vector<mrs::Id> clips;for(const auto& c:project()->clips)if(selectedClips.contains(c.id.value)||c.id==selectedClip)clips.push_back(c.id);app.remove_clips(std::move(clips));selectedClip.reset();selectedClips.clear();}
    else if(n==30)app.play();else if(n==31)app.pause();else if(n==32){if(app.recording())app.stop_recording();app.stop();}
    else if(n==33){if(app.recording()){app.stop_recording();app.stop();refresh(true);}else app.start_project_recording(path(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MR Studio").getChildFile("Projects")));}
    else if(n==34)app.musical().previous_section();else if(n==35)app.musical().next_section();
    else if(n==36){if(app.recording())throw std::runtime_error("Stop recording before changing loop");if(app.engine()->state().loop)app.musical().clear_loop();else if(app.musical().state().current_section)app.musical().loop_section(app.musical().state().current_section->id);}
    else if(n==40)audioSettings();
    else if(n==41)showProfiling();
});}
void Desktop::setWorkspace(mrs::desktop::Workspace w){app.workspace(w);prefs.workspace=w;if(dockedEditor&&dockedClip)dockedEditor->setVisible(w==mrs::desktop::Workspace::edit);resized();refresh();}
juce::String Desktop::projectCaption() const{return (app.path().empty()?label(project()->title):juce::String(app.path().stem().wstring().c_str()))+(app.dirty()?" *":"");}
void Desktop::resized(){if(homeVisible&&home){for(auto* child:getChildren())if(child!=home.get())child->setVisible(false);home->setBounds(getLocalBounds());home->toFront(false);return;}menu.setBounds(0,0,getWidth(),26);projectTitle.setBounds(300,0,juce::jmax(0,getWidth()-600),26);int x=8;
    for(auto* b:{&undo,&redo,&addTrack,&addBus,&split,&remove,&zoomIn,&zoomOut,&fit,&snapButton}){const int width=(b==&snapButton?86:78);b->setBounds(x,34,width,28);x+=width+5;}
    audio.setBounds(getWidth()-148,34,140,28);
    clickButton.setBounds(getWidth()-322,34,78,28);countButton.setBounds(getWidth()-239,34,86,28);
    browserWidth=juce::jlimit(200,juce::jmax(200,juce::jmin(700,getWidth()-700)),browserWidth);
    const int available=getWidth()-(sidebar?browserWidth:0)-16;
    const bool showMix=app.workspace()==mrs::desktop::Workspace::mix;
    const bool showEdit=app.workspace()==mrs::desktop::Workspace::edit&&dockedClip&&dockedEditor&&dockedEditor->isVisible();
    const int maxHeight=juce::jmax(260,getHeight()-268);
    const int baseHeight=juce::jlimit(260,juce::jmax(260,maxHeight-22),view.mixerHeight);
    int insertHeight=0;for(const auto& track:project()->tracks)insertHeight=juce::jmax(insertHeight,static_cast<int>(track.inserts.size())*22);
    insertHeight=juce::jmax(insertHeight,static_cast<int>(project()->master_inserts.size())*22);
    faderHeight=juce::jmax(40,baseHeight-220);
    const int mixerHeight=showEdit?juce::jmin(maxHeight,view.editorHeight):showMix ? juce::jmin(maxHeight,baseHeight+insertHeight) : 0;
    arrangeArea={8,70,available,getHeight()-128-mixerHeight};arrangement->setBounds(arrangeArea);
    mixArea={8,arrangeArea.getBottom()+6,available,mixerHeight-6};
    mixerDivider.setVisible(showMix||showEdit);mixerDivider.setBounds(8,arrangeArea.getBottom(),available,6);
    if(dockedEditor){dockedEditor->setVisible(showEdit);if(showEdit)dockedEditor->setBounds(mixArea);}
    mixerViewport.setVisible(showMix);if(master)master->setVisible(showMix);
    if(showMix){mixerViewport.setBounds(mixArea.withTrimmedRight(88));mixerBody.setSize(juce::jmax(mixerViewport.getWidth(),static_cast<int>(mixer.size())*88),mixArea.getHeight()-14);
        for(std::size_t i=0;i<mixer.size();++i){mixer[i]->setBounds(static_cast<int>(i)*88,0,86,mixerBody.getHeight());mixer[i]->resized();}
        if(master){master->setBounds(mixArea.getRight()-86,mixArea.getY(),86,mixArea.getHeight()-14);master->resized();}}
    browserWidth=juce::jlimit(200,juce::jmax(200,juce::jmin(700,getWidth()-700)),browserWidth);
    browserDivider.setVisible(sidebar);browserDivider.setBounds(getWidth()-browserWidth-8,70,8,getHeight()-128);
    browser->setVisible(sidebar);browser->setBounds(getWidth()-browserWidth,70,browserWidth-8,getHeight()-128);
    x=(getWidth()-(7*42+6*3))/2;for(auto* b:{&play,&pause,&stop,&record,&previous,&next,&loop}){b->setBounds(x,getHeight()-45,42,30);x+=45;}
    x=getWidth()-344;for(auto* b:{&arrangeButton,&editButton,&mixButton,&brows}){b->setBounds(x,getHeight()-45,78,30);x+=84;}
    cpuReadout.setBounds(8,getHeight()-44,88,20);audioCpuBar.setBounds(100,getHeight()-39,90,10);
    latencyReadout.setBounds(getWidth()-312,getHeight()-66,304,20);
}
void Desktop::paint(juce::Graphics& g){g.fillAll(juce::Colour(surface));g.setFont(theme.font(12));g.setColour(message.isEmpty()?juce::Colours::lightgrey:juce::Colours::orange);
    auto s=message.isEmpty()?(app.recording()?juce::String("RECORDING | ")+label(app.audio_name()):label(app.audio_name())):message;
    const auto head=displayState();if(head.count_remaining)s="COUNT-IN "+juce::String(static_cast<double>(head.count_remaining)/project()->sample_rate,1)+" s | "+label(app.audio_name());
    if(app.engine()->profile().enabled)s+=" | PROF";
    if(message.isEmpty()){const auto samples=app.engine()->compensation().output;s+="  |  PDC "+juce::String(static_cast<double>(samples)*1000/project()->sample_rate,2)+" ms";
        const auto metrics=app.engine()->metrics();const bool hardware=app.audio_name()!="Offline clock (no sound)"&&app.audio_name()!="Disconnected";
        if(hardware){s+="  |  B "+(metrics.callbacks?juce::String(metrics.min_frames)+(metrics.min_frames==metrics.max_frames?juce::String():"-"+juce::String(metrics.max_frames)):juce::String("--"));
            s+="  |  XR "+juce::String(static_cast<juce::int64>(metrics.input_underflows+metrics.input_overflows+metrics.output_underflows+metrics.output_overflows));
            s+="  |  Late "+juce::String(static_cast<juce::int64>(metrics.deadline_misses));
            s+="  |  D "+juce::String(static_cast<juce::int64>(metrics.disk_underruns));
            s+="  |  W "+juce::String(metrics.processing_workers);if(metrics.process_buffer_frames)s+="  |  A "+juce::String(metrics.anticipation_active?metrics.ahead_buffered_frames:0)+"/"+juce::String(metrics.process_buffer_frames)+(metrics.mixed_anticipation?" mixed":metrics.anticipation_active?juce::String():" direct")+" U"+juce::String(static_cast<juce::int64>(metrics.ahead_underruns));if(metrics.processing_fault)s+=" PROCESSING FAULT: reconnect audio";}}
    g.drawText(s,10,getHeight()-62,getWidth()-512,16,juce::Justification::left);
    if(sidebar){g.setColour(juce::Colour(0xff697580));g.fillRect(browserDivider.getX()+3,70,2,getHeight()-128);}
    if(mixerDivider.isVisible()){g.setColour(juce::Colour(0xff697580));g.fillRect(mixerDivider.getBounds().withHeight(1).translated(0,2));}
    g.setColour(juce::Colours::whitesmoke);g.drawText(juce::String(static_cast<double>(head.sample)/project()->sample_rate,2)+" s",(getWidth()-420)/2-86,getHeight()-44,80,28,juce::Justification::right);
}
void Desktop::mouseDown(const juce::MouseEvent& e){const auto local=e.getEventRelativeTo(this);resizingBrowser=sidebar&&browserDivider.getBounds().contains(local.getPosition());resizingMixer=mixerDivider.isVisible()&&mixerDivider.getBounds().contains(local.getPosition());mixerDragHeight=app.workspace()==mrs::desktop::Workspace::edit?view.editorHeight:view.mixerHeight;mixerDragY=local.y;}
void Desktop::resizeBrowser(int width){browserWidth=juce::jlimit(200,juce::jmax(200,juce::jmin(700,getWidth()-700)),width);resized();repaint();}
void Desktop::resizeMixer(int height){auto& panelHeight=app.workspace()==mrs::desktop::Workspace::edit?view.editorHeight:view.mixerHeight;const int minimum=app.workspace()==mrs::desktop::Workspace::edit?500:260;panelHeight=juce::jlimit(minimum,juce::jmax(minimum,getHeight()-290),height);resized();repaint();}
void Desktop::updatePerformance(const mrs::audio::DeviceStatus& status,bool hardware){
    const bool running=hardware&&status.phase==mrs::audio::DevicePhase::running;
    const bool measured=running&&std::isfinite(status.cpu_load)&&status.cpu_load>=0;
    audioCpu=measured?juce::jlimit(0.,1.,status.cpu_load):0.;
    cpuReadout.setTooltip("Driver callback wall time / deadline; excludes summed parallel worker CPU. Engine profiling is in the Transport menu.");
    cpuReadout.setText(measured?"CPU "+juce::String(status.cpu_load*100,1)+"%":"CPU --%",juce::dontSendNotification);
    audioCpuBar.setColour(juce::ProgressBar::foregroundColourId,!measured?juce::Colour(0xff62686e):status.cpu_load>=1?juce::Colour(0xffe64b54):status.cpu_load>=.8?juce::Colours::orange:juce::Colour(0xff45d899));
    auto latency=[&](double value){return running&&std::isfinite(value)&&value>=0?juce::String(value,2):juce::String("--");};
    auto text="Latency I "+latency(status.input_latency_ms)+" / O "+latency(status.output_latency_ms)+" ms";
    const auto metrics=app.engine()->metrics();
    if(metrics.monitoring_available)text+=" | Mon "+latency(status.input_latency_ms+status.output_latency_ms+static_cast<double>(metrics.monitoring_compensation_frames)*1000/project()->sample_rate);
    latencyReadout.setText(text,juce::dontSendNotification);
    latencyReadout.setTooltip("I/O: driver-reported ms. Mon: estimated input + output + fixed PDC; no Process Buffer hold on live input. Not a measured acoustic round trip. High-latency inserts remain active.");
}
void Desktop::mouseDrag(const juce::MouseEvent& e){if(resizingBrowser)resizeBrowser(getWidth()-e.getEventRelativeTo(this).x);if(resizingMixer)resizeMixer(mixerDragHeight+mixerDragY-e.getEventRelativeTo(this).y);}
void Desktop::mouseUp(const juce::MouseEvent&){if(resizingBrowser||resizingMixer)saveSettings();resizingBrowser=resizingMixer=false;}
void Desktop::refresh(bool force){if(!app.recording()){recordingPreview.clear();audioRecordingPreview.clear();}std::erase_if(selectedClips,[&](const auto& value){const auto p=project();return std::none_of(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id.value==value;});});const auto p=project();const auto state=app.services().projects->state();
    std::vector<mrs::Id> current;for(const auto& t:p->tracks)current.push_back(t.id);
    const bool replaced=displayedStore!=app.services().projects;
    if(replaced){displayedStore=app.services().projects;visualTransport.reset();revision=~0ULL;peaks={};masterPeak={};arrangement->horizontal=0;arrangement->vertical=0;mixerViewport.setViewPosition(0,0);}
    if(replaced||current!=ids){closeEditors();cancelPreview();ids=current;mixer.clear();for(const auto& t:p->tracks){auto s=std::make_unique<Strip>(*this,t.id);mixerBody.addAndMakeVisible(*s);mixer.push_back(std::move(s));}
        master=std::make_unique<Strip>(*this,std::nullopt);addAndMakeVisible(*master);arrangement->rebuild();resized();}
    if(force || revision!=state.revision){
        mrs::diagnostics::event("project_snapshot title="+p->title+" tracks="+std::to_string(p->tracks.size())+" clips="+std::to_string(p->clips.size())+" revision="+std::to_string(state.revision)+" insert_generation="+std::to_string(app.insert_generation()));
        for(const auto& t:p->tracks)for(const auto& fx:t.inserts)if(fx.kind==mrs::InsertKind::vst3)mrs::diagnostics::event("plugin track="+t.name+" name="+fx.plugin_name+" path="+fx.plugin_path+" class="+fx.class_id);
        for(const auto& fx:p->master_inserts)if(fx.kind==mrs::InsertKind::vst3)mrs::diagnostics::event("plugin master name="+fx.plugin_name+" path="+fx.plugin_path+" class="+fx.class_id);
        app.prepare_waveforms();for(auto& s:mixer)s->sync();for(auto& s:arrangement->rows)s->sync();if(master)master->sync();revision=state.revision;resized();}
    projectTitle.setText(projectCaption(),juce::dontSendNotification);
    undo.setEnabled(state.can_undo);redo.setEnabled(state.can_redo);
    arrangeButton.setToggleState(app.workspace()==mrs::desktop::Workspace::arrange,juce::dontSendNotification);
    editButton.setToggleState(clipEditorOpen(),juce::dontSendNotification);
    mixButton.setToggleState(app.workspace()==mrs::desktop::Workspace::mix,juce::dontSendNotification);
    record.setToggleState(app.recording(),juce::dontSendNotification);loop.setToggleState(displayState().loop.has_value(),juce::dontSendNotification);
    const auto click=app.click_settings();clickButton.setToggleState(click.playback||click.recording,juce::dontSendNotification);clickButton.setEnabled(!app.recording());countButton.setEnabled(!app.recording());countButton.setButtonText(click.count_bars?"Count "+juce::String(click.count_bars):"Count off");countButton.setToggleState(click.count_bars>0,juce::dontSendNotification);
    brows.setToggleState(sidebar,juce::dontSendNotification);repaint();arrangement->repaint();
}
bool Desktop::busyGesture() const{if(resizingMixer||resizingBrowser)return true;if(preview&&preview->active())return true;return false;}
void Desktop::setPreview(std::optional<mrs::Id> t,mrs::Track::Mix m,float gain,std::function<bool()> active){preview=MixPreview{t,m,gain,std::move(active)};app.preview_mix(t,m,gain);}
void Desktop::cancelPreview(){preview.reset();app.cancel_mix_preview();}
void Desktop::finishPreview(){preview.reset();}
bool Desktop::shortcutAllowed() const {return shortcutAllowedFor(juce::Component::getCurrentlyFocusedComponent());}
bool Desktop::shortcutAllowedFor(juce::Component* focused) const {
    if(juce::Component::getCurrentlyModalComponent()!=nullptr)return false;
    if(focused && focused!=this && !isParentOf(focused))return false;
    for(auto* c=focused;c&&c!=this;c=c->getParentComponent())if(dynamic_cast<juce::TextEditor*>(c)||dynamic_cast<juce::ComboBox*>(c))return false;
    return true;
}
bool Desktop::workspaceShortcut(const juce::KeyPress& k){if(homeVisible||k.getModifiers().isAnyModifierKeyDown()||juce::Component::getCurrentlyModalComponent())return false;
    if(k.getKeyCode()==juce::KeyPress::F2Key){run([&]{toggleEditor();});return true;}if(k.getKeyCode()==juce::KeyPress::F3Key){const bool open=app.workspace()!=mrs::desktop::Workspace::mix;if(open&&clipEditorOpen())toggleEditor();setWorkspace(open?mrs::desktop::Workspace::mix:mrs::desktop::Workspace::arrange);return true;}if(k.getKeyCode()==juce::KeyPress::F5Key){sidebar=!sidebar;resized();repaint();return true;}return false;}
bool Desktop::keyPressed(const juce::KeyPress& k){if(workspaceShortcut(k))return true;if(!shortcutAllowed())return false;
    if(homeVisible){if(k.getModifiers().isCtrlDown()&&k.getKeyCode()=='O'){action(2);return true;}if(k.getModifiers().isCtrlDown()&&k.getKeyCode()=='N'){action(1);return true;}return false;}
    auto c=k.getKeyCode();if(k.getModifiers().isAltDown())return false;if(k.getModifiers().isCtrlDown()){if(c=='Z'){action(10);return true;}if(c=='Y'){action(11);return true;}if(c=='S'){action(3);return true;}if(c=='O'){action(2);return true;}if(c=='N'){action(1);return true;}return false;}
    if(k.getModifiers().isShiftDown())return false;
    if(c=='S'){action(24);return true;}if(c=='R'){action(33);return true;}if(c==juce::KeyPress::backspaceKey||c==juce::KeyPress::deleteKey){action(25);return true;}return c==juce::KeyPress::spaceKey;
}
bool Desktop::keyStateChanged(bool){if(homeVisible||!shortcutAllowed()){spaceHeld=false;return false;}
    const bool down=juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey);spaceKey(down);return down;}
void Desktop::spaceKey(bool down){if(down&&!spaceHeld)run([&]{action(app.engine()->state().playback==mrs::PlaybackState::playing?32:30);});spaceHeld=down;}
void Desktop::updateRecordingPreview(){
    recordingPreview=app.midi_recording_preview();auto nextWaveforms=app.audio_recording_preview();
    for(auto& takePreview:nextWaveforms)if(!takePreview.waveform){const auto old=std::find_if(audioRecordingPreview.begin(),audioRecordingPreview.end(),[&](const auto& item){return item.clip.id==takePreview.clip.id;});if(old!=audioRecordingPreview.end())takePreview.waveform=old->waveform;}
    audioRecordingPreview=std::move(nextWaveforms);
}
void Desktop::focusLost(FocusChangeType){spaceHeld=false;}
void Desktop::timerCallback(){try{const bool wasRecording=app.recording();app.poll();if(wasRecording&&!app.recording()&&!app.recording_error().empty())message=label(app.recording_error());if(++previewTick%3==0)updateRecordingPreview();if(!app.recording()){recordingPreview.clear();audioRecordingPreview.clear();}if(dockedEditor&&!dockedClip)dockedEditor.reset();if(preview){if(preview->active())app.preview_mix(preview->target,preview->mix,preview->master);else preview.reset();}
    updatePerformance(app.device_status(),app.audio_name()!="Offline clock (no sound)"&&app.audio_name()!="Disconnected");
    if(++diagnosticTick%150==0){mrs::audio::RealtimeState head;if(app.engine()->try_state(head))mrs::diagnostics::event("heartbeat device="+app.audio_name()+" rate="+std::to_string(project()->sample_rate)+" requested_buffer="+std::to_string(prefs.buffer)+" workers="+std::to_string(prefs.processing_workers)+" process="+std::to_string(prefs.process_buffer_frames)+" playback="+std::to_string(static_cast<int>(head.playback))+" sample="+std::to_string(head.sample)+" recording="+std::to_string(app.recording()));}
    if(editorGeneration!=app.insert_generation()){closeEditors(true);editorGeneration=app.insert_generation();}
    std::erase_if(windows,[](const auto& w){return !w->isVisible();});
    if(scanner.valid()&&scanner.wait_for(std::chrono::seconds(0))==std::future_status::ready){catalog=scanner.get();browser->rebuild();}
    const auto meter=app.engine()->take_meters();for(std::size_t i=0;i<peaks.size();++i){peaks[i].left=juce::jmax(meter.tracks[i].left,peaks[i].left*.86f);peaks[i].right=juce::jmax(meter.tracks[i].right,peaks[i].right*.86f);}
    masterPeak={juce::jmax(meter.master.left,masterPeak.left*.86f),juce::jmax(meter.master.right,masterPeak.right*.86f)};
    for(auto& s:mixer)s->repaint();for(auto& s:arrangement->rows){s->refreshMidiStatus();s->repaint();}if(master)master->repaint();
    refresh();if(homeVisible&&home)home->repaint();
}catch(const std::exception& e){message=label(e.what());repaint();}}

Strip::Strip(Desktop& d,std::optional<mrs::Id> id,bool small):gain(!small),target(id),owner(d),mini(small){
    pan.rotary=mini;gain.setTitle("Channel gain");pan.setTitle("Channel pan");
    gain.setDescription("Arrow keys adjust gain; Ctrl for fine adjustment");pan.setDescription("Arrow keys adjust pan; Ctrl for fine adjustment");
    mute.setTitle("Mute");solo.setTitle("Solo");arm.setTitle("Arm recording");monitor.setTitle("Input monitoring");
    for(auto* c:{static_cast<juce::Component*>(&gain),static_cast<juce::Component*>(&pan)})addAndMakeVisible(c);
    for(auto* b:{&mute,&solo,&arm,&monitor,&input,&inserts,&output,&sends,&add}){b->getProperties().set("channelFont",true);addAndMakeVisible(b);}
    mute.onClick=[this]{owner.run([&]{auto m=track().mix;m.mute=!m.mute;owner.app.set_track_mix(*target,m);});};
    solo.onClick=[this]{owner.run([&]{auto m=track().mix;m.solo=!m.solo;owner.app.set_track_mix(*target,m);});};
    arm.onClick=[this]{owner.run([&]{owner.app.set_track_armed(*target,!owner.app.track_armed(*target));sync();});};
    monitor.onClick=[this]{owner.run([&]{const auto t=track();if(t.kind==mrs::TrackKind::instrument)owner.app.set_midi_input(*target,t.midi_input,t.midi_channel,!t.midi_monitor);else owner.app.set_track_monitoring(*target,!t.input_monitor);});};
    input.onClick=[this]{inputMenu();};inserts.onClick=[this]{owner.insertMenu(target);};add.onClick=inserts.onClick;
    output.onClick=[this]{if(target)owner.routeMenu(*target);else owner.textDialog("Master hardware outputs (1,2...)","1,2",[this](auto s){owner.app.set_hardware_output({},mrs::desktop::parse_outputs(s.toStdString()));});};
    sends.onClick=[this]{if(target)owner.sendsMenu(*target);};
    gain.preview=[this](double v){preview(true,v);};pan.preview=[this](double v){preview(false,v);};
    gain.commit=[this](double v){commit(true,v);};pan.commit=[this](double v){commit(false,v);};
    addAndMakeVisible(insertViewport);insertViewport.setViewedComponent(&insertBody,false);insertViewport.setScrollBarsShown(true,false);insertViewport.setVisible(!mini);
    gain.cancel=pan.cancel=[this]{owner.cancelPreview();sync();};sync();
}
mrs::Track Strip::track() const{if(target)for(const auto& t:owner.project()->tracks)if(t.id==target)return t;return {};}
void Strip::mouseDown(const juce::MouseEvent& e){owner.selectedTrack=target;dragTitle=target&&e.mods.isLeftButtonDown()&&e.y<26;owner.refresh();}
void Strip::mouseDrag(const juce::MouseEvent& e){if(dragTitle&&target&&e.getDistanceFromDragStart()>5&&!owner.isDragAndDropActive())owner.startDragging("mrs-track:"+label(target->value),this);}
mrs::Track::Mix Strip::adjusted(bool volume,double v) const{auto m=track().mix;if(volume)m.gain=static_cast<float>(std::pow(10.,(-60+72*v)/20));else m.pan=static_cast<float>(2*v-1);return m;}
void Strip::preview(bool volume,double v){owner.setPreview(target,adjusted(volume,v),target?owner.project()->master_gain:adjusted(volume,v).gain,[this]{return active();});}
void Strip::commit(bool volume,double v){owner.run([&]{owner.finishPreview();if(target)owner.app.set_track_mix(*target,adjusted(volume,v));else owner.app.set_master_gain(adjusted(volume,v).gain);owner.app.poll();owner.refresh(true);});}
void Strip::refreshMidiStatus(){const auto t=track();if(t.kind==mrs::TrackKind::instrument){const auto status=label(owner.app.midi_status(t.id));input.setButtonText(status);input.setTooltip(status+" — click to select MIDI port / channel");}}
void Strip::sync(){const auto t=track();gain.setTitle(label(t.name)+" gain");pan.setTitle(label(t.name)+" pan");const auto mix=t.mix;float v=target?mix.gain:owner.project()->master_gain;
    if(!gain.dragging())gain.value=juce::jlimit(0.,1.,(20*std::log10(juce::jmax(.001f,v))+60)/72.);if(!pan.dragging())pan.value=(mix.pan+1)/2.;
    mute.setVisible(target.has_value());solo.setVisible(target.has_value());pan.setVisible(target.has_value());
    arm.setVisible(mini&&target&&(t.kind==mrs::TrackKind::audio||t.kind==mrs::TrackKind::instrument));monitor.setVisible(arm.isVisible()||(mini&&target&&t.kind==mrs::TrackKind::instrument));input.setVisible(monitor.isVisible());
    inserts.setVisible(!mini);add.setVisible(!mini);output.setVisible(!mini);sends.setVisible(!mini&&target.has_value());
    mute.setToggleState(mix.mute,juce::dontSendNotification);solo.setToggleState(mix.solo,juce::dontSendNotification);
    arm.setToggleState(target&&owner.app.track_armed(*target),juce::dontSendNotification);monitor.setToggleState(t.kind==mrs::TrackKind::instrument?t.midi_monitor:t.input_monitor,juce::dontSendNotification);
    input.setButtonText(t.input==-2?"Input: default":t.input==-1?"Input: off":juce::String(t.input_stereo?"Stereo ":"Mono ")+juce::String(t.input+1));
    input.setComponentID(t.kind==mrs::TrackKind::instrument?"midi-input-status":"");
    if(t.kind==mrs::TrackKind::instrument){input.setButtonText(label(owner.app.midi_status(t.id)));input.setTooltip("MIDI input / channel; choose a VST3 instrument as the first insert in Mix");}
    juce::String out="Out: Master";if(t.output)for(const auto& x:owner.project()->tracks)if(x.id==t.output)out="Out: "+label(x.name);
    if(!t.hardware_outputs.empty())out="Out: hardware";output.setButtonText(target?out:"Out: Default");
    sends.setButtonText("Sends ("+juce::String(static_cast<int>(t.sends.size()))+")");insertList();resized();repaint();gain.repaint();pan.repaint();
}
void Strip::insertList(){const auto chain=owner.chain(target);insertButtons.clear();if(mini)return;
    inserts.setButtonText("Inserts ("+juce::String(static_cast<int>(chain.size()))+")");
    for(const auto& n:chain){auto b=std::make_unique<juce::TextButton>((n.bypass?"[B] ":"")+insertName(n));const auto slot=n.id;
        b->getProperties().set("channelFont",true);b->onClick=[this,slot]{owner.openInsert(target,slot);};insertBody.addAndMakeVisible(*b);insertButtons.push_back(std::move(b));}
}
void Strip::resized(){const int w=getWidth(),h=getHeight();if(mini){mute.setBounds(w-132,4,28,24);solo.setBounds(w-102,4,28,24);arm.setBounds(w-72,4,28,24);monitor.setBounds(w-42,4,28,24);
        gain.setBounds(8,32,w-70,36);pan.setBounds(w-64,34,58,32);input.setBounds(8,h-30,w-18,24);}
    else {inserts.setBounds(6,27,w-36,20);add.setBounds(w-26,27,20,20);
        const int top=h-108-owner.faderHeight;
        gain.setBounds(37,top,34,owner.faderHeight);pan.setBounds(6,top-27,w-12,23);
        insertViewport.setBounds(6,50,w-12,juce::jmax(0,top-79));
        const int insertWidth=w-12-(static_cast<int>(insertButtons.size())*20>insertViewport.getHeight()?insertViewport.getScrollBarThickness():0);
        insertBody.setSize(insertWidth,static_cast<int>(insertButtons.size())*20);
        for(std::size_t i=0;i<insertButtons.size();++i)insertButtons[i]->setBounds(0,static_cast<int>(i)*20,insertWidth,18);
        mute.setBounds(6,h-82,31,21);solo.setBounds(40,h-82,31,21);output.setBounds(6,h-56,w-12,21);sends.setBounds(6,h-30,w-12,21);}
}
void Strip::paint(juce::Graphics& g){const bool selected=target&&owner.selectedTrack==target;
    const auto base=mini?juce::Colour(panel):juce::Colour(selected?0xff373e44:0xff2d3236);
    g.fillAll(base);g.setColour(juce::Colour(accent));g.fillRect(0,0,mini?4:getWidth(),mini?getHeight():selected?3:2);
    if(!mini){g.setColour(juce::Colour(selected?0xff414a52:0xff353b40));g.fillRect(0,3,getWidth(),21);
        g.setColour(juce::Colour(0xff1b2024));g.fillRect(0,24,getWidth(),1);
        g.fillRect(6,getHeight()-88,getWidth()-12,1);}
    if(trackHover){g.setColour(juce::Colours::lightskyblue);if(mini)g.fillRect(0,dropBefore?0:getHeight()-3,getWidth(),3);else g.fillRect(dropBefore?0:getWidth()-3,0,3,getHeight());}
    const auto t=track();g.setFont(ui::Theme::channelFont());g.setColour(target?juce::Colours::whitesmoke:juce::Colours::gold);
    g.drawText(target?label(t.name):"MASTER",mini?8:6,3,mini?getWidth()-145:getWidth()-12,21,juce::Justification::left);
    mrs::audio::StereoPeak p=owner.masterPeak;if(target){const auto tracks=owner.project()->tracks;for(std::size_t i=0;i<tracks.size();++i)if(tracks[i].id==target)p=owner.peaks[i];}
    for(int i=0;i<2;++i){const float peak=i==0?p.left:p.right;const auto level=juce::jlimit(0.f,1.f,(20.f*std::log10(juce::jmax(.000001f,peak))+60.f)/60.f);
        const juce::Rectangle<int> r=mini?juce::Rectangle<int>(10,70+i*7,getWidth()-82,5):juce::Rectangle<int>(8+i*12,gain.getY(),8,gain.getHeight());
        g.setColour(juce::Colour(0xff111518));g.fillRect(r);g.setColour(peak>.95f?juce::Colours::orange:juce::Colour(0xff38cbbf));
        if(mini)g.fillRect(r.withWidth(static_cast<int>(r.getWidth()*level)));else g.fillRect(r.withTop(r.getBottom()-static_cast<int>(r.getHeight()*level)));
    }
    g.setFont(ui::Theme::channelFont());g.setColour(juce::Colours::whitesmoke);g.drawText(juce::String(-60+gain.value*72,1)+" dB",mini?10:6,mini?82:getHeight()-107,mini?100:getWidth()-12,18,juce::Justification::left);
}
void Strip::inputMenu(){if(!target)return;
    if(track().kind==mrs::TrackKind::instrument){
        const auto current=track();const auto ports=mrs::desktop::MidiInputs::ports();juce::PopupMenu midi;
        midi.addItem(1,"MIDI input: off",true,current.midi_input.empty());
        for(std::size_t i=0;i<ports.size();++i)midi.addItem(10+static_cast<int>(i),label(ports[i].name),true,current.midi_input==ports[i].id);
        juce::PopupMenu channels;channels.addItem(1000,"All channels",true,current.midi_channel<0);
        for(int ch=0;ch<16;++ch)channels.addItem(1001+ch,"Channel "+juce::String(ch+1),true,current.midi_channel==ch);
        midi.addSubMenu("Input channel",channels);midi.addSeparator();midi.addItem(2,label(owner.app.midi_status(current.id)),false);
        midi.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&input),[safe=juce::Component::SafePointer<Strip>(this),ports,current](int selected){if(!safe||!selected||selected==2)return;safe->owner.run([&]{auto port=current.midi_input;auto channel=current.midi_channel;if(selected==1)port.clear();else if(selected>=1000)channel=selected==1000?-1:selected-1001;else if(selected>=10&&static_cast<std::size_t>(selected-10)<ports.size())port=ports[static_cast<std::size_t>(selected-10)].id;safe->owner.app.set_midi_input(current.id,port,channel,current.midi_monitor);});});return;
    }
juce::PopupMenu m;m.addItem(1,"Default");m.addItem(2,"Off");const auto names=owner.app.input_names();
    for(std::size_t i=0;i<names.size();++i){m.addItem(10+static_cast<int>(i),"Mono "+label(names[i]));if(i+1<names.size())m.addItem(100+static_cast<int>(i),"Stereo "+label(names[i])+" / "+label(names[i+1]));}
    juce::Component::SafePointer<Strip> safe(this);m.showMenuAsync(popup(&input),[safe](int n){if(safe&&n)safe->owner.run([&]{safe->owner.app.set_track_input(*safe->target,n==1?-2:n==2?-1:n>=100?n-100:n-10,n>=100);safe->sync();});});
}
bool Strip::isInterestedInDragSource(const SourceDetails& s){const auto text=s.description.toString();if(text.startsWith("mrs-track:"))return target.has_value();const auto number=text.substring(5);return text.startsWith("vst3:")&&!mini&&!number.isEmpty()&&number.containsOnly("0123456789");}
void Strip::itemDragMove(const SourceDetails& s){trackHover=target&&s.description.toString().startsWith("mrs-track:");dropBefore=mini?s.localPosition.y<getHeight()/2:s.localPosition.x<getWidth()/2;repaint();}
void Strip::itemDragExit(const SourceDetails&){trackHover=false;repaint();}
void Strip::itemDropped(const SourceDetails& s){trackHover=false;repaint();if(!isInterestedInDragSource(s))return;const auto text=s.description.toString();if(text.startsWith("mrs-track:")){const auto p=owner.project();for(std::size_t i=0;i<p->tracks.size();++i)if(p->tracks[i].id==target){const bool before=mini?s.localPosition.y<getHeight()/2:s.localPosition.x<getWidth()/2;owner.dropTrack(text,i+(before?0:1));break;}}else owner.addPlugin(target,static_cast<std::size_t>(text.substring(5).getIntValue()));}

void Desktop::dropTrack(const juce::String& description,std::size_t boundary){const auto p=project();const mrs::Id id{description.substring(10).toStdString()};const auto found=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==id;});if(found==p->tracks.end())return;
    const auto source=static_cast<std::size_t>(found-p->tracks.begin());boundary=std::min(boundary,p->tracks.size());const auto index=boundary-(source<boundary?1:0);if(index==source)return;
    // Defer until JUCE has released source/target pointers; refresh rebuilds both views.
    juce::Component::SafePointer<Desktop> safe(this);juce::MessageManager::callAsync([safe,id,index]{if(safe)safe->run([&]{safe->app.reorder_track(id,index);safe->selectedTrack=id;});});
}
bool Desktop::isInterestedInDragSource(const SourceDetails& s){return s.description.toString().startsWith("mrs-track:")&&mixArea.contains(s.localPosition);}
void Desktop::itemDropped(const SourceDetails& s){if(isInterestedInDragSource(s))dropTrack(s.description.toString(),project()->tracks.size());}

Arrangement::Arrangement(Desktop& d):owner(d),playhead([this]{const float x=static_cast<float>(left+owner.visualSample()/owner.project()->sample_rate*pixelsPerSecond-horizontal);return x>=left&&x<getWidth()?juce::Rectangle<float>{x,0.f,1.f,static_cast<float>(getHeight())}:juce::Rectangle<float>{};}){setWantsKeyboardFocus(true);addAndMakeVisible(rowsBody);rowsBody.setInterceptsMouseClicks(false,true);addAndMakeVisible(playhead);}
void Arrangement::rebuild(){rows.clear();for(const auto& t:owner.project()->tracks){auto row=std::make_unique<Strip>(owner,t.id,true);rowsBody.addAndMakeVisible(*row);rows.push_back(std::move(row));}resized();}
void Arrangement::resized(){vertical=juce::jlimit(0.f,static_cast<float>(juce::jmax(0,static_cast<int>(rows.size())*trackHeight-(getHeight()-header))),vertical);
    rowsBody.setBounds(0,header,left-6,getHeight()-header);
    for(std::size_t i=0;i<rows.size();++i){const int y=static_cast<int>(i)*trackHeight-static_cast<int>(vertical);rows[i]->setBounds(0,y,left-6,trackHeight-4);rows[i]->setVisible(y+trackHeight>0 && y<rowsBody.getHeight());}playhead.update();}
int Arrangement::trackAt(float y) const{return static_cast<int>((y-header+vertical)/trackHeight);}
namespace {
juce::MouseCursor bracketCursor(bool right){
    static const auto create=[](bool reverse){juce::Image image(juce::Image::ARGB,24,24,true);juce::Graphics g(image);juce::Path p;
        const float stem=reverse?14.f:10.f,arm=reverse?8.f:16.f;p.startNewSubPath(arm,4);p.lineTo(stem,4);p.lineTo(stem,20);p.lineTo(arm,20);
        g.setColour(juce::Colours::black);g.strokePath(p,juce::PathStrokeType(4.f));g.setColour(juce::Colours::white);g.strokePath(p,juce::PathStrokeType(2.f));return juce::MouseCursor(image,static_cast<int>(stem),12);};
    static const auto left=create(false),rightCursor=create(true);return right?rightCursor:left;
}
}
int Arrangement::edgeAt(const mrs::Clip& c,juce::Point<float> point) const{
    const auto r=clipRect(c);if(!r.contains(point))return 0;
    const auto l=point.x-r.getX(),right=r.getRight()-point.x;
    return std::min(l,right)<7.f?(l<=right?-1:1):0;
}
std::optional<mrs::Id> Arrangement::clipAt(juce::Point<float> point) const{
    if(point.x<left||point.y<header)return {};
    const auto p=owner.project();
    // Match paint order: selected clips on top, then reverse insertion order.
    for(bool selected:{true,false})for(auto i=p->clips.rbegin();i!=p->clips.rend();++i){const bool active=i->id==owner.selectedClip||owner.selectedClips.contains(i->id.value);if(active==selected&&clipRect(*i).contains(point))return i->id;}
    return {};
}
void Arrangement::mouseMove(const juce::MouseEvent& e){
    if(owner.app.recording()){setMouseCursor(juce::MouseCursor::NormalCursor);return;}
    if(const auto id=clipAt(e.position)){const auto p=owner.project();const auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip){return clip.id==*id;});const auto edge=edgeAt(*c,e.position);setMouseCursor(edge?bracketCursor(edge>0):juce::MouseCursor(juce::MouseCursor::NormalCursor));}
    else setMouseCursor(juce::MouseCursor::NormalCursor);
}
void Arrangement::mouseExit(const juce::MouseEvent&){if(!drag)setMouseCursor(juce::MouseCursor::NormalCursor);}
void Arrangement::updateTrim(mrs::Sample delta){
    const auto p=owner.project();const mrs::Timeline time(p->time,p->sample_rate);
    if(drag->midi){const auto& m=*drag->midi;auto tick=time.to_ticks(std::max(mrs::Sample{0},(trim<0?drag->start:drag->start+drag->length)+delta));
        if(owner.snap)tick=((tick+mrs::ppq/8)/(mrs::ppq/4))*(mrs::ppq/4);
        if(trim<0){mrs::Tick prefix=mrs::max_tick;for(const auto& n:m.notes)prefix=std::min(prefix,mrs::max_tick-n.start-n.length);for(const auto& e:m.events)prefix=std::min(prefix,mrs::max_tick-e.start);tick=std::clamp(tick,std::max(mrs::Tick{0},m.start-m.source_offset-prefix),m.start+m.length-1);}
        else tick=std::clamp(tick,m.start+1,std::min(mrs::max_tick,m.start+mrs::max_tick-m.source_offset));
        trimStart=trim<0?time.to_samples(tick):drag->start;trimEnd=trim>0?time.to_samples(tick):drag->start+drag->length;
    }else{
        auto sample=std::max(mrs::Sample{0},(trim<0?drag->start:drag->start+drag->length)+delta);
        if(owner.snap)sample=mrs::snap_to_grid(*p,sample,mrs::ppq/4);
        trimStart=trim<0?std::clamp(sample,std::max(mrs::Sample{0},drag->start-drag->source_offset),drag->start+drag->length-1):drag->start;
        trimEnd=trim>0?std::clamp(sample,drag->start+1,std::min(mrs::max_sample,drag->start+dragSourceFrames-drag->source_offset)):drag->start+drag->length;
    }
}
mrs::Sample Arrangement::sampleAt(float x) const{auto sample=static_cast<mrs::Sample>(juce::jmax(0.,(x-left+horizontal)/pixelsPerSecond)*owner.project()->sample_rate);
    if(owner.snap){mrs::Timeline time(owner.project()->time,owner.project()->sample_rate);auto tick=time.to_ticks(sample);sample=time.to_samples((tick/(mrs::ppq/4))*(mrs::ppq/4));}return sample;}
mrs::Sample Arrangement::seekSampleAt(float x) const{
    const auto p=owner.project();const mrs::Timeline time(p->time,p->sample_rate);
    const auto raw=static_cast<mrs::Sample>(std::llround(juce::jmax(0.,(x-left+horizontal)/pixelsPerSecond)*p->sample_rate));
    const auto tick=time.to_ticks(raw),bar=time.musical_position(tick).bar;
    // Capture either side of a bar's painted pixel, independent of Snap.
    // The nearest bar wins even when zoom makes capture areas overlap.
    double distance=6.001;std::optional<mrs::Sample> captured;
    for(auto b=juce::jmax<std::int64_t>(1,bar-1);b<=bar+1;++b){
        const auto sample=time.to_samples(time.to_ticks(mrs::MusicalPosition{b,1,0}));
        const int pixel=left+static_cast<int>(static_cast<double>(sample)/p->sample_rate*pixelsPerSecond-horizontal);
        const double d=std::abs(x-pixel);if(d<=6.&&d<distance){distance=d;captured=sample;}
    }
    if(captured)return *captured;
    if(!owner.snap)return raw;
    const auto grid=mrs::ppq/4,lower=(tick/grid)*grid;
    const auto a=time.to_samples(lower),b=time.to_samples(std::min(mrs::max_tick,lower+grid));
    return raw-a<=b-raw?a:b;
}
juce::Rectangle<float> Arrangement::clipRect(const mrs::Clip& c) const{int index=0;for(const auto& t:owner.project()->tracks){if(t.id==c.track)break;++index;}
    const auto p=owner.project();const mrs::Timeline t(p->time,p->sample_rate);const double rate=p->sample_rate;const auto start=mrs::clip_start(c,t),end=mrs::clip_end(c,t);return {static_cast<float>(left+start/rate*pixelsPerSecond-horizontal),static_cast<float>(header+index*trackHeight-vertical+22),juce::jmax(2.f,static_cast<float>((end-start)/rate*pixelsPerSecond)),static_cast<float>(trackHeight-27)};}
void Arrangement::paint(juce::Graphics& g){g.fillAll(juce::Colour(0xff373b3f));g.setColour(juce::Colour(surface));g.fillRect(0,0,left,getHeight());g.setFont(owner.theme.font(13));g.setColour(juce::Colours::lightgrey);
    const auto p=owner.project();mrs::Timeline time(p->time,p->sample_rate);
    g.drawText("Tracks / input / monitor",8,5,left-10,22,juce::Justification::left);
    g.setFont(ui::Theme::channelFont());
    mrs::Tick grid=mrs::ppq/4;double fastest=0;for(const auto& tempo:p->time.tempos)fastest=juce::jmax(fastest,tempo.bpm);
    while(grid<mrs::max_tick/2&&grid*60.*pixelsPerSecond/(fastest*mrs::ppq)<14)grid*=2;
    const auto first=time.to_ticks(sampleAt(static_cast<float>(left)));const auto last=time.to_ticks(sampleAt(static_cast<float>(getWidth())));int lastLabel=left-70;
    for(auto tick=(first/grid)*grid;tick<=last&&tick<=mrs::max_tick-grid;tick+=grid){const int x=left+static_cast<int>(static_cast<double>(time.to_samples(tick))/p->sample_rate*pixelsPerSecond-horizontal);if(x<left)continue;const auto pos=time.musical_position(tick);g.setColour(juce::Colour(pos.tick==0?0xff4a5259:0xff40454a));g.drawVerticalLine(x,20,static_cast<float>(getHeight()));if(pos.tick==0&&x-lastLabel>=60){g.setColour(juce::Colours::lightgrey);g.drawText(juce::String(pos.bar)+":"+juce::String(pos.beat),x+2,0,58,20,juce::Justification::left);lastLabel=x;}}
    g.setFont(owner.theme.font(13));
    for(const auto& section:p->sections){const float x=static_cast<float>(left+static_cast<double>(time.to_samples(section.start))/p->sample_rate*pixelsPerSecond-horizontal);const float width=static_cast<float>(static_cast<double>(time.to_samples(section.end)-time.to_samples(section.start))/p->sample_rate*pixelsPerSecond);
        g.setColour(juce::Colour(section.color).withAlpha(1.f));g.fillRect(juce::Rectangle<float>(x,22,width,22));g.setColour(juce::Colours::white);g.drawText(label(section.name),static_cast<int>(x)+4,22,static_cast<int>(width)-4,22,juce::Justification::left);}
    for(const auto& chord:p->chords){int x=left+static_cast<int>(static_cast<double>(time.to_samples(chord.start))/p->sample_rate*pixelsPerSecond-horizontal);g.setColour(juce::Colours::lightgrey);g.drawText(label(chord.symbol),x,46,80,20,juce::Justification::left);}
    g.saveState();g.reduceClipRegion(left,header,getWidth()-left,getHeight()-header);
    for(std::size_t i=0;i<p->tracks.size();++i){int y=header+static_cast<int>(i)*trackHeight-static_cast<int>(vertical);g.setColour(juce::Colour(0xff465058));g.drawHorizontalLine(y,static_cast<float>(left),static_cast<float>(getWidth()));g.setColour(juce::Colours::lightgrey);g.drawText(label(p->tracks[i].name),left+8,y+2,200,20,juce::Justification::left);}
    // Paint selected clips last; both audio and MIDI reveal overlapping clips.
    auto paintedClips=p->clips;std::stable_partition(paintedClips.begin(),paintedClips.end(),[&](const auto& c){return c.id!=owner.selectedClip&&!owner.selectedClips.contains(c.id.value);});
    for(const auto& stored:paintedClips){auto c=stored;
        if(drag&&(drag->id==c.id||(!trim&&std::any_of(dragGroup.begin(),dragGroup.end(),[&](const auto& item){return item.id==c.id;})))){
            if(trim){
                if(c.midi)mrs::trim_midi_source(*c.midi,time.to_ticks(trimStart),time.to_ticks(trimEnd));
                else{c.source_offset+=trimStart-c.start;c.start=trimStart;c.length=trimEnd-trimStart;}
            }else{c.start=mrs::clip_start(stored,time);c.length=mrs::clip_end(stored,time)-c.start;c.midi.reset();c.start+=dragStart-drag->start;const auto at=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==c.track;});c.track=p->tracks[static_cast<std::size_t>(static_cast<int>(at-p->tracks.begin())+dragTrackDelta)].id;}
        }
        const auto midi=c.midi?c.midi:stored.midi;
        auto r=clipRect(c);if(!r.intersects(getLocalBounds().toFloat()))continue;g.setColour(juce::Colour((c.id==owner.selectedClip||owner.selectedClips.contains(c.id.value))?0xff3266ac:0xff294d7e).withAlpha(.72f));g.fillRoundedRectangle(r,2.f);g.setColour(juce::Colour(0xff8ac3ff));g.drawRoundedRectangle(r.reduced(.375f),2.f,.75f);
        g.drawText(label(c.name),r.toNearestInt().withHeight(20).reduced(5,0),juce::Justification::left);
        if(midi){g.saveState();g.reduceClipRegion(r.toNearestInt().reduced(1));for(const auto& n:midi->notes){const auto begin=std::max(n.start,midi->source_offset),end=std::min(n.start+n.length,midi->source_offset+midi->length);if(begin>=end)continue;const double total=static_cast<double>(time.to_samples(midi->start+midi->length)-time.to_samples(midi->start));const float x=r.getX()+static_cast<float>((time.to_samples(midi->start+begin-midi->source_offset)-time.to_samples(midi->start))/juce::jmax(1.,total)*r.getWidth());const float w=static_cast<float>((time.to_samples(midi->start+end-midi->source_offset)-time.to_samples(midi->start+begin-midi->source_offset))/juce::jmax(1.,total)*r.getWidth());const float y=r.getY()+25+(127-n.pitch)/127.f*juce::jmax(1.f,r.getHeight()-32);g.fillRect(x,y,juce::jmax(2.f,w),3.f);}g.restoreState();continue;}
        if(const auto* wave=owner.app.waveform(c.source))drawWaveform(g,*wave,r.withTrimmedTop(22),static_cast<double>(c.source_offset),p->sample_rate/pixelsPerSecond);
    }
    for(const auto& c:owner.recordingPreview){auto r=clipRect(c);g.setColour(juce::Colour(0xff683741).withAlpha(.72f));g.fillRoundedRectangle(r,2.f);g.setColour(juce::Colours::salmon);g.drawRoundedRectangle(r.reduced(.375f),2.f,.75f);g.drawText("Recording MIDI",r.toNearestInt().withHeight(20).reduced(5,0),juce::Justification::left);g.saveState();g.reduceClipRegion(r.toNearestInt().reduced(1));for(const auto& n:c.midi->notes){const float x=static_cast<float>(left+static_cast<double>(time.to_samples(c.midi->start+n.start))/p->sample_rate*pixelsPerSecond-horizontal);const float width=static_cast<float>(static_cast<double>(time.to_samples(c.midi->start+n.start+n.length)-time.to_samples(c.midi->start+n.start))/p->sample_rate*pixelsPerSecond);g.fillRect(x,r.getY()+25+(127-n.pitch)/127.f*juce::jmax(1.f,r.getHeight()-32),juce::jmax(2.f,width),3.f);}g.restoreState();}
    for(const auto& preview:owner.audioRecordingPreview){auto r=clipRect(preview.clip);g.setColour(juce::Colour(0xff683741).withAlpha(.72f));g.fillRoundedRectangle(r,2.f);g.setColour(juce::Colours::salmon);g.drawRoundedRectangle(r.reduced(.375f),2.f,.75f);g.drawText("Recording audio",r.toNearestInt().withHeight(20).reduced(5,0),juce::Justification::left);if(preview.waveform)drawRecordingWaveform(g,*preview.waveform,r.withTrimmedTop(22),p->sample_rate/pixelsPerSecond);}
    if(selecting){g.setColour(juce::Colours::skyblue.withAlpha(.12f));g.fillRect(selectionBox);g.setColour(juce::Colours::skyblue);g.drawRect(selectionBox,1.f);}
    g.restoreState();
}
void Arrangement::mouseDown(const juce::MouseEvent& e){
    grabKeyboardFocus();rulerSeeking=false;if(e.x<left||!e.mods.isLeftButtonDown())return;
    if(e.y<header){if(e.y<20){rulerSeeking=!owner.app.recording();owner.run([&]{owner.app.seek(seekSampleAt(e.position.x));});}return;}
    const auto p=owner.project();const int index=trackAt(e.position.y);if(index>=0&&index<static_cast<int>(p->tracks.size()))owner.selectedTrack=p->tracks[static_cast<std::size_t>(index)].id;
    dragGroup.clear();dragTrackDelta=0;
    if(const auto id=clipAt(e.position)){
        const auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip){return clip.id==*id;});
        trim=edgeAt(*c,e.position);auto selection=owner.selectedClips;if(trim||!selection.contains(c->id.value))selection={c->id.value};
        owner.activateClip(c->id);owner.selectedClips=std::move(selection);if(owner.app.recording())return;
        for(const auto& item:p->clips)if(owner.selectedClips.contains(item.id.value))dragGroup.push_back(item);
        drag=*c;const mrs::Timeline time(p->time,p->sample_rate);drag->start=mrs::clip_start(*c,time);drag->length=mrs::clip_end(*c,time)-drag->start;
        dragX=e.position.x;dragStart=trimStart=drag->start;trimEnd=drag->start+drag->length;
        if(trim&&!c->midi){bool ready=false;owner.run([&]{dragSourceFrames=owner.app.source_frames(c->id);ready=true;});if(!ready){drag.reset();dragGroup.clear();return;}}
        setMouseCursor(trim?bracketCursor(trim>0):juce::MouseCursor(juce::MouseCursor::NormalCursor));repaint();return;
    }
    drag.reset();dragTrack.reset();selecting=true;selectionOrigin=e.position;selectionBox={e.position,e.position};owner.selectedClips.clear();owner.selectedClip.reset();repaint();
}
void Arrangement::mouseDrag(const juce::MouseEvent& e){if(rulerSeeking){if(e.x>=left&&e.y>=0&&e.y<20)owner.run([&]{owner.app.seek(seekSampleAt(e.position.x));});return;}if(selecting){selectionBox=juce::Rectangle<float>(selectionOrigin,e.position).getIntersection(juce::Rectangle<float>(static_cast<float>(left),static_cast<float>(header),static_cast<float>(getWidth()-left),static_cast<float>(getHeight()-header)));owner.selectedClips.clear();owner.selectedClip.reset();for(const auto& c:owner.project()->clips)if(clipRect(c).intersects(selectionBox)){owner.selectedClips.insert(c.id.value);if(!owner.selectedClip)owner.selectedClip=c.id;}repaint();return;}if(!drag)return;
    const auto p=owner.project();const mrs::Timeline time(p->time,p->sample_rate);auto delta=static_cast<mrs::Sample>((e.x-dragX)/pixelsPerSecond*p->sample_rate);if(trim){updateTrim(delta);repaint();return;}if(!trim){mrs::Sample low=-mrs::max_sample,high=mrs::max_sample;for(const auto& c:dragGroup){const auto start=mrs::clip_start(c,time);low=std::max(low,-start);high=std::min(high,mrs::max_sample-mrs::clip_end(c,time));}delta=std::clamp(delta,low,high);if(owner.snap){const auto snapped=sampleAt(static_cast<float>(left+static_cast<double>(drag->start+delta)/p->sample_rate*pixelsPerSecond-horizontal));delta=std::clamp(snapped-drag->start,low,high);}}dragStart=juce::jmax<mrs::Sample>(0,drag->start+delta);
    const auto anchor=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==drag->track;});int wanted=trackAt(e.position.y)-static_cast<int>(anchor-p->tracks.begin()),low=0,high=static_cast<int>(p->tracks.size())-1;if(!dragGroup.empty()){low=-static_cast<int>(p->tracks.size());high=static_cast<int>(p->tracks.size());for(const auto& c:dragGroup){const auto t=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& item){return item.id==c.track;});const int at=static_cast<int>(t-p->tracks.begin());low=std::max(low,-at);high=std::min(high,static_cast<int>(p->tracks.size())-1-at);}}dragTrackDelta=std::clamp(wanted,low,high);dragTrack=p->tracks[static_cast<std::size_t>(static_cast<int>(anchor-p->tracks.begin())+dragTrackDelta)].id;repaint();
}
void Arrangement::mouseUp(const juce::MouseEvent& e){
    if(rulerSeeking){rulerSeeking=false;return;}
    if(selecting){selecting=false;selectionBox={};owner.refresh();return;}
    if(!drag)return;const auto c=*drag;const auto delta=dragStart-c.start,begin=trimStart,end=trimEnd;const auto trackDelta=dragTrackDelta,edge=trim;
    std::vector<mrs::Id> group;for(const auto& clip:dragGroup)group.push_back(clip.id);
    drag.reset();dragTrack.reset();dragGroup.clear();dragTrackDelta=0;trim=0;setMouseCursor(juce::MouseCursor::NormalCursor);
    if(edge){if(begin!=c.start||end!=c.start+c.length)owner.run([&]{owner.app.trim_clip(c.id,begin,end);});}
    else if(std::abs(e.x-dragX)>=2||trackDelta!=0)owner.run([&]{owner.app.move_clips(std::move(group),delta,trackDelta);});
    repaint();
}
bool Arrangement::keyPressed(const juce::KeyPress& key){if(key.getKeyCode()!=juce::KeyPress::escapeKey)return false;rulerSeeking=false;setMouseCursor(juce::MouseCursor::NormalCursor);drag.reset();dragTrack.reset();dragGroup.clear();selecting=false;selectionBox={};repaint();return true;}
void Arrangement::wheel(float delta,juce::ModifierKeys mods,float x){if(mods.isCtrlDown()){if(mods.isShiftDown()){const auto before=sampleAt(x);pixelsPerSecond=juce::jlimit(2.,2400.,pixelsPerSecond*std::pow(1.25,delta*4));horizontal=juce::jmax(0.,static_cast<double>(before)/owner.project()->sample_rate*pixelsPerSecond-(x-left));}
    else {const float before=(vertical+getHeight()/2.f-header)/trackHeight;trackHeight=juce::jlimit(128,360,trackHeight+static_cast<int>(delta*96));vertical=juce::jmax(0.f,before*trackHeight-(getHeight()/2.f-header));}}
    else if(mods.isShiftDown())horizontal=juce::jmax(0.,horizontal-delta*160);else vertical=juce::jmax(0.f,vertical-delta*128);
    resized();repaint();}
void Arrangement::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w){wheel(w.deltaY,e.mods,e.position.x);}
void Arrangement::mouseDoubleClick(const juce::MouseEvent& e){if(!e.mods.isLeftButtonDown())return;drag.reset();dragTrack.reset();dragGroup.clear();owner.run([&]{const auto p=owner.project();for(const auto& c:p->clips)if(clipRect(c).contains(e.position)){owner.selectedClip=c.id;owner.openClipEditor(c.id);return;}const int at=trackAt(e.position.y);if(e.x<left||e.y<header||at<0||at>=static_cast<int>(p->tracks.size()))return;const auto& track=p->tracks[static_cast<std::size_t>(at)];if(track.kind==mrs::TrackKind::instrument){const mrs::Timeline t(p->time,p->sample_rate);owner.selectedClip=owner.app.create_midi_clip(track.id,t.to_ticks(sampleAt(e.position.x)));owner.openMidiClip(*owner.selectedClip);}});}
void Arrangement::zoom(double factor){pixelsPerSecond=juce::jlimit(2.,2400.,pixelsPerSecond*factor);repaint();}
void Arrangement::fit(){const auto p=owner.project();const mrs::Timeline t(p->time,p->sample_rate);mrs::Sample end=p->sample_rate*16;for(const auto& c:p->clips)end=juce::jmax(end,mrs::clip_end(c,t));horizontal=0;pixelsPerSecond=juce::jlimit(2.,2400.,(getWidth()-left)*static_cast<double>(p->sample_rate)/end);repaint();}
bool Arrangement::isInterestedInFileDrag(const juce::StringArray& files){for(const auto& f:files)if(!f.endsWithIgnoreCase(".wav"))return false;return !files.isEmpty();}
void Arrangement::filesDropped(const juce::StringArray& files,int x,int y){owner.run([&]{
    if(!isInterestedInFileDrag(files))throw std::runtime_error("Select WAV samples to import");
    std::optional<mrs::Id> target;const int at=trackAt(static_cast<float>(y));
    if(y>=header&&at>=0&&at<static_cast<int>(owner.project()->tracks.size()))target=owner.project()->tracks[static_cast<std::size_t>(at)].id;
    std::vector<std::filesystem::path> paths;for(const auto& file:files)paths.push_back(path(juce::File(file)));
    owner.closeEditors();owner.app.import_wavs(paths,target,sampleAt(static_cast<float>(x)));owner.refresh(true);
});}
bool Arrangement::isInterestedInDragSource(const SourceDetails& details){return details.description.toString().startsWith("mrs-track:")||(details.description.toString()=="mrs-sample-files"&&!owner.browser->selectedSamples().isEmpty());}
void Arrangement::itemDropped(const SourceDetails& details){if(details.description.toString().startsWith("mrs-track:")){const auto y=details.localPosition.y;const auto index=juce::jlimit(0,static_cast<int>(owner.project()->tracks.size()),static_cast<int>(std::floor((y-header+vertical)/trackHeight+.5f)));owner.dropTrack(details.description.toString(),static_cast<std::size_t>(index));}else if(isInterestedInDragSource(details))filesDropped(owner.browser->selectedSamples(),details.localPosition.x,details.localPosition.y);}


namespace {
class BrowserNode final : public juce::TreeViewItem {
public:
    BrowserNode(juce::String text,int id=-1):name(std::move(text)),plugin(id){}
    bool mightContainSubItems() override{return plugin<0;}
    juce::String getUniqueName() const override{return name;}
    void paintItem(juce::Graphics& g,int width,int height) override{g.setColour(juce::Colours::whitesmoke);g.setFont(ui::Theme::channelFont());g.drawText(name,2,0,width-4,height,juce::Justification::left);}
    juce::var getDragSourceDescription() override{return plugin<0?juce::var():juce::var("vst3:"+juce::String(plugin));}
    juce::String name;int plugin;
};
}
Browser::Browser(Desktop& d):owner(d){
    addAndMakeVisible(locations);addAndMakeVisible(breadcrumbView);breadcrumbView.setViewedComponent(&breadcrumbBody,false);breadcrumbView.setScrollBarsShown(false,false);
    auto addLocation=[&](juce::String name,juce::File file){if(file.isDirectory()){locationPaths.push_back(file);locations.addItem(name,static_cast<int>(locationPaths.size()));}};
    addLocation("Desktop",juce::File::getSpecialLocation(juce::File::userDesktopDirectory));addLocation("Documents",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));
    addLocation("Music",juce::File::getSpecialLocation(juce::File::userMusicDirectory));addLocation("MR Studio",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MR Studio"));
    juce::Array<juce::File> drives;juce::File::findFileSystemRoots(drives);for(const auto& drive:drives)addLocation("Volume "+drive.getFullPathName(),drive);
    locations.setTitle("Quick folders and volumes");breadcrumbView.setTitle("Folder breadcrumbs");locations.setTextWhenNothingSelected("Folders");locations.onChange=[this]{const auto index=locations.getSelectedId()-1;if(index>=0&&index<static_cast<int>(locationPaths.size()))owner.run([&]{navigate(locationPaths[static_cast<std::size_t>(index)]);});};
    fileTree.addListener(this);startTimerHz(10);

    for(auto* component:{static_cast<juce::Component*>(&vstTab),static_cast<juce::Component*>(&filesTab),static_cast<juce::Component*>(&parentFolder),static_cast<juce::Component*>(&chooseFolder),static_cast<juce::Component*>(&folderPath),static_cast<juce::Component*>(&fileTree)})addAndMakeVisible(component);
    vstTab.onClick=[this]{showFiles(false);};filesTab.onClick=[this]{showFiles(true);};
    parentFolder.onClick=[this]{navigate(directory.getDirectory().getParentDirectory());};
    juce::Component::SafePointer<Browser> safe(this);chooseFolder.onClick=[safe]{if(safe)safe->owner.choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe](const auto& file){if(safe)safe->navigate(file);});};
    folderPath.setTitle("Computer folder path");folderPath.onReturnKey=[this]{owner.run([&]{navigate(juce::File(folderPath.getText()));});};
    fileTree.setColour(juce::TreeView::selectedItemBackgroundColourId,juce::Colour(0xff375a80));
    fileTree.setColour(juce::DirectoryContentsDisplayComponent::highlightColourId,juce::Colour(0xff375a80));fileTree.setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId,juce::Colours::white);
    fileTree.setColour(juce::TreeView::backgroundColourId,juce::Colour(surface));fileTree.setColour(juce::DirectoryContentsDisplayComponent::textColourId,juce::Colours::whitesmoke);
    fileTree.setTitle("Computer folders and WAV samples");fileTree.setDragAndDropDescription("mrs-sample-files");fileTree.setMultiSelectEnabled(true);
    fileThread.startThread();navigate(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));showFiles(false);
search.setTitle("Search VST3 plugins");search.setTextToShowWhenEmpty("Search VST3...",juce::Colours::grey);tree.setTitle("VST3 plugins by vendor");addAndMakeVisible(tree);addAndMakeVisible(scanButton);addAndMakeVisible(search);tree.setDefaultOpenness(false);tree.setRootItemVisible(false);
    search.setTextToShowWhenEmpty("Search VST3...",juce::Colours::grey);search.onTextChange=[this]{rebuild();};
    scanButton.onClick=[this]{owner.choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[this](const auto& f){owner.scan(f);});};rebuild();}
Browser::~Browser(){stopTimer();fileTree.removeListener(this);breadcrumbView.setViewedComponent(nullptr,false);tree.setRootItem(nullptr);fileThread.stopThread(2000);}
void Browser::navigate(const juce::File& folder){if(!folder.isDirectory())throw std::runtime_error("Folder is unavailable");directory.setDirectory(folder,true,true);folderPath.setText(folder.getFullPathName(),false);
    crumbs.clear();std::vector<juce::File> parents;auto current=folder;for(int i=0;i<32;++i){parents.push_back(current);auto parent=current.getParentDirectory();if(parent==current)break;current=parent;}
    int x=0;for(auto it=parents.rbegin();it!=parents.rend();++it){auto file=*it;auto text=file.getFileName().isEmpty()?file.getFullPathName():file.getFileName();auto button=std::make_unique<juce::TextButton>(text+"  >");const int width=juce::jlimit(62,170,text.length()*7+28);button->setBounds(x,0,width,24);x+=width+2;
        juce::Component::SafePointer<Browser> safe(this);button->onClick=[safe,file]{juce::MessageManager::callAsync([safe,file]{if(safe)safe->owner.run([&]{safe->navigate(file);});});};breadcrumbBody.addAndMakeVisible(*button);crumbs.push_back(std::move(button));}
    breadcrumbBody.setSize(x,24);breadcrumbView.setViewPosition(juce::jmax(0,x-breadcrumbView.getWidth()),0);
}
void Browser::selectionChanged(){const auto files=selectedSamples();wantedFile=files.isEmpty()?juce::File():juce::File(files[0]);if(wantedFile==juce::File()){preview={};repaint();}}
void Browser::timerCallback(){
    const auto selected=selectedSamples();wantedFile=selected.isEmpty()?juce::File():juce::File(selected[0]);
    if(previewWorker.valid()){if(previewWorker.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;auto result=previewWorker.get();if(result.file==wantedFile){preview=std::move(result);repaint();}}
    if(wantedFile==juce::File()||preview.file==wantedFile)return;auto file=wantedFile;
    previewWorker=std::async(std::launch::async,[file]{Preview result;result.file=file;try{auto wav=mrs::audio::inspect_wav(path(file));result.info=juce::String(wav.sample_rate/1000.,1)+" kHz | "+juce::String(wav.bits)+" bit | "+(wav.channels==1?"Mono":wav.channels==2?"Stereo":juce::String(wav.channels)+" channels")+"\nWAV | "+juce::String(static_cast<double>(wav.frame_count)/wav.sample_rate/60,2)+" min | "+file.getLastModificationTime().formatted("%d.%m.%Y %H:%M");
        for(std::size_t bin=0;bin<result.levels.size();++bin){const auto first=wav.frame_count*static_cast<mrs::Sample>(bin)/static_cast<mrs::Sample>(result.levels.size());const auto count=std::min<mrs::Sample>(128,wav.frame_count-first);if(count<=0)continue;std::vector<float> block(static_cast<std::size_t>(count)*wav.channels);wav.read(first,block);for(float value:block)if(std::isfinite(value))result.levels[bin]=std::max(result.levels[bin],std::abs(value));}
    }catch(const std::exception& e){result.info=label(e.what());}return result;});
}

void Browser::showFiles(bool files){filesVisible=files;locations.setVisible(files);breadcrumbView.setVisible(files);for(auto* component:{static_cast<juce::Component*>(&fileTree),static_cast<juce::Component*>(&folderPath),static_cast<juce::Component*>(&parentFolder),static_cast<juce::Component*>(&chooseFolder)})component->setVisible(files);
    tree.setVisible(!files);scanButton.setVisible(!files);search.setVisible(!files);vstTab.setToggleState(!files,juce::dontSendNotification);filesTab.setToggleState(files,juce::dontSendNotification);repaint();}
juce::StringArray Browser::selectedSamples() const {juce::StringArray files;for(int i=0;i<fileTree.getNumSelectedFiles();++i){auto file=fileTree.getSelectedFile(i);if(file.existsAsFile()&&file.hasFileExtension("wav"))files.add(file.getFullPathName());}return files;}
void Browser::resized(){vstTab.setBounds(8,0,76,26);filesTab.setBounds(90,0,76,26);scanButton.setBounds(8,32,getWidth()-16,26);search.setBounds(8,64,getWidth()-16,26);tree.setBounds(8,98,getWidth()-16,getHeight()-122);
    locations.setBounds(8,32,getWidth()-132,26);parentFolder.setBounds(getWidth()-118,32,44,26);chooseFolder.setBounds(getWidth()-68,32,60,26);chooseFolder.setButtonText("...");folderPath.setBounds(8,64,getWidth()-16,26);breadcrumbView.setBounds(8,98,getWidth()-16,24);fileTree.setBounds(4,126,getWidth()-8,juce::jmax(30,getHeight()-278));}
void Browser::paint(juce::Graphics& g){g.fillAll(juce::Colour(surface));g.setColour(juce::Colours::whitesmoke);g.setFont(owner.theme.font(11));
    if(filesVisible){auto area=juce::Rectangle<int>(0,getHeight()-148,getWidth(),124);g.setColour(juce::Colour(panel));g.fillRect(area);g.setColour(juce::Colours::whitesmoke);g.drawText(preview.file==juce::File()?"Select a WAV sample":preview.file.getFileNameWithoutExtension(),area.removeFromTop(28).reduced(8,0),juce::Justification::left);
        g.drawFittedText(preview.info,area.removeFromTop(42).reduced(8,0),juce::Justification::topLeft,2);auto wave=area.reduced(8,4).toFloat();g.setColour(juce::Colour(0xff929ba4));for(int x=0;x<static_cast<int>(wave.getWidth());++x){const auto index=static_cast<std::size_t>(x*256/juce::jmax(1,static_cast<int>(wave.getWidth())));const auto height=juce::jlimit(1.f,wave.getHeight(),preview.levels[index]*wave.getHeight());g.drawVerticalLine(static_cast<int>(wave.getX())+x,wave.getCentreY()-height/2,wave.getCentreY()+height/2);}
    }
    g.drawText(filesVisible?"Drag WAV onto a track or empty space":"Drag an effect onto a mixer channel",8,getHeight()-23,getWidth()-16,20,juce::Justification::left);}
void Browser::rebuild(){tree.setRootItem(nullptr);root=std::make_unique<BrowserNode>("root");std::map<std::string,BrowserNode*> vendors;
    for(std::size_t i=0;i<owner.catalog.size();++i){const auto& p=owner.catalog[i];if(!search.getText().isEmpty()&&!label(p.name+" "+p.vendor).containsIgnoreCase(search.getText()))continue;
        if(!vendors.contains(p.vendor)){auto* node=new BrowserNode(label(p.vendor));root->addSubItem(node);node->setOpen(false);vendors[p.vendor]=node;}vendors[p.vendor]->addSubItem(new BrowserNode(label(p.name),static_cast<int>(i)));}
    tree.setRootItem(root.get());root->setOpen(true);
}
void Desktop::scan(const juce::File& root){if(scanner.valid())throw std::runtime_error("Scan already running");*scanCancel=false;const auto folder=path(root);const auto helper=path(juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("mrs_vst3_scan.exe"));
    auto cache=cacheFile;auto cancel=scanCancel;scanner=std::async(std::launch::async,[folder,helper,cache,cancel]{return mrs::processing::scan_vst3(folder,helper,cache,cancel);});}
std::vector<mrs::NativeInsert> Desktop::chain(std::optional<mrs::Id> t) const{const auto p=project();if(!t)return p->master_inserts;for(const auto& track:p->tracks)if(track.id==t)return track.inserts;throw std::runtime_error("Track no longer exists");}
void Desktop::closeEditors(bool keepMidi){if(!keepMidi&&dockedEditor){dockedEditor->setVisible(false);dockedClip.reset();}app.engine()->set_profiling(false);app.close_plugin_editors();for(auto& w:windows){if(keepMidi&&static_cast<bool>(w->getProperties()["mrs-midi-clip"]))continue;retirePluginWindow(*w);w->setVisible(false);
    // Retire the native parent HWND while its plugin module is still alive.
    // Keep the C++ window until the next timer turn so an editor callback can return safely.
    if(static_cast<bool>(w->getProperties()["mrs-native-editor"]))w->removeFromDesktop();}}
void Desktop::closeUnpinnedEditors(){for(auto& w:windows)if(w->isVisible()&&!static_cast<bool>(w->getProperties()["mrs-pinned"]))w->closeButtonPressed();}
void Desktop::applyChain(std::optional<mrs::Id> target,std::vector<mrs::NativeInsert> effects){
    const auto before=chain(target);
    bool bypassChanged=before.size()==effects.size();
    bool changed=false;
    for(std::size_t i=0;i<before.size()&&bypassChanged;++i){auto a=before[i],b=effects[i];changed|=a.kind==mrs::InsertKind::vst3&&a.bypass!=b.bypass;a.bypass=b.bypass;bypassChanged=a==b;}
    bypassChanged&=changed;
    // Bypass can change graph PDC. Detach every view before old processors die,
    // then attach the new processors to the same visible editor windows.
    std::vector<EditorWindow*> views;
    if(bypassChanged){if(app.engine()->state().playback==mrs::PlaybackState::playing||app.recording())throw std::runtime_error("Pause/Stop before changing plugin bypass");
        for(auto& w:windows)if(w->isVisible()&&static_cast<bool>(w->getProperties()["mrs-native-editor"]))views.push_back(w.get());
        app.close_plugin_editors();for(auto* w:views)retirePluginWindow(*w);
    }
    try{app.set_inserts(target,std::move(effects));}catch(...){for(auto* w:views){try{reconnectPluginWindow(*w);}catch(...){w->setVisible(false);}}throw;}
    editorGeneration=app.insert_generation();
    for(auto* w:views){try{reconnectPluginWindow(*w);}catch(...){w->setVisible(false);throw;}}
    refresh(true);
}
void Desktop::addPlugin(std::optional<mrs::Id> target,std::size_t i){run([&]{if(i>=catalog.size())throw std::runtime_error("Unknown browser plugin");closeEditors();auto effects=chain(target);const auto& p=catalog[i];mrs::NativeInsert n;n.id=mrs::new_id();n.kind=mrs::InsertKind::vst3;n.plugin_name=p.name;n.plugin_path=p.path;n.class_id=p.class_id;effects.push_back(n);applyChain(target,std::move(effects));openInsert(target,n.id);});}
void Desktop::routeMenu(mrs::Id id){juce::PopupMenu m;m.addItem(1,"Master");auto p=project();for(std::size_t i=0;i<p->tracks.size();++i)if(p->tracks[i].kind==mrs::TrackKind::bus&&p->tracks[i].id!=id)m.addItem(10+static_cast<int>(i),label(p->tracks[i].name));m.addItem(2,"Hardware outputs...");
    juce::Component::SafePointer<Desktop> safe(this);m.showMenuAsync(popup(this),[safe,id,p](int n){if(!safe||!n)return;safe->run([&]{if(n==2)safe->textDialog("Hardware outputs (1,2...)","1,2",[safe,id](auto s){if(safe)safe->app.set_hardware_output(id,mrs::desktop::parse_outputs(s.toStdString()));});else safe->app.set_track_output(id,n==1?std::optional<mrs::Id>{}:p->tracks[static_cast<std::size_t>(n-10)].id);});});}
void Desktop::sendsMenu(mrs::Id id){juce::PopupMenu m;auto p=project();mrs::Track source;for(const auto& t:p->tracks)if(t.id==id)source=t;
    m.addItem(1,"New return / send...");for(std::size_t i=0;i<p->tracks.size();++i)if(p->tracks[i].kind==mrs::TrackKind::bus&&p->tracks[i].id!=id)m.addItem(10+static_cast<int>(i),"Add send to "+label(p->tracks[i].name));
    for(std::size_t i=0;i<source.sends.size();++i){juce::PopupMenu s;s.addItem(100+static_cast<int>(i),"Gain...");s.addItem(200+static_cast<int>(i),source.sends[i].pre_fader?"Use post fader":"Use pre fader");s.addItem(300+static_cast<int>(i),"Remove send");m.addSubMenu("Send "+juce::String(static_cast<int>(i)+1),s);}
    juce::Component::SafePointer<Desktop> safe(this);m.showMenuAsync(popup(this),[safe,id,p,source](int n){if(!safe||!n)return;safe->run([&]{if(n==1)safe->app.add_return_send(id,"Return");
        else if(n<100){auto sends=source.sends;sends.push_back({p->tracks[static_cast<std::size_t>(n-10)].id,1,false});safe->app.set_track_sends(id,sends);}
        else if(n<200){const auto index=static_cast<std::size_t>(n-100);safe->textDialog("Send gain (linear)",juce::String(source.sends[index].gain),[safe,id,index](auto s){if(safe)safe->app.set_send_gain(id,index,s.getFloatValue());});}
        else {auto sends=source.sends;auto i=static_cast<std::size_t>(n%100);if(n<300)sends[i].pre_fader=!sends[i].pre_fader;else sends.erase(sends.begin()+static_cast<std::ptrdiff_t>(i));safe->app.set_track_sends(id,sends);}});});}
}
