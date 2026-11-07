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
    void showInstrumentPicker(juce::Component& anchor) {
        owner.run([this,&anchor]{effectsMenu(true,&anchor);});
    }
    // Mixer insert rows use the exact same validated Live FX commands as the
    // previous selector; no second rack or direct Core/plugin mutations.
    void showInsertAddMenu(juce::Component& anchor) {
        owner.run([this,&anchor]{effectsMenu(false,&anchor,{},true);});
    }
    void showInsertSlotMenu(juce::Component& anchor,const mrs::Id& slot) {
        owner.run([this,&anchor,slot]{effectsMenu(false,&anchor,slot);});
    }
    void toggleInsertBypass(const mrs::Id& slot) {
        owner.run([this,slot]{
            if(owner.app.recording() || owner.displayState().playback==mrs::PlaybackState::playing)
                return; // Existing Live FX edit contract: graph changes while stopped.
            auto saved=live::decodeLiveInputProfile(profile());
            auto chain=saved.inserts;
            const auto at=std::find_if(chain.begin(),chain.end(),
                [&](const auto& effect){return effect.id==slot;});
            if(at==chain.end() || (saved.instrument && at==chain.begin()))return;
            at->bypass=!at->bypass;
            owner.setLiveInputEffects(id,std::move(chain));
        });
    }
    void resized() override {
        auto r=getLocalBounds();
        if(surface==Surface::mixer) {
            // Two fixed-height rows below the shared fader-zone divider.
            // Instrument's VSTi selector occupies row 1 in the owning Strip;
            // its physical output remains here on row 2.
            input.setBounds(r.removeFromTop(24).reduced(1,1));
            output.setBounds(r.removeFromTop(24).reduced(1,1));
            effects.setBounds({});
            return; // Mixer has real insert rows, not the old FX popup.
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
        const bool stopped=!owner.app.recording() && owner.displayState().playback!=mrs::PlaybackState::playing;
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
        effects.setVisible(surface!=Surface::mixer && !timelineInstrument);
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
        owner.setLiveChannelOutputs(id,outputs);
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
        const auto signature=saved.clone();const auto generation=owner.liveDocumentGeneration;
        juce::Component::SafePointer<LiveChannelControls> safe(this);
        menu.showMenuAsync(liveMenuOptions(input),[safe,ports,signature,generation,device,inputs](int action){if(!safe||!action)return;
            safe->owner.run([&]{
                if(safe->owner.liveDocumentGeneration!=generation || !live::sameShowValue(safe->profile(),signature))throw std::runtime_error("Channel changed while menu was open; reopen it");
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
        juce::Component::SafePointer<LiveChannelControls> safe(this);const auto signature=profile().clone();const auto generation=owner.liveDocumentGeneration;
        menu.showMenuAsync(liveMenuOptions(output),[safe,routes,signature,generation,device,outputs,names](int action){if(safe&&action>0&&static_cast<std::size_t>(action)<=routes.size())safe->owner.run([&]{
            if(safe->owner.liveDocumentGeneration!=generation || !live::sameShowValue(safe->profile(),signature))throw std::runtime_error("Channel changed; reopen output menu");
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
        // The integer generation and juce::String signature must have separate
        // auto declarations (MSVC C3538, Windows #382).
        const auto generation=owner.liveDocumentGeneration;
        const auto signature=saved.clone();
        juce::Component::SafePointer<LiveChannelControls> safe(this);
        channels.showMenuAsync(liveMenuOptions(output),[safe,generation,signature](int action) {
            if(!safe || action<2000 || action>2016)return;
            safe->owner.run([&]{
                if(safe->owner.liveDocumentGeneration!=generation || !live::sameShowValue(safe->profile(),signature))
                    throw std::runtime_error("Channel changed while MIDI menu was open; reopen it");
                const auto updated=live::decodeLiveInputProfile(safe->profile());
                safe->owner.configureLiveMidi(safe->id,updated.midiInput,action-2001,updated.outputs);
                safe->sync();
            });
        });
    }
    void effectsMenu(bool instrumentPicker=false,juce::Component* anchor=nullptr,
                     std::optional<mrs::Id> slotFilter={},bool addOnly=false) {
        if(!profile().isObject())throw std::runtime_error("Choose the channel input first");
        const auto row=live::find(owner.liveDocument->interfaceState,"channels",id);
        const auto kind=row["type"].toString();
        const auto saved=live::decodeLiveInputProfile(profile());
        if(kind=="Instrument"&&!saved.instrument)
            throw std::runtime_error("Legacy audio profile: convert this channel to Player explicitly");
        if((kind!="Instrument"&&saved.instrument)||kind=="Playback")
            throw std::runtime_error("Saved plugin profile does not match channel type");
        const bool stopped=!owner.app.recording() && owner.displayState().playback!=mrs::PlaybackState::playing;
        juce::PopupMenu menu;menu.setLookAndFeel(&owner.theme);
        const auto catalog=owner.catalog; // Shared scanner catalog, never scan or probe from the menu.
        // #289 A - channel patch library. A patch is a channel-owned snapshot of
        // this chain; applying it is a stopped, Undoable chain change. The same
        // menu assigns the current item's default patch.
        juce::PopupMenu patchMenu;
        const auto currentItem=owner.liveDocument->interfaceState["currentItem"].toString();
        const auto currentTrack=live::channelItemTrack(owner.liveDocument->interfaceState,currentItem,id);
        const auto defaultPatch=currentTrack["patchId"].toString();
        const bool canApply=stopped && owner.liveInputTrack(id).has_value() && profile().isObject();
        if(const auto* bank=row["patches"].getArray())
            for(int i=0;i<bank->size();++i) {
                const auto patch=(*bank)[i];
                const auto patchId=patch["id"].toString();
                const bool legacy=live::legacyPatch(patch);
                const auto currentSub=currentTrack["subPatchId"].toString();
                juce::PopupMenu entry;
                entry.addItem(5000+i*8,"Apply now",canApply&&!legacy,defaultPatch==patchId);
                juce::PopupMenu subs;
                subs.addItem(20000+i*64,"Base",canApply&&!legacy,
                             defaultPatch==patchId&&currentSub.isEmpty());
                if(const auto* subList=patch["subPatches"].getArray())
                    for(int j=0;j<subList->size()&&j<31;++j) {
                        const auto sub=(*subList)[j];
                        subs.addItem(20000+i*64+j+1,sub["name"].toString(),canApply&&!legacy,
                                     defaultPatch==patchId&&currentSub==sub["id"].toString());
                    }
                entry.addSubMenu("Sub-patches",subs);
                if(!legacy)
                    entry.addItem(30000+i,"Capture current as sub-patch…",canApply);
                entry.addItem(5001+i*8,"Set as item default",true,defaultPatch==patchId);
                if(defaultPatch==patchId)entry.addItem(5002+i*8,"Clear item default");
                entry.addItem(5003+i*8,"Rename…");
                juce::PopupMenu colours;
                for(int c=0;c<static_cast<int>(live::patchColours.size());++c) {
                    auto label=juce::String(live::patchColours[static_cast<std::size_t>(c)]);
                    colours.addItem(6000+i*8+c,label.substring(0,1).toUpperCase()+label.substring(1),true,
                                    patch["color"].toString()==label);
                }
                entry.addSubMenu("Colour",colours);
                entry.addItem(5004+i*8,"Delete…");
                patchMenu.addSubMenu(patch["name"].toString()+(legacy?" (legacy)":""),entry);
            }
        if(patchMenu.getNumItems()>0)patchMenu.addSeparator();
        patchMenu.addItem(7000,"Capture current chain as patch…",canApply);
        menu.addSubMenu("Patches",patchMenu);
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
        // Instrument picker lives on the Mixer strip. Timeline keeps only
        // MIDI input/channel and an editor icon beside the track title.
        const bool chooseInstrument=saved.instrument && (instrumentPicker || surface==Surface::timeline);
        const bool mixerInstrument=saved.instrument && !chooseInstrument;
        if(chooseInstrument) {
            juce::PopupMenu vsti;appendType(vsti,true);
            menu.addSubMenu(saved.inserts.empty()?"Choose VST3 Instrument":"Replace VST3 Instrument",vsti,stopped);
        }
        if(!chooseInstrument && !slotFilter) {
            juce::PopupMenu audioFx;appendType(audioFx,false);
            menu.addSubMenu("Add Audio FX",audioFx,stopped&&saved.inserts.size()<8&&(!saved.instrument||!saved.inserts.empty()));
        }
        const auto unknown=static_cast<int>(std::count_if(catalog.begin(),catalog.end(),[](const auto& p){return !p.instrument.has_value();}));
        if(unknown>0 && !slotFilter)
            menu.addItem(0,"Unknown plugin type ("+juce::String(unknown)+") — rescan catalog",false);
        for(std::size_t i=mixerInstrument?1u:0u;i<saved.inserts.size();++i) {
            if(chooseInstrument && i>0)break;
            const auto& fx=saved.inserts[i];
            if(addOnly || (slotFilter && !(fx.id==*slotFilter)))continue;
            juce::PopupMenu slot;
            auto& actions=slotFilter?menu:slot;
            // Editor access remains explicit; an assigned valid input is monitored
            // automatically, without a separate Monitor button.
            actions.addItem(100+static_cast<int>(i)*10,"Open editor",
                owner.liveInputTrack(id).has_value() || stopped);
            actions.addItem(101+static_cast<int>(i)*10,"Bypass",stopped,fx.bypass);
            actions.addItem(102+static_cast<int>(i)*10,"Move up",stopped&&i>(saved.instrument?1u:0u));
            actions.addItem(103+static_cast<int>(i)*10,"Move down",stopped&&i+1<saved.inserts.size()&&(!saved.instrument||i>0));
            actions.addItem(104+static_cast<int>(i)*10,"Remove",stopped&&(!saved.instrument||i>0||saved.inserts.size()==1));
            if(!slotFilter)menu.addSubMenu(juce::String(static_cast<int>(i+1))+". "+juce::String::fromUTF8(fx.plugin_name.c_str()),slot);
        }
        if(slotFilter && menu.getNumItems()==0)throw std::runtime_error("Channel FX slot changed; reopen insert menu");
        const auto signature=profile().clone();const auto generation=owner.liveDocumentGeneration;
        const auto patchSnapshot=row["patches"].clone();
        juce::Component::SafePointer<LiveChannelControls> safe(this);
        menu.showMenuAsync(liveMenuOptions(anchor?*anchor:effects),[safe,saved,signature,choices,generation,instrumentPicker,patchSnapshot,currentItem,stopped](int action){if(!safe||!action)return;safe->owner.run([&]{
            if(safe->owner.liveDocumentGeneration!=generation || !live::sameShowValue(safe->profile(),signature))
                throw std::runtime_error("Channel changed; reopen effect menu");
            if((action>=5000&&action<10000)||(action>=20000&&action<31000)) {
                safe->patchAction(patchSnapshot,currentItem,action,stopped);
                return;
            }
            auto chain=saved.inserts;
            if(action>=10000) {
                const auto index=static_cast<std::size_t>(action-10000);
                if(index>=choices.size())throw std::runtime_error("Invalid plugin menu action");
                const auto& plugin=choices[index];
                if(std::find(safe->owner.catalog.begin(),safe->owner.catalog.end(),plugin)==safe->owner.catalog.end())
                    throw std::runtime_error("Plugin catalog changed; reopen menu");
                if(!plugin.instrument.has_value())throw std::runtime_error("Unknown VST3 class type; rescan catalog");
                const bool vsti=*plugin.instrument;
                if(saved.instrument && (instrumentPicker || safe->surface==Surface::timeline)!=vsti)
                    throw std::runtime_error("Choose VSTi from the Mixer instrument selector, audio FX from its FX slot");
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
    // #289 A menu actions. Patches are addressed by index into the snapshot the
    // menu opened with; the generation check in effectsMenu rejects stale menus.
    void patchAction(const juce::var& patches,const juce::String& itemId,int action,bool stopped) {
        const auto patchAt=[&](int index)->juce::var {
            if(!patches.isArray()||index<0||index>=patches.size())return {};
            return patches[index];
        };
        juce::Component::SafePointer<LiveChannelControls> safe(this);
        if(action==7000) {
            juce::Component::SafePointer<Desktop> ownerSafe(&owner);
            ownerSafe->textDialog("Patch name","Live patch",[ownerSafe,safe](auto name){
                if(!safe||name.trim().isEmpty())return;
                ownerSafe->run([=]{ownerSafe->captureLiveChannelPatch(safe->id,name,"blue");safe->sync();});
            });
            return;
        }
        if(action>=20000&&action<30000) {
            // #289 B: Base (slot 0) or a named sub-patch of the same chain.
            const int index=(action-20000)/64,slot=(action-20000)%64;
            const auto patch=patchAt(index);
            if(!patch.isObject()||!stopped)return;
            const auto patchId=patch["id"].toString();
            if(slot==0) {
                owner.applyLiveChannelSubPatch(id,patchId,{});
            } else {
                const auto* subList=patch["subPatches"].getArray();
                if(!subList||slot-1>=subList->size())return;
                owner.applyLiveChannelSubPatch(id,patchId,(*subList)[slot-1]["id"].toString());
            }
            sync();
            return;
        }
        if(action>=30000&&action<31000) {
            const auto patch=patchAt(action-30000);
            if(!patch.isObject()||!stopped)return;
            const auto patchId=patch["id"].toString();
            juce::Component::SafePointer<Desktop> ownerSafe(&owner);
            ownerSafe->textDialog("Sub-patch name","Rhythm",[ownerSafe,safe,patchId](auto name){
                if(!safe||name.trim().isEmpty())return;
                ownerSafe->run([=]{ownerSafe->captureLiveChannelSubPatch(safe->id,patchId,name,"violet");safe->sync();});
            });
            return;
        }
        if(action>=6000&&action<7000) {
            const int index=(action-6000)/8,colour=(action-6000)%8;
            const auto patch=patchAt(index);
            if(!patch.isObject()||colour>=static_cast<int>(live::patchColours.size()))return;
            owner.setLiveChannelPatchColour(id,patch["id"].toString(),
                juce::String(live::patchColours[static_cast<std::size_t>(colour)]));
            return;
        }
        const int index=(action-5000)/8,operation=(action-5000)%8;
        const auto patch=patchAt(index);
        if(!patch.isObject())return;
        const auto patchId=patch["id"].toString();
        if(operation==0) {
            if(stopped)owner.applyLiveChannelPatch(id,patchId);
        } else if(operation==1) {
            owner.assignLiveItemPatch(id,patchId);
        } else if(operation==2&&itemId.isNotEmpty()) {
            owner.assignLiveItemPatch(id,{});
        } else if(operation==3) {
            juce::Component::SafePointer<Desktop> ownerSafe(&owner);
            ownerSafe->textDialog("Patch name",patch["name"].toString(),[ownerSafe,safe,patchId](auto name){
                if(!safe||name.trim().isEmpty())return;
                ownerSafe->run([=]{ownerSafe->renameLiveChannelPatch(safe->id,patchId,name);safe->sync();});
            });
            return;
        } else if(operation==4) {
            owner.deleteLiveChannelPatch(id,patchId);
        }
        sync();
    }
    Desktop& owner;juce::String id;Surface surface;
    juce::String cachedMidiId,cachedMidiName;juce::uint32 lastMidiRefresh{};
    juce::TextButton input{"Input"},output{"Master"},effects{"FX / VSTi"};
};
} // namespace ui
