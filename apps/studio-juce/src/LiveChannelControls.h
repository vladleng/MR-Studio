#pragma once
#include "Desktop.h"
#include "LiveInputState.h"
#include "LivePluginCatalog.h"
#include "LiveTheme.h"
#include "LiveContext.h"

namespace ui {
// Channel-local selectors use Core/catalog commands. Only a vendor editor gets
// its own window; no channel configuration dialog or separate channel save.
class LiveChannelControls final : public juce::Component {
public:
    enum class Surface { timeline, mixer };
    LiveChannelControls(Desktop& desktop,juce::String channel,Surface placement=Surface::timeline)
        :owner(desktop),id(std::move(channel)),surface(placement) {
        for(auto* button:{&input,&output,&effects})addAndMakeVisible(button);
        input.setComponentID("live-channel-input-"+id);
        output.setComponentID("live-channel-output-"+id);
        effects.setComponentID("live-channel-effects-"+id);
        input.onClick=[this]{inputMenu();};output.onClick=[this]{outputMenu();};effects.onClick=[this]{owner.run([&]{effectsMenu();});};
    }
    void resized() override {
        auto r=getLocalBounds();
        if(surface==Surface::mixer && !input.isVisible()) {
            output.setBounds(r.removeFromTop(22).reduced(1,0));
            effects.setBounds(r.removeFromTop(22).reduced(1,0));
            return;
        }
        auto row=r.removeFromTop(22);
        input.setBounds(row.removeFromLeft(row.getWidth()/2).reduced(1,0));
        output.setBounds(row.reduced(1,0));
        effects.setBounds(r.removeFromTop(22).reduced(1,0));
    }
    void sync() {
        if(!owner.liveDocument)return;
        const auto row=live::find(owner.liveDocument->interfaceState,"channels",id);
        const auto profile=row["liveInput"];
        const bool stopped=!owner.app.recording() && owner.app.engine()->state().playback!=mrs::PlaybackState::playing;
        const auto type=row["type"].toString();
        const bool legacyAudio=type=="Instrument" && profile.isObject() && static_cast<int>(profile["version"])==1;
        const bool timelineInstrument=surface==Surface::timeline && type=="Instrument"&&!legacyAudio;
        const bool mixerInstrument=surface==Surface::mixer && type=="Instrument"&&!legacyAudio;
        setEnabled(type!="Playback");
        const bool relayout=input.isVisible()==mixerInstrument;
        // Keep physical output routing in the Mixer. Timeline's second field
        // now owns MIDI Channel instead of Master, but the audio route must
        // remain adjustable on the actual Instrument mixer strip.
        input.setVisible(!mixerInstrument);output.setVisible(true);
        input.setEnabled(stopped);
        output.setEnabled(stopped&&!legacyAudio&&(!timelineInstrument||profile.isObject()));
        effects.setEnabled(!legacyAudio);
        if(timelineInstrument) {
            const auto midiId=profile["midiInput"].toString();
            const auto now=juce::Time::getMillisecondCounter();
            if(cachedMidiId!=midiId || now-lastMidiRefresh>3000u) {
                cachedMidiId=midiId;lastMidiRefresh=now;
                cachedMidiName=midiId.isEmpty()?"MIDI Off":"Missing MIDI";
                if(midiId.isNotEmpty())for(const auto& port:mrs::desktop::MidiInputs::ports()) {
                    if(juce::String::fromUTF8(port.id.c_str())==midiId) {
                        cachedMidiName=juce::String::fromUTF8(port.name.c_str());break;
                    }
                }
            }
            input.setButtonText(cachedMidiName);
            input.setTooltip("MIDI Input: "+cachedMidiName);
            const int channel=profile.isObject()?static_cast<int>(profile["midiChannel"]):-1;
            output.setButtonText(profile.isObject()?(channel<0?"Omni":"MIDI Ch "+juce::String(channel+1)):"MIDI Channel");
            output.setTooltip("MIDI receive channel: Omni or 1–16");
            effects.setButtonText(profile["inserts"].size()>0&&instrument()
                ?profile["inserts"][0]["name"].toString():"Choose Instrument");
            effects.setTooltip("VST3 instrument only. Audio FX belong to the Mixer.");
        } else {
            input.setButtonText(legacyAudio?"Convert to Player":profile.isObject()?
                (static_cast<int>(profile["version"])==2?"MIDI":"In "+juce::String(static_cast<int>(profile["input"])+1)+(static_cast<bool>(profile["stereo"])?" ST":"")):
                type=="Instrument"?"MIDI":"Audio Input");
            input.setTooltip(legacyAudio?"Legacy audio profile: explicitly convert to Player (Undo available)":
                profile.isObject()?profile["deviceName"].toString()+" / "+profile["inputName"].toString():
                type=="Instrument"?"Choose MIDI input":"Choose physical mono/stereo audio input");
            output.setButtonText(profile["outputs"].size()?"Out "+juce::String(static_cast<int>(profile["outputs"][0])+1):"Master");
            output.setTooltip(profile["outputNames"].size()?profile["outputNames"][0].toString():"Master / default hardware output");
            effects.setButtonText(mixerInstrument?"Audio FX ("+juce::String(juce::jmax(0,profile["inserts"].size()-1))+")":
                type=="Instrument"?(profile["inserts"].size()>0&&instrument()?profile["inserts"][0]["name"].toString()+" / FX":"VSTi / FX"):
                "Audio FX ("+juce::String(profile["inserts"].size())+")");
            effects.setTooltip(legacyAudio?"Convert the legacy channel to Player first":
                mixerInstrument?"Instrument audio effects only; choose VSTi on the Timeline":
                type=="Instrument"?"VST3 instrument + audio effects":"Audio FX only — saved in the show");
            if(!legacyAudio && profile.isObject() && profile["deviceName"].toString()!=juce::String::fromUTF8(owner.app.audio_name().c_str())) {
                input.setButtonText("Missing input");input.setTooltip("Saved device unavailable: "+profile["deviceName"].toString());
            }
        }
        if(relayout)resized();
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
        const auto device=owner.app.audio_name();
        const auto saved=profile();
        const auto channel=live::find(owner.liveDocument->interfaceState,"channels",id);
        const auto type=channel["type"].toString();
        const bool legacyAudio=type=="Instrument" && saved.isObject() && static_cast<int>(saved["version"])==1;
        if(type=="Instrument") {
            if(legacyAudio) {
                menu.addItem(4000,"Convert legacy audio channel to Player (Undoable)");
                menu.addItem(0,"Audio profile is retained until conversion",false);
            } else {
                const auto ports=mrs::desktop::MidiInputs::ports();
                juce::PopupMenu midi;
                midi.addItem(1000,"MIDI off / VSTi");
                for(std::size_t i=0;i<ports.size();++i)midi.addItem(1001+static_cast<int>(i),juce::String::fromUTF8(ports[i].name.c_str()));
                menu.addSubMenu("MIDI input",midi);

            }
        } else {
            for(std::size_t i=0;i<inputs.size();++i) {
                menu.addItem(100+static_cast<int>(i)*2,"Mono "+juce::String(static_cast<int>(i+1))+" - "+juce::String::fromUTF8(inputs[i].c_str()));
                if(i+1<inputs.size())menu.addItem(101+static_cast<int>(i)*2,"Stereo "+juce::String(static_cast<int>(i+1))+"/"+juce::String(static_cast<int>(i+2))+" - "+juce::String::fromUTF8(inputs[i].c_str())+" / "+juce::String::fromUTF8(inputs[i+1].c_str()));
            }
            if(inputs.empty())menu.addItem(0,"Audio inputs unavailable: connect Audio settings",false);
        }
        const auto ports=mrs::desktop::MidiInputs::ports();
        if(saved.isObject()&&!legacyAudio)menu.addItem(3000,"Clear channel input and effects");
        const auto signature=live::canonical(saved);const auto generation=owner.liveDocumentGeneration;
        juce::Component::SafePointer<LiveChannelControls> safe(this);
        menu.showMenuAsync(liveMenuOptions(input),[safe,ports,signature,generation,device,inputs](int action){if(!safe||!action)return;
            safe->owner.run([&]{
                if(safe->owner.liveDocumentGeneration!=generation || live::canonical(safe->profile())!=signature)throw std::runtime_error("Channel changed while menu was open; reopen it");
                if(safe->owner.app.audio_name()!=device || safe->owner.app.input_names()!=inputs)
                    throw std::runtime_error("Audio device changed; reopen input menu");
                const auto saved=safe->profile();
                const auto outputs=saved.isObject()?live::decodeLiveInputProfile(saved).outputs:std::vector<int>{};
                if(action==4000) {
                    const auto row=live::find(safe->owner.liveDocument->interfaceState,"channels",safe->id);
                    if(row["type"].toString()!="Instrument" || !saved.isObject() || static_cast<int>(saved["version"])!=1)
                        throw std::runtime_error("Legacy channel changed; reopen the input menu");
                    safe->owner.convertLegacyLiveInstrument(safe->id);
                } else if(action>=100 && action<1000)safe->audio((action-100)/2,(action-100)%2!=0,outputs);
                else if(action>=1000 && action<2000) {
                    const int i=action-1001;
                    const int midiChannel=saved.isObject()?live::decodeLiveInputProfile(saved).midiChannel:-1;
                    safe->owner.configureLiveMidi(safe->id,i<0?juce::String{}:juce::String::fromUTF8(ports.at(static_cast<std::size_t>(i)).id.c_str()),midiChannel,outputs);
                } else if(action==3000)safe->owner.editLiveInterface("Clear channel input",[&](auto& state){live::require(state,"channels",safe->id).getDynamicObject()->removeProperty("liveInput");},false,true);
            });if(safe)safe->sync();
        });
    }
    void outputMenu() {
        const auto row=live::find(owner.liveDocument->interfaceState,"channels",id);
        if(surface==Surface::timeline && row["type"].toString()=="Instrument" && instrument()) {
            midiChannelMenu();
            return;
        }
        if(!profile().isObject())return;
        juce::PopupMenu menu;menu.setLookAndFeel(&owner.theme);menu.addItem(1,"Master");
        std::vector<std::vector<int>> routes{{}};const auto outputs=owner.app.active_outputs();const auto names=owner.app.output_names();
        const auto device=owner.app.audio_name();
        const auto label=[&](int i){return juce::String(i+1)+" - "+(i>=0&&static_cast<std::size_t>(i)<names.size()?juce::String::fromUTF8(names[static_cast<std::size_t>(i)].c_str()):"Unavailable");};
        for(auto out:outputs){routes.push_back({out});menu.addItem(static_cast<int>(routes.size()),label(out));}
        for(std::size_t i=0;i+1<outputs.size();++i)if(outputs[i+1]==outputs[i]+1){routes.push_back({outputs[i],outputs[i+1]});menu.addItem(static_cast<int>(routes.size()),label(outputs[i])+" / "+label(outputs[i+1]));}
        juce::Component::SafePointer<LiveChannelControls> safe(this);const auto signature=live::canonical(profile());const auto generation=owner.liveDocumentGeneration;
        menu.showMenuAsync(liveMenuOptions(output),[safe,routes,signature,generation,device,outputs,names](int action){if(safe&&action>0&&static_cast<std::size_t>(action)<=routes.size())safe->owner.run([&]{
            if(safe->owner.liveDocumentGeneration!=generation || live::canonical(safe->profile())!=signature)throw std::runtime_error("Channel changed; reopen output menu");
            if(safe->owner.app.audio_name()!=device || safe->owner.app.output_names()!=names || safe->owner.app.active_outputs()!=outputs)
                throw std::runtime_error("Audio device changed; reopen output menu");
            safe->route(routes[static_cast<std::size_t>(action-1)]);safe->sync();
        });});
    }
    void midiChannelMenu() {
        const auto saved=profile();
        if(!saved.isObject() || !instrument())return;
        const auto current=live::decodeLiveInputProfile(saved);
        juce::PopupMenu channels;channels.setLookAndFeel(&owner.theme);
        channels.addItem(2000,"Omni",true,current.midiChannel==-1);
        for(int i=0;i<16;++i)
            channels.addItem(2001+i,"MIDI Channel "+juce::String(i+1),true,current.midiChannel==i);
        const auto generation=owner.liveDocumentGeneration,signature=live::canonical(saved);
        juce::Component::SafePointer<LiveChannelControls> safe(this);
        channels.showMenuAsync(liveMenuOptions(output),[safe,generation,signature](int action) {
            if(!safe || action<2000 || action>2016)return;
            safe->owner.run([&]{
                if(safe->owner.liveDocumentGeneration!=generation || live::canonical(safe->profile())!=signature)
                    throw std::runtime_error("Channel changed while MIDI menu was open; reopen it");
                const auto updated=live::decodeLiveInputProfile(safe->profile());
                safe->owner.configureLiveMidi(safe->id,updated.midiInput,action-2001,updated.outputs);
                safe->sync();
            });
        });
    }
    void effectsMenu() {
        if(!profile().isObject())throw std::runtime_error("Choose the channel input first");
        const auto row=live::find(owner.liveDocument->interfaceState,"channels",id);
        const auto kind=row["type"].toString();
        const auto saved=live::decodeLiveInputProfile(profile());
        if(kind=="Instrument"&&!saved.instrument)
            throw std::runtime_error("Legacy audio profile: convert this channel to Player explicitly");
        if((kind!="Instrument"&&saved.instrument)||kind=="Playback")
            throw std::runtime_error("Saved plugin profile does not match channel type");
        const bool stopped=!owner.app.recording() && owner.app.engine()->state().playback!=mrs::PlaybackState::playing;
        juce::PopupMenu menu;menu.setLookAndFeel(&owner.theme);
        const auto catalog=owner.catalog; // Shared scanner catalog, never scan or probe from the menu.
        std::vector<mrs::processing::VstPlugin> choices; // Explicit action -> exact class identity after sorting.
        const auto appendType=[&](juce::PopupMenu& target,bool vsti) {
            const auto indices=live::sortedPluginClasses(catalog,vsti);
            juce::PopupMenu folder;juce::String current;bool opened=false;
            for(const auto index:indices) {
                const auto& plugin=catalog[index];const auto group=live::pluginVendor(plugin);
                if(!opened||current.compareIgnoreCase(group)!=0) {
                    if(opened)target.addSubMenu(current,folder);
                    current=group;folder=juce::PopupMenu{};opened=true;
                }
                const auto action=10000+static_cast<int>(choices.size());
                choices.push_back(plugin);
                folder.addItem(action,juce::String::fromUTF8(plugin.name.c_str()));
            }
            if(opened)target.addSubMenu(current,folder);
            else target.addItem(0,vsti?"No scanned VST3 instruments":"No scanned audio effects",false);
        };
        const bool timelineInstrument=saved.instrument && surface==Surface::timeline;
        const bool mixerInstrument=saved.instrument && surface==Surface::mixer;
        if(timelineInstrument) {
            juce::PopupMenu vsti;appendType(vsti,true);
            menu.addSubMenu(saved.inserts.empty()?"Choose VST3 Instrument":"Replace VST3 Instrument",vsti,stopped);
        }
        if(!timelineInstrument) {
            juce::PopupMenu audioFx;appendType(audioFx,false);
            menu.addSubMenu("Add Audio FX",audioFx,stopped&&saved.inserts.size()<8&&(!saved.instrument||!saved.inserts.empty()));
        }
        const auto unknown=static_cast<int>(std::count_if(catalog.begin(),catalog.end(),[](const auto& p){return !p.instrument.has_value();}));
        if(unknown>0)menu.addItem(0,"Unknown plugin type ("+juce::String(unknown)+") — rescan catalog",false);
        for(std::size_t i=mixerInstrument?1u:0u;i<saved.inserts.size();++i) {
            if(timelineInstrument && i>0)break;
            const auto& fx=saved.inserts[i];juce::PopupMenu slot;
            // A saved VST3 can be edited before Monitor is enabled. An explicit
            // editor request prepares the Core track, but leaves monitoring OFF.
            slot.addItem(100+static_cast<int>(i)*10,"Open editor",
                owner.liveInputTrack(id).has_value() || stopped);
            slot.addItem(101+static_cast<int>(i)*10,"Bypass",stopped,fx.bypass);
            slot.addItem(102+static_cast<int>(i)*10,"Move up",stopped&&i>(saved.instrument?1u:0u));
            slot.addItem(103+static_cast<int>(i)*10,"Move down",stopped&&i+1<saved.inserts.size()&&(!saved.instrument||i>0));
            slot.addItem(104+static_cast<int>(i)*10,"Remove",stopped&&(!saved.instrument||i>0||saved.inserts.size()==1));
            menu.addSubMenu(juce::String(static_cast<int>(i+1))+". "+juce::String::fromUTF8(fx.plugin_name.c_str()),slot);
        }
        const auto signature=live::canonical(profile());const auto generation=owner.liveDocumentGeneration;
        juce::Component::SafePointer<LiveChannelControls> safe(this);
        menu.showMenuAsync(liveMenuOptions(effects),[safe,saved,signature,choices,generation](int action){if(!safe||!action)return;safe->owner.run([&]{
            if(safe->owner.liveDocumentGeneration!=generation || live::canonical(safe->profile())!=signature)
                throw std::runtime_error("Channel changed; reopen effect menu");
            auto chain=saved.inserts;
            if(action>=10000) {
                const auto index=static_cast<std::size_t>(action-10000);
                if(index>=choices.size())throw std::runtime_error("Invalid plugin menu action");
                const auto& plugin=choices[index];
                if(std::find(safe->owner.catalog.begin(),safe->owner.catalog.end(),plugin)==safe->owner.catalog.end())
                    throw std::runtime_error("Plugin catalog changed; reopen menu");
                if(!plugin.instrument.has_value())throw std::runtime_error("Unknown VST3 class type; rescan catalog");
                const bool vsti=*plugin.instrument;
                if(saved.instrument && (safe->surface==Surface::timeline)!=vsti)
                    throw std::runtime_error("VSTi belongs to Timeline; audio FX belongs to Mixer");
                if(vsti&&!saved.instrument)throw std::runtime_error("VSTi requires an Instrument channel");
                if(!vsti&&saved.instrument&&chain.empty())throw std::runtime_error("Choose a VSTi before adding audio effects");
                if((!vsti||!saved.instrument)&&chain.size()>=8)throw std::runtime_error("Live channel insert limit reached");
                mrs::NativeInsert fx;fx.id=mrs::new_id();fx.kind=mrs::InsertKind::vst3;
                fx.plugin_path=plugin.path;fx.class_id=plugin.class_id;fx.plugin_name=plugin.name;
                if(vsti&&saved.instrument) {
                    if(chain.empty())chain.push_back(std::move(fx));else chain.front()=std::move(fx);
                } else chain.push_back(std::move(fx));
            } else {
                if(action<100)return;
                const auto i=static_cast<std::size_t>((action-100)/10);
                const auto operation=(action-100)%10;if(i>=chain.size())return;
                if(operation==0){
                    const auto channel=safe->id;const auto slot=chain[i].id;
                    safe->owner.openLiveInsertEditor(channel,slot);
                    return;
                }
                if(operation==1)chain[i].bypass=!chain[i].bypass;
                else if(operation==2&&i>(saved.instrument?1u:0u))std::swap(chain[i],chain[i-1]);
                else if(operation==3&&i+1<chain.size()&&(!saved.instrument||i>0))std::swap(chain[i],chain[i+1]);
                else if(operation==4&&(!saved.instrument||i>0||chain.size()==1))chain.erase(chain.begin()+static_cast<std::ptrdiff_t>(i));
            }
            safe->owner.setLiveInputEffects(safe->id,std::move(chain));safe->sync();
        });});
    }
    Desktop& owner;juce::String id;Surface surface;
    juce::String cachedMidiId,cachedMidiName;juce::uint32 lastMidiRefresh{};
    juce::TextButton input{"Input"},output{"Master"},effects{"FX / VSTi"};
};
} // namespace ui
