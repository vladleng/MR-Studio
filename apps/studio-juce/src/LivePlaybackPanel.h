#pragma once
#include "Desktop.h"
#include "WaveformView.h"
#include "LivePlaybackView.h"
namespace ui {
class LivePlaybackPanel final : public juce::Component {
  public:
    explicit LivePlaybackPanel(Desktop &desktop) : owner(desktop) {
        setComponentID("live-playback-panel");
        for (auto *c : std::array<juce::Component *, 14>{&chooseFile, &fileName, &trimStart, &trimEnd,
                   &trimApply, &startLabel, &endLabel, &addStem, &removeStem, &outputChoice, &cue, &groupGain, &groupLabel, &audition})
            addAndMakeVisible(c);
        addAndMakeVisible(stemsViewport);
        addAndMakeVisible(detailsClose);
        stemsViewport.setViewedComponent(&stemsBody, false);
        stemsViewport.setScrollBarsShown(true, false);
        groupLabel.setText("Item Playback", juce::dontSendNotification);
        audition.setComponentID("live-browser-audition");
        audition.setButtonText("Listen");
        audition.setTooltip("Audition the first WAV from Browser without activating the setlist or show transport");
        audition.onClick = [this] { owner.run([&] { auditionStem({}); }); };
        groupGain.setComponentID("live-item-playback-gain");
        groupGain.setTitle("Playback level for this item");
        groupGain.setRange(-60, 12, .1);
        groupGain.setSliderStyle(juce::Slider::LinearHorizontal);
        groupGain.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 20);
        groupGain.setTextValueSuffix(" dB");
        groupGain.onDragEnd = [this] { commitGroup(); };
        groupGain.onValueChange = [this] { if (!groupGain.isMouseButtonDown()) commitGroup(); };
        detailsClose.setTitle("Close stem settings");
        detailsClose.onClick = [this] { detailsVisible = false; requestLayout(); };
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
        addStem.setComponentID("live-add-stems");
        removeStem.setComponentID("live-remove-stem");
        outputChoice.setComponentID("live-stem-output");
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
        cue.setComponentID("live-stem-cue");
        cue.setClickingTogglesState(true);
        cue.setTooltip("Cue WAV: independent of the global Playback level; choose its output separately");
        cue.onClick = [this] { owner.run([&] { owner.setLiveCue(cue.getToggleState()); }); };
    }
    ~LivePlaybackPanel() override { stemsViewport.setViewedComponent(nullptr, false); }
    std::function<void()> layoutChanged;
    int preferredHeight() const { return detailsVisible ? 370 : 270; }
    int stemCount() const { return static_cast<int>(stemRows.size()); }
    void showStemDetails(const juce::String &stem) {
        if (inspecting()) browserStem = stem;
        else owner.livePlaybackStem = stem;
        detailsVisible = true;
        requestLayout();
        sync();
    }
    void resized() override {
        auto area = getLocalBounds().reduced(8);
        auto header = area.removeFromTop(24);
        addStem.setBounds(header.removeFromRight(26).reduced(1));
        audition.setBounds(header.removeFromRight(70).reduced(1));
        auto group = area.removeFromTop(28);
        groupLabel.setBounds(group.removeFromLeft(82));
        groupGain.setBounds(group);
        area.removeFromTop(4);
        if (detailsVisible) {
            auto details = area.removeFromBottom(96);
            auto row = details.removeFromTop(26);
            detailsClose.setBounds(row.removeFromRight(24));
            removeStem.setBounds(row.removeFromRight(24));
            chooseFile.setBounds(row.removeFromRight(110));
            fileName.setBounds(row);
            row = details.removeFromTop(28);
            cue.setBounds(row.removeFromLeft(44).reduced(1));
            outputChoice.setBounds(row.reduced(1));
            row = details.removeFromTop(26);
            trimApply.setBounds(row.removeFromRight(52).reduced(1));
            auto start = row.removeFromLeft(row.getWidth() / 2);
            startLabel.setBounds(start.removeFromLeft(20)); trimStart.setBounds(start.reduced(1));
            endLabel.setBounds(row.removeFromLeft(26)); trimEnd.setBounds(row.reduced(1));
        } else if (stemRows.empty()) chooseFile.setBounds(area.removeFromTop(28));
        stemsViewport.setBounds(area);
        const int width = juce::jmax(1, stemsViewport.getMaximumVisibleWidth());
        int y = 0;
        for (auto &row : stemRows) { row->setBounds(0, y, width, 58); y += 58; }
        stemsBody.setSize(width, juce::jmax(y, stemsViewport.getHeight()));
    }
    void paint(juce::Graphics &g) override {
        g.setColour(juce::Colour(0xffdae5f4));
        g.setFont(owner.theme.font(12.f));
        juce::String caption = "Playback / stems";
        if (inspecting() && owner.liveDocument) {
            const auto item = live::find(owner.liveDocument->interfaceState, "items", owner.liveBrowserItem);
            caption += " - Browser: " + item["name"].toString() + " | " +
                       item["tempo"].toString() + " BPM | " + item["meter"].toString();
        }
        g.drawFittedText(caption, 8, 4, juce::jmax(0, getWidth() - 112), 22,
                         juce::Justification::left, 1);
        if (stemRows.empty()) {
            const auto message = inspecting() ? juce::String("No WAV or stems attached to this Browser item") :
                owner.livePlaybackLoading ? juce::String("Preparing Playback...") : owner.livePlaybackError;
            g.drawFittedText(message, getLocalBounds().reduced(8).withTrimmedTop(100), juce::Justification::topLeft, 3);
        }
    }
    void sync() {
        // Display polling may retain the last snapshot while audio publishes;
        // never use the throwing control-thread state() read from a UI timer.
        const bool preview = inspecting();
        const auto id = owner.liveDocument
            ? (preview ? owner.liveBrowserItem : owner.liveDocument->interfaceState["currentItem"].toString())
            : juce::String();
        if (id != currentItem || preview != previousInspecting) {
            currentItem = id; previousInspecting = preview; detailsVisible = false;
            browserStem.clear(); previewFile.clear(); requestLayout();
        }
        groupLabel.setText(preview ? "Browser WAV" : "Item Playback", juce::dontSendNotification);
        groupGain.setEnabled(id.isNotEmpty() && !preview);
        if (!groupGain.isMouseButtonDown() && owner.liveDocument)
            groupGain.setValue(live::itemPlaybackGain(owner.liveDocument->interfaceState, id), juce::dontSendNotification);
        const auto rows = owner.liveDocument ? live::playbackRows(owner.liveDocument->interfaceState, id) : std::vector<juce::var>();
        const auto row = owner.liveDocument ? live::playbackStem(owner.liveDocument->interfaceState, id,
            preview ? browserStem : owner.livePlaybackStem) : juce::var();
        juce::String nextStems = id;
        for (const auto &entry : rows) nextStems += "|" + entry["id"].toString();
        if (nextStems != stemSignature) {
            stemSignature = nextStems;
            stemRows.clear();
            for (std::size_t i = 0; i < rows.size(); ++i) {
                auto stemRow = std::make_unique<StemRow>(*this, id, i == 0 ? juce::String() : rows[i]["id"].toString());
                stemsBody.addAndMakeVisible(*stemRow); stemRows.push_back(std::move(stemRow));
            }
            resized();
        }
        std::vector<juce::String> files;
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (preview || !owner.livePlaybackReady() || owner.livePlaybackItem != id || i >= owner.app.playback_sources().size() || !owner.app.clip_waveform(owner.app.playback_sources()[i]))
                files.push_back(rows[i]["file"].toString());
        waveforms.sync(files);
        for (auto &stemRow : stemRows) stemRow->sync();
        audition.setVisible(preview);
        const bool canListen = preview && !rows.empty() && !owner.app.recording() &&
            owner.displayState().playback != mrs::PlaybackState::playing && owner.app.audio_running();
        audition.setEnabled(canListen);
        audition.setButtonText(preview && owner.app.library_preview_state().playing ? "Stop" : "Listen");
        for (auto *c : std::array<juce::Component *, 12>{&chooseFile, &fileName, &trimStart, &trimEnd, &trimApply,
                          &startLabel, &endLabel, &removeStem, &outputChoice, &cue, &detailsClose, &stemsViewport})
            c->setVisible(c == &stemsViewport || (c == &chooseFile && rows.empty()) || detailsVisible);
        chooseFile.setButtonText(rows.empty() ? "Choose WAV..." : "Replace WAV...");
        addStem.setEnabled(id.isNotEmpty() && !preview && rows.size() < 32 && !owner.app.recording());
        removeStem.setEnabled(!preview && owner.livePlaybackIndex() > 0 && !owner.app.recording());
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
        outputChoice.setEnabled(!preview && row.isObject() && !owner.app.recording() &&
                                owner.displayState().playback != mrs::PlaybackState::playing);
        chooseFile.setEnabled(id.isNotEmpty() && !preview && !owner.app.recording());
        fileName.setText(row.isObject() ? juce::File(row["file"].toString()).getFileName() : "No audio connected",
                         juce::dontSendNotification);
        cue.setEnabled(!preview && row.isObject() && !owner.app.recording());
        cue.setToggleState(static_cast<bool>(row["cue"]), juce::dontSendNotification);
        const bool ready = !preview && owner.livePlaybackReady() && owner.livePlaybackItem == id;
        const bool canTrim =
            ready && !live::playbackRegions(owner.liveDocument->interfaceState,id,row).isArray() && !owner.app.recording() && owner.displayState().playback != mrs::PlaybackState::playing;
        trimStart.setEnabled(canTrim);
        trimEnd.setEnabled(canTrim);
        trimApply.setEnabled(canTrim);
        const auto trimHint=row.isObject() && live::playbackRegions(owner.liveDocument->interfaceState,id,row).isArray()
            ? "Edit cut regions using their timeline edges or Region properties" : "Source window in seconds";
        for(auto* field:{&trimStart,&trimEnd})field->setTooltip(trimHint);
        trimApply.setTooltip(trimHint);
        if (ready) {
            const auto project = owner.project();
            const auto clip = owner.app.playback_sources()[owner.livePlaybackIndex()];
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
    class StemRow final : public juce::Component {
      public:
        StemRow(LivePlaybackPanel &playbackView, juce::String item, juce::String stem)
            : host(playbackView), itemId(std::move(item)), stemId(std::move(stem)) {
            const auto key = itemId + ":" + (stemId.isEmpty() ? "primary" : stemId);
            setComponentID("live-stem-row-" + key);
            for (auto *c : std::array<juce::Component *, 5>{&level, &mute, &solo, &details, &listen}) addAndMakeVisible(c);
            level.setComponentID("live-stem-level-" + key);
            level.setTitle("Stem level"); level.setRange(-60, 12, .1);
            level.setSliderStyle(juce::Slider::LinearHorizontal);
            level.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 18); level.setTextValueSuffix(" dB");
            level.onDragEnd = [this] { commit(); };
            level.onValueChange = [this] { if (!level.isMouseButtonDown()) commit(); };
            for (auto *button : {&mute, &solo}) { button->setClickingTogglesState(true); button->onClick = [this] { commit(); }; }
            listen.setButtonText("Play");
            listen.setComponentID("live-stem-audition-" + key);
            listen.setTitle("Listen to this Browser WAV stem without switching the show");
            listen.onClick = [this] { host.owner.run([&] { host.auditionStem(stemId); }); };
            details.setTitle("Stem file, trim and output settings");
            details.onClick = [this] { host.showStemDetails(stemId); };
        }
        void resized() override {
            auto header = getLocalBounds().removeFromTop(20);
            details.setBounds(header.removeFromRight(24).reduced(1));
            listen.setBounds(header.removeFromRight(42).reduced(1));
            solo.setBounds(header.removeFromRight(24).reduced(1)); mute.setBounds(header.removeFromRight(24).reduced(1));
            level.setBounds(2, 36, getWidth() - 4, 20);
        }
        void sync() {
            if (!host.owner.liveDocument) return;
            const auto row = live::playbackStem(host.owner.liveDocument->interfaceState, itemId, stemId);
            if (!level.isMouseButtonDown()) level.setValue(static_cast<double>(row["gain"]), juce::dontSendNotification);
            mute.setToggleState(static_cast<bool>(row["mute"]), juce::dontSendNotification);
            solo.setToggleState(static_cast<bool>(row["solo"]), juce::dontSendNotification);
            setEnabled(row.isObject());
            const bool preview = host.inspecting();
            for (auto *control : std::array<juce::Component *, 3>{&level, &mute, &solo})
                control->setEnabled(!preview);
            listen.setVisible(preview);
            listen.setEnabled(preview && row["file"].isString() && !host.owner.app.recording() &&
                host.owner.displayState().playback != mrs::PlaybackState::playing && host.owner.app.audio_running());
            listen.setButtonText(host.previewFile == row["file"].toString() &&
                host.owner.app.library_preview_state().playing ? "Stop" : "Play");
            repaint();
        }
        void paint(juce::Graphics &g) override {
            if (!host.owner.liveDocument) return;
            const auto state = host.owner.liveDocument->interfaceState;
            const auto rows = live::playbackRows(state, itemId);
            std::size_t index = rows.size();
            for (std::size_t i = 0; i < rows.size(); ++i)
                if ((i == 0 && stemId.isEmpty()) || (i > 0 && rows[i]["id"].toString() == stemId)) { index = i; break; }
            if (index == rows.size()) return;
            g.setColour(juce::Colour(0xffdae5f4)); g.setFont(host.owner.theme.font(10.f));
            g.drawText(juce::File(rows[index]["file"].toString()).getFileNameWithoutExtension(), 4, 0,
                       juce::jmax(0, getWidth() - 80), 20, juce::Justification::centredLeft, true);
            const auto source = live::playbackWaveform(host.owner, host.waveforms, state, itemId, index);
            const juce::Rectangle<float> area{4.f, 22.f, static_cast<float>(juce::jmax(1, getWidth() - 8)), 12.f};
            if (source.wave && source.length > 0) {
                g.setColour(juce::Colour(0xff17b5da));
                drawPeakEnvelope(g, 1, [&](double a, double b, std::uint32_t) { return source.display(a, b); }, area,
                                 source.start, source.length / area.getWidth());
            } else {
                const auto *cached = host.waveforms.find(rows[index]["file"].toString());
                g.drawText(cached && cached->error.isNotEmpty() ? "File unavailable" : "Preparing waveform...",
                           area.getSmallestIntegerContainer(), juce::Justification::centredLeft, true);
            }
        }
      private:
        void commit() { host.owner.run([&] { host.owner.setLivePlaybackStemMix(itemId, stemId, level.getValue(), mute.getToggleState(), solo.getToggleState()); }); }
        LivePlaybackPanel &host;
        juce::String itemId, stemId;
        juce::Slider level;
        juce::TextButton mute{"M"}, solo{"S"}, details{"..."}, listen{"Play"};
    };
    bool inspecting() const {
        return owner.liveDocument && owner.liveBrowserItem.isNotEmpty() &&
            live::find(owner.liveDocument->interfaceState, "items", owner.liveBrowserItem).isObject();
    }
    void auditionStem(const juce::String &stem) {
        if (!inspecting()) return;
        if (owner.app.recording() || owner.app.engine()->state().playback == mrs::PlaybackState::playing)
            throw std::runtime_error("Stop show playback before auditioning a Browser WAV");
        const auto row = live::playbackStem(owner.liveDocument->interfaceState, owner.liveBrowserItem, stem);
        const auto path = row["file"].toString();
        if (!row.isObject() || path.isEmpty()) return;
        const juce::File file(path);
        if (!file.existsAsFile() || !file.hasFileExtension("wav"))
            throw std::runtime_error("Browser preview WAV is missing or invalid");
        if (previewFile == path && owner.app.library_preview_state().playing) {
            owner.app.stop_library_preview();
            previewFile.clear();
            return;
        }
        if (!owner.app.play_library_preview(std::filesystem::path(file.getFullPathName().toWideCharPointer())))
            throw std::runtime_error("Connect an audio output before Browser audition");
        previewFile = path;
    }
    void requestLayout() { if (layoutChanged) layoutChanged(); else resized(); }
    void commitGroup() { owner.run([&] { owner.setLiveItemPlaybackGain(groupGain.getValue()); }); }
    void commitTrim() {
        owner.run([&] {
            // JSON::parse accepts objects/arrays, not scalar JSON numbers.
            const auto values = juce::JSON::parse("[" + trimStart.getText() + "," + trimEnd.getText() + "]");
            if (!values.isArray() || values.size() != 2 || !live::number(values[0]) || !live::number(values[1]))
                throw std::runtime_error("Enter Trim Start and End in seconds");
            owner.setLivePlaybackTrim(static_cast<double>(values[0]), static_cast<double>(values[1]));
        });
    }
    Desktop &owner;
    juce::TextButton chooseFile, cue{"Cue"}, detailsClose{"x"}, audition{"Listen"};
    juce::Label fileName;
    juce::Slider groupGain;
    juce::Label groupLabel;
    bool detailsVisible{}, previousInspecting{};
    juce::String currentItem, previewFile, browserStem;
    juce::TextEditor trimStart, trimEnd;
    juce::TextButton trimApply{"Trim"};
    juce::Label startLabel{{}, "In"}, endLabel{{}, "Out"};
    juce::String trimSignature, stemSignature, outputSignature;
    juce::ComboBox outputChoice;
    juce::TextButton addStem{"+"}, removeStem{"-"};
    juce::Viewport stemsViewport;
    juce::Component stemsBody;
    LiveWaveformCache waveforms;
    std::vector<std::unique_ptr<StemRow>> stemRows;
    std::vector<std::vector<int>> routes;
    std::unique_ptr<juce::FileChooser> stemChooser;
};
} // namespace ui
