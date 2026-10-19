#include "Desktop.h"
#include "NativeEditorSpace.h"
#include <windows.h>
#include <fstream>
#include <cmath>

namespace ui {
#ifdef MRS_HAS_ARA
namespace {
std::filesystem::path fsPath(const juce::File& f){return std::filesystem::path(f.getFullPathName().toWideCharPointer());}
juce::String displayStatus(const std::string& state){
    if(state.find("editor transport unavailable")!=std::string::npos)return "Editor transport unavailable — reopen editor";
    if(state=="unbound")return "Not connected";
    if(state.starts_with("ready"))return state.find("live ARA playback")!=std::string::npos?"Ready - live ARA":"Ready";
    if(state.starts_with("analysing"))return "Analysing — original audio";
    if(state.starts_with("pending"))return "Playback update needed";
    if(state.starts_with("disabled"))return "Bypassed — original audio";
    return juce::String(state);
}
const mrs::Clip& audioClip(const mrs::Project& p,const mrs::Id& id) {
    const auto at=std::find_if(p.clips.begin(),p.clips.end(),[&](const auto& c){return c.id==id&&!c.midi;});
    if(at==p.clips.end())throw std::runtime_error("Audio clip no longer exists");return *at;
}
// Only this modal HWND is pumped while immutable PCM is prepared. No Desktop,
// vendor window, timer, command or model notification can re-enter the session.
class PreparationProgress final {
    struct Content final:juce::Component {
        juce::Label label;juce::TextButton cancel{"Cancel"};
        explicit Content(const std::shared_ptr<std::atomic<bool>>& flag){setSize(440,112);addAndMakeVisible(label);addAndMakeVisible(cancel);label.setText("Preparing ARA playback...",juce::dontSendNotification);label.setBounds(16,12,408,32);cancel.setBounds(300,64,120,30);cancel.onClick=[this,flag]{flag->store(true);label.setText("Cancelling after the current plugin block...",juce::dontSendNotification);cancel.setEnabled(false);};}
    };
    Desktop& owner;std::shared_ptr<std::atomic<bool>> cancel;std::unique_ptr<EditorWindow> window;std::vector<HWND> disabled;
    HWND previousFocus{},previousActive{},previousForeground{};
    const std::chrono::steady_clock::time_point started{std::chrono::steady_clock::now()};
    const std::chrono::milliseconds delay;
public:
    PreparationProgress(Desktop& d,std::shared_ptr<std::atomic<bool>> c,std::chrono::milliseconds threshold=std::chrono::milliseconds(350)):owner(d),cancel(std::move(c)),delay(threshold){}
    bool visible() const {return window!=nullptr;}
    ~PreparationProgress(){
        if(disabled.empty())return;
        // Re-enable owners before destroying the active progress window. Windows
        // otherwise has no eligible owner to activate and keyboard focus is lost.
        const auto foreground=GetForegroundWindow();
        // Closing the progress HWND can activate another owned MR window before
        // this destructor runs. Restore within MR, while respecting an explicit
        // switch to another application.
        const bool ownedForeground=(owner.getPeer()&&foreground==owner.getPeer()->getNativeHandle())||
            std::any_of(owner.windows.begin(),owner.windows.end(),[foreground](const auto& editor){return editor->getPeer()&&foreground==editor->getPeer()->getNativeHandle();});
        const bool restore=ownedForeground||(window&&window->getPeer()&&foreground==window->getPeer()->getNativeHandle())||foreground==previousForeground;
        for(auto h:disabled)if(IsWindow(h))EnableWindow(h,TRUE);
        window.reset();
        if(restore&&IsWindow(previousActive)&&IsWindowEnabled(previousActive)){
            SetActiveWindow(previousActive);
            if(IsWindow(previousFocus)&&IsWindowEnabled(previousFocus))SetFocus(previousFocus);
        }
    }
    void tick(){
        if(!window&&std::chrono::steady_clock::now()-started<delay)return;
        if(!window){previousFocus=GetFocus();previousActive=GetActiveWindow();previousForeground=GetForegroundWindow();
            window=std::make_unique<EditorWindow>("ARA playback preparation",new Content(cancel),false);window->setLookAndFeel(&owner.theme);window->setOwner(owner);window->onClose=[flag=cancel]{flag->store(true);};
            // Cancel requests must keep the active HWND alive until the guarded
            // owners are enabled by the destructor. Hiding it while every owner
            // is disabled can activate an unrelated application.
            window->getProperties().set("mrs-hide-on-close",false);window->setVisible(true);
            // Keep an eligible MR window active throughout the handoff. Disabling
            // every owner before showing progress can activate another application.
            auto disable=[this](juce::Component& c){if(auto* peer=c.getPeer()){const auto h=static_cast<HWND>(peer->getNativeHandle());if(IsWindowEnabled(h)){EnableWindow(h,FALSE);disabled.push_back(h);}}};
            disable(owner);for(auto& w:owner.windows)if(w->isVisible())disable(*w);
        }
        MSG message{};const auto h=static_cast<HWND>(window->getPeer()->getNativeHandle());
        while(PeekMessageW(&message,h,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        RedrawWindow(h,nullptr,nullptr,RDW_UPDATENOW|RDW_ALLCHILDREN);
    }
};
class AraPanel final : public juce::Component, private juce::Timer, private juce::ScrollBar::Listener {
public:
    AraPanel(Desktop& d,mrs::Id id):owner(d),clip(id),store(d.app.services().projects),source(audioClip(*d.project(),id).source) {
        setSize(850,520);setTitle("ARA editor");setWantsKeyboardFocus(true);
        getProperties().set("mrs-ara-editor",true);getProperties().set("mrs-ara-document",juce::String(owner.app.ara_document_key(source)));getProperties().set("mrs-ara-source",juce::String(source));
        int i=0;for(auto* b:{&bypass,&detach}){addAndMakeVisible(b);b->setComponentID(i==0?"ara-bypass":"ara-detach");b->setTitle(b->getButtonText());++i;}
        addAndMakeVisible(status);status.setComponentID("ara-status");status.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(note);note.setComponentID("ara-preparation-details");note.setJustificationType(juce::Justification::centredLeft);note.setFont(juce::Font(juce::FontOptions(12.f)));
        if(owner.assetSkin()){
            auto tokens=DesignTokens::target();tokens.colours[static_cast<std::size_t>(ColourRole::raised)]=ConceptSkin::channelButton;
            chromeTheme.applyPreview(tokens);chromeTheme.useConceptAssets(true);
            for(auto* b:{&bypass,&detach,&pin,&dock,&full})b->setLookAndFeel(&chromeTheme);
        }
        bypass.getProperties().set("mrHostIcon","power");bypass.setTitle("ARA processing power");
        addAndMakeVisible(pin);pin.setComponentID("ara-pin");pin.getProperties().set("mrHostIcon","pin");pin.setTitle("Pin ARA editor");pin.setClickingTogglesState(true);pin.setTooltip("Keep this editor on the current clip; ignore clip selection");
        pin.onClick=[this]{getProperties().set("mrs-pinned",pin.getToggleState());if(window)window->getProperties().set("mrs-pinned",pin.getToggleState());};
        addAndMakeVisible(dock);dock.setComponentID("ara-attach");dock.getProperties().set("mrHostIcon","editor_attach");dock.setTitle("Attach ARA editor");dock.setTooltip("Attach editor to MR Studio");dock.onClick=[this]{juce::Component::SafePointer<AraPanel> safe(this);juce::MessageManager::callAsync([safe]{if(safe)safe->owner.run([&]{safe->returnAttached=false;if(safe->window&&safe->window->isFullScreen())safe->window->setFullScreen(false);safe->owner.attachAra(safe,!static_cast<bool>(safe->getProperties()["mrs-attached"]));});});};
        addAndMakeVisible(full);full.setComponentID("ara-fullscreen");full.getProperties().set("mrHostIcon","editor_fullscreen");full.setTitle("Full screen ARA editor");full.setTooltip("Full screen / Restore (F11)");full.onClick=[this]{deferFullScreen();};
        horizontal.setComponentID("ara-scroll-horizontal");vertical.setComponentID("ara-scroll-vertical");for(auto* bar:{&horizontal,&vertical}){addChildComponent(bar);bar->addListener(this);}
        bypass.onClick=[this]{owner.run([&]{validate();owner.app.bypass_ara_source(source,!owner.app.ara_bypassed(source));sync();});};
        detach.setTooltip("Remove ARA from this source and all its copies; keep original audio");
        detach.onClick=[this]{auto* d=&owner;const auto src=source;const auto expected=store;juce::Component::SafePointer<Desktop> safe(d);
            juce::MessageManager::callAsync([safe,src,expected]{if(safe&&safe->app.services().projects==expected)safe->run([&]{safe->closeAra(src);safe->app.detach_ara_source(src);safe->refresh(true);});});};
        sync();
    }
    ~AraPanel() override {retire();for(auto* b:{&bypass,&detach,&pin,&dock,&full})b->setLookAndFeel(nullptr);}
    void select(mrs::Id id){if(owner.app.services().projects!=store)throw std::runtime_error("ARA project changed");clip=std::move(id);source=audioClip(*owner.project(),clip).source;getProperties().set("mrs-ara-source",juce::String(source));if(window)window->getProperties().set("mrs-ara-source",juce::String(source));else owner.dockedClip=clip;validate();sync();}
    void validate() const {if(owner.app.services().projects!=store||audioClip(*owner.project(),clip).source!=source)throw std::runtime_error("ARA project or source changed");}
    void retire(){if(retired)return;retired=true;stopTimer();space.reset();if(owner.app.services().projects==store)owner.app.close_ara_editor(source);if(clipHost&&IsWindow(clipHost))DestroyWindow(clipHost);clipHost=host=nullptr;}
    void attach(EditorWindow& w){window=&w;w.getProperties().set("mrs-ara-source",juce::String(source));
        w.getProperties().set("mrs-ara-document",juce::String(owner.app.ara_document_key(source)));
        clipHost=CreateWindowExW(0,L"STATIC",L"MR Studio ARA viewport",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,96,640,420,static_cast<HWND>(w.getPeer()->getNativeHandle()),nullptr,GetModuleHandleW(nullptr),nullptr);
        host=CreateWindowExW(0,L"STATIC",L"MR Studio ARA host",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,0,640,420,clipHost,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!host)throw std::runtime_error("Cannot create ARA editor parent");
        try {native=owner.app.open_ara_editor(source,clip,host,width,height);}
        catch(const std::exception& e){editorError=juce::String::fromUTF8(e.what());}
        if(!native){
            // A failed native child otherwise covers the diagnostic painted by
            // the panel with a white STATIC control and its debug caption.
            if(editorError.isEmpty())editorError="The plugin did not provide a native ARA editor.";
            owner.app.close_ara_editor(source);DestroyWindow(clipHost);clipHost=host=nullptr;
            fit();sync();startTimerHz(10);return;
        }
        w.getProperties().set("mrs-native-host",static_cast<juce::int64>(reinterpret_cast<std::intptr_t>(host)));
        rehost(&w);
        fit();sync();startTimerHz(10);
    }
    void fit(){if(!window)return;const auto scale=window->getPeer()->getPlatformScaleFactor();window->fitNativeEditor(juce::jmax(width,juce::roundToInt(850*scale)),height+juce::roundToInt(toolbar*scale));window->setResizeLimits(640,260,8192,8192);if(const auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(window->getBounds())){const auto area=display->userBounds.toNearestInt().reduced(8);auto bounds=window->getBounds();bounds.setSize(juce::jmin(bounds.getWidth(),area.getWidth()),juce::jmin(bounds.getHeight(),area.getHeight()));window->setBounds(bounds.constrainedWithin(area));}resized();}
    void resized() override {bypass.setBounds(8,8,28,30);detach.setBounds(44,8,80,30);pin.setBounds(getWidth()-40,8,28,30);dock.setBounds(getWidth()-74,8,28,30);full.setBounds(getWidth()-108,8,28,30);status.setBounds(136,8,juce::jmax(1,getWidth()-254),30);note.setBounds(8,44,getWidth()-16,34);layoutNative(true);}
    void visibilityChanged() override {if(clipHost&&!layingOut)updateNativeVisibility();}
    // JUCE calls moved() before resized() for a combined bounds change. Let
    // resized() publish the final native layout once, without an intermediate
    // viewport move using the previous vendor size.
    void moved() override {if(lastLayoutSize==juce::Point<int>{getWidth(),getHeight()})layoutNative(false);}
    bool keyPressed(const juce::KeyPress& key) override {if(key.getKeyCode()==juce::KeyPress::F11Key){deferFullScreen();return true;}return false;}
    void rehost(EditorWindow* next){space.reset();window=next;if(window){window->getProperties().set("mrs-ara-source",juce::String(source));window->getProperties().set("mrs-ara-document",getProperties()["mrs-ara-document"]);window->getProperties().set("mrs-pinned",pin.getToggleState());window->getProperties().set("mrs-native-host",static_cast<juce::int64>(reinterpret_cast<std::intptr_t>(host)));}
        if(clipHost&&getPeer()){SetParent(clipHost,static_cast<HWND>(getPeer()->getNativeHandle()));juce::Component::SafePointer<AraPanel> safe(this);space=std::make_unique<NativeEditorSpace>(*this,[safe]{juce::MessageManager::callAsync([safe]{if(safe&&safe->isShowing()&&safe->host){safe->owner.spaceKey(false);safe->owner.spaceKey(true);safe->owner.spaceKey(false);}});},clipHost,[safe]{if(safe)safe->deferFullScreen();});}sync();resized();}
    void paint(juce::Graphics& g) override {drawCard(g,getLocalBounds().toFloat(),chromeTheme.conceptSkin()?DesignTokens::target().colour(ColourRole::window):juce::Colour(panel));if(!native){g.setColour(juce::Colour(primaryText));g.drawFittedText("Editor unavailable\n"+editorError+"\nClose this window and reopen the editor to retry. Playback controls remain available.",getLocalBounds().withTrimmedTop(toolbar).reduced(16),juce::Justification::centred,6);}}
    void sync(){validate();const bool stopped=!owner.app.recording()&&owner.app.engine()->state().playback!=mrs::PlaybackState::playing;
        bypass.setEnabled(stopped);detach.setEnabled(stopped);bypass.setButtonText(owner.app.ara_bypassed(source)?"Enable":"Bypass");
        bypass.setToggleState(owner.app.ara_bypassed(source),juce::dontSendNotification);bypass.setTooltip(owner.app.ara_bypassed(source)?"ARA bypassed — enable processing":"ARA active — bypass processing");
        const auto raw=owner.app.ara_status(source);auto state=displayStatus(raw);if(!native&&!editorError.isEmpty())state="Editor unavailable; playback: "+state;if(!stopped)state=(owner.app.ara_bypassed(source)?"ARA BYPASSED - original audio; ":raw.find("live ARA playback")!=std::string::npos?"ARA ON - live playback; ":"ARA ON - prepared playback; ")+state+"; toolbar locked";status.setText(state,juce::dontSendNotification);status.setTooltip(editorError.isEmpty()?juce::String(raw):editorError);
        if(const auto at=raw.find("; PCM ");at!=std::string::npos)lastPreparation=juce::String(raw.substr(at+2)).replace("; reason=","\nUpdate cause: ");
        note.setText(lastPreparation.isEmpty()?"ARA edits are saved with the project. Use the plugin's own Undo for note edits.":lastPreparation,juce::dontSendNotification);const bool attached=static_cast<bool>(getProperties()["mrs-attached"]);dock.getProperties().set("mrHostIcon",attached?"editor_detach":"editor_attach");dock.setTitle(attached?"Detach ARA editor":"Attach ARA editor");dock.setTooltip(attached?"Detach editor into a separate window":"Attach editor to MR Studio");full.getProperties().set("mrHostIcon",window&&window->isFullScreen()?"editor_restore":"editor_fullscreen");dock.repaint();full.repaint();}
private:
    friend class ui::Desktop;
    void deferFullScreen(){juce::Component::SafePointer<AraPanel> safe(this);juce::MessageManager::callAsync([safe]{if(!safe)return;safe->owner.run([&]{if(!safe->window){safe->returnAttached=true;safe->owner.attachAra(safe,false);safe->owner.araEditorAttached=true;}if(safe->window->isFullScreen()){safe->window->setFullScreen(false);const bool restoreDock=safe->returnAttached;safe->returnAttached=false;if(restoreDock)safe->owner.attachAra(safe,true);}else safe->window->setFullScreen(true);safe->sync();safe->resized();});});}
    void scrollBarMoved(juce::ScrollBar*,double) override {layoutNative(false);}
    void updateNativeVisibility(){const bool visible=(GetWindowLongPtrW(clipHost,GWL_STYLE)&WS_VISIBLE)!=0;if(visible!=isShowing())ShowWindow(clipHost,isShowing()?SW_SHOWNA:SW_HIDE);}
    void layoutNative(bool resize){if(!host||!getPeer()||layingOut||retired)return;juce::ScopedValueSetter<bool> guard(layingOut,true);
        lastLayoutSize={getWidth(),getHeight()};const auto scale=getPeer()->getPlatformScaleFactor();auto area=getLocalBounds().withTrimmedTop(toolbar);if(resize&&native){int w=juce::jmax(1,juce::roundToInt(area.getWidth()*scale)),h=juce::jmax(1,juce::roundToInt(area.getHeight()*scale));try{if((w!=width||h!=height)&&owner.app.resize_ara_editor(source,w,h)){width=w;height=h;}}catch(const std::exception&){}}
        const double logicalWidth=width/scale,logicalHeight=height/scale;bool v=logicalHeight>area.getHeight(),h=logicalWidth>area.getWidth()-(v?14:0);if(h&&logicalHeight>area.getHeight()-14)v=true;horizontal.setVisible(h);vertical.setVisible(v);if(v)vertical.setBounds(area.removeFromRight(14));if(h)horizontal.setBounds(area.removeFromBottom(14));horizontal.setRangeLimits(0,juce::jmax(logicalWidth,static_cast<double>(area.getWidth())));vertical.setRangeLimits(0,juce::jmax(logicalHeight,static_cast<double>(area.getHeight())));horizontal.setCurrentRange(horizontal.getCurrentRangeStart(),area.getWidth(),juce::dontSendNotification);vertical.setCurrentRange(vertical.getCurrentRangeStart(),area.getHeight(),juce::dontSendNotification);
        const auto place=[](HWND child,int x,int y,int w,int h){RECT current{};GetWindowRect(child,&current);MapWindowPoints(HWND_DESKTOP,GetParent(child),reinterpret_cast<POINT*>(&current),2);if(current.left!=x||current.top!=y||current.right-current.left!=w||current.bottom-current.top!=h)SetWindowPos(child,nullptr,x,y,w,h,SWP_NOZORDER|SWP_NOACTIVATE);};
        // Preserve the normal native paint lifecycle and retained pixels.
        // Unchanged inner HWNDs need no move/size messages at all.
        place(host,-juce::roundToInt(horizontal.getCurrentRangeStart()*scale),-juce::roundToInt(vertical.getCurrentRangeStart()*scale),width,height);
        const auto point=getPeer()->getComponent().getLocalPoint(this,area.getPosition());place(clipHost,juce::roundToInt(point.x*scale),juce::roundToInt(point.y*scale),juce::jmax(1,juce::jmin(width,juce::roundToInt(area.getWidth()*scale))),juce::jmax(1,juce::jmin(height,juce::roundToInt(area.getHeight()*scale))));updateNativeVisibility();}
    void timerCallback() override {try{if(owner.app.ara_status(source)=="unbound"){retire();if(window)window->setVisible(false);else setVisible(false);return;}sync();if(host&&native){RECT r{};if(GetClientRect(host,&r)&&r.right>0&&r.bottom>0&&(width!=r.right||height!=r.bottom)){width=r.right;height=r.bottom;layoutNative(false);}visibilityChanged();}}catch(const std::exception&){retire();if(window)window->setVisible(false);else setVisible(false);}}
    Desktop& owner;mrs::Id clip;std::shared_ptr<mrs::IProjectStore> store;std::string source;
    HWND host{},clipHost{};EditorWindow* window{};int width{640},height{420};bool native{},retired{},layingOut{},returnAttached{};juce::Rectangle<int> floatingBounds;juce::Point<int> lastLayoutSize;
    juce::ScrollBar horizontal{false},vertical{true};
    juce::String editorError;
    juce::String lastPreparation;
    juce::TooltipWindow tooltips{this,600};
    static constexpr int toolbar=86;
    Theme chromeTheme;
    juce::TextButton bypass{"Bypass"},detach{"Remove"},pin{"Pin"},dock{"Attach"},full{"Full screen"};juce::Label status,note;std::unique_ptr<NativeEditorSpace> space;
};
}
void performAraOperation(Desktop& owner,const std::function<void()>& operation) {
    const auto cancel=std::make_shared<std::atomic<bool>>(false);PreparationProgress progress(owner,cancel);
    owner.app.set_ara_preparation(cancel,[&]{progress.tick();});
    struct Reset {Desktop& owner;std::shared_ptr<std::atomic<bool>> cancel;~Reset(){owner.app.set_ara_preparation({},{});
        if(cancel->load()){if(const auto pending=owner.app.ara_pending_revision()){owner.araPreparationStore=owner.app.services().projects;owner.araPreparationModel=owner.araPreparationStore->state().revision;owner.araPreparationContent=*pending;owner.araPreparationAttempted=true;}}
    }} reset{owner,cancel};
    operation();
}
void Desktop::publishAraTools(){std::vector<mrs::desktop::Application::AraToolPlugin> plugins;for(const auto& c:araCatalog)if(c.capability.status==mrs::ara::CapabilityStatus::available)plugins.push_back({c.module,c.capability.class_id,c.capability.name,c.capability.version});app.set_ai_ara_catalog(std::move(plugins));}
void Desktop::loadAraCatalog() {
    const juce::File file(juce::String(araCatalogFile.wstring().c_str()));
    if(!file.existsAsFile())return;
    if(file.getSize()>4*1024*1024)throw std::runtime_error("ARA catalog exceeds size limit; rescan plugins");
    const auto data=juce::JSON::parse(file.loadFileAsString());const auto* entries=data["plugins"].getArray();
    if(static_cast<int>(data["schema"])!=1||!entries||entries->size()>1024)throw std::runtime_error("Invalid ARA catalog; rescan plugins");
    std::vector<AraCandidate> loaded;
    for(const auto& entry:*entries){
        auto text=[&](const char* key){const auto value=entry[key];if(!value.isString()||value.toString().length()>32768)throw std::runtime_error("Invalid ARA catalog entry; rescan plugins");return value.toString().toStdString();};
        AraCandidate item;item.module=text("module");auto& c=item.capability;
        c.class_id=text("class");c.name=text("name");c.factory_id=text("factory");c.version=text("version");c.archive_id=text("archive");
        c.api_generation=static_cast<int>(entry["generation"]);c.status=mrs::ara::CapabilityStatus::available;
        if(item.module.empty()||c.class_id.empty()||c.name.empty())throw std::runtime_error("Incomplete ARA catalog; rescan plugins");
        loaded.push_back(std::move(item));
    }
    araCatalog=std::move(loaded);araCatalogScanned=true;publishAraTools();
}
void Desktop::saveAraCatalog() {
    if(araCatalogFile.empty())return;
    // Machine-local discovery metadata, never plugin/project state or a module
    // load. Publication is atomic and happens only after a completed scan.
    juce::Array<juce::var> entries;
    for(const auto& item:araCatalog){const auto& c=item.capability;if(c.status!=mrs::ara::CapabilityStatus::available)continue;
        auto row=std::make_unique<juce::DynamicObject>();
        for(const auto& [key,value]:{std::pair{"module",item.module},{"class",c.class_id},{"name",c.name},{"factory",c.factory_id},{"version",c.version},{"archive",c.archive_id}})row->setProperty(key,juce::String::fromUTF8(value.c_str()));
        row->setProperty("generation",c.api_generation);entries.add(juce::var(row.release()));
    }
    if(entries.size()>1024)throw std::runtime_error("ARA catalog exceeds entry limit");
    auto root=std::make_unique<juce::DynamicObject>();root->setProperty("schema",1);root->setProperty("plugins",entries);
    const auto bytes=juce::JSON::toString(juce::var(root.release()));
    if(bytes.getNumBytesAsUTF8()>4*1024*1024)throw std::runtime_error("ARA catalog exceeds size limit");
    publishSettings(juce::File(juce::String(araCatalogFile.wstring().c_str())),bytes);
}
juce::PopupMenu Desktop::araMenu(mrs::Id id) const {
    juce::PopupMenu popupMenu;const auto p=project();const auto& clip=audioClip(*p,id);const auto state=app.ara_status(clip.source);
    const bool stopped=!app.recording()&&app.engine()->state().playback!=mrs::PlaybackState::playing;
    popupMenu.addSectionHeader("ARA: "+displayStatus(state));
    const bool unavailable=state.starts_with("unavailable")||state.starts_with("failed");
    const bool bound=state!="unbound";
    popupMenu.addItem(301,"Open editor",bound&&!unavailable);popupMenu.addItem(302,"Update playback",bound&&stopped);popupMenu.addItem(303,app.ara_bypassed(clip.source)?"Enable":"Bypass",bound&&stopped);popupMenu.addItem(304,"Remove from source (all copies)",bound&&stopped);popupMenu.addItem(306,"Retry saved ARA binding",stopped&&unavailable);popupMenu.addItem(307,"Locate source WAV...",bound&&stopped);
    popupMenu.addSeparator();popupMenu.addItem(300,araScanner.valid()?"Cancel ARA discovery":araCatalogScanned?"Rescan ARA plugins...":"Discover ARA plugins...",true);popupMenu.addItem(305,"Choose ARA plugin file...",!araScanner.valid());
    popupMenu.addSectionHeader("ARA state is saved with the project");return popupMenu;
}
juce::PopupMenu Desktop::araPluginsMenu(mrs::Id id) const {
    juce::PopupMenu popupMenu;const auto p=project();const auto& clip=audioClip(*p,id);const auto state=app.ara_status(clip.source);
    const bool stopped=!app.recording()&&app.engine()->state().playback!=mrs::PlaybackState::playing;
    const bool selectable=state=="unbound"||state.starts_with("unavailable")||state.starts_with("failed");
    const auto binding=std::find_if(p->ara_bindings.begin(),p->ara_bindings.end(),[&](const auto& b){return b.source==clip.source;});
    for(std::size_t i=0;i<araCatalog.size();++i){const auto& c=araCatalog[i];if(c.capability.status!=mrs::ara::CapabilityStatus::available)continue;
        const bool exists=juce::File(c.module).exists();const bool selected=binding!=p->ara_bindings.end()&&binding->module==c.module&&binding->class_id==c.capability.class_id;
        popupMenu.addItem(1000+static_cast<int>(i),juce::String(c.capability.name)+(exists?"":" (missing — rescan)"),stopped&&selectable&&exists,selected);
    }return popupMenu;
}
void Desktop::addAraMenus(juce::PopupMenu& popupMenu,mrs::Id id) const {
    popupMenu.addSubMenu("ARA editor",araMenu(id));const auto plugins=araPluginsMenu(id);popupMenu.addSubMenu("ARA 2",plugins,plugins.getNumItems()>0);popupMenu.addSeparator();
}
void Desktop::araChoice(mrs::Id id,int choice) {
    const auto p=project();const auto clip=audioClip(*p,id);
    if(choice==300){if(araScanner.valid())*araScanCancel=true;else scanAra(id);return;}
    if(choice==305){const auto expected=app.services().projects;choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::canSelectDirectories,[this,id,expected](const auto& file){if(app.services().projects==expected)scanAra(id,file);},"*.vst3");return;}
    if(choice==301){openAra(id);return;}
    if(choice==302){app.refresh_ara();return;}
    if(choice==303){app.bypass_ara_source(clip.source,!app.ara_bypassed(clip.source));return;}
    if(choice==304){closeAra(clip.source);app.detach_ara_source(clip.source);return;}
    if(choice==306){closeAra(clip.source);app.retry_ara_source(clip.source);return;}
    if(choice==307){const auto expected=app.services().projects;choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this,id,expected](const auto& file){if(app.services().projects==expected)run([&]{const auto source=audioClip(*project(),id).source;closeAra(source);app.relink_ara_media(source,fsPath(file));});},"*.wav");return;}
    if(choice>=1000){const auto state=app.ara_status(clip.source);if(state!="unbound"&&!state.starts_with("unavailable")&&!state.starts_with("failed"))throw std::runtime_error("ARA source is already connected; detach before choosing another plugin");const auto candidate=araCatalog.at(static_cast<std::size_t>(choice-1000));if(candidate.capability.status!=mrs::ara::CapabilityStatus::available)throw std::runtime_error("Incompatible ARA plugin");if(state=="unbound")app.attach_ara_source(clip.source,candidate.module,candidate.capability.class_id);else{closeAra(clip.source);app.relink_ara_source(clip.source,candidate.module,candidate.capability.class_id);}openAra(id);}
}
juce::var Desktop::beginAraPluginDrag(mrs::processing::VstPlugin plugin,bool araOnly,std::size_t index) {
    AraPluginDrag drag;drag.store=app.services().projects;drag.revision=drag.store->state().revision;drag.plugin=std::move(plugin);drag.generation=mrs::new_id().value;drag.araOnly=araOnly;
    drag.description=araOnly?"ara2:"+juce::String(drag.generation):"vst3:"+juce::String(static_cast<juce::int64>(index));araPluginDrag=drag;return drag.description;
}
bool Desktop::isAraPluginDrag(const juce::var& description) const {
    return araPluginDrag&&araPluginDrag->description==description.toString()&&araPluginDrag->store==app.services().projects&&araPluginDrag->revision==app.services().projects->state().revision;
}
bool Desktop::araDropCurrent(const AraDrop& drop) const {
    if(!isAraPluginDrag(drop.drag.description)||araPluginDrag->generation!=drop.drag.generation)return false;
    const auto p=project();return std::any_of(p->clips.begin(),p->clips.end(),[&](const auto& c){return c==drop.clip&&!c.midi;});
}
void Desktop::dropAraPlugin(mrs::Id id,const juce::var& description) {
    if(!isAraPluginDrag(description))return;AraDrop drop{*araPluginDrag,audioClip(*project(),id)};
    juce::Component::SafePointer<Desktop> safe(this);juce::MessageManager::callAsync([safe,drop]{if(safe)safe->run([&]{safe->applyAraDrop(drop);});});
}
void Desktop::applyAraDrop(AraDrop drop) {
    if(!araDropCurrent(drop))throw std::runtime_error("Project changed; repeat the ARA drop");
    if(app.recording()||app.engine()->state().playback==mrs::PlaybackState::playing)throw std::runtime_error("Stop playback before adding ARA");
    const auto p=project();const auto& plugin=drop.drag.plugin;
    const auto binding=std::find_if(p->ara_bindings.begin(),p->ara_bindings.end(),[&](const auto& b){return b.source==drop.clip.source;});
    if(binding!=p->ara_bindings.end()){
        if(binding->module==plugin.path&&binding->class_id==plugin.class_id){openAra(drop.clip.id);return;}
        throw std::runtime_error("Remove the existing ARA binding before adding another plugin");
    }
    if(!juce::File(plugin.path).exists())throw std::runtime_error("ARA plugin file is missing; rescan plugins");
    const auto candidate=std::find_if(araCatalog.begin(),araCatalog.end(),[&](const auto& c){return c.module==plugin.path&&c.capability.class_id==plugin.class_id;});
    if(candidate==araCatalog.end()){scanAra(drop.clip.id,juce::File(plugin.path));araDropPending=drop;return;}
    if(candidate->capability.status!=mrs::ara::CapabilityStatus::available)throw std::runtime_error("This VST3 plugin does not support ARA 2");
    app.attach_ara_source(drop.clip.source,plugin.path,plugin.class_id);openAra(drop.clip.id);refresh(true);
}
void Desktop::scanAra(mrs::Id clip,std::optional<juce::File> file) {
    if(araScanner.valid())throw std::runtime_error("ARA discovery already running");
    araDropPending.reset();
    std::vector<std::string> modules;for(const auto& p:catalog)modules.push_back(p.path);
    if(file)modules={file->getFullPathName().toStdString()};
    const auto root=juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory).getChildFile("Common Files/VST3").getFullPathName().toStdString();
    const auto helper=fsPath(juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("mrs_ara_scan.exe"));
    araScanCancel=std::make_shared<std::atomic<bool>>(false);const auto cancel=araScanCancel;araScanClip=clip;araScanStore=app.services().projects;araScanSingle=file.has_value();
    message="Discovering ARA plugins... (reopen menu to cancel)";
    araScanner=std::async(std::launch::async,[modules=std::move(modules),root,helper,cancel,search=!file]() mutable {
        if(search){std::error_code ec;for(std::filesystem::recursive_directory_iterator i(root,std::filesystem::directory_options::skip_permission_denied,ec),end;i!=end&&!ec&&!cancel->load();i.increment(ec)){
            if(i->path().extension()==".vst3"){modules.push_back(i->path().string());if(i->is_directory(ec))i.disable_recursion_pending();if(modules.size()>=256)break;}}}
        std::sort(modules.begin(),modules.end());modules.erase(std::unique(modules.begin(),modules.end()),modules.end());
        std::vector<AraCandidate> result;for(const auto& module:modules){if(cancel->load())break;try{for(auto c:mrs::ara::probe_isolated(module,helper,10000,cancel))result.push_back({module,std::move(c)});}catch(const std::exception& e){mrs::ara::Capability c;c.diagnostic=std::filesystem::exists(module)?std::string("failed scan: ")+e.what():"missing plugin";result.push_back({module,std::move(c)});}}
        return result;
    });
}
void Desktop::pollAra() {
    performAraOperation(*this,[&]{app.poll_ara();});
    prepareAraWhenIdle(juce::Time::getMillisecondCounterHiRes(),juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown()||juce::Component::getCurrentlyModalComponent()!=nullptr);
    if(araScanner.valid()&&araScanner.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto found=araScanner.get();auto pendingDrop=std::move(araDropPending);araDropPending.reset();
        if(araScanCancel->load()){message="ARA discovery cancelled";return;}
        if(araScanSingle){for(const auto& c:found)std::erase_if(araCatalog,[&](const auto& old){return old.module==c.module;});araCatalog.insert(araCatalog.end(),std::make_move_iterator(found.begin()),std::make_move_iterator(found.end()));}
        else araCatalog=std::move(found);
        araCatalogScanned=true;publishAraTools();
        const auto available=std::count_if(araCatalog.begin(),araCatalog.end(),[](const auto& c){return c.capability.status==mrs::ara::CapabilityStatus::available;});message=available==0?"No compatible ARA plugins found. Ordinary VST3 effects remain available.":"ARA discovery complete. Choose a plugin in ARA 2.";if(available==0)for(const auto& c:araCatalog)if(!c.capability.diagnostic.empty()){message+=" "+juce::String(c.capability.diagnostic);break;}
        try{saveAraCatalog();}catch(const std::exception& e){message+=" "+juce::String(e.what());}
        if(browser)browser->rebuild();
        if(pendingDrop){run([&]{
            if(!araDropCurrent(*pendingDrop))throw std::runtime_error("Project changed; repeat the ARA drop");
            const auto& plugin=pendingDrop->drag.plugin;
            if(std::none_of(araCatalog.begin(),araCatalog.end(),[&](const auto& c){return c.module==plugin.path&&c.capability.class_id==plugin.class_id&&c.capability.status==mrs::ara::CapabilityStatus::available;}))throw std::runtime_error("This VST3 plugin does not provide a compatible ARA 2 class");
            applyAraDrop(*pendingDrop);
        });return;}
        if(araScanStore==app.services().projects&&araScanClip){const auto p=project();if(std::any_of(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==*araScanClip;})){juce::PopupMenu popupMenu;addAraMenus(popupMenu,*araScanClip);popupMenu.setLookAndFeel(&theme);juce::Component::SafePointer<Desktop> safe(this);const auto id=*araScanClip;const auto expected=araScanStore;popupMenu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe,id,expected](int n){if(safe&&n&&safe->app.services().projects==expected)safe->run([&]{safe->araChoice(id,n);});});}}}
}
void Desktop::prepareAraWhenIdle(double now,bool interacting) {
    const auto pending=app.ara_pending_revision();
    if(!pending||app.recording()||app.engine()->state().playback==mrs::PlaybackState::playing){araPreparationStore.reset();return;}
    const auto store=app.services().projects;const auto modelRevision=store->state().revision;
    if(araPreparationStore!=store||araPreparationModel!=modelRevision||araPreparationContent!=*pending){araPreparationStore=store;araPreparationModel=modelRevision;araPreparationContent=*pending;araEditTime=now;araPreparationAttempted=false;}
    if(interacting){araEditTime=now;return;}
    if(araPreparationAttempted||now-araEditTime<600)return;
    // At most one automatic attempt for this exact content/project revision.
    // Cancel/failure waits for a new edit or explicit Update/Play, never a timer loop.
    araPreparationAttempted=true;
    performAraOperation(*this,[&]{app.refresh_ara(false);});
}
void Desktop::closeAra(const std::string& source){const auto document=juce::String(app.ara_document_key(source));if(dockedEditor&&dockedEditor->getProperties()["mrs-ara-document"].toString()==document){retireAraComponent(*dockedEditor);dockedEditor->setVisible(false);dockedClip.reset();setWorkspace(mrs::desktop::Workspace::arrange);}for(auto& w:windows)if(w->getProperties()["mrs-ara-source"].toString()==juce::String(source)||w->getProperties()["mrs-ara-document"].toString()==document)w->closeButtonPressed();}
void Desktop::attachAra(juce::Component* component,bool attach){
    auto* araContent=dynamic_cast<AraPanel*>(component);if(!araContent||araContent->retired)return;araContent->validate();araEditorAttached=attach;
    if(attach){
        if(dockedEditor.get()==araContent)return;
        auto* previousWindow=araContent->window;if(!previousWindow)return;
        araContent->floatingBounds=previousWindow->getBounds();araContent->space.reset();previousWindow->onClose={};previousWindow->setContentNonOwned(araContent,false);previousWindow->clearContentComponent();
        if(dockedEditor)retireAraComponent(*dockedEditor);
        dockedEditor.reset(araContent);dockedClip=araContent->clip;araContent->getProperties().set("mrs-attached",true);addAndMakeVisible(araContent);araContent->rehost(nullptr);if(getPeer())SetActiveWindow(GetAncestor(static_cast<HWND>(getPeer()->getNativeHandle()),GA_ROOT));previousWindow->setVisible(false);setWorkspace(mrs::desktop::Workspace::edit);
    }else{
        if(dockedEditor.get()!=araContent)return;
        araContent->space.reset();dockedEditor.release();dockedClip.reset();removeChildComponent(araContent);araContent->getProperties().set("mrs-attached",false);
        auto w=std::make_unique<EditorWindow>("ARA / "+juce::String::fromUTF8(audioClip(*project(),araContent->clip).name.c_str()),araContent,false);w->setLookAndFeel(&theme);w->setResizable(true,false);w->setResizeLimits(640,260,8192,8192);w->setOwner(*this);araContent->rehost(w.get());
        juce::Component::SafePointer<Desktop> safe(this);juce::Component::SafePointer<AraPanel> content(araContent);auto* closing=w.get();w->onClose=[safe,content,closing]{if(safe&&safe->getPeer()&&closing->getPeer()){const auto handle=static_cast<HWND>(closing->getPeer()->getNativeHandle());if(GetActiveWindow()==handle||GetForegroundWindow()==handle){SetActiveWindow(GetAncestor(static_cast<HWND>(safe->getPeer()->getNativeHandle()),GA_ROOT));safe->grabKeyboardFocus();}}if(content)content->retire();};
        if(!araContent->floatingBounds.isEmpty())w->setBounds(araContent->floatingBounds);else araContent->fit();w->setVisible(true);w->toFront(true);windows.push_back(std::move(w));setWorkspace(mrs::desktop::Workspace::arrange);
    }
    araContent->resized();araContent->grabKeyboardFocus();
}
void Desktop::followAra(mrs::Id id) {
    // Defer native editor teardown until the Arrangement mouse callback returns.
    // Recheck selection, project lifetime and Pin when the request is delivered.
    const auto expected=app.services().projects;
    juce::Component::SafePointer<Desktop> safe(this);
    juce::MessageManager::callAsync([safe,id,expected]{
        if(!safe||safe->app.services().projects!=expected||safe->selectedClip!=id)return;
        safe->run([&]{
            const auto p=safe->project();
            const auto clip=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});
            if(clip==p->clips.end()||clip->midi||std::none_of(p->ara_bindings.begin(),p->ara_bindings.end(),[&](const auto& b){return b.source==clip->source;}))return;
            const auto document=juce::String(safe->app.ara_document_key(clip->source));
            bool following=false;
            if(auto* docked=dynamic_cast<AraPanel*>(safe->dockedEditor.get());docked&&docked->isVisible()&&!docked->retired){const bool pinned=static_cast<bool>(docked->getProperties()["mrs-pinned"]);if(pinned&&docked->getProperties()["mrs-ara-document"].toString()==document)return;following|=!pinned;}
            for(const auto& w:safe->windows)if(w->isVisible()&&dynamic_cast<AraPanel*>(w->getContentComponent())){
                const bool pinned=static_cast<bool>(w->getProperties()["mrs-pinned"]);
                if(pinned&&w->getProperties()["mrs-ara-document"].toString()==document)return;
                following|=!pinned;
            }
            if(!following)return; // Selection alone never opens the first editor.
            const bool arrangementFocus=safe->arrangement->hasKeyboardFocus(true);
            // Open first: a failed target must not retire the current editor.
            safe->openAra(id,false);
            for(auto& w:safe->windows)if(w->isVisible()&&dynamic_cast<AraPanel*>(w->getContentComponent())&&!static_cast<bool>(w->getProperties()["mrs-pinned"])&&w->getProperties()["mrs-ara-document"].toString()!=document)w->closeButtonPressed();
            // Closing a native child may restore focus to the workspace root.
            // Keep the Arrangement gesture/Space owner selected by the click.
            if(arrangementFocus)safe->arrangement->grabKeyboardFocus();
        });
    });
}
void Desktop::openAra(mrs::Id id,bool activate) {
    const auto p=project();const auto source=audioClip(*p,id).source;
    app.select_ara_clip(source,id);
    if(auto* araContent=dynamic_cast<AraPanel*>(dockedEditor.get());araContent&&!araContent->retired&&araContent->getProperties()["mrs-ara-document"].toString()==juce::String(app.ara_document_key(source))){araContent->select(id);setWorkspace(mrs::desktop::Workspace::edit);if(activate)araContent->grabKeyboardFocus();return;}
    for(auto& w:windows)if(w->isVisible()&&w->getProperties()["mrs-ara-document"].toString()==juce::String(app.ara_document_key(source))){if(auto* content=dynamic_cast<AraPanel*>(w->getContentComponent()))content->select(id);w->setName("ARA / "+juce::String::fromUTF8(audioClip(*p,id).name.c_str()));w->toFront(activate);return;}
    auto content=std::make_unique<AraPanel>(*this,id);auto* contentPanel=content.get();auto w=std::make_unique<EditorWindow>("ARA / "+juce::String::fromUTF8(audioClip(*p,id).name.c_str()),content.release(),false);w->setLookAndFeel(&theme);w->setResizable(true,false);w->setResizeLimits(640,260,8192,8192);w->setOwner(*this);contentPanel->attach(*w);
    juce::Component::SafePointer<Desktop> safe(this);auto* closing=w.get();w->onClose=[safe,closing,contentPanel]{
        // Give MR an eligible active HWND before the vendor destroys its focused
        // child. Otherwise Windows may activate an unrelated application first.
        if(safe&&safe->getPeer()&&closing->getPeer()){const auto editorHandle=static_cast<HWND>(closing->getPeer()->getNativeHandle());
            if(GetActiveWindow()==editorHandle||GetForegroundWindow()==editorHandle){const auto ownerHandle=GetAncestor(static_cast<HWND>(safe->getPeer()->getNativeHandle()),GA_ROOT);SetActiveWindow(ownerHandle);safe->grabKeyboardFocus();}
        }
        contentPanel->retire();
    };
    const auto handle=static_cast<HWND>(w->getPeer()->getNativeHandle());
    const auto style=GetWindowLongPtrW(handle,GWL_EXSTYLE);
    if(!activate)SetWindowLongPtrW(handle,GWL_EXSTYLE,style|WS_EX_NOACTIVATE);
    w->setVisible(true);w->toFront(activate);contentPanel->resized();
    if(!activate)SetWindowLongPtrW(handle,GWL_EXSTYLE,style);
    windows.push_back(std::move(w));
    if(araEditorAttached&&(!dockedEditor||!dockedClip||!static_cast<bool>(dockedEditor->getProperties()["mrs-pinned"]))){attachAra(contentPanel,true);if(!activate)arrangement->grabKeyboardFocus();}
}
void retireAraWindow(EditorWindow& w){if(auto* p=dynamic_cast<AraPanel*>(w.getContentComponent()))p->retire();}
void retireAraComponent(juce::Component& component){if(auto* p=dynamic_cast<AraPanel*>(&component))p->retire();}
void araEditorSmoke(const juce::File& fixture) {
    const auto check=[](bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);};
    auto pump=[](){for(int pass=0;pass<20;++pass){MSG msg{};int count{};while(count++<200&&PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(1);}};
    struct Device final:mrs::audio::IAudioDevice {
        std::vector<mrs::audio::DeviceInfo> enumerate() override{return {{0,"ARA GUI fixture",{},{"L","R"},16,8192,128,1}};}
        void control_panel(int)override{}void open(const mrs::audio::DeviceConfig&,std::shared_ptr<mrs::audio::AudioEngine>)override{}
        void start()override{}void stop()override{}void close()noexcept override{}mrs::audio::DeviceStatus status()override{return {mrs::audio::DevicePhase::running,48000};}
    };
    const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mrs-ara179-",".wav");
    struct Cleanup {juce::File file;~Cleanup(){file.deleteFile();}} cleanup{file};
    {std::ofstream f(fsPath(file),std::ios::binary);auto u16=[&](unsigned v){f.put(static_cast<char>(v&255));f.put(static_cast<char>((v>>8)&255));};auto u32=[&](unsigned v){u16(v&65535);u16(v>>16);};f.write("RIFF",4);u32(36+8192);f.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(48000);u32(96000);u16(2);u16(16);f.write("data",4);u32(8192);for(int i=0;i<4096;++i)u16(8192);}
    juce::MemoryBlock original;check(file.loadFileAsData(original),"ARA fixture bytes");
    Desktop d(true);d.app.new_project();d.app.import_wavs({fsPath(file)});d.app.connect(std::make_unique<Device>(),{0,48000,128,{},{0,1}});d.setSize(1200,800);d.setVisible(true);d.addToDesktop(juce::ComponentPeer::windowIsTemporary);d.refresh(true);
    const auto clip=d.project()->clips.front();d.activateClip(clip.id);auto model=*d.project();auto revision=d.app.services().projects->state().revision;
    auto doubleClip=[&](Desktop& desktop,mrs::Id id,int modifiers=juce::ModifierKeys::leftButtonModifier){
        const auto p=desktop.project();const auto at=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});check(at!=p->clips.end(),"ARA double-click fixture clip");
        const auto point=desktop.arrangement->clipRect(*at).getCentre();const auto now=juce::Time::getCurrentTime();
        const juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(modifiers),1,0,0,0,0,desktop.arrangement.get(),desktop.arrangement.get(),now,point,now,2,false);
        desktop.arrangement->mouseDoubleClick(event);
    };
    auto clickClip=[&](Desktop& desktop,mrs::Id id,int modifiers=juce::ModifierKeys::leftButtonModifier){
        const auto p=desktop.project();const auto at=std::find_if(p->clips.begin(),p->clips.end(),[&](const auto& c){return c.id==id;});check(at!=p->clips.end(),"ARA selection fixture clip");
        const auto point=desktop.arrangement->clipRect(*at).getCentre();const auto now=juce::Time::getCurrentTime();
        const juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(modifiers),1,0,0,0,0,desktop.arrangement.get(),desktop.arrangement.get(),now,point,now,1,false);
        desktop.arrangement->mouseDown(event);desktop.arrangement->mouseUp(event);
    };
    doubleClip(d,clip.id);check(d.clipEditorOpen(),"unbound audio double click retains ordinary audio editor");d.closeEditors();d.windows.clear();
    {Desktop midi(true);midi.app.new_project();const auto track=midi.app.add_instrument_track("MIDI fixture");const auto noteClip=midi.app.create_midi_clip(track,0);midi.setSize(1200,800);midi.setVisible(true);midi.addToDesktop(juce::ComponentPeer::windowIsTemporary);midi.refresh(true);
        doubleClip(midi,noteClip);check(midi.clipEditorOpen()&&midi.selectedClip==noteClip,"MIDI double click retains piano roll");midi.closeEditors();midi.removeFromDesktop();}
    std::array<float,256> output{},dry{};auto stop=[&]{d.app.stop();d.app.engine()->process(nullptr,output.data(),128);d.app.poll();};
    d.app.play();d.app.engine()->process(nullptr,dry.data(),128);stop();
    const auto caps=mrs::ara::probe_isolated(fixture.getFullPathName().toStdString(),fsPath(juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("mrs_ara_scan.exe")));
    check(!caps.empty()&&caps.front().status==mrs::ara::CapabilityStatus::available,"ARA isolated discovery");
    const auto catalogFile=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mrs-ara-catalog-",".json");Cleanup catalogCleanup{catalogFile};
    d.araCatalogFile=fsPath(catalogFile);d.araCatalog={{fixture.getFullPathName().toStdString(),caps.front()}};d.saveAraCatalog();
    {Desktop restarted(true);restarted.araCatalogFile=fsPath(catalogFile);restarted.loadAraCatalog();
        check(restarted.araCatalogScanned&&restarted.araCatalog.size()==1&&!restarted.araScanner.valid()&&restarted.araCatalog.front().capability.class_id==caps.front().class_id,"ARA catalog restart restores discovery without a scan");
        const auto saved=catalogFile.loadFileAsString();check(catalogFile.replaceWithText("{broken"),"ARA corrupt catalog fixture");bool rejected=false;try{restarted.loadAraCatalog();}catch(const std::exception&){rejected=true;}check(rejected&&restarted.araCatalog.size()==1,"ARA corrupt cache preserves current catalog");check(catalogFile.replaceWithText(saved),"restore ARA catalog fixture");
    }
    auto missing=d.araCatalog.front();missing.module=catalogFile.getSiblingFile("missing-plugin.vst3").getFullPathName().toStdString();missing.capability.name="Missing fixture";d.araCatalog.push_back(missing);
    auto missingMenu=d.araPluginsMenu(clip.id);bool missingDisabled=false;for(juce::PopupMenu::MenuItemIterator it(missingMenu);it.next();)if(it.getItem().itemID==1001)missingDisabled=!it.getItem().isEnabled;
    check(missingDisabled,"ARA missing cached plugin is visible but disabled");
    juce::PopupMenu separated;d.addAraMenus(separated,clip.id);juce::PopupMenu::MenuItemIterator sections(separated);
    check(sections.next()&&sections.getItem().text=="ARA editor"&&sections.getItem().subMenu,"ARA commands are the first submenu");
    bool hasOpen=false,hasDiscovery=false;for(juce::PopupMenu::MenuItemIterator it(*sections.getItem().subMenu,true);it.next();){const auto& item=it.getItem();check(item.itemID<1000,"ARA command menu contains no plugin entries");if(item.itemID==301)hasOpen=!item.isEnabled;if(item.itemID==300)hasDiscovery=item.isEnabled;}
    check(hasOpen&&hasDiscovery,"unbound ARA commands retain disabled Open and enabled discovery");
    check(sections.next()&&sections.getItem().text=="ARA 2"&&sections.getItem().subMenu,"ARA plugin submenu follows commands");
    for(juce::PopupMenu::MenuItemIterator it(*sections.getItem().subMenu,true);it.next();)check(it.getItem().itemID>=1000&&!it.getItem().isSectionHeader&&!it.getItem().isSeparator,"ARA 2 contains only plugins");
    d.scanAra(clip.id,fixture);d.araScanClip.reset();for(int wait=0;wait<2000&&d.araScanner.valid();++wait){d.pollAra();Sleep(2);}check(!d.araScanner.valid()&&d.araCatalog.size()==2,"ARA single-file discovery merges catalog");
    const auto savedCatalog=catalogFile.loadFileAsString();d.scanAra(clip.id,fixture);d.araScanClip.reset();d.araScanCancel->store(true);for(int wait=0;wait<2000&&d.araScanner.valid();++wait){d.pollAra();Sleep(2);}check(!d.araScanner.valid()&&d.araCatalog.size()==2&&catalogFile.loadFileAsString()==savedCatalog,"ARA cancelled discovery preserves persistent catalog");
    // The missing entry sorts first after the single-module merge.
    d.araCatalog={{fixture.getFullPathName().toStdString(),caps.front()}};
    const mrs::processing::VstPlugin dragPlugin{fixture.getFullPathName().toStdString(),caps.front().class_id,caps.front().name,"Fixture",caps.front().version,false};
    {Desktop guarded(true);guarded.app.new_project();guarded.app.import_wavs({fsPath(file)});const auto originalClip=guarded.project()->clips.front();guarded.app.duplicate_clip(originalClip.id);guarded.setSize(1200,800);guarded.addToDesktop(juce::ComponentPeer::windowIsTemporary);guarded.setVisible(true);guarded.refresh(true);guarded.araCatalog={{dragPlugin.path,caps.front()}};
        guarded.beginAraPluginDrag(dragPlugin,true,0);auto pending=Desktop::AraDrop{*guarded.araPluginDrag,originalClip};guarded.applyAraDrop(pending);pump();
        check(!juce::Component::getCurrentlyModalComponent()&&guarded.project()->ara_bindings.size()==1,"shared source drop applies immediately without confirmation");guarded.closeEditors();guarded.app.undo();check(guarded.project()->ara_bindings.empty(),"immediate ARA drop remains a host Undo command");
        auto unsupported=caps.front();unsupported.status=mrs::ara::CapabilityStatus::unsupported;guarded.araCatalog={{dragPlugin.path,unsupported}};guarded.beginAraPluginDrag(dragPlugin,true,0);bool rejected=false;try{guarded.applyAraDrop({*guarded.araPluginDrag,originalClip});}catch(const std::exception&){rejected=true;}check(rejected&&guarded.project()->ara_bindings.empty(),"unsupported ARA class preserves model");
        auto missingPlugin=dragPlugin;missingPlugin.path+=".missing";guarded.beginAraPluginDrag(missingPlugin,true,0);rejected=false;try{guarded.applyAraDrop({*guarded.araPluginDrag,originalClip});}catch(const std::exception&){rejected=true;}check(rejected&&guarded.project()->ara_bindings.empty(),"missing ARA module preserves model");
        guarded.araCatalog.clear();guarded.beginAraPluginDrag(dragPlugin,true,0);pending={*guarded.araPluginDrag,originalClip};guarded.applyAraDrop(pending);check(guarded.araScanner.valid(),"uncached ARA drop starts isolated discovery");guarded.app.new_project();check(!guarded.araDropCurrent(pending),"switching project invalidates pending ARA operation");
        for(int wait=0;wait<2000&&guarded.araScanner.valid();++wait){guarded.pollAra();Sleep(2);}check(!guarded.araScanner.valid()&&guarded.project()->ara_bindings.empty()&&!guarded.araDropPending,"completed stale discovery cannot attach to another project");
        const auto midiTrack=guarded.app.add_instrument_track("Drop MIDI");const auto midiId=guarded.app.create_midi_clip(midiTrack,0);guarded.refresh(true);const auto midiClip=std::find_if(guarded.project()->clips.begin(),guarded.project()->clips.end(),[&](const auto& c){return c.id==midiId;});
        const auto midiPoint=guarded.arrangement->clipRect(*midiClip).getCentre().toInt();const auto midiBefore=*guarded.project();const auto araDescription=guarded.beginAraPluginDrag(dragPlugin,true,0);
        guarded.arrangement->itemDropped({araDescription,&guarded.browser->tree,midiPoint});guarded.arrangement->itemDropped({araDescription,&guarded.browser->tree,{guarded.arrangement->getWidth()-2,guarded.arrangement->getHeight()-2}});pump();check(*guarded.project()==midiBefore,"ARA-only drop on MIDI or empty space creates no insert or binding");guarded.removeFromDesktop();}
    d.catalog={dragPlugin};d.browser->rebuild();
    auto* araGroup=d.browser->tree.getRootItem()->getSubItem(0);check(araGroup&&araGroup->getUniqueName()=="ARA 2"&&araGroup->getNumSubItems()==1,"ARA browser exposes cached plugins");
    const auto dragDescription=araGroup->getSubItem(0)->getDragSourceDescription();check(dragDescription.toString().startsWith("ara2:")&&d.isAraPluginDrag(dragDescription),"ARA browser captures exact plugin identity");
    auto* vendorGroup=d.browser->tree.getRootItem()->getSubItem(1);check(vendorGroup&&vendorGroup->getSubItem(0)->getDragSourceDescription().toString()=="vst3:0","ordinary VST3 browser retains insert drag format");
    auto dropSnapshot=Desktop::AraDrop{*d.araPluginDrag,clip};
    d.app.play();d.app.engine()->process(nullptr,output.data(),128);bool playingRejected=false;try{d.applyAraDrop(dropSnapshot);}catch(const std::exception&){playingRejected=true;}stop();check(playingRejected&&d.project()->ara_bindings.empty(),"ARA drop during playback leaves model unchanged");
    d.beginAraPluginDrag(dragPlugin,true,0);check(!d.araDropCurrent(dropSnapshot),"new drag invalidates older asynchronous operation");
    dropSnapshot={*d.araPluginDrag,clip};d.app.rename_clips({clip.id},"ARA drop fixture");check(!d.araDropCurrent(dropSnapshot),"project revision invalidates ARA drop");d.app.undo();
    const auto target= d.arrangement->clipRect(clip).getCentre().toInt();
    d.setAssetDesign(true);const auto actualDescription=d.beginAraPluginDrag(dragPlugin,true,0);
    juce::DragAndDropTarget::SourceDetails dropDetails(actualDescription,&d.browser->tree,target);
    check(d.arrangement->isInterestedInDragSource(dropDetails),"Arrangement accepts ARA browser drag");
    d.araCatalog.clear();d.arrangement->itemDragMove(dropDetails);d.arrangement->itemDropped(dropDetails);check(d.project()->ara_bindings.empty(),"ARA drop defers changes until drag callback returns");pump();
    for(int wait=0;wait<2000&&d.araScanner.valid();++wait){d.pollAra();Sleep(2);}
    check(!d.araScanner.valid()&&d.windows.size()==1&&d.project()->ara_bindings.size()==1,"ARA drop probes unknown class then attaches and opens editor");
    d.beginAraPluginDrag(dragPlugin,true,0);const auto beforeRepeat=*d.project();d.applyAraDrop({*d.araPluginDrag,audioClip(*d.project(),clip.id)});check(d.windows.size()==1&&*d.project()==beforeRepeat,"same ARA drop raises editor without replacing notes");
    auto otherPlugin=dragPlugin;otherPlugin.class_id="different-class";d.beginAraPluginDrag(otherPlugin,true,0);bool replacementRejected=false;try{d.applyAraDrop({*d.araPluginDrag,audioClip(*d.project(),clip.id)});}catch(const std::exception&){replacementRejected=true;}check(replacementRejected&&*d.project()==beforeRepeat,"different ARA drop preserves existing binding");
    model=*d.project();revision=d.app.services().projects->state().revision;
    {const auto plugins=d.araPluginsMenu(clip.id);bool selected=false;for(juce::PopupMenu::MenuItemIterator it(plugins);it.next();)if(it.getItem().itemID==1000)selected=it.getItem().isTicked&&!it.getItem().isEnabled;check(selected,"ARA plugin list marks the current binding without replacing it");bool staleRejected=false;try{d.araChoice(clip.id,1000);}catch(const std::exception&){staleRejected=true;}check(staleRejected&&*d.project()==model,"stale plugin choice preserves the connected binding");}
    auto* w=d.windows.back().get();doubleClip(d,clip.id);check(d.windows.size()==1&&d.windows.back().get()==w,"ARA double click reuses editor window");
    const auto sameRevision=d.app.services().projects->state().revision;d.closeAra(clip.source);doubleClip(d,clip.id,juce::ModifierKeys::rightButtonModifier);check(!w->isVisible(),"right double click does not open ARA");d.knifeMode=true;doubleClip(d,clip.id);check(!w->isVisible(),"knife double click does not open ARA");d.knifeMode=false;doubleClip(d,clip.id);check(d.windows.back()->isVisible()&&d.app.services().projects->state().revision==sameRevision,"ARA double click reopens without a host edit");w=d.windows.back().get();
    const auto host=reinterpret_cast<HWND>(static_cast<std::intptr_t>(static_cast<juce::int64>(w->getProperties()["mrs-native-host"])));
    const auto native=GetWindow(host,GW_CHILD);const auto button=GetDlgItem(native,179);check(IsWindow(button),"ARA native fixture button attached");
    auto* content=w->getContentComponent();auto* power=content->findChildWithID("ara-bypass");auto* pin=dynamic_cast<juce::TextButton*>(content->findChildWithID("ara-pin"));
    auto* remove=dynamic_cast<juce::TextButton*>(content->findChildWithID("ara-detach"));check(remove&&remove->getButtonText()=="Remove"&&remove->getTooltip().contains("all its copies"),"ARA Remove describes source removal rather than window detachment");
    check(power&&pin&&power->getProperties()["mrHostIcon"].toString()=="power"&&&power->getLookAndFeel()!=&d.theme,"ARA uses common plugin power/pin chrome");
    pin->setToggleState(true,juce::dontSendNotification);pin->onClick();d.closeUnpinnedEditors();check(w->isVisible()&&static_cast<bool>(w->getProperties()["mrs-pinned"]),"ARA pin keeps editor open in session");
    auto* dock=dynamic_cast<juce::TextButton*>(content->findChildWithID("ara-attach"));auto* full=dynamic_cast<juce::TextButton*>(content->findChildWithID("ara-fullscreen"));
    check(dock&&full&&dock->getRight()<pin->getX()&&dock->getProperties()["mrHostIcon"].toString()=="editor_attach","ARA Attach icon sits next to Pin");
    const auto floating=w->getBounds();w->setSize(650,300);pump();RECT fixedSize{};GetClientRect(host,&fixedSize);
    check(fixedSize.right==640&&fixedSize.bottom==320&&content->findChildWithID("ara-scroll-vertical")->isVisible(),"fixed-size vendor keeps native geometry and scrolls inside resized container");w->setBounds(floating);
    juce::Component::SafePointer<EditorWindow> beforeDock(w);dock->onClick();pump();check(d.dockedEditor.get()==content&&(!beforeDock||!beforeDock->isVisible())&&IsWindow(native)&&GetAncestor(native,GA_ROOT)==GetAncestor(static_cast<HWND>(d.getPeer()->getNativeHandle()),GA_ROOT),"Attach reparents the same native editor into the shared workspace");
    check(dock->getProperties()["mrHostIcon"].toString()=="editor_detach"&&static_cast<bool>(content->getProperties()["mrs-pinned"]),"docked Attach icon becomes Detach and retains Pin");d.closeUnpinnedEditors();check(content->isVisible(),"Pin protects docked ARA editor");
    struct NativePositionProbe {
        HWND hwnd;WNDPROC previous;unsigned changes{},redrawToggles{},visibilityChanges{};
        explicit NativePositionProbe(HWND h):hwnd(h),previous(reinterpret_cast<WNDPROC>(GetWindowLongPtrW(h,GWLP_WNDPROC))){SetPropW(hwnd,L"MRS_ARA_POSITION_PROBE",this);SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(dispatch));}
        ~NativePositionProbe(){SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(previous));RemovePropW(hwnd,L"MRS_ARA_POSITION_PROBE");}
        static LRESULT CALLBACK dispatch(HWND h,UINT message,WPARAM w,LPARAM l){auto* probe=static_cast<NativePositionProbe*>(GetPropW(h,L"MRS_ARA_POSITION_PROBE"));if(message==WM_WINDOWPOSCHANGED)++probe->changes;if(message==WM_SETREDRAW)++probe->redrawToggles;if(message==WM_SHOWWINDOW)++probe->visibilityChanges;return CallWindowProcW(probe->previous,h,message,w,l);}
    };
    const auto dockHeight=d.view.editorHeight;
    {NativePositionProbe viewport(GetParent(host)),inner(host);
        for(int height=500;height<d.getHeight();height+=7){viewport.changes=inner.changes=0;d.resizeMixer(height);check(viewport.changes<=1&&inner.changes==0,"splitter publishes one viewport change and does not move the unchanged fixed-size vendor host");}
        viewport.changes=inner.changes=0;d.resized();check(viewport.changes==0&&inner.changes==0,"unchanged attached layout sends no native geometry messages");
    }
    d.resizeMixer(100000);
    check(d.arrangeArea.getHeight()==0&&content->getY()==d.arrangeArea.getY()+6&&content->getBottom()==d.getHeight()-(d.getWidth()<1500?58:42)&&IsWindow(native),"attached ARA expands to the top panel while preserving footer and native view");
    d.resizeMixer(500);check(d.arrangeArea.getHeight()>0,"attached ARA can shrink back to show arrangement");d.resizeMixer(dockHeight);
    {d.resizeMixer(500);const auto start=d.mixerDivider.getBounds().getCentre();
        const auto dragEvent=[&](juce::Point<int> point){const auto time=juce::Time::getCurrentTime();return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),d.mixerDivider.getLocalPoint(&d,point).toFloat(),juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,&d.mixerDivider,&d.mixerDivider,time,d.mixerDivider.getLocalPoint(&d,start).toFloat(),time,1,true);};
        d.mouseDown(dragEvent(start));NativePositionProbe viewport(GetParent(host));
        for(int step=1;step<=60;++step)d.mouseDrag(dragEvent(start.translated(0,-step)));
        check(viewport.changes==0&&d.view.editorHeight==500,"ARA mouse bursts defer layout until the display frame");d.flushAraDockResize();
        check(viewport.changes==1&&d.view.editorHeight==560&&d.mixerDivider.getBounds().getCentreY()==start.y-60,"ARA frame consumes the latest mouse position without intermediate boundary jumps");
        d.mouseDrag(dragEvent(start.translated(0,-68)));for(int wait=0;wait<20&&d.view.editorHeight!=568;++wait)pump();check(d.view.editorHeight==568,"display VBlank callback applies an ARA drag before mouse release");
        viewport.changes=0;for(int repeat=0;repeat<5;++repeat){d.mouseDrag(dragEvent(start.translated(0,-68)));d.flushAraDockResize();}check(viewport.changes==0,"stationary pointer and synthetic drag events cannot move the boundary");
        d.mouseDrag(dragEvent(start.translated(0,-78)));d.mouseUp(dragEvent(start.translated(0,-78)));check(d.view.editorHeight==578,"mouse release flushes the exact final ARA dock height");
        d.view.editorHeight=100000;d.resized();const auto clamped=d.mixerDivider.getBounds().getCentre();d.mouseDown(dragEvent(clamped));d.mouseDrag(dragEvent(clamped.translated(0,10)));d.mouseUp(dragEvent(clamped.translated(0,10)));
        check(d.mixerDivider.getBounds().getCentreY()==clamped.y+10,"ARA drag starts from the visible height when the saved preference exceeds the current screen");d.resizeMixer(dockHeight);
    }
    for(int wait=0;wait<1000&&d.app.ara_status(clip.source).starts_with("analysing");++wait){d.app.poll_ara();Sleep(1);}d.app.refresh_ara(false);
    auto dockedSpace=[&](HWND focus){SetFocus(focus);PostMessageW(focus,WM_KEYDOWN,VK_SPACE,1);PostMessageW(focus,WM_KEYUP,VK_SPACE,LPARAM{1}<<31);pump();d.app.engine()->process(nullptr,output.data(),128);d.app.poll();};
    dockedSpace(native);check(d.app.engine()->state().playback==mrs::PlaybackState::playing,"Space starts playback from docked native ARA view");dockedSpace(native);check(d.app.engine()->state().playback!=mrs::PlaybackState::playing,"Space stops playback from docked native ARA view");stop();
    content->grabKeyboardFocus();const auto dockedRoot=static_cast<HWND>(d.getPeer()->getNativeHandle());dockedSpace(dockedRoot);check(d.app.engine()->state().playback==mrs::PlaybackState::playing,"Space starts playback with docked host chrome focus");dockedSpace(dockedRoot);check(d.app.engine()->state().playback!=mrs::PlaybackState::playing,"Space stops playback with docked host chrome focus");stop();
    full->onClick();pump();w=d.windows.back().get();check(!d.dockedEditor&&w->isFullScreen()&&IsWindow(native),"full screen floats docked ARA without recreating native view");full->onClick();pump();check(d.dockedEditor.get()==content&&IsWindow(native),"restore from full screen returns to the attached workspace");
    dock->onClick();pump();w=d.windows.back().get();check(!d.dockedEditor&&w->isVisible()&&w->getBounds()==floating&&IsWindow(native)&&*d.project()==model,"Detach restores floating geometry and keeps vendor state/project unchanged");
    w->toFront(true);SetFocus(native);PostMessageW(native,WM_KEYDOWN,VK_F11,1);PostMessageW(native,WM_KEYUP,VK_F11,LPARAM{1}<<31);pump();check(w->isFullScreen(),"native ARA child routes F11 to full screen");full->onClick();pump();check(!w->isFullScreen(),"ARA full screen restores floating window");
    for(const auto scale:{1.,1.5}){auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("ara-workspace-"+juce::String(juce::roundToInt(scale*100))+".png").createOutputStream();check(stream&&juce::PNGImageFormat().writeImageToStream(content->createComponentSnapshot(content->getLocalBounds(),true,static_cast<float>(scale),juce::SoftwareImageType{}),*stream),"ARA workspace chrome preview");}
    for(int wait=0;wait<1000&&d.app.ara_status(clip.source).starts_with("analysing");++wait){d.app.poll_ara();Sleep(5);}
    check(!d.app.ara_status(clip.source).starts_with("analysing"),"ARA requested analysis completes");
    struct AnalysisCheck {~AnalysisCheck(){SetEnvironmentVariableW(L"MRS_ARA_TEST_REQUIRE_ANALYSIS",nullptr);}} analysisCheck;
    SetEnvironmentVariableW(L"MRS_ARA_TEST_REQUIRE_ANALYSIS",L"1");
    SendMessageW(button,BM_CLICK,0,0);check(d.app.poll_ara()&&d.app.ara_status(clip.source).starts_with("pending"),"ARA native edit invalidates cache");
    const auto beforeIdle=d.app.ara_caches().front().pcm;d.prepareAraWhenIdle(1000,false);d.prepareAraWhenIdle(1599,false);check(d.app.ara_caches().front().pcm==beforeIdle,"ARA idle debounce keeps last prepared audio");
    d.prepareAraWhenIdle(1600,true);d.prepareAraWhenIdle(2199,false);check(d.app.ara_caches().front().pcm==beforeIdle,"ARA gesture delays preparation until interaction finishes");
    d.prepareAraWhenIdle(2200,false);const auto preparedIdle=d.app.ara_caches().front().pcm;check(preparedIdle!=beforeIdle&&!d.app.ara_pending_revision(),"ARA prepares edited audio once after idle");
    dynamic_cast<AraPanel*>(content)->sync();const auto* preparationDetails=dynamic_cast<juce::Label*>(content->findChildWithID("ara-preparation-details"));
    check(preparationDetails&&preparationDetails->isShowing()&&preparationDetails->getText().contains("full=")&&preparationDetails->getText().contains("frames=")&&preparationDetails->getText().contains("ms=")&&preparationDetails->getText().contains("\nUpdate cause:"),"ARA preparation diagnostics are visible without hover");
    d.app.play();d.app.engine()->process(nullptr,output.data(),128);
    check(d.app.ara_caches().front().pcm==preparedIdle,"ARA Play reuses idle-prepared PCM");
    dynamic_cast<AraPanel*>(content)->sync();const auto* playingStatus=dynamic_cast<juce::Label*>(content->findChildWithID("ara-status"));check(!power->isEnabled()&&!d.app.ara_bypassed(clip.source)&&playingStatus&&playingStatus->getText().contains("ARA ON"),"locked ARA toolbar during Play does not imply bypass");
    for(std::size_t i=0;i<output.size();++i)check(std::abs(output[i]-dry[i]*.125f)<.00001f,"ARA edit to transport output exactly once");
    bool rejected=false;try{d.app.bypass_ara_source(clip.source,true);}catch(const std::exception&){rejected=true;}check(rejected,"ARA live structural mutation rejected");stop();
    SendMessageW(button,BM_CLICK,0,0);check(d.app.poll_ara(),"ARA edit pending before cancelled Play");const auto beforeCancelledPlay=d.app.ara_caches().front().pcm;
    const auto playCancel=std::make_shared<std::atomic<bool>>(true);d.app.set_ara_preparation(playCancel,{});bool playCancelled=false;try{d.app.play();}catch(const std::exception&){playCancelled=true;}d.app.set_ara_preparation({},{});d.app.engine()->process(nullptr,output.data(),128);
    check(playCancelled&&d.app.engine()->state().playback!=mrs::PlaybackState::playing&&d.app.ara_caches().front().pcm==beforeCancelledPlay,"cancelled ARA preparation preserves PCM and prevents Play");
    SendMessageW(button,BM_CLICK,0,0);d.app.play();d.app.engine()->process(nullptr,output.data(),128);for(std::size_t i=0;i<output.size();++i)check(std::abs(output[i]-dry[i]*.125f)<.00001f,"explicit Play retries cancelled ARA preparation");stop();
    d.araChoice(clip.id,303);check(d.app.ara_bypassed(clip.source),"ARA bypass status");d.app.play();d.app.engine()->process(nullptr,output.data(),128);for(std::size_t i=0;i<output.size();++i)check(std::abs(output[i]-dry[i])<.00001f,"ARA bypass restores dry");stop();d.araChoice(clip.id,303);
    check(*d.project()==model&&d.app.services().projects->state().revision==revision+2,"ARA bypass commands preserve other model data and create host Undo");
    using ReadTransport=bool(*)(std::int64_t*,std::uint32_t*,double*,double*,std::int32_t*,double*,double*,std::uint64_t*);
    using RequestTransport=void(*)(int,double,double,bool);
    const auto module=GetModuleHandleW(fixture.getFullPathName().toWideCharPointer());
    const auto notifyDocument=reinterpret_cast<void(*)()>(GetProcAddress(module,"MRSAraNotifyDocument"));check(notifyDocument,"ARA document notification hook");
    const auto documentCache=d.app.ara_caches().front().pcm;notifyDocument();check(d.app.poll_ara()&&!d.app.ara_pending_revision(),"ARA private document state is dirty without audio invalidation");d.app.refresh_ara(false);check(d.app.ara_caches().front().pcm==documentCache,"ARA document-only notification reuses PCM");
    SetEnvironmentVariableW(L"MRS_ARA_TEST_SLOW",L"2");const auto metadataCancel=std::make_shared<std::atomic<bool>>(false);d.app.set_ara_preparation(metadataCancel,[&]{notifyDocument();});d.app.refresh_ara();d.app.set_ara_preparation({},{});SetEnvironmentVariableW(L"MRS_ARA_TEST_SLOW",nullptr);check(d.app.ara_status(clip.source).starts_with("ready"),"document notifications during preparation do not reject completed audio");
    const auto notifySignal=reinterpret_cast<void(*)()>(GetProcAddress(module,"MRSAraNotifySignal"));check(notifySignal,"ARA signal notification hook");const auto beforeSignalRace=d.app.ara_caches().front().pcm;
    SetEnvironmentVariableW(L"MRS_ARA_TEST_SLOW",L"2");d.app.set_ara_preparation(metadataCancel,[&]{notifySignal();});d.app.refresh_ara();d.app.set_ara_preparation({},{});SetEnvironmentVariableW(L"MRS_ARA_TEST_SLOW",nullptr);
    check(d.app.ara_status(clip.source).starts_with("pending")&&d.app.ara_caches().front().pcm==beforeSignalRace,"real signal race retains prior PCM and pending status instead of failed dry fallback");d.app.refresh_ara(false);check(d.app.ara_status(clip.source).starts_with("ready"),"stable signal revision retries successfully");
    const auto readTransport=reinterpret_cast<ReadTransport>(GetProcAddress(module,"MRSAraReadTransport"));
    const auto requestTransport=reinterpret_cast<RequestTransport>(GetProcAddress(module,"MRSAraRequestTransport"));
    check(readTransport&&requestTransport,"ARA transport fixture exports");
    const auto audition=reinterpret_cast<void(*)(float)>(GetProcAddress(module,"MRSAraAudition"));
    const auto objectSelection=reinterpret_cast<bool(*)()>(GetProcAddress(module,"MRSAraObjectSelection"));
    const auto renderRequest=reinterpret_cast<void(*)(bool)>(GetProcAddress(module,"MRSAraRenderRequest"));check(renderRequest,"ARA render-thread request hook");
    check(audition&&objectSelection&&objectSelection(),"ARA clip selection sends objects without a time range");
    std::int64_t receivedSample{};std::uint32_t receivedFlags{};double receivedPpq{},receivedTempo{},cycleStart{},cycleEnd{};std::int32_t receivedMeter{};std::uint64_t receivedCalls{};
    auto delivered=[&](mrs::Sample expected,bool playing,bool cycle=false){for(int wait=0;wait<200;++wait){d.app.poll_ara();if(!playing)d.app.engine()->process(nullptr,output.data(),128);if(readTransport(&receivedSample,&receivedFlags,&receivedPpq,&receivedTempo,&receivedMeter,&cycleStart,&cycleEnd,&receivedCalls)&&receivedSample==expected&&static_cast<bool>(receivedFlags&2u)==playing&&static_cast<bool>(receivedFlags&4u)==cycle)return true;Sleep(2);}return false;};
    check(delivered(0,false),"ARA process-block stopped position delivery");
    audition(.125f);d.app.engine()->process(nullptr,output.data(),128);
    // Application's legacy foundation Master graph includes gain 0.5.
    check(std::all_of(output.begin(),output.end(),[](float v){return std::abs(v-.0625f)<.00001f;}),"ARA GUI audition passes through the Master graph exactly once");audition(0.f);
    renderRequest(true);d.app.engine()->process(nullptr,output.data(),128);renderRequest(false);d.app.poll_ara();d.app.engine()->process(nullptr,output.data(),128);check(d.app.engine()->state().playback!=mrs::PlaybackState::playing,"ARA transport request from render thread is ignored before taking a lock");
    SendMessageW(GetDlgItem(native,181),BM_CLICK,0,0);check(d.app.engine()->state().playback!=mrs::PlaybackState::playing,"ARA Play callback is deferred");
    d.app.poll_ara();d.app.engine()->process(nullptr,output.data(),128);check(delivered(0,true),"ARA native Play starts shared transport and editor head");
    for(std::size_t i=0;i<output.size();++i)check(std::abs(output[i]-dry[i]*.125f)<.00001f,"ARA editor adds no duplicate playback voice without audition");
    d.app.engine()->process(nullptr,output.data(),128);check(delivered(128,true),"ARA editor head advances with Engine");
    SendMessageW(native,WM_TIMER,1,0);wchar_t caption[128]{};GetWindowTextW(GetDlgItem(native,180),caption,128);check(juce::String(caption).contains("128")&&juce::String(caption).contains("Playing"),"ARA native view displays shared position");
    SendMessageW(GetDlgItem(native,182),BM_CLICK,0,0);d.app.poll_ara();d.app.engine()->process(nullptr,output.data(),128);check(delivered(256,false),"ARA native Stop pauses shared transport");
    SendMessageW(GetDlgItem(native,183),BM_CLICK,0,0);d.app.poll_ara();d.app.engine()->process(nullptr,output.data(),128);check(delivered(48000,false),"ARA native Seek updates stopped head");
    requestTransport(3,256./48000.,256./48000.,false);requestTransport(4,0,0,true);requestTransport(2,256./48000.,0,false);requestTransport(0,0,0,false);
    d.app.poll_ara();d.app.engine()->process(nullptr,output.data(),128);check(delivered(256,true,true)&&std::abs(cycleStart-256./24000.)<1.e-8&&std::abs(cycleEnd-512./24000.)<1.e-8,"ARA loop range/context mapping");
    d.app.engine()->process(nullptr,output.data(),128);check(delivered(384,true,true),"ARA editor follows loop wrap");
    requestTransport(1,0,0,false);requestTransport(4,0,0,false);d.app.poll_ara();d.app.engine()->process(nullptr,output.data(),128);d.app.seek(0);d.app.engine()->process(nullptr,output.data(),128);check(delivered(0,false),"ARA loop disable and seek");
    d.app.set_tempos(d.project()->time,{{0,90}});d.app.set_meters(d.project()->time,{{1,3,4}});d.app.seek(1000);d.app.engine()->process(nullptr,output.data(),128);
    check(delivered(1000,false)&&receivedTempo==90&&receivedMeter==3&&std::abs(receivedPpq-.03125)<1.e-8,"ARA context tempo/meter/PPQ uses current project");
    check(d.app.undo()&&d.app.undo()&&*d.project()==model,"ARA timing Undo with active context worker");d.app.seek(0);d.app.engine()->process(nullptr,output.data(),128);check(delivered(0,false),"ARA timing Undo restores head");
    d.app.move_clips({clip.id},128);check(d.app.undo()&&*d.project()==model&&w->isVisible(),"ARA host clip Undo retains editor");
    d.refreshInsertEditors();check(w->isVisible()&&IsWindow(native),"ARA editor survives ordinary insert-host refresh");
    w->toFront(true);const auto root=static_cast<HWND>(w->getPeer()->getNativeHandle());SetActiveWindow(root);SetFocus(native);
    SetEnvironmentVariableW(L"MRS_ARA_TEST_SLOW",L"1");
    check(!content->findChildWithID("ara-update"),"ARA update is absent from editor toolbar");const auto updateCommands=d.araMenu(clip.id);bool updateInMenu=false;for(juce::PopupMenu::MenuItemIterator it(updateCommands);it.next();)if(it.getItem().itemID==302)updateInMenu=it.getItem().isEnabled&&it.getItem().text=="Update playback";check(updateInMenu,"ARA update remains in clip command menu");d.run([&]{d.araChoice(clip.id,302);});
    SetEnvironmentVariableW(L"MRS_ARA_TEST_SLOW",nullptr);
    check(IsWindowEnabled(static_cast<HWND>(d.getPeer()->getNativeHandle()))&&IsWindowEnabled(static_cast<HWND>(w->getPeer()->getNativeHandle())),"ARA progress retires and restores owner/editor interaction");
    if(GetFocus()!=native||GetActiveWindow()!=root)throw std::runtime_error("ARA update focus: focus="+std::to_string(reinterpret_cast<std::uintptr_t>(GetFocus()))+" active="+std::to_string(reinterpret_cast<std::uintptr_t>(GetActiveWindow()))+" foreground="+std::to_string(reinterpret_cast<std::uintptr_t>(GetForegroundWindow()))+" root="+std::to_string(reinterpret_cast<std::uintptr_t>(root))+" native="+std::to_string(reinterpret_cast<std::uintptr_t>(native)));
    auto space=[&]{PostMessageW(GetFocus(),WM_KEYDOWN,VK_SPACE,1);PostMessageW(GetFocus(),WM_KEYUP,VK_SPACE,LPARAM{1}<<31);pump();d.app.engine()->process(nullptr,output.data(),128);d.app.poll();};
    space();if(d.app.engine()->state().playback!=mrs::PlaybackState::playing)throw std::runtime_error("ARA Space after update: "+d.message.toStdString()+" focus="+std::to_string(reinterpret_cast<std::uintptr_t>(GetFocus()))+" modal="+std::to_string(juce::Component::getCurrentlyModalComponent()!=nullptr)+" control="+std::to_string(GetKeyState(VK_CONTROL))+" shift="+std::to_string(GetKeyState(VK_SHIFT)));space();check(d.app.engine()->state().playback!=mrs::PlaybackState::playing,"ARA Space stops immediately after update");
    {const auto cancel=std::make_shared<std::atomic<bool>>(false);{PreparationProgress progress(d,cancel);progress.tick();check(!progress.visible()&&GetFocus()==native&&IsWindowEnabled(root),"ARA short preparation shows no window and preserves focus");Sleep(360);progress.tick();check(progress.visible()&&!IsWindowEnabled(root),"ARA long preparation shows delayed progress and guards editor");PostMessageW(GetActiveWindow(),WM_CLOSE,0,0);progress.tick();check(cancel->load(),"delayed ARA progress permits cancellation");}check(GetFocus()==native&&IsWindowEnabled(root),"ARA cancelled progress restores keyboard focus");}
    {const auto cancel=std::make_shared<std::atomic<bool>>(false);try{PreparationProgress progress(d,cancel,std::chrono::milliseconds(0));progress.tick();throw std::runtime_error("preparation failure fixture");}catch(const std::exception&){}check(GetFocus()==native&&IsWindowEnabled(root),"ARA failed preparation restores keyboard focus");}
    w->toFront(true);SetActiveWindow(root);SetFocus(native);d.run([&]{d.araChoice(clip.id,302);});check(GetFocus()==native,"ARA menu update preserves editor keyboard focus");space();check(d.app.engine()->state().playback==mrs::PlaybackState::playing,"ARA menu Space starts without refocus");space();check(d.app.engine()->state().playback!=mrs::PlaybackState::playing,"ARA menu Space stops without refocus");
    for(float scale:{1.f,1.25f,1.5f,2.f}){auto image=w->getContentComponent()->createComponentSnapshot(w->getContentComponent()->getLocalBounds(),true,scale);check(image.isValid(),"ARA software scale snapshot");auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("ara179-"+juce::String(static_cast<int>(scale*100))+".png").createOutputStream();check(stream&&stream->setPosition(0)&&stream->truncate().wasOk()&&juce::PNGImageFormat().writeImageToStream(image,*stream),"ARA snapshot export");}
    const auto preparedBeforeFailure=d.app.ara_caches().front().pcm;
    struct TransportFailure {~TransportFailure(){SetEnvironmentVariableW(L"MRS_ARA_TEST_TRANSPORT_FAIL",nullptr);}} transportFailure;
    SetEnvironmentVariableW(L"MRS_ARA_TEST_TRANSPORT_FAIL",L"1");for(int wait=0;wait<200&&d.app.ara_status(clip.source).find("editor transport unavailable")==std::string::npos;++wait){d.app.poll_ara();d.app.engine()->process(nullptr,output.data(),128);Sleep(2);}
    check(d.app.ara_status(clip.source).find("editor transport unavailable")!=std::string::npos&&d.app.ara_caches().front().pcm==preparedBeforeFailure,"ARA context rejection preserves prepared audio with diagnostic");SetEnvironmentVariableW(L"MRS_ARA_TEST_TRANSPORT_FAIL",nullptr);
    SendMessageW(GetDlgItem(native,181),BM_CLICK,0,0);w->toFront(true);SetActiveWindow(static_cast<HWND>(w->getPeer()->getNativeHandle()));w->closeButtonPressed();check(GetActiveWindow()==d.getPeer()->getNativeHandle(),"ARA close activates MR before retiring focused vendor child");d.app.poll_ara();d.app.engine()->process(nullptr,output.data(),128);check(d.app.engine()->state().playback!=mrs::PlaybackState::playing,"ARA close discards queued playback requests");check(!IsWindow(native),"ARA close removes native child before parent");
    d.openAra(clip.id);check(d.windows.back()->isVisible(),"ARA reopen");std::erase_if(d.windows,[](const auto& item){return !item->isVisible();});
    check(delivered(0,false),"ARA context worker recovers after reopen");
    const auto reopenedHost=reinterpret_cast<HWND>(static_cast<std::intptr_t>(static_cast<juce::int64>(d.windows.back()->getProperties()["mrs-native-host"])));check(IsWindow(GetWindow(reopenedHost,GW_CHILD)),"retired window destructor leaves reopened editor alive");d.closeEditors();d.openAra(clip.id);
    const auto folder=juce::File::getCurrentWorkingDirectory().getChildFile("ara180-"+juce::String(mrs::new_id().value));
    constexpr float savedGain=.5f;
    check(folder.getParentDirectory()==juce::File::getCurrentWorkingDirectory()&&!folder.exists()&&folder.createDirectory().wasOk(),"ARA persistence fixture directory");
    const auto savedFile=folder.getChildFile("Original/Project.mrs");
    const auto liveHost=reinterpret_cast<HWND>(static_cast<std::intptr_t>(static_cast<juce::int64>(d.windows.back()->getProperties()["mrs-native-host"])));const auto liveChild=GetWindow(liveHost,GW_CHILD);
    SendMessageW(GetDlgItem(liveChild,179),BM_CLICK,0,0);check(d.app.poll_ara()&&d.app.dirty(),"ARA pending edit before Update playback marks project dirty");
    d.app.save_project(fsPath(savedFile));check(!d.app.dirty(),"ARA successful save clears dirty");
    check(IsWindow(liveChild)&&GetWindow(liveHost,GW_CHILD)==liveChild&&d.windows.back()->isVisible(),"ARA Save As retains editor");
    const auto saved=mrs::persistence::load_project(fsPath(savedFile));const auto& savedBinding=saved.project.ara_bindings.front();
    check(saved.project.version==mrs::schema_version&&!savedBinding.archive.empty()&&!savedBinding.component_state.empty()&&savedBinding.source_checksum!=0&&savedBinding.source==saved.project.clips.front().source,"ARA model/archive/media reference saved together");
    juce::MemoryBlock savedBytes;check(savedFile.loadFileAsData(savedBytes),"ARA saved project bytes");
    SetEnvironmentVariableW(L"MRS_ARA_TEST_STATE_FAIL",L"1");bool saveRejected=false;try{d.app.save_project(fsPath(savedFile));}catch(const std::exception&){saveRejected=true;}SetEnvironmentVariableW(L"MRS_ARA_TEST_STATE_FAIL",nullptr);
    juce::MemoryBlock unchanged;check(saveRejected&&savedFile.loadFileAsData(unchanged)&&unchanged==savedBytes,"ARA archive failure leaves previous project intact");
    auto listen=[&](mrs::desktop::Application& app,float gain){app.stop();app.engine()->process(nullptr,output.data(),128);app.poll();app.play();app.engine()->process(nullptr,output.data(),128);for(std::size_t i=0;i<output.size();++i)check(std::abs(output[i]-dry[i]*gain)<.00001f,"ARA restored audio exactly once");app.stop();app.engine()->process(nullptr,output.data(),128);app.poll();};
    {mrs::desktop::Application reopened;reopened.open_project(fsPath(savedFile));reopened.connect(std::make_unique<Device>(),{0,48000,128,{},{0,1}});
        const auto source=reopened.services().projects->state().project->clips.front().source;check(reopened.ara_status(source).starts_with("ready"),"ARA restores source/model/archive before playback");listen(reopened,savedGain);
        reopened.bypass_ara_source(source,true);listen(reopened,1.f);check(reopened.undo()&&!reopened.ara_bypassed(source),"ARA bypass Undo");listen(reopened,savedGain);check(reopened.redo()&&reopened.ara_bypassed(source),"ARA bypass Redo");check(reopened.undo(),"ARA bypass back to active");
        reopened.detach_ara_source(source);check(reopened.ara_status(source)=="unbound","ARA durable detach");listen(reopened,1.f);check(reopened.undo(),"ARA detach Undo");listen(reopened,savedGain);check(reopened.redo()&&reopened.ara_status(source)=="unbound"&&reopened.undo(),"ARA detach Redo/Undo");listen(reopened,savedGain);
        const auto saveAs=folder.getChildFile("Relocated/Project.mrs");reopened.save_project(fsPath(saveAs));listen(reopened,savedGain);
        const auto relocated=mrs::persistence::load_project(fsPath(saveAs));check(relocated.project.ara_bindings.front().persistent_id==savedBinding.persistent_id,"ARA Save As preserves persistent IDs");
        reopened.open_project(fsPath(saveAs));reopened.connect(std::make_unique<Device>(),{0,48000,128,{},{0,1}});listen(reopened,savedGain);
    }
    auto fault=[&](const char* name,auto mutate){auto doc=saved;const auto media=savedFile.getParentDirectory().getChildFile(juce::String(saved.project.clips.front().source));
        for(auto& c:doc.project.clips)if(!c.midi)c.source=media.getFullPathName().toStdString();doc.project.ara_bindings.front().source=doc.project.clips.front().source;mutate(doc.project.ara_bindings.front(),doc.project);
        const auto target=folder.getChildFile(juce::String(name)+"/Project.mrs");check(target.getParentDirectory().createDirectory().wasOk(),"ARA fault fixture directory");mrs::persistence::save_project(fsPath(target),doc);return target;};
    const auto missingPlugin=fault("MissingPlugin",[&](auto& b,auto&){b.module=folder.getChildFile("absent.vst3").getFullPathName().toStdString();});
    {mrs::desktop::Application app;app.open_project(fsPath(missingPlugin));app.connect(std::make_unique<Device>(),{0,48000,128,{},{0,1}});const auto source=app.services().projects->state().project->clips.front().source;
        check(app.ara_status(source).starts_with("unavailable")&&app.snapshot().project.ara_bindings.front().archive==savedBinding.archive,"ARA missing plugin preserves opaque archive");listen(app,1.f);
        app.save_project(fsPath(missingPlugin));check(mrs::persistence::load_project(fsPath(missingPlugin)).project.ara_bindings.front().archive==savedBinding.archive,"ARA missing plugin resave preserves archive");
        app.relink_ara_source(source,fixture.getFullPathName().toStdString(),caps.front().class_id);listen(app,savedGain);check(app.undo()&&app.ara_status(source).starts_with("unavailable")&&app.redo(),"ARA plugin relink Undo/Redo");listen(app,savedGain);
    }
    for(bool corrupt:{false,true}){const auto broken=fault(corrupt?"CorruptArchive":"IncompatibleVersion",[&](auto& b,auto&){if(corrupt)b.archive.front()^=std::byte{1};else b.version+=" incompatible";});
        const auto before=mrs::persistence::load_project(fsPath(broken));mrs::desktop::Application app;app.open_project(fsPath(broken));app.connect(std::make_unique<Device>(),{0,48000,128,{},{0,1}});const auto source=app.services().projects->state().project->clips.front().source;
        check(app.ara_status(source).starts_with("unavailable")&&app.snapshot().project.ara_bindings.front().archive==before.project.ara_bindings.front().archive,"ARA corrupt/incompatible data remains opaque with dry fallback");listen(app,1.f);app.save_project(fsPath(broken));check(mrs::persistence::load_project(fsPath(broken)).project.ara_bindings.front().archive==before.project.ara_bindings.front().archive,"ARA damaged data resave preserves bytes");
    }
    const auto missingMedia=fault("MissingMedia",[&](auto& b,auto& p){b.source=folder.getChildFile("moved.wav").getFullPathName().toStdString();for(auto& c:p.clips)if(!c.midi)c.source=b.source;});
    {mrs::desktop::Application app;app.open_project(fsPath(missingMedia));app.connect(std::make_unique<Device>(),{0,48000,128,{},{0,1}});app.prepare_waveforms();const auto source=app.services().projects->state().project->clips.front().source;
        check(app.ara_status(source).starts_with("failed")&&app.snapshot().project.ara_bindings.front().archive==savedBinding.archive,"ARA missing media loads safely without losing archive");app.save_project(fsPath(missingMedia));
        const auto wrong=folder.getChildFile("Wrong.wav");auto bytes=original;static_cast<char*>(bytes.getData())[44]^=1;check(wrong.replaceWithData(bytes.getData(),bytes.getSize()),"ARA mismatched media fixture");const auto before=*app.services().projects->state().project;
        bool mediaRejected=false;try{app.relink_ara_media(source,fsPath(wrong));}catch(const std::exception&){mediaRejected=true;}check(mediaRejected&&*app.services().projects->state().project==before,"ARA wrong media relink is atomic and preserves archive");
        app.save_project(fsPath(missingMedia));check(missingMedia.getParentDirectory().getChildFile("Media").findChildFiles(juce::File::findFiles,false,"*.wav").isEmpty(),"ARA rejected relink never copies wrong media into project");
        app.relink_ara_media(source,fsPath(file));listen(app,savedGain);check(app.undo()&&app.services().projects->state().project->clips.front().source==source,"ARA media relink Undo");check(app.redo(),"ARA media relink Redo");listen(app,savedGain);
    }
    // RePitch/VocAlign create their actual view size only during attachment.
    // Exercise that lifecycle and failures through the real panel, including
    // retry, model preservation and removal of the covering native parent.
    struct EditorModeReset {~EditorModeReset(){SetEnvironmentVariableW(L"MRS_ARA_TEST_EDITOR_MODE",nullptr);}} editorModeReset;
    const auto editorClip=d.project()->clips.front();const auto editorModel=*d.project();
    d.closeAra(editorClip.source);
    for(const auto* mode:{L"late",L"invalid",L"attach-fail",L"late",L"resize"}){
        SetEnvironmentVariableW(L"MRS_ARA_TEST_EDITOR_MODE",mode);d.openAra(editorClip.id);
        auto* editor=d.windows.back().get();
        const auto editorHost=reinterpret_cast<HWND>(static_cast<std::intptr_t>(static_cast<juce::int64>(editor->getProperties()["mrs-native-host"])));
        auto* editorStatus=dynamic_cast<juce::Label*>(editor->getContentComponent()->findChildWithID("ara-status"));
        if(std::wcscmp(mode,L"late")==0||std::wcscmp(mode,L"resize")==0){
            check(IsWindow(editorHost)&&IsWindow(GetWindow(editorHost,GW_CHILD)),"ARA lazy-size native editor attaches and retries");
            RECT bounds{};GetClientRect(editorHost,&bounds);if(std::wcscmp(mode,L"resize")==0){const auto initialResizeBounds=editor->getBounds();NativePositionProbe resizePaint(GetParent(editorHost));editor->setContentComponentSize(1000,650);pump();RECT resized{};GetClientRect(editorHost,&resized);check(resized.right>=990&&resized.bottom>=550,"resizable vendor follows container through constrained VST3 onSize");
                check(resizePaint.redrawToggles==0&&resizePaint.visibilityChanges==0,"floating ARA resize never hides the viewport or toggles native redraw");
                const auto viewWindow=GetWindow(editorHost,GW_CHILD);check(!GetPropW(viewWindow,L"MRS_ARA_PARENT_SIZE_MISMATCH"),"vendor onSize observes the already-constrained native parent size");
                auto* resizingContent=editor->getContentComponent();d.attachAra(resizingContent,true);d.resizeMixer(500);resizePaint.redrawToggles=resizePaint.visibilityChanges=0;
                for(int height=510;height<d.getHeight()-140;height+=13)d.resizeMixer(height);
                check(resizePaint.redrawToggles==0&&resizePaint.visibilityChanges==0&&IsWindowVisible(viewWindow),"attached ARA resize keeps the native view visible and never toggles redraw");
                check(!GetPropW(viewWindow,L"MRS_ARA_PARENT_SIZE_MISMATCH"),"every dock resize sets native parent geometry before vendor onSize");d.attachAra(resizingContent,false);editor=d.windows.back().get();editor->setBounds(initialResizeBounds);
            }else check(bounds.right==640&&bounds.bottom==320,"ARA attached native size used");
        }else{
            check(editorHost==nullptr&&editorStatus&&editorStatus->getText().startsWith("Editor unavailable"),"ARA failed editor has no covering HWND and no false Ready");
            check(editorStatus->getTooltip().contains(std::wcscmp(mode,L"invalid")==0?"getSize after attached":"attached(HWND)"),"ARA failed editor exposes exact stage");
        }
        check(*d.project()==editorModel,"ARA editor open failure preserves project state");d.closeAra(editorClip.source);
    }
    SetEnvironmentVariableW(L"MRS_ARA_TEST_EDITOR_MODE",nullptr);
    SetEnvironmentVariableW(L"MRS_ARA_TEST_REQUIRE_ANALYSIS",nullptr);
    {Desktop closing(true);closing.app.new_project();closing.app.import_wavs({fsPath(file)});closing.app.connect(std::make_unique<Device>(),{0,48000,128,{},{0,1}});closing.setSize(1200,800);closing.addToDesktop(juce::ComponentPeer::windowIsTemporary);closing.setVisible(true);closing.refresh(true);
        const auto c=closing.project()->clips.front();closing.app.attach_ara_source(c.source,fixture.getFullPathName().toStdString(),caps.front().class_id);closing.openAra(c.id);const auto savedHost=static_cast<juce::int64>(closing.windows.back()->getProperties()["mrs-native-host"]);closing.attachAra(closing.windows.back()->getContentComponent(),true);closing.closeEditors();check(!closing.dockedClip&&!IsWindow(reinterpret_cast<HWND>(static_cast<std::intptr_t>(savedHost))),"closeEditors retires docked ARA native parent before project replacement");closing.app.new_project();for(int wait=0;wait<3&&closing.dockedEditor;++wait)pump();check(!closing.dockedEditor,"project replacement releases retired docked component");closing.removeFromDesktop();}
    {
        const auto secondFile=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mrs-ara-shared-",".wav");Cleanup secondCleanup{secondFile};check(file.copyFileTo(secondFile),"ARA second generated source");
        Desktop shared(true);shared.app.new_project();shared.app.import_wavs({fsPath(file),fsPath(secondFile)});shared.app.connect(std::make_unique<Device>(),{0,48000,128,{},{0,1}});
        shared.setSize(1200,800);shared.setVisible(true);shared.addToDesktop(juce::ComponentPeer::windowIsTemporary);shared.refresh(true);shared.setWorkspace(mrs::desktop::Workspace::arrange);
        const auto guide=shared.project()->clips[0],dub=shared.project()->clips[1];
        shared.app.attach_ara_source(guide.source,fixture.getFullPathName().toStdString(),caps.front().class_id);doubleClip(shared,guide.id);
        auto* commonWindow=shared.windows.back().get();const auto commonHost=commonWindow->getProperties()["mrs-native-host"];
        shared.araCatalog={{fixture.getFullPathName().toStdString(),caps.front()}};const auto siblingDescription=shared.beginAraPluginDrag(dragPlugin,true,0);shared.arrangement->itemDropped({siblingDescription,&shared.browser->tree,shared.arrangement->clipRect(dub).getCentre().toInt()});pump();if(shared.project()->ara_bindings.size()!=2)throw std::runtime_error("ARA neighbouring drop: "+shared.message.toStdString()+" target="+dub.source);doubleClip(shared,dub.id);
        check(shared.windows.size()==1&&shared.windows.back().get()==commonWindow&&commonWindow->getProperties()["mrs-native-host"]==commonHost,"ARA sources reuse the same native editor and HWND");
        check(shared.app.ara_document_key(guide.source)==shared.app.ara_document_key(dub.source)&&commonWindow->getProperties()["mrs-ara-source"].toString()==juce::String(dub.source),"ARA common editor selects the requested source");
        const auto selectionModel=*shared.project();const auto selectionRevision=shared.app.services().projects->state().revision;
        clickClip(shared,guide.id);pump();
        check(shared.selectedClip==guide.id&&commonWindow->getProperties()["mrs-ara-source"].toString()==juce::String(guide.source)&&commonWindow->getProperties()["mrs-native-host"]==commonHost&&shared.windows.size()==1,"single click follows source in the existing common native editor");
        check(shared.arrangement->hasKeyboardFocus(true),"ARA follow selection retains Arrangement keyboard focus");
        auto* commonPin=dynamic_cast<juce::TextButton*>(commonWindow->getContentComponent()->findChildWithID("ara-pin"));check(commonPin,"ARA common editor Pin");
        commonPin->setToggleState(true,juce::dontSendNotification);commonPin->onClick();clickClip(shared,dub.id);pump();
        check(shared.selectedClip==dub.id&&commonWindow->getProperties()["mrs-ara-source"].toString()==juce::String(guide.source),"Pin holds source while Arrangement selects another common-document clip");
        commonPin->setToggleState(false,juce::dontSendNotification);commonPin->onClick();clickClip(shared,dub.id);pump();
        check(commonWindow->getProperties()["mrs-ara-source"].toString()==juce::String(dub.source)&&*shared.project()==selectionModel&&shared.app.services().projects->state().revision==selectionRevision,"unpin and single click follow without a model or Undo transaction");
        clickClip(shared,guide.id);clickClip(shared,dub.id);pump();check(commonWindow->getProperties()["mrs-ara-source"].toString()==juce::String(dub.source),"queued selection ignores a superseded clip");
        auto* sharedDock=dynamic_cast<juce::TextButton*>(commonWindow->getContentComponent()->findChildWithID("ara-attach"));sharedDock->onClick();pump();auto* dockedCommon=shared.dockedEditor.get();clickClip(shared,guide.id);pump();check(shared.dockedEditor.get()==dockedCommon&&dockedCommon->getProperties()["mrs-ara-source"].toString()==juce::String(guide.source),"docked ARA follows clip selection without replacing it with Audio editor");sharedDock->onClick();pump();commonWindow=shared.windows.back().get();check(commonWindow->getProperties()["mrs-native-host"]==commonHost,"docked follow and detach preserve common native HWND");
        for(int wait=0;wait<1000&&(shared.app.ara_status(guide.source).starts_with("analysing")||shared.app.ara_status(dub.source).starts_with("analysing"));++wait){shared.app.poll_ara();Sleep(5);}
        shared.app.refresh_ara();check(shared.app.ara_caches().size()==2,"ARA prepares one cache for each common-document source");
        shared.app.play();std::array<float,256> followAudio{};shared.app.engine()->process(nullptr,followAudio.data(),128);clickClip(shared,guide.id);pump();
        check(commonWindow->getProperties()["mrs-ara-source"].toString()==juce::String(guide.source)&&shared.app.engine()->state().playback==mrs::PlaybackState::playing,"ARA follows clip during playback without stopping transport");shared.app.stop();shared.app.engine()->process(nullptr,followAudio.data(),128);shared.app.poll();
        shared.closeEditors(); // join the context worker before fixture model edits
        using SourceNote=bool(*)(const char*,float*,bool);const auto sourceNote=reinterpret_cast<SourceNote>(GetProcAddress(module,"MRSAraSourceNote"));check(sourceNote!=nullptr,"ARA per-source note fixture hook");
        const auto sourceStates=shared.app.snapshot().project.ara_bindings;
        for(std::size_t i=0;i<sourceStates.size();++i){float frequency=i==0?440.f:660.f;check(sourceNote(sourceStates[i].persistent_id.c_str(),&frequency,true),"ARA distinct source pitch edit");}
        const auto groupFile=folder.getChildFile("Shared/Project.mrs");shared.app.save_project(fsPath(groupFile));
        const auto groupState=mrs::persistence::load_project(fsPath(groupFile));check(groupState.project.ara_bindings.size()==2&&!groupState.project.ara_bindings[0].document_id.empty()&&groupState.project.ara_bindings[0].document_id==groupState.project.ara_bindings[1].document_id,"ARA common document identity survives Save As");
        {mrs::desktop::Application restored;restored.open_project(fsPath(groupFile));restored.connect(std::make_unique<Device>(),{0,48000,128,{},{0,1}});
            check(restored.ara_caches().size()==2,("ARA common document reopens both sources: "+restored.ara_status(restored.snapshot().project.clips[0].source)+" / "+restored.ara_status(restored.snapshot().project.clips[1].source)).c_str());
            check(restored.ara_document_key(restored.snapshot().project.clips[0].source)==restored.ara_document_key(restored.snapshot().project.clips[1].source),"ARA common document group identity restored");
            for(std::size_t i=0;i<sourceStates.size();++i){float frequency{};check(sourceNote(sourceStates[i].persistent_id.c_str(),&frequency,false)&&frequency==(i==0?440.f:660.f),"ARA separate source pitch edits survive shared document reopen");}
            const auto source=restored.services().projects->state().project->clips[0].source;restored.detach_ara_source(source);check(restored.ara_caches().size()==1&&restored.undo()&&restored.ara_caches().size()==2,"ARA common source detach Undo retains its neighbour");
            restored.detach_ara_source(source);restored.detach_ara_source(restored.services().projects->state().project->clips[1].source);check(restored.ara_caches().empty()&&restored.undo()&&restored.undo()&&restored.ara_caches().size()==2,"ARA restores a fully retired shared document through host Undo");
            for(std::size_t i=0;i<sourceStates.size();++i){float frequency{};check(sourceNote(sourceStates[i].persistent_id.c_str(),&frequency,false)&&frequency==(i==0?440.f:660.f),"ARA separate source pitch edits survive full retirement and Undo");}}
        shared.closeEditors();shared.app.disconnect();shared.removeFromDesktop();
    }
    {
        // A second synthetic module gives a distinct plugin document without
        // loading or modifying any installed third-party plugin.
        const auto otherModule=file.getParentDirectory().getNonexistentChildFile("mrs-ara-follow-fixture-",".vst3");Cleanup moduleCleanup{otherModule};check(fixture.copyFileTo(otherModule),"ARA follow alternate module copy");
        const auto otherFile=file.getParentDirectory().getNonexistentChildFile("mrs-ara-follow-source-",".wav");Cleanup sourceCleanup{otherFile};check(file.copyFileTo(otherFile),"ARA follow alternate source copy");
        Desktop follow(true);follow.app.new_project();follow.app.import_wavs({fsPath(file),fsPath(otherFile)});follow.app.connect(std::make_unique<Device>(),{0,48000,128,{},{0,1}});follow.setSize(1200,800);follow.setVisible(true);follow.addToDesktop(juce::ComponentPeer::windowIsTemporary);follow.refresh(true);
        const auto first=follow.project()->clips[0],second=follow.project()->clips[1];follow.app.attach_ara_source(first.source,fixture.getFullPathName().toStdString(),caps.front().class_id);
        clickClip(follow,first.id);pump();check(follow.windows.empty(),"selection with no open ARA editor creates no window");
        follow.openAra(first.id);auto* firstWindow=follow.windows.back().get();clickClip(follow,second.id);pump();check(firstWindow->isVisible()&&follow.project()->ara_bindings.size()==1,"unbound source selection preserves editor and bindings");
        follow.app.attach_ara_source(second.source,otherModule.getFullPathName().toStdString(),caps.front().class_id);const auto beforeFollow=*follow.project();const auto followRevision=follow.app.services().projects->state().revision;
        clickClip(follow,second.id);pump();auto* secondWindow=follow.windows.back().get();
        if(firstWindow->isVisible()||!secondWindow->isVisible()||secondWindow->getProperties()["mrs-ara-source"].toString()!=juce::String(second.source)||!follow.arrangement->hasKeyboardFocus(true))throw std::runtime_error("ARA cross-plugin follow: old="+std::to_string(firstWindow->isVisible())+" target="+std::to_string(secondWindow->isVisible())+" source="+secondWindow->getProperties()["mrs-ara-source"].toString().toStdString()+" focus="+std::to_string(follow.arrangement->hasKeyboardFocus(true))+" message="+follow.message.toStdString());
        auto* secondPin=dynamic_cast<juce::TextButton*>(secondWindow->getContentComponent()->findChildWithID("ara-pin"));secondPin->setToggleState(true,juce::dontSendNotification);secondPin->onClick();
        clickClip(follow,first.id);pump();check(follow.windows.back().get()==secondWindow&&secondWindow->isVisible(),"only pinned editors never open an automatic follower");
        follow.openAra(first.id);auto* follower=follow.windows.back().get();clickClip(follow,second.id);pump();check(follower->isVisible()&&secondWindow->isVisible()&&follower->getProperties()["mrs-ara-source"].toString()==juce::String(first.source),"pinned target blocks automatic reuse and preserves other editor");
        secondPin->setToggleState(false,juce::dontSendNotification);secondPin->onClick();clickClip(follow,second.id);pump();check(!follower->isVisible()&&secondWindow->isVisible(),"unpin permits reuse of already open target document");
        check(*follow.project()==beforeFollow&&follow.app.services().projects->state().revision==followRevision,"cross-plugin follow preserves complete project and Undo revision");
        follow.closeEditors();clickClip(follow,first.id);pump();check(std::none_of(follow.windows.begin(),follow.windows.end(),[](const auto& w){return w->isVisible();}),"closed ARA editors stay closed on selection");follow.app.disconnect();follow.removeFromDesktop();
    }
    {
        using Settings=bool(*)(std::uint32_t,std::uint32_t);using Patch=bool(*)(const char*,double,double,float,int);
        const auto settings=reinterpret_cast<Settings>(GetProcAddress(module,"MRSAraRenderSettings"));const auto patch=reinterpret_cast<Patch>(GetProcAddress(module,"MRSAraPatch"));check(settings&&patch&&settings(0,0),"ARA live fixture setup");
        struct Restore{Settings settings;Patch patch;~Restore(){patch("",0,0,1,-1);settings(17,0);}} restore{settings,patch};
        Desktop live(true);live.app.new_project();live.app.import_wavs({fsPath(file)});live.app.connect(std::make_unique<Device>(),{0,48000,128,{},{0,1}});live.setSize(1200,800);live.setVisible(true);live.addToDesktop(juce::ComponentPeer::windowIsTemporary);live.refresh(true);
        const auto c=live.project()->clips.front();live.app.attach_ara_source(c.source,fixture.getFullPathName().toStdString(),caps.front().class_id);live.openAra(c.id);
        for(int wait=0;wait<1000&&live.app.ara_status(c.source).starts_with("analysing");++wait){live.app.poll_ara();Sleep(5);}live.app.refresh_ara();
        const auto cached=live.app.ara_caches().front().pcm;const auto binding=live.app.services().projects->state().project->ara_bindings.front();check(patch(binding.persistent_id.c_str(),0.,1.,2.f,2),"ARA live edit without range");live.app.poll_ara();live.prepareAraWhenIdle(1000,false);live.prepareAraWhenIdle(2000,false);
        check(!live.app.ara_pending_revision()&&live.app.ara_caches().front().pcm==cached,"ARA live edit schedules no PCM preparation");
        auto* livePanel=dynamic_cast<AraPanel*>(live.windows.back()->getContentComponent());livePanel->sync();auto* label=dynamic_cast<juce::Label*>(livePanel->findChildWithID("ara-status"));auto* details=dynamic_cast<juce::Label*>(livePanel->findChildWithID("ara-preparation-details"));check(label&&label->getText().contains("live ARA")&&details&&details->getText().contains("live=1"),"ARA live mode visible in native editor host");
        for(float scale:{1.f,1.5f}){const auto image=livePanel->createComponentSnapshot(livePanel->getLocalBounds(),true,scale);auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("ara-live-"+juce::String(static_cast<int>(scale*100))+".png").createOutputStream();check(stream&&stream->setPosition(0)&&stream->truncate().wasOk()&&juce::PNGImageFormat().writeImageToStream(image,*stream),"ARA live host snapshot");}
        performAraOperation(live,[&]{live.app.play();});live.app.engine()->process(nullptr,output.data(),128);check(std::any_of(output.begin(),output.end(),[](float v){return std::abs(v)>.00001f;}),"ARA live signal reaches engine");live.closeEditors();live.app.engine()->process(nullptr,output.data(),128);check(!live.app.engine()->metrics().processing_fault,"closing editor keeps playback renderer alive");live.app.stop();live.app.engine()->process(nullptr,output.data(),128);live.app.disconnect();live.removeFromDesktop();
    }
    {Desktop aiHost(true);aiHost.app.new_project();aiHost.app.import_wavs({fsPath(file)});aiHost.app.connect(std::make_unique<Device>(),{0,48000,128,{},{0,1}});aiHost.setSize(1200,800);aiHost.addToDesktop(juce::ComponentPeer::windowIsTemporary);aiHost.setVisible(true);aiHost.refresh(true);
        const auto aiTarget=aiHost.project()->clips.front();aiHost.selectedClip=aiTarget.id;aiHost.selectedTrack=aiTarget.track;aiHost.refresh();aiHost.araCatalog={{fixture.getFullPathName().toStdString(),caps.front()}};aiHost.publishAraTools();auto policy=aiHost.app.ai_permissions();policy.mode=mrs::ai::PermissionMode::edit;aiHost.app.set_ai_permissions(policy);
        mrs::ai::Query choices;choices.kind=mrs::ai::QueryKind::ara_capabilities;const auto available=aiHost.app.ai_read_view().value->query(choices);const auto plugin=mrs::Id{juce::JSON::parse(juce::String::fromUTF8(available.json.c_str()))["items"][0]["plugin_id"].toString().toStdString()};check(!plugin.value.empty(),"ARA AI exposes host-issued plugin ID");int operation{};
        const auto propose=[&](mrs::ai::ToolKind kind,mrs::ai::AraArgs args){mrs::ai::Selection selection;selection.entities={{mrs::ai::EntityKind::track,aiTarget.track},{mrs::ai::EntityKind::clip,aiTarget.id}};const auto key=aiHost.app.ai_context(selection).value->key;auto snapshot=aiHost.app.ai_snapshot(key,{choices},selection);check(snapshot.value!=nullptr,"ARA AI GUI snapshot");auto request=aiHost.app.request_ai({snapshot.value->context,"fixture",std::chrono::seconds(2),snapshot.value},[](const auto&,const auto&){return std::string("fixture");});check(request.error==mrs::ai::Error::none,"ARA AI GUI mock request");bool done{};for(int wait=0;wait<100&&!done;++wait){done=aiHost.app.take_ai_result().has_value();pump();}check(done,"ARA AI GUI mock completes");auto plan=aiHost.app.ai_propose({1,request.request_id,"gui-ara-"+std::to_string(++operation),key,kind,args},snapshot.value);check(plan.value!=nullptr,"ARA AI GUI valid plan");auto preview=aiHost.app.ai_preview({plan.value});check(preview.value!=nullptr,"ARA AI GUI preview");return preview.value;};
        auto attach=propose(mrs::ai::ToolKind::attach_ara,{aiTarget.id,plugin});check(aiHost.project()->ara_bindings.empty()&&aiHost.windows.empty(),"ARA AI preview has no binding/editor side effects");check(aiHost.app.ai_accept_preview(attach).allowed(),"ARA AI attach Apply");
        const auto beforeOpen=*aiHost.project();auto open=propose(mrs::ai::ToolKind::open_ara_editor,{aiTarget.id});check(aiHost.windows.empty(),"ARA AI editor remains closed before review");check(aiHost.app.ai_accept_preview(open).allowed()&&!aiHost.windows.empty()&&*aiHost.project()==beforeOpen,"ARA AI opens real editor without project mutation");
        const auto parent=reinterpret_cast<HWND>(static_cast<std::intptr_t>(static_cast<juce::int64>(aiHost.windows.back()->getProperties()["mrs-native-host"])));check(IsWindow(parent)&&IsWindow(GetWindow(parent,GW_CHILD)),"ARA AI native editor attached");
        auto aiRemove=propose(mrs::ai::ToolKind::remove_ara,{aiTarget.id});check(aiHost.app.ai_accept_preview(aiRemove).allowed()&&!IsWindow(parent)&&aiHost.project()->ara_bindings.empty(),"ARA AI Remove closes editor before removing session");check(aiHost.app.undo()&&!aiHost.project()->ara_bindings.empty(),"ARA AI Remove Undo restores binding");aiHost.closeEditors();aiHost.app.disconnect();aiHost.removeFromDesktop();
    }
    d.app.new_project();d.refresh(true);check(d.app.ara_status(clip.source)=="unbound","ARA project replace retires binding");d.closeEditors();d.app.disconnect();d.removeFromDesktop();juce::MemoryBlock after;check(file.loadFileAsData(after)&&after==original,"ARA never changes source WAV");
}
#else
void retireAraWindow(EditorWindow&){}
void retireAraComponent(juce::Component&){}
void performAraOperation(Desktop&,const std::function<void()>& operation){operation();}
void araEditorSmoke(const juce::File&){throw std::runtime_error("ARA disabled in this build");}
#endif
}
