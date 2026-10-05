#include "Desktop.h"
#include "PluginPreset.h"
#include <windows.h>

namespace ui {
namespace {
juce::String label(const std::string& s){return juce::String::fromUTF8(s.c_str());}
std::filesystem::path path(const juce::File& f){return std::filesystem::path(f.getFullPathName().toWideCharPointer());}
}
class PresetSelector final : public juce::ComboBox {
public:
    PresetSelector(Desktop& desktop,std::optional<mrs::Id> track,mrs::Id id):owner(desktop),target(track),slot(id) {
        setTitle("Saved plugin presets");setTextWhenNothingSelected("Saved presets...");
        refresh();
        onChange=[this]{const auto index=getSelectedId()-1;if(index>=0&&index<files.size())owner.loadPresetFile(target,slot,files[index]);};
    }
    void refresh(){const auto text=getText();clear(juce::dontSendNotification);files.clear();
        const auto effects=owner.chain(target);for(const auto& effect:effects)if(effect.id==slot)files=presetFiles(effect);
        for(int i=0;i<files.size();++i){const auto name=files[i].getFileNameWithoutExtension();addItem(name,i+1);if(name==text)setSelectedId(i+1,juce::dontSendNotification);}
    }
private:
    Desktop& owner;std::optional<mrs::Id> target;mrs::Id slot;juce::Array<juce::File> files;
};
class FxPanel final : public juce::Component {
public:
    FxPanel(Desktop& d,std::optional<mrs::Id> t,mrs::Id id):owner(d),target(t),slot(id),library(d,t,id){
        addAndMakeVisible(library);
        gain.setComponentID("native-gain");apply.setComponentID("apply-native");
        for(auto* b:{&apply,&bypass,&remove,&up,&down,&enabled,&load,&invert,&savePreset,&loadPreset})addAndMakeVisible(b);
        for(auto* c:{static_cast<juce::Component*>(&bands),static_cast<juce::Component*>(&parameters),static_cast<juce::Component*>(&presets),static_cast<juce::Component*>(&gain),static_cast<juce::Component*>(&frequency),static_cast<juce::Component*>(&q),static_cast<juce::Component*>(&mix)})addAndMakeVisible(c);
        bands.addItemList({"HP / Low cut","Band 1","Band 2","Band 3","LP / High cut"},1);bands.setSelectedId(2);
        presets.addItemList({"Neutral","Warm","Bright"},1);presets.setTextWhenNothingSelected("Preset...");
        bands.onChange=[this]{band=static_cast<std::size_t>(bands.getSelectedId()-1);sync();};parameters.onChange=[this]{sync();};
        apply.onClick=[this]{owner.run([&]{auto n=current();if(n.kind==mrs::InsertKind::vst3){const int i=parameters.getSelectedId()-1;if(i>=0&&i<static_cast<int>(infos.size()))owner.app.set_plugin_parameter(target,slot,infos[static_cast<std::size_t>(i)].id,gain.getText().getFloatValue());return;}
            if(n.kind==mrs::InsertKind::channel_eq){n.bands[band].gain=gain.getText().getFloatValue();n.bands[band].frequency=frequency.getText().getFloatValue();n.bands[band].q=q.getText().getFloatValue();}
            else {n.gain=n.kind==mrs::InsertKind::eq?gain.getText().getFloatValue():std::pow(10.f,gain.getText().getFloatValue()/20);n.frequency=frequency.getText().getFloatValue();n.q=q.getText().getFloatValue();if(n.kind==mrs::InsertKind::cab_ir){n.ir.mix=mix.getText().getFloatValue();n.ir.low_cut=n.frequency;n.ir.high_cut=n.q;}}
            commit(n);});};
        savePreset.onClick=[this]{owner.savePreset(target,slot);};loadPreset.onClick=[this]{owner.loadPreset(target,slot);};
        bypass.onClick=[this]{owner.run([&]{auto n=current();n.bypass=!n.bypass;commit(n);});};
        enabled.onClick=[this]{owner.run([&]{auto n=current();n.bands[band].enabled=!n.bands[band].enabled;commit(n);});};
        invert.onClick=[this]{owner.run([&]{auto n=current();n.ir.invert=!n.ir.invert;commit(n);});};
        presets.onChange=[this]{owner.run([&]{auto n=current();const auto p=presets.getSelectedId();n.gain=1;n.ir.mix=1;n.ir.invert=false;n.ir.low_cut=p==2?80.f:p==3?50.f:20.f;n.ir.high_cut=p==2?5000.f:p==3?10000.f:20000.f;commit(n);});};
        juce::Component::SafePointer<FxPanel> safe(this);load.onClick=[safe]{if(safe)safe->owner.choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe](const auto& f){if(safe){auto n=safe->current();n.ir=mrs::audio::load_cab_ir(path(f));safe->commit(n);}},"*.wav");};
        auto structure=[this](int direction){owner.run([&]{auto effects=owner.chain(target);auto i=index(effects);if(direction==0)effects.erase(effects.begin()+static_cast<std::ptrdiff_t>(i));
            else if((direction<0&&i>0)||(direction>0&&i+1<effects.size()))std::swap(effects[i],effects[direction<0?i-1:i+1]);
            owner.closeEditors();owner.applyChain(target,effects);});};
        remove.onClick=[structure]{structure(0);};up.onClick=[structure]{structure(-1);};down.onClick=[structure]{structure(1);};
        setWantsKeyboardFocus(true);setSize(720,550);sync();
    }
    ~FxPanel() override {if(drag)owner.app.cancel_insert_preview();}
    std::size_t index(const std::vector<mrs::NativeInsert>& effects) const{for(std::size_t i=0;i<effects.size();++i)if(effects[i].id==slot)return i;throw std::runtime_error("Insert no longer exists");}
    mrs::NativeInsert current() const{auto effects=owner.chain(target);return effects[index(effects)];}
    void commit(const mrs::NativeInsert& n){auto effects=owner.chain(target);effects[index(effects)]=n;owner.applyChain(target,effects);sync();}
    void sync(){auto n=current();const bool eq=n.kind==mrs::InsertKind::channel_eq,cab=n.kind==mrs::InsertKind::cab_ir,vst=n.kind==mrs::InsertKind::vst3;
        gain.setEnabled(!eq||(band>0&&band<4));
        bands.setVisible(eq);enabled.setVisible(eq);load.setVisible(cab);mix.setVisible(cab);presets.setVisible(cab);invert.setVisible(cab);parameters.setVisible(vst);
        frequency.setEnabled(!vst&&n.kind!=mrs::InsertKind::gain);q.setEnabled(frequency.isEnabled());
        if(vst&&infos.empty()){infos=owner.app.plugin_parameters(target,slot);std::erase_if(infos,[](const auto& p){return p.hidden;});for(std::size_t i=0;i<infos.size();++i)parameters.addItem(label(infos[i].name),static_cast<int>(i)+1);if(!infos.empty())parameters.setSelectedId(1,juce::dontSendNotification);}
        float value=eq?n.bands[band].gain:n.kind==mrs::InsertKind::eq?n.gain:20*std::log10(juce::jmax(.000001f,n.gain));
        if(vst){const auto live=owner.app.plugin_parameters(target,slot);int at=parameters.getSelectedId()-1;if(at>=0&&at<static_cast<int>(infos.size())){for(const auto& p:live)if(p.id==infos[static_cast<std::size_t>(at)].id)value=p.initial;apply.setEnabled(infos[static_cast<std::size_t>(at)].automatable);}}
        gain.setText(juce::String(value,4),false);frequency.setText(juce::String(cab?n.ir.low_cut:eq?n.bands[band].frequency:n.frequency,2),false);
        q.setText(juce::String(cab?n.ir.high_cut:eq?n.bands[band].q:n.q,4),false);mix.setText(juce::String(n.ir.mix,3),false);
        enabled.setButtonText(eq&&n.bands[band].enabled?"Band on":"Band off");invert.setButtonText(n.ir.invert?"Polarity -":"Polarity +");bypass.setButtonText(n.bypass?"Enable":"Bypass");repaint();
    }
    void resized() override {library.setBounds(20,514,670,28);bypass.setBounds(20,14,90,28);remove.setBounds(118,14,90,28);up.setBounds(216,14,70,28);down.setBounds(294,14,70,28);savePreset.setBounds(380,14,150,28);loadPreset.setBounds(540,14,150,28);
        bands.setBounds(20,335,180,28);enabled.setBounds(210,335,100,28);parameters.setBounds(20,335,460,28);
        load.setBounds(20,335,120,28);presets.setBounds(150,335,125,28);invert.setBounds(285,335,115,28);mix.setBounds(500,335,100,28);
        gain.setBounds(20,402,170,28);frequency.setBounds(215,402,170,28);q.setBounds(410,402,170,28);apply.setBounds(20,447,160,30);}
    juce::Rectangle<float> curve() const{return {24,76,660,234};}
    juce::Point<float> point(const mrs::EqBand& b) const {auto r=curve();return {r.getX()+static_cast<float>(std::log(b.frequency/20.)/std::log(1000.))*r.getWidth(),r.getCentreY()-b.gain/48.f*r.getHeight()};}
    void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(0xff31363b));g.setColour(juce::Colours::lightgrey);g.setFont(owner.theme.font(12));
        g.drawText("Gain dB / VST 0..1",20,374,180,24,juce::Justification::left);g.drawText("Frequency / low cut Hz",215,374,180,24,juce::Justification::left);g.drawText("Q / high cut Hz",410,374,180,24,juce::Justification::left);
        g.drawText("Live native parameters; structural edits and project save after Pause / Stop",20,483,680,24,juce::Justification::left);
        auto n=drag?*drag:current();g.setColour(juce::Colour(0xff232a30));g.fillRect(curve());
        if(n.kind==mrs::InsertKind::channel_eq){g.setColour(juce::Colour(0xff46515a));for(int i=0;i<5;++i)g.drawHorizontalLine(static_cast<int>(curve().getY()+i*curve().getHeight()/4),curve().getX(),curve().getRight());
            juce::Path response;for(int x=0;x<=660;++x){const double hz=20*std::pow(1000.,x/660.);const double db=mrs::processing::eq_response_db(n,hz,owner.project()->sample_rate);float y=curve().getCentreY()-static_cast<float>(juce::jlimit(-24.,24.,db))/48.f*curve().getHeight();if(x==0)response.startNewSubPath(curve().getX(),y);else response.lineTo(curve().getX()+x,y);}
            g.setColour(juce::Colours::skyblue);g.strokePath(response,juce::PathStrokeType(2));for(std::size_t i=0;i<n.bands.size();++i){auto p=point(n.bands[i]);g.setColour(n.bands[i].enabled?juce::Colours::orange:juce::Colours::grey);g.fillEllipse(p.x-5,p.y-5,10,10);}
            g.setColour(juce::Colours::lightgrey);g.drawText("Point drag: frequency / gain; wheel on point: Q; Ctrl: fine",20,52,660,20,juce::Justification::left);
        }else {g.setColour(juce::Colours::lightgrey);g.drawText(n.kind==mrs::InsertKind::cab_ir?label(n.ir.name)+" | Embedded IR | 0 additional algorithmic samples":n.kind==mrs::InsertKind::vst3?label(n.plugin_name)+" | latency "+juce::String(owner.app.plugin_latency(target,slot))+" samples":"Native gain / filter",curve().toNearestInt(),juce::Justification::centred);}
    }
    void mouseDown(const juce::MouseEvent& e) override{owner.run([&]{auto n=current();if(n.kind!=mrs::InsertKind::channel_eq)return;for(std::size_t i=0;i<5;++i)if(point(n.bands[i]).getDistanceFrom(e.position)<12){band=i;bands.setSelectedId(static_cast<int>(i)+1,juce::dontSendNotification);drag=n;last=e.position;grabKeyboardFocus();break;}});}
    void mouseDrag(const juce::MouseEvent& e) override {if(!drag)return;owner.run([&]{auto& b=drag->bands[band];float fine=e.mods.isCtrlDown()?.1f:1.f;b.frequency=juce::jlimit(20.f,20000.f,b.frequency*static_cast<float>(std::pow(1000.,(e.position.x-last.x)*fine/curve().getWidth())));
        if(band>0&&band<4)b.gain=juce::jlimit(-24.f,24.f,b.gain-(e.position.y-last.y)*fine/curve().getHeight()*48);last=e.position;
        auto effects=owner.chain(target);effects[index(effects)]=*drag;owner.app.preview_inserts(target,effects);repaint();});}
    void mouseUp(const juce::MouseEvent&) override {if(!drag)return;auto n=*drag;drag.reset();owner.run([&]{commit(n);});}
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& w) override{owner.run([&]{auto n=current();if(n.kind!=mrs::InsertKind::channel_eq)return;for(std::size_t i=0;i<5;++i)if(point(n.bands[i]).getDistanceFrom(e.position)<14){band=i;n.bands[i].q=juce::jlimit(.1f,10.f,n.bands[i].q*std::pow(1.2f,w.deltaY*4));commit(n);break;}});}
    bool keyPressed(const juce::KeyPress& k) override {if(k==juce::KeyPress::escapeKey&&drag){cancel();return true;}return false;}
    void focusLost(FocusChangeType) override {cancel();}
    void cancel(){if(drag){drag.reset();owner.app.cancel_insert_preview();repaint();}}
private:
    Desktop& owner;std::optional<mrs::Id> target;mrs::Id slot;std::size_t band{1};
    std::optional<mrs::NativeInsert> drag;juce::Point<float> last;
    std::vector<mrs::processing::ParameterInfo> infos;
    juce::TextButton savePreset{"Save preset..."},loadPreset{"Load preset..."};
    juce::TextButton apply{"Apply parameters"},bypass{"Bypass"},remove{"Remove"},up{"Up"},down{"Down"},enabled{"Band on"},load{"Load IR WAV"},invert{"Polarity +"};
    PresetSelector library;
    juce::ComboBox bands,parameters,presets;
    juce::TextEditor gain,frequency,q,mix;
};
juce::PopupMenu Desktop::pluginMenu(int base) const {
    std::map<juce::String,juce::PopupMenu> groups;
    for(std::size_t i=0;i<catalog.size();++i){auto vendor=label(catalog[i].vendor).trim();if(vendor.isEmpty())vendor="Unknown vendor";groups[vendor].addItem(base+static_cast<int>(i),label(catalog[i].name));}
    juce::PopupMenu result;for(auto& [vendor,items]:groups)result.addSubMenu(vendor,items);return result;
}
void Desktop::refreshPresetLists(){
    std::function<void(juce::Component*)> visit=[&](juce::Component* component){if(auto* selector=dynamic_cast<PresetSelector*>(component))selector->refresh();for(int i=0;i<component->getNumChildComponents();++i)visit(component->getChildComponent(i));};
    for(auto& window:windows)if(window->isVisible())visit(window.get());
}
void Desktop::savePreset(std::optional<mrs::Id> target,mrs::Id slot){run([&]{
    auto effect=app.capture_insert(target,slot);auto bytes=encodePreset(effect);auto folder=presetFolder(effect);
    juce::Component::SafePointer<Desktop> safe(this);
    textDialog("Save plugin preset (new name)","Preset",[safe,bytes,folder](auto name){if(!safe)return;
        name=name.trim();if(name.isEmpty()||name!=juce::File::createLegalFileName(name)||name.length()>100)throw std::runtime_error("Use a valid preset name (1..100 characters)");
        auto file=folder.getChildFile(name+".mrspreset");if(file.exists())throw std::runtime_error("Preset name already exists; choose a new name");
        if(folder.createDirectory().failed())throw std::runtime_error("Cannot create plugin preset folder");
        publishSettings(file,bytes);safe->refreshPresetLists();
    });
});}
void Desktop::loadPresetFile(std::optional<mrs::Id> target,mrs::Id slot,const juce::File& file){run([&]{
    if(!file.existsAsFile()||file.getSize()>8*1024*1024)throw std::runtime_error("Plugin preset missing or exceeds size limit");
    auto effects=chain(target);auto at=std::find_if(effects.begin(),effects.end(),[&](const auto& n){return n.id==slot;});if(at==effects.end())throw std::runtime_error("Insert no longer exists");
    auto preset=decodePreset(file.loadFileAsString(),*at);
    if(app.engine()->state().playback==mrs::PlaybackState::playing||app.recording())throw std::runtime_error("Pause/Stop before loading a plugin preset");
    const bool nativeView=preset.kind==mrs::InsertKind::vst3;
    if(!nativeView)closeEditors();app.load_insert_preset(target,std::move(preset));editorGeneration=app.insert_generation();refresh(true);
    if(!nativeView)openInsert(target,slot);
});}
void Desktop::loadPreset(std::optional<mrs::Id> target,mrs::Id slot){juce::Component::SafePointer<Desktop> safe(this);
    choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,target,slot](auto file){if(safe)safe->loadPresetFile(target,slot,file);},"*.mrspreset");
}
void Desktop::insertMenu(std::optional<mrs::Id> target){juce::PopupMenu m;m.addItem(1,"Add Gain");m.addItem(2,"Add Channel EQ");m.addItem(3,"Add Cab IR...");
    const auto existing=chain(target);for(std::size_t i=0;i<existing.size();++i)m.addItem(1000+static_cast<int>(i),"Edit insert "+juce::String(static_cast<int>(i)+1));
    m.addSeparator();auto vendors=pluginMenu();for(juce::PopupMenu::MenuItemIterator it(vendors);it.next();)m.addItem(it.getItem());
    juce::Component::SafePointer<Desktop> safe(this);m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe,target](int n){if(!safe||!n)return;safe->run([&]{
        if(n>=10000){safe->addPlugin(target,static_cast<std::size_t>(n-10000));return;}if(n>=1000){auto effects=safe->chain(target);safe->openInsert(target,effects.at(static_cast<std::size_t>(n-1000)).id);return;}
        if(n==3){safe->choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,target](auto f){if(!safe)return;auto effects=safe->chain(target);mrs::NativeInsert ir;ir.id=mrs::new_id();ir.kind=mrs::InsertKind::cab_ir;ir.ir=mrs::audio::load_cab_ir(path(f));effects.push_back(ir);safe->closeEditors();safe->applyChain(target,effects);safe->openInsert(target,ir.id);},"*.wav");return;}
        auto effects=safe->chain(target);mrs::NativeInsert effect;effect.id=mrs::new_id();effect.kind=n==1?mrs::InsertKind::gain:mrs::InsertKind::channel_eq;effects.push_back(effect);safe->closeEditors();safe->applyChain(target,effects);safe->openInsert(target,effect.id);
    });});}
class PluginPanel final : public juce::Component,private juce::Timer {
public:
    PluginPanel(Desktop& d,std::optional<mrs::Id> t,mrs::Id id):owner(d),target(t),slot(id),library(d,t,id){
        addAndMakeVisible(library);
        setTitle("Plugin editor and insert controls");setSize(760,520);
        for(auto* button:{&save,&load,&bypass,&remove,&up,&down,&parameters}){addAndMakeVisible(button);button->setTitle(button->getButtonText());}
        save.onClick=[this]{owner.savePreset(target,slot);};load.onClick=[this]{owner.loadPreset(target,slot);};
        bypass.onClick=[this]{owner.run([&]{auto effects=owner.chain(target);auto at=find(effects);at->bypass=!at->bypass;owner.applyChain(target,effects);sync();});};
        auto structure=[this](int direction){owner.run([&]{auto effects=owner.chain(target);auto at=find(effects);auto pos=at-effects.begin();
            if(direction==0)effects.erase(at);else if((direction<0&&pos>0)||(direction>0&&pos+1<static_cast<std::ptrdiff_t>(effects.size())))std::iter_swap(at,at+direction);
            owner.closeEditors();owner.applyChain(target,effects);});};
        remove.onClick=[structure]{structure(0);};up.onClick=[structure]{structure(-1);};down.onClick=[structure]{structure(1);};
        parameters.onClick=[this]{auto window=std::make_unique<EditorWindow>("Plugin parameters",new FxPanel(owner,target,slot));window->setLookAndFeel(&owner.theme);owner.windows.push_back(std::move(window));};sync();
    }
    ~PluginPanel() override {retire();}
    void retire(){stopTimer();if(host&&IsWindow(host))DestroyWindow(host);host=nullptr;}
    auto find(std::vector<mrs::NativeInsert>& effects){auto at=std::find_if(effects.begin(),effects.end(),[&](const auto& n){return n.id==slot;});if(at==effects.end())throw std::runtime_error("Insert no longer exists");return at;}
    void sync(){auto effects=owner.chain(target);bypass.setButtonText(find(effects)->bypass?"Enable":"Bypass");}
    bool attach(EditorWindow& w){window=&w;host=CreateWindowExW(0,L"STATIC",L"MR Studio plugin host",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,50,640,480,static_cast<HWND>(w.getPeer()->getNativeHandle()),nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!host)throw std::runtime_error("Cannot create native plugin editor host");
        if(!owner.app.open_plugin_editor(target,slot,host,nativeWidth,nativeHeight))return false;
        w.getProperties().set("mrs-native-host",static_cast<juce::int64>(reinterpret_cast<std::intptr_t>(host)));resizeWindow();startTimerHz(10);return true;
    }
    void resizeWindow(){const auto scale=window->getPeer()->getPlatformScaleFactor();window->fitNativeEditor(juce::jmax(nativeWidth,juce::roundToInt(760*scale)),nativeHeight+juce::roundToInt(toolbar*scale));resized();}
    void resized() override {library.setBounds(8,46,getWidth()-16,28);int x=8;for(auto* button:{&save,&load,&bypass,&up,&down,&remove,&parameters}){int width=button==&save||button==&load?124:button==&parameters?112:button==&up||button==&down?48:80;button->setBounds(x,8,width,30);x+=width+6;}
        if(host&&window&&window->getPeer()){auto scale=window->getPeer()->getPlatformScaleFactor();SetWindowPos(host,nullptr,0,juce::roundToInt(toolbar*scale),nativeWidth,nativeHeight,SWP_NOZORDER|SWP_NOACTIVATE);}
    }
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff31363b));}
private:
    void timerCallback() override {if(!host||!IsWindow(host)||!window||!window->isVisible())return;RECT rect{};if(GetClientRect(host,&rect)&&rect.right>0&&rect.bottom>0&&(rect.right!=nativeWidth||rect.bottom!=nativeHeight)){nativeWidth=rect.right;nativeHeight=rect.bottom;resizeWindow();}}
    Desktop& owner;std::optional<mrs::Id> target;mrs::Id slot;HWND host{};EditorWindow* window{};int nativeWidth{640},nativeHeight{480};static constexpr int toolbar=86;
    PresetSelector library;
    juce::TextButton save{"Save preset..."},load{"Load preset..."},bypass{"Bypass"},up{"Up"},down{"Down"},remove{"Remove"},parameters{"Parameters"};
};
void retirePluginWindow(EditorWindow& window){if(auto* content=dynamic_cast<PluginPanel*>(window.getContentComponent()))content->retire();}
void Desktop::openInsert(std::optional<mrs::Id> target,mrs::Id slot){run([&]{auto effects=chain(target);auto found=std::find_if(effects.begin(),effects.end(),[&](const auto& n){return n.id==slot;});if(found==effects.end())return;
    const auto key=label((target?target->value:"master")+":"+slot.value);for(auto& w:windows)if(w->isVisible()&&w->getProperties()["mrs-slot"].toString()==key){w->toFront(true);return;}
    closeEditors();editorGeneration=app.insert_generation();if(found->kind==mrs::InsertKind::vst3){auto* content=new PluginPanel(*this,target,slot);auto window=std::make_unique<EditorWindow>(label(found->plugin_name),content,false);window->setLookAndFeel(&theme);
        bool attached=false;try{attached=content->attach(*window);}catch(...){app.close_plugin_editors();content->retire();throw;}
        if(attached){window->setVisible(true);window->getProperties().set("mrs-slot",key);window->getProperties().set("mrs-native-editor",true);window->onClose=[this,content]{app.close_plugin_editors();content->retire();};windows.push_back(std::move(window));return;}}
    auto fx=std::make_unique<EditorWindow>("Insert editor",new FxPanel(*this,target,slot));fx->setLookAndFeel(&theme);fx->getProperties().set("mrs-slot",key);windows.push_back(std::move(fx));
});}

class AudioPanel final : public juce::Component {
public:
    explicit AudioPanel(Desktop& d,bool test=false):owner(d){
        for(auto* c:{static_cast<juce::Component*>(&devices),static_cast<juce::Component*>(&rate),static_cast<juce::Component*>(&buffer),static_cast<juce::Component*>(&workers),static_cast<juce::Component*>(&processBuffer),static_cast<juce::Component*>(&inputs),static_cast<juce::Component*>(&outputs),static_cast<juce::Component*>(&connect),static_cast<juce::Component*>(&disconnect),static_cast<juce::Component*>(&panelButton),static_cast<juce::Component*>(&profiles),static_cast<juce::Component*>(&profileName),static_cast<juce::Component*>(&save),static_cast<juce::Component*>(&load),static_cast<juce::Component*>(&remove),static_cast<juce::Component*>(&reconnect)})addAndMakeVisible(c);
        setFocusContainerType(FocusContainerType::keyboardFocusContainer);setTitle("Audio settings");devices.setTitle("ASIO device");rate.setTitle("Sample rate");buffer.setTitle("Device Buffer frames");processBuffer.setTitle("Process Buffer frames, off or eligible playback lookahead");workers.setTitle("Audio workers including callback, 1 serial");inputs.setTitle("Physical inputs, one-based channel numbers");outputs.setTitle("Physical outputs, one-based channel numbers");profiles.setTitle("Saved audio profiles");profileName.setTitle("Audio profile name");
        devices.addItem("Offline clock (no sound)",1);
#ifdef MRS_HAS_ASIO
        if(!test){probe=mrs::audio::make_asio_device();infos=probe->enumerate();}
#else
        (void)test;
#endif
        populateDevices();rate.setEditableText(true);buffer.setEditableText(true);
        rate.addItemList({"44100","48000","88200","96000","192000"},1);rate.setText(juce::String(owner.prefs.rate),juce::dontSendNotification);
        buffer.addItemList({"64","128","256","512","1024","2048"},1);buffer.setText(juce::String(owner.prefs.buffer),juce::dontSendNotification);
        processBuffer.addItem("Off (direct DSP)",1);for(auto size:{256,512,1024,2048,4096,8192})processBuffer.addItem(juce::String(size),size);processBuffer.setSelectedId(owner.prefs.process_buffer_frames?static_cast<int>(owner.prefs.process_buffer_frames):1,juce::dontSendNotification);
        workers.addItemList({"1 (serial)","2","3","4","5","6","7","8"},1);workers.setSelectedId(static_cast<int>(owner.prefs.processing_workers),juce::dontSendNotification);
        inputs.setText(owner.view.inputs.isEmpty()&&owner.prefs.monitor_input>=0?juce::String(owner.prefs.monitor_input+1):owner.view.inputs);outputs.setText(channels(owner.prefs.outputs));
        reconnect.setToggleState(owner.prefs.reconnect_audio,juce::dontSendNotification);reconnect.onClick=[this]{owner.run([&]{owner.prefs.reconnect_audio=reconnect.getToggleState();owner.saveSettings();});};
        auto changed=[this]{staged.reset();};devices.onChange=changed;rate.onChange=changed;buffer.onChange=changed;inputs.onTextChange=changed;outputs.onTextChange=changed;
        connect.onClick=[this]{owner.run([&]{
            const int selected=devices.getSelectedId()-2;
            if(selected<0){owner.closeEditors();owner.resetDevice();owner.prefs.reconnect_audio=false;reconnect.setToggleState(false,juce::dontSendNotification);owner.saveSettings();return;}
            auto config=configuration();auto info=infos.at(static_cast<std::size_t>(selected));
            if(config.sample_rate!=owner.project()->sample_rate)throw std::runtime_error("Device rate must match project sample rate");
#ifdef MRS_HAS_ASIO
            auto device=mrs::audio::make_asio_device();auto fresh=device->enumerate();
            auto found=std::find_if(fresh.begin(),fresh.end(),[&](const auto& i){return i.name==info.name;});
            if(found==fresh.end())throw std::runtime_error("Selected ASIO device is no longer available");
            config.device=found->index;mrs::audio::validate_device_config(*found,config);
            if(staged)config=mrs::desktop::resolve_profile(*staged,*found);config.processing_workers=static_cast<std::uint32_t>(workers.getSelectedId());config.process_buffer_frames=processFrames();mrs::audio::validate_device_config(*found,config);
            info=*found;owner.closeEditors();try{owner.app.connect(std::move(device),config);}catch(...){owner.resetDevice();throw;}
#else
            throw std::runtime_error("ASIO is not enabled in this build");
#endif
            owner.view.deviceChannels=deviceLayout(info);owner.prefs.device_name=info.name;owner.prefs.rate=config.sample_rate;owner.prefs.buffer=config.buffer_frames;owner.prefs.processing_workers=config.processing_workers;owner.prefs.process_buffer_frames=config.process_buffer_frames;owner.prefs.outputs=config.outputs;
            owner.prefs.monitor_input=config.inputs.size()==1?config.inputs.front():-1;owner.view.inputs=channels(config.inputs);owner.saveSettings();
        });};
        disconnect.onClick=[this]{owner.run([&]{owner.closeEditors();owner.resetDevice();});};
        panelButton.onClick=[this]{owner.run([&]{const int i=devices.getSelectedId()-2;if(i>=0&&probe)probe->control_panel(infos.at(static_cast<std::size_t>(i)).index);});};
        save.onClick=[this]{owner.run([&]{const int i=devices.getSelectedId()-2;if(i<0)throw std::runtime_error("Select an ASIO device before saving a profile");
            auto profile=mrs::desktop::capture_profile(profileName.getText().trim().toStdString(),infos.at(static_cast<std::size_t>(i)),configuration());
            auto next=owner.prefs;auto found=std::find_if(next.profiles.begin(),next.profiles.end(),[&](const auto& p){return p.name==profile.name;});
            if(found==next.profiles.end())next.profiles.push_back(profile);else *found=profile;next.validate();owner.prefs=std::move(next);owner.saveSettings();populateProfiles(profile.name);
        });};
        load.onClick=[this]{owner.run([&]{const int i=profiles.getSelectedId()-1;if(i<0)throw std::runtime_error("Select an audio profile");
            auto profile=owner.prefs.profiles.at(static_cast<std::size_t>(i));auto found=std::find_if(infos.begin(),infos.end(),[&](const auto& info){return info.name==profile.device_name;});
            if(found==infos.end())throw std::runtime_error("Profile ASIO device is unavailable");auto config=mrs::desktop::resolve_profile(profile,*found);
            devices.setSelectedId(static_cast<int>(found-infos.begin())+2,juce::dontSendNotification);rate.setText(juce::String(config.sample_rate),juce::dontSendNotification);buffer.setText(juce::String(config.buffer_frames),juce::dontSendNotification);
            inputs.setText(channels(config.inputs),false);outputs.setText(channels(config.outputs),false);profileName.setText(label(profile.name),false);staged=profile;
        });};
        remove.onClick=[this]{owner.run([&]{const int i=profiles.getSelectedId()-1;if(i<0)throw std::runtime_error("Select an audio profile");owner.prefs.profiles.erase(owner.prefs.profiles.begin()+i);staged.reset();owner.saveSettings();populateProfiles();});};
        populateProfiles();setSize(680,550);
    }
    static juce::String channels(const std::vector<int>& v){juce::StringArray values;for(int channel:v)values.add(juce::String(channel+1));return values.joinIntoString(",");}
    void populateDevices(){devices.clear(juce::dontSendNotification);devices.addItem("Offline clock (no sound)",1);int selected=1;for(std::size_t i=0;i<infos.size();++i){devices.addItem(label(infos[i].name),static_cast<int>(i)+2);if(infos[i].name==owner.prefs.device_name)selected=static_cast<int>(i)+2;}devices.setSelectedId(selected,juce::dontSendNotification);}
    void populateProfiles(const std::string& selected={}){profiles.clear(juce::dontSendNotification);for(std::size_t i=0;i<owner.prefs.profiles.size();++i){profiles.addItem(label(owner.prefs.profiles[i].name),static_cast<int>(i)+1);if(owner.prefs.profiles[i].name==selected)profiles.setSelectedId(static_cast<int>(i)+1,juce::dontSendNotification);}}
    std::uint32_t processFrames() const {return processBuffer.getSelectedId()==1?0U:static_cast<std::uint32_t>(processBuffer.getSelectedId());}
    mrs::audio::DeviceConfig configuration() const {int i=devices.getSelectedId()-2;if(i<0)throw std::runtime_error("Select an ASIO device");
        const auto parse=[](juce::String text){text=text.trim();if(text.isEmpty()||text.containsOnly("0123456789")==false)throw std::runtime_error("Rate and buffer must be whole positive numbers");auto number=text.getLargeIntValue();if(number<8||number>768000)throw std::runtime_error("Rate or buffer outside supported range");return static_cast<std::uint32_t>(number);};
        mrs::audio::DeviceConfig c{infos.at(static_cast<std::size_t>(i)).index,parse(rate.getText()),parse(buffer.getText()),{},mrs::desktop::parse_outputs(outputs.getText().toStdString()),static_cast<std::uint32_t>(workers.getSelectedId()),processFrames()};
        if(!inputs.getText().trim().isEmpty())c.inputs=mrs::desktop::parse_outputs(inputs.getText().toStdString());mrs::audio::validate_device_config(infos.at(static_cast<std::size_t>(i)),c);return c;
    }
    void resized() override{devices.setBounds(180,20,470,28);rate.setBounds(180,62,180,28);buffer.setBounds(470,62,180,28);inputs.setBounds(180,110,470,28);outputs.setBounds(180,155,470,28);connect.setBounds(20,205,130,30);disconnect.setBounds(160,205,130,30);panelButton.setBounds(300,205,150,30);
        reconnect.setBounds(20,248,450,28);workers.setBounds(555,248,95,28);processBuffer.setBounds(180,290,180,28);profiles.setBounds(180,334,300,28);load.setBounds(490,334,70,28);remove.setBounds(570,334,80,28);profileName.setBounds(180,374,300,28);save.setBounds(490,374,160,28);}
    void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(background));g.setColour(juce::Colours::whitesmoke);g.setFont(owner.theme.font(12));
        g.drawText("Device",20,20,150,28,juce::Justification::left);g.drawText("Sample rate",20,62,150,28,juce::Justification::left);g.drawText("Device buf",380,62,85,28,juce::Justification::left);g.drawText("Inputs (1,2...)",20,110,150,28,juce::Justification::left);g.drawText("Outputs (1,2...)",20,155,150,28,juce::Justification::left);
        g.drawText("Workers",480,248,75,28,juce::Justification::left);g.drawText("Process Buffer",20,290,150,28,juce::Justification::left);g.drawText("Native playback; VST/live use direct DSP.",370,290,280,28,juce::Justification::left);g.drawText("Saved profiles",20,334,150,28,juce::Justification::left);g.drawText("Profile name",20,374,150,28,juce::Justification::left);
        g.drawText("Load stages settings; Connect activates audio. Profiles support one monitor input.",20,426,640,26,juce::Justification::left);g.drawText("Empty inputs = playback only. Device rate must match project rate.",20,456,640,26,juce::Justification::left);
        g.setColour(juce::Colours::orange);g.drawText(owner.message,20,512,640,24,juce::Justification::left);
    }
    Desktop& owner;std::unique_ptr<mrs::audio::IAudioDevice> probe;std::vector<mrs::audio::DeviceInfo> infos;
    std::optional<mrs::desktop::DeviceProfile> staged;
    juce::ComboBox devices,rate,buffer,profiles,workers,processBuffer;juce::TextEditor inputs,outputs,profileName;
    juce::ToggleButton reconnect{"Reconnect saved ASIO device on startup and project open"};
    juce::TextButton connect{"Connect"},disconnect{"Offline"},panelButton{"ASIO control panel"},save{"Save / replace profile"},load{"Load"},remove{"Delete"};
};
void j3AudioSmoke(Desktop& d){
    auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    auto original=d.prefs;auto originalView=d.view;
    AudioPanel audioPanel(d,true);audioPanel.infos={{7,"J3 test interface",{"Mic L","Mic R"},{"Main L","Main R","Cue L","Cue R"},64,1024,128,0}};audioPanel.populateDevices();
    audioPanel.devices.setSelectedId(2,juce::dontSendNotification);audioPanel.rate.setText("48000",juce::dontSendNotification);audioPanel.buffer.setText("128",juce::dontSendNotification);audioPanel.inputs.setText("2",false);audioPanel.outputs.setText("3,4",false);audioPanel.profileName.setText("Guitar",false);audioPanel.save.onClick();
    audioPanel.processBuffer.setSelectedId(1024,juce::dontSendNotification);check(audioPanel.configuration().process_buffer_frames==1024,"Process Buffer setting reaches device config");audioPanel.processBuffer.setSelectedId(1,juce::dontSendNotification);
    audioPanel.workers.setSelectedId(4,juce::dontSendNotification);check(audioPanel.configuration().processing_workers==4,"parallel worker setting reaches device config");
    audioPanel.workers.setSelectedId(1,juce::dontSendNotification);check(audioPanel.configuration().processing_workers==1,"serial reference setting reaches device config");
    {auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("juce-audio-workers-preview.png").createOutputStream();juce::PNGImageFormat format;if(stream){stream->setPosition(0);stream->truncate();format.writeImageToStream(audioPanel.createComponentSnapshot(audioPanel.getLocalBounds(),true,1.f,juce::SoftwareImageType{}),*stream);}}
    check(d.message.isEmpty()&&d.prefs.profiles.size()==original.profiles.size()+1,"audio profile save binding");
    audioPanel.outputs.setText("1,2",false);audioPanel.load.onClick();check(audioPanel.outputs.getText()=="3,4"&&audioPanel.inputs.getText()=="2"&&audioPanel.staged.has_value(),"profile stages mapped channels");
    check(d.app.audio_name()!="J3 test interface","profile load never activates hardware");
    audioPanel.infos.front().outputs[2]="Changed output";audioPanel.load.onClick();check(d.message.contains("labels changed"),"profile changed channel labels rejected");audioPanel.infos.front().outputs[2]="Cue L";
    audioPanel.inputs.setText("1,2",false);audioPanel.save.onClick();check(d.message.contains("one monitor input")&&d.prefs.profiles.size()==original.profiles.size()+1,"multi-input profile rejects without mutation");
    audioPanel.inputs.setText("2",false);audioPanel.buffer.setText("128garbage",juce::dontSendNotification);audioPanel.save.onClick();check(!d.message.isEmpty(),"invalid buffer rejected");
    audioPanel.remove.onClick();check(d.prefs.profiles.size()==original.profiles.size(),"profile delete binding");
    d.prefs=original;d.view=originalView;d.message.clear();
}
void Desktop::audioSettings(){for(auto& existing:windows)if(existing->isVisible()&&existing->getName()=="Audio settings"){existing->toFront(true);return;}auto window=std::make_unique<EditorWindow>("Audio settings",new AudioPanel(*this));window->setLookAndFeel(&theme);windows.push_back(std::move(window));}
}
