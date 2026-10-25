#include "MusicalGrid.h"
#include "MeterTrack.h"
#include "Desktop.h"
#include "LibraryFoundationPanel.h"
#include <MRTrackHeaderAssets.h>
#include "MRControlBinding.h"
#include "ChordTrack.h"
#include "SectionTrack.h"
#include "TempoTrack.h"
#include "MarkerTrack.h"
#include "ProjectHome.h"
#include "LiveWorkspace.h"
#include "WaveformView.h"
#include <fstream>
#include <charconv>
#include <cmath>
#include <sstream>

namespace ui {
namespace {
class StudioAlertWindow final : public juce::AlertWindow {
public:
    StudioAlertWindow(Desktop& owner,const juce::String& title,const juce::String& message,juce::MessageBoxIconType icon)
        :juce::AlertWindow(title,message,icon,&owner){
        if(owner.assetSkin()){
            auto tokens=DesignTokens::target();tokens.colours[static_cast<std::size_t>(ColourRole::raised)]=ConceptSkin::channelButton;
            dialogTheme.applyPreview(tokens);
            dialogTheme.setColour(juce::AlertWindow::backgroundColourId,tokens.colour(ColourRole::window));
            dialogTheme.setColour(juce::AlertWindow::textColourId,tokens.colour(ColourRole::text));
            dialogTheme.setColour(juce::AlertWindow::outlineColourId,tokens.colour(ColourRole::border));
            dialogTheme.setColour(juce::TextEditor::focusedOutlineColourId,tokens.colour(ColourRole::focus));
            setLookAndFeel(&dialogTheme);
        }
    }
    ~StudioAlertWindow() override {setLookAndFeel(nullptr);}
private:
    Theme dialogTheme;
};
juce::String label(const std::string& s){return juce::String::fromUTF8(s.c_str());}
std::filesystem::path path(const juce::File& f){return std::filesystem::path(f.getFullPathName().toWideCharPointer());}
juce::String insertName(const mrs::NativeInsert& n){
    if(n.kind==mrs::InsertKind::vst3)return label(n.plugin_name);
    if(n.kind==mrs::InsertKind::channel_eq)return "Channel EQ";
    if(n.kind==mrs::InsertKind::cab_ir)return "Cab IR";
    if(n.kind==mrs::InsertKind::rig)return "MR Rig";
    if(n.kind==mrs::InsertKind::cab_collection)return "MR Rig Models";
    if(n.kind==mrs::InsertKind::reverb)return "MR Reverb";
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
void EditorWindow::closeButtonPressed(){if(onClose)onClose();if(static_cast<bool>(getProperties().getWithDefault("mrs-hide-on-close",true)))setVisible(false);}
void EditorWindow::fitNativeEditor(int width,int height){
    const double scale=getPeer()?getPeer()->getPlatformScaleFactor():1.;
    if(width<1||height<1||width>4096||height>4096||!std::isfinite(scale)||scale<=0)
        throw std::runtime_error("Invalid native editor dimensions");
    setResizeLimits(1,1,8192,8192);
    setContentComponentSize(juce::jmax(1,juce::roundToInt(width/scale)),juce::jmax(1,juce::roundToInt(height/scale)));
}

Desktop::Desktop(bool test,bool startAtHome):testing(test){
#ifdef MRS_HAS_ARA
    juce::Component::SafePointer<Desktop> araOwner(this);
    app.set_ai_ara_editor_handler([araOwner](mrs::Id id){if(!araOwner)throw std::runtime_error("Editor owner closed");const auto p=araOwner->project();const auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip){return clip.id==id;});if(c==p->clips.end())throw std::runtime_error("ARA target changed");araOwner->selectedClip=id;araOwner->selectedTrack=c->track;araOwner->openAra(id);araOwner->refresh();},[araOwner](mrs::Id id){if(araOwner){const auto p=araOwner->project();const auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip){return clip.id==id;});if(c!=p->clips.end())araOwner->closeAra(c->source);}});
#endif
    getProperties().set("mrPopupRoot",true);setLookAndFeel(&theme);setWantsKeyboardFocus(true);setTitle("Moon River Studio workspace");setFocusContainerType(FocusContainerType::keyboardFocusContainer);
    const auto config=juce::File(juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA",juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getFullPathName())).getChildFile("MoonRiverStudio");
    viewFile=path(config.getChildFile("juce-view.json"));prefsFile=path(config.getChildFile("juce-preferences.json"));cacheFile=path(config.getChildFile("vst3.cache"));
#ifdef MRS_HAS_ARA
    if(!testing)araCatalogFile=path(config.getChildFile("ara-catalog.json"));
    if(!testing)try{loadAraCatalog();}catch(const std::exception& e){message=label(e.what());}
#endif
    if(!testing)try{const auto readPrefs=std::filesystem::exists(prefsFile)?prefsFile:path(config.getChildFile("desktop.cfg"));std::ifstream in(readPrefs,std::ios::binary);if(in){std::string bytes((std::istreambuf_iterator<char>(in)),{});prefs=mrs::desktop::decode_preferences(bytes);}
        catalog=mrs::processing::load_vst3_cache(cacheFile);}catch(const std::exception& e){message=label(e.what());}
    if(!testing)try{juce::File file(juce::String(viewFile.wstring().c_str()));if(file.existsAsFile())view=ViewSettings::decode(file.loadFileAsString());}catch(const std::exception& e){message=label(e.what());}
    sidebar=view.sidebar;browserWidth=view.browserWidth;snap=view.snap;
    if(view.cabRoot.isNotEmpty())app.set_cab_root(path(juce::File(view.cabRoot)),view.cabManifest.toStdString());
    if(view.reverbRoot.isNotEmpty())app.set_reverb_root(path(juce::File(view.reverbRoot)));
    projectSessionStarted=testing&&!startAtHome;if(projectSessionStarted)app.demo();else app.new_project();resetDevice();
    app.set_click_settings(prefs.click);
    app.set_ai_permissions(prefs.ai);
    arrangement=std::make_unique<Arrangement>(*this);browser=std::make_unique<Browser>(*this);
    // Normal Studio/Live/Release startup uses the user-approved v1 layout.
    // This is embedded in the binary and has no filesystem/Hot Reload dependency.
    restoreBundledTrackHeaderLayout();
    addAndMakeVisible(browserDivider);browserDivider.setTitle("Resize browser");browserDivider.setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);browserDivider.addMouseListener(this,false);
    addAndMakeVisible(mixerDivider);mixerDivider.setTitle("Resize mixer");mixerDivider.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);mixerDivider.addMouseListener(this,false);
    for(auto* c:{static_cast<juce::Component*>(&menu),static_cast<juce::Component*>(arrangement.get()),static_cast<juce::Component*>(browser.get()),static_cast<juce::Component*>(&mixerViewport)})addAndMakeVisible(c);
    mixerBody.addMouseListener(this,false);mixerViewport.setViewedComponent(&mixerBody,false);mixerViewport.setScrollBarsShown(false,true);
    addAndMakeVisible(projectTitle);projectTitle.setJustificationType(juce::Justification::centred);projectTitle.setInterceptsMouseClicks(false,false);projectTitle.setFont(theme.font(13));
    for(auto* component:{static_cast<juce::Component*>(&audioCpuBar),static_cast<juce::Component*>(&cpuReadout),static_cast<juce::Component*>(&latencyReadout)})addAndMakeVisible(component);
    addChildComponent(shellStatus);addChildComponent(shellTime);shellStatus.setComponentID("shell-transport-state");shellStatus.setTitle("Transport and device status");shellStatus.setInterceptsMouseClicks(false,false);shellTime.setComponentID("shell-time");shellTime.setTitle("Transport position in seconds");shellTime.setInterceptsMouseClicks(false,false);
    cpuReadout.setFont(theme.font(12));latencyReadout.setFont(theme.font(12));
    cpuReadout.setTitle("Audio CPU load percent");audioCpuBar.setTitle("Audio processing budget usage");latencyReadout.setTitle("Device input and output latency in milliseconds");
    cpuReadout.setDescription("Audio callback CPU load as a percentage of the buffer processing deadline; not whole-computer CPU usage.");
    latencyReadout.setDescription("Driver-reported input/output latency. Plugin delay compensation is shown separately as PDC.");
    audioCpuBar.setPercentageDisplay(false);audioCpuBar.setColour(juce::ProgressBar::backgroundColourId,juce::Colour(0xff121619));
    updatePerformance(app.device_status(),false);
    for(auto* b:{&play,&pause,&stop,&record,&previous,&next,&loop,&editButton,&mixButton,&brows,&addTrack,&addBus,&undo,&redo,&split,&duplicate,&remove,&zoomIn,&zoomOut,&fit,&snapButton})addAndMakeVisible(b);
    auto bind=[this](juce::TextButton& b,int cmd){b.onClick=[this,cmd]{action(cmd);};};
    bind(play,30);bind(pause,31);bind(stop,32);bind(record,33);bind(previous,34);bind(next,35);bind(loop,36);
    addAndMakeVisible(pointerTool);addAndMakeVisible(knifeTool);pointerTool.getProperties().set("mrHostIcon","tool_pointer");knifeTool.getProperties().set("mrHostIcon","tool_split");split.getProperties().set("mrHostIcon","tool_split");pointerTool.setTooltip("Pointer (1)");knifeTool.setTooltip("Split (3): click a clip, chord or section");pointerTool.onClick=[this]{setKnifeMode(false);};knifeTool.onClick=[this]{setKnifeMode(true);};pointerTool.setToggleState(true,juce::dontSendNotification);for(auto* tool:{&pointerTool,&knifeTool,&duplicate})tool->getProperties().set("mrSmallToolbarIcon",true);
    for(auto item:{std::pair{&undo,"action_undo"},std::pair{&redo,"action_redo"},std::pair{&duplicate,"action_duplicate"},std::pair{&zoomIn,"action_zoom_in"},std::pair{&zoomOut,"action_zoom_out"},std::pair{&fit,"action_fit"},std::pair{&snapButton,"action_snap"},std::pair{&clickButton,"metronome"},std::pair{&countButton,"count_off"}}){item.first->getProperties().set("mrHostIcon",item.second);item.first->getProperties().set("mrSmallToolbarIcon",true);}
    bind(addTrack,20);bind(addBus,21);bind(undo,10);bind(redo,11);bind(split,24);bind(duplicate,45);bind(remove,25);duplicate.setComponentID("duplicate-clip");duplicate.setTooltip("Duplicate (D)");
    for(auto* b:{&clickButton,&countButton}){addAndMakeVisible(b);b->setTitle(b==&clickButton?"Toggle metronome":"Toggle recording count-in");b->setTooltip("Toggle on/off. Settings: Transport → Metronome / count-in");}
    clickButton.onClick=[this]{action(503);};countButton.onClick=[this]{action(504);};
    editButton.onClick=[this]{run([&]{toggleEditor();});};
    mixButton.onClick=[this]{setWorkspace(app.workspace()==mrs::desktop::Workspace::mix?mrs::desktop::Workspace::arrange:mrs::desktop::Workspace::mix);};
    brows.onClick=[this]{sidebar=!sidebar;resized();repaint();};
    zoomIn.onClick=[this]{arrangement->zoom(1.25);};zoomOut.onClick=[this]{arrangement->zoom(.8);};fit.onClick=[this]{arrangement->fit();};
    snapButton.setButtonText(snap ? "Snap smart" : "Snap off");snapButton.setToggleState(snap,juce::dontSendNotification);
    arrangement->trackHeight=view.trackHeight;arrangement->pixelsPerSecond=view.pixelsPerSecond;
    addAndMakeVisible(navigationMode);navigationMode.addItem("Sections",1);navigationMode.addItem("Markers",2);navigationMode.setSelectedId(1,juce::dontSendNotification);navigationMode.setTitle("Navigation target");navigationMode.setComponentID("navigation-mode");
    previous.onClick=[this]{action(navigationMode.getSelectedId()==2?48:34);};next.onClick=[this]{action(navigationMode.getSelectedId()==2?49:35);};
    navigationMode.onChange=[this]{const bool markers=navigationMode.getSelectedId()==2;previous.setTooltip(markers?"Previous marker":"Previous section");next.setTooltip(markers?"Next marker":"Next section");};
    contextLabels[1].addMouseListener(this,false);contextLabels[2].addMouseListener(this,false);
    int focus=1;for(auto* b:{&undo,&redo,&addTrack,&addBus,&split,&duplicate,&remove,&zoomIn,&zoomOut,&fit,&snapButton,&play,&pause,&stop,&record,&previous,&next,&loop,&editButton,&mixButton,&brows}){b->setTitle(b->getButtonText());b->setTooltip(b->getButtonText());b->setExplicitFocusOrder(focus++);}
    snapButton.setTitle("Smart snap");snapButton.setTooltip(snap?"Snap enabled":"Snap disabled");snapButton.onClick=[this]{snap=!snap;snapButton.setButtonText(snap ? "Snap smart" : "Snap off");snapButton.setToggleState(snap,juce::dontSendNotification);snapButton.setTooltip(snap?"Snap enabled":"Snap disabled");};
    for(std::size_t i=0;i<contextLabels.size();++i){auto& l=contextLabels[i];addAndMakeVisible(l);l.setComponentID("musical-context-"+juce::String(static_cast<int>(i)));l.setFont(theme.font(12));l.setColour(juce::Label::textColourId,juce::Colours::lightgrey);l.setInterceptsMouseClicks(true,false);}
    setSize(1400,850);refresh(true);setWorkspace(testing?mrs::desktop::Workspace::mix:prefs.workspace==mrs::desktop::Workspace::live?mrs::desktop::Workspace::arrange:prefs.workspace);
    liveWorkspace=std::make_unique<LiveWorkspace>(*this);addChildComponent(*liveWorkspace);
    home=std::make_unique<ProjectHome>(*this);addChildComponent(*home);if(!testing||startAtHome){showProjectHome(true);if(!testing)home->rescan();}startTimerHz(30);
}
Desktop::~Desktop(){
    araDockResizeFrame={};pendingAraDockHeight.reset();resizingAraDock=nullptr;
#ifdef MRS_HAS_ARA
    *araScanCancel=true;
#endif
setAssetDesign(false);setShellDesign(false);juce::PopupMenu::dismissAllActiveMenus();try{saveSettings();}catch(...){}stopTimer();*scanCancel=true;chooser.reset();closeEditors();dockedEditor.reset();home.reset();liveWorkspace.reset();
    if(scanner.valid())scanner.wait();setLookAndFeel(nullptr);mixerViewport.setViewedComponent(nullptr,false);}
void Desktop::run(std::function<void()> f){try{performAraOperation(*this,f);message.clear();if(!app.recording()){recordingPreview.clear();audioRecordingPreview.clear();}else if(recordingPreview.empty()&&audioRecordingPreview.empty())updateRecordingPreview();refresh();}catch(const std::exception& e){mrs::diagnostics::event(std::string("ui_error ")+e.what());refresh();message=label(e.what());repaint();}}
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
void Desktop::remember(){if(testing)return;mrs::desktop::remember_project(prefs,path(documentFile()));saveSettings();}
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
void Desktop::openFile(const juce::File& f){mrs::diagnostics::event("project_open_begin "+f.getFullPathName().toStdString());const bool session=app.audio_name()!="Offline clock (no sound)"&&app.audio_name()!="Disconnected";closeEditors();cancelPreview();if(f.hasFileExtension("mrlive")){
    auto showState=LiveDocument::read(f);
    if(showState.active>=0)app.open_project(path(showState.projects.at(static_cast<std::size_t>(showState.active))));else app.new_project(session?prefs.rate:48000);
    liveDocument=std::move(showState);liveFile=f;liveDirty=false;app.workspace(mrs::desktop::Workspace::live);
}else{const bool leavingLive=liveDocument.has_value()||app.workspace()==mrs::desktop::Workspace::live;app.open_project(path(f));liveDocument.reset();liveFile={};liveDirty=false;if(leavingLive)app.workspace(mrs::desktop::Workspace::arrange);}
resetDevice();selectedClip.reset();selectedTrack.reset();remember();refresh(true);arrangement->fit();showProjectHome(false);reconnectDevice(session);if(liveDocument&&liveDocument->active<0)liveEmptyRevision=std::pair{app.services().projects->state().revision,app.graphs()->state().revision};else liveEmptyRevision.reset();mrs::diagnostics::event("project_open_end");}
bool Desktop::songDirty() const{
    if(liveDocument&&liveDocument->active<0&&liveEmptyRevision&&
       liveEmptyRevision->first==app.services().projects->state().revision&&
       liveEmptyRevision->second==app.graphs()->state().revision)return false;
    return app.dirty();
}
juce::File Desktop::documentFile() const{return liveDocument?liveFile:juce::File(juce::String(app.path().wstring().c_str()));}
void Desktop::createLive(const juce::File& requested){
    auto file=requested.withFileExtension("mrlive");LiveDocument showState;showState.title=file.getFileNameWithoutExtension();showState.write(file);
    const bool session=app.audio_name()!="Offline clock (no sound)"&&app.audio_name()!="Disconnected";
    closeEditors();cancelPreview();app.new_project(session?prefs.rate:48000);liveDocument=std::move(showState);liveFile=file;liveDirty=false;
    resetDevice();app.workspace(mrs::desktop::Workspace::live);remember();refresh(true);showProjectHome(false);reconnectDevice(session);
    liveEmptyRevision=std::pair{app.services().projects->state().revision,app.graphs()->state().revision};
}
void Desktop::selectLiveProject(int index){
    if(!liveDocument)throw std::runtime_error("No MR Live show is open");
    const auto file=liveDocument->projects.at(static_cast<std::size_t>(index));
    const bool session=app.audio_name()!="Offline clock (no sound)"&&app.audio_name()!="Disconnected";
    closeEditors();cancelPreview();app.open_project(path(file));resetDevice();liveDocument->active=index;liveDirty=true;
    selectedClip.reset();selectedTrack.reset();refresh(true);reconnectDevice(session);
}
void Desktop::addLiveProject(const juce::File& file){
    if(!liveDocument||!file.hasFileExtension("mrsproject"))throw std::runtime_error("Select a Studio project for this show");
    const bool session=app.audio_name()!="Offline clock (no sound)"&&app.audio_name()!="Disconnected";
    closeEditors();cancelPreview();app.open_project(path(file));resetDevice();
    liveDocument->projects.push_back(file);liveDocument->active=static_cast<int>(liveDocument->projects.size())-1;liveDirty=true;
    selectedClip.reset();selectedTrack.reset();refresh(true);reconnectDevice(session);
}
void Desktop::saveFile(const juce::File& f){
    if(liveDocument){
        auto showState=*liveDocument;auto file=f.withFileExtension("mrlive");
        if(songDirty()){
            liveDirty=true; // Keep unsaved references pending if publishing the show fails.
            auto song=app.path();
            if(song.empty()){
                auto candidate=file.getSiblingFile(file.getFileNameWithoutExtension()+" song.mrsproject");
                candidate=candidate.getNonexistentSibling();song=mrs::desktop::project_folder_file(path(candidate));
                if(std::filesystem::exists(song))throw std::runtime_error("Song destination exists; choose another show filename with Save as");
            }
            app.save_project(song);const juce::File saved(juce::String(app.path().wstring().c_str()));
            if(showState.active<0){showState.projects.push_back(saved);showState.active=static_cast<int>(showState.projects.size())-1;}
            else showState.projects.at(static_cast<std::size_t>(showState.active))=saved;
        }
        if(showState.active<0&&!app.path().empty()){
            showState.projects.emplace_back(juce::String(app.path().wstring().c_str()));showState.active=static_cast<int>(showState.projects.size())-1;
        }
        showState.write(file);liveDocument=std::move(showState);liveFile=file;liveDirty=false;remember();refresh();return;
    }
    auto destination=path(f);if(destination!=app.path())destination=mrs::desktop::project_folder_file(destination);app.save_project(destination);remember();refresh();}
void Desktop::importFiles(const juce::StringArray& files){closeEditors();std::vector<std::filesystem::path> paths;for(const auto& f:files)paths.push_back(path(juce::File(f)));app.queue_wav_import(paths);refresh(true);}
void Desktop::choose(int chooserOptions,std::function<void(const juce::File&)> callback,juce::String pattern){
    const auto studio=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MR Studio").getChildFile("Projects");
    chooser=std::make_unique<juce::FileChooser>("Moon River Studio",app.path().empty()?studio:juce::File(juce::String(app.path().wstring().c_str())),pattern,true);
    juce::Component::SafePointer<Desktop> safe(this);
    chooser->launchAsync(chooserOptions,[safe,callback=std::move(callback)](const juce::FileChooser& result){
        if(safe && result.getResult()!=juce::File())safe->run([&]{callback(result.getResult());});
    });
}
void Desktop::textDialog(juce::String title,juce::String initial,std::function<void(juce::String)> callback){
    auto* alert=new StudioAlertWindow(*this,title,"",juce::MessageBoxIconType::NoIcon);
    alert->addTextEditor("value",initial);alert->addButton("OK",1,juce::KeyPress(juce::KeyPress::returnKey));alert->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<Desktop> safe(this);
    alert->enterModalState(true,juce::ModalCallbackFunction::create([safe,alert,callback](int r){if(safe && r==1)safe->run([&]{callback(alert->getTextEditorContents("value"));});}),true);
}
void Desktop::renameClips(std::vector<mrs::Id> clipIds){
    if(clipIds.empty())return;const auto store=app.services().projects;const auto revisionAtDialog=store->state().revision;const auto p=project();const auto first=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==clipIds.front();});if(first==p->clips.end())return;
    juce::Component::SafePointer<Desktop> safe(this);textDialog(clipIds.size()>1?"Rename clips: base name + 01, 02...":"Rename clip",label(first->name),[safe,store,revisionAtDialog,clipIds](auto text){if(!safe)return;safe->run([&]{if(safe->app.services().projects!=store||store->state().revision!=revisionAtDialog)throw std::runtime_error("Project changed; retry rename");if(text.trim().isNotEmpty())safe->app.rename_clips(clipIds,text.trim().toStdString());});});
}
void Desktop::renameTrack(mrs::Id id){
    auto& strips=app.workspace()==mrs::desktop::Workspace::mix?mixer:arrangement->rows;
    for(auto& strip:strips)if(strip->target==id){strip->beginNameEdit();return;}
}
void Desktop::confirmDiscard(std::function<void()> nextAction){if(app.recording()){run([&]{app.pause();});if(app.recording()||!message.isEmpty())return;}if(!projectSessionStarted||!documentDirty()){nextAction();return;}
    auto* alert=new StudioAlertWindow(*this,"Save project changes?",label(project()->title),juce::MessageBoxIconType::NoIcon);
    alert->addButton("Save",1);alert->addButton("Discard",2);alert->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<Desktop> safe(this);alert->enterModalState(true,juce::ModalCallbackFunction::create([safe,nextAction](int n){if(!safe)return;
        if(n==2)nextAction();else if(n==1)safe->run([&]{if(safe->documentFile()!=juce::File()){safe->saveFile(safe->documentFile());nextAction();}
            else safe->choose(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe,nextAction](auto f){if(safe){safe->saveFile(f.withFileExtension("mrsproject"));nextAction();}},"*.mrsproject");});
    }),true);
}
juce::StringArray Desktop::getMenuBarNames(){return {"File","Options","Edit","Track","Transport","AI: "+label(std::string(app.ai_permissions().enabled?mrs::ai::permission_name(app.ai_permissions().mode):std::string_view("OFF")))};}
juce::PopupMenu Desktop::getMenuForIndex(int n,const juce::String&){juce::PopupMenu m;
    if(n==0){m.addItem(701,"New MR Live show...");m.addItem(9,"MR Live workspace",liveDocument.has_value());m.addSeparator();}
    if(n==1){
        m.addItem(710,"Load Track Header JSON...",!app.recording());
        m.addItem(711,"Hot Reload Track Header JSON",trackHeaderLayoutSource!=juce::File() && !app.recording(),
                  trackHeaderHotReload);
        m.addItem(712,"Restore Default Track Header",trackHeaderLayoutSource!=juce::File() && !app.recording());
        return m;
    }
    // Existing menus retain their action IDs after inserting Options next to File.
    const int originalIndex = n > 1 ? n - 1 : n;
    if(originalIndex==0){m.addItem(6,"Project home");m.addItem(8,"Close project",projectSessionStarted);m.addItem(1,"New project");m.addItem(2,"Open project...");m.addItem(3,"Save");m.addItem(4,"Save as...");m.addItem(5,"Import WAV...");
        juce::PopupMenu recent;for(std::size_t i=0;i<prefs.recent_projects.size();++i)recent.addItem(1000+static_cast<int>(i),label(prefs.recent_projects[i]));m.addSubMenu("Open recent project",recent);m.addSeparator();m.addItem(7,"Open diagnostics folder");}
    if(originalIndex==1){m.addItem(10,"Undo",app.services().projects->state().can_undo);m.addItem(11,"Redo",app.services().projects->state().can_redo);m.addItem(24,"Split selected clip (S)");m.addItem(25,"Delete selected clip (Backspace)");m.addItem(43,"Create MIDI clip at cursor",selectedTrack.has_value());m.addItem(44,"Edit MIDI clip notes",selectedClip.has_value());m.addItem(45,"Duplicate selected clip (D)",selectedClip.has_value());m.addItem(52,"Merge MIDI clips (G)",selectedClips.size()>=2);m.addItem(46,"Loop selected clip",selectedClip.has_value());}
    if(originalIndex==2){m.addItem(20,"Add audio track");m.addItem(28,"Add instrument track");m.addItem(21,"Add bus");m.addItem(22,"Rename track");m.addItem(23,"Delete track");m.addItem(26,"Move track up");m.addItem(27,"Move track down");}
    if(originalIndex==3){m.addItem(30,"Play");m.addItem(31,"Pause");m.addItem(32,"Stop");m.addItem(33,"Record");m.addItem(34,"Previous section");m.addItem(35,"Next section");m.addItem(36,"Loop section");m.addItem(48,"Previous marker",!app.recording());m.addItem(49,"Next marker",!app.recording());m.addItem(50,"Go to section...",!app.recording());m.addItem(51,"Go to marker...",!app.recording());m.addItem(40,"Audio settings...");m.addItem(41,"Engine profiling...");m.addItem(42,"MIDI panic / all notes off");m.addItem(47,"Project tempo...");
        const auto c=app.click_settings();juce::PopupMenu click,count,level;const bool enabled=!app.recording();
        click.addItem(500,"During playback",enabled,c.playback);click.addItem(501,"During recording",enabled,c.recording);click.addItem(502,"Accent first beat",enabled,c.accent);
        for(const auto value:{10,20,35,50,75,100})level.addItem(600+value,juce::String(value)+"%",enabled,c.level==value);click.addSubMenu("Level",level);
        for(int bars=0;bars<=4;++bars)count.addItem(510+bars,bars?juce::String(bars)+" bar(s)":"Off",enabled,c.count_bars==bars);
        click.addSubMenu("Count-in before recording",count);m.addSubMenu("Metronome / count-in",click);}
    if(originalIndex==4){m.addItem(54,"Assistant...");m.addItem(52,"Permissions...");m.addItem(53,"Marker Apply requires preview",false);}
    return m;
}
void Desktop::selectTrack(mrs::Id id,juce::ModifierKeys mods){
    const auto p=project();
    if(selectedTracks.empty()&&selectedTrack)selectedTracks.insert(selectedTrack->value);
    if(mods.isShiftDown()&&selectedTrack){auto a=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==*selectedTrack;});auto b=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==id;});
        if(a!=p->tracks.end()&&b!=p->tracks.end()){if(a>b)std::swap(a,b);for(;a<=b;++a)selectedTracks.insert(a->id.value);}}
    else if(mods.isCommandDown()){if(selectedTracks.erase(id.value)){if(selectedTrack==id)selectedTrack=selectedTracks.empty()?std::nullopt:std::optional<mrs::Id>{mrs::Id{*selectedTracks.begin()}};return;}selectedTracks.insert(id.value);}
    else {selectedTracks.clear();selectedTracks.insert(id.value);}
    selectedTrack=id;
}
std::vector<mrs::Id> Desktop::folderSelection(std::optional<mrs::Id> fallback) const{
    std::vector<mrs::Id> result;
    if(fallback&&!selectedTracks.contains(fallback->value))return {*fallback};
    for(const auto& track:project()->tracks)if(selectedTracks.contains(track.id.value))result.push_back(track.id);
    if(result.empty()&&fallback)result.push_back(*fallback);return result;
}
void Desktop::showFolderMenu(mrs::Id id,juce::Point<int> position){
    const auto p=project();const auto at=std::find_if(p->folders.begin(),p->folders.end(),[&](const auto& f){return f.id==id;});if(at==p->folders.end())return;
    const auto folder=*at;juce::PopupMenu folderPopup;folderPopup.setLookAndFeel(&theme);const bool enabled=!app.recording()&&!busyGesture();
    folderPopup.addItem(1,folder.collapsed?"Expand folder":"Collapse folder",enabled);folderPopup.addItem(2,"Rename folder...",enabled);
    folderPopup.addItem(3,"Move selected tracks into folder",enabled&&!folderSelection(selectedTrack).empty());
    folderPopup.addSeparator();folderPopup.addItem(4,"Create and link Bus",enabled&&!folder.bus);juce::PopupMenu buses;std::vector<mrs::Id> busIds;
    for(const auto& track:p->tracks)if(track.kind==mrs::TrackKind::bus){busIds.push_back(track.id);buses.addItem(100+static_cast<int>(busIds.size()),label(track.name),true,folder.bus==track.id);}
    folderPopup.addSubMenu("Link existing Bus",buses,enabled&&!busIds.empty());folderPopup.addItem(5,"Detach Bus (keep routing)",enabled&&folder.bus.has_value());
    folderPopup.addSeparator();folderPopup.addItem(6,"Dissolve folder (keep tracks and routing)",enabled);
    juce::Component::SafePointer<Desktop> safe(this);const auto store=app.services().projects;
    folderPopup.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({position.x,position.y,1,1}),[safe,store,folder,busIds](int choice){if(!safe||!choice||safe->app.services().projects!=store)return;safe->run([&]{
        if(choice==1)safe->app.update_track_folder(folder.id,folder.name,!folder.collapsed);
        else if(choice==2)safe->textDialog("Rename track folder",label(folder.name),[safe,store,folder](auto text){if(safe&&safe->app.services().projects==store&&text.trim().isNotEmpty())safe->app.update_track_folder(folder.id,text.trim().toStdString(),folder.collapsed);});
        else if(choice==3)safe->app.move_tracks_to_folder(safe->folderSelection(safe->selectedTrack),folder.id);
        else if(choice==4){safe->closeEditors();safe->app.link_folder_bus(folder.id,{},true);}
        else if(choice==5)safe->app.link_folder_bus(folder.id,{});
        else if(choice==6)safe->app.remove_track_folder(folder.id);
        else if(choice>=101&&choice<101+static_cast<int>(busIds.size())){safe->closeEditors();safe->app.link_folder_bus(folder.id,busIds[static_cast<std::size_t>(choice-101)]);}
    });});
}
juce::PopupMenu Desktop::channelMenu(std::optional<mrs::Id> target){
    juce::PopupMenu channelPopup;channelPopup.setLookAndFeel(&theme);const bool enabled=!app.recording()&&!busyGesture();
    channelPopup.addItem(20,"Add audio channel",enabled);channelPopup.addItem(29,"Add FX channel",enabled);channelPopup.addItem(21,"Add Bus channel",enabled);
    if(target){for(const auto& t:project()->tracks)if(t.id==*target){channelPopup.addSeparator();channelPopup.addItem(23,t.fx_channel?"Del FX channel":t.kind==mrs::TrackKind::bus?"Del Bus channel":"Del channel",enabled);break;}}
    channelPopup.addSeparator();channelPopup.addItem(70,"Group selected tracks in folder...",enabled&&!folderSelection(target?target:selectedTrack).empty());
    if(target){channelPopup.addItem(71,"Move selected tracks out of folder",enabled);juce::PopupMenu folders;int n=0;for(const auto& f:project()->folders)folders.addItem(2000+n++,label(f.name));channelPopup.addSubMenu("Move selected tracks into folder",folders,enabled&&n>0);}
    return channelPopup;
}
void Desktop::showChannelMenu(std::optional<mrs::Id> target,juce::Point<int> position){
    auto channelPopup=channelMenu(target);juce::Component::SafePointer<Desktop> safe(this);const auto store=app.services().projects;std::vector<mrs::Id> folderIds;for(const auto& f:project()->folders)folderIds.push_back(f.id);const auto members=folderSelection(target?target:selectedTrack);
    channelPopup.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({position.x,position.y,1,1}),[safe,target,store,folderIds,members](int choice){if(!safe||!choice||safe->app.services().projects!=store)return;
        if(choice==70){safe->textDialog("Group tracks in folder","Tracks",[safe,store,members](auto text){if(safe&&safe->app.services().projects==store&&text.trim().isNotEmpty())safe->app.create_track_folder(text.trim().toStdString(),members);});return;}
        if(choice==71){safe->run([&]{safe->app.move_tracks_to_folder(members,{});});return;}
        if(choice>=2000&&choice<2000+static_cast<int>(folderIds.size())){safe->run([&]{safe->app.move_tracks_to_folder(members,folderIds[static_cast<std::size_t>(choice-2000)]);});return;}
        if(choice==23){if(!target)return;const auto p=safe->project();if(std::none_of(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==*target;}))return;safe->selectedTrack=target;}safe->action(choice);});
}
void Desktop::navigationMenu(bool sections){
    if(busyGesture()||app.recording()){message="Finish editing/recording before navigating";repaint();return;}
    juce::PopupMenu choices;choices.setLookAndFeel(&theme);std::vector<std::pair<mrs::Tick,mrs::Id>> navigationEntries;
    const auto p=project();if(sections)for(const auto& item:p->sections)navigationEntries.push_back({item.start,item.id});else for(const auto& item:p->markers)navigationEntries.push_back({item.tick,item.id});
    std::stable_sort(navigationEntries.begin(),navigationEntries.end(),[](const auto& a,const auto& b){return a.first<b.first;});
    const mrs::Timeline time(p->time,p->sample_rate);int index=0;
    for(const auto& [tick,id]:navigationEntries){juce::String caption;if(sections){for(const auto& item:p->sections)if(item.id==id)caption=label(item.name);}else{for(const auto& item:p->markers)if(item.id==id)caption=label(item.name);}const auto pos=time.musical_position(tick);choices.addItem(++index,caption+" @ "+juce::String(pos.bar)+":"+juce::String(pos.beat)+":"+juce::String(pos.tick));}
    if(navigationEntries.empty())choices.addItem(1,sections?"No sections":"No markers",false);
    juce::Component::SafePointer<Desktop> safe(this);const auto store=app.services().projects;
    choices.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&contextLabels[sections?1:2]),[safe,store,navigationEntries,sections](int result){if(!safe||result<=0||result>static_cast<int>(navigationEntries.size()))return;safe->run([&]{if(store!=safe->app.services().projects)throw std::runtime_error("Project changed; reopen navigation list");if(safe->busyGesture())throw std::runtime_error("Finish editing before navigating");const auto& id=navigationEntries[static_cast<std::size_t>(result-1)].second;if(sections)safe->app.musical().seek_section(id);else safe->app.musical().seek_marker(id);});});
}
void Desktop::menuItemSelected(int n,int){action(n);}
void Desktop::action(int n){if(busyGesture()){if(n==34||n==35||n==48||n==49||n==50||n==51){message="Finish editing before navigating";repaint();}return;}mrs::diagnostics::event("ui_action="+std::to_string(n));run([&]{
    if(n==710){
        if(app.recording())return;
        choose(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
               [this](const juce::File& file){
                   juce::String error;
                   if(!enableTrackHeaderLayout(file,error)){
                       message="Track Header layout rejected: "+error;
                       repaint();
                   } else {
                       trackHeaderHotReload=false; // Choosing a file never silently starts watching.
                       message="Track Header JSON loaded. Options > Hot Reload to watch for edits.";
                       repaint();
                   }
               },"*.json");
        return;
    }
    if(n==711){
        if(!app.recording()&&trackHeaderLayout && trackHeaderLayoutSource!=juce::File()
           && (trackHeaderHotReload || trackHeaderLayoutSource.existsAsFile())){
            // Disabling a watch must remain possible after a file was moved/deleted.
            trackHeaderHotReload=!trackHeaderHotReload;
            trackHeaderPendingBytes=-1;
            message=trackHeaderHotReload?"Track Header Hot Reload ON":"Track Header Hot Reload OFF";
            repaint();
        }
        return;
    }
    if(n==712){
        if(!app.recording())clearTrackHeaderLayout();
        return;
    }
    if(n==7){const auto logs=mrs::diagnostics::folder();if(logs.empty())throw std::runtime_error("Diagnostics unavailable; see package README");if(!testing)juce::File(juce::String(logs.wstring().c_str())).revealToUser();return;}
    if(n>=500&&n<=700){auto c=app.click_settings();if(n==500)c.playback=!c.playback;else if(n==501)c.recording=!c.recording;else if(n==502)c.accent=!c.accent;else if(n==503)c.playback=c.recording=!(c.playback||c.recording);else if(n==504){if(c.count_bars){lastCountBars=c.count_bars;c.count_bars=0;}else c.count_bars=lastCountBars;}else if(n>=510&&n<=514)c.count_bars=n-510;else if(n>=600)c.level=n-600;else return;app.set_click_settings(c);if(c.count_bars)lastCountBars=c.count_bars;prefs.click=c;saveSettings();return;}
    if(n==9){if(liveDocument)setWorkspace(mrs::desktop::Workspace::live);return;}
    if(n==701){confirmDiscard([this]{choose(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[this](const auto& file){createLive(file);},"*.mrlive");});return;}
    if(n==8){confirmDiscard([this]{run([&]{closeEditors();cancelPreview();app.pause();app.new_project();liveDocument.reset();liveFile={};liveDirty=false;app.workspace(mrs::desktop::Workspace::arrange);resetDevice();selectedTrack.reset();selectedClip.reset();selectedClips.clear();refresh(true);showProjectHome(true);projectSessionStarted=false;home->rescan();});});return;}
    if(n==6){app.pause();showProjectHome(true);home->rescan();return;}
    if(n>=1000){auto at=static_cast<std::size_t>(n-1000);if(at<prefs.recent_projects.size()){const auto f=juce::File(label(prefs.recent_projects[at]));confirmDiscard([this,f]{run([&]{openFile(f);});});}return;}
    if(n==1 || n==2){confirmDiscard([this,n]{run([&]{if(n==1){const bool session=app.audio_name()!="Offline clock (no sound)"&&app.audio_name()!="Disconnected";closeEditors();const bool leavingLive=liveDocument.has_value()||app.workspace()==mrs::desktop::Workspace::live;app.new_project(session?prefs.rate:48000);liveDocument.reset();liveFile={};liveDirty=false;if(leavingLive)app.workspace(mrs::desktop::Workspace::arrange);resetDevice();selectedClip.reset();selectedTrack.reset();refresh(true);showProjectHome(false);reconnectDevice(session);}else choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](const auto& f){openFile(f);},"*.mrsproject;*.mrlive");});});}
    else if(n==3){if(documentFile()==juce::File())action(4);else saveFile(documentFile());}
    else if(n==4)choose(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[this](const auto& f){saveFile(f.withFileExtension(liveDocument?"mrlive":"mrsproject"));},liveDocument?"*.mrlive":"*.mrsproject");
    else if(n==5)choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](const auto& f){importFiles({f.getFullPathName()});},"*.wav");
    else if(n==10 || n==11){if(!app.history_preserves_plugins(n==11))closeEditors(true);if(n==10)app.undo();else app.redo();refresh(true);}
    else if(n==20)selectedTrack=app.add_audio_track("Audio "+std::to_string(project()->tracks.size()+1));
    else if(n==28)selectedTrack=app.add_instrument_track("Instrument "+std::to_string(project()->tracks.size()+1));
    else if(n==42)app.midi_panic();
    else if(n==29)selectedTrack=app.add_fx_channel("FX "+std::to_string(project()->tracks.size()+1));
    else if(n==21)selectedTrack=app.add_bus("Bus "+std::to_string(project()->tracks.size()+1));
    else if(n==22 && selectedTrack)renameTrack(*selectedTrack);
    else if(n==23 && selectedTrack){closeEditors();app.remove_track(*selectedTrack);selectedTrack.reset();selectedClip.reset();}
    else if((n==26 || n==27) && selectedTrack){const auto p=project();for(std::size_t i=0;i<p->tracks.size();++i)if(p->tracks[i].id==selectedTrack && ((n==26&&i>0)||(n==27&&i+1<p->tracks.size())))app.reorder_track(*selectedTrack,n==26?i-1:i+1);}
    else if(n==47){textDialog("Project tempo (BPM at project start)",juce::String(project()->time.tempos.front().bpm),[this](juce::String text){std::size_t used{};const auto value=std::stod(text.toStdString(),&used);if(used!=static_cast<std::size_t>(text.length()))throw std::runtime_error("Enter a valid BPM number");auto time=project()->time;time.tempos.front().bpm=value;app.set_time_map(std::move(time));});}
    else if(n==43 && selectedTrack){const mrs::Timeline t(project()->time,project()->sample_rate);selectedClip=app.create_midi_clip(*selectedTrack,t.to_ticks(app.engine()->state().sample));openMidiClip(*selectedClip);}
    else if(n==44 && selectedClip){openMidiClip(*selectedClip);}
    else if(n==45 && arrangement->markers->hasSelection()){arrangement->markers->duplicateSelected();}
    else if(n==45 && arrangement->sections->hasSelection()){arrangement->sections->duplicateSelected();}
    else if(n==45 && arrangement->chords->hasSelection()){arrangement->chords->duplicateSelected();}
    else if(n==45 && selectedClip){activateClip(app.duplicate_clip(*selectedClip));}
    else if(n==52){std::vector<mrs::Id> ids;for(const auto& c:project()->clips)if(selectedClips.contains(c.id.value)||c.id==selectedClip)ids.push_back(c.id);activateClip(app.merge_midi_clips(std::move(ids)));}
    else if(n==46 && selectedClip){if(app.recording())throw std::runtime_error("Stop recording before changing loop");const auto p=project();const mrs::Timeline t(p->time,p->sample_rate);auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip){return clip.id==*selectedClip;});if(c!=p->clips.end())app.services().transport->set_loop(mrs::LoopRange{mrs::clip_start(*c,t),mrs::clip_end(*c,t)});}
    else if(n==24 && arrangement->sections->hasSelection()){arrangement->sections->splitSelected(mrs::Timeline(project()->time,project()->sample_rate).to_ticks(app.engine()->state().sample));}
    else if(n==24 && arrangement->chords->hasSelection()){arrangement->chords->splitSelected(mrs::Timeline(project()->time,project()->sample_rate).to_ticks(app.engine()->state().sample));}
    else if(n==24 && selectedClip){std::vector<mrs::Id> ids;for(const auto& c:project()->clips)if(selectedClips.contains(c.id.value)||c.id==selectedClip)ids.push_back(c.id);const auto rights=app.split_clips(std::move(ids),app.engine()->state().sample);activateClip(rights.front());selectedClips.clear();for(const auto& id:rights)selectedClips.insert(id.value);}
    else if(n==25 && arrangement->meters->hasSelection()){arrangement->meters->removeSelected();}
    else if(n==25 && arrangement->tempos->hasSelection()){arrangement->tempos->removeSelected();}
    else if(n==25 && arrangement->sections->hasSelection()){arrangement->sections->removeSelected();}
    else if(n==25 && arrangement->markers->hasSelection()){arrangement->markers->removeSelected();}
    else if(n==25 && arrangement->chords->hasSelection()){arrangement->chords->removeSelected();}
    else if(n==25 && selectedClip){std::vector<mrs::Id> clips;for(const auto& c:project()->clips)if(selectedClips.contains(c.id.value)||c.id==selectedClip)clips.push_back(c.id);app.remove_clips(std::move(clips));selectedClip.reset();selectedClips.clear();}
    else if(n==30)app.play();else if(n==31)app.pause();else if(n==32){if(app.recording())app.stop_recording();app.stop();}
    else if(n==33){if(app.recording()){app.stop_recording();app.stop();refresh(true);}else app.start_project_recording(path(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MR Studio").getChildFile("Projects")));}
    else if(n==48)app.musical().previous_marker();else if(n==49)app.musical().next_marker();else if(n==50||n==51)navigationMenu(n==50);
    else if(n==34)app.musical().previous_section();else if(n==35)app.musical().next_section();
    else if(n==36){if(app.recording())throw std::runtime_error("Stop recording before changing loop");if(app.engine()->state().loop)app.musical().clear_loop();else if(app.musical().state().current_section)app.musical().loop_section(app.musical().state().current_section->id);}
    else if(n==40)audioSettings();
    else if(n==52)aiPermissions();
    else if(n==54)aiAssistant();
    else if(n==41)showProfiling();
});}
void Desktop::setKnifeMode(bool enabled){knifeMode=enabled;arrangement->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));arrangement->sections->cancel();arrangement->chords->cancel();pointerTool.setToggleState(!enabled,juce::dontSendNotification);knifeTool.setToggleState(enabled,juce::dontSendNotification);arrangement->setMouseCursor(enabled?juce::MouseCursor::CrosshairCursor:juce::MouseCursor::NormalCursor);}
void Desktop::setWorkspace(mrs::desktop::Workspace w){app.workspace(w);prefs.workspace=w;if(dockedEditor&&dockedClip)dockedEditor->setVisible(w==mrs::desktop::Workspace::edit);resized();refresh();}
juce::String Desktop::projectCaption() const{return (liveDocument?liveDocument->title:app.path().empty()?label(project()->title):juce::String(app.path().stem().wstring().c_str()))+(documentDirty()?" *":"");}
void Desktop::resized(){if(homeVisible&&home){for(auto* child:getChildren())if(child!=home.get())child->setVisible(false);home->setBounds(getLocalBounds());home->toFront(false);return;}
    if(liveWorkspace && app.workspace()==mrs::desktop::Workspace::live){liveLayoutActive=true;for(auto* child:getChildren())child->setVisible(child==liveWorkspace.get());liveWorkspace->setBounds(getLocalBounds());liveWorkspace->toFront(false);return;}
    if(liveLayoutActive){for(auto* child:getChildren())if(child!=home.get()&&child!=liveWorkspace.get())child->setVisible(true);liveLayoutActive=false;}
    if(home)home->setVisible(false);if(liveWorkspace)liveWorkspace->setVisible(false);
    menu.setBounds(0,0,getWidth(),26);projectTitle.setBounds(440,0,juce::jmax(0,getWidth()-880),26);int x=8;
    addTrack.setVisible(false);addBus.setVisible(false);remove.setVisible(false);

    split.setVisible(false);for(auto* b:{&pointerTool,&knifeTool,&undo,&redo,&duplicate,&zoomIn,&zoomOut,&fit,&snapButton}){const int width=32;b->setBounds(x,34,width,28);x+=width+5;if(b==&knifeTool)x+=8;}
    for(std::size_t i=0;i<contextLabels.size();++i)contextLabels[i].setBounds(8+static_cast<int>(i)*(getWidth()-16)/3,66,(getWidth()-16)/3-4,22);
    clickButton.setBounds(getWidth()-340,getHeight()-34,30,30);countButton.setBounds(getWidth()-306,getHeight()-34,30,30);
    browserWidth=juce::jlimit(200,juce::jmax(200,juce::jmin(700,getWidth()-700)),browserWidth);
    const int available=getWidth()-(sidebar?browserWidth:0)-16;
    const bool showMix=app.workspace()==mrs::desktop::Workspace::mix;
    const bool showEdit=app.workspace()==mrs::desktop::Workspace::edit&&dockedClip&&dockedEditor&&dockedEditor->isVisible();
    const bool instrumentChannels=std::any_of(project()->tracks.begin(),project()->tracks.end(),[](const auto& track){return track.kind==mrs::TrackKind::instrument;});
    const int footerHeight=getWidth()<1500?58:42;
    const bool araDock=showEdit&&static_cast<bool>(dockedEditor->getProperties()["mrs-ara-editor"]);
    const int maxHeight=araDock?juce::jmax(0,getHeight()-94-footerHeight):juce::jmax(instrumentChannels?436:260,getHeight()-292);
    const int insertHeight=mixerInsertHeight();
    // mixerHeight remains the legacy base height, not the total dock height.
    // Keep the fixed fader/pan and lower controls; reserve one visible send row on tracks.
    const int baseHeight=juce::jmax(juce::jmax(260,(project()->tracks.empty()?352:instrumentChannels?436:380)-insertHeight),view.mixerHeight);
    const int mixerHeight=showEdit?juce::jmin(maxHeight,view.editorHeight):showMix ? juce::jmin(maxHeight,baseHeight+insertHeight) : 0;
    const int mixerShift=showMix?14:0; // Move the whole mixer down, keeping strip/fader dimensions.
    arrangeArea={8,94,available,getHeight()-94-footerHeight-mixerHeight+mixerShift};arrangement->setBounds(arrangeArea);
    mixArea={8,arrangeArea.getBottom()+6,available,mixerHeight-6-mixerShift};
    mixerDivider.setVisible(showMix||showEdit);mixerDivider.setBounds(8,arrangeArea.getBottom(),available,6);
    if(dockedEditor){dockedEditor->setVisible(showEdit);if(showEdit)dockedEditor->setBounds(mixArea);}
    mixerViewport.setVisible(showMix);if(master)master->setVisible(showMix);
    if(showMix){mixerViewport.setBounds(mixArea.withTrimmedRight(88));mixerBody.setSize(juce::jmax(mixerViewport.getWidth(),mixerColumnCount*88),mixArea.getHeight());
        for(std::size_t i=0;i<mixer.size();++i)if(i<mixerColumns.size()&&mixerColumns[i]>=0){mixer[i]->setBounds(mixerColumns[i]*88,0,86,mixerBody.getHeight());mixer[i]->resized();}
        for(std::size_t i=0;i<mixerFolders.size();++i){mixerFolders[i]->setBounds(mixerFolderColumns[i]*88,0,86,mixerBody.getHeight());mixerFolders[i]->resized();}
        if(master){master->setBounds(mixArea.getRight()-86,mixArea.getY(),86,mixArea.getHeight());master->resized();}}
    browserWidth=juce::jlimit(200,juce::jmax(200,juce::jmin(700,getWidth()-700)),browserWidth);
    browserDivider.setVisible(sidebar);browserDivider.setBounds(getWidth()-browserWidth-8,94,8,getHeight()-94-footerHeight);
    browser->setVisible(sidebar);browser->setBounds(getWidth()-browserWidth,94,browserWidth-8,getHeight()-94-footerHeight);
    x=juce::jmax(400,(getWidth()-520)/2);navigationMode.setBounds(x,getHeight()-34,86,30);x+=96;for(auto* b:{&play,&pause,&stop,&record,&previous,&next,&loop}){b->setBounds(x,getHeight()-34,36,30);x+=40;}for(auto* b:{&clickButton,&countButton}){b->setBounds(x,getHeight()-34,32,30);x+=36;}
    x=getWidth()-260;for(auto* b:{&editButton,&mixButton,&brows}){b->setBounds(x,getHeight()-34,78,30);x+=84;}
    cpuReadout.setBounds(8,getHeight()-29,88,20);audioCpuBar.setBounds(100,getHeight()-21,90,4);
    latencyReadout.setBounds(196,getHeight()-29,188,20);
    shellStatus.setVisible(shellTheme!=nullptr);shellTime.setVisible(shellTheme!=nullptr);if(shellTheme)layoutShell();
}
juce::String Desktop::footerStatus(){
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
    return s;
}
void Desktop::paint(juce::Graphics& g){g.fillAll(juce::Colour(surface));if(shellTheme&&!homeVisible)paintShell(g);g.setFont(theme.font(12));
    if(assetSkin()&&!homeVisible){g.setColour(juce::Colour(surface));g.fillRect(mixArea);}
    const auto head=displayState();
    // Keep errors/count-in visible; routine driver/counter status lives in CPU tooltip.
    if(!message.isEmpty()||head.count_remaining){g.setColour(message.isEmpty()?juce::Colours::lightgrey:juce::Colours::orange);g.drawText(message.isEmpty()?"COUNT-IN "+juce::String(static_cast<double>(head.count_remaining)/project()->sample_rate,1)+" s":message,10,getHeight()-55,getWidth()-512,16,juce::Justification::left);}
    if(sidebar){g.setColour(juce::Colour(separatorColour));g.fillRect(browserDivider.getX()+3,browserDivider.getY(),2,browserDivider.getHeight());}
    if(mixerDivider.isVisible()){g.setColour(juce::Colour(separatorColour));g.fillRect(mixerDivider.getBounds().withHeight(1).translated(0,2));}
    if(!shellTheme){g.setColour(juce::Colours::whitesmoke);g.drawText(juce::String(static_cast<double>(head.sample)/project()->sample_rate,2)+" s",play.getX()-86,getHeight()-(getWidth()<1500?56:33),80,28,juce::Justification::right);}
}
void Desktop::mouseDown(const juce::MouseEvent& e){if(e.eventComponent==&contextLabels[1]||e.eventComponent==&contextLabels[2]){if(e.mods.isLeftButtonDown()||e.mods.isPopupMenu())navigationMenu(e.eventComponent==&contextLabels[1]);return;}const auto local=e.getEventRelativeTo(this);if(e.mods.isPopupMenu()&&mixArea.contains(local.getPosition())){showChannelMenu({},e.getScreenPosition());return;}resizingBrowser=sidebar&&browserDivider.getBounds().contains(local.getPosition());resizingMixer=mixerDivider.isVisible()&&mixerDivider.getBounds().contains(local.getPosition());mixerDragHeight=app.workspace()==mrs::desktop::Workspace::edit?mixArea.getHeight()+6:mixArea.getHeight()+20-mixerInsertHeight();mixerDragY=local.y;
    araDockResizeFrame={};pendingAraDockHeight.reset();resizingAraDock=nullptr;
    if(resizingMixer&&app.workspace()==mrs::desktop::Workspace::edit&&dockedEditor&&static_cast<bool>(dockedEditor->getProperties()["mrs-ara-editor"])){
        resizingAraDock=dockedEditor.get();araDockResizeFrame=juce::VBlankAttachment(this,[this](double){flushAraDockResize();});
    }
}
void Desktop::resizeBrowser(int width){browserWidth=juce::jlimit(200,juce::jmax(200,juce::jmin(700,getWidth()-700)),width);resized();repaint();}
int Desktop::mixerInsertHeight() const {
    std::size_t rows=project()->master_inserts.size();
    int height=static_cast<int>(rows)*20;
    for(const auto& track:project()->tracks)height=std::max(height,static_cast<int>(track.inserts.size())*20+static_cast<int>(track.sends.size())*24+24);
    return height; // Inserts and bottom send rows share the expandable area.
}
void Desktop::resizeSendArea(int height){
    int maximum=0;for(const auto& strip:mixer)maximum=juce::jmax(maximum,strip->gain.getY()-56-8-24);
    sendAreaHeight=juce::jlimit(juce::jmin(24,maximum),maximum,height);
    for(auto& strip:mixer){strip->resized();strip->repaint();}if(master){master->resized();master->repaint();}
}
void Desktop::resizeMixer(int height){
    const auto previousHeight=app.workspace()==mrs::desktop::Workspace::edit?view.editorHeight:view.mixerHeight;
    if(app.workspace()==mrs::desktop::Workspace::edit){
        const bool araDock=dockedEditor&&static_cast<bool>(dockedEditor->getProperties()["mrs-ara-editor"]);
        const int maximum=araDock?juce::jmax(0,getHeight()-94-(getWidth()<1500?58:42)):juce::jmax(500,getHeight()-290);
        view.editorHeight=juce::jlimit(juce::jmin(500,maximum),maximum,height);
    }
    else {
        const int inserts=mixerInsertHeight();
        const bool instrumentChannels=std::any_of(project()->tracks.begin(),project()->tracks.end(),[](const auto& track){return track.kind==mrs::TrackKind::instrument;});
        const int minimum=juce::jmax(260,(project()->tracks.empty()?352:instrumentChannels?436:380)-inserts);
        const int maximum=juce::jmax(minimum,getHeight()-292-inserts);
        view.mixerHeight=juce::jlimit(minimum,maximum,height);
    }
    if(previousHeight==(app.workspace()==mrs::desktop::Workspace::edit?view.editorHeight:view.mixerHeight))return;
    resized();repaint();
}
void Desktop::updatePerformance(const mrs::audio::DeviceStatus& status,bool hardware){
    const bool running=hardware&&status.phase==mrs::audio::DevicePhase::running;
    const bool measured=running&&std::isfinite(status.cpu_load)&&status.cpu_load>=0;
    const auto now=juce::Time::getMillisecondCounterHiRes();
    if(measured){const auto dt=juce::jlimit(0.,1.,(now-cpuUpdateMs)/1000.);displayedCpu=cpuMeasured?displayedCpu+(1-std::exp(-dt/.7))*(status.cpu_load-displayedCpu):status.cpu_load;}else displayedCpu=0;
    cpuUpdateMs=now;cpuMeasured=measured;audioCpu=juce::jlimit(0.,1.,displayedCpu);
    const auto diagnostic=footerStatus();
    cpuReadout.setTooltip("Driver callback wall time / deadline; excludes summed parallel worker CPU. Engine profiling is in the Transport menu.\n"+diagnostic);
    audioCpuBar.setTooltip(diagnostic);
    cpuReadout.setText(measured?"CPU "+juce::String(displayedCpu*100,1)+"%":"CPU --%",juce::dontSendNotification);
    audioCpuBar.setColour(juce::ProgressBar::foregroundColourId,juce::Colour(accent));
    auto latency=[&](double value){return running&&std::isfinite(value)&&value>=0?juce::String(value,2):juce::String("--");};
    auto text="Latency I "+latency(status.input_latency_ms)+" / O "+latency(status.output_latency_ms)+" ms";
    const auto metrics=app.engine()->metrics();
    if(metrics.monitoring_available)text+=" | Mon "+latency(status.input_latency_ms+status.output_latency_ms+static_cast<double>(metrics.monitoring_compensation_frames)*1000/project()->sample_rate);
    latencyReadout.setText(text,juce::dontSendNotification);
    latencyReadout.setTooltip("I/O: driver-reported ms. Mon: estimated input + output + fixed PDC; no Process Buffer hold on live input. Not a measured acoustic round trip. High-latency inserts remain active.");
}
void Desktop::flushAraDockResize(){
    if(!pendingAraDockHeight)return;const int height=*pendingAraDockHeight;pendingAraDockHeight.reset();
    if(resizingMixer&&resizingAraDock&&dockedEditor.get()==resizingAraDock.getComponent()&&app.workspace()==mrs::desktop::Workspace::edit)resizeMixer(height);
}
void Desktop::mouseDrag(const juce::MouseEvent& e){const auto local=e.getEventRelativeTo(this);if(resizingBrowser)resizeBrowser(getWidth()-local.x);if(resizingMixer){const int height=mixerDragHeight+mixerDragY-local.y;if(resizingAraDock)pendingAraDockHeight=height;else resizeMixer(height);}}
void Desktop::mouseUp(const juce::MouseEvent&){flushAraDockResize();araDockResizeFrame={};pendingAraDockHeight.reset();resizingAraDock=nullptr;if(resizingBrowser||resizingMixer)saveSettings();resizingBrowser=resizingMixer=false;}
void Desktop::refresh(bool force){
    mrs::ai::Selection aiSelection;if(selectedTrack)aiSelection.entities.push_back({mrs::ai::EntityKind::track,*selectedTrack});if(selectedClip)aiSelection.entities.push_back({mrs::ai::EntityKind::clip,*selectedClip});app.set_ai_selection(std::move(aiSelection));
    if(insertPowerProject!=app.services().projects){insertPowerRecall.clear();insertPowerProject=app.services().projects;}
    if(insertDrag&&insertDrag->store!=app.services().projects)insertDrag.reset();if(!app.recording()){recordingPreview.clear();audioRecordingPreview.clear();}std::erase_if(selectedClips,[&](const auto& value){const auto p=project();return std::none_of(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id.value==value;});});const auto p=project();const auto state=app.services().projects->state();
    std::vector<mrs::Id> current;for(const auto& t:p->tracks)current.push_back(t.id);
    const bool replaced=displayedStore!=app.services().projects;
    if(replaced){selectedTracks.clear();displayedStore=app.services().projects;visualTransport.reset();revision=~0ULL;peaks={};masterPeak={};arrangement->horizontal=0;arrangement->vertical=0;mixerViewport.setViewPosition(0,0);}
    if(replaced||current!=ids){auto oldIds=ids,newIds=current;const auto byId=[](const auto& a,const auto& b){return a.value<b.value;};std::sort(oldIds.begin(),oldIds.end(),byId);std::sort(newIds.begin(),newIds.end(),byId);const bool reorder=!replaced&&oldIds==newIds;if(!reorder)closeEditors();cancelPreview();ids=current;if(reorder){std::vector<std::unique_ptr<Strip>> ordered;for(const auto& id:current){auto at=std::find_if(mixer.begin(),mixer.end(),[&](const auto& s){return s&&s->target==id;});ordered.push_back(std::move(*at));}mixer=std::move(ordered);arrangement->rebuild();resized();}else{mixer.clear();for(const auto& t:p->tracks){auto s=std::make_unique<Strip>(*this,t.id);mixerBody.addAndMakeVisible(*s);mixer.push_back(std::move(s));}
        master=std::make_unique<Strip>(*this,std::nullopt);addAndMakeVisible(*master);arrangement->rows.clear();arrangement->rebuild();resized();}}
    if(force || revision!=state.revision){
        mrs::diagnostics::event("project_snapshot title="+p->title+" tracks="+std::to_string(p->tracks.size())+" clips="+std::to_string(p->clips.size())+" revision="+std::to_string(state.revision)+" insert_generation="+std::to_string(app.insert_generation()));
        for(const auto& t:p->tracks)for(const auto& fx:t.inserts)if(fx.kind==mrs::InsertKind::vst3)mrs::diagnostics::event("plugin track="+t.name+" name="+fx.plugin_name+" path="+fx.plugin_path+" class="+fx.class_id);
        for(const auto& fx:p->master_inserts)if(fx.kind==mrs::InsertKind::vst3)mrs::diagnostics::event("plugin master name="+fx.plugin_name+" path="+fx.plugin_path+" class="+fx.class_id);
        std::erase_if(selectedTracks,[&](const auto& id){return std::none_of(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id.value==id;});});arrangement->rebuildFolderLayout();arrangement->resized();rebuildMixerFolders();
        app.prepare_waveforms();for(auto& s:mixer)s->sync();for(auto& s:mixerFolders)s->sync();for(auto& s:arrangement->rows)s->sync();if(master)master->sync();refreshPresetLists();revision=state.revision;resized();}
    // Arm is transient host state and may change without a project revision.
    if(assetSkin()){for(auto& s:mixer)s->refreshArmState();for(auto& s:arrangement->rows)s->refreshArmState();}
    navigationMode.setEnabled(!app.recording());previous.setEnabled(!app.recording());next.setEnabled(!app.recording());
    refreshMusicalContext();
    projectTitle.setText(projectCaption(),juce::dontSendNotification);
    undo.setEnabled(state.can_undo);redo.setEnabled(state.can_redo);
    arrangement->meters->sync(replaced);arrangement->tempos->sync(replaced);arrangement->sections->sync(replaced);
    arrangement->chords->sync(replaced);
    arrangement->markers->sync(replaced);
    duplicate.setEnabled((selectedClip.has_value()||arrangement->chords->hasSelection()||arrangement->sections->hasSelection()||arrangement->markers->hasSelection())&&!app.recording());
    remove.setButtonText(arrangement->markers->hasSelection()?"Del mark":arrangement->chords->hasSelection()?"Del chord":"Del clip");
    editButton.setToggleState(clipEditorOpen(),juce::dontSendNotification);
    mixButton.setToggleState(app.workspace()==mrs::desktop::Workspace::mix,juce::dontSendNotification);
    record.setToggleState(app.recording(),juce::dontSendNotification);loop.setToggleState(displayState().loop.has_value(),juce::dontSendNotification);
    const auto click=app.click_settings();clickButton.setToggleState(click.playback||click.recording,juce::dontSendNotification);clickButton.setEnabled(!app.recording());countButton.setEnabled(!app.recording());countButton.setButtonText(click.count_bars?"Count "+juce::String(click.count_bars):"Count off");countButton.setToggleState(click.count_bars>0,juce::dontSendNotification);
    brows.setToggleState(sidebar,juce::dontSendNotification);if(shellTheme)refreshShell();repaint();arrangement->repaint();
}
void Desktop::refreshMusicalContext(){
    app.musical().refresh();
    const auto& nextContext=app.musical().state();
    const bool labelsChanged=displayedMusical.project!=nextContext.project||
        displayedMusical.current_chord!=nextContext.current_chord||displayedMusical.next_chord!=nextContext.next_chord||
        displayedMusical.current_section!=nextContext.current_section||displayedMusical.next_section!=nextContext.next_section||
        displayedMusical.current_marker!=nextContext.current_marker||displayedMusical.next_marker!=nextContext.next_marker;
    const auto id=[](const auto& value)->std::optional<mrs::Id>{return value?std::optional<mrs::Id>(value->id):std::nullopt;};
    // Interpolation must not leave the line on the old side of a musical boundary.
    if(id(displayedMusical.current_chord)!=id(nextContext.current_chord)||
       id(displayedMusical.current_section)!=id(nextContext.current_section)||
       id(displayedMusical.current_marker)!=id(nextContext.current_marker))visualTransport.reset();
    displayedMusical=nextContext;
    if(!labelsChanged)return;
    const auto chord=[](const auto& c){return c?label(c->symbol):juce::String("-");};
    const auto named=[](const auto& c){return c?label(c->name):juce::String("-");};
    const std::array<juce::String,3> texts{
        "Chord: "+chord(displayedMusical.current_chord)+"  →  "+chord(displayedMusical.next_chord),
        "Section: "+named(displayedMusical.current_section)+"  →  "+named(displayedMusical.next_section),
        "Marker: "+named(displayedMusical.current_marker)+"  →  "+named(displayedMusical.next_marker)};
    for(std::size_t i=0;i<texts.size();++i){contextLabels[i].setText(texts[i],juce::dontSendNotification);contextLabels[i].setTooltip("Current → Next\n"+texts[i]+(i?"\nClick to navigate":""));contextLabels[i].setTitle(texts[i]);}
}
bool Desktop::busyGesture() const{if(resizingMixer||resizingBrowser||arrangement->meters->dragging()||arrangement->tempos->dragging()||arrangement->sections->dragging()||arrangement->chords->dragging()||arrangement->markers->dragging())return true;for(const auto& strip:mixer)if(strip->active())return true;if(master&&master->active())return true;if(preview&&preview->active())return true;return false;}
void Desktop::setPreview(std::optional<mrs::Id> t,mrs::Track::Mix m,float gain,std::function<bool()> active){preview=MixPreview{t,m,gain,std::move(active)};app.preview_mix(t,m,gain);}
void Desktop::cancelPreview(){preview.reset();app.cancel_mix_preview();}
void Desktop::finishPreview(){preview.reset();}
void Desktop::setSendPreview(mrs::Id source,mrs::Id bus,float gain,std::function<bool()> active){
    preview=MixPreview{source,{},project()->master_gain,std::move(active),bus,gain};app.preview_send_gain(source,bus,gain);
}
bool Desktop::shortcutAllowed() const {return shortcutAllowedFor(juce::Component::getCurrentlyFocusedComponent());}
bool Desktop::shortcutAllowedFor(juce::Component* focused) const {
    if(juce::Component::getCurrentlyModalComponent()!=nullptr)return false;
    if(focused && focused!=this && !isParentOf(focused))return false;
    for(auto* c=focused;c&&c!=this;c=c->getParentComponent())if(c->getComponentID()=="mr-library-browser"||dynamic_cast<juce::TextEditor*>(c)||dynamic_cast<juce::ComboBox*>(c))return false;
    return true;
}
bool Desktop::workspaceShortcut(const juce::KeyPress& k){if(homeVisible||k.getModifiers().isAnyModifierKeyDown()||juce::Component::getCurrentlyModalComponent())return false;
    if(k.getKeyCode()==juce::KeyPress::F2Key){run([&]{toggleEditor();});return true;}if(k.getKeyCode()==juce::KeyPress::F3Key){const bool open=app.workspace()!=mrs::desktop::Workspace::mix;if(open&&clipEditorOpen())toggleEditor();setWorkspace(open?mrs::desktop::Workspace::mix:mrs::desktop::Workspace::arrange);return true;}if(k.getKeyCode()==juce::KeyPress::F5Key){sidebar=!sidebar;resized();repaint();return true;}return false;}
bool Desktop::keyPressed(const juce::KeyPress& k){if(workspaceShortcut(k))return true;if(!shortcutAllowed())return false;
    if(k.getKeyCode()==juce::KeyPress::escapeKey&&app.stretch_status().busy){app.cancel_stretch();return true;}
    if(k.getKeyCode()==juce::KeyPress::escapeKey&&app.wav_import_status().busy){app.cancel_wav_import();return true;}
    if(homeVisible){if(k.getModifiers().isCtrlDown()&&k.getKeyCode()=='O'){action(2);return true;}if(k.getModifiers().isCtrlDown()&&k.getKeyCode()=='N'){action(1);return true;}return false;}
    auto c=k.getKeyCode();if(k.getModifiers().isAltDown())return false;if(k.getModifiers().isCtrlDown()){if(c=='D'&&(arrangement->sections->hasSelection()||arrangement->chords->hasSelection()||arrangement->markers->hasSelection())){action(45);return true;}if(c=='Z'){action(10);return true;}if(c=='Y'){action(11);return true;}if(c=='S'){action(3);return true;}if(c=='O'){action(2);return true;}if(c=='N'){action(1);return true;}return false;}
    if(k.getModifiers().isShiftDown())return false;
    if(c=='1'){setKnifeMode(false);return true;}if(c=='3'){setKnifeMode(true);return true;}if(c=='D'){action(45);return true;}if(c=='G'){action(52);return true;}if(c=='S'){action(24);return true;}if(c=='R'){action(33);return true;}if(c==juce::KeyPress::backspaceKey||c==juce::KeyPress::deleteKey){action(25);return true;}return c==juce::KeyPress::spaceKey;
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
void Desktop::timerCallback(){try{const bool wasRecording=app.recording();app.poll();
    const auto importing=app.wav_import_status();
    if(importing.busy)message="Preparing WAV import: "+juce::String(static_cast<int>(importing.progress*100.f))+"% (Esc to cancel)";
    else if(wavImportRevision!=importing.revision)message=label(importing.message);
    wavImportRevision=importing.revision;
    const auto stretching=app.stretch_status();
    if(!importing.busy){if(stretching.busy)message="Preparing Time / Pitch: "+juce::String(static_cast<int>(stretching.progress*100.f))+"% (Esc to cancel)";else if(stretchRevision!=stretching.revision)message=label(stretching.message);}
    stretchRevision=stretching.revision;
    // One cheap metadata check every 500ms, and only if explicitly enabled.
    if(++trackHeaderPollTick>=15){trackHeaderPollTick=0;pollTrackHeaderLayout();}if(wasRecording&&!app.recording()&&!app.recording_error().empty())message=label(app.recording_error());if(++previewTick%3==0)updateRecordingPreview();if(!app.recording()){recordingPreview.clear();audioRecordingPreview.clear();}if(dockedEditor&&!dockedClip)dockedEditor.reset();if(preview){if(preview->active()){if(preview->sendBus&&preview->target)app.preview_send_gain(*preview->target,*preview->sendBus,preview->sendGain);else app.preview_mix(preview->target,preview->mix,preview->master);}else cancelPreview();}
    updatePerformance(app.device_status(),app.audio_name()!="Offline clock (no sound)"&&app.audio_name()!="Disconnected");
    if(++diagnosticTick%150==0){mrs::audio::RealtimeState head;if(app.engine()->try_state(head))mrs::diagnostics::event("heartbeat device="+app.audio_name()+" rate="+std::to_string(project()->sample_rate)+" requested_buffer="+std::to_string(prefs.buffer)+" workers="+std::to_string(prefs.processing_workers)+" process="+std::to_string(prefs.process_buffer_frames)+" playback="+std::to_string(static_cast<int>(head.playback))+" sample="+std::to_string(head.sample)+" recording="+std::to_string(app.recording()));}
    if(editorGeneration!=app.insert_generation()){refreshInsertEditors();editorGeneration=app.insert_generation();}
    #ifdef MRS_HAS_ARA
    pollAra();
#endif
    std::erase_if(windows,[](const auto& w){return !w->isVisible();});
    if(scanner.valid()&&scanner.wait_for(std::chrono::seconds(0))==std::future_status::ready){catalog=scanner.get();browser->rebuild();}
    const auto meter=app.engine()->take_meters();for(std::size_t i=0;i<peaks.size();++i){peaks[i].left=juce::jmax(meter.tracks[i].left,peaks[i].left*.86f);peaks[i].right=juce::jmax(meter.tracks[i].right,peaks[i].right*.86f);}
    masterPeak={juce::jmax(meter.master.left,masterPeak.left*.86f),juce::jmax(meter.master.right,masterPeak.right*.86f)};
    for(auto& s:mixer)s->repaint();for(auto& s:arrangement->rows){s->refreshMidiStatus();s->repaint();}if(master)master->repaint();
    refresh();if(homeVisible&&home)home->repaint();
}catch(const std::exception& e){message=label(e.what());repaint();}}

bool Desktop::enableTrackHeaderLayout(const juce::File& file, juce::String& error) {
    // Only called from the JUCE message thread; never modifies project state.
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    auto candidate = TrackHeaderLayout::readFile(file, error);
    if (!candidate) return false; // Atomic last-known-good / built-in fallback.
    trackHeaderLayout = std::move(*candidate);
    trackHeaderLayoutSource = file;
    trackHeaderObservedStamp = file.getLastModificationTime();
    trackHeaderObservedBytes = file.getSize();
    trackHeaderPendingBytes = -1;
    if (arrangement) {
        for (auto& row : arrangement->rows) row->sync();
        arrangement->resized();
    }
    return true;
}

void Desktop::restoreBundledTrackHeaderLayout(){
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    // Reset only UI geometry and dev-file selection. Do not touch host bindings,
    // transport, project or audio-engine state. On failure, legacy geometry remains.
    trackHeaderHotReload=false;
    trackHeaderLayoutSource=juce::File();
    trackHeaderObservedBytes=trackHeaderPendingBytes=-1;
    int resourceSize=0;
    // JUCE mangles punctuation in BinaryData identifiers, so the source-file
    // spelling is not a safe getNamedResource key. This dedicated bundle
    // deliberately contains exactly one approved JSON layout.
    const auto* resource = MRTrackHeaderAssets::namedResourceListSize == 1
        ? MRTrackHeaderAssets::getNamedResource(MRTrackHeaderAssets::namedResourceList[0],resourceSize)
        : nullptr;
    juce::String error;
    if (!resource || resourceSize<=0)
        error="MRTrackHeaderAssets must contain exactly one non-empty JSON resource";
    auto bundled=resource && resourceSize>0
        ? TrackHeaderLayout::parse(juce::String::fromUTF8(resource,resourceSize),error)
        : std::optional<TrackHeaderLayout>{};
    if(bundled){
        trackHeaderLayout=std::move(*bundled);
    }else{
        trackHeaderLayout.reset(); // Legacy layout is always the final fallback.
        juce::Logger::writeToLog("Bundled Track Header JSON unavailable; legacy geometry active: "+error);
    }
    if(arrangement){
        for(auto& row:arrangement->rows)row->sync();
        arrangement->resized();
    }
}

void Desktop::clearTrackHeaderLayout(){
    restoreBundledTrackHeaderLayout();
    message=trackHeaderLayout?"Bundled Track Header restored":"Legacy Track Header restored";
    repaint();
}

void Desktop::pollTrackHeaderLayout(){
    // Development-only opt-in behavior; never active at default Release startup.
    // Do not apply new bounds while recording, editing names or dragging controls.
    if(!trackHeaderHotReload || !trackHeaderLayout || trackHeaderLayoutSource==juce::File()
       || app.recording() || busyGesture() || !arrangement) return;
    for(const auto& strip:arrangement->rows)
        if(strip && (strip->active() || strip->isEditingTrackName()))return;
    const auto file=trackHeaderLayoutSource;
    if(!file.existsAsFile())return; // Deleted/temporarily renamed by atomic save: retain last good.
    const auto stamp=file.getLastModificationTime();
    const auto size=file.getSize();
    if(stamp==trackHeaderObservedStamp && size==trackHeaderObservedBytes){
        trackHeaderPendingBytes=-1;
        return;
    }
    // Observe the same metadata twice before parsing, to avoid partially written
    // exports during file replacement. Failed JSON never touches the active layout.
    if(stamp!=trackHeaderPendingStamp || size!=trackHeaderPendingBytes){
        trackHeaderPendingStamp=stamp;
        trackHeaderPendingBytes=size;
        return;
    }
    trackHeaderObservedStamp=stamp;
    trackHeaderObservedBytes=size;
    trackHeaderPendingBytes=-1;
    juce::String error;
    if(!enableTrackHeaderLayout(file,error)){
        message="Hot Reload rejected JSON; previous Track Header retained: "+error;
        juce::Logger::writeToLog(message);
    }else message="Track Header layout reloaded";
    repaint();
}

Strip::Strip(Desktop& d,std::optional<mrs::Id> id,bool small,std::optional<mrs::Id> folderId):gain(!small),target(id),folder(std::move(folderId)),owner(d),mini(small){
    addChildComponent(folderToggle);addChildComponent(folderMenuButton);folderToggle.setComponentID("mixer-folder-toggle");folderMenuButton.setComponentID("mixer-folder-menu");
    folderToggle.onClick=[this]{if(!folder)return;const auto p=owner.project();const auto f=std::find_if(p->folders.begin(),p->folders.end(),[&](const auto& item){return item.id==*folder;});if(f==p->folders.end())return;const auto value=*f;owner.run([&]{owner.app.update_track_folder(value.id,value.name,!value.collapsed);});};
    folderMenuButton.onClick=[this]{if(folder)owner.showFolderMenu(*folder,folderMenuButton.getScreenBounds().getBottomLeft());};
    addChildComponent(nameEditor);nameEditor.setComponentID("track-name-editor");nameEditor.setTitle("Track name");
    nameEditor.setMultiLine(false);nameEditor.setFont(ui::Theme::stripFont());nameEditor.setIndents(0,0);
    nameEditor.setColour(juce::TextEditor::backgroundColourId,juce::Colour(panel));
    nameEditor.setColour(juce::TextEditor::focusedOutlineColourId,juce::Colour(accent));
    nameEditor.onReturnKey=[this]{finishNameEdit(true);};nameEditor.onEscapeKey=[this]{finishNameEdit(false);};
    nameEditor.onFocusLost=[this]{finishNameEdit(true);};
    pan.rotary=mini;gain.setTitle("Channel gain");pan.setTitle("Channel pan");
    gain.setDescription("Arrow keys adjust gain; Ctrl for fine adjustment");pan.setDescription("Arrow keys adjust pan; Ctrl for fine adjustment");
    mute.setTitle("Mute");solo.setTitle("Solo");arm.setTitle("Arm recording");monitor.setTitle("Input monitoring");
    for(auto* c:{static_cast<juce::Component*>(&gain),static_cast<juce::Component*>(&pan)})addAndMakeVisible(c);
    for(auto* b:{&mute,&solo,&arm,&monitor,&input,&output,&add}){b->getProperties().set("stripFont",true);b->getProperties().set("flatMixerButton",!mini);addAndMakeVisible(b);}
    mute.onClick=[this]{owner.run([&]{auto m=track().mix;m.mute=!m.mute;owner.app.set_track_mix(*target,m);});};
    solo.onClick=[this]{owner.run([&]{auto m=track().mix;m.solo=!m.solo;owner.app.set_track_mix(*target,m);});};
    arm.onClick=[this]{owner.run([&]{owner.app.set_track_armed(*target,!owner.app.track_armed(*target));sync();});};
    monitor.onClick=[this]{owner.run([&]{const auto t=track();if(t.kind==mrs::TrackKind::instrument)owner.app.set_midi_input(*target,t.midi_input,t.midi_channel,!t.midi_monitor);else owner.app.set_track_monitoring(*target,!t.input_monitor);});};
    arm.setComponentID("track-arm");monitor.setComponentID("track-monitor");add.setComponentID("mixer-add-insert");
    mute.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffd94747));solo.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffd5b63e));solo.setColour(juce::TextButton::textColourOnId,juce::Colour(0xff212830));
    arm.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xffd94747));
    arm.setTooltip("Arm recording (enables input monitoring)");monitor.setTooltip("Input monitoring");add.setTooltip("Add insert");
    input.onClick=[this]{inputMenu();};add.onClick=[this]{owner.insertMenu(target);};
    for(auto* b:{&instrument,&piano}){addAndMakeVisible(b);b->onClick=[this]{openInstrument();};}
    instrument.setComponentID("channel-instrument");piano.setComponentID("channel-piano");piano.setTitle("Open instrument");piano.setTooltip("Open or raise this channel's instrument");
    instrument.getProperties().set("stripFont",true);instrument.getProperties().set("flatMixerButton",true);
    // The instrument shortcut uses the same flat mixer chrome in normal mode.
    piano.getProperties().set("stripFont",true);piano.getProperties().set("flatMixerButton",true);
    colorField.setComponentID("channel-color");colorField.setTitle("Channel color");colorField.setTooltip("Choose channel color (32 colors)");addAndMakeVisible(colorField);colorField.onClick=[this]{colorMenu();};
    output.onClick=[this]{if(target)owner.routeMenu(*target);else owner.textDialog("Master hardware outputs (1,2...)","1,2",[this](auto s){owner.app.set_hardware_output({},mrs::desktop::parse_outputs(s.toStdString()));});};
    sends.getProperties().set("stripFont",true);sends.getProperties().set("flatMixerButton",true);
    sends.setComponentID("mixer-add-send");sends.setTooltip("Add send; choose destination");sendBody.addAndMakeVisible(sends);
    sends.onClick=[this]{owner.run([&]{if(target)owner.sendsMenu(*target,{},&sends);});};
    addAndMakeVisible(sendViewport);sendViewport.setComponentID("mixer-sends");sendViewport.setViewedComponent(&sendBody,false);sendViewport.setScrollBarsShown(true,false);
    insertViewport.setComponentID("mixer-inserts");
    allPower.getProperties().set("flatMixerButton",true);allPower.getProperties().set("stripFont",true);allPower.setComponentID("mixer-inserts-power");allPower.setTitle("Insert FX power");addAndMakeVisible(allPower);
    juce::Component::SafePointer<Strip> powerSafe(this);allPower.onClick=[powerSafe]{if(!powerSafe)return;const auto store=powerSafe->owner.app.services().projects;const auto revision=store->state().revision;const auto destination=powerSafe->target;juce::MessageManager::callAsync([powerSafe,store,revision,destination]{if(powerSafe&&powerSafe->owner.app.services().projects==store&&store->state().revision==revision)powerSafe->owner.run([&]{powerSafe->owner.toggleInsertPower(destination);});});};
    addChildComponent(sendDivider);sendDivider.setComponentID("mixer-send-divider");sendDivider.setTitle("Resize inserts and sends");
    sendDivider.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);sendDivider.addMouseListener(this,false);sendDivider.setVisible(!mini);
    gain.preview=[this](double v){preview(true,v);};pan.preview=[this](double v){preview(false,v);};
    gain.commit=[this](double v){commit(true,v);};pan.commit=[this](double v){commit(false,v);};
    addAndMakeVisible(insertViewport);insertViewport.setViewedComponent(&insertBody,false);insertViewport.addMouseListener(this,true);insertViewport.setScrollBarsShown(true,false);insertViewport.setVisible(!mini);
    gain.cancel=pan.cancel=[this]{owner.cancelPreview();sync();};sync();
}
mrs::Track Strip::track() const{if(target)for(const auto& t:owner.project()->tracks)if(t.id==target)return t;return {};}
juce::Rectangle<int> Strip::nameBounds() const{
    if(folder)return {6,3,juce::jmax(0,getWidth()-12),21};
    if (mini && target) if (const auto* layout = owner.trackLayout())
        if (const auto* item = layout->find(getHeight(), "name"))
            return layout->rectangle(getHeight(), getWidth(), *item, "name");
    if(owner.assetSkin()&&mini)return {32,3,juce::jmax(0,getWidth()-144),21};
    return {mini?8:25,3,juce::jmax(0,mini?getWidth()-145:getWidth()-53),21};
}
void Strip::mouseDown(const juce::MouseEvent& e){
    if(e.mods.isPopupMenu()){if(dynamic_cast<juce::Button*>(e.eventComponent)||e.eventComponent->findParentComponentOfClass<juce::Button>())return;if(folder)owner.showFolderMenu(*folder,e.getScreenPosition());else owner.showChannelMenu(target,e.getScreenPosition());return;}
    if(e.eventComponent==&sendDivider&&e.mods.isLeftButtonDown()){resizingSends=true;sendDragY=e.getScreenY();sendDragHeight=sendViewport.getHeight();return;}
    if(target)owner.selectTrack(*target,e.mods);else owner.selectedTrack.reset();dragTitle=target&&!folder&&!owner.app.recording()&&e.mods.isLeftButtonDown()&&e.getNumberOfClicks()==1&&(e.eventComponent==this||e.eventComponent==&insertBody||e.eventComponent==&sendBody||e.eventComponent==&insertViewport);owner.refresh();
}
void Strip::mouseDoubleClick(const juce::MouseEvent& e){dragTitle=false;if(folder){owner.showFolderMenu(*folder,e.getScreenPosition());return;}if(target&&e.mods.isLeftButtonDown()&&nameBounds().contains(e.getPosition()))beginNameEdit();}
void Strip::beginNameEdit(){
    if(!target||folder||owner.app.recording()||editingName)return;
    owner.selectedTrack=target;dragTitle=false;editingName=true;
    nameEditor.setText(label(track().name),false);nameEditor.setBounds(nameBounds());nameEditor.setVisible(true);
    nameEditor.grabKeyboardFocus();nameEditor.selectAll();repaint();
}
void Strip::finishNameEdit(bool accept){
    if(!editingName)return;editingName=false;
    const auto text=nameEditor.getText();const auto id=target;const auto store=owner.app.services().projects;
    nameEditor.setVisible(false);repaint();
    if(!accept||!id||text.trim().isEmpty())return;
    juce::Component::SafePointer<Strip> safe(this);
    // Refresh may replace strips: leave the TextEditor callback before committing.
    juce::MessageManager::callAsync([safe,store,id,text]{
        if(!safe||safe->owner.app.services().projects!=store||safe->owner.app.recording())return;
        const auto p=safe->owner.project();const auto at=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==id;});
        if(at==p->tracks.end()||text==label(at->name))return;
        safe->owner.run([&]{safe->owner.app.rename_track(*id,text.toStdString());});
    });
}
void Strip::mouseDrag(const juce::MouseEvent& e){if(resizingSends){owner.resizeSendArea(sendDragHeight+sendDragY-e.getScreenY());return;}if(dragTitle&&target&&e.getDistanceFromDragStart()>5&&!owner.isDragAndDropActive())owner.startDragging("mrs-track:"+label(target->value),this);}
void Strip::mouseUp(const juce::MouseEvent&){resizingSends=false;dragTitle=false;}
mrs::Track::Mix Strip::adjusted(bool volume,double v) const{auto m=track().mix;if(volume)m.gain=static_cast<float>(std::pow(10.,(-60+72*v)/20));else m.pan=static_cast<float>(2*v-1);return m;}
void Strip::preview(bool volume,double v){if(folder&&!target)return;owner.setPreview(target,adjusted(volume,v),target?owner.project()->master_gain:adjusted(volume,v).gain,[this]{return active();});}
void Strip::commit(bool volume,double v){if(folder&&!target)return;owner.run([&]{owner.finishPreview();if(target)owner.app.set_track_mix(*target,adjusted(volume,v));else owner.app.set_master_gain(adjusted(volume,v).gain);owner.app.poll();owner.refresh(true);});}
void Strip::refreshMidiStatus(){const auto t=track();if(t.kind==mrs::TrackKind::instrument){const auto status=label(owner.app.midi_status(t.id));input.setButtonText(status);input.setTooltip(status+" — click to select MIDI port / channel");}}
void Strip::syncFolderPresentation(){
    const bool isFolder=folder.has_value();folderToggle.setVisible(isFolder);folderMenuButton.setVisible(isFolder);
    if(!isFolder)return;
    if(!folderIcon)folderIcon=juce::Drawable::createFromImageData(MRConceptAssets::folder_svg,MRConceptAssets::folder_svgSize);
    setComponentID("mixer-folder-"+label(folder->value));
    for(auto* component:{static_cast<juce::Component*>(&insertViewport),static_cast<juce::Component*>(&sendViewport),static_cast<juce::Component*>(&sendDivider),static_cast<juce::Component*>(&allPower),static_cast<juce::Component*>(&add),static_cast<juce::Component*>(&sends),static_cast<juce::Component*>(&input),static_cast<juce::Component*>(&output),static_cast<juce::Component*>(&arm),static_cast<juce::Component*>(&monitor),static_cast<juce::Component*>(&instrument),static_cast<juce::Component*>(&piano),static_cast<juce::Component*>(&colorField)})component->setVisible(false);
    gain.setVisible(true);pan.setVisible(true);mute.setVisible(true);solo.setVisible(true);
    for(auto* component:{static_cast<juce::Component*>(&gain),static_cast<juce::Component*>(&pan),static_cast<juce::Component*>(&mute),static_cast<juce::Component*>(&solo)})component->setEnabled(target.has_value());
    const auto p=owner.project();for(const auto& f:p->folders)if(f.id==*folder){folderToggle.setButtonText(f.collapsed?">":"v");folderToggle.setTitle(f.collapsed?"Expand folder":"Collapse folder");folderToggle.setTooltip(folderToggle.getTitle());setTooltip(label(f.name)+(target?" — linked Bus controls":" — link a Bus to enable channel controls"));break;}
    folderMenuButton.setTitle("Track folder menu");folderToggle.setEnabled(!owner.app.recording());folderMenuButton.setEnabled(!owner.app.recording());
}
void Strip::sync(){
    if(folder&&!target){applyAssetDesign();gain.value=60./72.;pan.value=.5;mute.setToggleState(false,juce::dontSendNotification);solo.setToggleState(false,juce::dontSendNotification);syncFolderPresentation();resized();repaint();return;}
    folderToggle.setVisible(false);folderMenuButton.setVisible(false);if(!folder)setComponentID("");
    gain.setEnabled(true);pan.setEnabled(true);mute.setEnabled(target.has_value());solo.setEnabled(target.has_value());insertViewport.setVisible(!mini);sendDivider.setVisible(!mini);
    const auto t=track();gain.setTitle(label(t.name)+" gain");pan.setTitle(label(t.name)+" pan");const auto mix=t.mix;float v=target?mix.gain:owner.project()->master_gain;
    applyAssetDesign();
    if(editingName&&owner.app.recording())finishNameEdit(false);
    if(!gain.dragging())gain.value=juce::jlimit(0.,1.,(20*std::log10(juce::jmax(.001f,v))+60)/72.);if(!pan.dragging())pan.value=(mix.pan+1)/2.;
    gain.setTooltip(juce::String(-60+gain.value*72,1)+" dB — drag the fill endpoint; Ctrl fine; arrows; compact double-click resets 0 dB");
    mute.setVisible(target.has_value());solo.setVisible(target.has_value());pan.setVisible(target.has_value());
    arm.setVisible(target&&(t.kind==mrs::TrackKind::audio||t.kind==mrs::TrackKind::instrument));monitor.setVisible(arm.isVisible());input.setVisible(monitor.isVisible()&&(mini||t.kind==mrs::TrackKind::instrument));instrument.setVisible(!mini&&target&&t.kind==mrs::TrackKind::instrument);piano.setVisible(target&&t.kind==mrs::TrackKind::instrument);colorField.setVisible(target.has_value());colorField.setEnabled(!owner.app.recording());colorField.rgb=t.color.value_or(ObjectPalette::defaultClip);colorField.repaint();const auto instrumentChain=owner.chain(target);instrument.setButtonText(instrumentChain.empty()?"Choose instrument":insertName(instrumentChain.front()));instrument.setTooltip(instrumentChain.empty()?"No instrument — choose from instrument browser":label(owner.app.midi_status(t.id))+" — "+instrument.getButtonText());
    add.setVisible(!mini);output.setVisible(!mini);sends.setVisible(!mini&&target.has_value());
    mute.setToggleState(mix.mute,juce::dontSendNotification);solo.setToggleState(mix.solo,juce::dontSendNotification);
    arm.setToggleState(target&&owner.app.track_armed(*target),juce::dontSendNotification);monitor.setToggleState(t.kind==mrs::TrackKind::instrument?t.midi_monitor:t.input_monitor,juce::dontSendNotification);
    setTooltip(label(t.name)+" — "+(t.fx_channel?"FX return":t.kind==mrs::TrackKind::bus?"Bus":t.kind==mrs::TrackKind::audio?"Audio":t.kind==mrs::TrackKind::instrument?"Instrument":"MIDI"));
    input.setButtonText(t.input==-2?"Input: default":t.input==-1?"Input: off":juce::String(t.input_stereo?"Stereo ":"Mono ")+juce::String(t.input+1));
    if(t.kind!=mrs::TrackKind::instrument)input.setTooltip(input.getButtonText()+" — select audio input");
    input.setComponentID(t.kind==mrs::TrackKind::instrument?"midi-input-status":"");
    if(t.kind==mrs::TrackKind::instrument){input.setButtonText(label(owner.app.midi_status(t.id)));input.setTooltip("MIDI input / channel; instrument button below Sends in Mix");}
    juce::String out=t.output?"Out: Missing":"Out: Master";if(t.output)for(const auto& x:owner.project()->tracks)if(x.id==t.output)out="Out: "+label(x.name);
    const auto& physical=target?t.hardware_outputs:owner.project()->master_outputs;
    juce::String channels;for(const auto channel:physical){if(channels.isNotEmpty())channels+=",";channels+=juce::String(channel+1);}
    if(channels.isNotEmpty())out="Out: HW "+channels;
    output.setButtonText(target?out:channels.isEmpty()?"Out: Default":out);
    output.setTooltip((target?label(t.name):juce::String("Master"))+" → "+(target?out:channels.isEmpty()?juce::String("first selected hardware outputs"):out)+"; click to choose output");
    output.setTitle(output.getTooltip());
    sendViewport.setVisible(!mini&&target.has_value());insertList();sendList();syncFolderPresentation();resized();repaint();gain.repaint();pan.repaint();
}
void Strip::insertList(){const auto chain=owner.chain(target);
    const auto start=target&&track().kind==mrs::TrackKind::instrument?std::min(std::size_t{1},chain.size()):0;
    const bool allOff=chain.size()>start&&std::all_of(chain.begin()+static_cast<std::ptrdiff_t>(start),chain.end(),[](const auto& fx){return fx.bypass;});
    allPower.setVisible(!mini);allPower.setEnabled(chain.size()>start&&!owner.app.recording());allPower.setToggleState(allOff,juce::dontSendNotification);
    const bool mixed=!allOff&&chain.size()>start&&std::any_of(chain.begin()+static_cast<std::ptrdiff_t>(start),chain.end(),[](const auto& fx){return fx.bypass;});
    allPower.getProperties().set("mrMixedPower",mixed);allPower.setTooltip(allOff?"Restore insert FX power states":mixed?"Mixed FX power states — bypass all insert FX":"Bypass all insert FX (instrument unchanged)");insertButtons.clear();if(mini)return;
    for(std::size_t slotIndex=start;slotIndex<chain.size();++slotIndex){const auto& n=chain[slotIndex];auto b=std::make_unique<MixerInsert>(insertName(n),n.bypass);const auto slot=n.id;
        b->setComponentID("mixer-insert:"+label(slot.value));juce::Component::SafePointer<MixerInsert> control(b.get());b->editor.onClick=[this,slot,control]{if(control&&!control->dragged)owner.openInsert(target,slot);};
        b->beginDrag=[this,slot,control]{if(!control||owner.isDragAndDropActive()||owner.app.recording())return;
            const auto store=owner.app.services().projects;owner.insertDrag=Desktop::InsertDrag{store,store->state().revision,target,slot};owner.startDragging("mrs-insert:"+label(slot.value),control.getComponent());};
        juce::Component::SafePointer<Strip> safe(this);const auto store=owner.app.services().projects;
        b->menu=[safe,store,slot](juce::Point<int> point){if(safe&&safe->owner.app.services().projects==store)safe->owner.insertContextMenu(safe->target,slot,point);};
        b->bypass.onClick=[safe,store,slot]{
            // applyChain refreshes and replaces these buttons. Run after the click
            // returns; reject actions queued for a closed strip or different project.
            juce::MessageManager::callAsync([safe,store,slot]{if(!safe||safe->owner.app.services().projects!=store)return;
                safe->owner.run([&]{auto effects=safe->owner.chain(safe->target);auto at=std::find_if(effects.begin(),effects.end(),[&](const auto& effect){return effect.id==slot;});
                    if(at==effects.end())return;at->bypass=!at->bypass;safe->owner.applyChain(safe->target,std::move(effects));});
            });
        };
        insertBody.addAndMakeVisible(*b);insertButtons.push_back(std::move(b));}
}
void Strip::sendList(){
    if(mini||!target)return;const auto t=track();std::vector<mrs::Id> destinations;for(const auto& send:t.sends)destinations.push_back(send.bus);
    if(destinations!=sendDestinations){
        for(auto& button:sendButtons)button->abort();sendButtons.clear();sendDestinations=destinations;
        const auto source=*target;const auto store=owner.app.services().projects;juce::Component::SafePointer<Strip> safe(this);
        for(const auto& bus:destinations){auto button=std::make_unique<SendLevel>();juce::Component::SafePointer<SendLevel> control(button.get());
            button->setComponentID("mixer-send:"+label(bus.value));
            button->preview=[safe,control,store,source,bus](float value){if(safe&&safe->owner.app.services().projects==store)
                safe->owner.setSendPreview(source,bus,value,[control]{return control&&control->dragging();});};
            button->cancel=[safe]{if(safe)safe->owner.cancelPreview();};
            button->commit=[safe,store,source,bus](float value){if(!safe||safe->owner.app.services().projects!=store)return;
                safe->owner.run([&]{safe->owner.finishPreview();const auto p=safe->owner.project();
                    for(const auto& track:p->tracks)if(track.id==source)for(std::size_t i=0;i<track.sends.size();++i)if(track.sends[i].bus==bus){safe->owner.app.set_send_gain(source,i,value);return;}
                    safe->owner.cancelPreview();});};
            button->menu=[safe,control,store,source,bus]{if(safe&&safe->owner.app.services().projects==store)safe->owner.run([&]{safe->owner.sendsMenu(source,bus,control.getComponent());});};
            button->toggleBypass=[safe,store,source,bus]{juce::MessageManager::callAsync([safe,store,source,bus]{
                if(!safe||safe->owner.app.services().projects!=store)return;
                safe->owner.run([&]{safe->owner.cancelPreview();const auto p=safe->owner.project();
                    for(const auto& track:p->tracks)if(track.id==source)for(std::size_t i=0;i<track.sends.size();++i)if(track.sends[i].bus==bus){safe->owner.app.set_send_bypassed(source,i,!track.sends[i].bypass);return;}});
            });};
            sendBody.addAndMakeVisible(*button);sendButtons.push_back(std::move(button));
        }
    }
    const auto p=owner.project();for(std::size_t i=0;i<t.sends.size();++i){juce::String name="Missing send";for(const auto& bus:p->tracks)if(bus.id==t.sends[i].bus)name=label(bus.name);sendButtons[i]->sync(name,t.sends[i].gain,t.sends[i].bypass);}
    sends.setEnabled(t.sends.size()<8);
}
void Strip::resized(){const int w=getWidth(),h=getHeight();if(mini){mute.setBounds(w-132,4,28,24);solo.setBounds(w-102,4,28,24);arm.setBounds(w-72,4,28,24);monitor.setBounds(w-42,4,28,24);
        gain.setBounds(8,32,w-70,36);pan.setBounds(w-64,34,58,32);input.setBounds(8,h-30,w-18,24);}
    else {allPower.setBounds(8,7,13,13);add.setBounds(w-26,4,20,19);
        const int top=h-108-owner.faderHeight;
        gain.setBounds(37,top,34,owner.faderHeight);pan.setBounds(6,top-27,w-12,23);
        const bool hasInstrument=target&&track().kind==mrs::TrackKind::instrument;const int areaHeight=juce::jmax(0,top-56-(hasInstrument?48:0)),sendContent=static_cast<int>(sendButtons.size())*24+20;
        const int gap=juce::jmin(8,areaHeight);
        const int maximum=juce::jmax(0,areaHeight-gap-24),minimum=juce::jmin(24,maximum);
        const int sendHeight=juce::jlimit(minimum,maximum,owner.sendAreaHeight<0?areaHeight/3:owner.sendAreaHeight);
        insertViewport.setBounds(6,27,w-12,areaHeight-sendHeight-gap);
        sendViewport.setBounds(6,27+areaHeight-sendHeight,w-12,sendHeight);
        sendDivider.setBounds(0,sendViewport.getY()-gap,w,gap);if(hasInstrument){instrument.setBounds(6,sendViewport.getBottom()+4,w-12,20);input.setBounds(6,instrument.getBottom()+2,w-12,20);}
        const int sendWidth=juce::jmax(1,w-12-(sendContent>sendHeight?sendViewport.getScrollBarThickness():0));
        sendBody.setSize(sendWidth,sendContent);
        for(std::size_t i=0;i<sendButtons.size();++i)sendButtons[i]->setBounds(0,static_cast<int>(i)*24,sendWidth,22);
        sends.setBounds(0,static_cast<int>(sendButtons.size())*24,sendWidth,20);
        const int insertWidth=w-12-(static_cast<int>(insertButtons.size())*20>insertViewport.getHeight()?insertViewport.getScrollBarThickness():0);
        insertBody.setSize(insertWidth,static_cast<int>(insertButtons.size())*20);
        for(std::size_t i=0;i<insertButtons.size();++i)insertButtons[i]->setBounds(0,static_cast<int>(i)*20,insertWidth,18);
        mute.setBounds(6,h-82,31,21);solo.setBounds(40,h-82,31,21);arm.setBounds(6,h-56,31,21);monitor.setBounds(40,h-56,31,21);output.setBounds(6,h-30,w-12,21);}
    if(owner.assetSkin()){
        if(mini){if(h<124){gain.setBounds(8,26,juce::jmax(40,w-110),26);pan.setBounds(w-98,27,24,24);input.setBounds(w-70,28,60,22);}else gain.setBounds(8,32,w-76,36);}
        constexpr int side=20,gap=4;
        if(mini){int x=w-12-(4*side+3*gap);for(auto* b:{&mute,&solo,&arm,&monitor}){b->setBounds(x,4,side,side);x+=side+gap;}}
        else{const int x=(w-(2*side+gap))/2;
            mute.setBounds(x,h-82,side,side);solo.setBounds(x+side+gap,h-82,side,side);
            arm.setBounds(x,h-56,side,side);monitor.setBounds(x+side+gap,h-56,side,side);}
    }
    if(mini){gain.compact=h<84;gain.setVisible(true);if(gain.compact)gain.setBounds(8,25,w-20,juce::jmin(16,h-26));pan.setVisible(!gain.compact);input.setVisible(!gain.compact&&monitor.isVisible());}
    if (mini && target) if (const auto* layout = owner.trackLayout()) {
        // Translate semantic JSON roles onto the existing JUCE controls. Their
        // callbacks, host parameters, focus, Undo and automation remain intact.
        const auto apply = [&](const char* role, juce::Component& component) {
            if (const auto* item = layout->find(h, role)) {
                component.setBounds(layout->rectangle(h, w, *item, role));
                if (!item->visible) component.setVisible(false);
            } else component.setVisible(false);
        };
        apply("mute", mute); apply("solo", solo);
        apply("record", arm); apply("monitor", monitor);
        apply("gain", gain); apply("pan", pan);
        apply("input", input); apply("output", output);
        if (const auto* item = layout->find(h, "gain"))
            gain.compact = (item->style == "micro");
        const auto* panItem = layout->find(h, "pan");
        pan.setVisible(target.has_value() && !gain.compact && panItem && panItem->visible);
        const auto* inputItem = layout->find(h, "input");
        input.setVisible(monitor.isVisible() && inputItem && inputItem->visible);
        const auto* outputItem = layout->find(h, "output");
        output.setVisible(target.has_value() && outputItem && outputItem->visible);
    }
    if(folder){folderToggle.setBounds(6,30,24,24);folderMenuButton.setBounds(getWidth()-30,30,24,24);}
    colorField.setBounds(mini?juce::Rectangle<int>(w-6,2,6,h-4):juce::Rectangle<int>(2,h-6,w-4,6));piano.setBounds(mini?juce::Rectangle<int>(6,2,22,22):juce::Rectangle<int>(7,h-107,20,18));nameEditor.setBounds(nameBounds());
}
void Strip::paint(juce::Graphics& g){const bool selected=target&&(owner.selectedTrack==target||owner.selectedTracks.contains(target->value));
    const auto* trackDesign = (mini && target) ? owner.trackLayout() : nullptr;
    const auto* skin=owner.assetSkin();const auto base=juce::Colour(skin?ConceptSkin::channel:panel);
    drawCard(g,getLocalBounds().toFloat(),base);if(selected){g.setColour(owner.selectionColour());g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),cornerRadius,borderWidth);}
    if(!mini){g.setColour(skin?base.interpolatedWith(owner.selectionColour(),selected?.08f:0.f):juce::Colour(selected?raised:panel));g.fillRoundedRectangle(juce::Rectangle<float>(1,1,static_cast<float>(getWidth()-2),22),cornerRadius);
        g.setColour(juce::Colour(separatorColour));g.fillRect(0,24,getWidth(),1);
        g.fillRect(6,getHeight()-88,getWidth()-12,1);
        if(!folder){const auto split=sendDivider.getBounds();g.setColour(base);g.fillRect(split);
        g.setColour(juce::Colour(separatorColour));g.fillRect(split.getX(),split.getCentreY(),split.getWidth(),1);}}
    if(trackHover){g.setColour(owner.selectionColour());if(mini)g.fillRect(0,dropBefore?0:getHeight()-3,getWidth(),3);else g.fillRect(dropBefore?0:getWidth()-3,0,3,getHeight());}
    const auto t=track();g.setFont(ui::Theme::stripFont());g.setColour(target||folder?juce::Colours::whitesmoke:juce::Colours::gold);
    if (skin && trackDesign && t.kind != mrs::TrackKind::instrument) {
        if (const auto* item = trackDesign->find(getHeight(), "icon"); item && item->visible) {
            const auto id = item->assetId.fromLastOccurrenceOf("/", false, false);
            const auto icon = (id == "track_guitar" || id == "track_guitar_compact")
                ? id : t.kind == mrs::TrackKind::bus ? juce::String("bus") : juce::String("wave");
            skin->draw(g, icon, trackDesign->rectangle(getHeight(), getWidth(), *item, "icon").toFloat());
        }
    } else if(skin&&mini&&t.kind!=mrs::TrackKind::instrument){
        const auto icon=!target?"track_master":t.kind==mrs::TrackKind::bus?"bus":t.kind==mrs::TrackKind::audio?"wave":"track_synth";
        skin->draw(g,icon,{static_cast<float>(mini?8:6),5,16,16});}
    juce::String channelName=target?label(t.name):"MASTER";if(folder)for(const auto& f:owner.project()->folders)if(f.id==*folder){channelName=label(f.name);break;}
    if(!editingName)g.drawText(channelName,nameBounds(),juce::Justification::left,true);
    if(folder){if(folderIcon)folderIcon->drawWithin(g,{static_cast<float>(getWidth()/2-10),32.f,20.f,20.f},juce::RectanglePlacement::centred,1.f);g.setColour(juce::Colour(secondaryText));const int captionHeight=juce::jlimit(0,32,pan.getY()-64);if(captionHeight>=12)g.drawFittedText(target?"Bus: "+label(t.name):"Link Bus",6,60,getWidth()-12,captionHeight,juce::Justification::centred,2);}
    mrs::audio::StereoPeak p=folder&&!target?mrs::audio::StereoPeak{}:owner.masterPeak;if(target){const auto tracks=owner.project()->tracks;for(std::size_t i=0;i<tracks.size();++i)if(tracks[i].id==target)p=owner.peaks[i];}
    for(int i=0;i<2;++i){const float peak=i==0?p.left:p.right;const auto level=juce::jlimit(0.f,1.f,(20.f*std::log10(juce::jmax(.000001f,peak))+60.f)/60.f);
        if (trackDesign && !trackDesign->find(getHeight(), "meter")) continue;
        if (!trackDesign && mini && getHeight()<84)continue;
        juce::Rectangle<int> r;
        if (trackDesign) {
            const auto* item = trackDesign->find(getHeight(), "meter");
            if (!item->visible) continue;
            const auto area = trackDesign->rectangle(getHeight(), getWidth(), *item, "meter");
            const int line = juce::jmax(1, (area.getHeight()-1)/2);
            r = {area.getX(), area.getY()+i*(line+1), area.getWidth(), line};
        } else r = mini?juce::Rectangle<int>(10,skin?(getHeight()<124?70:68)+i*6:70+i*7,getWidth()-82,skin?4:5):juce::Rectangle<int>(8+i*12,gain.getY(),8,gain.getHeight());
        if(skin){ConceptSkin::meter(g,r,level,mini,isEnabled());continue;}
        g.setColour(juce::Colour(0xff111518));g.fillRect(r);g.setColour(peak>.95f?juce::Colours::orange:juce::Colour(0xff38cbbf));
        if(mini)g.fillRect(r.withWidth(static_cast<int>(r.getWidth()*level)));else g.fillRect(r.withTop(r.getBottom()-static_cast<int>(r.getHeight()*level)));
    }
    const auto valueBounds=juce::Rectangle<int>(mini?10:6,mini?(skin?(getHeight()<124?52:80):82):getHeight()-107,mini?100:getWidth()-12,skin&&mini?14:18);
    if(skin&&!mini&&t.kind!=mrs::TrackKind::instrument){const auto icon=!target?"track_master":t.kind==mrs::TrackKind::bus?"bus":t.kind==mrs::TrackKind::audio?"wave":"track_synth";
        skin->draw(g,icon,{static_cast<float>(valueBounds.getX()+3),static_cast<float>(valueBounds.getY()+2),14,14});}
    auto textBounds=skin?valueBounds.reduced(5,0):valueBounds;if(skin&&!mini)textBounds.removeFromLeft(16);
    if(!skin||!mini||getHeight()>=72){g.setFont(ui::Theme::stripFont());g.setColour(juce::Colours::whitesmoke);g.drawText(juce::String(-60+gain.value*72,1)+" dB",textBounds,juce::Justification::left);}
    if(skin&&mini&&getHeight()>=72){g.setColour(juce::Colour(secondaryText));g.drawText(t.fx_channel?"FX RETURN":t.kind==mrs::TrackKind::bus?"BUS":t.kind==mrs::TrackKind::audio?"AUDIO":t.kind==mrs::TrackKind::instrument?"INSTRUMENT":"MIDI",118,valueBounds.getY(),getWidth()-130,14,juce::Justification::centredRight,true);}
}
void Strip::colorMenu(){if(!target||owner.app.recording())return;
    const auto id=*target;const auto store=owner.app.services().projects;const auto color=std::make_shared<std::uint32_t>(track().color.value_or(ObjectPalette::defaultClip));
    juce::PopupMenu menu;menu.setLookAndFeel(&owner.theme);menu.addItem(1,"Default channel color");
    auto palette=std::make_unique<MenuColourPalette>(owner.theme,*color,color,true,true);palette->setName("Channel colors");menu.addCustomItem(10,std::move(palette),nullptr,"Channel colors (32)");
    juce::Component::SafePointer<Strip> safe(this);menu.showMenuAsync(popup(&colorField),[safe,id,store,color](int result){if(!safe||!result||safe->owner.app.services().projects!=store)return;safe->owner.run([&]{safe->owner.app.set_track_color(id,result==1?std::nullopt:std::optional<std::uint32_t>{*color});});});
}
void Strip::openInstrument(){if(!target||track().kind!=mrs::TrackKind::instrument)return;
    const auto effects=owner.chain(target);if(!effects.empty()){owner.openInsert(target,effects.front().id);return;}
    juce::PopupMenu menu;menu.setLookAndFeel(&owner.theme);for(std::size_t i=0;i<owner.catalog.size();++i)if(owner.catalog[i].instrument.value_or(false))menu.addItem(1+static_cast<int>(i),label(owner.catalog[i].name));
    if(menu.getNumItems()==0)menu.addItem(0,"No instruments scanned — scan the plugin browser",false);
    const auto store=owner.app.services().projects;const auto id=*target;juce::Component::SafePointer<Strip> safe(this);
    menu.showMenuAsync(popup(&instrument),[safe,store,id](int result){if(safe&&result&&safe->owner.app.services().projects==store&&safe->owner.chain(id).empty())safe->owner.dropPlugin(id,static_cast<std::size_t>(result-1));});
}
void Strip::inputMenu(){if(!target)return;
    if(track().kind==mrs::TrackKind::instrument){
        const auto current=track();const auto ports=mrs::desktop::MidiInputs::ports();juce::PopupMenu midi;
        midi.addItem(1,"MIDI input: off",true,current.midi_input.empty());
        for(std::size_t i=0;i<ports.size();++i)midi.addItem(10+static_cast<int>(i),label(ports[i].name),true,current.midi_input==ports[i].id);
        if(!current.midi_input.empty()&&std::none_of(ports.begin(),ports.end(),[&](const auto& port){return port.id==current.midi_input;}))midi.addItem(3,"Missing: "+label(current.midi_input),false,true);
        juce::PopupMenu channels;channels.addItem(1000,"All channels",true,current.midi_channel<0);
        for(int ch=0;ch<16;++ch)channels.addItem(1001+ch,"Channel "+juce::String(ch+1),true,current.midi_channel==ch);
        midi.addSubMenu("Input channel",channels);
        const auto outputs=mrs::desktop::MidiOutputs::ports();juce::PopupMenu out,outputChannels;
        out.addItem(2000,"Off",true,current.midi_output.empty());
        for(std::size_t i=0;i<outputs.size();++i)out.addItem(2010+static_cast<int>(i),label(outputs[i].name),outputs[i].id!=current.midi_input,current.midi_output==outputs[i].id);
        if(!current.midi_output.empty()&&std::none_of(outputs.begin(),outputs.end(),[&](const auto& p){return p.id==current.midi_output;}))out.addItem(2002,"Missing: "+label(current.midi_output),false,true);
        outputChannels.addItem(3000,"Original channels",true,current.midi_output_channel<0);
        for(int ch=0;ch<16;++ch)outputChannels.addItem(3001+ch,"Channel "+juce::String(ch+1),true,current.midi_output_channel==ch);
        midi.addSubMenu("External MIDI output",out);midi.addSubMenu("Output channel",outputChannels);
        midi.addItem(4000,"Panic — all notes off");midi.addItem(4001,"Reconnect MIDI ports",!owner.app.recording());
        midi.addSeparator();midi.addItem(2,label(owner.app.midi_status(current.id)),false);
        midi.setLookAndFeel(&owner.theme);midi.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&input),[safe=juce::Component::SafePointer<Strip>(this),ports,outputs,current](int selected){if(!safe||!selected||selected==2||selected==2002)return;safe->owner.run([&]{
            if(selected==4000){safe->owner.app.midi_panic();return;}if(selected==4001){safe->owner.app.reconnect_midi();return;}
            if(selected>=3000){safe->owner.app.set_midi_output(current.id,current.midi_output,selected==3000?-1:selected-3001);return;}
            if(selected>=2000){auto output=current.midi_output;if(selected==2000)output.clear();else if(static_cast<std::size_t>(selected-2010)<outputs.size())output=outputs[static_cast<std::size_t>(selected-2010)].id;safe->owner.app.set_midi_output(current.id,output,current.midi_output_channel);return;}
            auto port=current.midi_input;auto channel=current.midi_channel;if(selected==1)port.clear();else if(selected>=1000)channel=selected==1000?-1:selected-1001;else if(selected>=10&&static_cast<std::size_t>(selected-10)<ports.size())port=ports[static_cast<std::size_t>(selected-10)].id;safe->owner.app.set_midi_input(current.id,port,channel,current.midi_monitor);
        });});return;
    }
juce::PopupMenu m;m.addItem(1,"Default");m.addItem(2,"Off");const auto names=owner.app.input_names();
    for(std::size_t i=0;i<names.size();++i){m.addItem(10+static_cast<int>(i),"Mono "+label(names[i]));if(i+1<names.size())m.addItem(100+static_cast<int>(i),"Stereo "+label(names[i])+" / "+label(names[i+1]));}
    m.setLookAndFeel(&owner.theme);juce::Component::SafePointer<Strip> safe(this);m.showMenuAsync(popup(&input),[safe](int n){if(safe&&n)safe->owner.run([&]{safe->owner.app.set_track_input(*safe->target,n==1?-2:n==2?-1:n>=100?n-100:n-10,n>=100);safe->sync();});});
}
bool Strip::isInterestedInDragSource(const SourceDetails& s){if(folder)return false;const auto text=s.description.toString();if(text.startsWith("mrs-insert:"))return !mini&&owner.insertDrag.has_value();if(text=="mrs-sample-files"){const auto files=owner.browser->selectedSamples();return target&&track().kind==mrs::TrackKind::instrument&&!files.isEmpty()&&juce::File(files[0]).hasFileExtension("mid;midi");}if(text.startsWith("mrs-track:"))return target.has_value();const auto number=text.substring(5);return text.startsWith("vst3:")&&!number.isEmpty()&&number.containsOnly("0123456789");}
std::size_t Strip::pluginInsertionAt(juce::Point<int> point) const {
    const auto count=insertButtons.size();const auto offset=target&&track().kind==mrs::TrackKind::instrument&&!track().inserts.empty()?std::size_t{1}:0;
    auto position=count;
    if(insertViewport.getBounds().contains(point))position=static_cast<std::size_t>(juce::jlimit(0,static_cast<int>(count),(point.y-insertViewport.getY()+insertViewport.getViewPositionY()+10)/20));
    // Translate the visible FX boundary to the existing chain's instrument offset.
    return position+offset;
}
void Strip::paintOverChildren(juce::Graphics& g){
    if(!pluginHover)return;const auto area=insertViewport.getBounds();if(area.isEmpty())return;
    juce::Graphics::ScopedSaveState save(g);g.reduceClipRegion(area);
    g.setColour(juce::Colour(insertDropValid?accent:0xffe68f88));
    const int y=juce::jlimit(area.getY()+1,area.getBottom()-2,area.getY()+static_cast<int>(*pluginHover-(target&&track().kind==mrs::TrackKind::instrument&&!track().inserts.empty()?1:0))*20-insertViewport.getViewPositionY());
    g.fillRect(area.getX()+1,y,area.getWidth()-2,2);
    if(insertDropMode.isNotEmpty()){g.setFont(10.f);g.drawText(insertDropMode,area.withY(juce::jlimit(area.getY(),area.getBottom()-14,y+2)).withHeight(14),juce::Justification::centredRight);}
}
void Strip::itemDragMove(const SourceDetails& s){
    trackHover=target&&s.description.toString().startsWith("mrs-track:");
    if(insertViewport.getBounds().contains(s.localPosition))insertViewport.autoScroll(s.localPosition.x-insertViewport.getX(),s.localPosition.y-insertViewport.getY(),12,4);
    insertDropMode.clear();insertDropValid=true;pluginHover.reset();
    if(s.description.toString().startsWith("mrs-insert:")&&isInterestedInDragSource(s)){pluginHover=pluginInsertionAt(s.localPosition);insertDropValid=owner.insertDropAllowed(target);insertDropMode=insertDropValid?(owner.insertDrag->source==target?"Reorder":juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown()?"Move":"Copy"):"Invalid drop";}
    if(!trackHover&&s.description.toString().startsWith("vst3:")&&isInterestedInDragSource(s)&&insertButtons.size()<8&&!owner.app.recording())pluginHover=pluginInsertionAt(s.localPosition);
    dropBefore=mini?s.localPosition.y<getHeight()/2:s.localPosition.x<getWidth()/2;repaint();
}
void Strip::itemDragExit(const SourceDetails&){trackHover=false;pluginHover.reset();repaint();}
void Strip::itemDropped(const SourceDetails& s){
    if(!getLocalBounds().contains(s.localPosition))return;
    trackHover=false;pluginHover.reset();repaint();if(!isInterestedInDragSource(s))return;
    const auto text=s.description.toString();
    if(text.startsWith("mrs-insert:")){if(!owner.insertDropAllowed(target))return;const auto drag=*owner.insertDrag;const auto destination=target;const auto boundary=pluginInsertionAt(s.localPosition);const bool move=juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown();
        juce::Component::SafePointer<Desktop> safe(&owner);juce::MessageManager::callAsync([safe,drag,destination,boundary,move]{if(!safe||safe->app.services().projects!=drag.store||drag.store->state().revision!=drag.revision)return;safe->run([&]{if(drag.source!=destination)safe->closeEditors(true);safe->app.transfer_insert(drag.source,drag.slot,destination,boundary,move);safe->refresh(true);});});return;}
    if(text=="mrs-sample-files"){const auto files=owner.browser->selectedSamples();const auto destination=target;juce::Component::SafePointer<Desktop> safe(&owner);juce::MessageManager::callAsync([safe,files,destination]{if(safe)safe->run([&]{safe->importMidiFiles(files,destination,0);});});}
    else if(text.startsWith("mrs-track:")){const auto p=owner.project();for(std::size_t i=0;i<p->tracks.size();++i)if(p->tracks[i].id==target){const bool before=mini?s.localPosition.y<getHeight()/2:s.localPosition.x<getWidth()/2;owner.dropTrack(text,i+(before?0:1));break;}}
    else {
        const auto position=pluginInsertionAt(s.localPosition);const auto destination=target;
        const auto store=owner.app.services().projects;const auto revision=store->state().revision;
        juce::Component::SafePointer<Desktop> safe(&owner);const auto plugin=static_cast<std::size_t>(text.substring(5).getIntValue());
        if(plugin>=owner.catalog.size())return;const auto descriptor=owner.catalog[plugin];
        // Refresh destroys strip/source pointers: defer until JUCE releases the drag.
        juce::MessageManager::callAsync([safe,destination,position,plugin,descriptor,store,revision]{if(safe&&safe->app.services().projects==store&&store->state().revision==revision&&plugin<safe->catalog.size()&&safe->catalog[plugin]==descriptor)safe->dropPlugin(destination,plugin,position);});
    }
}

void Desktop::dropTrack(const juce::String& description,std::size_t boundary){const auto p=project();const mrs::Id id{description.substring(10).toStdString()};const auto found=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==id;});if(found==p->tracks.end())return;
    const auto source=static_cast<std::size_t>(found-p->tracks.begin());boundary=std::min(boundary,p->tracks.size());const auto index=boundary-(source<boundary?1:0);if(index==source)return;
    // Defer past JUCE's drag callback; reject a drop queued for a stale document.
    const auto store=app.services().projects;const auto dropRevision=store->state().revision;
    juce::Component::SafePointer<Desktop> safe(this);juce::MessageManager::callAsync([safe,id,index,store,dropRevision]{if(safe&&safe->app.services().projects==store&&store->state().revision==dropRevision)safe->run([&]{safe->app.reorder_track(id,index);safe->selectedTrack=id;});});
}
bool Desktop::isInterestedInDragSource(const SourceDetails& s){return (s.description.toString().startsWith("mrs-track:")||s.description.toString().startsWith("vst3:")||(s.description.toString()=="mrs-sample-files"&&!browser->selectedSamples().isEmpty()&&juce::File(browser->selectedSamples()[0]).hasFileExtension("mid;midi")));}
void Desktop::itemDropped(const SourceDetails& s){if(!isInterestedInDragSource(s)||!mixArea.contains(s.localPosition))return;if(s.description.toString().startsWith("mrs-track:"))dropTrack(s.description.toString(),project()->tracks.size());else if(s.description.toString()=="mrs-sample-files"){const auto files=browser->selectedSamples();juce::Component::SafePointer<Desktop> safe(this);juce::MessageManager::callAsync([safe,files]{if(safe)safe->run([&]{safe->importMidiFiles(files,{},0);});});}else {const auto index=static_cast<std::size_t>(s.description.toString().substring(5).getLargeIntValue());juce::Component::SafePointer<Desktop> safe(this);juce::MessageManager::callAsync([safe,index]{if(safe)safe->addInstrument(index);});}}

namespace {
class LaneButton final:public juce::Button {
public:
    LaneButton(Desktop& desktop,juce::String name,int kind):Button(name),owner(desktop),kind(kind){setTitle(name);setTooltip("Show/hide "+name);setDescription("Toggle musical lane visibility");}
    void paintButton(juce::Graphics& g,bool hover,bool down)override{
        auto r=getLocalBounds().toFloat().reduced(1.f);
        if(!owner.assetSkin()||kind==4){g.setColour((getToggleState()?owner.selectionColour():juce::Colour(raised)).withMultipliedBrightness(hover||down?1.15f:1.f));g.fillRoundedRectangle(r,3.f);}
        if(owner.assetSkin()&&getToggleState()&&kind!=4){g.setColour(owner.selectionColour());g.fillEllipse(r.getCentreX()-1,r.getBottom()-2,2,2);}
        g.setColour(owner.assetSkin()?(kind==4&&getToggleState()?juce::Colour(ConceptSkin::canvas):getToggleState()?owner.selectionColour():juce::Colour(primaryText)):juce::Colour(primaryText));const float x=getWidth()/2.f,y=getHeight()/2.f;juce::Path p;
        if(kind==0){p.startNewSubPath(x-4,y+5);p.lineTo(x-4,y-5);p.lineTo(x+5,y-5);p.lineTo(x+2,y-1);p.lineTo(x-4,y-1);}
        else if(kind==1){if(owner.assetSkin()){p.addRectangle(x-6,y-4,5.f,8.f);p.addRectangle(x+1,y-4,5.f,8.f);}else{g.drawRect(x-6,y-4,5.f,8.f,1.f);g.drawRect(x+1,y-4,5.f,8.f,1.f);}}
        else if(kind==2){if(owner.assetSkin()){p.addRectangle(x-6,y-5,12.f,10.f);for(int i=0;i<3;++i){p.startNewSubPath(x-3+i*3,y-5);p.lineTo(x-3+i*3,y+5);}}else{g.drawRect(x-6,y-5,12.f,10.f,1.f);for(int i=0;i<3;++i)g.drawVerticalLine(juce::roundToInt(x-3+i*3),y-5,y+5);}}
        else if(kind==3){p.startNewSubPath(x-5,y+5);p.lineTo(x,y-6);p.lineTo(x+5,y+5);p.closeSubPath();p.startNewSubPath(x,y+3);p.lineTo(x+4,y-4);}
        else{g.setFont(Theme::detailFont());g.drawText("3/4",getLocalBounds(),juce::Justification::centred);}
        if(owner.assetSkin()&&getToggleState()&&kind!=4)ConceptSkin::glow([&](juce::Point<float> offset,float a){juce::Graphics::ScopedSaveState s(g);g.addTransform(juce::AffineTransform::translation(offset.x,offset.y));g.setOpacity(a);g.strokePath(p,juce::PathStrokeType(1.f));});
        g.strokePath(p,juce::PathStrokeType(1.f));if(hasKeyboardFocus(false)){g.setColour(juce::Colours::white);if(owner.assetSkin()&&kind!=4)g.drawLine(4.f,getHeight()-1.f,getWidth()-4.f,getHeight()-1.f,1.f);else g.drawRoundedRectangle(r,3.f,1.f);}
    }
private:Desktop& owner;int kind;
};
}
bool Arrangement::laneVisible(int index)const{return index>=0&&index<5&&(owner.view.musicalLanes&(1<<index));}
int Arrangement::headerHeight()const{constexpr std::array<int,5> heights{24,33,24,32,32};int y=20;for(int i=0;i<5;++i)if(laneVisible(i))y+=heights[i];return y;}
void Arrangement::setLaneVisible(int index,bool visible){
    if(index<0||index>=5||visible==laneVisible(index))return;
    juce::PopupMenu::dismissAllActiveMenus();meters->sync(true);tempos->sync(true);sections->sync(true);chords->sync(true);markers->sync(true);
    drag.reset();dragGroup.clear();dragTrack.reset();copyDrag=dragMoved=selecting=rulerSeeking=false;selectionBox={};fileDragExit({});
    if(visible)owner.view.musicalLanes|=1<<index;else owner.view.musicalLanes&=~(1<<index);
    resized();repaint();owner.refresh();owner.saveSettings();
}
class TrackFolderHeader final:public juce::Component {
    Arrangement& arrangement;Desktop& owner;mrs::Folder folder;int depth{};std::unique_ptr<juce::Drawable> icon;
public:
    TrackFolderHeader(Arrangement& a,Desktop& d,const Arrangement::FolderRow& row):arrangement(a),owner(d),folder(row.folder),depth(row.depth),icon(juce::Drawable::createFromImageData(MRConceptAssets::folder_svg,MRConceptAssets::folder_svgSize)){
        setComponentID("track-folder-"+label(folder.id.value));setTitle(label(folder.name));setWantsKeyboardFocus(true);
    }
    void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(panel));g.setColour(juce::Colour(outline));g.drawHorizontalLine(getHeight()-1,0.f,static_cast<float>(getWidth()));
        const int x=8+depth*12;g.setColour(juce::Colour(primaryText));g.setFont(Theme::stripFont());g.drawText(folder.collapsed?">":"v",x,0,12,getHeight(),juce::Justification::centred);
        if(icon)icon->drawWithin(g,{static_cast<float>(x+16),7.f,22.f,22.f},juce::RectanglePlacement::centred,1.f);
        juce::String caption=label(folder.name);if(folder.bus)for(const auto& t:owner.project()->tracks)if(t.id==*folder.bus)caption+="  > "+label(t.name);
        g.drawText(caption,x+44,0,getWidth()-x-70,getHeight(),juce::Justification::centredLeft,true);g.drawText("...",getWidth()-26,0,22,getHeight(),juce::Justification::centred);
    }
    void toggle(){owner.run([&]{owner.app.update_track_folder(folder.id,folder.name,!folder.collapsed);});}
    void mouseDown(const juce::MouseEvent& e) override{if(e.mods.isPopupMenu()||e.x>getWidth()-30)owner.showFolderMenu(folder.id,e.getScreenPosition());else toggle();}
    bool keyPressed(const juce::KeyPress& k) override{if(k==juce::KeyPress::returnKey||k==juce::KeyPress::spaceKey){toggle();return true;}return false;}
};
void Arrangement::rebuildFolderLayout(){
    const auto p=owner.project();trackPositions.assign(p->tracks.size(),-1);folderRows.clear();folderHeaders.clear();contentHeight=0;layoutTrackHeight=trackHeight;
    struct Task{std::optional<mrs::Id> folder;int track{-1},depth{};};
    const auto children=[&](std::optional<mrs::Id> parent,int depth){std::vector<Task> result;std::set<std::string> emitted;
        for(std::size_t i=0;i<p->tracks.size();++i){const auto& track=p->tracks[i];if(track.folder==parent){result.push_back({{},static_cast<int>(i),depth});continue;}
            auto ancestor=track.folder;std::optional<mrs::Id> child;
            for(std::size_t n=0;ancestor&&n<p->folders.size();++n){const auto f=std::find_if(p->folders.begin(),p->folders.end(),[&](const auto& v){return v.id==*ancestor;});if(f==p->folders.end())break;if(f->parent==parent){child=f->id;break;}ancestor=f->parent;}
            if(child&&emitted.insert(child->value).second)result.push_back({child,-1,depth});}
        for(const auto& folder:p->folders)if(folder.parent==parent&&emitted.insert(folder.id.value).second)result.push_back({folder.id,-1,depth});return result;};
    auto roots=children({},0);std::vector<Task> tasks(roots.rbegin(),roots.rend());
    while(!tasks.empty()){const auto task=tasks.back();tasks.pop_back();if(task.track>=0){trackPositions[static_cast<std::size_t>(task.track)]=contentHeight;contentHeight+=trackHeight;continue;}
        const auto f=std::find_if(p->folders.begin(),p->folders.end(),[&](const auto& v){return task.folder==v.id;});if(f==p->folders.end())continue;
        folderRows.push_back({*f,contentHeight,task.depth});contentHeight+=36;if(!f->collapsed){const auto next=children(f->id,task.depth+1);tasks.insert(tasks.end(),next.rbegin(),next.rend());}}
    for(const auto& row:folderRows){auto component=std::make_unique<TrackFolderHeader>(*this,owner,row);rowsBody.addAndMakeVisible(*component);folderHeaders.push_back(std::move(component));}
}
void Desktop::rebuildMixerFolders(){
    const auto p=project();mixerColumns.assign(mixer.size(),-1);mixerFolderColumns.clear();mixerColumnCount=0;
    for(auto& channel:mixer)channel->folder.reset();
    struct Entry{int y;int track;const mrs::Folder* folder;};std::vector<Entry> entries;std::set<std::string> linked;
    for(const auto& folder:p->folders)if(folder.bus)linked.insert(folder.bus->value);
    for(std::size_t i=0;i<p->tracks.size();++i)if(i<arrangement->trackPositions.size()&&arrangement->trackPositions[i]>=0&&!linked.contains(p->tracks[i].id.value))entries.push_back({arrangement->trackPositions[i],static_cast<int>(i),nullptr});
    for(const auto& row:arrangement->folderRows)for(const auto& folder:p->folders)if(folder.id==row.folder.id){entries.push_back({row.y,-1,&folder});break;}
    std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.y<b.y;});
    std::vector<std::unique_ptr<Strip>> folderChannels;
    for(const auto& entry:entries){
        if(!entry.folder){mixerColumns[static_cast<std::size_t>(entry.track)]=mixerColumnCount++;continue;}
        const auto& folder=*entry.folder;
        if(folder.bus){for(std::size_t i=0;i<p->tracks.size();++i)if(p->tracks[i].id==*folder.bus){mixerColumns[i]=mixerColumnCount++;mixer[i]->folder=folder.id;break;}}
        else{auto at=std::find_if(mixerFolders.begin(),mixerFolders.end(),[&](const auto& item){return item&&item->folder==folder.id;});
            if(at!=mixerFolders.end())folderChannels.push_back(std::move(*at));else{auto channel=std::make_unique<Strip>(*this,std::nullopt,false,folder.id);mixerBody.addAndMakeVisible(*channel);folderChannels.push_back(std::move(channel));}
            mixerFolderColumns.push_back(mixerColumnCount++);}
    }
    mixerFolders=std::move(folderChannels);
    for(std::size_t i=0;i<mixer.size();++i)mixer[i]->setVisible(mixerColumns[i]>=0);
}
int Arrangement::trackY(int index) const{
    if(index<0)return -1000000;
    if(index>=static_cast<int>(trackPositions.size()))return contentHeight+(index-static_cast<int>(trackPositions.size()))*trackHeight;
    const int y=trackPositions[static_cast<std::size_t>(index)];return y<0?-1000000:y;
}
Arrangement::Arrangement(Desktop& d):owner(d),playhead([this]{const float x=static_cast<float>(left+owner.visualSample()/owner.project()->sample_rate*pixelsPerSecond-horizontal);return x>=left&&x<getWidth()?juce::Rectangle<float>{x,0.f,1.f,static_cast<float>(getHeight())}:juce::Rectangle<float>{};}){setWantsKeyboardFocus(true);addAndMakeVisible(rowsBody);rowsBody.setInterceptsMouseClicks(false,true);meters=std::make_unique<MeterTrack>(owner,*this);addAndMakeVisible(*meters);tempos=std::make_unique<TempoTrack>(owner,*this);addAndMakeVisible(*tempos);sections=std::make_unique<SectionTrack>(owner,*this);addAndMakeVisible(*sections);chords=std::make_unique<ChordTrack>(owner,*this);addAndMakeVisible(*chords);markers=std::make_unique<MarkerTrack>(owner,*this);addAndMakeVisible(*markers);addAndMakeVisible(playhead);
    const juce::Component::SafePointer<Arrangement> safe(this);
    dropWorker=std::jthread([this,safe](std::stop_token stop){
        for(;;){
            juce::StringArray files;std::uint64_t generation{};
            {std::unique_lock lock(dropMutex);dropWake.wait(lock,stop,[this]{return dropRequested;});
                if(stop.stop_requested())return;
                files=pendingDropFiles;generation=dropGeneration;dropRequested=false;}
            std::vector<DropWave> waves;
            try{for(const auto& file:files){
                {std::lock_guard lock(dropMutex);if(stop.stop_requested()||generation!=dropGeneration)break;}
                const auto wav=mrs::audio::inspect_wav(path(juce::File(file)));
                waves.push_back({static_cast<std::uint64_t>(wav.frame_count),wav.sample_rate});
            }}catch(...){waves.clear();}
            {std::lock_guard lock(dropMutex);if(stop.stop_requested())return;
                if(generation!=dropGeneration)continue;
                dropWaves=std::move(waves);}
            juce::MessageManager::callAsync([safe]{if(safe)safe->repaint();});
        }
    });
    const std::array<juce::String,5> names{"Markers","Sections","Chord Track","Tempo","Meter"};
    for(int i=0;i<5;++i){laneButtons[i]=std::make_unique<LaneButton>(owner,names[i],i);addAndMakeVisible(*laneButtons[i]);laneButtons[i]->onClick=[this,i]{setLaneVisible(i,!laneVisible(i));};}
}
Arrangement::~Arrangement(){dropWorker.request_stop();dropWake.notify_all();if(dropWorker.joinable())dropWorker.join();}
void Arrangement::rebuild(){fileDragExit({});std::vector<std::unique_ptr<Strip>> next;for(const auto& t:owner.project()->tracks){auto at=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row&&row->target==t.id;});if(at!=rows.end())next.push_back(std::move(*at));else{auto row=std::make_unique<Strip>(owner,t.id,true);rowsBody.addAndMakeVisible(*row);next.push_back(std::move(row));}}rows=std::move(next);rebuildFolderLayout();resized();}
void Arrangement::resized(){if(layoutTrackHeight!=trackHeight)rebuildFolderLayout();vertical=juce::jlimit(0.f,static_cast<float>(juce::jmax(0,contentHeight-(getHeight()-headerHeight()))),vertical);
    const std::array<juce::Component*,5> lanes{markers.get(),sections.get(),chords.get(),tempos.get(),meters.get()};
    constexpr std::array<int,5> heights{24,33,24,32,32};int laneY=20;
    for(int i=0;i<5;++i){const bool visible=laneVisible(i);lanes[i]->setVisible(visible);lanes[i]->setBounds(0,laneY,getWidth(),visible?heights[i]:0);if(visible)laneY+=heights[i];laneButtons[i]->setBounds(112+i*26,0,24,20);laneButtons[i]->setToggleState(visible,juce::dontSendNotification);}
    rowsBody.setBounds(0,headerHeight(),left-6,getHeight()-headerHeight());
    for(std::size_t i=0;i<rows.size();++i){const int y=trackY(static_cast<int>(i))-static_cast<int>(vertical);rows[i]->setBounds(0,y,left-6,trackHeight-2);rows[i]->setVisible(y+trackHeight>0 && y<rowsBody.getHeight());}
    for(std::size_t i=0;i<folderHeaders.size();++i){const int y=folderRows[i].y-static_cast<int>(vertical);folderHeaders[i]->setBounds(0,y,left-6,35);folderHeaders[i]->setVisible(y+36>0&&y<rowsBody.getHeight());}playhead.update();}
int Arrangement::trackAt(float y) const{const float local=y-headerHeight()+vertical;if(local<0)return -1;for(std::size_t i=0;i<trackPositions.size();++i)if(trackPositions[i]>=0&&local>=trackPositions[i]&&local<trackPositions[i]+trackHeight)return static_cast<int>(i);return local>=contentHeight?static_cast<int>(trackPositions.size()):-1;}
namespace {
juce::MouseCursor bracketCursor(bool right){
    static const auto create=[](bool reverse){juce::Image image(juce::Image::ARGB,24,24,true);juce::Graphics g(image);juce::Path p;
        const float stem=reverse?14.f:10.f,arm=reverse?8.f:16.f;p.startNewSubPath(arm,4);p.lineTo(stem,4);p.lineTo(stem,20);p.lineTo(arm,20);
        g.setColour(juce::Colours::black);g.strokePath(p,juce::PathStrokeType(4.f));g.setColour(juce::Colours::white);g.strokePath(p,juce::PathStrokeType(2.f));return juce::MouseCursor(image,static_cast<int>(stem),12);};
    static const auto left=create(false),rightCursor=create(true);return right?rightCursor:left;
}
}
juce::MouseCursor noteTrimCursor(bool right){return bracketCursor(right);}
int Arrangement::edgeAt(const mrs::Clip& c,juce::Point<float> point) const{
    const auto r=clipRect(c);if(!r.contains(point))return 0;
    const auto l=point.x-r.getX(),right=r.getRight()-point.x;
    return std::min(l,right)<7.f?(l<=right?-1:1):0;
}
std::optional<mrs::Id> Arrangement::clipAt(juce::Point<float> point) const{
    if(point.x<left||point.y<headerHeight())return {};
    const auto p=owner.project();
    // Match paint order: selected clips on top, then reverse insertion order.
    for(bool selected:{true,false})for(auto i=p->clips.rbegin();i!=p->clips.rend();++i){const bool active=i->id==owner.selectedClip||owner.selectedClips.contains(i->id.value);if(active==selected&&clipRect(*i).contains(point))return i->id;}
    return {};
}
void Arrangement::mouseMove(const juce::MouseEvent& e){
    if(owner.assetSkin()){juce::String text;if(const auto id=clipAt(e.position)){const auto project=owner.project();for(const auto& clip:project->clips)if(clip.id==*id){text=label(clip.name)+(clip.midi?" — MIDI":" — Audio");break;}}setTooltip(text);}
    if(owner.knifeMode){setMouseCursor(juce::MouseCursor::CrosshairCursor);return;}
    if(owner.app.recording()){setMouseCursor(juce::MouseCursor::NormalCursor);return;}
    if(const auto id=clipAt(e.position)){const auto p=owner.project();const auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip){return clip.id==*id;});const auto edge=edgeAt(*c,e.position);setMouseCursor(edge?bracketCursor(edge>0):juce::MouseCursor(juce::MouseCursor::NormalCursor));}
    else setMouseCursor(juce::MouseCursor::NormalCursor);
}
void Arrangement::mouseExit(const juce::MouseEvent&){setTooltip({});if(!drag)setMouseCursor(juce::MouseCursor::NormalCursor);}
void Arrangement::updateTrim(mrs::Sample delta,bool useSnap){
    const auto p=owner.project();const mrs::Timeline time(p->time,p->sample_rate);
    if(drag->midi){const auto& m=*drag->midi;auto tick=time.to_ticks(std::max(mrs::Sample{0},(trim<0?drag->start:drag->start+drag->length)+delta));
        if(useSnap)tick=snapTick(tick);
        if(trim<0){mrs::Tick prefix=mrs::max_tick;for(const auto& n:m.notes)prefix=std::min(prefix,mrs::max_tick-n.start-n.length);for(const auto& e:m.events)prefix=std::min(prefix,mrs::max_tick-e.start);tick=std::clamp(tick,std::max(mrs::Tick{0},m.start-m.source_offset-prefix),m.start+m.length-1);}
        else tick=std::clamp(tick,m.start+1,std::min(mrs::max_tick,m.start+mrs::max_tick-m.source_offset));
        trimStart=trim<0?time.to_samples(tick):drag->start;trimEnd=trim>0?time.to_samples(tick):drag->start+drag->length;
    }else{
        auto sample=std::max(mrs::Sample{0},(trim<0?drag->start:drag->start+drag->length)+delta);
        if(useSnap)sample=snapSample(sample);
        trimStart=trim<0?std::clamp(sample,std::max(mrs::Sample{0},drag->start-mrs::audio_output_offset(*drag)),drag->start+drag->length-1):drag->start;
        trimEnd=trim>0?std::clamp(sample,drag->start+1,std::min(mrs::max_sample,drag->start+(drag->stretch?mrs::audio_output_extent(*drag):dragSourceFrames)-mrs::audio_output_offset(*drag))):drag->start+drag->length;
    }
    auto sharedDelta=trim<0?trimStart-drag->start:trimEnd-drag->start-drag->length;
    mrs::Sample low=-mrs::max_sample,high=mrs::max_sample;
    for(const auto& c:dragGroup){const auto start=mrs::clip_start(c,time),end=mrs::clip_end(c,time);
        if(c.midi){const auto& m=*c.midi;if(trim<0){mrs::Tick prefix=mrs::max_tick;for(const auto& n:m.notes)prefix=std::min(prefix,mrs::max_tick-n.start-n.length);for(const auto& e:m.events)prefix=std::min(prefix,mrs::max_tick-e.start);low=std::max(low,time.to_samples(std::max(mrs::Tick{0},m.start-m.source_offset-prefix))-start);high=std::min(high,time.to_samples(m.start+m.length-1)-start);}else{low=std::max(low,time.to_samples(m.start+1)-end);high=std::min(high,time.to_samples(std::min(mrs::max_tick,m.start+mrs::max_tick-m.source_offset))-end);}}
        else if(trim<0){low=std::max(low,std::max(mrs::Sample{0},start-mrs::audio_output_offset(c))-start);high=std::min(high,end-start-1);}else{low=std::max(low,start+1-end);high=std::min(high,start+(c.stretch?mrs::audio_output_extent(c):trimFrames.at(c.id.value))-mrs::audio_output_offset(c)-end);}
    }
    sharedDelta=std::clamp(sharedDelta,low,high);trimStart=drag->start+(trim<0?sharedDelta:0);trimEnd=drag->start+drag->length+(trim>0?sharedDelta:0);trimPreviews=dragGroup;
    for(auto& c:trimPreviews){const auto start=mrs::clip_start(c,time),end=mrs::clip_end(c,time);if(c.midi)mrs::trim_midi_source(*c.midi,time.to_ticks(start+(trim<0?sharedDelta:0)),time.to_ticks(end+(trim>0?sharedDelta:0)));else mrs::trim_audio_source(c,start+(trim<0?sharedDelta:0),end+(trim>0?sharedDelta:0),trimFrames.at(c.id.value));}
}
mrs::Tick Arrangement::displaySpacing() const {
    double fastest=0;for(const auto& tempo:owner.project()->time.tempos)fastest=std::max(fastest,tempo.bpm);
    return std::max(mrs::Tick{1},static_cast<mrs::Tick>(std::ceil(fastest*mrs::ppq*10/(60*pixelsPerSecond))));
}
mrs::Tick Arrangement::snapTick(mrs::Tick tick) const {
    tick=std::clamp(tick,mrs::Tick{0},mrs::max_tick);
    const auto p=owner.project();const mrs::Timeline time(p->time,p->sample_rate);const auto [a,b]=displayGridNeighbours(p->time,tick,displaySpacing());
    const auto position=time.to_samples(tick),leftSample=time.to_samples(a),rightSample=time.to_samples(b);return position-leftSample<=rightSample-position?a:b;
}
mrs::Sample Arrangement::snapSample(mrs::Sample sample) const {
    const auto p=owner.project();const mrs::Timeline time(p->time,p->sample_rate);const auto [a,b]=displayGridNeighbours(p->time,time.to_ticks(sample),displaySpacing());
    const auto leftSample=time.to_samples(a),rightSample=time.to_samples(b);return sample-leftSample<=rightSample-sample?leftSample:rightSample;
}
mrs::Sample Arrangement::sampleAt(float x) const{auto sample=static_cast<mrs::Sample>(juce::jmax(0.,(x-left+horizontal)/pixelsPerSecond)*owner.project()->sample_rate);
    if(owner.snap)sample=snapSample(sample);return sample;}
mrs::Sample Arrangement::seekSampleAt(float x) const{
    const auto p=owner.project();const mrs::Timeline time(p->time,p->sample_rate);
    const auto raw=static_cast<mrs::Sample>(std::llround(juce::jmax(0.,(x-left+horizontal)/pixelsPerSecond)*p->sample_rate));
    if(owner.snap)return snapSample(raw);
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
    return raw;
}
juce::Rectangle<float> Arrangement::clipRect(const mrs::Clip& c) const{int index=0;for(const auto& t:owner.project()->tracks){if(t.id==c.track)break;++index;}
    const auto p=owner.project();const mrs::Timeline t(p->time,p->sample_rate);const double rate=p->sample_rate;const auto start=mrs::clip_start(c,t),end=mrs::clip_end(c,t);return {static_cast<float>(left+start/rate*pixelsPerSecond-horizontal),static_cast<float>(headerHeight()+trackY(index)-vertical+1),juce::jmax(2.f,static_cast<float>((end-start)/rate*pixelsPerSecond)),static_cast<float>(trackHeight-2)};}
void Arrangement::paintGrid(juce::Graphics& g,float top,float bottom,bool labels) const {
    const auto p=owner.project();const mrs::Timeline time(p->time,p->sample_rate);
    const auto rawTick=[&](double x){return time.to_ticks(static_cast<mrs::Sample>(std::clamp((x-left+horizontal)/pixelsPerSecond*p->sample_rate,0.,static_cast<double>(mrs::max_sample))));};
    const auto first=rawTick(left),last=rawTick(getWidth());int lastLabel=left-70;
    const auto spacing=std::max(mrs::Tick{1},static_cast<mrs::Tick>(std::ceil(displaySpacing()*2.4)));
    visitDisplayGrid(p->time,first,last,spacing,[&](mrs::Tick tick,mrs::MusicalPosition pos){const int x=left+juce::roundToInt(static_cast<double>(time.to_samples(tick))/p->sample_rate*pixelsPerSecond-horizontal);if(x<left)return;g.setColour(juce::Colour(pos.tick==0&&pos.beat==1?outline:separatorColour).withAlpha(pos.tick==0?1.f:.35f));g.drawVerticalLine(x,top,bottom);if(labels&&pos.tick==0&&x-lastLabel>=60){g.setColour(juce::Colours::lightgrey);g.drawText(juce::String(pos.bar)+":"+juce::String(pos.beat),x+2,0,58,20,juce::Justification::left);lastLabel=x;}});
}
void Arrangement::paint(juce::Graphics& g){g.fillAll(juce::Colour(arrangementCanvas));g.setColour(juce::Colour(surface));g.fillRect(0,0,left,getHeight());g.setFont(Theme::detailFont());g.setColour(juce::Colours::lightgrey);
    const auto p=owner.project();mrs::Timeline time(p->time,p->sample_rate);
    g.drawText("Tracks",8,0,90,20,juce::Justification::left);
    g.setFont(ui::Theme::channelFont());paintGrid(g,20,static_cast<float>(getHeight()),true);
    g.setFont(Theme::detailFont());

    g.saveState();g.reduceClipRegion(left,headerHeight(),getWidth()-left,getHeight()-headerHeight());
    g.setFont(Theme::stripFont());
    for(std::size_t i=0;i<p->tracks.size();++i){int y=headerHeight()+trackY(static_cast<int>(i))-static_cast<int>(vertical);g.setColour(juce::Colour(separatorColour));g.drawHorizontalLine(y,static_cast<float>(left),static_cast<float>(getWidth()));}
    for(const auto& folder:folderRows){const int y=headerHeight()+folder.y-static_cast<int>(vertical);g.setColour(juce::Colour(panel));g.fillRect(left,y,getWidth()-left,35);g.setColour(juce::Colour(outline));g.drawHorizontalLine(y+35,static_cast<float>(left),static_cast<float>(getWidth()));}
    g.setFont(Theme::detailFont());
    // Paint selected clips last; both audio and MIDI reveal overlapping clips.
    auto paintedClips=p->clips;std::stable_partition(paintedClips.begin(),paintedClips.end(),[&](const auto& c){return c.id!=owner.selectedClip&&!owner.selectedClips.contains(c.id.value);});
    const auto originalCount=paintedClips.size();if(copyDrag&&dragMoved)paintedClips.insert(paintedClips.end(),dragGroup.begin(),dragGroup.end());
    for(std::size_t painted=0;painted<paintedClips.size();++painted){const auto& stored=paintedClips[painted];auto c=stored;if(fadeOriginal&&fadeOriginal->id==c.id){c.fade_in=fadeInPreview;c.fade_out=fadeOutPreview;}const bool ghost=painted>=originalCount;
        if(drag&&(!copyDrag||ghost)&&(drag->id==c.id||std::any_of(dragGroup.begin(),dragGroup.end(),[&](const auto& item){return item.id==c.id;}))){
            if(trim){
                const auto at=std::find_if(trimPreviews.begin(),trimPreviews.end(),[&](const auto& item){return item.id==c.id;});if(at!=trimPreviews.end())c=*at;
            }else{const auto start=mrs::clip_start(stored,time)+dragStart-drag->start;if(c.midi)c.midi->start=time.to_ticks(start);else c.start=start;const auto at=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==c.track;});c.track=p->tracks[static_cast<std::size_t>(static_cast<int>(at-p->tracks.begin())+dragTrackDelta)].id;}
        }
        const auto midi=c.midi?c.midi:stored.midi;
        const auto defaultColor=owner.assetSkin()?ObjectPalette::defaultClip:((c.id==owner.selectedClip||owner.selectedClips.contains(c.id.value))?0x3266ac:0x294d7e);
        auto r=clipRect(c);if(!r.intersects(getLocalBounds().toFloat()))continue;g.setColour(juce::Colour(0xff000000|c.color.value_or(defaultColor)).withAlpha(clipFillAlpha));g.fillRoundedRectangle(r,cornerRadius);if(c.id==owner.selectedClip||owner.selectedClips.contains(c.id.value)){g.setColour(owner.selectionColour().withAlpha(.10f));g.fillRoundedRectangle(r,cornerRadius);}g.setColour((c.id==owner.selectedClip||owner.selectedClips.contains(c.id.value))?owner.selectionColour():juce::Colour(outline));g.drawRoundedRectangle(r.reduced(.5f),cornerRadius,borderWidth);g.setColour(juce::Colour(primaryText));
        const auto clipInk=owner.assetSkin()?ObjectPalette::ink(c.color.value_or(defaultColor),clipFillAlpha,arrangementCanvas):c.color&&juce::Colour(0xff000000|*c.color).getPerceivedBrightness()>.65f?juce::Colour(0xff16212a):juce::Colour(primaryText);g.setColour(clipInk);
        if(owner.assetSkin()){
            juce::Graphics::ScopedSaveState clipScope(g);g.reduceClipRegion(r.toNearestInt().reduced(1));
            const auto clipHeader=r.withHeight(20);g.setColour(clipInk.withAlpha(.08f));g.fillRect(clipHeader);
            g.setColour(clipInk.withAlpha(.25f));g.drawHorizontalLine(juce::roundToInt(clipHeader.getBottom()),clipHeader.getX(),clipHeader.getRight());
            auto title=clipHeader.toNearestInt().reduced(5,0);
            // Type comes from actual clip data; waveform and note previews keep
            // their existing sample/tick mapping and source offsets.
            if(r.getWidth()>=48){owner.assetSkin()->draw(g,midi?"track_synth":"wave",{r.getX()+4,r.getY()+3,14,14},1.f,false,clipInk.getBrightness()<.5f);title.removeFromLeft(18);}
            if(r.getWidth()>=160){auto badge=title.removeFromRight(42);g.setColour(clipInk);g.drawText(midi?"MIDI":"AUDIO",badge,juce::Justification::centredRight,true);title.removeFromRight(6);}
            if(title.getWidth()>12){g.setColour(clipInk);g.drawText(label(c.name)+(ghost?" [Copy]":""),title,juce::Justification::centredLeft,true);}
        }else g.drawText(label(c.name)+(ghost?" [Copy]":""),r.toNearestInt().withHeight(20).reduced(5,0),juce::Justification::left);
        if(ghost){g.setColour(juce::Colours::white);g.drawRoundedRectangle(r.reduced(1.f),cornerRadius,1.5f);g.setColour(juce::Colour(0xff8ac3ff));}
        if(midi){g.setColour(c.color||owner.assetSkin()?clipInk:juce::Colour(0xff8ac3ff));g.saveState();g.reduceClipRegion(r.toNearestInt().reduced(1));for(const auto& n:midi->notes){const auto begin=std::max(n.start,midi->source_offset),end=std::min(n.start+n.length,midi->source_offset+midi->length);if(begin>=end)continue;const double total=static_cast<double>(time.to_samples(midi->start+midi->length)-time.to_samples(midi->start));const float x=r.getX()+static_cast<float>((time.to_samples(midi->start+begin-midi->source_offset)-time.to_samples(midi->start))/juce::jmax(1.,total)*r.getWidth());const float w=static_cast<float>((time.to_samples(midi->start+end-midi->source_offset)-time.to_samples(midi->start+begin-midi->source_offset))/juce::jmax(1.,total)*r.getWidth());const float y=r.getY()+25+(127-n.pitch)/127.f*juce::jmax(1.f,r.getHeight()-32);g.fillRect(x,y,juce::jmax(2.f,w),3.f);}g.restoreState();continue;}
        if(const auto* wave=owner.app.clip_waveform(c)){g.setColour(c.color||owner.assetSkin()?clipInk:juce::Colour(0xff8ac3ff));drawWaveform(g,*wave,r.withTrimmedTop(22),static_cast<double>(owner.app.clip_playback_offset(c)),p->sample_rate/pixelsPerSecond);}
        if(araDropClip&&c.id==*araDropClip){g.setColour(owner.selectionColour());g.drawRoundedRectangle(r.reduced(2.f),cornerRadius,2.f);g.drawText("ARA 2",r.toNearestInt().reduced(6).removeFromBottom(20),juce::Justification::centredRight);}
        if(r.getWidth()>=24){const auto width=r.getWidth();const auto inWidth=static_cast<float>(static_cast<double>(c.fade_in)/c.length*width),outWidth=static_cast<float>(static_cast<double>(c.fade_out)/c.length*width);g.setColour(clipInk.withAlpha(.8f));
            if(c.fade_in)g.drawLine(r.getX(),r.getBottom()-2,r.getX()+inWidth,r.getY()+26,1.f);if(c.fade_out)g.drawLine(r.getRight()-outWidth,r.getY()+26,r.getRight(),r.getBottom()-2,1.f);
            if(c.id==owner.selectedClip||owner.selectedClips.contains(c.id.value)){const auto maxWidth=juce::jmax(6.f,width-6);g.fillRect(r.getX()+juce::jlimit(6.f,maxWidth,inWidth)-3,r.getY()+23,6.f,6.f);g.fillRect(r.getRight()-juce::jlimit(6.f,maxWidth,outWidth)-3,r.getY()+23,6.f,6.f);}}

    }
    {juce::Graphics::ScopedSaveState save(g);g.reduceClipRegion(juce::Rectangle<int>(left,headerHeight(),getWidth()-left,getHeight()-headerHeight()));
        const auto ghosts=wavDropRects();
        for(std::size_t i=0;i<ghosts.size();++i){const auto& rect=ghosts[i];
            g.setColour(owner.selectionColour().withAlpha(.25f));g.fillRoundedRectangle(rect,cornerRadius);
            g.setColour(owner.selectionColour().withAlpha(.8f));g.drawRoundedRectangle(rect.reduced(.5f),cornerRadius,1.f);
            g.drawText(juce::File(dropFiles[static_cast<int>(i)]).getFileName(),rect.toNearestInt().withHeight(20).reduced(5,0),juce::Justification::left);
        }
    }
    {juce::Graphics::ScopedSaveState save(g);g.reduceClipRegion(juce::Rectangle<int>(left,headerHeight(),getWidth()-left,getHeight()-headerHeight()));for(const auto& rect:midiDropRects()){g.setColour(owner.selectionColour().withAlpha(.25f));g.fillRoundedRectangle(rect,cornerRadius);g.setColour(owner.selectionColour().withAlpha(.8f));g.drawRoundedRectangle(rect.reduced(.5f),cornerRadius,1.f);g.drawText("MIDI",rect.toNearestInt().withHeight(20).reduced(5,0),juce::Justification::left);}}
    for(const auto& c:owner.recordingPreview){auto r=clipRect(c);g.setColour(juce::Colour(0xff683741).withAlpha(.72f));g.fillRoundedRectangle(r,cornerRadius);g.setColour(juce::Colours::salmon);g.drawRoundedRectangle(r.reduced(.5f),cornerRadius,borderWidth);g.drawText("Recording MIDI",r.toNearestInt().withHeight(20).reduced(5,0),juce::Justification::left);g.saveState();g.reduceClipRegion(r.toNearestInt().reduced(1));for(const auto& n:c.midi->notes){const auto begin=std::max(n.start,c.midi->source_offset),end=std::min(n.start+n.length,c.midi->source_offset+c.midi->length);if(begin>=end)continue;const float x=static_cast<float>(left+static_cast<double>(time.to_samples(c.midi->start+begin-c.midi->source_offset))/p->sample_rate*pixelsPerSecond-horizontal);const float width=static_cast<float>(static_cast<double>(time.to_samples(c.midi->start+end-c.midi->source_offset)-time.to_samples(c.midi->start+begin-c.midi->source_offset))/p->sample_rate*pixelsPerSecond);g.fillRect(x,r.getY()+25+(127-n.pitch)/127.f*juce::jmax(1.f,r.getHeight()-32),juce::jmax(2.f,width),3.f);}g.restoreState();}
    for(const auto& preview:owner.audioRecordingPreview){auto r=clipRect(preview.clip);g.setColour(juce::Colour(0xff683741).withAlpha(.72f));g.fillRoundedRectangle(r,cornerRadius);g.setColour(juce::Colours::salmon);g.drawRoundedRectangle(r.reduced(.5f),cornerRadius,borderWidth);g.drawText("Recording audio",r.toNearestInt().withHeight(20).reduced(5,0),juce::Justification::left);if(preview.waveform)drawRecordingWaveform(g,*preview.waveform,r.withTrimmedTop(22),p->sample_rate/pixelsPerSecond,static_cast<double>(preview.clip.source_offset));}
    if(selecting){g.setColour((owner.assetSkin()?owner.selectionColour():juce::Colours::skyblue).withAlpha(.12f));g.fillRect(selectionBox);g.setColour(owner.assetSkin()?owner.selectionColour():juce::Colours::skyblue);g.drawRect(selectionBox,1.f);}
    g.restoreState();
}
void Desktop::editClipStretch(mrs::Id id){
    const auto store=app.services().projects;const auto projectRevision=store->state().revision;
    const auto p=project();const auto it=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});
    if(it==p->clips.end()||it->midi)return;
    if(app.recording()||app.engine()->state().playback==mrs::PlaybackState::playing)throw std::runtime_error("Stop playback before editing time / pitch");
    const auto parameters=it->stretch?it->stretch->parameters:mrs::stretch::Parameters{};
    const auto sourceFrames=it->stretch?it->stretch->window_end-it->stretch->window_begin:static_cast<double>(it->length);
    auto* alert=new StudioAlertWindow(*this,"Clip Time / Pitch",
        "Source: "+juce::String(sourceFrames/p->sample_rate,3)+" s  |  Clip: "+juce::String(static_cast<double>(it->length)/p->sample_rate,3)+" s\n"
        "Experimental. Sharp transients may move slightly; audition critical material.",juce::MessageBoxIconType::NoIcon);
    alert->addComboBox("enabled",{"Enabled","Bypassed"},"Processing");alert->getComboBoxComponent("enabled")->setSelectedId(parameters.enabled?1:2);
    const auto number=[&](const char* key,double value,const char* caption){alert->addTextEditor(key,juce::String(value,17),caption);};
    number("ratio",parameters.duration_ratio,"Duration ratio (0.125 .. 8; 1 = original)");
    number("pitch",parameters.semitones,"Pitch (-24 .. +24 semitones, including cents)");
    number("cents",parameters.cents,"Fine pitch (-100 .. +100 cents)");
    alert->addComboBox("formants",{"Follow pitch","Preserve","Shift"},"Formants");alert->getComboBoxComponent("formants")->setSelectedId(static_cast<int>(parameters.formants)+1);
    number("shift",parameters.formant_semitones,"Formant shift (-12 .. +12 semitones; Shift mode)");
    number("base",parameters.formant_base_hz,"Formant base (20 .. 2000 Hz; 0 = default)");
    alert->addComboBox("quality",{"Balanced","Economical"},"Quality");alert->getComboBoxComponent("quality")->setSelectedId(static_cast<int>(parameters.quality)+1);
    alert->addButton("Apply",1,juce::KeyPress(juce::KeyPress::returnKey));alert->addButton("Reset",2);alert->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<Desktop> safe(this);
    alert->enterModalState(true,juce::ModalCallbackFunction::create([safe,alert,store,projectRevision,id,parameters](int result){
        if(!safe||!result)return;
        safe->run([&]{
            if(safe->app.services().projects!=store||store->state().revision!=projectRevision)throw std::runtime_error("Project changed; reopen Time / Pitch");
            if(result==2){safe->app.set_clip_stretch(id,parameters,true);return;}
            auto value=parameters;
            const auto read=[&](const char* key){auto text=alert->getTextEditorContents(key).trim().toStdString();if(text.starts_with('+'))text.erase(0,1);double n{};
                const auto parsed=std::from_chars(text.data(),text.data()+text.size(),n);
                if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||!std::isfinite(n))throw std::runtime_error("Enter a finite number using a decimal point");return n;};
            value.enabled=alert->getComboBoxComponent("enabled")->getSelectedId()==1;
            value.duration_ratio=read("ratio");value.semitones=read("pitch");value.cents=read("cents");
            value.formants=static_cast<mrs::stretch::Formants>(alert->getComboBoxComponent("formants")->getSelectedId()-1);
            value.formant_semitones=read("shift");value.formant_base_hz=read("base");
            value.quality=static_cast<mrs::stretch::Quality>(alert->getComboBoxComponent("quality")->getSelectedId()-1);
            safe->app.set_clip_stretch(id,value);
        });
    }),true);
}
void Arrangement::clipMenu(mrs::Id id,juce::Point<int> point){
    const auto store=owner.app.services().projects;const auto p=owner.project();const auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});if(c==p->clips.end())return;
    auto ids=std::vector<mrs::Id>{};if(owner.selectedClips.contains(id.value)){for(const auto& item:p->clips)if(owner.selectedClips.contains(item.id.value))ids.push_back(item.id);}else{owner.activateClip(id);ids.push_back(id);}
    const auto color=std::make_shared<std::uint32_t>(c->color.value_or(owner.assetSkin()?ObjectPalette::defaultClip:0x294d7e));juce::PopupMenu menu;menu.setLookAndFeel(&owner.theme);const bool enabled=!owner.app.recording();
#ifdef MRS_HAS_ARA
    if(!c->midi)owner.addAraMenus(menu,id);
#endif
    if(!c->midi)menu.addItem(20,"Time / Pitch...",enabled&&owner.displayState().playback!=mrs::PlaybackState::playing);
    menu.addItem(1,"Split selected clips (S)",enabled);menu.addItem(2,"Duplicate (D)",enabled);menu.addItem(3,"Merge MIDI clips (G)",enabled&&ids.size()>1);menu.addItem(4,"Delete selected clips",enabled);menu.addItem(5,"Default color",enabled);menu.addItem(6,"Rename selected clips...",enabled);menu.addSeparator();auto palette=std::make_unique<MenuColourPalette>(owner.theme,*color,color,true,owner.assetSkin()!=nullptr);palette->setEnabled(enabled);menu.addCustomItem(10,std::move(palette),nullptr,"Clip colors");
    const auto cut=sampleAt(static_cast<float>(getLocalPoint(nullptr,point).x));juce::Component::SafePointer<Arrangement> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({point.x,point.y,1,1}),[safe,store,ids,color,cut,id](int result){if(!safe||!result)return;safe->owner.run([&]{if(store!=safe->owner.app.services().projects)throw std::runtime_error("Project changed; reopen clip menu");
#ifdef MRS_HAS_ARA
        if(result>=300){safe->owner.araChoice(id,result);return;}
#endif
        if(result==20){safe->owner.editClipStretch(id);return;}
        if(result==6){safe->owner.renameClips(ids);return;}if(result==10||result==5)safe->owner.app.set_clip_colors(ids,result==5?std::nullopt:std::optional<std::uint32_t>{*color});else if(result==1)safe->owner.app.split_clips(ids,cut);else if(result==2)safe->owner.app.duplicate_clip(ids.front());else if(result==3)safe->owner.activateClip(safe->owner.app.merge_midi_clips(ids));else if(result==4)safe->owner.app.remove_clips(ids);});});
}
void Arrangement::mouseDown(const juce::MouseEvent& e){
    grabKeyboardFocus();rulerSeeking=false;fadeOriginal.reset();fadeStore.reset();shiftRemove=false;if(e.mods.isPopupMenu()){if(const auto id=clipAt(e.position))clipMenu(*id,e.getScreenPosition());return;}if(e.x<left||!e.mods.isLeftButtonDown())return;
    if(e.y<headerHeight()){if(sections->isVisible()&&sections->getBounds().contains(e.getPosition())){const auto p=owner.project();const mrs::Timeline time(p->time,p->sample_rate);const auto sample=std::clamp<mrs::Sample>(static_cast<mrs::Sample>(std::llround((e.position.x-left+horizontal)*p->sample_rate/pixelsPerSecond)),0,mrs::max_sample);for(const auto& section:p->sections)if(time.to_samples(section.start)<=sample&&sample<time.to_samples(section.end)){owner.run([&]{owner.app.musical().seek_section(section.id);});break;}return;}if(e.y<20){rulerSeeking=!owner.app.recording();owner.run([&]{owner.app.seek(seekSampleAt(e.position.x));});}return;}
    chords->clearSelection();
    markers->clearSelection();
    sections->clearSelection();
    meters->clearSelection();tempos->clearSelection();
    const auto p=owner.project();const int index=trackAt(e.position.y);if(index>=0&&index<static_cast<int>(p->tracks.size()))owner.selectTrack(p->tracks[static_cast<std::size_t>(index)].id,e.mods);
    dragGroup.clear();dragTrackDelta=0;copyDrag=false;dragMoved=false;
    if(const auto id=clipAt(e.position)){
        const auto c=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& clip){return clip.id==*id;});
        if(owner.knifeMode){std::vector<mrs::Id> ids;if(owner.selectedClips.contains(c->id.value)){for(const auto& item:p->clips)if(owner.selectedClips.contains(item.id.value))ids.push_back(item.id);}else ids.push_back(c->id);owner.run([&]{const auto right=owner.app.split_clips(ids,sampleAt(e.position.x));owner.activateClip(right.front());owner.selectedClips.clear();for(const auto& id:right)owner.selectedClips.insert(id.value);});return;}
        copyDrag=e.mods.isAltDown();trim=copyDrag?0:edgeAt(*c,e.position);auto selection=owner.selectedClips;if(owner.selectedClip)selection.insert(owner.selectedClip->value);
        shiftRemove=e.mods.isShiftDown()&&!copyDrag&&selection.contains(c->id.value);
        if(e.mods.isShiftDown())selection.insert(c->id.value);else if(!selection.contains(c->id.value))selection={c->id.value};
        owner.activateClip(c->id);owner.selectedClips=std::move(selection);if(owner.app.recording())return;
        if(!c->midi&&!copyDrag&&!e.mods.isShiftDown()&&owner.app.engine()->state().playback!=mrs::PlaybackState::playing){const auto r=clipRect(*c);const auto width=r.getWidth();const auto xIn=r.getX()+juce::jlimit(6.f,juce::jmax(6.f,width-6),static_cast<float>(static_cast<double>(c->fade_in)/c->length*width));const auto xOut=r.getRight()-juce::jlimit(6.f,juce::jmax(6.f,width-6),static_cast<float>(static_cast<double>(c->fade_out)/c->length*width));
            if(width>=24&&std::abs(e.position.y-(r.getY()+26))<=5&&(std::abs(e.position.x-xIn)<=5||std::abs(e.position.x-xOut)<=5)){
                fadeOriginal=*c;fadeStore=owner.app.services().projects;fadeRight=std::abs(e.position.x-xOut)<std::abs(e.position.x-xIn);fadeInPreview=c->fade_in;fadeOutPreview=c->fade_out;drag.reset();dragGroup.clear();repaint();return;}}

        for(const auto& item:p->clips)if(owner.selectedClips.contains(item.id.value))dragGroup.push_back(item);
        drag=*c;const mrs::Timeline time(p->time,p->sample_rate);drag->start=mrs::clip_start(*c,time);drag->length=mrs::clip_end(*c,time)-drag->start;
        dragX=e.position.x;dragStart=trimStart=drag->start;trimEnd=drag->start+drag->length;
        trimPreviews.clear();trimFrames.clear();if(trim){bool ready=false;owner.run([&]{for(const auto& item:dragGroup)if(!item.midi)trimFrames[item.id.value]=owner.app.source_frames(item.id);dragSourceFrames=c->midi?0:trimFrames.at(c->id.value);ready=true;});if(!ready){drag.reset();dragGroup.clear();return;}trimPreviews=dragGroup;}
        setMouseCursor(trim?bracketCursor(trim>0):juce::MouseCursor(juce::MouseCursor::NormalCursor));repaint();return;
    }
    drag.reset();dragTrack.reset();selecting=true;selectionOrigin=e.position;selectionBox={e.position,e.position};selectionBase=e.mods.isShiftDown()?owner.selectedClips:std::set<std::string>{};if(e.mods.isShiftDown()&&owner.selectedClip)selectionBase.insert(owner.selectedClip->value);owner.selectedClips=selectionBase;if(selectionBase.empty())owner.selectedClip.reset();repaint();
}
void Arrangement::mouseDrag(const juce::MouseEvent& e){if(fadeOriginal){const auto r=clipRect(*fadeOriginal);const auto frames=static_cast<mrs::Sample>(std::llround(static_cast<double>(fadeRight?r.getRight()-e.position.x:e.position.x-r.getX())/static_cast<double>(juce::jmax(1.f,r.getWidth()))*static_cast<double>(fadeOriginal->length)));if(fadeRight)fadeOutPreview=std::clamp(frames,mrs::Sample{0},fadeOriginal->length);else fadeInPreview=std::clamp(frames,mrs::Sample{0},fadeOriginal->length);repaint();return;}if(rulerSeeking){if(e.x>=left&&e.y>=0&&e.y<20)owner.run([&]{owner.app.seek(seekSampleAt(e.position.x));});return;}if(selecting){selectionBox=juce::Rectangle<float>(selectionOrigin,e.position).getIntersection(juce::Rectangle<float>(static_cast<float>(left),static_cast<float>(headerHeight()),static_cast<float>(getWidth()-left),static_cast<float>(getHeight()-headerHeight())));owner.selectedClips=selectionBase;owner.selectedClip=selectionBase.empty()?std::nullopt:std::optional<mrs::Id>{mrs::Id{*selectionBase.begin()}};for(const auto& c:owner.project()->clips)if(clipRect(c).intersects(selectionBox)){owner.selectedClips.insert(c.id.value);if(!owner.selectedClip)owner.selectedClip=c.id;}repaint();return;}if(!drag)return;
    const auto p=owner.project();const mrs::Timeline time(p->time,p->sample_rate);auto delta=static_cast<mrs::Sample>((e.x-dragX)/pixelsPerSecond*p->sample_rate);if(trim){updateTrim(delta,owner.snap&&!e.mods.isShiftDown());repaint();return;}if(!trim){mrs::Sample low=-mrs::max_sample,high=mrs::max_sample;for(const auto& c:dragGroup){const auto start=mrs::clip_start(c,time);low=std::max(low,-start);high=std::min(high,mrs::max_sample-mrs::clip_end(c,time));}delta=std::clamp(delta,low,high);if(owner.snap&&!e.mods.isShiftDown()){const auto snapped=sampleAt(static_cast<float>(left+static_cast<double>(drag->start+delta)/p->sample_rate*pixelsPerSecond-horizontal));delta=std::clamp(snapped-drag->start,low,high);}}dragStart=juce::jmax<mrs::Sample>(0,drag->start+delta);
    const auto anchor=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==drag->track;});if(anchor==p->tracks.end()){keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));return;}const int targetIndex=trackAt(e.position.y);if(targetIndex<0)return;int wanted=targetIndex-static_cast<int>(anchor-p->tracks.begin()),low=0,high=static_cast<int>(p->tracks.size())-1;if(!dragGroup.empty()){low=-static_cast<int>(p->tracks.size());high=static_cast<int>(p->tracks.size());for(const auto& c:dragGroup){const auto t=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& item){return item.id==c.track;});const int at=static_cast<int>(t-p->tracks.begin());low=std::max(low,-at);high=std::min(high,static_cast<int>(p->tracks.size())-1-at);}}dragTrackDelta=std::clamp(wanted,low,high);dragTrack=p->tracks[static_cast<std::size_t>(static_cast<int>(anchor-p->tracks.begin())+dragTrackDelta)].id;dragMoved=dragMoved||std::abs(e.x-dragX)>=2||dragTrackDelta!=0;repaint();
}
void Arrangement::mouseUp(const juce::MouseEvent& e){
    if(fadeOriginal){const auto original=*fadeOriginal;const auto store=fadeStore;const auto in=fadeInPreview,out=fadeOutPreview;fadeOriginal.reset();fadeStore.reset();owner.run([&]{if(owner.app.services().projects!=store)throw std::runtime_error("Project changed; retry fade");owner.app.set_clip_fades(original,in,out);});repaint();return;}
    if(drag&&shiftRemove&&!dragMoved&&std::abs(e.x-dragX)<2){owner.selectedClips.erase(drag->id.value);owner.selectedClip=owner.selectedClips.empty()?std::nullopt:std::optional<mrs::Id>{mrs::Id{*owner.selectedClips.begin()}};drag.reset();dragGroup.clear();shiftRemove=false;repaint();return;}
    shiftRemove=false;
    if(rulerSeeking){rulerSeeking=false;return;}
    if(selecting){selecting=false;selectionBox={};owner.refresh();return;}
    if(!drag)return;const auto c=*drag;const auto delta=dragStart-c.start,begin=trimStart,end=trimEnd;const auto trackDelta=dragTrackDelta,edge=trim;const bool copy=copyDrag;
#ifdef MRS_HAS_ARA
    if(!copy&&!dragMoved&&std::abs(e.x-dragX)<2)owner.followAra(c.id);
#endif
    const auto baseline=dragGroup;std::vector<mrs::Id> group;for(const auto& clip:baseline)group.push_back(clip.id);
    drag.reset();dragTrack.reset();dragGroup.clear();dragTrackDelta=0;trim=0;copyDrag=false;dragMoved=false;setMouseCursor(juce::MouseCursor::NormalCursor);
    if(edge){if(begin!=c.start||end!=c.start+c.length)owner.run([&]{owner.app.trim_clips(baseline,edge<0?begin-c.start:end-c.start-c.length,edge<0);});}
    else if(std::abs(e.x-dragX)>=2||trackDelta!=0)owner.run([&]{if(copy){const auto p=owner.project();for(const auto& original:baseline){const auto current=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& item){return item.id==original.id;});if(current==p->clips.end()||*current!=original)throw std::runtime_error("Source clip changed; retry copy drag");}const auto copied=owner.app.copy_clips(std::move(group),delta,trackDelta);owner.activateClip(copied.front());owner.selectedClips.clear();for(const auto& id:copied)owner.selectedClips.insert(id.value);}else owner.app.move_clips(std::move(group),delta,trackDelta);});
    repaint();
}
bool Arrangement::keyPressed(const juce::KeyPress& key){if(key.getKeyCode()!=juce::KeyPress::escapeKey)return false;if(owner.app.wav_import_status().busy)owner.app.cancel_wav_import();if(owner.app.stretch_status().busy)owner.app.cancel_stretch();cancelGesture();return true;}
void Arrangement::cancelGesture(){rulerSeeking=false;fadeOriginal.reset();fadeStore.reset();shiftRemove=false;setMouseCursor(juce::MouseCursor::NormalCursor);drag.reset();dragTrack.reset();dragGroup.clear();copyDrag=false;dragMoved=false;selecting=false;selectionBox={};repaint();}
void Arrangement::focusLost(FocusChangeType){cancelGesture();}
void Arrangement::wheel(float delta,juce::ModifierKeys mods,float x){if(mods.isCtrlDown()){if(mods.isShiftDown()){const auto before=static_cast<mrs::Sample>(std::clamp((x-left+horizontal)/pixelsPerSecond*owner.project()->sample_rate,0.,static_cast<double>(mrs::max_sample)));pixelsPerSecond=juce::jlimit(.1,2400.,pixelsPerSecond*std::pow(1.25,delta*4));horizontal=juce::jmax(0.,static_cast<double>(before)/owner.project()->sample_rate*pixelsPerSecond-(x-left));}
    else {const float before=(vertical+getHeight()/2.f-headerHeight())/trackHeight;trackHeight=juce::jlimit(40,360,trackHeight+static_cast<int>(delta*96));vertical=juce::jmax(0.f,before*trackHeight-(getHeight()/2.f-headerHeight()));rebuildFolderLayout();}}
    else if(mods.isShiftDown())horizontal=juce::jmax(0.,horizontal-delta*160);else vertical=juce::jmax(0.f,vertical-delta*128);
    resized();repaint();}
void Arrangement::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w){wheel(w.deltaY,e.mods,e.position.x);}
void Arrangement::mouseDoubleClick(const juce::MouseEvent& e){
    if(owner.knifeMode||!e.mods.isLeftButtonDown())return;
    drag.reset();dragTrack.reset();dragGroup.clear();
    owner.run([&]{const auto p=owner.project();for(const auto& c:p->clips)if(clipRect(c).contains(e.position)){
        owner.selectedClip=c.id;
#ifdef MRS_HAS_ARA
        if(!c.midi&&owner.app.ara_status(c.source)!="unbound"){
            owner.selectedTrack=c.track;owner.selectedClips={c.id.value};owner.openAra(c.id);return;
        }
#endif
        owner.openClipEditor(c.id);return;
    }
    const int at=trackAt(e.position.y);if(e.x<left||e.y<headerHeight()||at<0||at>=static_cast<int>(p->tracks.size()))return;const auto& track=p->tracks[static_cast<std::size_t>(at)];if(track.kind==mrs::TrackKind::instrument){const mrs::Timeline t(p->time,p->sample_rate);owner.selectedClip=owner.app.create_midi_clip(track.id,t.to_ticks(sampleAt(e.position.x)));owner.openMidiClip(*owner.selectedClip);}});
}
void Arrangement::zoom(double factor){const double anchor=juce::jmax(0.,(getWidth()-left)*.5);const auto before=(horizontal+anchor)/pixelsPerSecond;pixelsPerSecond=juce::jlimit(.1,2400.,pixelsPerSecond*factor);horizontal=juce::jmax(0.,before*pixelsPerSecond-anchor);resized();repaint();}
void Arrangement::fit(){const auto p=owner.project();const mrs::Timeline t(p->time,p->sample_rate);mrs::Sample end=p->sample_rate*16;for(const auto& c:p->clips)end=juce::jmax(end,mrs::clip_end(c,t));horizontal=0;pixelsPerSecond=juce::jlimit(.1,2400.,(getWidth()-left)*static_cast<double>(p->sample_rate)/end);repaint();}
bool Arrangement::isInterestedInFileDrag(const juce::StringArray& files){if(files.isEmpty())return false;bool midi=juce::File(files[0]).hasFileExtension("mid;midi");for(const auto& f:files)if(!juce::File(f).hasFileExtension(midi?"mid;midi":"wav"))return false;return true;}
std::optional<mrs::Id> Arrangement::midiDropTarget(int y) const {const int at=trackAt(static_cast<float>(y));const auto p=owner.project();if(y>=headerHeight()&&at>=0&&at<static_cast<int>(p->tracks.size())&&p->tracks[static_cast<std::size_t>(at)].kind==mrs::TrackKind::instrument)return p->tracks[static_cast<std::size_t>(at)].id;return {};}
mrs::Tick Arrangement::midiDropTick(int x) const {const auto p=owner.project();return mrs::Timeline(p->time,p->sample_rate).to_ticks(x<left?0:sampleAt(static_cast<float>(x)));}
void Arrangement::fileDragMove(const juce::StringArray& files,int x,int y){
    if(files!=dropFiles){dropFiles=files;dropParts.clear();
        {std::lock_guard lock(dropMutex);++dropGeneration;dropRequested=false;dropWaves.clear();pendingDropFiles.clear();
            if(isInterestedInFileDrag(files)&&files.size()<=128&&juce::File(files[0]).hasFileExtension("wav")){pendingDropFiles=files;dropRequested=true;dropWake.notify_one();}}
        if(isInterestedInFileDrag(files)&&juce::File(files[0]).hasFileExtension("mid;midi")){try{for(const auto& file:files){auto parts=mrs::load_midi_file(path(juce::File(file)));if(dropParts.size()+parts.size()>128)throw std::runtime_error("Too many MIDI parts");dropParts.insert(dropParts.end(),std::make_move_iterator(parts.begin()),std::make_move_iterator(parts.end()));}}catch(...){dropParts.clear();}}}
    dropPoint=y>=headerHeight()?std::optional<juce::Point<int>>{{x,y}}:std::nullopt;
    playhead.setVisible(files.isEmpty()||!juce::File(files[0]).hasFileExtension("wav"));repaint();
}
void Arrangement::fileDragExit(const juce::StringArray&){{std::lock_guard lock(dropMutex);++dropGeneration;dropRequested=false;pendingDropFiles.clear();dropWaves.clear();}dropFiles.clear();dropParts.clear();dropPoint.reset();playhead.setVisible(true);playhead.update();repaint();}
void Arrangement::itemDragMove(const SourceDetails& s){
#ifdef MRS_HAS_ARA
    araDropClip.reset();if(owner.isAraPluginDrag(s.description)){const auto id=clipAt(s.localPosition.toFloat());if(id){const auto p=owner.project();if(std::any_of(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==*id&&!c.midi;}))araDropClip=id;}}repaint();
#endif
    if(s.description.toString()=="mrs-sample-files")fileDragMove(owner.browser->selectedSamples(),s.localPosition.x,s.localPosition.y);
    if(s.description.toString()=="mrs-library-wav")fileDragMove(owner.browser->selectedLibrarySamples(),s.localPosition.x,s.localPosition.y);}
void Arrangement::itemDragExit(const SourceDetails&){araDropClip.reset();fileDragExit({});repaint();}
std::vector<juce::Rectangle<float>> Arrangement::midiDropRects() const {
    std::vector<juce::Rectangle<float>> result;if(!dropPoint)return result;const auto p=owner.project();const mrs::Timeline time(p->time,p->sample_rate);const auto tick=midiDropTick(dropPoint->x);const auto target=midiDropTarget(dropPoint->y);int next=static_cast<int>(p->tracks.size());
    for(std::size_t i=0;i<dropParts.size();++i){int row=next++;if(i==0&&target){--next;row=trackAt(static_cast<float>(dropPoint->y));}if(dropParts[i].midi.length>mrs::max_tick-tick)continue;const auto begin=time.to_samples(tick),end=time.to_samples(tick+dropParts[i].midi.length);result.push_back({static_cast<float>(left+static_cast<double>(begin)/p->sample_rate*pixelsPerSecond-horizontal),static_cast<float>(headerHeight()+trackY(row)-vertical+1),std::max(2.f,static_cast<float>(static_cast<double>(end-begin)/p->sample_rate*pixelsPerSecond)),static_cast<float>(trackHeight-2)});}
    return result;
}
std::vector<juce::Rectangle<float>> Arrangement::wavDropRects() const {
    std::vector<juce::Rectangle<float>> result;
    if(!dropPoint||dropFiles.isEmpty()||!juce::File(dropFiles[0]).hasFileExtension("wav"))return result;
    const auto p=owner.project();const int at=trackAt(static_cast<float>(dropPoint->y));
    const bool target=at>=0&&at<static_cast<int>(p->tracks.size());
    if(target&&p->tracks[static_cast<std::size_t>(at)].kind!=mrs::TrackKind::audio)return result;
    std::vector<DropWave> waves;{std::lock_guard lock(dropMutex);waves=dropWaves;}
    if(waves.size()!=static_cast<std::size_t>(dropFiles.size()))return result;
    mrs::Sample start=dropPoint->x<left?0:sampleAt(static_cast<float>(dropPoint->x));
    int row=target?at:static_cast<int>(p->tracks.size());
    for(const auto& wave:waves){
        if(!wave.rate||!wave.frames)return {};
        const auto frames=(wave.frames*p->sample_rate+wave.rate-1)/wave.rate;
        if(frames>static_cast<std::uint64_t>(mrs::max_sample-start))return {};
        result.push_back({static_cast<float>(left+static_cast<double>(start)/p->sample_rate*pixelsPerSecond-horizontal),
            static_cast<float>(headerHeight()+trackY(row)-vertical+1),
            std::max(2.f,static_cast<float>(static_cast<double>(frames)/p->sample_rate*pixelsPerSecond)),static_cast<float>(trackHeight-2)});
        if(target)start+=static_cast<mrs::Sample>(frames);else ++row;
    }
    return result;
}
void Arrangement::filesDropped(const juce::StringArray& files,int x,int y){fileDragExit(files);owner.run([&]{
    if(!isInterestedInFileDrag(files)||y<headerHeight())throw std::runtime_error("Drop WAV or MIDI files below the ruler");
    std::vector<std::filesystem::path> paths;for(const auto& file:files)paths.push_back(path(juce::File(file)));
    if(juce::File(files[0]).hasFileExtension("mid;midi")){const auto tick=midiDropTick(x);owner.closeEditors();const auto clips=owner.app.import_midi(paths,midiDropTarget(y),tick);owner.activateClip(clips.front());}
    else {std::optional<mrs::Id> target;const int at=trackAt(static_cast<float>(y));if(at>=0&&at<static_cast<int>(owner.project()->tracks.size()))target=owner.project()->tracks[static_cast<std::size_t>(at)].id;owner.closeEditors();owner.app.queue_wav_import(paths,target,x<left?0:sampleAt(static_cast<float>(x)));}
    owner.refresh(true);
});}
bool Arrangement::isInterestedInDragSource(const SourceDetails& details){
    const auto text=details.description.toString();
#ifdef MRS_HAS_ARA
    if(text.startsWith("ara2:"))return owner.isAraPluginDrag(details.description);
#endif
    if(text.startsWith("vst3:")){
        const auto index=text.substring(5);
        return !index.isEmpty()&&index.length()<=10&&index.containsOnly("0123456789")&&index.getLargeIntValue()<static_cast<juce::int64>(owner.catalog.size());
    }
    return text.startsWith("mrs-track:")||
        (text=="mrs-sample-files"&&!owner.browser->selectedSamples().isEmpty())||
        (text=="mrs-library-wav"&&!owner.browser->selectedLibrarySamples().isEmpty());
}
void Arrangement::itemDropped(const SourceDetails& details){if(!getLocalBounds().contains(details.localPosition)||details.localPosition.y<headerHeight())return;
    araDropClip.reset();repaint();
#ifdef MRS_HAS_ARA
    if(details.description.toString().startsWith("ara2:")||details.description.toString().startsWith("vst3:")){
        const auto id=clipAt(details.localPosition.toFloat());if(id){const auto p=owner.project();if(std::any_of(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==*id&&!c.midi;})){owner.dropAraPlugin(*id,details.description);return;}}
        if(details.description.toString().startsWith("ara2:"))return;
    }
#endif
    if(details.description.toString().startsWith("mrs-track:")){const auto y=details.localPosition.y;if(trackAt(static_cast<float>(y))<0)return;const auto index=juce::jlimit(0,static_cast<int>(owner.project()->tracks.size()),trackAt(static_cast<float>(y))+(y-headerHeight()+vertical-trackY(trackAt(static_cast<float>(y)))>=trackHeight*.5f?1:0));owner.dropTrack(details.description.toString(),static_cast<std::size_t>(index));}else if(isInterestedInDragSource(details)){if(details.description.toString().startsWith("vst3:")){const auto point=details.localPosition;if(!getLocalBounds().contains(point)||point.y<headerHeight()||(trackAt(static_cast<float>(point.y))<static_cast<int>(owner.project()->tracks.size())&&!midiDropTarget(point.y)))return;const auto index=static_cast<std::size_t>(details.description.toString().substring(5).getLargeIntValue());const auto target=midiDropTarget(details.localPosition.y);if(target)owner.dropPlugin(target,index);else owner.addInstrument(index);}else filesDropped(details.description.toString()=="mrs-library-wav"?
    owner.browser->selectedLibrarySamples():owner.browser->selectedSamples(),
    details.localPosition.x,details.localPosition.y);}}


namespace {
class BrowserNode final : public juce::TreeViewItem {
public:
    BrowserNode(juce::String text,int id=-1,std::function<juce::var()> describe={}):name(std::move(text)),plugin(id),describeDrag(std::move(describe)){}
    bool mightContainSubItems() override{return plugin<0;}
    juce::String getUniqueName() const override{return name;}
    void paintItem(juce::Graphics& g,int width,int height) override{
        const auto* view=getOwnerView();const auto* theme=view?dynamic_cast<const Theme*>(&view->getLookAndFeel()):nullptr;
        const bool themed=theme&&theme->conceptSkin();
        if(isSelected()){g.setColour(juce::Colour(themed?ConceptSkin::selection:accent));g.fillRect(0,0,width,height);}
        g.setColour(themed&&isSelected()?juce::Colour(ConceptSkin::canvas):juce::Colours::whitesmoke);g.setFont(ui::Theme::browserFont());g.drawText(name,2,0,width-4,height,juce::Justification::left);
    }
    juce::var getDragSourceDescription() override{if(describeDrag)return describeDrag();return plugin<0?juce::var():juce::var("vst3:"+juce::String(plugin));}
    juce::String name;int plugin;std::function<juce::var()> describeDrag;
};
}
Browser::Browser(Desktop& d):owner(d){
    search.setComponentID("browser-search");folderPath.setComponentID("browser-folder-path");fileTree.setComponentID("browser-files");
    for(auto item:{std::pair{&vstTab,"browser_fx"},std::pair{&filesTab,"browser_folder"},std::pair{&aiTab,"assistant"}}){item.first->getProperties().set("mrHostIcon",item.second);item.first->getProperties().set("mrSmallToolbarIcon",true);item.first->setTooltip(item.first==&vstTab?"FX plugins":item.first==&filesTab?"Files":"AI assistant");}vstTab.setComponentID("browser-vst-tab");filesTab.setComponentID("browser-files-tab");aiTab.setComponentID("browser-ai-tab");
    addAndMakeVisible(aiTab);aiTab.setTitle("AI Assistant tab");aiTab.onClick=[this]{showAssistant();};
    addAndMakeVisible(libraryTab);libraryTab.setComponentID("browser-library-tab");
    libraryTab.setTitle("MR Library WAV sources");libraryTab.setTooltip("Local WAV catalogue");
    libraryTab.onClick=[this]{showLibrary();};
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
    folderPath.setFont(Theme::browserFont());folderPath.setTitle("Computer folder path");folderPath.onReturnKey=[this]{owner.run([&]{navigate(juce::File(folderPath.getText()));});};
    fileTree.setColour(juce::TreeView::selectedItemBackgroundColourId,juce::Colour(0xff375a80));
    fileTree.setColour(juce::DirectoryContentsDisplayComponent::highlightColourId,juce::Colour(0xff375a80));fileTree.setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId,juce::Colours::white);
    fileTree.setColour(juce::TreeView::backgroundColourId,juce::Colour(surface));fileTree.setColour(juce::DirectoryContentsDisplayComponent::textColourId,juce::Colours::whitesmoke);
    fileTree.setTitle("Computer folders, WAV and MIDI files");fileTree.setDragAndDropDescription("mrs-sample-files");fileTree.setMultiSelectEnabled(true);
    fileThread.startThread();navigate(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));showFiles(false);
search.setTitle("Search VST3 plugins");search.setTextToShowWhenEmpty("Search VST3...",juce::Colours::grey);tree.setTitle("VST3 plugins by vendor");addAndMakeVisible(tree);addAndMakeVisible(scanButton);search.setFont(Theme::browserFont());addAndMakeVisible(search);tree.setDefaultOpenness(false);tree.setRootItemVisible(false);
    search.setTextToShowWhenEmpty("Search VST3...",juce::Colours::grey);search.onTextChange=[this]{rebuild();};
    scanButton.onClick=[this]{owner.choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[this](const auto& f){owner.scan(f);});};rebuild();}
Browser::~Browser(){stopTimer();fileTree.removeListener(this);breadcrumbView.setViewedComponent(nullptr,false);tree.setRootItem(nullptr);fileThread.stopThread(2000);}
void Browser::navigate(const juce::File& folder){if(!folder.isDirectory())throw std::runtime_error("Folder is unavailable");directory.setDirectory(folder,true,true);folderPath.setText(folder.getFullPathName(),false);
    crumbs.clear();std::vector<juce::File> parents;auto current=folder;for(int i=0;i<32;++i){parents.push_back(current);auto parent=current.getParentDirectory();if(parent==current)break;current=parent;}
    int x=0;for(auto it=parents.rbegin();it!=parents.rend();++it){auto file=*it;auto text=file.getFileName().isEmpty()?file.getFullPathName():file.getFileName();auto button=std::make_unique<juce::TextButton>(text+"  >");const int width=juce::jlimit(62,170,text.length()*7+28);button->setBounds(x,0,width,24);x+=width+2;
        button->setLookAndFeel(owner.assetLookAndFeel());juce::Component::SafePointer<Browser> safe(this);button->onClick=[safe,file]{juce::MessageManager::callAsync([safe,file]{if(safe)safe->owner.run([&]{safe->navigate(file);});});};breadcrumbBody.addAndMakeVisible(*button);crumbs.push_back(std::move(button));}
    breadcrumbBody.setSize(x,24);breadcrumbView.setViewPosition(juce::jmax(0,x-breadcrumbView.getWidth()),0);
}
void Browser::selectionChanged(){const auto files=selectedSamples();wantedFile=files.isEmpty()?juce::File():juce::File(files[0]);if(wantedFile==juce::File()){preview={};repaint();}}
void Browser::timerCallback(){
    const auto selected=selectedSamples();wantedFile=selected.isEmpty()?juce::File():juce::File(selected[0]);
    if(previewWorker.valid()){if(previewWorker.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;auto result=previewWorker.get();if(result.file==wantedFile){preview=std::move(result);repaint();}}
    if(wantedFile==juce::File()||preview.file==wantedFile)return;auto file=wantedFile;
    previewWorker=std::async(std::launch::async,[file]{Preview result;result.file=file;try{if(file.hasFileExtension("mid;midi")){auto parts=mrs::load_midi_file(path(file));std::size_t notes{};mrs::Tick end{};for(const auto& part:parts){notes+=part.midi.notes.size();end=std::max(end,part.midi.length);}result.info="MIDI | "+juce::String(static_cast<int>(parts.size()))+" parts | "+juce::String(static_cast<int>(notes))+" notes\n"+juce::String(static_cast<double>(end)/mrs::ppq,2)+" beats | Project tempo";return result;}auto wav=mrs::audio::inspect_wav(path(file));result.info=juce::String(wav.sample_rate/1000.,1)+" kHz | "+juce::String(wav.bits)+" bit | "+(wav.channels==1?"Mono":wav.channels==2?"Stereo":juce::String(wav.channels)+" channels")+"\nWAV | "+juce::String(static_cast<double>(wav.frame_count)/wav.sample_rate/60,2)+" min | "+file.getLastModificationTime().formatted("%d.%m.%Y %H:%M");
        for(std::size_t bin=0;bin<result.levels.size();++bin){const auto first=wav.frame_count*static_cast<mrs::Sample>(bin)/static_cast<mrs::Sample>(result.levels.size());const auto count=std::min<mrs::Sample>(128,wav.frame_count-first);if(count<=0)continue;std::vector<float> block(static_cast<std::size_t>(count)*wav.channels);wav.read(first,block);for(float value:block)if(std::isfinite(value))result.levels[bin]=std::max(result.levels[bin],std::abs(value));}
    }catch(const std::exception& e){result.info=label(e.what());}return result;});
}

void Browser::showFiles(bool files){if(libraryVisible)owner.app.stop_library_preview();filesVisible=files;libraryVisible=false;
    if(libraryPanel)libraryPanel->setVisible(false);
    libraryTab.setToggleState(false,juce::dontSendNotification);locations.setVisible(files);breadcrumbView.setVisible(files);for(auto* component:{static_cast<juce::Component*>(&fileTree),static_cast<juce::Component*>(&folderPath),static_cast<juce::Component*>(&parentFolder),static_cast<juce::Component*>(&chooseFolder)})component->setVisible(files);
    assistantVisible=false;if(assistant)assistant->setVisible(false);aiTab.setToggleState(false,juce::dontSendNotification);
    tree.setVisible(!files);scanButton.setVisible(!files);search.setVisible(!files);vstTab.setToggleState(!files,juce::dontSendNotification);filesTab.setToggleState(files,juce::dontSendNotification);repaint();}
juce::StringArray Browser::selectedSamples() const {juce::StringArray files;for(int i=0;i<fileTree.getNumSelectedFiles();++i){auto file=fileTree.getSelectedFile(i);if(file.existsAsFile()&&file.hasFileExtension("wav;mid;midi"))files.add(file.getFullPathName());}return files;}
juce::StringArray Browser::selectedLibrarySamples() const {
    return libraryPanel?libraryPanel->selectedLibrarySamples():juce::StringArray{};
}
void Browser::showLibrary(){
    showFiles(false);
    libraryVisible=true;
    tree.setVisible(false);scanButton.setVisible(false);search.setVisible(false);
    vstTab.setToggleState(false,juce::dontSendNotification);
    filesTab.setToggleState(false,juce::dontSendNotification);
    aiTab.setToggleState(false,juce::dontSendNotification);
    libraryTab.setToggleState(true,juce::dontSendNotification);
    if(!libraryPanel){
        libraryPanel=std::make_unique<LibraryFoundationPanel>(
            [this](const std::filesystem::path& file,double position){
                return owner.app.play_library_preview(file,position);
            },
            [this]{owner.app.stop_library_preview();},
            [this]{
                const auto state=owner.app.library_preview_state();
                return std::tuple{state.playing,state.progress,state.failed};
            });
        addChildComponent(libraryPanel.get());
        libraryPanel->setLookAndFeel(owner.assetLookAndFeel());
    }
    libraryPanel->setVisible(true);
    resized();repaint();
}
void Browser::showAssistant(){showFiles(false);assistantVisible=true;if(!assistant){assistant=makeAiAssistant(owner);addChildComponent(assistant.get());}assistant->setVisible(true);tree.setVisible(false);scanButton.setVisible(false);search.setVisible(false);vstTab.setToggleState(false,juce::dontSendNotification);aiTab.setToggleState(true,juce::dontSendNotification);resized();repaint();}
void Browser::resized(){vstTab.setBounds(8,0,32,28);filesTab.setBounds(42,0,32,28);aiTab.setBounds(76,0,32,28);libraryTab.setBounds(110,0,44,28);if(libraryPanel)libraryPanel->setBounds(4,30,getWidth()-8,getHeight()-34);if(assistant)assistant->setBounds(4,30,getWidth()-8,getHeight()-34);scanButton.setBounds(8,32,getWidth()-16,26);search.setBounds(8,64,getWidth()-16,26);tree.setBounds(8,98,getWidth()-16,getHeight()-122);
    locations.setBounds(8,32,getWidth()-132,26);parentFolder.setBounds(getWidth()-118,32,44,26);chooseFolder.setBounds(getWidth()-68,32,60,26);chooseFolder.setButtonText("...");folderPath.setBounds(8,64,getWidth()-16,26);breadcrumbView.setBounds(8,98,getWidth()-16,24);fileTree.setBounds(4,126,getWidth()-8,juce::jmax(30,getHeight()-278));}
void Browser::paint(juce::Graphics& g){drawCard(g,getLocalBounds().toFloat(),owner.assetSkin()?juce::Colour(surface):juce::Colour(panel));g.setColour(juce::Colours::whitesmoke);g.setFont(Theme::browserFont());
    if(filesVisible){auto area=juce::Rectangle<int>(0,getHeight()-148,getWidth(),124);g.setColour(owner.assetSkin()?juce::Colour(surface):juce::Colour(panel));g.fillRect(area);g.setColour(juce::Colours::whitesmoke);g.drawText(preview.file==juce::File()?"Select an audio or MIDI file":preview.file.getFileNameWithoutExtension(),area.removeFromTop(28).reduced(8,0),juce::Justification::left);
        {auto information=area.removeFromTop(42).reduced(8,0);const auto lines=juce::StringArray::fromLines(preview.info);for(int line=0;line<juce::jmin(2,lines.size());++line)g.drawText(lines[line],information.removeFromTop(21),juce::Justification::centredLeft,true);}auto wave=area.reduced(8,4).toFloat();g.setColour(juce::Colour(0xff929ba4));for(int x=0;x<static_cast<int>(wave.getWidth());++x){const auto index=static_cast<std::size_t>(x*256/juce::jmax(1,static_cast<int>(wave.getWidth())));const auto height=juce::jlimit(1.f,wave.getHeight(),preview.levels[index]*wave.getHeight());g.drawVerticalLine(static_cast<int>(wave.getX())+x,wave.getCentreY()-height/2,wave.getCentreY()+height/2);}
    }
    if(filesVisible)g.drawText("Drag audio / MIDI onto a track or empty space",8,getHeight()-23,getWidth()-16,20,juce::Justification::left);}
void Browser::rebuild(){tree.setRootItem(nullptr);root=std::make_unique<BrowserNode>("root");std::map<std::string,BrowserNode*> vendors;
#ifdef MRS_HAS_ARA
    auto* araGroup=new BrowserNode("ARA 2");root->addSubItem(araGroup);
    for(const auto& candidate:owner.araCatalog){const auto& c=candidate.capability;if(c.status!=mrs::ara::CapabilityStatus::available||(!search.getText().isEmpty()&&!label(c.name).containsIgnoreCase(search.getText())))continue;
        mrs::processing::VstPlugin plugin{candidate.module,c.class_id,c.name,"",c.version,false};juce::Component::SafePointer<Desktop> safe(&owner);
        araGroup->addSubItem(new BrowserNode(label(c.name),0,[safe,plugin]{return safe?safe->beginAraPluginDrag(plugin,true,0):juce::var();}));}
#endif
    for(std::size_t i=0;i<owner.catalog.size();++i){const auto& p=owner.catalog[i];if(!search.getText().isEmpty()&&!label(p.name+" "+p.vendor).containsIgnoreCase(search.getText()))continue;
        if(!vendors.contains(p.vendor)){auto* node=new BrowserNode(label(p.vendor));root->addSubItem(node);node->setOpen(false);vendors[p.vendor]=node;}
#ifdef MRS_HAS_ARA
        juce::Component::SafePointer<Desktop> safe(&owner);vendors[p.vendor]->addSubItem(new BrowserNode(label(p.name),static_cast<int>(i),[safe,plugin=p,i]{return safe?safe->beginAraPluginDrag(plugin,false,i):juce::var();}));
#else
        vendors[p.vendor]->addSubItem(new BrowserNode(label(p.name),static_cast<int>(i)));
#endif
    }
    tree.setRootItem(root.get());root->setOpen(true);
}
void browserDialogSmoke(Desktop& owner){
    auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    const auto before=*owner.project();owner.browser->showFiles(false);
    for(const auto* id:{"browser-search","browser-folder-path","browser-files","browser-ai-tab"}){
        const auto* control=owner.browser->findChildWithID(id);
        check(control&&&control->getLookAndFeel()==owner.assetLookAndFeel(),"P4 Browser controls borrow scoped Theme");
    }
    auto* selected=new BrowserNode("P4 selected plugin",0);owner.browser->tree.getRootItem()->addSubItem(selected);selected->setSelected(true,true);
    const auto save=[](juce::Component& component,const char* name){
        auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(name).createOutputStream();
        if(!stream||!juce::PNGImageFormat().writeImageToStream(component.createComponentSnapshot(component.getLocalBounds()),*stream))throw std::runtime_error("P4 screenshot");
    };
    save(*owner.browser,"p4-browser-vst.png");owner.browser->rebuild();owner.browser->showFiles(true);save(*owner.browser,"p4-browser-files.png");
    {
        StudioAlertWindow dialog(owner,"MR Studio / project name","",juce::MessageBoxIconType::NoIcon);
        dialog.addTextEditor("value",juce::String::fromUTF8("Название проекта"));dialog.addButton("OK",1,juce::KeyPress(juce::KeyPress::returnKey));dialog.addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
        check(dialog.getTextEditorContents("value")==juce::String::fromUTF8("Название проекта"),"P4 dialog preserves Unicode input");
        check(dialog.getLookAndFeel().findColour(juce::AlertWindow::backgroundColourId)==DesignTokens::target().colour(ColourRole::window),"P4 dialog scoped palette");
        save(dialog,"p4-dialog.png");
    }
    owner.browser->showFiles(false);check(*owner.project()==before,"P4 visuals do not edit project");
}
void Desktop::scan(const juce::File& root){if(scanner.valid())throw std::runtime_error("Scan already running");*scanCancel=false;const auto folder=path(root);const auto helper=path(juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("mrs_vst3_scan.exe"));
    auto cache=cacheFile;auto cancel=scanCancel;scanner=std::async(std::launch::async,[folder,helper,cache,cancel]{return mrs::processing::scan_vst3(folder,helper,cache,cancel);});}
std::vector<mrs::NativeInsert> Desktop::chain(std::optional<mrs::Id> t) const{const auto p=project();if(!t)return p->master_inserts;for(const auto& track:p->tracks)if(track.id==t)return track.inserts;throw std::runtime_error("Track no longer exists");}
void Desktop::closeEditors(bool keepMidi){if(dockedEditor&&(!keepMidi||static_cast<bool>(dockedEditor->getProperties()["mrs-ara-editor"]))){retireAraComponent(*dockedEditor);dockedEditor->setVisible(false);dockedClip.reset();}app.engine()->set_profiling(false);app.close_plugin_editors();for(auto& w:windows){if(keepMidi&&static_cast<bool>(w->getProperties()["mrs-midi-clip"]))continue;retirePluginWindow(*w);retireAraWindow(*w);w->setVisible(false);
    // Retire the native parent HWND while its plugin module is still alive.
    // Keep the C++ window until the next timer turn so an editor callback can return safely.
    if(static_cast<bool>(w->getProperties()["mrs-native-editor"]))w->removeFromDesktop();}}
void Desktop::closeUnpinnedEditors(){if(dockedEditor&&static_cast<bool>(dockedEditor->getProperties()["mrs-ara-editor"])&&!static_cast<bool>(dockedEditor->getProperties()["mrs-pinned"])){retireAraComponent(*dockedEditor);dockedEditor->setVisible(false);dockedClip.reset();}for(auto& w:windows)if(w->isVisible()&&!static_cast<bool>(w->getProperties()["mrs-pinned"]))w->closeButtonPressed();}
void Desktop::applyChain(std::optional<mrs::Id> target,std::vector<mrs::NativeInsert> effects){
    if(insertPowerProject!=app.services().projects){insertPowerRecall.clear();insertPowerProject=app.services().projects;}
    app.set_inserts(target,std::move(effects));
    editorGeneration=app.insert_generation();
    refresh(true);
}
bool Desktop::insertDropAllowed(std::optional<mrs::Id> destination) const{
    if(!insertDrag||app.services().projects!=insertDrag->store||insertDrag->store->state().revision!=insertDrag->revision||app.recording()||app.engine()->state().playback==mrs::PlaybackState::playing)return false;
    try{const auto from=chain(insertDrag->source),to=chain(destination);const auto at=std::find_if(from.begin(),from.end(),[&](const auto& fx){return fx.id==insertDrag->slot;});if(at==from.end())return false;
        const auto p=project();for(const auto& t:p->tracks){if(t.id==destination&&t.kind==mrs::TrackKind::midi)return false;if(t.id==insertDrag->source&&t.kind==mrs::TrackKind::instrument&&at==from.begin())return false;}
        return destination==insertDrag->source||to.size()<8;
    }catch(const std::exception&){return false;}
}
void Desktop::toggleInsertPower(std::optional<mrs::Id> target){
    if(insertPowerProject!=app.services().projects){insertPowerRecall.clear();insertPowerProject=app.services().projects;}
    auto effects=chain(target);const auto p=project();const bool instrument=target&&std::any_of(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==target&&t.kind==mrs::TrackKind::instrument;});const std::size_t start=instrument?1:0;if(effects.size()<=start)return;
    const auto key=target?target->value:std::string{};const bool off=std::all_of(effects.begin()+static_cast<std::ptrdiff_t>(start),effects.end(),[](const auto& fx){return fx.bypass;});
    auto recall=insertPowerRecall[key];
    if(off){for(std::size_t i=start;i<effects.size();++i){const auto at=recall.find(effects[i].id.value);effects[i].bypass=at!=recall.end()?at->second:false;}}
    else{recall.clear();for(std::size_t i=start;i<effects.size();++i){recall[effects[i].id.value]=effects[i].bypass;effects[i].bypass=true;}}
    // The actual bypass remains project/Undo state. Recall is a session utility,
    // separate from parameters and presets; new FX keep their own power state.
    app.set_inserts(target,std::move(effects));insertPowerRecall[key]=std::move(recall);editorGeneration=app.insert_generation();refresh(true);
}
void Desktop::addPlugin(std::optional<mrs::Id> target,std::size_t i,std::optional<std::size_t> position){run([&]{if(i>=catalog.size())throw std::runtime_error("Unknown browser plugin");auto effects=chain(target);if(effects.size()>=8)throw std::runtime_error("Insert chain supports up to eight effects");closeEditors();const auto& p=catalog[i];mrs::NativeInsert n;n.id=mrs::new_id();n.kind=mrs::InsertKind::vst3;n.plugin_name=p.name;n.plugin_path=p.path;n.class_id=p.class_id;effects.insert(effects.begin()+static_cast<std::ptrdiff_t>(std::min(position.value_or(effects.size()),effects.size())),n);applyChain(target,std::move(effects));openInsert(target,n.id);});}
void Desktop::importMidiFiles(const juce::StringArray& files,std::optional<mrs::Id> target,mrs::Tick tick){std::vector<std::filesystem::path> paths;for(const auto& file:files)paths.push_back(path(juce::File(file)));closeEditors();const auto clips=app.import_midi(paths,target,tick);activateClip(clips.front());refresh(true);}
void Desktop::addInstrument(std::size_t i){run([&]{
    if(i>=catalog.size())throw std::runtime_error("Unknown browser plugin");
    const auto& plugin=catalog[i];mrs::NativeInsert insert;insert.id=mrs::new_id();insert.kind=mrs::InsertKind::vst3;
    insert.plugin_name=plugin.name;insert.plugin_path=plugin.path;insert.class_id=plugin.class_id;
    // Editors must detach before any engine preparation, including rollback.
    closeEditors();selectedTrack=app.add_instrument_track(plugin.name,insert);refresh(true);
    openInsert(selectedTrack,insert.id);
});}
void Desktop::replaceInstrument(mrs::Id target,std::size_t i){
    if(i>=catalog.size())throw std::runtime_error("Unknown browser instrument");
    const auto p=project();const auto t=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==target;});
    if(t==p->tracks.end()||t->kind!=mrs::TrackKind::instrument)throw std::runtime_error("Instrument track no longer exists");
    auto effects=t->inserts;const auto& plugin=catalog[i];mrs::NativeInsert n;n.id=mrs::new_id();n.kind=mrs::InsertKind::vst3;n.plugin_name=plugin.name;n.plugin_path=plugin.path;n.class_id=plugin.class_id;
    if(effects.empty())effects.push_back(n);else effects.front()=n;
    closeEditors();applyChain(target,std::move(effects));openInsert(target,n.id);
}
void Desktop::dropPlugin(std::optional<mrs::Id> target,std::size_t i,std::optional<std::size_t> position){run([&]{
    if(i>=catalog.size())throw std::runtime_error("Unknown browser plugin");
    // Old cache entries have no category. Resolve only the chosen module, off audio threads.
    if(!catalog[i].instrument){const auto found=mrs::processing::probe_vst3(catalog[i].path);const auto selected=std::find_if(found.begin(),found.end(),[&](const auto& p){return p.class_id==catalog[i].class_id;});if(selected==found.end())throw std::runtime_error("Plugin class no longer exists");catalog[i].instrument=selected->instrument;}
    if(!catalog[i].instrument.value_or(false)){addPlugin(target,i,position);return;}
    if(!target){addInstrument(i);return;}
    const auto p=project();const auto t=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==*target;});
    if(t==p->tracks.end()||t->kind!=mrs::TrackKind::instrument)throw std::runtime_error("Drop VSTi on an instrument track or empty space");
    if(t->inserts.empty()){replaceInstrument(*target,i);return;}
    const auto store=app.services().projects;const auto revision=store->state().revision;const auto descriptor=catalog[i];juce::Component::SafePointer<Desktop> safe(this);
    auto* alert=new StudioAlertWindow(*this,"Replace instrument?","Replace the instrument on "+label(t->name)+"? Effects after the instrument will be kept.",juce::MessageBoxIconType::QuestionIcon);
    alert->addButton("Replace",1,juce::KeyPress(juce::KeyPress::returnKey));alert->addButton("Do not touch",0,juce::KeyPress(juce::KeyPress::escapeKey));
    alert->enterModalState(true,juce::ModalCallbackFunction::create([safe,store,revision,target,i,descriptor](int result){if(result==1&&safe&&safe->app.services().projects==store&&store->state().revision==revision&&i<safe->catalog.size()&&safe->catalog[i]==descriptor)safe->run([&]{safe->replaceInstrument(*target,i);});}),true);
});}
void Desktop::routeMenu(mrs::Id id){juce::PopupMenu m;m.setLookAndFeel(&theme);auto p=project();
    const auto source=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==id;});if(source==p->tracks.end())return;
    m.addItem(1,"Master",true,!source->output&&source->hardware_outputs.empty());
    for(std::size_t i=0;i<p->tracks.size();++i)if(p->tracks[i].kind==mrs::TrackKind::bus&&p->tracks[i].id!=id)m.addItem(10+static_cast<int>(i),label(p->tracks[i].name),true,source->output==p->tracks[i].id);
    m.addItem(2,"Hardware outputs...",true,!source->hardware_outputs.empty());
    juce::Component::SafePointer<Desktop> safe(this);m.showMenuAsync(popup(this),[safe,id,p](int n){if(!safe||!n)return;safe->run([&]{if(n==2)safe->textDialog("Hardware outputs (1,2...)","1,2",[safe,id](auto s){if(safe)safe->app.set_hardware_output(id,mrs::desktop::parse_outputs(s.toStdString()));});else safe->app.set_track_output(id,n==1?std::optional<mrs::Id>{}:p->tracks[static_cast<std::size_t>(n-10)].id);});});}
juce::PopupMenu Desktop::sendMenu(mrs::Id id,std::optional<mrs::Id> bus,std::vector<mrs::Id>& choices) const{
    juce::PopupMenu options;choices.clear();const auto p=project();const auto source=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==id;});if(source==p->tracks.end())return options;
    const auto selected=std::find_if(source->sends.begin(),source->sends.end(),[&](const auto& s){return bus&&s.bus==*bus;});if(bus&&selected==source->sends.end())return options;
    mrs::audio::RealtimeState state;const bool stopped=app.engine()->try_state(state)&&state.playback!=mrs::PlaybackState::playing&&!app.recording();
    options.addItem(1,"New return / send...",stopped&&source->sends.size()<8);
    for(const auto& destination:p->tracks)if(destination.kind==mrs::TrackKind::bus){
        choices.push_back(destination.id);bool valid=stopped&&(bus.has_value()||source->sends.size()<8);
        auto candidate=*p;auto& sends=candidate.tracks[static_cast<std::size_t>(source-p->tracks.begin())].sends;
        if(bus)sends[static_cast<std::size_t>(selected-source->sends.begin())].bus=destination.id;else sends.push_back({destination.id,1,false});
        try{candidate.validate();}catch(...){valid=false;}
        options.addItem(10+static_cast<int>(choices.size()-1),label(destination.name),valid,bus&&destination.id==*bus);
    }
    if(bus){options.addSeparator();options.addItem(2,selected->pre_fader?"Use post fader":"Use pre fader",stopped);options.addItem(3,"Remove send",stopped);}
    return options;
}
void Desktop::applySendChoice(mrs::Id id,std::optional<mrs::Id> bus,int choice,const std::vector<mrs::Id>& choices){
    if(!choice)return;const auto p=project();const auto source=std::find_if(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==id;});if(source==p->tracks.end())return;
    auto sends=source->sends;const auto selected=std::find_if(sends.begin(),sends.end(),[&](const auto& s){return bus&&s.bus==*bus;});if(bus&&selected==sends.end())return;
    if(app.recording()||app.engine()->state().playback==mrs::PlaybackState::playing)throw std::runtime_error("Pause or stop playback before changing send destinations");
    if(choice==1){if(sends.size()>=8)return;closeEditors();app.add_return_send(id,"Return");return;}
    if(choice==2&&bus)selected->pre_fader=!selected->pre_fader;
    else if(choice==3&&bus)sends.erase(selected);
    else if(choice>=10&&static_cast<std::size_t>(choice-10)<choices.size()){
        const auto destination=choices[static_cast<std::size_t>(choice-10)];if(bus&&destination==*bus)return;
        if(bus)selected->bus=destination;else sends.push_back({destination,1,false});
    }else return;
    auto candidate=*p;candidate.tracks[static_cast<std::size_t>(source-p->tracks.begin())].sends=sends;candidate.validate();
    closeEditors();cancelPreview();app.set_track_sends(id,std::move(sends));
}
void Desktop::sendsMenu(mrs::Id id,std::optional<mrs::Id> bus,juce::Component* anchor){
    std::vector<mrs::Id> choices;auto options=sendMenu(id,bus,choices);options.setLookAndFeel(&theme);
    const auto store=app.services().projects;juce::Component::SafePointer<Desktop> safe(this);
    options.showMenuAsync(popup(anchor?anchor:this),[safe,store,id,bus,choices](int choice){if(safe&&safe->app.services().projects==store)safe->run([&]{safe->applySendChoice(id,bus,choice,choices);});});
}
}
