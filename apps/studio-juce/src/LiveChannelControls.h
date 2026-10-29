#pragma once
#include "Desktop.h"
#include "LiveInputState.h"
#include "LiveTheme.h"
#include "LiveContext.h"

namespace ui {
// Channel-local selectors use Core/catalog commands. Only a vendor editor gets
// its own window; no channel configuration dialog or separate channel save.
class LiveChannelControls final : public juce::Component {
public:
    LiveChannelControls(Desktop& desktop,juce::String channel):owner(desktop),id(std::move(channel)) {
        for(auto* button:{&input,&output,&effects})addAndMakeVisible(button);
        input.setComponentID("live-channel-input-"+id);
        output.setComponentID("live-channel-output-"+id);
        effects.setComponentID("live-channel-effects-"+id);
        input.onClick=[this]{inputMenu();};output.onClick=[this]{outputMenu();};effects.onClick=[this]{owner.run([&]{effectsMenu();});};
    }
    void resized() override {
        auto r=getLocalBounds();auto row=r.removeFromTop(22);
        input.setBounds(row.removeFromLeft(row.getWidth()/2).reduced(1,0));output.setBounds(row.reduced(1,0));
        effects.setBounds(r.removeFromTop(22).reduced(1,0));
    }
    void sync() {
        if(!owner.liveDocument)return;
        const auto row=live::find(owner.liveDocument->interfaceState,"channels",id);
        const auto profile=row["liveInput"];
        const bool stopped=!owner.app.recording() && owner.app.engine()->state().playback!=mrs::PlaybackState::playing;
        setEnabled(stopped && row["type"].toString()!="Playback");
        input.setButtonText(profile.isObject()?(static_cast<int>(profile["version"])==2?"MIDI":"In "+juce::String(static_cast<int>(profile["input"])+1)+(static_cast<bool>(profile["stereo"])?" ST":"")):"Input");
        input.setTooltip(profile.isObject()?profile["deviceName"].toString()+" / "+profile["inputName"].toString():"Choose audio input or MIDI / VSTi");
        output.setButtonText(profile["outputs"].size()?"Out "+juce::String(static_cast<int>(profile["outputs"][0])+1):"Master");
        effects.setButtonText(instrument()&&profile["inserts"].size()>0?profile["inserts"][0]["name"].toString()+" / FX":"FX / VSTi ("+juce::String(profile["inserts"].size())+")");
        output.setTooltip(profile["outputNames"].size()?profile["outputNames"][0].toString():"Master / default hardware output");
        if(profile.isObject() && profile["deviceName"].toString()!=juce::String::fromUTF8(owner.app.audio_name().c_str())) {
            input.setButtonText("Missing input");input.setTooltip("Saved device unavailable: "+profile["deviceName"].toString());
        }
        effects.setTooltip("Instrument and effects: add, editor, bypass, reorder, remove. Saved with the show.");
    }
private:
    juce::var profile()const{return owner.liveDocument?live::find(owner.liveDocument->interfaceState,"channels",id)["liveInput"]:juce::var{};}
    bool instrument()const {const auto p=profile();return p.isObject()&&static_cast<int>(p["version"])==2;}
    void audio(int port,bool stereo,const std::vector<int>& route) {owner.configureLiveInput(id,port,stereo,route);}
    void route(const std::vector<int>& outputs) {
        const auto p=live::decodeLiveInputProfile(profile());
        if(p.instrument)owner.configureLiveMidi(id,p.midiInput,p.midiChannel,outputs);
        else audio(p.input,p.stereo,outputs);
    }
    void inputMenu() {
        juce::PopupMenu menu;menu.setLookAndFeel(&owner.theme);
        const auto inputs=owner.app.input_names();
        const auto saved=profile();
        const auto channel=live::find(owner.liveDocument->interfaceState,"channels",id);
        for(std::size_t i=0;i<inputs.size();++i) {
            menu.addItem(100+static_cast<int>(i)*2,"Mono "+juce::String(static_cast<int>(i+1))+" - "+juce::String::fromUTF8(inputs[i].c_str()));
            if(i+1<inputs.size())menu.addItem(101+static_cast<int>(i)*2,"Stereo "+juce::String(static_cast<int>(i+1))+"/"+juce::String(static_cast<int>(i+2))+" - "+juce::String::fromUTF8(inputs[i].c_str())+" / "+juce::String::fromUTF8(inputs[i+1].c_str()));
        }
        if(inputs.empty())menu.addItem(0,"Audio inputs unavailable: connect Audio settings",false);
        const auto ports=mrs::desktop::MidiInputs::ports();
        if(channel["type"].toString()=="Instrument") {
            juce::PopupMenu midi;
            midi.addItem(1000,"MIDI off / VSTi");
            for(std::size_t i=0;i<ports.size();++i)midi.addItem(1001+static_cast<int>(i),juce::String::fromUTF8(ports[i].name.c_str()));
            menu.addSubMenu("MIDI / VSTi",midi);
            if(instrument()) {
                juce::PopupMenu channels;channels.addItem(2000,"Omni",true,static_cast<int>(saved["midiChannel"])==-1);
                for(int i=0;i<16;++i)channels.addItem(2001+i,"Channel "+juce::String(i+1),true,static_cast<int>(saved["midiChannel"])==i);
                menu.addSubMenu("MIDI channel",channels);
            }
        }
        if(saved.isObject())menu.addItem(3000,"Clear channel input and effects");
        const auto signature=live::canonical(saved);
        juce::Component::SafePointer<LiveChannelControls> safe(this);
        menu.showMenuAsync(liveMenuOptions(input),[safe,ports,signature](int action){if(!safe||!action)return;
            safe->owner.run([&]{
                if(live::canonical(safe->profile())!=signature)throw std::runtime_error("Channel changed while menu was open; reopen it");
                const auto saved=safe->profile();
                const auto outputs=saved.isObject()?live::decodeLiveInputProfile(saved).outputs:std::vector<int>{};
                if(action>=100 && action<1000)safe->audio((action-100)/2,(action-100)%2!=0,outputs);
                else if(action>=1000 && action<2000) {
                    const int i=action-1001;safe->owner.configureLiveMidi(safe->id,i<0?juce::String{}:juce::String::fromUTF8(ports.at(static_cast<std::size_t>(i)).id.c_str()),-1,outputs);
                } else if(action>=2000 && action<=2016) {
                    const auto p=live::decodeLiveInputProfile(saved);safe->owner.configureLiveMidi(safe->id,p.midiInput,action-2001,outputs);
                } else if(action==3000)safe->owner.editLiveInterface("Clear channel input",[&](auto& state){live::require(state,"channels",safe->id).getDynamicObject()->removeProperty("liveInput");});
            });safe->sync();
        });
    }
    void outputMenu() {
        if(!profile().isObject())return;
        juce::PopupMenu menu;menu.setLookAndFeel(&owner.theme);menu.addItem(1,"Master");
        std::vector<std::vector<int>> routes{{}};const auto outputs=owner.app.active_outputs();const auto names=owner.app.output_names();
        const auto label=[&](int i){return juce::String(i+1)+" - "+(i>=0&&static_cast<std::size_t>(i)<names.size()?juce::String::fromUTF8(names[static_cast<std::size_t>(i)].c_str()):"Unavailable");};
        for(auto out:outputs){routes.push_back({out});menu.addItem(static_cast<int>(routes.size()),label(out));}
        for(std::size_t i=0;i+1<outputs.size();++i)if(outputs[i+1]==outputs[i]+1){routes.push_back({outputs[i],outputs[i+1]});menu.addItem(static_cast<int>(routes.size()),label(outputs[i])+" / "+label(outputs[i+1]));}
        juce::Component::SafePointer<LiveChannelControls> safe(this);const auto signature=live::canonical(profile());
        menu.showMenuAsync(liveMenuOptions(output),[safe,routes,signature](int action){if(safe&&action>0&&static_cast<std::size_t>(action)<=routes.size())safe->owner.run([&]{
            if(live::canonical(safe->profile())!=signature)throw std::runtime_error("Channel changed; reopen output menu");
            safe->route(routes[static_cast<std::size_t>(action-1)]);safe->sync();
        });});
    }
    void effectsMenu() {
        if(!profile().isObject())throw std::runtime_error("Choose the channel input first");
        const auto saved=live::decodeLiveInputProfile(profile());
        juce::PopupMenu menu,add;menu.setLookAndFeel(&owner.theme);
        const auto catalog=owner.catalog;
        for(std::size_t i=0;i<catalog.size();++i) {
            const auto& plugin=catalog[i];
            // Unknown old catalog entries are probed only after explicit selection.
            const bool synth=plugin.instrument.value_or(false);
            if(synth && !saved.instrument)continue;
            add.addItem(10000+static_cast<int>(i),juce::String::fromUTF8(plugin.name.c_str())+(synth?" [VSTi]":""));
        }
        if(add.getNumItems()==0)add.addItem(0,"Scan the plugin browser first",false);
        menu.addSubMenu(saved.instrument&&saved.inserts.empty()?"Choose VSTi":"Add / replace plugin",add,saved.inserts.size()<8||saved.instrument);
        for(std::size_t i=0;i<saved.inserts.size();++i) {
            const auto& fx=saved.inserts[i];juce::PopupMenu slot;
            slot.addItem(100+static_cast<int>(i)*10,"Open editor",owner.liveInputTrack(id).has_value());
            slot.addItem(101+static_cast<int>(i)*10,"Bypass",true,fx.bypass);
            slot.addItem(102+static_cast<int>(i)*10,"Move up",i>(saved.instrument?1u:0u));
            slot.addItem(103+static_cast<int>(i)*10,"Move down",i+1<saved.inserts.size()&&(!saved.instrument||i>0));
            slot.addItem(104+static_cast<int>(i)*10,"Remove",!saved.instrument||i>0||saved.inserts.size()==1);
            menu.addSubMenu(juce::String(static_cast<int>(i+1))+". "+juce::String::fromUTF8(fx.plugin_name.c_str()),slot);
        }
        const auto signature=live::canonical(profile());juce::Component::SafePointer<LiveChannelControls> safe(this);
        menu.showMenuAsync(liveMenuOptions(effects),[safe,saved,signature,catalog](int action){if(!safe||!action)return;safe->owner.run([&]{
            if(live::canonical(safe->profile())!=signature)throw std::runtime_error("Channel changed; reopen effect menu");
            auto chain=saved.inserts;
            if(action>=10000) {
                const auto index=static_cast<std::size_t>(action-10000);if(index>=catalog.size())throw std::runtime_error("Plugin catalog changed");
                auto plugin=catalog[index];
                if(!plugin.instrument) {
                    const auto found=mrs::processing::probe_vst3(plugin.path);
                    const auto at=std::find_if(found.begin(),found.end(),[&](const auto& p){return p.class_id==plugin.class_id;});
                    if(at==found.end())throw std::runtime_error("Plugin class is unavailable");plugin.instrument=at->instrument;
                }
                if(plugin.instrument.value_or(false) && !saved.instrument)throw std::runtime_error("VSTi requires a MIDI / Instrument channel");
                if(saved.instrument&&chain.empty()&&!plugin.instrument.value_or(false))throw std::runtime_error("Choose a VSTi before adding audio effects");
                mrs::NativeInsert fx;fx.id=mrs::new_id();fx.kind=mrs::InsertKind::vst3;fx.plugin_path=plugin.path;fx.class_id=plugin.class_id;fx.plugin_name=plugin.name;
                if(saved.instrument&&plugin.instrument.value_or(false)&&!chain.empty())chain.front()=std::move(fx);else chain.push_back(std::move(fx));
            } else {
                const auto i=static_cast<std::size_t>((action-100)/10);const auto operation=(action-100)%10;if(i>=chain.size())return;
                if(operation==0){const auto track=safe->owner.liveInputTrack(safe->id);if(track)safe->owner.openInsert(*track,chain[i].id);return;}
                if(operation==1)chain[i].bypass=!chain[i].bypass;
                else if(operation==2&&i>(saved.instrument?1u:0u))std::swap(chain[i],chain[i-1]);
                else if(operation==3&&i+1<chain.size()&&(!saved.instrument||i>0))std::swap(chain[i],chain[i+1]);
                else if(operation==4&&(!saved.instrument||i>0||chain.size()==1))chain.erase(chain.begin()+static_cast<std::ptrdiff_t>(i));
            }
            safe->owner.setLiveInputEffects(safe->id,std::move(chain));safe->sync();
        });});
    }
    Desktop& owner;juce::String id;juce::TextButton input{"Input"},output{"Master"},effects{"FX / VSTi"};
};
} // namespace ui
