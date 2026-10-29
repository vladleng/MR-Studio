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
    explicit LiveInputPilotPanel(Desktop& desktop) : owner(desktop) {
        setComponentID("live-input-pilot");
        setSize(440, 248);
        title.setText("Live Input - one channel (pilot)", juce::dontSendNotification);
        description.setText("Empty item only. Session-only; disable before switching items.", juce::dontSendNotification);
        description.setJustificationType(juce::Justification::centredLeft);
        inputLabel.setText("Physical input", juce::dontSendNotification);
        vstLabel.setText("VST3 insert", juce::dontSendNotification);
        for (auto* component : std::array<juce::Component*, 9>{
            &title, &description, &inputLabel, &physicalInput, &enable,
            &disable, &vstLabel, &vst3, &addEffect}) addAndMakeVisible(component);
        addAndMakeVisible(openEditor);
        addAndMakeVisible(status);
        enable.setButtonText("Enable Monitor");
        disable.setButtonText("Disable");
        addEffect.setButtonText("Add VST3");
        openEditor.setButtonText("Open editor");
        physicalInput.setComponentID("live-input-physical");
        enable.setComponentID("live-input-enable");
        disable.setComponentID("live-input-disable");
        vst3.setComponentID("live-input-plugin");
        addEffect.setComponentID("live-input-add-vst");
        const auto names=owner.app.input_names();
        for (std::size_t i=0;i<names.size();++i)
            physicalInput.addItem(juce::String(i+1) + " - " +
                juce::String::fromUTF8(names[i].c_str()),static_cast<int>(i+1));
        if (!names.empty()) physicalInput.setSelectedId(1,juce::dontSendNotification);
        for (std::size_t i=0;i<owner.catalog.size();++i) {
            const auto& plugin=owner.catalog[i];
            if (plugin.instrument.value_or(false)) continue;
            pluginIndices.push_back(i);
            vst3.addItem(juce::String::fromUTF8(plugin.name.c_str()),static_cast<int>(pluginIndices.size()));
        }
        if (!pluginIndices.empty()) vst3.setSelectedId(1,juce::dontSendNotification);
        enable.onClick=[this] {
            owner.run([&] { owner.startLiveInputPilot(physicalInput.getSelectedId()-1); });
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
            owner.addPlugin(*owner.liveInputPilotTrack,pluginIndices[static_cast<std::size_t>(index)]);
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
        description.setBounds(area.removeFromTop(25));
        inputLabel.setBounds(area.removeFromTop(20));
        physicalInput.setBounds(area.removeFromTop(28));
        auto controls=area.removeFromTop(30);
        enable.setBounds(controls.removeFromLeft(200).reduced(1));
        disable.setBounds(controls.reduced(1));
        area.removeFromTop(6);
        vstLabel.setBounds(area.removeFromTop(20));
        vst3.setBounds(area.removeFromTop(28));
        auto effectControls=area.removeFromTop(30);
        addEffect.setBounds(effectControls.removeFromLeft(200).reduced(1));
        openEditor.setBounds(effectControls.reduced(1));
        status.setBounds(area.removeFromTop(30));
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
        physicalInput.setEnabled(!active && stopped);
        enable.setEnabled(!active && stopped && hardware && emptyItem && physicalInput.getNumItems()>0);
        disable.setEnabled(active && stopped);
        addEffect.setEnabled(active && stopped && !pluginIndices.empty());
        vst3.setEnabled(active && stopped && !pluginIndices.empty());
        openEditor.setEnabled(false);
        if (!active) {
            status.setText(hardware ? "Choose a hardware input; plugin list uses the shared VST3 catalog."
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
    Desktop& owner;
    juce::Label title,description,inputLabel,vstLabel,status;
    juce::ComboBox physicalInput,vst3;
    juce::TextButton enable,disable,addEffect,openEditor;
    std::vector<std::size_t> pluginIndices;
};
inline void openLiveInputPilot(Desktop& desktop) {
    showLiveContext(std::make_unique<LiveInputPilotPanel>(desktop),desktop,
                    liveContextTarget(juce::Desktop::getMousePosition()));
}
} // namespace ui
