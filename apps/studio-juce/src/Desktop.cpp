#include "Desktop.h"
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
EditorWindow::EditorWindow(juce::String name,juce::Component* content)
    :DocumentWindow(name,juce::Colour(background),closeButton){
    setUsingNativeTitleBar(true);setContentOwned(content,true);centreWithSize(content->getWidth(),content->getHeight());
    setVisible(true);
}
void EditorWindow::closeButtonPressed(){if(onClose)onClose();setVisible(false);}
void EditorWindow::fitNativeEditor(int width,int height){
    const double scale=getPeer()?getPeer()->getPlatformScaleFactor():1.;
    if(width<1||height<1||width>4096||height>4096||!std::isfinite(scale)||scale<=0)
        throw std::runtime_error("Invalid native editor dimensions");
    setResizeLimits(1,1,8192,8192);
    setContentComponentSize(juce::jmax(1,juce::roundToInt(width/scale)),juce::jmax(1,juce::roundToInt(height/scale)));
}

Desktop::Desktop(bool test):testing(test){
    setLookAndFeel(&theme);setWantsKeyboardFocus(true);setTitle("Moon River Studio workspace");setFocusContainerType(FocusContainerType::keyboardFocusContainer);
    const auto config=juce::File(juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA",juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getFullPathName())).getChildFile("MoonRiverStudio");
    viewFile=path(config.getChildFile("juce-view.json"));prefsFile=path(config.getChildFile("juce-preferences.json"));cacheFile=path(config.getChildFile("vst3.cache"));
    if(!testing)try{const auto readPrefs=std::filesystem::exists(prefsFile)?prefsFile:path(config.getChildFile("desktop.cfg"));std::ifstream in(readPrefs,std::ios::binary);if(in){std::string bytes((std::istreambuf_iterator<char>(in)),{});prefs=mrs::desktop::decode_preferences(bytes);}
        catalog=mrs::processing::load_vst3_cache(cacheFile);}catch(const std::exception& e){message=label(e.what());}
    if(!testing)try{juce::File file(juce::String(viewFile.wstring().c_str()));if(file.existsAsFile())view=ViewSettings::decode(file.loadFileAsString());}catch(const std::exception& e){message=label(e.what());}
    sidebar=view.sidebar;browserWidth=view.browserWidth;snap=view.snap;
    app.demo();resetDevice();
    arrangement=std::make_unique<Arrangement>(*this);browser=std::make_unique<Browser>(*this);
    for(auto* c:{static_cast<juce::Component*>(&menu),static_cast<juce::Component*>(arrangement.get()),static_cast<juce::Component*>(browser.get()),static_cast<juce::Component*>(&mixerViewport)})addAndMakeVisible(c);
    mixerViewport.setViewedComponent(&mixerBody,false);mixerViewport.setScrollBarsShown(false,true);
    for(auto* b:{&play,&pause,&stop,&record,&previous,&next,&loop,&arrangeButton,&editButton,&mixButton,&brows,&addTrack,&addBus,&undo,&redo,&split,&remove,&zoomIn,&zoomOut,&fit,&audio,&snapButton})addAndMakeVisible(b);
    auto bind=[this](juce::TextButton& b,int cmd){b.onClick=[this,cmd]{action(cmd);};};
    bind(play,30);bind(pause,31);bind(stop,32);bind(record,33);bind(previous,34);bind(next,35);bind(loop,36);
    bind(addTrack,20);bind(addBus,21);bind(undo,10);bind(redo,11);bind(split,24);bind(remove,25);bind(audio,40);
    arrangeButton.onClick=[this]{setWorkspace(mrs::desktop::Workspace::arrange);};
    editButton.onClick=[this]{setWorkspace(mrs::desktop::Workspace::edit);};
    mixButton.onClick=[this]{setWorkspace(app.workspace()==mrs::desktop::Workspace::mix?mrs::desktop::Workspace::arrange:mrs::desktop::Workspace::mix);};
    brows.onClick=[this]{sidebar=!sidebar;resized();repaint();};
    zoomIn.onClick=[this]{arrangement->zoom(1.25);};zoomOut.onClick=[this]{arrangement->zoom(.8);};fit.onClick=[this]{arrangement->fit();};
    snapButton.setButtonText(snap ? "Snap on" : "Snap off");
    arrangement->trackHeight=view.trackHeight;arrangement->pixelsPerSecond=view.pixelsPerSecond;
    int focus=1;for(auto* b:{&undo,&redo,&addTrack,&addBus,&split,&remove,&zoomIn,&zoomOut,&fit,&snapButton,&audio,&play,&pause,&stop,&record,&previous,&next,&loop,&arrangeButton,&editButton,&mixButton,&brows}){b->setTitle(b->getButtonText());b->setExplicitFocusOrder(focus++);}
    snapButton.onClick=[this]{snap=!snap;snapButton.setButtonText(snap ? "Snap on" : "Snap off");};
    setSize(1400,850);refresh(true);setWorkspace(testing?mrs::desktop::Workspace::mix:prefs.workspace==mrs::desktop::Workspace::live?mrs::desktop::Workspace::arrange:prefs.workspace);if(!testing)try{reconnectDevice();}catch(const std::exception& e){message=label(e.what());}startTimerHz(30);
}
Desktop::~Desktop(){try{saveSettings();}catch(...){}stopTimer();*scanCancel=true;chooser.reset();closeEditors();
    if(scanner.valid())scanner.wait();setLookAndFeel(nullptr);mixerViewport.setViewedComponent(nullptr,false);}
void Desktop::run(std::function<void()> f){try{f();message.clear();refresh();}catch(const std::exception& e){message=label(e.what());repaint();}}
void Desktop::resetDevice(){app.connect(mrs::audio::make_offline_device(),{0,project()->sample_rate,128,{}, {0,1}});}
void Desktop::saveSettings(){if(testing)return;
    prefs.workspace=app.workspace();view.sidebar=sidebar;view.browserWidth=browserWidth;view.snap=snap;
    if(arrangement){view.trackHeight=arrangement->trackHeight;view.pixelsPerSecond=arrangement->pixelsPerSecond;}
    publishSettings(juce::File(juce::String(prefsFile.wstring().c_str())),label(mrs::desktop::encode_preferences(prefs)));
    publishSettings(juce::File(juce::String(viewFile.wstring().c_str())),view.encode());
}
void Desktop::remember(){if(testing)return;mrs::desktop::remember_project(prefs,app.path());saveSettings();}
void Desktop::reconnectDevice(){if(testing||!prefs.reconnect_audio||prefs.device_name.empty())return;
    if(project()->sample_rate!=prefs.rate)throw std::runtime_error("ASIO reconnect skipped: saved device rate differs from project rate. Review Audio settings.");
#ifdef MRS_HAS_ASIO
    auto device=mrs::audio::make_asio_device();auto infos=device->enumerate();
    auto found=std::find_if(infos.begin(),infos.end(),[&](const auto& info){return info.name==prefs.device_name;});
    if(found==infos.end())throw std::runtime_error("Saved ASIO device unavailable; running offline. Review Audio settings.");
    if(!view.deviceChannels.isEmpty()&&view.deviceChannels!=deviceLayout(*found))
        throw std::runtime_error("ASIO channel names changed; running offline. Review Audio settings.");
    mrs::audio::DeviceConfig config{found->index,prefs.rate,prefs.buffer,{},prefs.outputs};
    if(!view.inputs.trim().isEmpty())config.inputs=mrs::desktop::parse_outputs(view.inputs.toStdString());
    else if(prefs.monitor_input>=0)config.inputs={prefs.monitor_input};
    try{app.connect(std::move(device),config);}catch(...){resetDevice();throw;}
#endif
}
void Desktop::openFile(const juce::File& f){closeEditors();cancelPreview();app.open_project(path(f));resetDevice();selectedClip.reset();selectedTrack.reset();remember();refresh(true);arrangement->fit();reconnectDevice();}
void Desktop::saveFile(const juce::File& f){auto destination=path(f);if(destination!=app.path())destination=mrs::desktop::project_folder_file(destination);app.save_project(destination);remember();refresh();}
void Desktop::importFiles(const juce::StringArray& files){closeEditors();std::vector<std::filesystem::path> paths;for(const auto& f:files)paths.push_back(path(juce::File(f)));app.import_wavs(paths);refresh(true);}
void Desktop::choose(int chooserOptions,std::function<void(const juce::File&)> callback,juce::String pattern){
    chooser=std::make_unique<juce::FileChooser>("Moon River Studio",juce::File(),pattern,true);
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
void Desktop::confirmDiscard(std::function<void()> nextAction){if(!app.dirty()){nextAction();return;}
    auto* alert=new juce::AlertWindow("Save project changes?",label(project()->title),juce::MessageBoxIconType::NoIcon);
    alert->addButton("Save",1);alert->addButton("Discard",2);alert->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<Desktop> safe(this);alert->enterModalState(true,juce::ModalCallbackFunction::create([safe,nextAction](int n){if(!safe)return;
        if(n==2)nextAction();else if(n==1)safe->run([&]{if(!safe->app.path().empty()){safe->saveFile(juce::File(juce::String(safe->app.path().wstring().c_str())));nextAction();}
            else safe->choose(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe,nextAction](auto f){if(safe){safe->saveFile(f.withFileExtension("mrsproject"));nextAction();}},"*.mrsproject");});
    }),true);
}
juce::StringArray Desktop::getMenuBarNames(){return {"File","Edit","Track","Transport"};}
juce::PopupMenu Desktop::getMenuForIndex(int n,const juce::String&){juce::PopupMenu m;
    if(n==0){m.addItem(1,"New project");m.addItem(2,"Open project...");m.addItem(3,"Save");m.addItem(4,"Save as...");m.addItem(5,"Import WAV...");
        juce::PopupMenu recent;for(std::size_t i=0;i<prefs.recent_projects.size();++i)recent.addItem(1000+static_cast<int>(i),label(prefs.recent_projects[i]));m.addSubMenu("Open recent project",recent);}
    if(n==1){m.addItem(10,"Undo",app.services().projects->state().can_undo);m.addItem(11,"Redo",app.services().projects->state().can_redo);m.addItem(24,"Split selected clip (S)");m.addItem(25,"Delete selected clip");}
    if(n==2){m.addItem(20,"Add audio track");m.addItem(21,"Add bus");m.addItem(22,"Rename track");m.addItem(23,"Delete track");m.addItem(26,"Move track up");m.addItem(27,"Move track down");}
    if(n==3){m.addItem(30,"Play");m.addItem(31,"Pause");m.addItem(32,"Stop");m.addItem(33,"Record");m.addItem(34,"Previous section");m.addItem(35,"Next section");m.addItem(36,"Loop section");m.addItem(40,"Audio settings...");}
    return m;
}
void Desktop::menuItemSelected(int n,int){action(n);}
void Desktop::action(int n){if(busyGesture())return;run([&]{
    if(n>=1000){auto at=static_cast<std::size_t>(n-1000);if(at<prefs.recent_projects.size()){const auto f=juce::File(label(prefs.recent_projects[at]));confirmDiscard([this,f]{run([&]{openFile(f);});});}return;}
    if(n==1 || n==2){confirmDiscard([this,n]{run([&]{if(n==1){closeEditors();app.new_project();resetDevice();reconnectDevice();selectedClip.reset();selectedTrack.reset();refresh(true);}else choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](const auto& f){openFile(f);},"*.mrsproject");});});}
    else if(n==3){if(app.path().empty())action(4);else saveFile(juce::File(juce::String(app.path().wstring().c_str())));}
    else if(n==4)choose(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[this](const auto& f){saveFile(f.withFileExtension("mrsproject"));},"*.mrsproject");
    else if(n==5)choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](const auto& f){importFiles({f.getFullPathName()});},"*.wav");
    else if(n==10 || n==11){closeEditors();if(n==10)app.undo();else app.redo();refresh(true);}
    else if(n==20)selectedTrack=app.add_audio_track("Audio "+std::to_string(project()->tracks.size()+1));
    else if(n==21)selectedTrack=app.add_bus("Bus "+std::to_string(project()->tracks.size()+1));
    else if(n==22 && selectedTrack){auto id=*selectedTrack;for(const auto& t:project()->tracks)if(t.id==id)textDialog("Rename track",label(t.name),[this,id](auto s){app.rename_track(id,s.toStdString());});}
    else if(n==23 && selectedTrack){closeEditors();app.remove_track(*selectedTrack);selectedTrack.reset();selectedClip.reset();}
    else if((n==26 || n==27) && selectedTrack){const auto p=project();for(std::size_t i=0;i<p->tracks.size();++i)if(p->tracks[i].id==selectedTrack && ((n==26&&i>0)||(n==27&&i+1<p->tracks.size())))app.reorder_track(*selectedTrack,n==26?i-1:i+1);}
    else if(n==24 && selectedClip){auto id=app.split_clip(*selectedClip,app.engine()->state().sample);selectedClip=id;}
    else if(n==25 && selectedClip){app.remove_clip(*selectedClip);selectedClip.reset();}
    else if(n==30)app.play();else if(n==31)app.pause();else if(n==32){if(app.recording())app.stop_recording();app.stop();}
    else if(n==33){if(app.recording()){app.stop_recording();app.stop();refresh(true);}else choose(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles,[this](auto f){app.start_recording(path(f.withFileExtension("wav")));},"*.wav");}
    else if(n==34)app.musical().previous_section();else if(n==35)app.musical().next_section();
    else if(n==36){if(app.engine()->state().loop)app.musical().clear_loop();else if(app.musical().state().current_section)app.musical().loop_section(app.musical().state().current_section->id);}
    else if(n==40)audioSettings();
});}
void Desktop::setWorkspace(mrs::desktop::Workspace w){app.workspace(w);prefs.workspace=w;resized();refresh();}
void Desktop::resized(){menu.setBounds(0,0,getWidth(),26);int x=8;
    for(auto* b:{&undo,&redo,&addTrack,&addBus,&split,&remove,&zoomIn,&zoomOut,&fit,&snapButton}){const int width=(b==&snapButton?86:78);b->setBounds(x,34,width,28);x+=width+5;}
    audio.setBounds(getWidth()-148,34,140,28);
    const int available=getWidth()-(sidebar?browserWidth:0)-16;
    const bool showMix=app.workspace()==mrs::desktop::Workspace::mix;
    const int mixerHeight=showMix ? juce::jlimit(210,365,(getHeight()-120)/2) : 0;
    arrangeArea={8,70,available,getHeight()-128-mixerHeight};arrangement->setBounds(arrangeArea);
    mixArea={8,arrangeArea.getBottom()+6,available,mixerHeight-6};
    mixerViewport.setVisible(showMix);if(master)master->setVisible(showMix);
    if(showMix){mixerViewport.setBounds(mixArea.withTrimmedRight(140));mixerBody.setSize(juce::jmax(mixerViewport.getWidth(),static_cast<int>(mixer.size())*148),mixArea.getHeight()-14);
        for(std::size_t i=0;i<mixer.size();++i)mixer[i]->setBounds(static_cast<int>(i)*148,0,140,mixerBody.getHeight());
        if(master)master->setBounds(mixArea.getRight()-132,mixArea.getY(),132,mixArea.getHeight()-14);}
    browser->setVisible(sidebar);browser->setBounds(getWidth()-browserWidth,70,browserWidth-8,getHeight()-128);
    x=8;for(auto* b:{&play,&pause,&stop,&record,&previous,&next,&loop}){int width=b==&record||b==&loop?100:78;b->setBounds(x,getHeight()-45,width,30);x+=width+5;}
    x=getWidth()-344;for(auto* b:{&arrangeButton,&editButton,&mixButton,&brows}){b->setBounds(x,getHeight()-45,78,30);x+=84;}
}
void Desktop::paint(juce::Graphics& g){g.fillAll(juce::Colour(background));g.setFont(theme.font(12));g.setColour(message.isEmpty()?juce::Colours::lightgrey:juce::Colours::orange);
    auto s=message.isEmpty()?label(project()->title)+(app.dirty()?" *":"")+"  |  "+label(app.audio_name()):message;
    g.drawText(s,10,getHeight()-62,getWidth()-20,16,juce::Justification::left);
    g.setColour(juce::Colours::whitesmoke);g.drawText(juce::String(static_cast<double>(app.engine()->state().sample)/project()->sample_rate,2)+" s",680,getHeight()-44,120,28,juce::Justification::left);
}
void Desktop::mouseDown(const juce::MouseEvent& e){resizingBrowser=sidebar&&std::abs(e.x-(getWidth()-browserWidth-4))<=4;}
void Desktop::mouseDrag(const juce::MouseEvent& e){if(resizingBrowser){browserWidth=juce::jlimit(200,450,getWidth()-e.x);resized();repaint();}}
void Desktop::mouseUp(const juce::MouseEvent&){resizingBrowser=false;}
void Desktop::refresh(bool force){const auto p=project();const auto state=app.services().projects->state();
    std::vector<mrs::Id> current;for(const auto& t:p->tracks)current.push_back(t.id);
    if(current!=ids){closeEditors();cancelPreview();ids=current;mixer.clear();for(const auto& t:p->tracks){auto s=std::make_unique<Strip>(*this,t.id);mixerBody.addAndMakeVisible(*s);mixer.push_back(std::move(s));}
        master=std::make_unique<Strip>(*this,std::nullopt);addAndMakeVisible(*master);arrangement->rebuild();resized();}
    if(force || revision!=state.revision){app.prepare_waveforms();for(auto& s:mixer)s->sync();for(auto& s:arrangement->rows)s->sync();if(master)master->sync();revision=state.revision;}
    undo.setEnabled(state.can_undo);redo.setEnabled(state.can_redo);
    arrangeButton.setToggleState(app.workspace()==mrs::desktop::Workspace::arrange,juce::dontSendNotification);
    editButton.setToggleState(app.workspace()==mrs::desktop::Workspace::edit,juce::dontSendNotification);
    mixButton.setToggleState(app.workspace()==mrs::desktop::Workspace::mix,juce::dontSendNotification);
    brows.setToggleState(sidebar,juce::dontSendNotification);repaint();arrangement->repaint();
}
bool Desktop::busyGesture() const{if(preview&&preview->active())return true;return false;}
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
bool Desktop::keyPressed(const juce::KeyPress& k){if(!shortcutAllowed())return false;
    auto c=k.getKeyCode();if(k.getModifiers().isAltDown())return false;if(k.getModifiers().isCtrlDown()){if(c=='Z'){action(10);return true;}if(c=='Y'){action(11);return true;}if(c=='S'){action(3);return true;}if(c=='O'){action(2);return true;}if(c=='N'){action(1);return true;}return false;}
    if(k.getModifiers().isShiftDown())return false;
    if(c=='S'){action(24);return true;}if(c=='R'){action(33);return true;}if(c==juce::KeyPress::deleteKey){action(25);return true;}return c==juce::KeyPress::spaceKey;
}
bool Desktop::keyStateChanged(bool){if(!shortcutAllowed()){spaceHeld=false;return false;}
    const bool down=juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey);spaceKey(down);return down;}
void Desktop::spaceKey(bool down){if(down&&!spaceHeld)action(app.engine()->state().playback==mrs::PlaybackState::playing?32:30);spaceHeld=down;}
void Desktop::focusLost(FocusChangeType){spaceHeld=false;}
void Desktop::timerCallback(){try{app.poll();if(preview){if(preview->active())app.preview_mix(preview->target,preview->mix,preview->master);else preview.reset();}
    if(editorGeneration!=app.insert_generation()){closeEditors();editorGeneration=app.insert_generation();}
    std::erase_if(windows,[](const auto& w){return !w->isVisible();});
    if(scanner.valid()&&scanner.wait_for(std::chrono::seconds(0))==std::future_status::ready){catalog=scanner.get();browser->rebuild();}
    const auto meter=app.engine()->take_meters();for(std::size_t i=0;i<peaks.size();++i){peaks[i].left=juce::jmax(meter.tracks[i].left,peaks[i].left*.86f);peaks[i].right=juce::jmax(meter.tracks[i].right,peaks[i].right*.86f);}
    masterPeak={juce::jmax(meter.master.left,masterPeak.left*.86f),juce::jmax(meter.master.right,masterPeak.right*.86f)};
    for(auto& s:mixer)s->repaint();for(auto& s:arrangement->rows)s->repaint();if(master)master->repaint();
    refresh();
}catch(const std::exception& e){message=label(e.what());repaint();}}

Strip::Strip(Desktop& d,std::optional<mrs::Id> id,bool small):gain(!small),target(id),owner(d),mini(small){
    pan.rotary=mini;gain.setTitle("Channel gain");pan.setTitle("Channel pan");
    gain.setDescription("Arrow keys adjust gain; Ctrl for fine adjustment");pan.setDescription("Arrow keys adjust pan; Ctrl for fine adjustment");
    mute.setTitle("Mute");solo.setTitle("Solo");arm.setTitle("Arm recording");monitor.setTitle("Input monitoring");
    for(auto* c:{static_cast<juce::Component*>(&gain),static_cast<juce::Component*>(&pan)})addAndMakeVisible(c);
    for(auto* b:{&mute,&solo,&arm,&monitor,&input,&inserts,&output,&sends,&add})addAndMakeVisible(b);
    mute.onClick=[this]{owner.run([&]{auto m=track().mix;m.mute=!m.mute;owner.app.set_track_mix(*target,m);});};
    solo.onClick=[this]{owner.run([&]{auto m=track().mix;m.solo=!m.solo;owner.app.set_track_mix(*target,m);});};
    arm.onClick=[this]{owner.run([&]{owner.app.set_track_armed(*target,!owner.app.track_armed(*target));sync();});};
    monitor.onClick=[this]{owner.run([&]{owner.app.set_track_monitoring(*target,!track().input_monitor);});};
    input.onClick=[this]{inputMenu();};inserts.onClick=[this]{owner.insertMenu(target);};add.onClick=inserts.onClick;
    output.onClick=[this]{if(target)owner.routeMenu(*target);else owner.textDialog("Master hardware outputs (1,2...)","1,2",[this](auto s){owner.app.set_hardware_output({},mrs::desktop::parse_outputs(s.toStdString()));});};
    sends.onClick=[this]{if(target)owner.sendsMenu(*target);};
    gain.preview=[this](double v){preview(true,v);};pan.preview=[this](double v){preview(false,v);};
    gain.commit=[this](double v){commit(true,v);};pan.commit=[this](double v){commit(false,v);};
    gain.cancel=pan.cancel=[this]{owner.cancelPreview();sync();};sync();
}
mrs::Track Strip::track() const{if(target)for(const auto& t:owner.project()->tracks)if(t.id==target)return t;return {};}
void Strip::mouseDown(const juce::MouseEvent&){owner.selectedTrack=target;owner.refresh();}
mrs::Track::Mix Strip::adjusted(bool volume,double v) const{auto m=track().mix;if(volume)m.gain=static_cast<float>(std::pow(10.,(-60+72*v)/20));else m.pan=static_cast<float>(2*v-1);return m;}
void Strip::preview(bool volume,double v){owner.setPreview(target,adjusted(volume,v),target?owner.project()->master_gain:adjusted(volume,v).gain,[this]{return active();});}
void Strip::commit(bool volume,double v){owner.run([&]{owner.finishPreview();if(target)owner.app.set_track_mix(*target,adjusted(volume,v));else owner.app.set_master_gain(adjusted(volume,v).gain);owner.app.poll();owner.refresh(true);});}
void Strip::sync(){const auto t=track();gain.setTitle(label(t.name)+" gain");pan.setTitle(label(t.name)+" pan");const auto mix=t.mix;float v=target?mix.gain:owner.project()->master_gain;
    if(!gain.dragging())gain.value=juce::jlimit(0.,1.,(20*std::log10(juce::jmax(.001f,v))+60)/72.);if(!pan.dragging())pan.value=(mix.pan+1)/2.;
    mute.setVisible(target.has_value());solo.setVisible(target.has_value());pan.setVisible(target.has_value());
    arm.setVisible(mini&&target&&t.kind==mrs::TrackKind::audio);monitor.setVisible(arm.isVisible());input.setVisible(arm.isVisible());
    inserts.setVisible(!mini);add.setVisible(!mini);output.setVisible(!mini);sends.setVisible(!mini&&target.has_value());
    mute.setToggleState(mix.mute,juce::dontSendNotification);solo.setToggleState(mix.solo,juce::dontSendNotification);
    arm.setToggleState(target&&owner.app.track_armed(*target),juce::dontSendNotification);monitor.setToggleState(t.input_monitor,juce::dontSendNotification);
    input.setButtonText(t.input==-2?"Input: default":t.input==-1?"Input: off":juce::String(t.input_stereo?"Stereo ":"Mono ")+juce::String(t.input+1));
    juce::String out="Out: Master";if(t.output)for(const auto& x:owner.project()->tracks)if(x.id==t.output)out="Out: "+label(x.name);
    if(!t.hardware_outputs.empty())out="Out: hardware";output.setButtonText(target?out:"Out: Default");
    sends.setButtonText("Sends ("+juce::String(static_cast<int>(t.sends.size()))+")");insertList();resized();repaint();gain.repaint();pan.repaint();
}
void Strip::insertList(){const auto chain=owner.chain(target);insertButtons.clear();if(mini)return;
    inserts.setButtonText("Inserts ("+juce::String(static_cast<int>(chain.size()))+")");
    for(const auto& n:chain){auto b=std::make_unique<juce::TextButton>((n.bypass?"[B] ":"")+insertName(n));const auto slot=n.id;
        b->onClick=[this,slot]{owner.openInsert(target,slot);};addAndMakeVisible(*b);insertButtons.push_back(std::move(b));}
}
void Strip::resized(){const int w=getWidth(),h=getHeight();if(mini){mute.setBounds(w-132,4,28,24);solo.setBounds(w-102,4,28,24);arm.setBounds(w-72,4,28,24);monitor.setBounds(w-42,4,28,24);
        gain.setBounds(8,32,w-70,36);pan.setBounds(w-64,34,58,32);input.setBounds(8,h-30,w-18,24);}
    else {inserts.setBounds(8,26,w-40,24);add.setBounds(w-30,26,24,24);
        for(std::size_t i=0;i<insertButtons.size();++i){insertButtons[i]->setVisible(i<3);insertButtons[i]->setBounds(8,52+static_cast<int>(i)*22,w-16,20);}
        const int effectHeight=juce::jmin(66,static_cast<int>(insertButtons.size())*22);
        pan.setBounds(8,56+effectHeight,w-16,30);gain.setBounds(w-62,90+effectHeight,56,juce::jmax(55,h-200-effectHeight));
        mute.setBounds(8,h-85,40,24);solo.setBounds(52,h-85,40,24);output.setBounds(8,h-56,w-16,23);sends.setBounds(8,h-30,w-16,23);}
}
void Strip::paint(juce::Graphics& g){g.fillAll(juce::Colour(surface));g.setColour(juce::Colour(accent));g.fillRect(0,0,mini?4:getWidth(),mini?getHeight():3);
    const auto t=track();g.setFont(owner.theme.font(12));g.setColour(target?juce::Colours::whitesmoke:juce::Colours::gold);
    g.drawText(target?label(t.name):"MASTER",8,3,mini?getWidth()-145:getWidth()-16,23,juce::Justification::left);
    mrs::audio::StereoPeak p=owner.masterPeak;if(target){const auto tracks=owner.project()->tracks;for(std::size_t i=0;i<tracks.size();++i)if(tracks[i].id==target)p=owner.peaks[i];}
    for(int i=0;i<2;++i){const float peak=i==0?p.left:p.right;const auto level=juce::jlimit(0.f,1.f,(20.f*std::log10(juce::jmax(.000001f,peak))+60.f)/60.f);
        const juce::Rectangle<int> r=mini?juce::Rectangle<int>(10,70+i*7,getWidth()-82,5):juce::Rectangle<int>(12+i*17,90+juce::jmin(66,static_cast<int>(insertButtons.size())*22),12,juce::jmax(25,getHeight()-202-juce::jmin(66,static_cast<int>(insertButtons.size())*22)));
        g.setColour(juce::Colour(0xff121619));g.fillRect(r);g.setColour(peak>.95f?juce::Colours::orange:juce::Colour(0xff45d899));
        if(mini)g.fillRect(r.withWidth(static_cast<int>(r.getWidth()*level)));else g.fillRect(r.withTop(r.getBottom()-static_cast<int>(r.getHeight()*level)));
    }
    g.setColour(juce::Colours::whitesmoke);g.drawText(juce::String(-60+gain.value*72,1)+" dB",mini?10:8,mini?82:getHeight()-110,100,20,juce::Justification::left);
}
void Strip::inputMenu(){if(!target)return;juce::PopupMenu m;m.addItem(1,"Default");m.addItem(2,"Off");const auto names=owner.app.input_names();
    for(std::size_t i=0;i<names.size();++i){m.addItem(10+static_cast<int>(i),"Mono "+label(names[i]));if(i+1<names.size())m.addItem(100+static_cast<int>(i),"Stereo "+label(names[i])+" / "+label(names[i+1]));}
    juce::Component::SafePointer<Strip> safe(this);m.showMenuAsync(popup(&input),[safe](int n){if(safe&&n)safe->owner.run([&]{safe->owner.app.set_track_input(*safe->target,n==1?-2:n==2?-1:n>=100?n-100:n-10,n>=100);safe->sync();});});
}
bool Strip::isInterestedInDragSource(const SourceDetails& s){const auto text=s.description.toString();const auto number=text.substring(5);return text.startsWith("vst3:")&&!mini&&!number.isEmpty()&&number.containsOnly("0123456789");}
void Strip::itemDropped(const SourceDetails& s){if(isInterestedInDragSource(s))owner.addPlugin(target,static_cast<std::size_t>(s.description.toString().substring(5).getIntValue()));}

Arrangement::Arrangement(Desktop& d):owner(d){setWantsKeyboardFocus(true);addAndMakeVisible(rowsBody);rowsBody.setInterceptsMouseClicks(false,true);}
void Arrangement::rebuild(){rows.clear();for(const auto& t:owner.project()->tracks){auto row=std::make_unique<Strip>(owner,t.id,true);rowsBody.addAndMakeVisible(*row);rows.push_back(std::move(row));}resized();}
void Arrangement::resized(){vertical=juce::jlimit(0.f,static_cast<float>(juce::jmax(0,static_cast<int>(rows.size())*trackHeight-(getHeight()-header))),vertical);
    rowsBody.setBounds(0,header,left-6,getHeight()-header);
    for(std::size_t i=0;i<rows.size();++i){const int y=static_cast<int>(i)*trackHeight-static_cast<int>(vertical);rows[i]->setBounds(0,y,left-6,trackHeight-4);rows[i]->setVisible(y+trackHeight>0 && y<rowsBody.getHeight());}}
int Arrangement::trackAt(float y) const{return static_cast<int>((y-header+vertical)/trackHeight);}
mrs::Sample Arrangement::sampleAt(float x) const{auto sample=static_cast<mrs::Sample>(juce::jmax(0.,(x-left+horizontal)/pixelsPerSecond)*owner.project()->sample_rate);
    if(owner.snap){mrs::Timeline time(owner.project()->time,owner.project()->sample_rate);auto tick=time.to_ticks(sample);sample=time.to_samples((tick/(mrs::ppq/4))*(mrs::ppq/4));}return sample;}
juce::Rectangle<float> Arrangement::clipRect(const mrs::Clip& c) const{int index=0;for(const auto& t:owner.project()->tracks){if(t.id==c.track)break;++index;}
    const double rate=owner.project()->sample_rate;return {static_cast<float>(left+c.start/rate*pixelsPerSecond-horizontal),static_cast<float>(header+index*trackHeight-vertical+22),static_cast<float>(c.length/rate*pixelsPerSecond),static_cast<float>(trackHeight-27)};}
void Arrangement::paint(juce::Graphics& g){g.fillAll(juce::Colour(surface));g.setFont(owner.theme.font(11));g.setColour(juce::Colours::lightgrey);
    const auto p=owner.project();mrs::Timeline time(p->time,p->sample_rate);
    g.drawText("Tracks / input / monitor",8,5,left-10,22,juce::Justification::left);
    for(int x=left;x<getWidth();x+=40){const auto sample=sampleAt(static_cast<float>(x));auto pos=time.musical_position(time.to_ticks(sample));g.setColour(juce::Colour(0xff4a5259));g.drawVerticalLine(x,20,static_cast<float>(getHeight()));g.setColour(juce::Colours::lightgrey);g.drawText(juce::String(pos.bar)+":"+juce::String(pos.beat),x+2,0,40,20,juce::Justification::left);}
    for(const auto& section:p->sections){const float x=static_cast<float>(left+static_cast<double>(time.to_samples(section.start))/p->sample_rate*pixelsPerSecond-horizontal);const float width=static_cast<float>(static_cast<double>(time.to_samples(section.end)-time.to_samples(section.start))/p->sample_rate*pixelsPerSecond);
        g.setColour(juce::Colour(section.color).withAlpha(1.f));g.fillRect(juce::Rectangle<float>(x,22,width,22));g.setColour(juce::Colours::white);g.drawText(label(section.name),static_cast<int>(x)+4,22,static_cast<int>(width)-4,22,juce::Justification::left);}
    for(const auto& chord:p->chords){int x=left+static_cast<int>(static_cast<double>(time.to_samples(chord.start))/p->sample_rate*pixelsPerSecond-horizontal);g.setColour(juce::Colours::lightgrey);g.drawText(label(chord.symbol),x,46,80,20,juce::Justification::left);}
    g.saveState();g.reduceClipRegion(left,header,getWidth()-left,getHeight()-header);
    for(std::size_t i=0;i<p->tracks.size();++i){int y=header+static_cast<int>(i)*trackHeight-static_cast<int>(vertical);g.setColour(juce::Colour(0xff465058));g.drawHorizontalLine(y,static_cast<float>(left),static_cast<float>(getWidth()));g.setColour(juce::Colours::lightgrey);g.drawText(label(p->tracks[i].name),left+8,y+2,200,20,juce::Justification::left);}
    for(const auto& stored:p->clips){auto c=stored;if(drag&&drag->id==c.id){const auto delta=dragStart-c.start;if(trim<0){const auto start=juce::jlimit<mrs::Sample>(0,c.start+c.length-1,c.start+delta);c.source_offset+=start-c.start;c.length-=start-c.start;c.start=start;}else if(trim>0)c.length=juce::jmax<mrs::Sample>(1,c.length+delta);else{c.start=dragStart;if(dragTrack)c.track=*dragTrack;}}
        auto r=clipRect(c);if(!r.intersects(getLocalBounds().toFloat()))continue;g.setColour(juce::Colour(c.id==owner.selectedClip?0xff3266ac:0xff294d7e));g.fillRect(r);g.setColour(juce::Colour(0xff8ac3ff));g.drawRect(r,1.5f);
        g.drawText(label(c.name),r.toNearestInt().withHeight(20).reduced(5,0),juce::Justification::left);
        if(const auto* wave=owner.app.waveform(c.source)){const int channels=static_cast<int>(wave->channels());for(int channel=0;channel<channels;++channel){const float lane=(r.getHeight()-22)/channels;const float centre=r.getY()+22+lane*(channel+.5f);
            for(int x=juce::jmax(left,static_cast<int>(r.getX()));x<juce::jmin(getWidth(),static_cast<int>(r.getRight()));++x){const auto begin=c.source_offset+static_cast<mrs::Sample>((x-r.getX())/pixelsPerSecond*p->sample_rate);const auto end=begin+juce::jmax<mrs::Sample>(1,static_cast<mrs::Sample>(p->sample_rate/pixelsPerSecond));auto peak=wave->range(begin,end,static_cast<std::uint32_t>(channel));const float top=centre-peak.maximum*lane*.45f;const float bottom=centre-peak.minimum*lane*.45f;g.fillRect(juce::Rectangle<float>(static_cast<float>(x),top,1,juce::jmax(1.f,bottom-top)));}}
        }
    }
    g.setColour(juce::Colours::white);const int playhead=left+static_cast<int>(static_cast<double>(owner.app.engine()->state().sample)/p->sample_rate*pixelsPerSecond-horizontal);g.drawVerticalLine(playhead,static_cast<float>(header),static_cast<float>(getHeight()));g.restoreState();
}
void Arrangement::mouseDown(const juce::MouseEvent& e){grabKeyboardFocus();if(e.position.x<left)return;const auto p=owner.project();if(e.position.y<header){owner.run([&]{owner.app.seek(sampleAt(e.position.x));});return;}
    const int index=trackAt(e.position.y);if(index>=0&&index<static_cast<int>(p->tracks.size()))owner.selectedTrack=p->tracks[static_cast<std::size_t>(index)].id;
    owner.selectedClip.reset();for(const auto& c:p->clips)if(clipRect(c).contains(e.position)){owner.selectedClip=c.id;drag=c;dragX=e.position.x;dragStart=c.start;auto r=clipRect(c);trim=e.position.x-r.getX()<7?-1:r.getRight()-e.position.x<7?1:0;break;}
    if(!owner.selectedClip)owner.run([&]{owner.app.seek(sampleAt(e.position.x));});repaint();
}
void Arrangement::mouseDrag(const juce::MouseEvent& e){if(!drag)return;const double delta=(e.position.x-dragX)/pixelsPerSecond*owner.project()->sample_rate;dragStart=juce::jmax<mrs::Sample>(0,drag->start+static_cast<mrs::Sample>(delta));const int at=trackAt(e.position.y);if(at>=0&&at<static_cast<int>(owner.project()->tracks.size()))dragTrack=owner.project()->tracks[static_cast<std::size_t>(at)].id;repaint();}
void Arrangement::mouseUp(const juce::MouseEvent& e){if(!drag)return;auto c=*drag;drag.reset();dragTrack.reset();if(std::abs(e.position.x-dragX)<2)return;owner.run([&]{
    if(trim){const auto delta=dragStart-c.start;if(trim<0)owner.app.trim_clip(c.id,juce::jlimit<mrs::Sample>(0,c.start+c.length-1,c.start+delta),c.start+c.length);else owner.app.trim_clip(c.id,c.start,juce::jmax(c.start+1,c.start+c.length+delta));}
    else {const int index=trackAt(e.position.y);auto tracks=owner.project()->tracks;if(index>=0&&index<static_cast<int>(tracks.size()))owner.app.move_clip(c.id,tracks[static_cast<std::size_t>(index)].id,owner.snap?sampleAt(static_cast<float>(left+static_cast<double>(dragStart)/owner.project()->sample_rate*pixelsPerSecond-horizontal)):dragStart);}
});}
void Arrangement::wheel(float delta,juce::ModifierKeys mods,float x){if(mods.isCtrlDown()){if(mods.isShiftDown()){const auto before=sampleAt(x);pixelsPerSecond=juce::jlimit(2.,2400.,pixelsPerSecond*std::pow(1.25,delta*4));horizontal=juce::jmax(0.,static_cast<double>(before)/owner.project()->sample_rate*pixelsPerSecond-(x-left));}
    else {const float before=(vertical+getHeight()/2.f-header)/trackHeight;trackHeight=juce::jlimit(128,360,trackHeight+static_cast<int>(delta*96));vertical=juce::jmax(0.f,before*trackHeight-(getHeight()/2.f-header));}}
    else if(mods.isShiftDown())horizontal=juce::jmax(0.,horizontal-delta*160);else vertical=juce::jmax(0.f,vertical-delta*128);
    resized();repaint();}
void Arrangement::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w){wheel(w.deltaY,e.mods,e.position.x);}
void Arrangement::zoom(double factor){pixelsPerSecond=juce::jlimit(2.,2400.,pixelsPerSecond*factor);repaint();}
void Arrangement::fit(){mrs::Sample end=48000*16;for(const auto& c:owner.project()->clips)end=juce::jmax(end,c.start+c.length);horizontal=0;pixelsPerSecond=juce::jlimit(2.,2400.,(getWidth()-left)*static_cast<double>(owner.project()->sample_rate)/end);repaint();}
bool Arrangement::isInterestedInFileDrag(const juce::StringArray& files){for(const auto& f:files)if(!f.endsWithIgnoreCase(".wav"))return false;return !files.isEmpty();}
void Arrangement::filesDropped(const juce::StringArray& files,int,int){owner.run([&]{owner.importFiles(files);});}

namespace {
class BrowserNode final : public juce::TreeViewItem {
public:
    BrowserNode(juce::String text,int id=-1):name(std::move(text)),plugin(id){}
    bool mightContainSubItems() override{return plugin<0;}
    juce::String getUniqueName() const override{return name;}
    void paintItem(juce::Graphics& g,int width,int height) override{g.setColour(juce::Colours::whitesmoke);g.setFont(13.f);g.drawText(name,2,0,width-4,height,juce::Justification::left);}
    juce::var getDragSourceDescription() override{return plugin<0?juce::var():juce::var("vst3:"+juce::String(plugin));}
    juce::String name;int plugin;
};
}
Browser::Browser(Desktop& d):owner(d){search.setTitle("Search VST3 plugins");search.setTextToShowWhenEmpty("Search VST3...",juce::Colours::grey);tree.setTitle("VST3 plugins by vendor");addAndMakeVisible(tree);addAndMakeVisible(scanButton);addAndMakeVisible(search);tree.setDefaultOpenness(false);tree.setRootItemVisible(false);
    search.setTextToShowWhenEmpty("Search VST3...",juce::Colours::grey);search.onTextChange=[this]{rebuild();};
    scanButton.onClick=[this]{owner.choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[this](const auto& f){owner.scan(f);});};rebuild();}
void Browser::resized(){scanButton.setBounds(8,28,getWidth()-16,28);search.setBounds(8,62,getWidth()-16,26);tree.setBounds(8,94,getWidth()-16,getHeight()-120);}
void Browser::paint(juce::Graphics& g){g.fillAll(juce::Colour(surface));g.setColour(juce::Colours::whitesmoke);g.setFont(owner.theme.font(12));g.drawText("VST3",8,2,100,24,juce::Justification::left);g.drawText("Drag an effect onto a mixer channel",8,getHeight()-23,getWidth()-16,20,juce::Justification::left);}
void Browser::rebuild(){tree.setRootItem(nullptr);root=std::make_unique<BrowserNode>("root");std::map<std::string,BrowserNode*> vendors;
    for(std::size_t i=0;i<owner.catalog.size();++i){const auto& p=owner.catalog[i];if(!search.getText().isEmpty()&&!label(p.name+" "+p.vendor).containsIgnoreCase(search.getText()))continue;
        if(!vendors.contains(p.vendor)){auto* node=new BrowserNode(label(p.vendor));root->addSubItem(node);node->setOpen(false);vendors[p.vendor]=node;}vendors[p.vendor]->addSubItem(new BrowserNode(label(p.name),static_cast<int>(i)));}
    tree.setRootItem(root.get());root->setOpen(true);
}
void Desktop::scan(const juce::File& root){if(scanner.valid())throw std::runtime_error("Scan already running");*scanCancel=false;const auto folder=path(root);const auto helper=path(juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("mrs_vst3_scan.exe"));
    auto cache=cacheFile;auto cancel=scanCancel;scanner=std::async(std::launch::async,[folder,helper,cache,cancel]{return mrs::processing::scan_vst3(folder,helper,cache,cancel);});}
std::vector<mrs::NativeInsert> Desktop::chain(std::optional<mrs::Id> t) const{const auto p=project();if(!t)return p->master_inserts;for(const auto& track:p->tracks)if(track.id==t)return track.inserts;throw std::runtime_error("Track no longer exists");}
void Desktop::closeEditors(){app.close_plugin_editors();for(auto& w:windows){retirePluginWindow(*w);w->setVisible(false);
    // Retire the native parent HWND while its plugin module is still alive.
    // Keep the C++ window until the next timer turn so an editor callback can return safely.
    if(static_cast<bool>(w->getProperties()["mrs-native-editor"]))w->removeFromDesktop();}}
void Desktop::applyChain(std::optional<mrs::Id> target,std::vector<mrs::NativeInsert> effects){app.set_inserts(target,std::move(effects));editorGeneration=app.insert_generation();refresh(true);}
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
