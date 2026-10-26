#pragma once
#include "Desktop.h"
#include "WaveformView.h"
namespace ui {
class LivePlaybackPanel final : public juce::Component {
  public:
    explicit LivePlaybackPanel(Desktop &desktop) : owner(desktop) {
        setComponentID("live-playback-panel");
        for (auto *c : std::array<juce::Component *, 5>{&chooseFile, &fileName, &gain, &mute, &solo})
            addAndMakeVisible(c);
        chooseFile.setButtonText("Choose WAV...");
        chooseFile.onClick = [this] {
            if (!owner.liveDocument)
                return;
            const auto id = owner.liveDocument->interfaceState["currentItem"].toString();
            juce::Component::SafePointer<Desktop> safe(&owner);
            owner.choose(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safe, id](const auto &file) {
                    if (safe)
                        safe->run([&] { safe->attachLivePlayback(id, file); });
                },
                "*.wav");
        };
        gain.setRange(-60, 12, .1);
        gain.setSliderStyle(juce::Slider::LinearHorizontal);
        gain.setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, 22);
        gain.setTextValueSuffix(" dB");
        gain.onDragEnd = [this] { commit(); };
        gain.onValueChange = [this] {
            if (!gain.isMouseButtonDown())
                commit();
        };
        mute.onClick = [this] { commit(); };
        solo.onClick = [this] { commit(); };
        mute.setClickingTogglesState(true);
        solo.setClickingTogglesState(true);
    }
    void resized() override {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(24);
        auto row = area.removeFromTop(28);
        chooseFile.setBounds(row.removeFromRight(110));
        fileName.setBounds(row);
        row = area.removeFromTop(28);
        mute.setBounds(row.removeFromLeft(28).reduced(1));
        solo.setBounds(row.removeFromLeft(28).reduced(1));
        gain.setBounds(area.removeFromTop(30));
    }
    void paint(juce::Graphics &g) override {
        g.setColour(juce::Colour(0xffdae5f4));
        g.setFont(owner.theme.font(12.f));
        g.drawText("Playback / stems", 8, 4, getWidth() - 16, 22, juce::Justification::left);
        const float peak = owner.livePlaybackReady() ? juce::jmax(owner.peaks[0].left, owner.peaks[0].right) : 0.f;
        auto bar = getLocalBounds().reduced(10).withTrimmedTop(116).withHeight(6).toFloat();
        g.setColour(juce::Colour(0xff343638));
        g.fillRect(bar);
        bar.setWidth(bar.getWidth() * juce::jlimit(0.f, 1.f, peak));
        g.setColour(juce::Colour(0xff59ad87));
        g.fillRect(bar);
        if (owner.livePlaybackReady()) {
            const auto clip = owner.project()->clips.front();
            const auto area = getLocalBounds().reduced(10).withTrimmedTop(130).toFloat();
            if (const auto *wave = owner.app.clip_waveform(clip)) {
                g.setColour(juce::Colour(0xff17b5da));
                drawWaveform(g, *wave, area, 0., static_cast<double>(clip.length) / juce::jmax(1.f, area.getWidth()));
            }
            const auto fraction = juce::jlimit(
                0., 1., static_cast<double>(owner.displayState().sample) / static_cast<double>(juce::jmax<mrs::Sample>(1, clip.length)));
            g.setColour(juce::Colours::white);
            g.drawVerticalLine(juce::roundToInt(area.getX() + area.getWidth() * fraction), area.getY(),
                               area.getBottom());
        }
        const auto text = owner.livePlaybackLoading ? juce::String("Loading WAV...") : owner.livePlaybackError;
        g.setColour(juce::Colour(0xffdae5f4));
        g.drawFittedText(text, getLocalBounds().reduced(8).withTrimmedTop(130), juce::Justification::topLeft, 3);
    }
    void sync() {
        const auto id =
            owner.liveDocument ? owner.liveDocument->interfaceState["currentItem"].toString() : juce::String();
        const auto row = owner.liveDocument ? live::playback(owner.liveDocument->interfaceState, id) : juce::var();
        chooseFile.setEnabled(id.isNotEmpty() && !owner.app.recording());
        fileName.setText(row.isObject() ? juce::File(row["file"].toString()).getFileName() : "No audio connected",
                         juce::dontSendNotification);
        const bool ready = owner.livePlaybackReady();
        gain.setEnabled(ready);
        mute.setEnabled(ready);
        solo.setEnabled(ready);
        if (!gain.isMouseButtonDown())
            gain.setValue(row.isObject() ? static_cast<double>(row["gain"]) : 0., juce::dontSendNotification);
        mute.setToggleState(static_cast<bool>(row["mute"]), juce::dontSendNotification);
        solo.setToggleState(static_cast<bool>(row["solo"]), juce::dontSendNotification);
        repaint();
    }

  private:
    void commit() {
        owner.run([&] { owner.setLivePlaybackMix(gain.getValue(), mute.getToggleState(), solo.getToggleState()); });
    }
    Desktop &owner;
    juce::TextButton chooseFile, mute{"M"}, solo{"S"};
    juce::Label fileName;
    juce::Slider gain;
};
} // namespace ui
