#pragma once
#include "Desktop.h"
#include "WaveformView.h"
namespace ui {
class LivePlaybackPanel final : public juce::Component {
  public:
    explicit LivePlaybackPanel(Desktop &desktop) : owner(desktop) {
        setComponentID("live-playback-panel");
        for (auto *c : std::array<juce::Component *, 15>{&chooseFile, &fileName, &gain, &mute, &solo, &trimStart,
                                                         &trimEnd, &trimApply, &startLabel, &endLabel, &stemChoice,
                                                         &addStem, &removeStem, &outputChoice, &cue})
            addAndMakeVisible(c);
        chooseFile.setButtonText("Choose WAV...");
        chooseFile.onClick = [this] {
            if (!owner.liveDocument)
                return;
            const auto id = owner.liveDocument->interfaceState["currentItem"].toString(), stem = owner.livePlaybackStem;
            juce::Component::SafePointer<Desktop> safe(&owner);
            owner.choose(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safe, id, stem](const auto &file) {
                    if (safe)
                        safe->run([&] { safe->attachLivePlayback(id, file, stem); });
                },
                "*.wav");
        };
        stemChoice.setComponentID("live-stem-choice");
        addStem.setComponentID("live-add-stems");
        removeStem.setComponentID("live-remove-stem");
        outputChoice.setComponentID("live-stem-output");
        stemChoice.onChange = [this] {
            const int index = stemChoice.getSelectedId() - 1;
            if (index >= 0 && index < static_cast<int>(stemKeys.size())) {
                owner.livePlaybackStem = stemKeys[static_cast<std::size_t>(index)];
                owner.refresh();
            }
        };
        addStem.onClick = [this] {
            if (!owner.liveDocument)
                return;
            const auto id = owner.liveDocument->interfaceState["currentItem"].toString();
            stemChooser = std::make_unique<juce::FileChooser>("Add WAV stems", juce::File(), "*.wav", true);
            juce::Component::SafePointer<Desktop> safe(&owner);
            stemChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles |
                                         juce::FileBrowserComponent::canSelectMultipleItems,
                                     [safe, id](const auto &result) {
                                         if (safe)
                                             safe->run([&] { safe->addLivePlaybackStems(id, result.getResults()); });
                                     });
        };
        removeStem.onClick = [this] { owner.run([&] { owner.removeLivePlaybackStem(); }); };
        outputChoice.onChange = [this] {
            const int index = outputChoice.getSelectedId() - 1;
            if (index >= 0 && index < static_cast<int>(routes.size()))
                owner.run([&] { owner.setLivePlaybackOutputs(routes[static_cast<std::size_t>(index)]); });
        };
        trimStart.setComponentID("live-trim-start");
        trimEnd.setComponentID("live-trim-end");
        trimApply.setComponentID("live-trim-apply");
        trimStart.setTooltip("Trim Start, seconds in the original WAV");
        trimEnd.setTooltip("Trim End, seconds in the original WAV");
        trimApply.setTooltip("Apply nondestructive trim (pause or stop first)");
        trimStart.onReturnKey = trimEnd.onReturnKey = [this] { commitTrim(); };
        trimApply.onClick = [this] { commitTrim(); };
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
        cue.setComponentID("live-stem-cue");
        cue.setClickingTogglesState(true);
        cue.setTooltip("Cue WAV: independent of the global Playback level; choose its output separately");
        cue.onClick = [this] { owner.run([&] { owner.setLiveCue(cue.getToggleState()); }); };
        mute.setClickingTogglesState(true);
        solo.setClickingTogglesState(true);
    }
    void resized() override {
        auto area = getLocalBounds().reduced(8);
        auto header = area.removeFromTop(24);
        header.removeFromLeft(124);
        removeStem.setBounds(header.removeFromRight(24).reduced(1));
        addStem.setBounds(header.removeFromRight(24).reduced(1));
        stemChoice.setBounds(header.reduced(1));
        auto row = area.removeFromTop(28);
        chooseFile.setBounds(row.removeFromRight(110));
        fileName.setBounds(row);
        row = area.removeFromTop(28);
        mute.setBounds(row.removeFromLeft(28).reduced(1));
        solo.setBounds(row.removeFromLeft(28).reduced(1));
        cue.setBounds(row.removeFromLeft(44).reduced(1));
        outputChoice.setBounds(row.reduced(1));
        gain.setBounds(area.removeFromTop(30));
        row = area.removeFromTop(26);
        trimApply.setBounds(row.removeFromRight(52).reduced(1));
        auto startArea = row.removeFromLeft(row.getWidth() / 2);
        startLabel.setBounds(startArea.removeFromLeft(20));
        trimStart.setBounds(startArea.reduced(1));
        endLabel.setBounds(row.removeFromLeft(26));
        trimEnd.setBounds(row.reduced(1));
    }
    void paint(juce::Graphics &g) override {
        g.setColour(juce::Colour(0xffdae5f4));
        g.setFont(owner.theme.font(12.f));
        g.drawText("Playback / stems", 8, 4, getWidth() - 16, 22, juce::Justification::left);
        const float peak = owner.livePlaybackReady() ? juce::jmax(owner.peaks[owner.livePlaybackIndex()].left,
                                                                  owner.peaks[owner.livePlaybackIndex()].right)
                                                     : 0.f;
        auto bar = getLocalBounds().reduced(10).withTrimmedTop(144).withHeight(6).toFloat();
        g.setColour(juce::Colour(0xff343638));
        g.fillRect(bar);
        bar.setWidth(bar.getWidth() * juce::jlimit(0.f, 1.f, peak));
        g.setColour(juce::Colour(0xff59ad87));
        g.fillRect(bar);
        if (owner.livePlaybackReady()) {
            const auto clip = owner.project()->clips[owner.livePlaybackIndex()];
            const auto area = getLocalBounds().reduced(10).withTrimmedTop(158).toFloat();
            if (const auto *wave = owner.app.clip_waveform(clip)) {
                g.setColour(juce::Colour(0xff17b5da));
                drawWaveform(g, *wave, area, static_cast<double>(clip.source_offset),
                             static_cast<double>(clip.length) / juce::jmax(1.f, area.getWidth()));
            }
            const auto fraction = juce::jlimit(0., 1.,
                                               static_cast<double>(owner.displayState().sample - clip.start) /
                                                   static_cast<double>(juce::jmax<mrs::Sample>(1, clip.length)));
            g.setColour(juce::Colours::white);
            g.drawVerticalLine(juce::roundToInt(area.getX() + area.getWidth() * fraction), area.getY(),
                               area.getBottom());
        }
        const auto text = owner.livePlaybackLoading ? juce::String("Loading WAV...") : owner.livePlaybackError;
        g.setColour(juce::Colour(0xffdae5f4));
        g.drawFittedText(text, getLocalBounds().reduced(8).withTrimmedTop(158), juce::Justification::topLeft, 3);
    }
    void sync() {
        const auto id =
            owner.liveDocument ? owner.liveDocument->interfaceState["currentItem"].toString() : juce::String();
        const auto rows =
            owner.liveDocument ? live::playbackRows(owner.liveDocument->interfaceState, id) : std::vector<juce::var>();
        const auto row = owner.liveDocument
                             ? live::playbackStem(owner.liveDocument->interfaceState, id, owner.livePlaybackStem)
                             : juce::var();
        juce::String nextStems = id;
        for (const auto &entry : rows)
            nextStems += entry["id"].toString() + entry["file"].toString();
        if (nextStems != stemSignature) {
            stemSignature = nextStems;
            stemChoice.clear(juce::dontSendNotification);
            stemKeys.clear();
            for (std::size_t i = 0; i < rows.size(); ++i) {
                stemKeys.push_back(i == 0 ? juce::String() : rows[i]["id"].toString());
                stemChoice.addItem(juce::File(rows[i]["file"].toString()).getFileNameWithoutExtension(),
                                   static_cast<int>(i + 1));
            }
        }
        stemChoice.setSelectedId(rows.empty() ? 0 : static_cast<int>(owner.livePlaybackIndex() + 1),
                                 juce::dontSendNotification);
        addStem.setEnabled(id.isNotEmpty() && rows.size() < 32 && !owner.app.recording());
        removeStem.setEnabled(owner.livePlaybackIndex() > 0 && !owner.app.recording());
        const auto active = owner.app.active_outputs();
        juce::String routeKey;
        for (const auto &name : owner.app.output_names())
            routeKey += juce::String::fromUTF8(name.c_str()) + "|";
        for (int output : active)
            routeKey += juce::String(output) + ":";
        if (routeKey != outputSignature || routes.empty()) {
            outputSignature = routeKey;
            routes = {{}};
            outputChoice.clear(juce::dontSendNotification);
            outputChoice.addItem("Master", 1);
            const auto names = owner.app.output_names();
            const auto name = [&](int output) {
                return output >= 0 && static_cast<std::size_t>(output) < names.size()
                           ? juce::String::fromUTF8(names[static_cast<std::size_t>(output)].c_str())
                           : "Out " + juce::String(output + 1);
            };
            for (std::size_t i = 0; i < active.size(); ++i) {
                routes.push_back({active[i]});
                outputChoice.addItem(name(active[i]), static_cast<int>(routes.size()));
                if (i + 1 < active.size()) {
                    routes.push_back({active[i], active[i + 1]});
                    outputChoice.addItem(name(active[i]) + " / " + name(active[i + 1]),
                                         static_cast<int>(routes.size()));
                }
            }
        }
        int routeId = 0;
        for (std::size_t i = 0; i < routes.size(); ++i)
            if (routes[i] == live::playbackOutputs(row))
                routeId = static_cast<int>(i + 1);
        outputChoice.setSelectedId(routeId, juce::dontSendNotification);
        outputChoice.setTextWhenNothingSelected("Output unavailable");
        outputChoice.setEnabled(row.isObject() && !owner.app.recording() &&
                                owner.app.engine()->state().playback != mrs::PlaybackState::playing);
        chooseFile.setEnabled(id.isNotEmpty() && !owner.app.recording());
        fileName.setText(row.isObject() ? juce::File(row["file"].toString()).getFileName() : "No audio connected",
                         juce::dontSendNotification);
        cue.setEnabled(row.isObject() && !owner.app.recording());
        cue.setToggleState(static_cast<bool>(row["cue"]), juce::dontSendNotification);
        const bool ready = owner.livePlaybackReady();
        gain.setEnabled(ready);
        mute.setEnabled(ready);
        solo.setEnabled(ready);
        if (!gain.isMouseButtonDown())
            gain.setValue(row.isObject() ? static_cast<double>(row["gain"]) : 0., juce::dontSendNotification);
        mute.setToggleState(static_cast<bool>(row["mute"]), juce::dontSendNotification);
        solo.setToggleState(static_cast<bool>(row["solo"]), juce::dontSendNotification);
        const bool canTrim =
            ready && !owner.app.recording() && owner.app.engine()->state().playback != mrs::PlaybackState::playing;
        trimStart.setEnabled(canTrim);
        trimEnd.setEnabled(canTrim);
        trimApply.setEnabled(canTrim);
        if (ready) {
            const auto project = owner.project();
            const auto clip = project->clips[owner.livePlaybackIndex()];
            const auto nextTrim =
                id + owner.livePlaybackStem + ":" + juce::String(clip.start) + ":" + juce::String(clip.length);
            if (nextTrim != trimSignature) {
                trimSignature = nextTrim;
                trimStart.setText(juce::String(static_cast<double>(clip.start) / project->sample_rate, 6), false);
                trimEnd.setText(juce::String(static_cast<double>(clip.start + clip.length) / project->sample_rate, 6),
                                false);
            }
        } else {
            trimSignature.clear();
            trimStart.setText({}, false);
            trimEnd.setText({}, false);
        }
        repaint();
    }

  private:
    void commitTrim() {
        owner.run([&] {
            // JSON::parse accepts objects/arrays, not scalar JSON numbers.
            const auto values = juce::JSON::parse("[" + trimStart.getText() + "," + trimEnd.getText() + "]");
            if (!values.isArray() || values.size() != 2 || !live::number(values[0]) || !live::number(values[1]))
                throw std::runtime_error("Enter Trim Start and End in seconds");
            owner.setLivePlaybackTrim(static_cast<double>(values[0]), static_cast<double>(values[1]));
        });
    }
    void commit() {
        owner.run([&] { owner.setLivePlaybackMix(gain.getValue(), mute.getToggleState(), solo.getToggleState()); });
    }
    Desktop &owner;
    juce::TextButton chooseFile, mute{"M"}, solo{"S"}, cue{"Cue"};
    juce::Label fileName;
    juce::Slider gain;
    juce::TextEditor trimStart, trimEnd;
    juce::TextButton trimApply{"Trim"};
    juce::Label startLabel{{}, "In"}, endLabel{{}, "Out"};
    juce::String trimSignature, stemSignature, outputSignature;
    juce::ComboBox stemChoice, outputChoice;
    juce::TextButton addStem{"+"}, removeStem{"-"};
    std::vector<juce::String> stemKeys;
    std::vector<std::vector<int>> routes;
    std::unique_ptr<juce::FileChooser> stemChooser;
};
} // namespace ui
