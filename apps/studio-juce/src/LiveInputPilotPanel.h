#pragma once
#include "Desktop.h"
#include "LiveContext.h"
#include "LivePlaybackState.h"

namespace ui {
// #297: deliberately bounded first Live Input vertical slice.
// Physical input, monitoring and hosted VST3 all belong to the existing
// Application/Core graph. This panel owns no audio callback or plugin instance.
class LiveInputPilotPanel final : public juce::Component {
public:
    explicit LiveInputPilotPanel(Desktop& desktop, const juce::String& preferredChannel = {}) : owner(desktop) {
        setComponentID("live-input-pilot");
        setSize(440, 424);
        title.setText("Channel Input / FX (single active channel)", juce::dontSendNotification);
        description.setText("Shared Core input + VST3. Save to Show stores channel profile.", juce::dontSendNotification);
        description.setJustificationType(juce::Justification::centredLeft);
        channelLabel.setText("Show channel (Guitar / Keys / Vocal)",juce::dontSendNotification);
        inputLabel.setText("Physical ASIO input (select port or stereo pair)", juce::dontSendNotification);
        vstLabel.setText("VST3 audio effect", juce::dontSendNotification);
        for (auto* component : std::array<juce::Component*, 9>{
            &title, &description, &inputLabel, &physicalInput, &enable,
            &disable, &vstLabel, &vst3, &addEffect}) addAndMakeVisible(component);
        for(auto* component:std::array<juce::Component*,4>{&channelLabel,&showChannel,&saveProfile,&loadProfile})
            addAndMakeVisible(component);
        addAndMakeVisible(openEditor);
        addAndMakeVisible(stereoInput);
        addAndMakeVisible(routeLabel);
        addAndMakeVisible(outputRoute);
        addAndMakeVisible(status);
        routeLabel.setText("Channel hardware outputs", juce::dontSendNotification);
        stereoInput.setButtonText("Stereo (input N + N+1)");
        saveProfile.setButtonText("Save to Show");
        loadProfile.setButtonText("Load from Show");
        enable.setButtonText("Enable Monitor");
        disable.setButtonText("Disable");
        addEffect.setButtonText("Add VST3");
        openEditor.setButtonText("Open editor");
        showChannel.setComponentID("live-input-channel");
        saveProfile.setComponentID("live-input-save-profile");
        loadProfile.setComponentID("live-input-load-profile");
        physicalInput.setComponentID("live-input-physical");
        stereoInput.setComponentID("live-input-stereo");
        outputRoute.setComponentID("live-input-output-route");
        enable.setComponentID("live-input-enable");
        disable.setComponentID("live-input-disable");
        vst3.setComponentID("live-input-plugin");
        addEffect.setComponentID("live-input-add-vst");
        if(owner.liveDocument)
            if(const auto* channels=owner.liveDocument->interfaceState["channels"].getArray())
                for(const auto& channel:*channels) if(channel["type"].toString()!="Playback") {
                    channelIds.push_back(channel["id"].toString());
                    showChannel.addItem(channel["name"].toString(),static_cast<int>(channelIds.size()));
                }
        if(!channelIds.empty()) {
            int selected=1;
            for(std::size_t i=0;i<channelIds.size();++i)
                if(channelIds[i]==(preferredChannel.isNotEmpty()?preferredChannel:owner.liveInputPilotChannel))
                    selected=static_cast<int>(i+1);
            showChannel.setSelectedId(selected,juce::dontSendNotification);
        }
        const auto names=owner.app.input_names();
        for (std::size_t i=0;i<names.size();++i)
            physicalInput.addItem(juce::String(static_cast<int>(i+1)) + " - " +
                juce::String::fromUTF8(names[i].c_str()),static_cast<int>(i+1));
        if (!names.empty()) physicalInput.setSelectedId(1,juce::dontSendNotification);
        // Direct outputs must be selected from currently configured hardware ports;
        // never create a virtual output or a second device.
        outputRoutes.push_back({});
        outputRoute.addItem("Master / default routing", 1);
        const auto activeOutputs=owner.app.active_outputs();
        for (const auto physical:activeOutputs) {
            outputRoutes.push_back({physical});
            outputRoute.addItem("Output " + juce::String(physical+1) + " (mono)",
                                static_cast<int>(outputRoutes.size()));
        }
        for (std::size_t i=0;i+1<activeOutputs.size();++i)
            if (activeOutputs[i+1]==activeOutputs[i]+1) {
                outputRoutes.push_back({activeOutputs[i],activeOutputs[i+1]});
                outputRoute.addItem("Outputs " + juce::String(activeOutputs[i]+1) + "/" +
                                    juce::String(activeOutputs[i+1]+1) + " (stereo)",
                                    static_cast<int>(outputRoutes.size()));
            }
        outputRoute.setSelectedId(1,juce::dontSendNotification);
        if (owner.liveInputPilotTrack && owner.liveInputPilotChannel == selectedShowChannel()) {
            for(const auto& track:owner.project()->tracks)
                if(track.id==*owner.liveInputPilotTrack) {
                    physicalInput.setSelectedId(track.input+1,juce::dontSendNotification);
                    stereoInput.setToggleState(track.input_stereo,juce::dontSendNotification);
                    for(std::size_t i=0;i<outputRoutes.size();++i)
                        if(outputRoutes[i]==track.hardware_outputs)
                            outputRoute.setSelectedId(static_cast<int>(i+1),juce::dontSendNotification);
                    break;
                }
        } else if(owner.liveDocument) {
            const auto saved=live::find(owner.liveDocument->interfaceState,"channels",selectedShowChannel())["liveInput"];
            if(saved.isObject()) {
                const auto profile=live::decodeLiveInputProfile(saved);
                stereoInput.setToggleState(profile.stereo,juce::dontSendNotification);
            }
        }
        for (std::size_t i=0;i<owner.catalog.size();++i) {
            const auto& plugin=owner.catalog[i];
            if (plugin.instrument.value_or(false)) continue;
            pluginIndices.push_back(i);
            vst3.addItem(juce::String::fromUTF8(plugin.name.c_str()),static_cast<int>(pluginIndices.size()));
        }
        if (!pluginIndices.empty()) vst3.setSelectedId(1,juce::dontSendNotification);
        enable.onClick=[this] {
            owner.run([&] {
                if(owner.liveInputPilotTrack) {
                    if(owner.liveInputPilotChannel!=selectedShowChannel())
                        throw std::runtime_error("Disable the active channel before enabling another input");
                    owner.enableLiveInputPilotMonitoring();
                } else {
                    owner.startLiveInputPilot(physicalInput.getSelectedId()-1,selectedShowChannel(),
                                              stereoInput.getToggleState());
                    if(const auto outputs=selectedOutputs(); !outputs.empty())
                        owner.app.set_hardware_output(*owner.liveInputPilotTrack,outputs);
                }
            });
            sync();
        };
        saveProfile.onClick=[this] {
            owner.run([&] {owner.saveLiveInputProfile(selectedShowChannel());});
            sync();
        };
        loadProfile.onClick=[this] {
            owner.run([&] {owner.loadLiveInputProfile(selectedShowChannel());});
            sync();
        };
        showChannel.onChange=[this] { sync(); };
        outputRoute.onChange=[this] {
            if(owner.liveInputPilotTrack && owner.liveInputPilotChannel==selectedShowChannel())
                owner.run([&] { owner.app.set_hardware_output(*owner.liveInputPilotTrack,selectedOutputs()); });
            sync();
        };
        disable.onClick=[this] {
            owner.run([&] { owner.stopLiveInputPilot(); });
            sync();
        };
        addEffect.onClick=[this] {
            if (!owner.liveInputPilotTrack) return;
            const int index=vst3.getSelectedId()-1;
            if (index<0||static_cast<std::size_t>(index)>=pluginIndices.size()) return;
            // Existing drop path probes unknown VST3 classes off RT and rejects VSTi on audio tracks.
            owner.dropPlugin(*owner.liveInputPilotTrack,pluginIndices[static_cast<std::size_t>(index)]);
            sync();
        };
        openEditor.onClick=[this] {
            if (!owner.liveInputPilotTrack) return;
            const auto project=owner.project();
            for (const auto& track:project->tracks)
                if (track.id==*owner.liveInputPilotTrack && !track.inserts.empty()) {
                    const auto insert=track.inserts.back().id;
                    owner.run([&] { owner.openInsert(track.id,insert); });
                    break;
                }
        };
        sync();
    }
    void resized() override {
        auto area=getLocalBounds().reduced(12);
        title.setBounds(area.removeFromTop(28));
        description.setBounds(area.removeFromTop(24));
        channelLabel.setBounds(area.removeFromTop(20));
        showChannel.setBounds(area.removeFromTop(28));
        inputLabel.setBounds(area.removeFromTop(20));
        physicalInput.setBounds(area.removeFromTop(28));
        stereoInput.setBounds(area.removeFromTop(24));
        routeLabel.setBounds(area.removeFromTop(20));
        outputRoute.setBounds(area.removeFromTop(28));
        auto controls=area.removeFromTop(30);
        enable.setBounds(controls.removeFromLeft(200).reduced(1));
        disable.setBounds(controls.reduced(1));
        area.removeFromTop(6);
        vstLabel.setBounds(area.removeFromTop(20));
        vst3.setBounds(area.removeFromTop(28));
        auto effectControls=area.removeFromTop(30);
        addEffect.setBounds(effectControls.removeFromLeft(200).reduced(1));
        openEditor.setBounds(effectControls.reduced(1));
        area.removeFromTop(6);
        auto persistence=area.removeFromTop(30);
        saveProfile.setBounds(persistence.removeFromLeft(200).reduced(1));
        loadProfile.setBounds(persistence.reduced(1));
        status.setBounds(area.removeFromTop(28));
    }
    void sync() {
        const bool active=owner.liveInputPilotTrack.has_value();
        const bool hardware=owner.app.audio_name()!="Offline clock (no sound)" &&
            owner.app.audio_name()!="Disconnected" && owner.app.audio_running();
        const bool stopped=owner.app.engine()->state().playback!=mrs::PlaybackState::playing &&
            !owner.app.recording();
        const bool emptyItem=owner.liveDocument &&
            live::playbackRows(owner.liveDocument->interfaceState,
                owner.liveDocument->interfaceState["currentItem"].toString()).empty();
        const auto channel=selectedShowChannel();
        const bool selectedActive=active && channel==owner.liveInputPilotChannel;
        const auto profile=owner.liveDocument ? live::find(owner.liveDocument->interfaceState,"channels",channel)["liveInput"] : juce::var{};
        bool monitoring=false;
        if(active) for(const auto& track:owner.project()->tracks)
            if(track.id==*owner.liveInputPilotTrack) monitoring=track.input_monitor;
        physicalInput.setEnabled(!active && stopped);
        stereoInput.setEnabled(!active && stopped);
        outputRoute.setEnabled(stopped && (!active || selectedActive));
        showChannel.setEnabled(!active);
        enable.setEnabled(stopped && hardware && emptyItem &&
                          (active ? selectedActive && !monitoring
                                  : physicalInput.getNumItems()>0 && channel.isNotEmpty()));
        disable.setEnabled(active && stopped);
        saveProfile.setEnabled(selectedActive && stopped);
        loadProfile.setEnabled(!active && stopped && hardware && emptyItem && profile.isObject());
        addEffect.setEnabled(selectedActive && stopped && !pluginIndices.empty());
        vst3.setEnabled(selectedActive && stopped && !pluginIndices.empty());
        openEditor.setEnabled(false);
        if(active && !selectedActive) {
            status.setText("Another channel is active: Disable it before switching.",
                           juce::dontSendNotification);
            return;
        }
        if (!active) {
            status.setText(hardware ? (profile.isObject() ? "Saved profile available; Load keeps Monitor OFF."
                                                           : "Choose input/effects, then Save to Show.")
                                    : "Connect a hardware input in Audio settings first.",
                           juce::dontSendNotification);
            return;
        }
        for (const auto& track:owner.project()->tracks)
            if (track.id==*owner.liveInputPilotTrack) {
                juce::String message = track.input_monitor ? "Monitor ON" : "Monitor OFF";
                message += " - input " + juce::String(track.input+1);
                if (!track.inserts.empty()) {
                    const auto& fx=track.inserts.back();
                    message += " - " + juce::String::fromUTF8(fx.plugin_name.c_str());
                    message += " / " + juce::String(owner.app.plugin_latency(track.id,fx.id)) + " samples";
                    openEditor.setEnabled(true);
                }
                status.setText(message,juce::dontSendNotification);
                return;
            }
        status.setText("Pilot track unavailable - review the Core session",juce::dontSendNotification);
    }
private:
    std::vector<int> selectedOutputs() const {
        const int i=outputRoute.getSelectedId()-1;
        return i>=0 && static_cast<std::size_t>(i)<outputRoutes.size()
            ? outputRoutes[static_cast<std::size_t>(i)] : std::vector<int>{};
    }
    juce::String selectedShowChannel() const {
        const int i=showChannel.getSelectedId()-1;
        return i>=0 && static_cast<std::size_t>(i)<channelIds.size()
            ? channelIds[static_cast<std::size_t>(i)] : juce::String{};
    }
    Desktop& owner;
    juce::Label title,description,channelLabel,inputLabel,routeLabel,vstLabel,status;
    juce::ComboBox showChannel,physicalInput,outputRoute,vst3;
    juce::ToggleButton stereoInput;
    juce::TextButton enable,disable,addEffect,openEditor,saveProfile,loadProfile;
    std::vector<std::size_t> pluginIndices;
    std::vector<std::vector<int>> outputRoutes;
    std::vector<juce::String> channelIds;
};
inline void openLiveInputPilot(Desktop& desktop, const juce::String& channel = {}) {
    showLiveContext(std::make_unique<LiveInputPilotPanel>(desktop,channel),desktop,
                    liveContextTarget(juce::Desktop::getMousePosition()));
}
} // namespace ui
