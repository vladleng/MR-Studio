#include "Desktop.h"
#include "ReverbKit.h"
#include "PluginPreset.h"
#include "NativeEditorSpace.h"
#include "ReverbBrowser.h"
#include "CabBrowser.h"
#include "ToneBrowser.h"
#include "RigPanel.h"
#include <mrs/recording.hpp>
#include <fstream>
#include <set>
#include <windows.h>

namespace ui {
kit::MRNativeParameterHost& Desktop::parameterHost(){
    if(!controlHost){juce::Component::SafePointer<Desktop> safe(this);controlHost=std::make_unique<kit::MRNativeParameterHost>([safe]{return safe?&safe->app:nullptr;});}
    return *controlHost;
}
void EditorWindow::setOwner(juce::Component& owner){if(getPeer()&&owner.getPeer())SetWindowLongPtrW(static_cast<HWND>(getPeer()->getNativeHandle()),GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(GetAncestor(static_cast<HWND>(owner.getPeer()->getNativeHandle()),GA_ROOT)));}
namespace {
juce::String label(const std::string& s){return juce::String::fromUTF8(s.c_str());}
std::filesystem::path path(const juce::File& f){return std::filesystem::path(f.getFullPathName().toWideCharPointer());}
}
class PresetSelector final : public juce::ComboBox {
public:
    PresetSelector(Desktop& desktop,std::optional<mrs::Id> track,mrs::Id id,juce::File libraryRoot={}):owner(desktop),target(track),slot(id),overrideFolder(std::move(libraryRoot)) {
        if(owner.assetSkin()){auto theme=std::make_shared<Theme>();theme->applyPreview(DesignTokens::target());menuTheme=std::move(theme);}
        setTitle("Current MR Studio preset / preset library");setComponentID("preset-library");setTextWhenNothingSelected("Preset: none");refresh();
    }
    void refresh(){for(const auto& effect:owner.chain(target))if(effect.id==slot){selected=juce::String::fromUTF8(effect.preset_name.c_str());setText(selected.isEmpty()?"Preset: none":selected,juce::dontSendNotification);setTooltip(selected.isEmpty()?"No MR Studio preset selected":"Last MR Studio preset: "+selected+" (plugin edits may change its sound)");}}
    void showPopup() override {
        if(!isEnabled()){hidePopup();return;}if(scanning)return;const bool mouseRequest=isPopupActive();
        owner.run([&]{refresh();for(const auto& effect:owner.chain(target))if(effect.id==slot){folder=overrideFolder==juce::File()?presetFolder(effect):overrideFolder;break;}
            juce::Component::SafePointer<PresetSelector> safe(this);const auto capturedFolder=folder;const auto currentName=selected;scanning=true;
            // Directory traversal is independent of the UI/audio owner; completion is lifetime guarded.
            if(!juce::Thread::launch([safe,capturedFolder,currentName,mouseRequest]{
                juce::Array<juce::File> found;juce::PopupMenu menu;juce::String error;
                try{found=capturedFolder.findChildFiles(juce::File::findFiles,true,"*.mrspreset",juce::File::FollowSymlinks::no);found.sort();if(found.size()>10000)throw std::runtime_error("Preset library exceeds 10000 files");menu=presetMenu(found,capturedFolder,currentName);}catch(const std::exception& e){error=juce::String::fromUTF8(e.what());}
                juce::MessageManager::callAsync([safe,capturedFolder,found,menu,error,mouseRequest]() mutable {if(!safe)return;safe->scanning=false;if(!safe->isShowing()){safe->hidePopup();return;}if(mouseRequest&&!safe->isPopupActive())return;if(error.isNotEmpty()){safe->hidePopup();safe->owner.message=error;safe->owner.refresh();return;}safe->displayMenu(std::move(menu),found,capturedFolder);});
            })){scanning=false;hidePopup();throw std::runtime_error("Cannot start preset library scan");}
        });
    }
private:
    void displayMenu(juce::PopupMenu menu,juce::Array<juce::File> found,juce::File root){
        if(overrideFolder!=juce::File())getProperties().set("mrs-popup-count",static_cast<int>(getProperties()["mrs-popup-count"])+1);
        menu.setLookAndFeel(menuTheme.get());menu.addSeparator();menu.addItem(10001,"New folder...");menu.addItem(10002,"Load file...");
        juce::Component::SafePointer<PresetSelector> safe(this);
        auto options=juce::PopupMenu::Options().withTargetComponent(this).withMinimumWidth(280).withStandardItemHeight(22).withMaximumNumColumns(1);if(overrideFolder!=juce::File())options=options.withParentComponent(&owner);
        menu.showMenuAsync(options,[safe,found,root,theme=menuTheme](int id){if(!safe)return;safe->hidePopup();if(!id)return;
            if(id==10002){safe->owner.loadPreset(safe->target,safe->slot);return;}
            if(id==10001){safe->owner.textDialog("New preset folder (relative path)","Folder",[safe,root](auto name){if(!safe)return;auto directory=presetChild(root,name,false);if(directory.existsAsFile()||directory.createDirectory().failed())throw std::runtime_error("Cannot create preset folder");});return;}
            if(id>0&&id<=found.size())safe->owner.loadPresetFile(safe->target,safe->slot,found[id-1]);
        });
    }
    std::shared_ptr<juce::LookAndFeel> menuTheme{std::make_shared<PresetMenuTheme>()};
    Desktop& owner;std::optional<mrs::Id> target;mrs::Id slot;juce::File folder,overrideFolder;juce::String selected;bool scanning{};
};
void presetSelectorSmoke(Desktop& owner,mrs::Id target,mrs::Id slot,const juce::File& folder){
    auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    auto selector=std::make_unique<PresetSelector>(owner,target,slot,folder);selector->setBounds(12,12,300,24);owner.addAndMakeVisible(*selector);
    auto pump=[](){MSG msg{};int limit=0;while(limit++<200&&PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}std::this_thread::sleep_for(std::chrono::milliseconds(1));};
    for(int attempt=0;attempt<2;++attempt){const auto before=static_cast<int>(selector->getProperties()["mrs-popup-count"]);selector->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));for(int pass=0;pass<1500&&static_cast<int>(selector->getProperties()["mrs-popup-count"])==before;++pass)pump();check(static_cast<int>(selector->getProperties()["mrs-popup-count"])==before+1,"async preset selector popup opens/reopens");
        juce::PopupMenu::dismissAllActiveMenus();for(int pass=0;pass<10;++pass)pump();check(!selector->isPopupActive(),"preset selector clears popup state after dismissal");}
    selector->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));pump();selector.reset();for(int pass=0;pass<200;++pass)pump();
    check(owner.message.isEmpty(),"async preset scan after editor retirement is harmless");
}
class FxPanel final : public juce::Component,private juce::Timer {
public:
    FxPanel(Desktop& d,std::optional<mrs::Id> t,mrs::Id id):owner(d),target(t),slot(id),library(d,t,id){
        projectIdentity=owner.app.services().projects;originalKind=current().kind;
        const auto view=owner.view.nativeEditor(label(slot.value));band=static_cast<std::size_t>(view.band);optionsVisible=view.options;
        addAndMakeVisible(library);
        gain.setComponentID("native-gain");apply.setComponentID("apply-native");
        bypass.setComponentID("plugin-bypass");bypass.getProperties().set("mrHostIcon","power");bypass.setTitle("Plugin power");
        pin.setComponentID("plugin-pin");pin.getProperties().set("mrHostIcon","pin");pin.setTitle("Pin editor");pin.setTooltip("Keep this editor open; remembered after restart");pin.setClickingTogglesState(true);
        pin.setToggleState(owner.view.nativeEditor(label(slot.value)).pinned,juce::dontSendNotification);
        pin.onClick=[this]{owner.run([&]{owner.view.nativeEditor(label(slot.value)).pinned=pin.getToggleState();if(editorWindow)editorWindow->getProperties().set("mrs-pinned",pin.getToggleState());owner.saveSettings();});};
        for(auto* b:{&apply,&bypass,&remove,&up,&down,&enabled,&load,&invert,&savePreset,&loadPreset,&pin})addAndMakeVisible(b);
        for(auto* c:{static_cast<juce::Component*>(&bands),static_cast<juce::Component*>(&parameters),static_cast<juce::Component*>(&presets),static_cast<juce::Component*>(&gain),static_cast<juce::Component*>(&frequency),static_cast<juce::Component*>(&q),static_cast<juce::Component*>(&mix)})addAndMakeVisible(c);
        bands.setComponentID("native-eq-band");bands.addItemList({"HP / Low cut","Band 1","Band 2","Band 3","LP / High cut"},1);bands.setSelectedId(static_cast<int>(band)+1,juce::dontSendNotification);
        presets.addItemList({"Neutral","Warm","Bright"},1);presets.setTextWhenNothingSelected("Preset...");
        bands.onChange=[this]{owner.run([&]{const auto selectedBand=bands.getSelectedId();if(selectedBand<1||selectedBand>5)return;band=static_cast<std::size_t>(selectedBand-1);sync();});};parameters.onChange=[this]{owner.run([&]{sync();});};
        apply.onClick=[this]{owner.run([&]{auto n=current();if(n.kind==mrs::InsertKind::vst3){const int i=parameters.getSelectedId()-1;if(i>=0&&i<static_cast<int>(infos.size()))owner.app.set_plugin_parameter(target,slot,infos[static_cast<std::size_t>(i)].id,gain.getText().getFloatValue());return;}
            if(n.kind==mrs::InsertKind::channel_eq){n.bands[band].gain=gain.getText().getFloatValue();n.bands[band].frequency=frequency.getText().getFloatValue();n.bands[band].q=q.getText().getFloatValue();}
            else {if(gain.isEnabled())n.gain=n.kind==mrs::InsertKind::eq?gain.getText().getFloatValue():std::pow(10.f,gain.getText().getFloatValue()/20);if(frequency.isEnabled()){n.frequency=frequency.getText().getFloatValue();n.q=q.getText().getFloatValue();}if(n.kind==mrs::InsertKind::cab_ir){n.ir.mix=mix.getText().getFloatValue();n.ir.low_cut=n.frequency;n.ir.high_cut=n.q;}}
            commit(n);});};
        savePreset.onClick=[this]{owner.savePreset(target,slot);};loadPreset.onClick=[this]{owner.loadPreset(target,slot);};
        bypass.onClick=[this]{owner.run([&]{auto n=current();n.bypass=!n.bypass;commit(n);});};
        enabled.onClick=[this]{owner.run([&]{auto n=current();n.bands[band].enabled=!n.bands[band].enabled;commit(n);});};
        invert.onClick=[this]{owner.run([&]{auto n=current();n.ir.invert=!n.ir.invert;commit(n);});};
        presets.onChange=[this]{owner.run([&]{auto n=current();const auto p=presets.getSelectedId();n.gain=1;n.ir.mix=1;n.ir.invert=false;n.ir.low_cut=p==2?80.f:p==3?50.f:20.f;n.ir.high_cut=p==2?5000.f:p==3?10000.f:20000.f;commit(n);});};
        juce::Component::SafePointer<FxPanel> safe(this);load.onClick=[safe]{if(safe)safe->owner.choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe](const auto& f){if(safe){auto n=safe->current();n.ir=mrs::audio::load_cab_ir(path(f));safe->commit(n);}},"*.wav");};
        auto structure=[this](int direction){owner.run([&]{auto effects=owner.chain(target);auto i=index(effects);if(direction==0)effects.erase(effects.begin()+static_cast<std::ptrdiff_t>(i));
            else if((direction<0&&i>0)||(direction>0&&i+1<effects.size()))std::swap(effects[i],effects[direction<0?i-1:i+1]);
            if(direction==0)owner.closeEditors();owner.applyChain(target,effects);});};
        remove.onClick=[structure]{structure(0);};up.onClick=[structure]{structure(-1);};down.onClick=[structure]{structure(1);};
        if(current().kind==mrs::InsertKind::rig){
            (void)owner.app.reverb_loader(target,slot,true);for(int i=0;i<getNumChildComponents();++i)getChildComponent(i)->setVisible(false);
            rigPanel=std::make_unique<RigPanel>(owner,target,slot);addAndMakeVisible(*rigPanel);setSize(1060,760);startTimerHz(20);
        }else if(current().kind==mrs::InsertKind::cab_collection){
            (void)owner.app.reverb_loader(target,slot,true);
            cabBrowser=std::make_unique<CabBrowser>(owner.app,target,slot,owner.view,[this]{owner.saveSettings();});cabBrowser->rememberedState=[this]{return current().reverb;};cabBrowser->rememberedMetadata=[this]{return current().cab_metadata;};cabBrowser->setComponentID("cab-browser");addChildComponent(*cabBrowser);
            toneBrowser=std::make_unique<ToneBrowser>(owner.app,target,slot,owner.view,[this]{owner.saveSettings();});addAndMakeVisible(*toneBrowser);showFiles=owner.view.nativeEditor(label(slot.value)).rigFiles;
            auto files=[this](bool raw){showFiles=raw;owner.view.nativeEditor(label(slot.value)).rigFiles=raw;cabBrowser->setVisible(raw);toneBrowser->setVisible(!raw);modelsButton.setVisible(raw);resized();};
            toneBrowser->manageFiles=[files]{files(true);};modelsButton.setComponentID("tone-models");addChildComponent(modelsButton);modelsButton.onClick=[files]{files(false);};files(showFiles);
            for(std::size_t i=0;i<4;++i){auto& s=cabControls[i];addAndMakeVisible(s);addAndMakeVisible(cabLabels[i]);cabLabels[i].setText(i==0?"Gain":i==1?"Mix":i==2?"Low cut":"High cut",juce::dontSendNotification);s.setTitle(i==0?"Gain":i==1?"Mix":i==2?"Low cut":"High cut");s.setComponentID("cab-control-"+juce::String(static_cast<int>(i)));if(i==0)s.setTooltip("Wet output gain after IR and Low/High cut, before Mix. Dry signal is unchanged.");s.setSliderStyle(juce::Slider::LinearHorizontal);s.setTextBoxStyle(juce::Slider::TextBoxRight,false,95,24);s.setRange(i==0?-60.:i==1?0.:20.,i==0?12.0412:i==1?100.:20000.,.01);s.setTextValueSuffix(i==0?" dB":i==1?" %":" Hz");if(i>=2)s.setSkewFactorFromMidPoint(1000);s.setDoubleClickReturnValue(true,i==0?0.:i==1?100.:i==2?20.:20000.);s.setVelocityModeParameters(.25,1,0.,true,juce::ModifierKeys::ctrlModifier);
                s.onDragStart=[this]{drag=current();};s.onValueChange=[this,i]{owner.run([&]{auto n=drag?*drag:current();const auto v=static_cast<float>(cabControls[i].getValue());if(i==0)n.gain=juce::jmin(4.f,std::pow(10.f,v/20));else if(i==1)n.ir.mix=v*.01f;else if(i==2)n.ir.low_cut=juce::jmin(v,n.ir.high_cut-1);else n.ir.high_cut=juce::jmax(v,n.ir.low_cut+1);if(drag){drag=n;auto effects=owner.chain(target);effects[index(effects)]=n;owner.app.preview_inserts(target,effects);}else if(n!=current())commit(n);});};s.onDragEnd=[this]{if(drag){auto n=current();n.gain=drag->gain;n.ir=drag->ir;drag.reset();owner.run([&]{commit(n);});}};
            }setSize(1000,550);startTimerHz(20);
        }else if(current().kind==mrs::InsertKind::reverb){
            options.setComponentID("reverb-options");options.setTitle("MR Reverb options");addAndMakeVisible(options);
            options.onClick=[this]{optionsVisible=!optionsVisible;sync();resized();};
            gate.setComponentID("reverb-gate");gate.setTitle("Input gate");addChildComponent(gate);
            gate.setTooltip("Gate only the wet input; existing reverb tails continue. Disabled by default.");
            gate.onClick=[this]{owner.run([&]{auto n=current();n.reverb.gate_enabled=gate.getToggleState();if(n!=current())commit(n);});};
            reverbBrowser=std::make_unique<ReverbBrowser>(owner.app.reverb_library(),owner.app.reverb_loader(target,slot,true),owner.view,[this]{owner.app.set_reverb_root(path(juce::File(owner.view.reverbRoot)));owner.saveSettings();},label(slot.value));
            reverbBrowser->loaderProvider=[this]{return owner.app.reverb_loader(target,slot);};reverbBrowser->rememberedStateProvider=[this]{return current().reverb;};reverbBrowser->setComponentID("reverb-browser");addAndMakeVisible(*reverbBrowser);
            for(std::size_t i=0;i<4;++i){auto& slider=reverbControls[i];addAndMakeVisible(slider);addAndMakeVisible(reverbLabels[i]);
                const juce::String name=i==0?"Pre-delay":i==1?"Tone":i==2?"Mix":"Threshold";reverbLabels[i].setText(name,juce::dontSendNotification);slider.setTitle(name);slider.setComponentID("reverb-parameter-"+juce::String(static_cast<int>(i==3?4:i)));
                slider.setSliderStyle(juce::Slider::LinearHorizontal);slider.setTextBoxStyle(juce::Slider::TextBoxRight,false,85,24);
                slider.setRange(i==3?-120.:i==1?-6.:0.,i==0?200.:i==1?6.:i==3?-60.:100.,i==3?.1:.01);slider.setTextValueSuffix(i==0?" ms":i==1||i==3?" dB":" %");slider.setDoubleClickReturnValue(true,i==2?25.:i==3?-90.:0.);slider.setMouseDragSensitivity(250);
                if(i==2)slider.setSkewFactorFromMidPoint(25.);
                slider.setTooltip(i==0?"Wet pre-delay (0–200 ms)":i==1?"Wet high-frequency shelf (−6 to +6 dB)":i==3?"Wet input threshold (−120 to −60 dBFS), 0.1 dB steps; Ctrl for fine adjustment.":"Wet mix with finer low-level travel. Midpoint = 25%; set 100% for a send return.");
                slider.setVelocityModeParameters(.25,1,0.,true,juce::ModifierKeys::ctrlModifier);
                slider.onDragStart=[this]{drag=current();};
                slider.onValueChange=[this,i]{owner.run([&]{auto n=current();if(drag){n.reverb.pre_delay=drag->reverb.pre_delay;n.reverb.tone=drag->reverb.tone;n.reverb.mix=drag->reverb.mix;n.reverb.gate_enabled=drag->reverb.gate_enabled;n.reverb.gate_threshold=drag->reverb.gate_threshold;}auto value=static_cast<float>(reverbControls[i].getValue());if(i==0)n.reverb.pre_delay=value;else if(i==1)n.reverb.tone=value;else if(i==2)n.reverb.mix=value*.01f;else n.reverb.gate_threshold=value;
                    if(drag){drag=n;auto effects=owner.chain(target);effects[index(effects)]=n;owner.app.preview_inserts(target,effects);}else if(n!=current())commit(n);});};
                slider.onDragEnd=[this]{if(drag){auto n=current();n.reverb.pre_delay=drag->reverb.pre_delay;n.reverb.tone=drag->reverb.tone;n.reverb.mix=drag->reverb.mix;n.reverb.gate_enabled=drag->reverb.gate_enabled;n.reverb.gate_threshold=drag->reverb.gate_threshold;drag.reset();owner.run([&]{if(n!=current())commit(n);});}};
            }
            if(owner.reverbKitPilot){
                reverbKit=std::make_unique<ReverbKitControls>(owner.parameterHost(),target,slot);addAndMakeVisible(*reverbKit);
                reverbTheme=std::make_unique<Theme>();auto tokens=DesignTokens::target();
                tokens.colours[static_cast<std::size_t>(ColourRole::raised)]=ConceptSkin::channelButton;
                tokens.colours[static_cast<std::size_t>(ColourRole::deep)]=ConceptSkin::channel;
                reverbTheme->applyPreview(tokens);reverbTheme->useConceptAssets(true);setLookAndFeel(reverbTheme.get());
            }
            setSize(reverbKit?1040:900,reverbKit?720:450);startTimerHz(20);
        }else if(!cabBrowser&&!rigPanel)setSize(720,550);
        addAndMakeVisible(pin);bypass.setVisible(true);bypass.toFront(false);pin.toFront(false);setWantsKeyboardFocus(true);startTimerHz(10);sync();
    }
    ~FxPanel() override {if(drag)owner.app.cancel_insert_preview();setLookAndFeel(nullptr);}
    void attachSpace(EditorWindow& window){
        editorWindow=&window;window.getProperties().set("mrs-pinned",pin.getToggleState());
        juce::Component::SafePointer<FxPanel> safe(this);juce::Component::SafePointer<EditorWindow> view(&window);const auto generation=spaceGeneration;
        spaceBridge=std::make_unique<NativeEditorSpace>(window,[safe,view,generation]{juce::MessageManager::callAsync([safe,view,generation]{if(safe&&view&&view->isVisible()&&safe->spaceGeneration==generation){safe->owner.spaceKey(false);safe->owner.spaceKey(true);safe->owner.spaceKey(false);}});});
    }
    void retireSpace(){if(!retired){cancel();auto& view=owner.view.nativeEditor(label(slot.value));view.band=static_cast<int>(band);view.options=optionsVisible;try{owner.saveSettings();}catch(const std::exception& e){owner.message=label(e.what());}retired=true;}++spaceGeneration;spaceBridge.reset();stopTimer();rigPanel.reset();toneBrowser.reset();cabBrowser.reset();reverbKit.reset();reverbBrowser.reset();bands.onChange=nullptr;parameters.onChange=nullptr;presets.onChange=nullptr;}
    std::size_t index(const std::vector<mrs::NativeInsert>& effects) const{for(std::size_t i=0;i<effects.size();++i)if(effects[i].id==slot)return i;throw std::runtime_error("Insert no longer exists");}
    mrs::NativeInsert current() const{auto effects=owner.chain(target);return effects[index(effects)];}
    bool compatible()const{if(retired||originalKind==mrs::InsertKind::vst3||projectIdentity!=owner.app.services().projects)return false;try{return current().kind==originalKind;}catch(const std::exception&){return false;}}
    void commit(const mrs::NativeInsert& n){auto effects=owner.chain(target);effects[index(effects)]=n;owner.applyChain(target,effects);sync();}
    void sync(){if(retired)return;const auto power=current().bypass;bypass.setToggleState(power,juce::dontSendNotification);bypass.setTooltip(power?"Enable plugin processing":"Bypass plugin processing");if(rigPanel){rigPanel->refresh();return;}auto n=current();const bool eq=n.kind==mrs::InsertKind::channel_eq,cab=n.kind==mrs::InsertKind::cab_ir,vst=n.kind==mrs::InsertKind::vst3;
        if(n.kind==mrs::InsertKind::cab_collection){for(auto* c:std::array<juce::Component*,12>{&apply,&enabled,&load,&bands,&parameters,&presets,&gain,&frequency,&q,&mix,&up,&down})c->setVisible(false);if(!drag){cabControls[0].setValue(20*std::log10(juce::jmax(.001f,n.gain)),juce::dontSendNotification);cabControls[1].setValue(n.ir.mix*100,juce::dontSendNotification);cabControls[2].setValue(n.ir.low_cut,juce::dontSendNotification);cabControls[3].setValue(n.ir.high_cut,juce::dontSendNotification);}invert.setButtonText(n.ir.invert?"Polarity -":"Polarity +");bypass.setButtonText(n.bypass?"Enable":"Bypass");return;}
        if(n.kind==mrs::InsertKind::reverb){for(auto* c:std::array<juce::Component*,13>{&apply,&enabled,&load,&invert,&bands,&parameters,&presets,&gain,&frequency,&q,&mix,&up,&down})c->setVisible(false);
            if(!drag){reverbControls[0].setValue(n.reverb.pre_delay,juce::dontSendNotification);reverbControls[1].setValue(n.reverb.tone,juce::dontSendNotification);reverbControls[2].setValue(n.reverb.mix*100,juce::dontSendNotification);reverbControls[3].setValue(n.reverb.gate_threshold,juce::dontSendNotification);gate.setToggleState(n.reverb.gate_enabled,juce::dontSendNotification);}
            gate.setVisible(optionsVisible);reverbControls[3].setVisible(optionsVisible);reverbLabels[3].setVisible(optionsVisible);reverbControls[3].setEnabled(n.reverb.gate_enabled);options.setToggleState(optionsVisible,juce::dontSendNotification);
            if(reverbKit){for(std::size_t i=0;i<4;++i){reverbControls[i].setVisible(false);reverbLabels[i].setVisible(false);}reverbKit->sync(optionsVisible,n.reverb.gate_enabled);}
            bypass.setButtonText(n.bypass?"Enable":"Bypass");repaint();return;}
        gain.setEnabled(n.kind!=mrs::InsertKind::highpass&&n.kind!=mrs::InsertKind::lowpass&&(!eq||(band>0&&band<4)));
        gain.setTooltip(vst?"Selected plugin parameter (normalized 0..1)":eq||n.kind==mrs::InsertKind::eq?"Selected peak EQ band gain in dB; not overall output gain":cab?"Wet output gain after IR/cuts, before Mix; dry is unchanged":n.kind==mrs::InsertKind::gain?"Gain applied to the whole signal at this insert, in dB":"No gain parameter for this filter");
        frequency.setTooltip(cab?"Wet low cut in Hz":"Filter or selected EQ band frequency in Hz");q.setTooltip(cab?"Wet high cut in Hz":"Filter or selected EQ band Q");
        bands.setVisible(eq);enabled.setVisible(eq);load.setVisible(cab);mix.setVisible(cab);presets.setVisible(cab);invert.setVisible(cab);parameters.setVisible(vst);
        frequency.setEnabled(!vst&&n.kind!=mrs::InsertKind::gain);q.setEnabled(frequency.isEnabled());
        if(vst&&infos.empty()){infos=owner.app.plugin_parameters(target,slot);std::erase_if(infos,[](const auto& p){return p.hidden;});for(std::size_t i=0;i<infos.size();++i)parameters.addItem(label(infos[i].name),static_cast<int>(i)+1);if(!infos.empty())parameters.setSelectedId(1,juce::dontSendNotification);}
        float value=eq?n.bands[band].gain:n.kind==mrs::InsertKind::eq?n.gain:20*std::log10(juce::jmax(.000001f,n.gain));
        if(vst){const auto live=owner.app.plugin_parameters(target,slot);int at=parameters.getSelectedId()-1;if(at>=0&&at<static_cast<int>(infos.size())){for(const auto& p:live)if(p.id==infos[static_cast<std::size_t>(at)].id)value=p.initial;apply.setEnabled(infos[static_cast<std::size_t>(at)].automatable);}}
        gain.setText(juce::String(value,4),false);frequency.setText(juce::String(cab?n.ir.low_cut:eq?n.bands[band].frequency:n.frequency,2),false);
        q.setText(juce::String(cab?n.ir.high_cut:eq?n.bands[band].q:n.q,4),false);mix.setText(juce::String(n.ir.mix,3),false);
        enabled.setButtonText(eq&&n.bands[band].enabled?"Band on":"Band off");invert.setButtonText(n.ir.invert?"Polarity -":"Polarity +");bypass.setButtonText(n.bypass?"Enable":"Bypass");repaint();
    }
    void resized() override {pin.setBounds(getWidth()-40,8,28,28);if(rigPanel){rigPanel->setBounds(getLocalBounds().withTrimmedTop(40));bypass.setBounds(12,8,28,28);return;}library.setBounds(20,514,300,24);bypass.setBounds(12,14,28,28);remove.setBounds(118,14,90,28);up.setBounds(216,14,70,28);down.setBounds(294,14,70,28);savePreset.setBounds(380,14,150,28);loadPreset.setBounds(540,14,150,28);
        if(cabBrowser){auto r=getLocalBounds().reduced(12);auto top=r.removeFromTop(28);top.removeFromRight(32);bypass.setBounds(top.removeFromLeft(32));remove.setBounds(top.removeFromLeft(80));savePreset.setBounds(top.removeFromLeft(120));loadPreset.setBounds(top.removeFromLeft(120));library.setBounds(top.removeFromLeft(260));invert.setBounds(top.removeFromLeft(100));modelsButton.setBounds(top.removeFromLeft(90));r.removeFromTop(8);auto row=r.removeFromBottom(65);for(std::size_t i=0;i<cabControls.size();++i){auto cell=row.removeFromLeft(juce::jmax(1,getWidth()/4-9)).reduced(4);cabLabels[i].setBounds(cell.removeFromTop(22));cabControls[i].setBounds(cell);}cabBrowser->setBounds(r);if(toneBrowser)toneBrowser->setBounds(r);return;}
        if(reverbBrowser){auto area=getLocalBounds().reduced(12);auto shell=area.removeFromTop(28);shell.removeFromRight(32);bypass.setBounds(shell.removeFromLeft(32));shell.removeFromLeft(8);remove.setBounds(shell.removeFromLeft(80));shell.removeFromLeft(8);savePreset.setBounds(shell.removeFromLeft(120));shell.removeFromLeft(8);loadPreset.setBounds(shell.removeFromLeft(120));shell.removeFromLeft(8);options.setBounds(shell.removeFromRight(88));shell.removeFromRight(8);library.setBounds(shell);
            if(reverbKit){area.removeFromTop(8);reverbKit->setBounds(area.removeFromBottom(212));if(optionsVisible){auto row=area.removeFromBottom(32).reduced(8,0);gate.setBounds(row.removeFromLeft(150));}reverbBrowser->setBounds(area);return;}
            area.removeFromTop(8);auto controls=area.removeFromBottom(70);int width=controls.getWidth()/3;for(std::size_t i=0;i<3;++i){auto part=controls.removeFromLeft(width).reduced(8,0);reverbLabels[i].setBounds(part.removeFromTop(22));reverbControls[i].setBounds(part.removeFromTop(34));}
            if(optionsVisible){auto row=area.removeFromBottom(44).reduced(8,6);gate.setBounds(row.removeFromLeft(130));reverbLabels[3].setBounds(row.removeFromLeft(80));reverbControls[3].setBounds(row.removeFromLeft(juce::jmin(380,row.getWidth())));}
            reverbBrowser->setBounds(area);return;}
        bands.setBounds(20,335,180,28);enabled.setBounds(210,335,100,28);parameters.setBounds(20,335,460,28);
        load.setBounds(20,335,120,28);presets.setBounds(150,335,125,28);invert.setBounds(285,335,115,28);mix.setBounds(500,335,100,28);
        gain.setBounds(20,402,170,28);frequency.setBounds(215,402,170,28);q.setBounds(410,402,170,28);apply.setBounds(20,447,160,30);}
    juce::Rectangle<float> curve() const{return {24,76,660,234};}
    juce::Point<float> point(const mrs::EqBand& b) const {auto r=curve();return {r.getX()+static_cast<float>(std::log(b.frequency/20.)/std::log(1000.))*r.getWidth(),r.getCentreY()-b.gain/48.f*r.getHeight()};}
    void paint(juce::Graphics& g) override{drawCard(g,getLocalBounds().toFloat(),reverbTheme?DesignTokens::target().colour(ColourRole::window):juce::Colour(panel));g.setColour(juce::Colours::lightgrey);g.setFont(Theme::detailFont());
        if(retired||reverbBrowser||cabBrowser||rigPanel)return;
        // A queued paint can outlive insert removal and the next editor-generation tick.
        std::optional<mrs::NativeInsert> visible;try{visible=current();}catch(const std::exception&){return;}
        const auto& shown=*visible;g.drawText(shown.kind==mrs::InsertKind::vst3?"Parameter (0..1)":shown.kind==mrs::InsertKind::cab_ir?"Wet output gain (dB)":shown.kind==mrs::InsertKind::channel_eq||shown.kind==mrs::InsertKind::eq?"Band gain (dB)":shown.kind==mrs::InsertKind::highpass||shown.kind==mrs::InsertKind::lowpass?"Gain: not applicable":"Gain (dB)",20,374,180,24,juce::Justification::left);g.drawText(shown.kind==mrs::InsertKind::cab_ir?"Low cut (Hz)":"Frequency (Hz)",215,374,180,24,juce::Justification::left);g.drawText(shown.kind==mrs::InsertKind::cab_ir?"High cut (Hz)":"Q",410,374,180,24,juce::Justification::left);
        g.drawText("Live native parameters; structural edits and project save after Pause / Stop",20,483,680,24,juce::Justification::left);
        auto n=drag?*drag:*visible;drawCard(g,curve());
        if(n.kind==mrs::InsertKind::channel_eq){g.setColour(juce::Colour(separatorColour));for(int i=0;i<5;++i)g.drawHorizontalLine(static_cast<int>(curve().getY()+i*curve().getHeight()/4),curve().getX(),curve().getRight());
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
    void cancel(){if(reverbKit)reverbKit->cancel();if(rigPanel)rigPanel->cancelGesture();if(drag){drag.reset();owner.app.cancel_insert_preview();repaint();}}
private:
    void timerCallback() override {try{sync();}catch(const std::exception&){stopTimer();}}
    std::unique_ptr<Theme> reverbTheme;
    std::unique_ptr<RigPanel> rigPanel;
    std::unique_ptr<CabBrowser> cabBrowser;std::unique_ptr<ToneBrowser> toneBrowser;juce::TextButton modelsButton{"Models"};bool showFiles{};std::array<juce::Slider,4> cabControls;std::array<juce::Label,4> cabLabels;
    std::unique_ptr<ReverbBrowser> reverbBrowser;
    std::unique_ptr<ReverbKitControls> reverbKit;
    std::array<juce::Slider,4> reverbControls;
    std::array<juce::Label,4> reverbLabels;
    juce::TextButton options{"Options..."};juce::ToggleButton gate{"Input gate"};bool optionsVisible{},retired{};
    Desktop& owner;std::optional<mrs::Id> target;mrs::Id slot;std::size_t band{1};
    std::shared_ptr<mrs::IProjectStore> projectIdentity;mrs::InsertKind originalKind{mrs::InsertKind::gain};
    std::unique_ptr<NativeEditorSpace> spaceBridge;std::uint64_t spaceGeneration{};
    std::optional<mrs::NativeInsert> drag;juce::Point<float> last;
    std::vector<mrs::processing::ParameterInfo> infos;
    juce::Component::SafePointer<EditorWindow> editorWindow;
    juce::TextButton pin{"Pin"},savePreset{"Save preset..."},loadPreset{"Load preset..."};
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
    juce::Component::SafePointer<Desktop> safe(this);const auto store=app.services().projects;
    textDialog("Save preset (name or Folder/Name)","Preset",[safe,bytes,folder,target,slot,store](auto name){if(!safe)return;
        if(safe->app.services().projects!=store)throw std::runtime_error("Project changed; save the preset again");
        auto effects=safe->chain(target);if(std::none_of(effects.begin(),effects.end(),[&](const auto& n){return n.id==slot;}))throw std::runtime_error("Insert no longer exists");
        auto file=presetChild(folder,name,true);if(file.exists())throw std::runtime_error("Preset name already exists; choose a new name");
        if(file.getParentDirectory().createDirectory().failed())throw std::runtime_error("Cannot create plugin preset folder");
        publishSettings(file,bytes);safe->app.name_insert_preset(target,slot,presetLabel(file,folder).toStdString());safe->refreshPresetLists();safe->refresh();
    });
});}
void Desktop::loadPresetFile(std::optional<mrs::Id> target,mrs::Id slot,const juce::File& file){run([&]{
    if(!file.existsAsFile()||file.getSize()>8*1024*1024)throw std::runtime_error("Plugin preset missing or exceeds size limit");
    auto effects=chain(target);auto at=std::find_if(effects.begin(),effects.end(),[&](const auto& n){return n.id==slot;});if(at==effects.end())throw std::runtime_error("Insert no longer exists");
    auto preset=decodePreset(file.loadFileAsString(),*at);preset.preset_name=presetLabel(file,presetFolder(*at)).toStdString();
    if(app.engine()->state().playback==mrs::PlaybackState::playing||app.recording())throw std::runtime_error("Pause/Stop before loading a plugin preset");
    const auto key=label((target?target->value:"master")+":"+slot.value);
    const bool nativeView=preset.kind==mrs::InsertKind::vst3,keepHostView=!nativeView&&std::any_of(windows.begin(),windows.end(),[&](const auto& window){return window->isVisible()&&window->getProperties()["mrs-slot"].toString()==key&&dynamic_cast<FxPanel*>(window->getContentComponent());});
    if(keepHostView){
        for(auto& window:windows)if(window->isVisible()&&window->getProperties()["mrs-slot"].toString()==key)if(auto* fxView=dynamic_cast<FxPanel*>(window->getContentComponent()))fxView->cancel();
        // The generic preset transaction may rebuild the graph. Detach external
        // plugin editors before that replacement, while retaining this host-only UI.
        app.engine()->set_profiling(false);app.close_plugin_editors();
        for(auto& window:windows)if(window->getProperties()["mrs-slot"].toString()!=key){retirePluginWindow(*window);window->setVisible(false);if(static_cast<bool>(window->getProperties()["mrs-native-editor"]))window->removeFromDesktop();}
    }
    if(!nativeView&&!keepHostView)closeEditors();app.load_insert_preset(target,std::move(preset));editorGeneration=app.insert_generation();refresh(true);
    if(!nativeView&&!keepHostView)openInsert(target,slot);
    if(keepHostView)for(auto& window:windows)if(window->isVisible()&&window->getProperties()["mrs-slot"].toString()==key)if(auto* fxView=dynamic_cast<FxPanel*>(window->getContentComponent()))fxView->sync();
    refreshPresetLists();
});}
void Desktop::loadPreset(std::optional<mrs::Id> target,mrs::Id slot){juce::Component::SafePointer<Desktop> safe(this);const auto store=app.services().projects;
    choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,target,slot,store](auto file){if(safe&&safe->app.services().projects==store)safe->loadPresetFile(target,slot,file);},"*.mrspreset");
}
void Desktop::applyInsertChoice(std::optional<mrs::Id> target,mrs::Id slot,int choice){
    if(target){const auto p=project();if(std::none_of(p->tracks.begin(),p->tracks.end(),[&](const auto& t){return t.id==target;}))return;}
    auto effects=chain(target);auto at=std::find_if(effects.begin(),effects.end(),[&](const auto& n){return n.id==slot;});
    if(at==effects.end())return;
    if(choice==2){openInsert(target,slot);return;}
    if(choice==3){at->bypass=!at->bypass;applyChain(target,std::move(effects));return;}
    if(choice==1){if(app.recording()||app.engine()->state().playback==mrs::PlaybackState::playing)return;effects.erase(at);closeEditors();applyChain(target,std::move(effects));}
}
void Desktop::insertContextMenu(std::optional<mrs::Id> target,mrs::Id slot,juce::Point<int> point){
    const auto effects=chain(target);const auto at=std::find_if(effects.begin(),effects.end(),[&](const auto& n){return n.id==slot;});if(at==effects.end())return;
    juce::PopupMenu insertPopup;insertPopup.setLookAndFeel(&theme);
    insertPopup.addItem(1,"Remove",!app.recording()&&app.engine()->state().playback!=mrs::PlaybackState::playing);
    insertPopup.addItem(2,"Editor");insertPopup.addItem(3,"Bypass",!app.recording(),at->bypass);
    const auto store=app.services().projects;juce::Component::SafePointer<Desktop> safe(this);
    insertPopup.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea({point.x,point.y,1,1}),[safe,store,target,slot](int choice){
        if(!safe||!choice||safe->app.services().projects!=store)return;
        safe->run([&]{safe->applyInsertChoice(target,slot,choice);});
    });
}
void Desktop::insertMenu(std::optional<mrs::Id> target){juce::PopupMenu m;m.setLookAndFeel(&theme);m.addItem(1,"Add Gain");m.addItem(2,"Add Channel EQ");m.addItem(3,"Add MR Rig");m.addItem(4,"Add MR Reverb");
    const auto existing=chain(target);for(std::size_t i=0;i<existing.size();++i)m.addItem(1000+static_cast<int>(i),"Edit insert "+juce::String(static_cast<int>(i)+1));
    m.addSeparator();auto vendors=pluginMenu();for(juce::PopupMenu::MenuItemIterator it(vendors);it.next();)m.addItem(it.getItem());
    juce::Component::SafePointer<Desktop> safe(this);m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe,target](int n){if(!safe||!n)return;safe->run([&]{
        if(n>=10000){safe->addPlugin(target,static_cast<std::size_t>(n-10000));return;}if(n>=1000){auto effects=safe->chain(target);safe->openInsert(target,effects.at(static_cast<std::size_t>(n-1000)).id);return;}

        auto effects=safe->chain(target);mrs::NativeInsert effect;effect.id=mrs::new_id();effect.kind=n==3?mrs::InsertKind::rig:n==1?mrs::InsertKind::gain:n==4?mrs::InsertKind::reverb:mrs::InsertKind::channel_eq;effects.push_back(effect);safe->closeEditors();safe->applyChain(target,effects);safe->openInsert(target,effect.id);
    });});}
class PluginPanel final : public juce::Component,private juce::Timer {
public:
    PluginPanel(Desktop& d,std::optional<mrs::Id> t,mrs::Id id):owner(d),target(t),slot(id),library(d,t,id){
        addAndMakeVisible(library);
        setTitle("Plugin editor and insert controls");setSize(760,520);
        for(auto* button:{&save,&load,&bypass,&remove,&up,&down,&parameters,&pin}){addAndMakeVisible(button);button->setTitle(button->getButtonText());}
        if(owner.assetSkin()){
            auto tokens=DesignTokens::target();tokens.colours[static_cast<std::size_t>(ColourRole::raised)]=ConceptSkin::channelButton;
            chromeTheme.applyPreview(tokens);chromeTheme.useConceptAssets(true);
            for(auto* button:{&save,&load,&bypass,&remove,&up,&down,&parameters,&pin})button->setLookAndFeel(&chromeTheme);
            library.setLookAndFeel(&chromeTheme);
        }
        bypass.setComponentID("plugin-bypass");bypass.getProperties().set("mrHostIcon","power");pin.setComponentID("plugin-pin");pin.getProperties().set("mrHostIcon","pin");pin.setClickingTogglesState(true);pin.setTooltip("Keep this editor open; remembered after restart");pin.setToggleState(owner.view.nativeEditor(label(slot.value)).pinned,juce::dontSendNotification);pin.onClick=[this]{owner.run([&]{owner.view.nativeEditor(label(slot.value)).pinned=pin.getToggleState();if(window)window->getProperties().set("mrs-pinned",pin.getToggleState());owner.saveSettings();});};
        save.onClick=[this]{owner.savePreset(target,slot);};load.onClick=[this]{owner.loadPreset(target,slot);};
        bypass.onClick=[this]{owner.run([&]{auto effects=owner.chain(target);auto at=find(effects);at->bypass=!at->bypass;owner.applyChain(target,effects);sync();});};
        auto structure=[this](int direction){owner.run([&]{auto effects=owner.chain(target);auto at=find(effects);auto pos=at-effects.begin();
            if(direction==0)effects.erase(at);else if((direction<0&&pos>0)||(direction>0&&pos+1<static_cast<std::ptrdiff_t>(effects.size())))std::iter_swap(at,at+direction);
            if(direction==0)owner.closeEditors();owner.applyChain(target,effects);});};
        remove.onClick=[structure]{structure(0);};up.onClick=[structure]{structure(-1);};down.onClick=[structure]{structure(1);};
        parameters.onClick=[this]{auto* content=new FxPanel(owner,target,slot);auto window=std::make_unique<EditorWindow>("Plugin parameters",content,false);window->setLookAndFeel(&owner.theme);window->setOwner(owner);content->attachSpace(*window);window->onClose=[content]{content->retireSpace();};window->setVisible(true);owner.windows.push_back(std::move(window));};sync();
    }
    ~PluginPanel() override {retire();for(auto* button:{&save,&load,&bypass,&remove,&up,&down,&parameters,&pin})button->setLookAndFeel(nullptr);library.setLookAndFeel(nullptr);}
    void retire(){++spaceGeneration;spaceBridge.reset();stopTimer();if(host&&IsWindow(host))DestroyWindow(host);host=nullptr;}
    auto find(std::vector<mrs::NativeInsert>& effects){auto at=std::find_if(effects.begin(),effects.end(),[&](const auto& n){return n.id==slot;});if(at==effects.end())throw std::runtime_error("Insert no longer exists");return at;}
    void sync(){auto effects=owner.chain(target);const bool inactive=find(effects)->bypass;bypass.setButtonText(inactive?"Enable":"Bypass");bypass.setToggleState(inactive,juce::dontSendNotification);bypass.setTooltip(inactive?"Insert bypassed — enable processing":"Insert active — bypass processing");repaint();}
    bool attach(EditorWindow& w){window=&w;w.getProperties().set("mrs-pinned",pin.getToggleState());host=CreateWindowExW(0,L"STATIC",L"MR Studio plugin host",WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN,0,50,640,480,static_cast<HWND>(w.getPeer()->getNativeHandle()),nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!host)throw std::runtime_error("Cannot create native plugin editor host");
        if(!owner.app.open_plugin_editor(target,slot,host,nativeWidth,nativeHeight))return false;
        juce::Component::SafePointer<PluginPanel> safe(this);
        const auto generation=spaceGeneration;
        spaceBridge=std::make_unique<NativeEditorSpace>(w,[safe,generation]{juce::MessageManager::callAsync([safe,generation]{if(safe&&safe->spaceGeneration==generation&&safe->host&&IsWindow(safe->host)&&safe->window&&safe->window->isVisible()){safe->owner.spaceKey(false);safe->owner.spaceKey(true);safe->owner.spaceKey(false);}});});
        w.getProperties().set("mrs-native-host",static_cast<juce::int64>(reinterpret_cast<std::intptr_t>(host)));resizeWindow();startTimerHz(10);return true;
    }
    void reconnect(EditorWindow& w){try{if(!attach(w))throw std::runtime_error("Cannot reopen native plugin editor");sync();}catch(...){owner.app.close_plugin_editor(target,slot);retire();throw;}}
    void resizeWindow(){const auto scale=window->getPeer()->getPlatformScaleFactor();window->fitNativeEditor(juce::jmax(nativeWidth,juce::roundToInt(850*scale)),nativeHeight+juce::roundToInt(toolbar*scale));resized();}
    void resized() override {bypass.setBounds(8,8,28,30);pin.setBounds(getWidth()-40,8,28,30);library.setBounds(8,46,juce::jmin(300,getWidth()-16),24);int x=44;for(auto* button:{&save,&load,&up,&down,&remove,&parameters}){int width=button==&save||button==&load?124:button==&parameters?112:button==&up||button==&down?48:80;button->setBounds(x,8,width,30);x+=width+6;}
        if(host&&window&&window->getPeer()){auto scale=window->getPeer()->getPlatformScaleFactor();SetWindowPos(host,nullptr,0,juce::roundToInt(toolbar*scale),nativeWidth,nativeHeight,SWP_NOZORDER|SWP_NOACTIVATE);}
    }
    void paint(juce::Graphics& g) override {
        drawCard(g,getLocalBounds().toFloat(),chromeTheme.conceptSkin()?DesignTokens::target().colour(ColourRole::window):juce::Colour(panel));
        if(chromeTheme.conceptSkin()){g.setFont(Theme::stripFont());g.setColour(juce::Colour(secondaryText));
            g.drawText(bypass.getToggleState()?"BYPASSED":"ACTIVE",320,46,150,24,juce::Justification::centredLeft);}
    }
private:
    void timerCallback() override {if(!host||!IsWindow(host)||!window||!window->isVisible())return;try{sync();}catch(const std::exception&){stopTimer();return;}RECT rect{};if(GetClientRect(host,&rect)&&rect.right>0&&rect.bottom>0&&(rect.right!=nativeWidth||rect.bottom!=nativeHeight)){nativeWidth=rect.right;nativeHeight=rect.bottom;resizeWindow();}}
    Desktop& owner;std::optional<mrs::Id> target;mrs::Id slot;HWND host{};EditorWindow* window{};int nativeWidth{640},nativeHeight{480};static constexpr int toolbar=86;
    std::unique_ptr<NativeEditorSpace> spaceBridge;
    std::uint64_t spaceGeneration{};
    Theme chromeTheme;
    PresetSelector library;
    juce::TextButton save{"Save preset..."},load{"Load preset..."},bypass{"Bypass"},up{"Up"},down{"Down"},remove{"Remove"},parameters{"Parameters"},pin{"Pin"};
};
// Exercise the real host toolbar against an owned gain fixture, without a vendor
// HWND. Existing J3 independently covers actual synthetic VST3 editor lifetime.
void pluginChromeSmoke(Desktop& owner,std::optional<mrs::Id> target,mrs::Id slot){
    PluginPanel content(owner,target,slot);content.setSize(850,520);
    auto* bypass=dynamic_cast<juce::TextButton*>(content.findChildWithID("plugin-bypass"));
    auto* pin=dynamic_cast<juce::TextButton*>(content.findChildWithID("plugin-pin"));
    if(!bypass||!pin||&bypass->getLookAndFeel()==&owner.theme)throw std::runtime_error("P3 scoped host toolbar Theme");
    const auto before=owner.chain(target);bypass->onClick();
    if(!owner.chain(target).front().bypass||!bypass->getToggleState())throw std::runtime_error("P3 host bypass state");
    if(!owner.app.undo())throw std::runtime_error("P3 host bypass Undo");owner.refresh();content.sync();
    if(owner.chain(target)!=before||bypass->getToggleState())throw std::runtime_error("P3 host bypass exact restore");
    auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("p3-plugin-toolbar.png").createOutputStream();
    if(!stream||!juce::PNGImageFormat().writeImageToStream(content.createComponentSnapshot(content.getLocalBounds()),*stream))throw std::runtime_error("P3 toolbar screenshot");
}
void reconnectPluginWindow(EditorWindow& window){if(auto* content=dynamic_cast<PluginPanel*>(window.getContentComponent()))content->reconnect(window);}
void retirePluginWindow(EditorWindow& window){if(auto* content=dynamic_cast<PluginPanel*>(window.getContentComponent()))content->retire();if(auto* content=dynamic_cast<FxPanel*>(window.getContentComponent()))content->retireSpace();}
void Desktop::refreshInsertEditors(){
    app.engine()->set_profiling(false);app.close_plugin_editors();
    for(auto& window:windows){if(static_cast<bool>(window->getProperties()["mrs-midi-clip"]))continue;
        // ARA sessions own their processors independently of the insert host.
        // Rebuilding ordinary inserts must not retire their editor/Space bridge.
        // Project replacement and explicit close still use closeEditors().
        if(window->getProperties().contains("mrs-ara-source"))continue;
        if(auto* content=dynamic_cast<FxPanel*>(window->getContentComponent());window->isVisible()&&content&&content->compatible()){content->cancel();content->sync();continue;}
        retirePluginWindow(*window);window->setVisible(false);if(static_cast<bool>(window->getProperties()["mrs-native-editor"]))window->removeFromDesktop();
    }
}
void Desktop::openInsert(std::optional<mrs::Id> target,mrs::Id slot){run([&]{auto effects=chain(target);auto found=std::find_if(effects.begin(),effects.end(),[&](const auto& n){return n.id==slot;});if(found==effects.end())return;
    const auto key=label((target?target->value:"master")+":"+slot.value);for(auto& w:windows)if(w->isVisible()&&w->getProperties()["mrs-slot"].toString()==key){w->toFront(true);return;}
    closeUnpinnedEditors();editorGeneration=app.insert_generation();if(found->kind==mrs::InsertKind::vst3){auto* content=new PluginPanel(*this,target,slot);auto window=std::make_unique<EditorWindow>(label(found->plugin_name),content,false);window->setLookAndFeel(&theme);
        bool attached=false;try{attached=content->attach(*window);}catch(...){app.close_plugin_editor(target,slot);content->retire();throw;}
        if(attached){window->setOwner(*this);window->setVisible(true);window->getProperties().set("mrs-slot",key);window->getProperties().set("mrs-native-editor",true);window->onClose=[this,content,target,slot]{app.close_plugin_editor(target,slot);content->retire();};windows.push_back(std::move(window));editorGeneration=app.insert_generation();return;}}
    const auto title=found->kind==mrs::InsertKind::rig?"MR Rig":found->kind==mrs::InsertKind::cab_collection?"MR Rig Models":found->kind==mrs::InsertKind::reverb?"MR Reverb":found->kind==mrs::InsertKind::channel_eq?"Channel EQ":found->kind==mrs::InsertKind::gain?"Gain":found->kind==mrs::InsertKind::highpass?"High pass":found->kind==mrs::InsertKind::lowpass?"Low pass":found->kind==mrs::InsertKind::eq?"EQ":found->kind==mrs::InsertKind::cab_ir?"Legacy Cab IR":"Plugin parameters";
    auto* content=new FxPanel(*this,target,slot);auto fx=std::make_unique<EditorWindow>(title,content,false);fx->setLookAndFeel(&theme);fx->onClose=[content]{content->retireSpace();};fx->getProperties().set("mrs-slot",key);windows.push_back(std::move(fx));editorGeneration=app.insert_generation();
    if(found->kind==mrs::InsertKind::rig){auto& window=*windows.back();window.setResizable(true,false);window.setResizeLimits(940,640,1800,1200);if(const auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds())){const auto area=display->userArea.reduced(12);auto bounds=window.getBounds();bounds.setSize(juce::jmin(bounds.getWidth(),area.getWidth()),juce::jmin(bounds.getHeight(),area.getHeight()));window.setBounds(bounds.constrainedWithin(area));}}
    if(found->kind==mrs::InsertKind::cab_collection){auto& window=*windows.back();window.setName("MR Rig Models");window.setResizable(true,false);window.setResizeLimits(1000,500,1600,1100);}
    if(found->kind==mrs::InsertKind::reverb){auto& window=*windows.back();window.setName(reverbKitPilot?"MR Reverb / UDS Preview #207":"MR Reverb");window.setResizable(true,false);window.setResizeLimits(reverbKitPilot?900:760,reverbKitPilot?650:390,1500,1000);}
    windows.back()->setOwner(*this); // setResizable may recreate the native peer and discard its owner HWND.
    content->attachSpace(*windows.back());windows.back()->setVisible(true);
});}

void nativeEditorSmoke(){
    const auto check=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    Desktop owner(true);owner.setSize(1200,700);owner.addToDesktop(0);owner.setVisible(true);const auto track=owner.project()->tracks.front().id;
    const auto temporary=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mrs-native-editors-"+label(mrs::new_id().value));check(!temporary.exists()&&temporary.createDirectory().wasOk(),"native editor fixtures");struct Cleanup{juce::File folder;~Cleanup(){folder.deleteRecursively();}}cleanup{temporary};
    auto pump=[](){MSG msg{};int limit{};while(limit++<200&&PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}std::this_thread::sleep_for(std::chrono::milliseconds(1));};
    for(auto kind:{mrs::InsertKind::gain,mrs::InsertKind::highpass,mrs::InsertKind::lowpass,mrs::InsertKind::eq,mrs::InsertKind::channel_eq,mrs::InsertKind::cab_ir,mrs::InsertKind::reverb,mrs::InsertKind::cab_collection}){
        const auto stage=[&](const juce::String& step){juce::File::getCurrentWorkingDirectory().getChildFile("native-editor-stage.txt").replaceWithText(juce::String(static_cast<int>(kind))+": "+step);};stage("opening");
        owner.closeEditors();mrs::NativeInsert effect;effect.id=mrs::new_id();effect.kind=kind;if(kind==mrs::InsertKind::cab_ir){effect.ir.name="Synthetic";effect.ir.sample_rate=48000;effect.ir.samples={1};}
        owner.applyChain(track,{effect});owner.openInsert(track,effect.id);check(owner.message.isEmpty(),"native editor opens");auto* window=owner.windows.back().get();auto* content=window->getContentComponent();const auto hwnd=static_cast<HWND>(window->getPeer()->getNativeHandle());
        if(kind==mrs::InsertKind::highpass||kind==mrs::InsertKind::lowpass)check(!content->findChildWithID("native-gain")->isEnabled(),"filter exposes only DSP-backed controls");
        auto* editorPin=dynamic_cast<juce::TextButton*>(content->findChildWithID("plugin-pin"));auto* editorPower=content->findChildWithID("plugin-bypass");check(editorPin&&editorPower,"#210 native host exposes power and pin");
        editorPin->setToggleState(true,juce::dontSendNotification);editorPin->onClick();check(ViewSettings::decode(owner.view.encode()).nativeEditorViews.at(label(effect.id.value)).pinned,"#210 native pin preference roundtrip");
        check(GetWindow(hwnd,GW_OWNER)==owner.getPeer()->getNativeHandle()&&!(GetWindowLongPtrW(hwnd,GWL_EXSTYLE)&WS_EX_TOPMOST),"native owner, no global topmost");
        SetWindowPos(static_cast<HWND>(owner.getPeer()->getNativeHandle()),HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);bool above=false;for(auto h=GetWindow(static_cast<HWND>(owner.getPeer()->getNativeHandle()),GW_HWNDPREV);h;h=GetWindow(h,GW_HWNDPREV))if(h==hwnd)above=true;check(above,"native editor stays above Studio");
        const auto count=owner.windows.size();owner.openInsert(track,effect.id);check(owner.windows.size()==count&&owner.windows.back().get()==window,"repeat open raises same editor");
        window->setTopLeftPosition(window->getPosition()+juce::Point<int>(17,13));const auto bounds=window->getBounds();
        struct Probe final:juce::ComponentListener{juce::Component::SafePointer<juce::Component> observed;int changes{};explicit Probe(juce::Component& c):observed(&c){c.addComponentListener(this);}~Probe()override{if(observed)observed->removeComponentListener(this);}void componentVisibilityChanged(juce::Component&)override{++changes;}}probe(*window);
        for(bool alternate:{true,false,true}){
            stage(alternate?"preset A":"preset B");
            auto preset=owner.chain(track).front();const auto oldBypass=preset.bypass;preset.bypass=alternate;
            if(kind==mrs::InsertKind::reverb)preset.reverb.mix=alternate?.35f:.65f;else if(kind==mrs::InsertKind::cab_ir||kind==mrs::InsertKind::cab_collection)preset.ir.mix=alternate?.35f:.65f;else if(kind==mrs::InsertKind::channel_eq)preset.bands[1].gain=alternate?-6.f:3.f;else if(kind==mrs::InsertKind::highpass||kind==mrs::InsertKind::lowpass)preset.frequency=alternate?500.f:4000.f;else preset.gain=kind==mrs::InsertKind::eq?(alternate?-6.f:3.f):(alternate?.25f:.75f);
            const auto file=temporary.getChildFile("native.mrspreset");check(file.replaceWithText(encodePreset(preset)),"native preset fixture");owner.loadPresetFile(track,effect.id,file);for(int i=0;i<40;++i)pump();
            const auto active=owner.chain(track).front();const auto expectedBypass=kind==mrs::InsertKind::reverb||kind==mrs::InsertKind::cab_collection?alternate:oldBypass;
            const auto parameterValue=[](const mrs::NativeInsert& n){if(n.kind==mrs::InsertKind::reverb)return n.reverb.mix;if(n.kind==mrs::InsertKind::cab_ir||n.kind==mrs::InsertKind::cab_collection)return n.ir.mix;if(n.kind==mrs::InsertKind::channel_eq)return n.bands[1].gain;if(n.kind==mrs::InsertKind::highpass||n.kind==mrs::InsertKind::lowpass)return n.frequency;return n.gain;};
            if(owner.message.isNotEmpty()||active.bypass!=expectedBypass||std::abs(parameterValue(active)-parameterValue(preset))>.0001f)throw std::runtime_error("native preset kind="+std::to_string(static_cast<int>(kind))+" error="+owner.message.toStdString());
            check(window->isVisible()&&window->getContentComponent()==content&&window->getPeer()->getNativeHandle()==hwnd&&window->getBounds()==bounds&&probe.changes==0,"native preset retains window/content/HWND/bounds without hide/show after timer");
            if(kind==mrs::InsertKind::gain||kind==mrs::InsertKind::eq||kind==mrs::InsertKind::channel_eq){const auto value=dynamic_cast<juce::TextEditor*>(content->findChildWithID("native-gain"))->getText().getFloatValue();const auto wanted=kind==mrs::InsertKind::gain?20*std::log10(active.gain):kind==mrs::InsertKind::eq?active.gain:active.bands[1].gain;check(std::abs(value-wanted)<.001f,"preset updates native parameter controls in place");}
        }
        stage("close/reopen");if(kind==mrs::InsertKind::channel_eq){auto* band=dynamic_cast<juce::ComboBox*>(content->findChildWithID("native-eq-band"));check(band!=nullptr,"EQ band chooser");band->setSelectedId(5,juce::sendNotificationSync);}
        const auto revision=owner.app.services().projects->state().revision;window->closeButtonPressed();const auto settings=temporary.getChildFile("view.json");publishSettings(settings,owner.view.encode());owner.view=ViewSettings::decode(settings.loadFileAsString());owner.openInsert(track,effect.id);check(owner.app.services().projects->state().revision==revision,"UI close/reopen does not edit project");
        if(kind==mrs::InsertKind::channel_eq)check(dynamic_cast<juce::ComboBox*>(owner.windows.back()->getContentComponent()->findChildWithID("native-eq-band"))->getSelectedId()==5,"EQ band survives close and settings file roundtrip");
        stage("passed");
    }
    owner.closeEditors();check(owner.message.isEmpty(),"native editor lifetime safe");
    auto legacy=juce::JSON::parse(owner.view.encode());legacy.getDynamicObject()->removeProperty("nativeEditorViews");check(ViewSettings::decode(juce::JSON::toString(legacy)).nativeEditorViews.empty(),"legacy preferences without native views");
    check(owner.view.nativeEditor(label(mrs::new_id().value)).band==1,"independent native editor view");
}
void cabEditorSmoke(){
    auto check=[](bool b,const char* s){if(!b)throw std::runtime_error(s);};
    Desktop owner(true);owner.setSize(1200,700);owner.addToDesktop(0);owner.setVisible(true);const auto track=owner.project()->tracks.front().id;mrs::NativeInsert fx;fx.id=mrs::new_id();fx.kind=mrs::InsertKind::cab_collection;owner.applyChain(track,{fx});owner.openInsert(track,fx.id);
    auto* content=owner.windows.back()->getContentComponent();auto* browser=dynamic_cast<CabBrowser*>(content->findChildWithID("cab-browser"));check(browser!=nullptr,"Cab browser");
    auto* mix=dynamic_cast<juce::Slider*>(content->findChildWithID("cab-control-1"));check(mix!=nullptr,"Cab mix control");
    auto pump=[](){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}std::this_thread::sleep_for(std::chrono::milliseconds(1));};
    const auto temporary=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mrs-cab-gui-"+label(mrs::new_id().value));check(!temporary.exists(),"Cab temporary scope");temporary.createDirectory();struct Cleanup{juce::File f;~Cleanup(){f.deleteRecursively();}}cleanup{temporary};
    for(const auto frames:{9600u,24000u}){auto folder=temporary.getChildFile(frames==9600?"200ms":"500ms");folder.createDirectory();std::vector<float> samples(frames);samples[0]=.5f;mrs::audio::Recorder writer(path(folder.getChildFile("Synthetic Hi-Gn 121+57 Celestion.wav")),48000,0,{0});writer.capture(samples.data(),1,frames,0);writer.finish();}
    owner.app.set_cab_root(path(temporary));for(int i=0;i<3000&&browser->visibleRows()==0;++i){pump();browser->refresh();}check(browser->visibleRows()==1,"Cab defaults to actual 200ms variant");browser->activateRow(0);for(int i=0;i<3000;++i){pump();owner.app.poll();browser->refresh();if(!owner.chain(track).front().cab_variant_id.empty())break;}check(!owner.chain(track).front().cab_variant_id.empty(),"Cab selected variant persisted");
    const auto lengthSlot=mrs::new_id();{CabBrowser lengths(owner.app,track,lengthSlot,owner.view,[]{});lengths.filterFacet(5,"500 ms");check(lengths.browserView().facets[5]=="500 ms","Explicit 500ms filter");}{CabBrowser lengths(owner.app,track,lengthSlot,owner.view,[]{});check(lengths.browserView().facets[5]=="500 ms"&&lengths.visibleRows()==1,"500ms survives reopening instead of default 200ms");lengths.filterFacet(5,"Any Length");}{CabBrowser lengths(owner.app,track,lengthSlot,owner.view,[]{});check(lengths.browserView().facets[5].isEmpty()&&lengths.visibleRows()==2,"Explicit Any Length survives reopening");}
    const auto editorHwnd=static_cast<HWND>(owner.windows.back()->getPeer()->getNativeHandle());const auto mainHwnd=static_cast<HWND>(owner.getPeer()->getNativeHandle());check(GetWindow(editorHwnd,GW_OWNER)==mainHwnd,"Cab owner survives resizable peer recreation");SetWindowPos(mainHwnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);bool above=false;for(auto h=GetWindow(mainHwnd,GW_HWNDPREV);h;h=GetWindow(h,GW_HWNDPREV))if(h==editorHwnd)above=true;check(above,"Cab remains above main window");
    auto bad=temporary.getChildFile("200ms/AAA-broken.wav");{std::ofstream file(path(bad),std::ios::binary);auto word=[&](unsigned v,int bytes){for(int j=0;j<bytes;++j)file.put(static_cast<char>(v>>(j*8)));};file.write("RIFF",4);word(36+9600*8,4);file.write("WAVEfmt ",8);word(16,4);word(1,2);word(4,2);word(48000,4);word(48000*8,4);word(8,2);word(16,2);file.write("data",4);word(9600*8,4);for(int j=0;j<9600*8;++j)file.put(0);}auto previous=*owner.app.cab_library()->snapshot();auto missing=previous.assets.front();missing.relative="200ms/missing.wav";missing.id="missing-test";previous.assets.push_back(missing);const auto revision=previous.revision;owner.app.cab_library()->scan(path(temporary),previous.collection_id,previous);for(int i=0;i<3000&&owner.app.cab_library()->snapshot()->revision==revision;++i){pump();browser->refresh();}browser->refresh();check(browser->visibleRows()==2,"Missing catalog records hidden by default");const auto active=browser->activeRow();check(active==1,"Actual loaded row located");browser->activateRow(0);check(browser->selectedRow()==active&&browser->activeRow()==active,"Unavailable click keeps active selection");

    auto before=owner.app.services().projects->state().revision;mix->onDragStart();mix->setValue(45,juce::sendNotificationSync);mix->setValue(60,juce::sendNotificationSync);check(owner.app.services().projects->state().revision==before,"Cab preview outside history");mix->onDragEnd();check(std::abs(owner.chain(track).front().ir.mix-.6f)<1e-6f,"Cab mix commits");check(owner.app.services().projects->state().revision==before+1,"Cab one gesture one Undo");check(owner.app.undo()&&owner.chain(track).front().ir.mix==1,"Cab Undo");check(owner.app.redo(),"Cab Redo");
    for(const auto scale:{1.f,1.5f}){content->setSize(1000,550);for(int i=0;i<10;++i)pump();auto image=content->createComponentSnapshot(content->getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("cab-editor-"+juce::String(static_cast<int>(scale*100))+".png").createOutputStream();check(stream!=nullptr,"Cab preview stream");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(image,*stream),"Cab preview");}
    auto combo=temporary.getChildFile("combo");combo.createDirectory();for(auto rate:{44100u,96000u}){std::vector<float> samples(static_cast<std::size_t>(rate)*3/2);samples[0]=.5f;samples[1]=.25f;mrs::audio::Recorder writer(path(combo.getChildFile(rate==44100?"Polytone 1_441.wav":"Polytone 1.wav")),rate,0,{0,1});writer.capture(samples.data(),2,rate*3/4,0);writer.finish();}
    auto previousRevision=owner.app.cab_library()->snapshot()->revision;owner.app.set_cab_root(path(combo));for(int i=0;i<3000&&owner.app.cab_library()->snapshot()->revision==previousRevision;++i){pump();browser->refresh();}browser->filterRole(4);browser->refresh();check(browser->visibleRows()==2,"Generic WAV remains unknown");
    CabMappingPanel mappingPanel([&](bool all,const auto& metadata,std::string suffix){check(all,"Profile explicitly maps collection");owner.app.map_cab_assets(track,fx.id,{"Polytone 1.wav","Polytone 1_441.wav"},metadata,suffix);},{},false);
    auto* profile=dynamic_cast<juce::TextButton*>(mappingPanel.findChildWithID("cab-mapping-profile"));auto* apply=dynamic_cast<juce::TextButton*>(mappingPanel.findChildWithID("cab-mapping-apply"));check(profile&&apply,"User mapping entry points");profile->onClick();apply->onClick();
    for(int i=0;i<3000;++i){pump();browser->refresh();if(owner.app.cab_library()->snapshot()->overrides.size()==2)break;}browser->filterRole(3);browser->refresh();check(browser->visibleRows()==1,"Full Combo rates grouped into capture");browser->activateRow(0);
    for(int i=0;i<3000;++i){pump();owner.app.poll();browser->refresh();if(owner.chain(track).front().cab_metadata.contains("amp_model"))break;}
    check(owner.chain(track).front().cab_metadata.at("ir_role")=="full_combo"&&owner.chain(track).front().cab_metadata.at("amp_model")=="Mini Brute","Combo role and model reach project");check(std::abs(owner.chain(track).front().ir.mix-.6f)<1e-6f,"Role selection preserves wet control");check(browser->activeRow()==0&&browser->selectedRow()==0,"Grouped active row agrees with Ready");
    const auto comboPreset=owner.chain(track).front();const auto comboDecoded=decodePreset(encodePreset(comboPreset),comboPreset);check(comboDecoded.cab_metadata==comboPreset.cab_metadata&&comboDecoded.reverb==comboPreset.reverb&&comboDecoded.ir.mix==comboPreset.ir.mix,"Full Combo MR preset round trip");
    {
        struct VisibilityProbe final:juce::ComponentListener{juce::Component::SafePointer<juce::Component> component;int changes{};explicit VisibilityProbe(juce::Component& c):component(&c){c.addComponentListener(this);}~VisibilityProbe()override{if(component)component->removeComponentListener(this);}void componentVisibilityChanged(juce::Component&)override{++changes;}} visibility(*owner.windows.back());
        auto* heldWindow=owner.windows.back().get();auto* heldContent=content;const auto heldHwnd=static_cast<HWND>(heldWindow->getPeer()->getNativeHandle());const auto heldBounds=heldWindow->getBounds();
        auto alternate=comboPreset;for(const auto& asset:owner.app.cab_library()->snapshot()->assets)if(asset.relative=="Polytone 1_441.wav"){alternate.reverb.name=asset.relative;alternate.reverb.first={asset.relative,{},asset.digest,asset.bytes};alternate.reverb.sample_rate=asset.rate;alternate.reverb.channels=asset.channels;alternate.reverb.frames=asset.frames;}
        alternate.ir.mix=.35f;const auto file=temporary.getChildFile("Combo-window.mrspreset");check(file.replaceWithText(encodePreset(alternate)),"Cab alternate fixture preset");owner.loadPresetFile(track,fx.id,file);
        for(int i=0;i<3000;++i){pump();owner.app.poll();const auto loader=owner.app.reverb_loader(track,fx.id);if(loader&&loader->snapshot()->phase==mrs::audio::ReverbLoadPhase::ready)break;}browser->refresh();
        check(heldWindow->isVisible()&&heldWindow->getContentComponent()==heldContent&&heldWindow->getPeer()->getNativeHandle()==heldHwnd&&heldWindow->getBounds()==heldBounds&&visibility.changes==0,"Cab preset retains visible window/native handle/geometry");
        check(owner.chain(track).front().reverb.first.digest==alternate.reverb.first.digest&&browser->activeRow()==0&&std::abs(mix->getValue()-35)<.001,"Preset updates IR/Ready/highlight and controls without reopening");
        check(file.replaceWithText(encodePreset(comboPreset)),"Cab original fixture preset");owner.loadPresetFile(track,fx.id,file);for(int i=0;i<3000;++i){pump();owner.app.poll();const auto loader=owner.app.reverb_loader(track,fx.id);if(loader&&loader->snapshot()->phase==mrs::audio::ReverbLoadPhase::ready)break;}browser->refresh();
        check(heldWindow->getPeer()->getNativeHandle()==heldHwnd&&visibility.changes==0&&browser->activeRow()==0&&std::abs(mix->getValue()-60)<.001,"Repeated preset changes retain Cab window and refresh active identity");
    }
    browser->filterRole(2);check(browser->visibleRows()==0,"Combo absent from Cabinet filter");browser->filterRole(3);
    for(const auto scale:{1.f,1.5f,2.f}){content->setSize(1000,550);auto image=content->createComponentSnapshot(content->getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("cab-combo-"+juce::String(static_cast<int>(scale*100))+".png").createOutputStream();check(stream!=nullptr,"Combo preview stream");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(image,*stream),"Combo preview");}
    {auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("cab-mapping-150.png").createOutputStream();check(stream!=nullptr,"Mapping preview stream");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(mappingPanel.createComponentSnapshot(mappingPanel.getLocalBounds(),true,1.5f,juce::SoftwareImageType{}),*stream),"Mapping preview");}
    browser->filterFacet(0,"Polytone Mini Brute");browser->filterFacet(1,"Polytone 1");browser->filterFacet(5,"750 ms");browser->searchFor("polytone");browser->filterFlags(true,true);
    const auto expectedView=browser->browserView();check(expectedView.role==3&&expectedView.facets[0]=="Polytone Mini Brute"&&expectedView.facets[1]=="Polytone 1"&&expectedView.facets[5]=="750 ms"&&expectedView.search=="polytone"&&expectedView.favoritesOnly&&expectedView.unavailable,"Cab browser configured state");
    const auto beforeClose=owner.chain(track).front();const auto viewRevision=owner.app.services().projects->state().revision,viewGeneration=owner.app.insert_generation();
    owner.windows.back()->closeButtonPressed();owner.openInsert(track,fx.id);auto* restored=dynamic_cast<CabBrowser*>(owner.windows.back()->getContentComponent()->findChildWithID("cab-browser"));check(restored&&restored->browserView()==expectedView,"Close button and reopen preserve menus/search/toggles");check(owner.chain(track).front()==beforeClose&&owner.app.services().projects->state().revision==viewRevision&&owner.app.insert_generation()==viewGeneration,"Browser reopening retains IR/controls/history/graph");
    owner.closeEditors();auto settingsFile=temporary.getChildFile("cab-view.json");publishSettings(settingsFile,owner.view.encode());owner.view=ViewSettings::decode(settingsFile.loadFileAsString());check(owner.view.cabBrowserViews.at(label(fx.id.value))==expectedView,"Browser settings survive preferences file round trip");
    auto legacy=juce::JSON::parse(owner.view.encode());legacy.getDynamicObject()->removeProperty("cabBrowserViews");check(ViewSettings::decode(juce::JSON::toString(legacy)).cabBrowserViews.empty(),"Older preferences without browser state load");
    {CabBrowser other(owner.app,track,mrs::new_id(),owner.view,[]{});check(other.browserView().role==1&&other.browserView().search.isEmpty()&&!other.browserView().favoritesOnly,"Other Cab instance has independent browser state");}
    {mrs::desktop::Application delayed;auto preferences=ViewSettings::decode(owner.view.encode());CabBrowser late(delayed,track,fx.id,preferences,[]{});check(late.browserView()==expectedView,"Pending scan retains saved filter intent");delayed.set_cab_root(path(combo),preferences.cabManifest.toStdString());for(int i=0;i<3000&&delayed.cab_library()->snapshot()->assets.empty();++i){pump();late.refresh();}late.refresh();check(late.browserView()==expectedView,"Filters restore after asynchronous catalog scan");}
    for(int i=0;i<4;++i){owner.openInsert(track,fx.id);auto* reopened=dynamic_cast<CabBrowser*>(owner.windows.back()->getContentComponent()->findChildWithID("cab-browser"));check(reopened&&reopened->browserView()==expectedView,"Cab repeated editor lifetime retains browser state");owner.closeEditors();}check(owner.message.isEmpty(),"Cab editor error");
}
void reverbEditorSmoke(){
    const auto check=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);const auto folder=temp.getChildFile("mrs-reverb-editor-"+label(mrs::new_id().value));
    check(folder.getParentDirectory()==temp&&!folder.exists(),"editor fixture scope");check(folder.createDirectory().wasOk(),"editor fixture directory");
    struct Cleanup{juce::File folder;~Cleanup(){folder.deleteRecursively();}}cleanup{folder};
    const auto hall=folder.getChildFile(juce::String::fromUTF8("Halls/Зал.wav"));check(hall.getParentDirectory().createDirectory().wasOk(),"subfolder fixture");
    std::array<float,256> impulse{};impulse[0]=.5f;impulse[1]=.25f;
    {mrs::audio::Recorder writer(path(hall),48000,0,{0,1});writer.capture(impulse.data(),2,128,0);writer.finish();}
    Desktop owner(true);const auto track=owner.project()->tracks.front().id;mrs::NativeInsert effect;effect.id=mrs::new_id();effect.kind=mrs::InsertKind::reverb;owner.applyChain(track,{effect});owner.openInsert(track,effect.id);
    auto* content=owner.windows.back()->getContentComponent();auto* browser=dynamic_cast<ReverbBrowser*>(content->findChildWithID("reverb-browser"));check(browser!=nullptr,"normal native MR Reverb editor");browser->chooseRoot(folder);
    auto pump=[&]{MSG msg{};int limit{};while(limit++<200&&PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}owner.app.poll();std::this_thread::sleep_for(std::chrono::milliseconds(1));};
    auto wait=[&](auto predicate,const char* why){for(int i=0;i<3000;++i){pump();if(predicate())return;}throw std::runtime_error(why);};
    wait([&]{return browser->visibleRows()==1;},"editor IR scan");browser->searchFor(juce::String::fromUTF8("зал"));check(browser->visibleRows()==1,"editor Unicode search");browser->activate(0);
    wait([&]{return !owner.chain(track).front().reverb.first.digest.empty();},"selection reaches central model");const auto selected=owner.chain(track).front();check(selected.reverb.first.relative=="Halls/Зал.wav","relative IR identity");
    auto* mix=dynamic_cast<juce::Slider*>(content->findChildWithID("reverb-parameter-2"));check(mix&&mix->getValue()==25.,"default mix visible");
    check(std::abs(mix->proportionOfLengthToValue(.5)-25.)<1e-6&&std::abs(mix->proportionOfLengthToValue(.1)-1.)<1e-6,"Mix finer nonlinear travel");
    auto* options=dynamic_cast<juce::TextButton*>(content->findChildWithID("reverb-options"));auto* gate=dynamic_cast<juce::ToggleButton*>(content->findChildWithID("reverb-gate"));auto* threshold=dynamic_cast<juce::Slider*>(content->findChildWithID("reverb-parameter-4"));
    check(options&&gate&&threshold&&!gate->isVisible()&&!gate->getToggleState(),"gate optional/default off");options->onClick();check(gate->isVisible()&&!threshold->isEnabled(),"Options reveals disabled threshold");
    const auto generation=owner.app.insert_generation(),revision=owner.app.services().projects->state().revision;owner.app.play();wait([&]{return owner.app.engine()->state().playback==mrs::PlaybackState::playing;},"editor play");
    mix->onDragStart();mix->setValue(35.,juce::sendNotificationSync);mix->setValue(60.,juce::sendNotificationSync);check(owner.app.services().projects->state().revision==revision,"slider preview not history");mix->onDragEnd();
    check(std::abs(owner.chain(track).front().reverb.mix-.6f)<1e-6f&&owner.app.services().projects->state().revision==revision+1,"one Undo per slider gesture");check(owner.app.undo()&&owner.chain(track).front().reverb.mix==.25f,"live parameter Undo");check(owner.app.redo()&&owner.app.insert_generation()==generation,"live parameter Redo retains graph");
    gate->setToggleState(true,juce::dontSendNotification);gate->onClick();check(owner.chain(track).front().reverb.gate_enabled&&threshold->isEnabled(),"live gate toggle");
    const auto gateRevision=owner.app.services().projects->state().revision;threshold->onDragStart();threshold->setValue(-85.3,juce::sendNotificationSync);threshold->setValue(-75.3,juce::sendNotificationSync);
    check(owner.app.services().projects->state().revision==gateRevision,"threshold preview not history");threshold->onDragEnd();
    check(owner.app.services().projects->state().revision==gateRevision+1&&std::abs(owner.chain(track).front().reverb.gate_threshold+75.3f)<1e-6f,"one threshold Undo per gesture");
    check(owner.app.undo()&&owner.chain(track).front().reverb.gate_threshold==-90.f,"threshold Undo");check(owner.app.redo()&&owner.app.insert_generation()==generation,"gate Redo retains graph");
    owner.app.stop();wait([&]{return owner.app.engine()->state().playback!=mrs::PlaybackState::playing;},"editor stop");
    auto preset=owner.chain(track).front();preset.bypass=true;const auto decoded=decodePreset(encodePreset(preset),selected);check(decoded.reverb==preset.reverb&&decoded.bypass,"MR preset state and bypass");
    const auto presetFile=folder.getChildFile("Reverb.mrspreset");check(presetFile.replaceWithText(encodePreset(preset)),"write fixture preset");owner.loadPresetFile(track,effect.id,presetFile);check(owner.chain(track).front().bypass,"native shell Load preset");
    check(owner.windows.back()->getContentComponent()==content&&options->getToggleState(),"Reverb preset retains editor and Options");
    gate=dynamic_cast<juce::ToggleButton*>(content->findChildWithID("reverb-gate"));threshold=dynamic_cast<juce::Slider*>(content->findChildWithID("reverb-parameter-4"));check(gate->getToggleState()&&std::abs(threshold->getValue()+75.3)<.001,"preset restores gate controls");
    wait([&]{return owner.app.reverb_loader(track,effect.id)->snapshot()->phase==mrs::audio::ReverbLoadPhase::ready;},"preset IR restore");for(int i=0;i<10;++i)pump();
    for(float scale:{1.f,1.5f,2.f}){content->setSize(900,450);for(int i=0;i<5;++i)pump();const auto image=content->createComponentSnapshot(content->getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("reverb-editor-"+juce::String(static_cast<int>(scale*100))+".png").createOutputStream();check(stream!=nullptr,"editor preview stream");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(image,*stream),"editor preview export");}
    content->setSize(760,390);for(int i=0;i<5;++i)pump();check(content->getLocalBounds().contains(threshold->getBounds())&&content->findChildWithID("reverb-browser")->getBounds().getBottom()<threshold->getY(),"Options minimum layout");auto narrow=content->createComponentSnapshot(content->getLocalBounds(),true,1.f,juce::SoftwareImageType{});auto narrowStream=juce::File::getCurrentWorkingDirectory().getChildFile("reverb-editor-narrow.png").createOutputStream();check(narrowStream!=nullptr,"narrow stream");narrowStream->setPosition(0);narrowStream->truncate();check(juce::PNGImageFormat().writeImageToStream(narrow,*narrowStream),"narrow preview");
    auto* search=dynamic_cast<juce::TextEditor*>(content->findChildWithID("reverb-browser")->findChildWithID("reverb-search"));check(search!=nullptr,"reverb search editor");
    auto* window=owner.windows.back().get();window->toFront(true);const auto hwnd=static_cast<HWND>(window->getPeer()->getNativeHandle());SetFocus(hwnd);search->grabKeyboardFocus();
    PostMessageW(hwnd,WM_KEYDOWN,VK_SPACE,1);PostMessageW(hwnd,WM_CHAR,L' ',1);PostMessageW(hwnd,WM_KEYUP,VK_SPACE,LPARAM{1}<<31);for(int i=0;i<10;++i)pump();check(owner.app.engine()->state().playback!=mrs::PlaybackState::playing,"search Space does not start transport");
    auto* folders=dynamic_cast<juce::TreeView*>(content->findChildWithID("reverb-browser")->findChildWithID("reverb-folders"));check(folders&&folders->getRootItem()->getNumSubItems()==1,"Reverb folder tree");folders->getRootItem()->getSubItem(0)->setSelected(true,true);
    const auto expectedSearch=search->getText();
    for(int i=0;i<8;++i){owner.windows.back()->closeButtonPressed();owner.view=ViewSettings::decode(owner.view.encode());check(owner.view.nativeEditor(label(effect.id.value)).folder=="Halls","Reverb folder preference roundtrip");owner.openInsert(track,effect.id);auto* reopened=owner.windows.back()->getContentComponent();auto* reopenedSearch=dynamic_cast<juce::TextEditor*>(reopened->findChildWithID("reverb-browser")->findChildWithID("reverb-search"));auto* reopenedFolders=dynamic_cast<juce::TreeView*>(reopened->findChildWithID("reverb-browser")->findChildWithID("reverb-folders"));check(reopenedSearch&&reopenedSearch->getText()==expectedSearch&&reopenedFolders->getRootItem()->getSubItem(0)->isSelected()&&dynamic_cast<juce::TextButton*>(reopened->findChildWithID("reverb-options"))->getToggleState(),"Reverb close/reopen retains search, folder and Options");}
    owner.closeEditors();check(owner.message.isEmpty(),"editor lifetime safe");
}
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
    void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(background));g.setColour(juce::Colours::whitesmoke);g.setFont(Theme::detailFont());
        g.drawText("Device",20,20,150,28,juce::Justification::left);g.drawText("Sample rate",20,62,150,28,juce::Justification::left);g.drawText("Device buf",380,62,85,28,juce::Justification::left);g.drawText("Inputs (1,2...)",20,110,150,28,juce::Justification::left);g.drawText("Outputs (1,2...)",20,155,150,28,juce::Justification::left);
        g.drawText("Workers",480,248,75,28,juce::Justification::left);g.drawText("Process Buffer",20,290,150,28,juce::Justification::left);g.drawText("Playback ahead; live/shared DSP direct.",370,290,280,28,juce::Justification::left);g.drawText("Saved profiles",20,334,150,28,juce::Justification::left);g.drawText("Profile name",20,374,150,28,juce::Justification::left);
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
class AiPermissionsPanel final : public juce::Component {
public:
    Desktop& owner;
    juce::ComboBox mode;
    juce::ToggleButton enabled{"Enable AI access"},parameters{"Read saved plugin parameters"};
    std::array<juce::ToggleButton,5> automatic;
    juce::TextButton apply{"Apply permissions"},revoke{"Revoke access"};
    juce::Label status;
    explicit AiPermissionsPanel(Desktop& desktop):owner(desktop){
        setLookAndFeel(&owner.theme);setSize(640,416);setTitle("AI Permissions");
        mode.addItemList({"READ","SUGGEST","EDIT (markers + MIDI)","AUTO (tool allowlist)"},1);mode.setTitle("AI permission mode");mode.setComponentID("ai-permission-mode");
        for(auto* component:{static_cast<juce::Component*>(&mode),static_cast<juce::Component*>(&enabled),static_cast<juce::Component*>(&parameters),static_cast<juce::Component*>(&apply),static_cast<juce::Component*>(&revoke),static_cast<juce::Component*>(&status)})addAndMakeVisible(component);
        const std::array<juce::String,5> names{"Allow marker creation","Allow marker rename","Allow selection changes","Allow note transpose","Allow note velocity changes"};
        for(std::size_t i=0;i<automatic.size();++i){automatic[i].setButtonText(names[i]);automatic[i].setTitle(names[i]);automatic[i].setTooltip("Explicit AUTO allowlist. MIDI transpose/velocity require stopped playback, preview and confirmation. Draft insert/replace/edit requires EDIT; selection remains unsupported.");addAndMakeVisible(automatic[i]);}
        mode.onChange=[this]{updateStatus();};enabled.onClick=[this]{updateStatus();};parameters.setTooltip("Saved project/graph values only. No DSP or plugin-editor reads.");
        apply.onClick=[this]{owner.run([&]{mrs::ai::PermissionPolicy policy;policy.mode=static_cast<mrs::ai::PermissionMode>(mode.getSelectedId()-1);policy.enabled=enabled.getToggleState();policy.stored_plugin_parameters=parameters.getToggleState();
            for(std::size_t i=0;i<automatic.size();++i)if(automatic[i].getToggleState())policy.automatic_tools|=1u<<i;
            owner.setAiPermissions(policy);sync();});};
        revoke.onClick=[this]{owner.run([&]{auto policy=owner.app.ai_permissions();policy.enabled=false;owner.setAiPermissions(policy);sync();});};
        sync();
    }
    ~AiPermissionsPanel() override {setLookAndFeel(nullptr);}
    void sync(){const auto policy=owner.app.ai_permissions();mode.setSelectedId(static_cast<int>(policy.mode)+1,juce::dontSendNotification);enabled.setToggleState(policy.enabled,juce::dontSendNotification);parameters.setToggleState(policy.stored_plugin_parameters,juce::dontSendNotification);
        for(std::size_t i=0;i<automatic.size();++i)automatic[i].setToggleState((policy.automatic_tools&(1u<<i))!=0,juce::dontSendNotification);updateStatus();}
    void updateStatus(){const auto current=owner.app.ai_permissions();status.setText("Current: "+label(std::string(current.enabled?mrs::ai::permission_name(current.mode):std::string_view("OFF")))+"\nApply requires preview/confirmation. MIDI: stopped, loop off; draft requires EDIT.",juce::dontSendNotification);
        for(auto& item:automatic)item.setEnabled(enabled.getToggleState() && mode.getSelectedId()==4);parameters.setEnabled(enabled.getToggleState());}
    void resized() override {mode.setBounds(18,58,206,28);enabled.setBounds(248,58,350,28);parameters.setBounds(18,102,560,28);
        for(std::size_t i=0;i<automatic.size();++i)automatic[i].setBounds(26,164+static_cast<int>(i)*28,570,26);
        status.setBounds(18,312,604,44);apply.setBounds(18,370,202,30);revoke.setBounds(426,370,196,30);}
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(surface));g.setColour(juce::Colour(primaryText));g.setFont(Theme::browserFont().withHeight(18.f));g.drawText("AI Permissions",18,14,590,26,juce::Justification::left);
        g.setFont(Theme::browserFont());g.drawText("AUTO allowlist",18,138,590,20,juce::Justification::left);}
};
void Desktop::setAiPermissions(mrs::ai::PermissionPolicy policy){app.set_ai_permissions(policy);prefs.ai=policy;saveSettings();menuItemsChanged();resized();repaint();}
void Desktop::aiPermissions(){for(auto& existing:windows)if(existing->isVisible()&&existing->getName()=="AI Permissions"){existing->toFront(true);return;}
    auto window=std::make_unique<EditorWindow>("AI Permissions",new AiPermissionsPanel(*this));window->setLookAndFeel(&theme);windows.push_back(std::move(window));}
void aiPermissionsSmoke(Desktop& owner){
    const auto original=owner.app.ai_permissions();const auto project=owner.app.snapshot();AiPermissionsPanel permissionPanel(owner);
    const auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    const auto export_image=[&](float scale,const juce::String& name){auto image=permissionPanel.createComponentSnapshot(permissionPanel.getLocalBounds(),true,scale,juce::SoftwareImageType{});auto stream=juce::File::getCurrentWorkingDirectory().getChildFile(name).createOutputStream();if(!stream)throw std::runtime_error("AI settings snapshot output");stream->setPosition(0);stream->truncate();check(juce::PNGImageFormat().writeImageToStream(image,*stream),"AI settings PNG");};
    export_image(1.f,"ai-permissions-100.png");permissionPanel.mode.setSelectedId(1,juce::dontSendNotification);permissionPanel.updateStatus();permissionPanel.apply.onClick();check(owner.app.ai_permissions().mode==mrs::ai::PermissionMode::read,"AI READ applies");
    permissionPanel.mode.setSelectedId(4,juce::dontSendNotification);permissionPanel.updateStatus();permissionPanel.automatic[0].setToggleState(true,juce::dontSendNotification);permissionPanel.apply.onClick();check(owner.app.ai_permissions().automatic_tools==1 && owner.app.ai_permissions().mode==mrs::ai::PermissionMode::automatic,"AUTO explicit allowlist applies");
    check(owner.getMenuBarNames().contains("AI: AUTO"),"visible AI mode");export_image(1.5f,"ai-permissions-150.png");permissionPanel.revoke.onClick();check(!owner.app.ai_permissions().enabled && owner.getMenuBarNames().contains("AI: OFF"),"AI access revoked");
    check(owner.app.snapshot()==project,"AI permission UI does not mutate project");for(const auto* component:{static_cast<const juce::Component*>(&permissionPanel.mode),static_cast<const juce::Component*>(&permissionPanel.apply),static_cast<const juce::Component*>(&permissionPanel.revoke)})check(permissionPanel.getLocalBounds().contains(component->getBounds()),"AI settings layout bounds");
    owner.setAiPermissions(original);owner.message.clear();
}
void Desktop::audioSettings(){for(auto& existing:windows)if(existing->isVisible()&&existing->getName()=="Audio settings"){existing->toFront(true);return;}auto window=std::make_unique<EditorWindow>("Audio settings",new AudioPanel(*this));window->setLookAndFeel(&theme);windows.push_back(std::move(window));}
}
