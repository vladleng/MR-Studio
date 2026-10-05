#include "Desktop.h"

namespace ui {
namespace {
juce::String label(const std::string& s){return juce::String::fromUTF8(s.c_str());}
std::filesystem::path path(const juce::File& f){return std::filesystem::path(f.getFullPathName().toWideCharPointer());}
}
class FxPanel final : public juce::Component {
public:
    FxPanel(Desktop& d,std::optional<mrs::Id> t,mrs::Id id):owner(d),target(t),slot(id){
        for(auto* b:{&apply,&bypass,&remove,&up,&down,&enabled,&load,&invert})addAndMakeVisible(b);
        for(auto* c:{static_cast<juce::Component*>(&bands),static_cast<juce::Component*>(&parameters),static_cast<juce::Component*>(&presets),static_cast<juce::Component*>(&gain),static_cast<juce::Component*>(&frequency),static_cast<juce::Component*>(&q),static_cast<juce::Component*>(&mix)})addAndMakeVisible(c);
        bands.addItemList({"HP / Low cut","Band 1","Band 2","Band 3","LP / High cut"},1);bands.setSelectedId(2);
        presets.addItemList({"Neutral","Warm","Bright"},1);presets.setTextWhenNothingSelected("Preset...");
        bands.onChange=[this]{band=static_cast<std::size_t>(bands.getSelectedId()-1);sync();};parameters.onChange=[this]{sync();};
        apply.onClick=[this]{owner.run([&]{auto n=current();if(n.kind==mrs::InsertKind::vst3){const int i=parameters.getSelectedId()-1;if(i>=0&&i<static_cast<int>(infos.size()))owner.app.set_plugin_parameter(target,slot,infos[static_cast<std::size_t>(i)].id,gain.getText().getFloatValue());return;}
            if(n.kind==mrs::InsertKind::channel_eq){n.bands[band].gain=gain.getText().getFloatValue();n.bands[band].frequency=frequency.getText().getFloatValue();n.bands[band].q=q.getText().getFloatValue();}
            else {n.gain=std::pow(10.f,gain.getText().getFloatValue()/20);n.frequency=frequency.getText().getFloatValue();n.q=q.getText().getFloatValue();if(n.kind==mrs::InsertKind::cab_ir){n.ir.mix=mix.getText().getFloatValue();n.ir.low_cut=n.frequency;n.ir.high_cut=n.q;}}
            commit(n);});};
        bypass.onClick=[this]{owner.run([&]{auto n=current();n.bypass=!n.bypass;commit(n);});};
        enabled.onClick=[this]{owner.run([&]{auto n=current();n.bands[band].enabled=!n.bands[band].enabled;commit(n);});};
        invert.onClick=[this]{owner.run([&]{auto n=current();n.ir.invert=!n.ir.invert;commit(n);});};
        presets.onChange=[this]{owner.run([&]{auto n=current();const auto p=presets.getSelectedId();n.gain=1;n.ir.mix=1;n.ir.invert=false;n.ir.low_cut=p==2?80.f:p==3?50.f:20.f;n.ir.high_cut=p==2?5000.f:p==3?10000.f:20000.f;commit(n);});};
        juce::Component::SafePointer<FxPanel> safe(this);load.onClick=[safe]{if(safe)safe->owner.choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe](const auto& f){if(safe){auto n=safe->current();n.ir=mrs::audio::load_cab_ir(path(f));safe->commit(n);}},"*.wav");};
        auto structure=[this](int direction){owner.run([&]{auto effects=owner.chain(target);auto i=index(effects);if(direction==0)effects.erase(effects.begin()+static_cast<std::ptrdiff_t>(i));
            else if((direction<0&&i>0)||(direction>0&&i+1<effects.size()))std::swap(effects[i],effects[direction<0?i-1:i+1]);
            owner.closeEditors();owner.applyChain(target,effects);});};
        remove.onClick=[structure]{structure(0);};up.onClick=[structure]{structure(-1);};down.onClick=[structure]{structure(1);};
        setWantsKeyboardFocus(true);setSize(720,510);sync();
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
        float value=eq?n.bands[band].gain:20*std::log10(juce::jmax(.000001f,n.gain));
        if(vst){const auto live=owner.app.plugin_parameters(target,slot);int at=parameters.getSelectedId()-1;if(at>=0&&at<static_cast<int>(infos.size())){for(const auto& p:live)if(p.id==infos[static_cast<std::size_t>(at)].id)value=p.initial;apply.setEnabled(infos[static_cast<std::size_t>(at)].automatable);}}
        gain.setText(juce::String(value,4),false);frequency.setText(juce::String(cab?n.ir.low_cut:eq?n.bands[band].frequency:n.frequency,2),false);
        q.setText(juce::String(cab?n.ir.high_cut:eq?n.bands[band].q:n.q,4),false);mix.setText(juce::String(n.ir.mix,3),false);
        enabled.setButtonText(eq&&n.bands[band].enabled?"Band on":"Band off");invert.setButtonText(n.ir.invert?"Polarity -":"Polarity +");bypass.setButtonText(n.bypass?"Enable":"Bypass");repaint();
    }
    void resized() override {bypass.setBounds(20,14,90,28);remove.setBounds(118,14,90,28);up.setBounds(216,14,70,28);down.setBounds(294,14,70,28);
        bands.setBounds(20,335,180,28);enabled.setBounds(210,335,100,28);parameters.setBounds(20,335,460,28);
        load.setBounds(20,335,120,28);presets.setBounds(150,335,125,28);invert.setBounds(285,335,115,28);mix.setBounds(500,335,100,28);
        gain.setBounds(20,402,170,28);frequency.setBounds(215,402,170,28);q.setBounds(410,402,170,28);apply.setBounds(20,447,160,30);}
    juce::Rectangle<float> curve() const{return {24,76,660,234};}
    juce::Point<float> point(const mrs::EqBand& b) const {auto r=curve();return {r.getX()+static_cast<float>(std::log(b.frequency/20.)/std::log(1000.))*r.getWidth(),r.getCentreY()-b.gain/48.f*r.getHeight()};}
    void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(background));g.setColour(juce::Colours::lightgrey);g.setFont(owner.theme.font(12));
        g.drawText("Gain dB / VST 0..1",20,374,180,24,juce::Justification::left);g.drawText("Frequency / low cut Hz",215,374,180,24,juce::Justification::left);g.drawText("Q / high cut Hz",410,374,180,24,juce::Justification::left);
        g.drawText("Live native parameters; structural edits and project save after Pause / Stop",20,483,680,24,juce::Justification::left);
        auto n=drag?*drag:current();g.setColour(juce::Colour(0xff1d2328));g.fillRect(curve());
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
    juce::TextButton apply{"Apply parameters"},bypass{"Bypass"},remove{"Remove"},up{"Up"},down{"Down"},enabled{"Band on"},load{"Load IR WAV"},invert{"Polarity +"};
    juce::ComboBox bands,parameters,presets;
    juce::TextEditor gain,frequency,q,mix;
};
void Desktop::insertMenu(std::optional<mrs::Id> target){juce::PopupMenu m;m.addItem(1,"Add Gain");m.addItem(2,"Add Channel EQ");m.addItem(3,"Add Cab IR...");
    const auto existing=chain(target);for(std::size_t i=0;i<existing.size();++i)m.addItem(1000+static_cast<int>(i),"Edit insert "+juce::String(static_cast<int>(i)+1));
    for(std::size_t i=0;i<catalog.size();++i)m.addItem(10000+static_cast<int>(i),label(catalog[i].vendor)+" / "+label(catalog[i].name));
    juce::Component::SafePointer<Desktop> safe(this);m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),[safe,target](int n){if(!safe||!n)return;safe->run([&]{
        if(n>=10000){safe->addPlugin(target,static_cast<std::size_t>(n-10000));return;}if(n>=1000){auto effects=safe->chain(target);safe->openInsert(target,effects.at(static_cast<std::size_t>(n-1000)).id);return;}
        if(n==3){safe->choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,target](auto f){if(!safe)return;auto effects=safe->chain(target);mrs::NativeInsert ir;ir.id=mrs::new_id();ir.kind=mrs::InsertKind::cab_ir;ir.ir=mrs::audio::load_cab_ir(path(f));effects.push_back(ir);safe->closeEditors();safe->applyChain(target,effects);safe->openInsert(target,ir.id);},"*.wav");return;}
        auto effects=safe->chain(target);mrs::NativeInsert effect;effect.id=mrs::new_id();effect.kind=n==1?mrs::InsertKind::gain:mrs::InsertKind::channel_eq;effects.push_back(effect);safe->closeEditors();safe->applyChain(target,effects);safe->openInsert(target,effect.id);
    });});}
void Desktop::openInsert(std::optional<mrs::Id> target,mrs::Id slot){run([&]{auto effects=chain(target);auto found=std::find_if(effects.begin(),effects.end(),[&](const auto& n){return n.id==slot;});if(found==effects.end())return;
    const auto key=label((target?target->value:"master")+":"+slot.value);for(auto& w:windows)if(w->isVisible()&&w->getProperties()["mrs-slot"].toString()==key){w->toFront(true);return;}
    closeEditors();if(found->kind==mrs::InsertKind::vst3){auto* content=new juce::Component;content->setSize(640,480);auto window=std::make_unique<EditorWindow>(label(found->plugin_name),content);int w=640,h=480;
        if(app.open_plugin_editor(target,slot,window->getPeer()->getNativeHandle(),w,h)){window->setContentComponentSize(w,h);window->getProperties().set("mrs-slot",key);window->getProperties().set("mrs-native-editor",true);window->onClose=[this]{app.close_plugin_editors();};windows.push_back(std::move(window));return;}}
    auto fx=std::make_unique<EditorWindow>("Insert editor",new FxPanel(*this,target,slot));fx->setLookAndFeel(&theme);fx->getProperties().set("mrs-slot",key);windows.push_back(std::move(fx));
});}

class AudioPanel final : public juce::Component {
public:
    explicit AudioPanel(Desktop& d):owner(d){for(auto* c:{static_cast<juce::Component*>(&devices),static_cast<juce::Component*>(&rate),static_cast<juce::Component*>(&buffer),static_cast<juce::Component*>(&inputs),static_cast<juce::Component*>(&outputs),static_cast<juce::Component*>(&connect),static_cast<juce::Component*>(&disconnect),static_cast<juce::Component*>(&panelButton)})addAndMakeVisible(c);
        devices.addItem("Offline clock (no sound)",1);
#ifdef MRS_HAS_ASIO
        probe=mrs::audio::make_asio_device();infos=probe->enumerate();for(std::size_t i=0;i<infos.size();++i)devices.addItem(label(infos[i].name),static_cast<int>(i)+2);
#endif
        devices.setSelectedId(1);rate.addItemList({"44100","48000","88200","96000","192000"},1);rate.setText(juce::String(owner.project()->sample_rate),juce::dontSendNotification);
        buffer.addItemList({"64","128","256","512","1024","2048"},1);buffer.setSelectedId(2);inputs.setText("");outputs.setText("1,2");
        connect.onClick=[this]{owner.run([&]{owner.closeEditors();const auto device=devices.getSelectedId()-2;mrs::audio::DeviceConfig config{0,static_cast<std::uint32_t>(rate.getText().getIntValue()),static_cast<std::uint32_t>(buffer.getText().getIntValue()),{},mrs::desktop::parse_outputs(outputs.getText().toStdString())};
            if(!inputs.getText().trim().isEmpty())config.inputs=mrs::desktop::parse_outputs(inputs.getText().toStdString());
            if(device<0){owner.resetDevice();return;}
#ifdef MRS_HAS_ASIO
            config.device=infos.at(static_cast<std::size_t>(device)).index;owner.app.connect(mrs::audio::make_asio_device(),config);
#else
            throw std::runtime_error("ASIO is not enabled in this build");
#endif
        });};disconnect.onClick=[this]{owner.run([&]{owner.closeEditors();owner.app.disconnect();});};
        panelButton.onClick=[this]{owner.run([&]{const int i=devices.getSelectedId()-2;if(i>=0&&probe)probe->control_panel(infos.at(static_cast<std::size_t>(i)).index);});};setSize(620,350);
    }
    void resized() override{devices.setBounds(160,20,430,28);rate.setBounds(160,62,180,28);buffer.setBounds(410,62,180,28);inputs.setBounds(160,110,430,28);outputs.setBounds(160,155,430,28);connect.setBounds(20,220,130,30);disconnect.setBounds(160,220,130,30);panelButton.setBounds(300,220,150,30);}
    void paint(juce::Graphics& g) override{g.fillAll(juce::Colour(background));g.setColour(juce::Colours::whitesmoke);g.setFont(owner.theme.font(12));
        g.drawText("Device",20,20,135,28,juce::Justification::left);g.drawText("Sample rate",20,62,135,28,juce::Justification::left);g.drawText("Frames",345,62,60,28,juce::Justification::left);g.drawText("Inputs (1,2...)",20,110,135,28,juce::Justification::left);g.drawText("Outputs (1,2...)",20,155,135,28,juce::Justification::left);
        g.drawText("Inputs are physical channel numbers. Empty = playback only.",20,268,580,24,juce::Justification::left);g.drawText("Project rate must match the selected device rate.",20,298,580,24,juce::Justification::left);}
private:
    Desktop& owner;std::unique_ptr<mrs::audio::IAudioDevice> probe;std::vector<mrs::audio::DeviceInfo> infos;
    juce::ComboBox devices,rate,buffer;juce::TextEditor inputs,outputs;
    juce::TextButton connect{"Connect"},disconnect{"Disconnect"},panelButton{"ASIO control panel"};
};
void Desktop::audioSettings(){auto window=std::make_unique<EditorWindow>("Audio settings",new AudioPanel(*this));window->setLookAndFeel(&theme);windows.push_back(std::move(window));}
}
