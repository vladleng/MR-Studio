#pragma once
#include "Desktop.h"
#include "LiveContext.h"

namespace ui {
// Show settings. The common Application owns DSP and physical-to-stream mapping.
class LiveAudioPanel final : public juce::Component {
  public:
    explicit LiveAudioPanel(Desktop &desktop) : owner(&desktop) {
        setComponentID("live-audio-settings");
        setSize(380, 246);
        for (auto *component : std::array<juce::Component *, 8>{&title, &musicLabel, &musicGain, &clickEnabled, &accent,
                                                                &clickLevel, &output, &apply})
            addAndMakeVisible(component);
        title.setText("Playback / Click / Cue", juce::dontSendNotification);
        musicLabel.setText("Playback level (excludes Cue / Click)", juce::dontSendNotification);
        musicGain.setRange(-60., 12., .1);
        musicGain.setSliderStyle(juce::Slider::LinearHorizontal);
        musicGain.setTextBoxStyle(juce::Slider::TextBoxRight, false, 74, 22);
        musicGain.setTextValueSuffix(" dB");
        clickLevel.setRange(0, 100, 1);
        clickLevel.setSliderStyle(juce::Slider::LinearHorizontal);
        clickLevel.setTextBoxStyle(juce::Slider::TextBoxRight, false, 74, 22);
        clickLevel.setTextValueSuffix(" %");
        clickEnabled.setButtonText("Click");
        accent.setButtonText("Accent first beat");
        auto click = live::liveClick(desktop.liveDocument->interfaceState);
        musicGain.setValue(live::playbackGain(desktop.liveDocument->interfaceState), juce::dontSendNotification);
        clickEnabled.setToggleState(click.playback, juce::dontSendNotification);
        accent.setToggleState(click.accent, juce::dontSendNotification);
        clickLevel.setValue(click.level, juce::dontSendNotification);
        routes = {{-1, -1}};
        output.addItem("Click → Master", 1);
        const auto active = desktop.app.active_outputs();
        const auto names = desktop.app.output_names();
        const auto name = [&](int channel) {
            return static_cast<std::size_t>(channel) < names.size()
                       ? juce::String::fromUTF8(names[static_cast<std::size_t>(channel)].c_str())
                       : "Out " + juce::String(channel + 1);
        };
        for (std::size_t i = 0; i < active.size(); ++i) {
            routes.push_back({active[i], -1});
            output.addItem(name(active[i]), static_cast<int>(routes.size()));
            if (i + 1 < active.size()) {
                routes.push_back({active[i], active[i + 1]});
                output.addItem(name(active[i]) + " / " + name(active[i + 1]), static_cast<int>(routes.size()));
            }
        }
        int selected = 0;
        for (std::size_t i = 0; i < routes.size(); ++i)
            if (routes[i] == std::pair{click.output_left, click.output_right})
                selected = static_cast<int>(i + 1);
        output.setSelectedId(selected, juce::dontSendNotification);
        output.setTextWhenNothingSelected("Click output unavailable — choose a route");
        const bool canRoute = desktop.app.engine()->state().playback != mrs::PlaybackState::playing;
        output.setEnabled(canRoute);
        apply.setButtonText("Apply");
        apply.onClick = [this, click]() mutable {
            if (!owner || !owner->liveDocument)
                return;
            const int index = output.getSelectedId() - 1;
            if (index < 0 || index >= static_cast<int>(routes.size()))
                return;
            click.playback = clickEnabled.getToggleState();
            click.accent = accent.getToggleState();
            click.level = static_cast<int>(clickLevel.getValue());
            click.output_left = routes[static_cast<std::size_t>(index)].first;
            click.output_right = routes[static_cast<std::size_t>(index)].second;
            owner->run([&] { owner->setLiveAudio(musicGain.getValue(), click); });
        };
        apply.setEnabled(!desktop.app.recording());
        for (auto *component :
             std::array<juce::Component *, 5>{&musicGain, &clickEnabled, &accent, &clickLevel, &output})
            component->setComponentID("live-audio-" + juce::String(component == &musicGain    ? "gain"
                                                                   : component == &output     ? "output"
                                                                   : component == &clickLevel ? "level"
                                                                   : component == &accent     ? "accent"
                                                                                              : "click"));
    }
    void resized() override {
        auto area = getLocalBounds().reduced(12);
        title.setBounds(area.removeFromTop(26));
        musicLabel.setBounds(area.removeFromTop(22));
        musicGain.setBounds(area.removeFromTop(30));
        auto row = area.removeFromTop(28);
        clickEnabled.setBounds(row.removeFromLeft(110));
        accent.setBounds(row);
        clickLevel.setBounds(area.removeFromTop(30));
        output.setBounds(area.removeFromTop(28).reduced(1));
        area.removeFromTop(8);
        apply.setBounds(area.removeFromTop(28));
    }

  private:
    juce::Component::SafePointer<Desktop> owner;
    juce::Label title, musicLabel;
    juce::Slider musicGain, clickLevel;
    juce::ToggleButton clickEnabled, accent;
    juce::ComboBox output;
    juce::TextButton apply;
    std::vector<std::pair<int, int>> routes;
};
inline void openLiveAudio(Desktop &owner) {
    if (owner.liveDocument)
        showLiveContext(std::make_unique<LiveAudioPanel>(owner), owner, liveContextTarget(juce::Desktop::getMousePosition()));
}
} // namespace ui
