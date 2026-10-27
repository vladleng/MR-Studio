#pragma once
#include "Desktop.h"
#include "LiveContext.h"
#include "LiveMusicalState.h"
#include "LivePlaybackView.h"
#include "LiveShowState.h"
#include "LiveTheme.h"
#include "LiveWaveformCache.h"
#include "WaveformView.h"

namespace ui {
// Native projection of the editor show. Geometry is metadata, never an audio clock.
class LiveTimeline final : public juce::Component {
  public:
    explicit LiveTimeline(Desktop &desktop)
        : owner(desktop), body(*this), viewport(*this), playhead([this] { return playheadBounds(); }) {
        setComponentID("live-show-timeline");
        setName("MR Live item timeline");
        addAndMakeVisible(viewport);
        addAndMakeVisible(stemsFold);
        stemsFold.setComponentID("live-stems-fold");
        stemsFold.setTitle("Expand or collapse Playback stems");
        stemsFold.onClick = [this] { owner.run([&] { owner.editLiveInterface("Fold Playback stems", [&](auto &state) {
            live::put(live::performance(state), "stemsExpanded", !live::playbackExpanded(state));
        }); }); };
        addAndMakeVisible(addTrack);
        addTrack.setComponentID("live-add-track");
        addTrack.onClick = [this] { channelMenu({}); };
        for (auto *control : std::array<juce::Component *, 3>{&playbackMute, &playbackSolo, &playbackGain})
            addAndMakeVisible(control);
        playbackMute.setComponentID("live-channel-mute");
        playbackSolo.setComponentID("live-channel-solo");
        playbackGain.setComponentID("live-channel-gain");
        playbackMute.setClickingTogglesState(true);
        playbackSolo.setClickingTogglesState(true);
        playbackGain.setRange(-60., 12., .1);
        playbackGain.setSliderStyle(juce::Slider::LinearHorizontal);
        playbackGain.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 18);
        playbackGain.setTextValueSuffix(" dB");
        playbackMute.onClick = playbackSolo.onClick = [this] { commitMix(); };
        playbackGain.onDragEnd = [this] { commitMix(); };
        playbackGain.onValueChange = [this] {
            if (!playbackGain.isMouseButtonDown())
                commitMix();
        };
        body.addAndMakeVisible(playhead);
        playhead.setComponentID("live-transport-playhead");
        viewport.setViewedComponent(&body, false);
        viewport.setScrollBarsShown(true, true);
        setTitle("Show timeline; playhead follows Core Transport, track/patch bindings are still in progress");
    }
    ~LiveTimeline() override { viewport.setViewedComponent(nullptr, false); }
    void setChannelWidth(int width) {
        channelWidth = juce::jmax(0, width);
        resized();
    }
    void resized() override {
        addTrack.setBounds(6, 8, juce::jmax(0, channelWidth - 12), 26);
        viewport.setBounds(getLocalBounds().withTrimmedLeft(channelWidth));
        layout();
        syncChannel();
    }
    void refresh() {
        syncChannel();
        std::vector<juce::String> visibleFiles;
        const auto visible = viewport.getViewArea();
        for (const auto &item : items)
            if (item.bounds.intersects(visible))
                for (const auto &row : live::playbackRows(state, item.value["id"].toString()))
                    visibleFiles.push_back(row["file"].toString());
        if (waveforms.sync(visibleFiles)) {
            layout();
            body.repaint();
        }
        int waveReady = 0;
        if (owner.livePlaybackReady())
            for (const auto &clip : owner.project()->clips)
                waveReady += owner.app.clip_waveform(clip) ? 1 : 0;
        if (waveReady != observedWaveform) {
            observedWaveform = waveReady;
            body.repaint(); // Worker completion does not edit the show.
        }
        const auto next = owner.liveDocument ? owner.liveDocument->interfaceState : juce::var();
        auto key = juce::JSON::toString(next, true);
        if (owner.livePlaybackReady()) {
            const auto clip = owner.project()->clips.front();
            key += "|" + juce::String::fromUTF8(clip.id.value.c_str()) + ":" + juce::String(clip.start) + ":" +
                   juce::String(clip.length);
        }
        const auto section = owner.app.musical().state().current_section;
        const auto nextSection = section ? section->id.value : std::string();
        const bool sectionChanged = nextSection != activeSection;
        if (sectionChanged) {
            activeSection = nextSection;
            body.repaint();
        }
        const auto playback = owner.displayState().playback;
        const bool started =
            sectionChanged || (playback != observedPlayback &&
                               (playback == mrs::PlaybackState::playing || playback == mrs::PlaybackState::stopped));
        observedPlayback = playback;
        const bool focused = focusRevision != owner.liveTimelineFocusRevision;
        focusRevision = owner.liveTimelineFocusRevision;
        if (key == signature) {
            if (focused)
                revealSelection();
            if (started)
                revealTransport();
            return;
        }
        const auto selectedItem = next["currentItem"].toString();
        const bool selectionChanged = selectedItem != revealedItem;
        revealedItem = selectedItem;
        signature = key;
        state = next;
        layout();
        syncChannel();
        if (selectionChanged || focused)
            revealSelection();
        if (started)
            revealTransport();
        playhead.update();
        body.repaint();
        repaint();
    }
    int itemCount() const { return static_cast<int>(items.size()); }
    int channelCount() const { return state["channels"].size(); }
    juce::Rectangle<int> itemBounds(int index) const {
        return index >= 0 && index < itemCount() ? items[static_cast<std::size_t>(index)].bounds
                                                 : juce::Rectangle<int>();
    }
    juce::Rectangle<int> playbackStemLaneBounds(const juce::String &channel) const {
        const int y = channelY(channel);
        return y >= lanesTop ? juce::Rectangle<int>(0, y, body.getWidth(), rowHeight) : juce::Rectangle<int>();
    }
    // Body coordinates. Position is observed from Core; no independent UI clock.
    juce::Rectangle<float> playheadBounds() {
        if (!owner.liveTransportReady())
            return {};
        for (const auto &item : items) {
            if (!currentEntry(item) || item.collapsed)
                continue;
            const auto project = owner.project();
            const auto &meter = project->time.meters.front();
            const auto ticksPerBar = 4 * mrs::ppq * meter.numerator / meter.denominator;
            const mrs::Timeline clock(project->time, project->sample_rate);
            const double bar = static_cast<double>(clock.to_ticks(static_cast<mrs::Sample>(owner.visualSample()))) /
                               static_cast<double>(ticksPerBar);
            const double x = static_cast<double>(item.bounds.getX()) + 1. + (item.lead + bar) * item.scale;
            if (!std::isfinite(x) || x >= item.bounds.getRight())
                return {};
            return {static_cast<float>(x), static_cast<float>(headerHeight), 1.f,
                    static_cast<float>(item.bounds.getHeight() - headerHeight)};
        }
        return {};
    }
    void paint(juce::Graphics &g) override {
        const auto tokens = liveReferenceTokens();
        g.fillAll(tokens.colour(ColourRole::panel));
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(0, 0, channelWidth, getHeight());
        g.addTransform(juce::AffineTransform::translation(0.f, -static_cast<float>(viewport.getViewPositionY())));
        text(g, "Sections", {6, headerHeight + rulerHeight, channelWidth - 12, sectionHeight});
        text(g, "Chord Track", {6, headerHeight + rulerHeight + sectionHeight, channelWidth - 12, chordHeight});
        int y = lanesTop;
        const auto active = state["currentItem"].toString();
        for (const auto &lane : lanes) {
            const auto channel = live::find(state, "channels", lane.channel);
            const auto index = lane.child ? playbackIndexForChannel(active, lane.channel) : -1;
            const auto rows = live::playbackRows(state, active);
            const auto label = index >= 0 ? juce::File(rows[static_cast<std::size_t>(index)]["file"].toString()).getFileNameWithoutExtension()
                                         : channel["name"].toString();
            const int indent = lane.container ? 30 : lane.child ? 22 : 8;
            text(g, label, {indent, y + 8, channelWidth - indent - 8, 24});
            text(g, lane.child ? "Stem" : channel["type"].toString(), {indent, y + 34, channelWidth - indent - 8, 20}, true);
            if (lane.child) {
                g.setColour(tokens.colour(ColourRole::border));
                g.drawVerticalLine(12, static_cast<float>(y), static_cast<float>(y + rowHeight));
                g.drawHorizontalLine(y + 20, 12.f, 19.f);
            }
            line(g, {0, y, channelWidth, rowHeight});
            y += rowHeight;
        }
    }

    void mouseDown(const juce::MouseEvent &event) override {
        if (!event.mods.isPopupMenu() || event.x >= channelWidth) return;
        const int y = event.y + viewport.getViewPositionY();
        if (const auto *lane = laneAt(y)) channelMenu(lane->channel);
    }

  private:
    void channelMenu(const juce::String &id) {
        juce::PopupMenu menu;
        menu.setLookAndFeel(&getLookAndFeel());
        const juce::StringArray types{"Instrument", "Player", "Playback", "Audio"};
        if (id.isEmpty()) for (int i = 0; i < types.size(); ++i) menu.addItem(i + 1, types[i]);
        else {
            menu.addItem(10, "Rename channel...");
            // Removing a source-bearing channel requires explicit media management; keep its audio intact.
            bool used = false;
            if (owner.liveDocument) for (const auto &item : *state["items"].getArray())
                for (const auto &row : live::playbackRows(state, item["id"].toString()))
                    used = used || live::playbackChannelId(state, row) == id;
            menu.addItem(11, "Remove empty channel", !used);
        }
        juce::Component::SafePointer<Desktop> safe(&owner);
        menu.showMenuAsync(liveMenuOptions(*this), [safe, id, types](int result) {
            if (!safe || !result) return;
            if (result == 10) {
                const auto channel = live::find(safe->liveDocument->interfaceState, "channels", id);
                safe->textDialog("Channel name", channel["name"].toString(), [safe, id](auto name) {
                    if (safe && name.trim().isNotEmpty()) safe->run([&] { safe->editLiveInterface("Rename channel", [&](auto &state) {
                        live::put(live::require(state, "channels", id), "name", name.trim());
                    }); });
                }); return;
            }
            safe->run([&] { safe->editLiveInterface(result == 11 ? "Remove channel" : "Add channel", [&](auto &state) {
                if (result >= 1 && result <= types.size()) { live::addChannel(state, types[result - 1], types[result - 1]); return; }
                if (result != 11) return;
                auto *channels = live::property(state, "channels").getArray();
                for (int i = channels->size(); --i >= 0;) if ((*channels)[i]["id"].toString() == id) channels->remove(i);
                for (auto &item : *live::property(state, "items").getArray()) if (auto *tracks = live::property(item, "tracks").getArray())
                    for (int i = tracks->size(); --i >= 0;) if ((*tracks)[i]["channelId"].toString() == id) tracks->remove(i);
            }); });
        });
    }
    struct Item {
        juce::var value;
        juce::Rectangle<int> bounds;
        double scale{}, lead{}, bars{};
        bool preview{};
        juce::String entry;
        bool collapsed{};
    };
    bool currentEntry(const Item &item) const {
        if (item.value["id"] != state["currentItem"])
            return false;
        return item.preview || owner.liveCurrentEntry.isEmpty() || item.entry == owner.liveCurrentEntry;
    }
    class StemMix final : public juce::Component {
      public:
        StemMix(LiveTimeline &timeline, juce::String stem) : key(std::move(stem)), parent(timeline) {
            for (auto *control : std::array<juce::Component *, 4>{&mute, &solo, &gain, &arm})
                addAndMakeVisible(control);
            arm.setEnabled(false);
            arm.setTooltip("Recording is not available for this channel yet");
            mute.setClickingTogglesState(true);
            solo.setClickingTogglesState(true);
            gain.setRange(-60., 12., .1);
            gain.setSliderStyle(juce::Slider::LinearHorizontal);
            gain.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 18);
            gain.setTextValueSuffix(" dB");
            mute.onClick = solo.onClick = gain.onDragEnd = [this] { commit(); };
            gain.onValueChange = [this] {
                if (!gain.isMouseButtonDown())
                    commit();
            };
        }
        void resized() override {
            mute.setBounds(8, 0, 26, 22);
            solo.setBounds(36, 0, 26, 22);
            arm.setBounds(64, 0, 26, 22);
            gain.setBounds(6, 26, juce::jmax(0, getWidth() - 12), 24);
        }
        void sync(const juce::var &row) {
            arm.setVisible(row["type"].toString() != "Playback");
            mute.setToggleState(static_cast<bool>(row["mute"]), juce::dontSendNotification);
            solo.setToggleState(static_cast<bool>(row["solo"]), juce::dontSendNotification);
            if (!gain.isMouseButtonDown())
                gain.setValue(static_cast<double>(row["gain"]), juce::dontSendNotification);
            setEnabled(parent.owner.liveDocument.has_value());
        }
        juce::String key;

      private:
        void commit() {
            parent.owner.run([&] {
                const auto channel = live::find(parent.state, "channels", key);
                parent.owner.setLiveChannelMix(key, gain.getValue(), live::channelPan(channel), mute.getToggleState(), solo.getToggleState());
            });
        }
        LiveTimeline &parent;
        juce::TextButton mute{"M"}, solo{"S"}, arm{"R"};
        juce::Slider gain;
    };
    class Viewport final : public juce::Viewport {
      public:
        explicit Viewport(LiveTimeline &host) : parent(host) {}
        void visibleAreaChanged(const juce::Rectangle<int> &) override {
            parent.syncChannel();
            parent.repaint();
        }

      private:
        LiveTimeline &parent;
    };
    class Body final : public juce::Component {
      public:
        explicit Body(LiveTimeline &host) : parent(host) { setComponentID("live-timeline-items"); }
        void paint(juce::Graphics &g) override { parent.paintItems(g); }
        void mouseDown(const juce::MouseEvent &event) override {
            if (!event.mods.isLeftButtonDown())
                return;
            for (const auto &item : parent.items) {
                if (event.y < headerHeight && event.x >= item.bounds.getRight() - 24 && item.bounds.contains(event.getPosition()) && item.entry.isNotEmpty()) {
                    parent.owner.run([&] { parent.owner.editLiveInterface("Fold item", [&](auto &state) {
                        live::put(live::map(live::performance(state), "collapsedItems"), item.entry, !item.collapsed);
                    }); });
                    return;
                }
            }
            if (parent.owner.livePlaybackReady() && event.y >= lanesTop + 10) {
                for (const auto &item : parent.items) {
                    if (item.collapsed || item.value["id"] != parent.state["currentItem"] || !item.bounds.contains(event.getPosition()))
                        continue;
                    const auto *lane = parent.laneAt(event.y);
                    if (!lane) continue;
                    if (lane->container) {
                        parent.owner.run([&] { parent.owner.app.seek(parent.sampleAt(item, event.position.x)); });
                        return;
                    }
                    const auto channelId = lane->channel;
                    const int index = parent.playbackIndexForChannel(item.value["id"].toString(), channelId);
                    if (index < 0)
                        continue;
                    const auto rows = live::playbackRows(parent.state, item.value["id"].toString());
                    parent.owner.livePlaybackStem =
                        index == 0 ? juce::String() : rows[static_cast<std::size_t>(index)]["id"].toString();
                    const auto clip = parent.owner.project()->clips[static_cast<std::size_t>(index)];
                    const auto bounds = parent.audioBounds(item, clip, parent.channelY(channelId));
                    const bool canTrim = parent.owner.app.engine()->state().playback != mrs::PlaybackState::playing &&
                                         !parent.owner.app.recording();
                    if (canTrim && bounds.getWidth() >= 12 &&
                        (std::abs(event.position.x - bounds.getX()) <= 5 ||
                         std::abs(event.position.x - bounds.getRight()) <= 5)) {
                        dragItem = item.value["id"].toString();
                        dragClip = clip;
                        dragLeft = std::abs(event.position.x - bounds.getX()) <= 5;
                        dragPosition = event.position.x;
                        dragStart = event.position.x;
                        parent.owner.refresh();
                        return;
                    }
                    parent.owner.run([&] { parent.owner.app.seek(parent.sampleAt(item, event.position.x)); });
                    return;
                }
            }
            if (event.y >= headerHeight && event.y < headerHeight + rulerHeight + sectionHeight) {
                for (const auto &item : parent.items) {
                    if (!item.bounds.contains(event.getPosition()) || item.value["id"] != parent.state["currentItem"] ||
                        !parent.owner.livePlaybackReady())
                        continue;
                    const auto project = parent.owner.project();
                    const auto &meter = project->time.meters.front();
                    const double bar = (event.x - item.bounds.getX() - 1.) / item.scale - item.lead;
                    if (bar < 0 && event.y >= headerHeight + rulerHeight)
                        return;
                    const auto tick = static_cast<mrs::Tick>(std::llround(
                        juce::jmax(0., bar) * static_cast<double>(4 * mrs::ppq * meter.numerator / meter.denominator)));
                    if (event.y < headerHeight + rulerHeight) {
                        parent.owner.run([&] {
                            parent.owner.app.seek(mrs::Timeline(project->time, project->sample_rate).to_samples(tick));
                        });
                    } else {
                        // Resolve clicks against the same single-valued lane used by Core.
                        for (const auto &section : project->sections)
                            if (section.start <= tick && tick < section.end) {
                                parent.owner.run([&] { parent.owner.app.musical().seek_section(section.id); });
                                break;
                            }
                    }
                    return;
                }
                return;
            }
            for (const auto &item : parent.items)
                if (item.bounds.contains(event.getPosition()) && event.y < headerHeight) {
                    const auto id = item.value["id"].toString(), entry = item.entry;
                    juce::Component::SafePointer<Desktop> safe(&parent.owner);
                    parent.owner.confirmDiscard(
                        [safe, id, entry] {
                            if (safe)
                                safe->run([&] { safe->selectLiveItem(id, entry); });
                        },
                        true);
                    break;
                }
        }

        void mouseDrag(const juce::MouseEvent &event) override {
            if (dragItem.isEmpty())
                return;
            dragPosition = event.position.x;
            repaint();
        }
        void mouseUp(const juce::MouseEvent &) override {
            const auto id = dragItem;
            dragItem.clear();
            repaint();
            if (id.isEmpty() || std::abs(dragPosition - dragStart) < 1.f || !parent.owner.livePlaybackReady() ||
                parent.state["currentItem"].toString() != id ||
                parent.owner.project()->clips[parent.owner.livePlaybackIndex()] != dragClip)
                return;
            for (const auto &item : parent.items)
                if (item.value["id"].toString() == id) {
                    const auto project = parent.owner.project();
                    const auto frames = parent.owner.app.source_frames(dragClip.id);
                    const auto position = parent.sampleAt(item, dragPosition);
                    const auto start =
                        dragLeft ? juce::jlimit<mrs::Sample>(0, dragClip.start + dragClip.length - 1, position)
                                 : dragClip.start;
                    const auto end = dragLeft ? dragClip.start + dragClip.length
                                              : juce::jlimit<mrs::Sample>(start + 1, frames, position);
                    parent.owner.run([&] {
                        parent.owner.setLivePlaybackTrim(static_cast<double>(start) / project->sample_rate,
                                                         static_cast<double>(end) / project->sample_rate);
                    });
                    return;
                }
        }
        bool trimming() const { return dragItem.isNotEmpty(); }
        float trimX() const { return dragPosition; }

      private:
        LiveTimeline &parent;
        juce::String dragItem;
        mrs::Clip dragClip;
        bool dragLeft{};
        float dragPosition{}, dragStart{};
    };
    static constexpr int headerHeight = 60, rulerHeight = 24, sectionHeight = 34, chordHeight = 40;
    static constexpr int lanesTop = headerHeight + rulerHeight + sectionHeight + chordHeight;
    static constexpr int rowHeight = 124;
    static double numeric(const juce::var &value, double fallback, double minimum, double maximum) {
        return live::number(value) ? juce::jlimit(minimum, maximum, static_cast<double>(value)) : fallback;
    }
    void text(juce::Graphics &g, const juce::String &value, juce::Rectangle<int> area, bool muted = false) const {
        g.setColour(liveReferenceTokens().colour(muted ? ColourRole::muted : ColourRole::text));
        g.setFont(owner.theme.font(muted ? 10.f : 12.f));
        g.drawText(value, area, juce::Justification::centredLeft, true);
    }
    static void line(juce::Graphics &g, juce::Rectangle<int> area) {
        g.setColour(liveReferenceTokens().colour(ColourRole::border));
        g.drawRect(area, 1);
    }
    void revealSelection() {
        for (const auto &item : items)
            if (currentEntry(item)) {
                viewport.setViewPosition(juce::jmax(0, item.bounds.getX() - 12), viewport.getViewPositionY());
                if (owner.livePlaybackReady())
                    revealTransport();
                break;
            }
    }
    void revealTransport() {
        const auto lineBounds = playheadBounds();
        if (lineBounds.isEmpty())
            return;
        const int x = juce::roundToInt(lineBounds.getX());
        if (x < viewport.getViewPositionX() || x >= viewport.getViewPositionX() + viewport.getMaximumVisibleWidth())
            viewport.setViewPosition(juce::jmax(0, x - 24), viewport.getViewPositionY());
    }
    void layout() {
        items.clear();
        int x = 0;
        lanes = live::playbackLanes(state);
        const int height = lanesTop + static_cast<int>(lanes.size()) * rowHeight;
        auto entries = live::activeSetlist(state)["entries"].clone();
        const auto selected = state["currentItem"].toString();
        bool found = false;
        if (const auto *values = entries.getArray())
            for (const auto &entry : *values)
                found = found || entry["itemId"].toString() == selected;
        if (!found && live::find(state, "items", selected).isObject()) {
            auto preview = live::object();
            live::put(preview, "itemId", selected);
            live::put(preview, "browserPreview", true);
            if (!entries.isArray())
                entries = juce::Array<juce::var>();
            entries.getArray()->add(preview);
        }
        if (const auto *values = entries.getArray())
            for (const auto &entry : *values) {
                const auto item = live::find(state, "items", entry["itemId"].toString());
                if (!item.isObject())
                    continue;
                const auto view = state["performance"]["arrangement"][juce::Identifier(item["id"].toString())];
                const double scale = numeric(view["scale"], 16, 4, 128);
                const double lead = numeric(view["lead"], 0, 0, 4096);
                double bars = numeric(view["length"], numeric(item["bars"], 1, 1, 4096), 1, 4096);
                if (item["id"] == state["currentItem"] && owner.livePlaybackReady()) {
                    const auto project = owner.project();
                    mrs::Sample end = 0;
                    for (const auto &clip : project->clips)
                        end = juce::jmax(end, clip.start + clip.length);
                    const auto &meter = project->time.meters.front();
                    const mrs::Timeline clock(project->time, project->sample_rate);
                    bars = juce::jmax(bars, std::ceil(static_cast<double>(clock.to_ticks(end)) /
                                                      (4. * mrs::ppq * meter.numerator / meter.denominator)));
                    bars = juce::jmin(bars, 4096.); // Keep the existing bounded scroll-surface contract.
                }
                for (const auto &row : live::playbackRows(state, item["id"].toString())) {
                    const auto *cached = waveforms.find(row["file"].toString());
                    if (!cached || !cached->wave)
                        continue;
                    const auto projection = live::musicalProjection(live::musicalVersion(state, item["id"].toString()));
                    const double seconds = row["trimEnd"].isVoid()
                                               ? static_cast<double>(cached->wave->frames()) / cached->rate
                                               : static_cast<double>(row["trimEnd"]);
                    const mrs::Timeline clock(projection.time, cached->rate);
                    const auto &meter = projection.time.meters.front();
                    bars = juce::jmin(
                        4096., juce::jmax(bars, std::ceil(static_cast<double>(clock.to_ticks(static_cast<mrs::Sample>(
                                                              std::llround(seconds * cached->rate)))) /
                                                          (4. * mrs::ppq * meter.numerator / meter.denominator))));
                }
                // The model permits 1000 entries, 4096 bars/lead and 128 px/bar.
                // Keep their complete geometry within a signed-int scroll surface.
                const bool collapsed = static_cast<bool>(state["performance"]["collapsedItems"][juce::Identifier(entry["id"].toString())]);
                const int width = collapsed ? 160 : juce::jlimit(6, 1048578, juce::roundToInt((bars + lead) * scale) + 2);
                items.push_back({item,
                                 {x, 0, width, height},
                                 scale,
                                 lead,
                                 bars,
                                 static_cast<bool>(entry["browserPreview"]),
                                 entry["id"].toString(), collapsed});
                x += width;
            }
        body.setSize(juce::jmax(x, viewport.getMaximumVisibleWidth()),
                     juce::jmax(height, viewport.getMaximumVisibleHeight()));
    }
    juce::Rectangle<int> extent(const Item &item, const juce::var &span, int y, int height, double offset = 0) const {
        const double start = numeric(span["start"], 0, 0, 4096) + offset;
        const double length = numeric(span["length"], .25, 0, 4096);
        return {item.bounds.getX() + 1 + juce::roundToInt((item.lead + start) * item.scale), y,
                juce::jmax(1, juce::roundToInt(length * item.scale)), height};
    }
    void block(juce::Graphics &g, juce::Rectangle<int> area, const juce::String &label, const juce::var &color) const {
        g.setColour(liveBlockColour(color.toString()).withAlpha(.18f));
        g.fillRect(area);
        g.setColour(liveBlockColour(color.toString()));
        g.drawRect(area);
        text(g, label, area.reduced(6, 0));
    }
    void paintItems(juce::Graphics &g) const {
        const auto tokens = liveReferenceTokens();
        g.fillAll(tokens.colour(ColourRole::canvas));
        if (items.empty())
            text(g, "Setlist is empty. Add items from Browser.", {12, 12, 480, 30}, true);
        for (const auto &item : items) {
            if (!g.getClipBounds().intersects(item.bounds))
                continue;
            juce::Graphics::ScopedSaveState save(g);
            g.reduceClipRegion(item.bounds);
            const auto id = item.value["id"].toString();
            const int x = item.bounds.getX(), width = item.bounds.getWidth();
            g.setColour(tokens.colour(ColourRole::panel));
            g.fillRect(item.bounds);
            const bool selected = id == state["currentItem"].toString();
            const auto itemColour =
                liveBlockColour(state["performance"]["itemColors"][juce::Identifier(id)].toString());
            g.setColour(itemColour.withAlpha(.23f));
            g.fillRect(x, 0, width, headerHeight);
            text(g,
                 item.value["name"].toString() + (item.preview ? juce::String::fromUTF8(" · Browser") : juce::String()),
                 {x + 10, 4, juce::jmax(0, width - 20), 26});
            text(g,
                 item.value["tempo"].toString() + " BPM · " + item.value["meter"].toString() + " · " +
                     juce::String(item.bars) + " bars",
                 {x + 10, 30, juce::jmax(0, width - 20), 22}, true);
            text(g, item.collapsed ? ">" : "v", {x + width - 24, 4, 20, 26});
            if (item.collapsed) { line(g, item.bounds); continue; }
            const int step = juce::jmax(1, static_cast<int>(std::ceil(item.bars / 16)));
            for (int bar = 0; bar < static_cast<int>(std::ceil(item.bars)); ++bar) {
                const int bx = x + 1 + juce::roundToInt((item.lead + bar) * item.scale);
                g.setColour(tokens.colour(ColourRole::border).withAlpha(.3f));
                g.drawVerticalLine(bx, static_cast<float>(headerHeight + rulerHeight),
                                   static_cast<float>(item.bounds.getHeight()));
                if (bar % step == 0)
                    text(g, juce::String(bar + 1), {bx + 3, headerHeight, 38, rulerHeight}, true);
            }
            if (const auto *sections = item.value["sections"].getArray())
                for (const auto &section : *sections) {
                    const auto area = extent(item, section, headerHeight + rulerHeight + 1, sectionHeight - 2);
                    block(g, area, section["name"].toString(), section["color"]);
                    if (selected && section["id"].toString().toStdString() == activeSection) {
                        g.setColour(tokens.colour(ColourRole::focus));
                        g.drawRect(area, 2);
                    }
                }
            const auto chords = state["performance"]["chords"][juce::Identifier(id)];
            if (const auto *values = chords.getArray())
                for (const auto &chord : *values)
                    block(g, extent(item, chord, lanesTop - chordHeight + 2, chordHeight - 4),
                          chord["symbol"].toString(), chord["color"]);
            int y = lanesTop;
            for (const auto &lane : lanes) {
                const auto channel = live::find(state, "channels", lane.channel);
                if (lane.container) paintPlaybackContainer(g, item, y);
                else {
                    const int playbackIndex = playbackIndexForChannel(id, lane.channel);
                    if (playbackIndex >= 0) paintAudio(g, item, y, static_cast<std::size_t>(playbackIndex));
                    else if (const auto *tracks = item.value["tracks"].getArray())
                        for (const auto &track : *tracks) if (track["channelId"] == channel["id"]) paintTrack(g, item, channel, track, y);
                }
                line(g, {x, y, width, rowHeight});
                y += rowHeight;
            }
            g.setColour(selected ? tokens.colour(ColourRole::focus) : itemColour);
            g.drawRect(item.bounds);
        }
    }
    int playbackIndexForChannel(const juce::String &item, const juce::String &channelId) const {
        const auto rows = live::playbackRows(state, item);
        if (rows.empty())
            return -1;
        for (std::size_t i = 1; i < rows.size(); ++i)
            if (rows[i]["id"].toString() == channelId)
                return static_cast<int>(i);
        for (const auto &channel : *state["channels"].getArray())
            if (channel["type"].toString() == "Playback")
                return channel["id"].toString() == channelId ? 0 : -1;
        return -1;
    }
    int playbackLaneY() const {
        if (const auto *channels = state["channels"].getArray())
            for (int i = 0; i < channels->size(); ++i)
                if ((*channels)[i]["type"].toString() == "Playback")
                    return channelY((*channels)[i]["id"].toString());
        return -rowHeight;
    }
    mrs::Sample sampleAt(const Item &item, double x) const {
        const auto project = owner.project();
        const auto &meter = project->time.meters.front();
        const double bar = juce::jmax(0., (x - item.bounds.getX() - 1.) / item.scale - item.lead);
        const auto tick = static_cast<mrs::Tick>(
            std::llround(bar * static_cast<double>(4 * mrs::ppq * meter.numerator / meter.denominator)));
        return mrs::Timeline(project->time, project->sample_rate).to_samples(tick);
    }
    juce::Rectangle<float> audioBounds(const Item &item, const mrs::Clip &clip, int y) const {
        const auto project = owner.project();
        const auto &meter = project->time.meters.front();
        const double ticksPerBar = 4. * mrs::ppq * meter.numerator / meter.denominator;
        const mrs::Timeline clock(project->time, project->sample_rate);
        const auto x = [&](mrs::Sample sample) {
            return static_cast<float>(item.bounds.getX() + 1. +
                                      (item.lead + static_cast<double>(clock.to_ticks(sample)) / ticksPerBar) *
                                          item.scale);
        };
        const float start = x(clip.start), end = x(clip.start + clip.length);
        return {start, static_cast<float>(y + 10), juce::jmax(1.f, end - start), static_cast<float>(rowHeight - 20)};
    }
    void paintPlaybackContainer(juce::Graphics &g, const Item &item, int y) const {
        const auto id = item.value["id"].toString();
        const auto rows = live::playbackRows(state, id);
        if (rows.empty()) {
            if (const auto *tracks = item.value["tracks"].getArray()) for (const auto &track : *tracks)
                if (track["channelId"].toString() == live::playbackContainer(state)) {
                    paintTrack(g, item, primaryChannel(), track, y); break;
                }
            return;
        }
        std::vector<live::PlaybackWaveformSource> sources;
        double end = 0.; int ready = 0, unavailable = 0;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto source = live::playbackWaveform(owner, waveforms, state, id, i);
            sources.push_back(source);
            end = std::max(end, source.start + source.length);
            ready += source.wave ? 1 : 0;
            const auto *cached = waveforms.find(rows[i]["file"].toString());
            unavailable += !source.wave && cached && cached->error.isNotEmpty() ? 1 : 0;
        }
        const auto projection = live::musicalProjection(live::musicalVersion(state, id));
        const mrs::Timeline clock(projection.time, 48000);
        const auto &meter = projection.time.meters.front();
        const double ticksPerBar = 4. * mrs::ppq * meter.numerator / meter.denominator;
        const float x = static_cast<float>(item.bounds.getX() + 1. + item.lead * item.scale);
        const float right = static_cast<float>(x + static_cast<double>(clock.to_ticks(static_cast<mrs::Sample>(std::llround(end * 48000)))) / ticksPerBar * item.scale);
        const juce::Rectangle<float> area{x, static_cast<float>(y + 8), std::max(1.f, right - x), static_cast<float>(rowHeight - 16)};
        if (ready) block(g, area.getSmallestIntegerContainer(), {}, "blue");
        text(g, "Playback · " + juce::String(static_cast<int>(rows.size())) + " stems" +
                     (unavailable ? " · file unavailable" : ready < static_cast<int>(rows.size()) ? " · preparing waveforms" : ""),
             {item.bounds.getX() + 8, y + 10, item.bounds.getWidth() - 16, 24});
        if (ready && end > 0.) {
            const auto waveArea = area.withTrimmedTop(28).reduced(1.f, 4.f);
            g.setColour(juce::Colour(0xff17b5da));
            drawPeakEnvelope(g, 1, [&](double begin, double finish, std::uint32_t) {
                return live::averagePlaybackEnvelope(sources, begin, finish);
            }, waveArea, 0., end / std::max(1.f, waveArea.getWidth()));
        }
    }
    void paintAudio(juce::Graphics &g, const Item &item, int y, std::size_t index) const {
        const auto row = live::playbackRows(state, item.value["id"].toString())[index];
        if (item.value["id"] != state["currentItem"] || !owner.livePlaybackReady()) {
            text(g, juce::File(row["file"].toString()).getFileName(),
                 {item.bounds.getX() + 8, y + 10, item.bounds.getWidth() - 16, 28});
            const auto *cached = waveforms.find(row["file"].toString());
            if (cached && cached->wave) {
                const auto projection =
                    live::musicalProjection(live::musicalVersion(state, item.value["id"].toString()));
                const mrs::Timeline clock(projection.time, cached->rate);
                const double ticksPerBar = 4. * mrs::ppq * projection.time.meters.front().numerator /
                                           projection.time.meters.front().denominator;
                const double startSeconds = static_cast<double>(row["trimStart"]);
                const double endSeconds = row["trimEnd"].isVoid()
                                              ? static_cast<double>(cached->wave->frames()) / cached->rate
                                              : static_cast<double>(row["trimEnd"]);
                const auto x = [&](double seconds) {
                    return static_cast<float>(item.bounds.getX() + 1. +
                                              (item.lead + static_cast<double>(clock.to_ticks(static_cast<mrs::Sample>(
                                                               std::llround(seconds * cached->rate)))) /
                                                               ticksPerBar) *
                                                  item.scale);
                };
                const juce::Rectangle<float> area{x(startSeconds), static_cast<float>(y + 40),
                                                  juce::jmax(1.f, x(endSeconds) - x(startSeconds)),
                                                  static_cast<float>(rowHeight - 54)};
                g.setColour(juce::Colour(0xff17b5da));
                drawWaveform(g, *cached->wave, area, startSeconds * cached->rate,
                             (endSeconds - startSeconds) * cached->rate / juce::jmax(1.f, area.getWidth()));
            } else {
                text(g, cached ? cached->error : "Preparing waveform...",
                     {item.bounds.getX() + 8, y + 42, item.bounds.getWidth() - 16, 28}, true);
            }
            return;
        }
        const auto project = owner.project();
        const auto clip = project->clips[index];
        const auto area = audioBounds(item, clip, y);
        block(g, area.getSmallestIntegerContainer(), {}, "blue");
        text(g,
             juce::File(row["file"].toString()).getFileName() + "  " +
                 juce::String(static_cast<double>(clip.length) / project->sample_rate, 2) + " s",
             area.getSmallestIntegerContainer().reduced(6).withHeight(24));
        const auto waveformArea = area.withTrimmedTop(30).reduced(1.f, 4.f);
        if (const auto *wave = owner.app.clip_waveform(clip)) {
            g.setColour(juce::Colour(0xff17b5da));
            drawWaveform(g, *wave, waveformArea, static_cast<double>(clip.source_offset),
                         static_cast<double>(clip.length) / juce::jmax(1.f, waveformArea.getWidth()));
        } else if (const auto *cached = waveforms.find(row["file"].toString()); cached && cached->wave) {
            const double ratio = static_cast<double>(cached->rate) / project->sample_rate;
            g.setColour(juce::Colour(0xff17b5da));
            drawWaveform(g, *cached->wave, waveformArea, static_cast<double>(clip.source_offset) * ratio,
                         static_cast<double>(clip.length) * ratio / juce::jmax(1.f, waveformArea.getWidth()));
        }
        if (body.trimming()) {
            g.setColour(liveReferenceTokens().colour(ColourRole::focus));
            g.drawVerticalLine(juce::roundToInt(body.trimX()), area.getY(), area.getBottom());
        }
    }
    void commitMix() {
        if (!owner.liveDocument) return;
        const auto channel = primaryChannel();
        if (channel.isObject()) owner.run([&] {
            owner.setLiveChannelMix(channel["id"].toString(), playbackGain.getValue(), live::channelPan(channel),
                                    playbackMute.getToggleState(), playbackSolo.getToggleState());
        });
    }
    juce::var primaryChannel() const {
        if (const auto *channels = state["channels"].getArray())
            for (const auto &channel : *channels) if (channel["type"].toString() == "Playback") return channel;
        return {};
    }
    const live::PlaybackLane *laneAt(int y) const {
        if (y < lanesTop) return nullptr;
        const auto i = static_cast<std::size_t>((y - lanesTop) / rowHeight);
        return i < lanes.size() ? &lanes[i] : nullptr;
    }
    int channelY(const juce::String &id) const {
        for (std::size_t i = 0; i < lanes.size(); ++i)
            if (lanes[i].channel == id && !lanes[i].container) return lanesTop + static_cast<int>(i) * rowHeight;
        return -rowHeight;
    }
    int containerY() const {
        for (std::size_t i = 0; i < lanes.size(); ++i)
            if (lanes[i].container) return lanesTop + static_cast<int>(i) * rowHeight;
        return -rowHeight;
    }
    void syncChannel() {
        const int foldY = containerY() - viewport.getViewPositionY();
        stemsFold.setBounds(5, foldY + 8, 22, 24);
        stemsFold.setVisible(containerY() >= lanesTop && foldY + rowHeight > 0 && foldY < getHeight());
        stemsFold.setButtonText(live::playbackExpanded(state) ? "v" : ">");
        const auto current =
            owner.liveDocument ? owner.liveDocument->interfaceState["currentItem"].toString() : juce::String();
        const auto bindings = owner.liveDocument ? live::playbackRows(owner.liveDocument->interfaceState, current)
                                                 : std::vector<juce::var>();
        juce::String channelsKey = primaryChannel()["id"].toString() + "|";
        const auto channels = owner.liveDocument ? owner.liveDocument->interfaceState["channels"] : juce::var();
        if (const auto *entries = channels.getArray()) for (const auto &channel : *entries) channelsKey += channel["id"].toString();
        if (channelsKey != stemControlsKey) {
            stemControlsKey = channelsKey;
            stemControls.clear();
            if (const auto *entries = channels.getArray()) for (const auto &channel : *entries) {
                if (channel["id"] == primaryChannel()["id"]) continue;
                auto control = std::make_unique<StemMix>(*this, channel["id"].toString());
                addAndMakeVisible(*control); stemControls.push_back(std::move(control));
            }
        }
        for (auto &control : stemControls) {
            const int y = channelY(control->key);
            const int top = y + 58 - viewport.getViewPositionY();
            const auto lane = laneAt(y);
            const int indent = lane && lane->child ? 14 : 0;
            control->setBounds(indent, top, juce::jmax(0, channelWidth - indent), 50);
            control->setVisible(y >= lanesTop && top + 50 > 0 && top < getHeight());
            control->sync(live::find(owner.liveDocument->interfaceState, "channels", control->key));
        }
        const auto row = primaryChannel();
        const auto y = playbackLaneY() - viewport.getViewPositionY();
        const bool visible = y >= lanesTop - viewport.getViewPositionY() && y + rowHeight > 0 && y < getHeight();
        for (auto *control : std::array<juce::Component *, 3>{&playbackMute, &playbackSolo, &playbackGain}) {
            control->setVisible(visible && playbackLaneY() >= lanesTop);
            control->setEnabled(primaryChannel().isObject());
        }
        playbackMute.setBounds(22, y + 58, 26, 22);
        playbackSolo.setBounds(50, y + 58, 26, 22);
        playbackGain.setBounds(20, y + 84, juce::jmax(0, channelWidth - 26), 24);
        playbackMute.setToggleState(static_cast<bool>(row["mute"]), juce::dontSendNotification);
        playbackSolo.setToggleState(static_cast<bool>(row["solo"]), juce::dontSendNotification);
        if (!playbackGain.isMouseButtonDown())
            playbackGain.setValue(row.isObject() ? static_cast<double>(row["gain"]) : 0., juce::dontSendNotification);
    }
    void paintTrack(juce::Graphics &g, const Item &item, const juce::var &channel, const juce::var &track,
                    int y) const {
        if (channel["type"].toString() == "Playback") {
            const auto clips = state["performance"]["clips"][juce::Identifier(track["id"].toString())];
            if (const auto *values = clips.getArray())
                for (const auto &clip : *values) {
                    const auto bounds = extent(item, clip, y + 10, rowHeight - 20);
                    block(g, bounds, {}, "blue");
                    text(g, track["media"]["name"].toString(), bounds.reduced(6).withHeight(28));
                    text(g, "Audio file not connected", bounds.reduced(6).withTrimmedTop(44).withHeight(22), true);
                }
            return;
        }
        if (const auto *order = track["patchOrder"].getArray())
            for (const auto &patchId : *order) {
                const auto patch = live::find(channel, "patches", patchId.toString());
                const auto span = track["spans"][juce::Identifier(patchId.toString())];
                if (!patch.isObject() || !span.isObject())
                    continue;
                const auto patchBounds = extent(item, span, y + 10, rowHeight - 20);
                block(g, patchBounds, {}, patch["color"]);
                text(g, patch["name"].toString(), patchBounds.reduced(6).withHeight(30));
                if (const auto *subs = patch["subPatches"].getArray())
                    for (const auto &sub : *subs) {
                        const auto geometry = span["subs"][juce::Identifier(sub["id"].toString())];
                        if (geometry.isObject())
                            block(g, extent(item, geometry, y + 64, 26, numeric(span["start"], 0, 0, 4096)),
                                  sub["name"].toString(),
                                  sub["color"].toString() == "inherit" ? patch["color"] : sub["color"]);
                    }
            }
    }
    Desktop &owner;
    LiveWaveformCache waveforms;
    Body body;
    Viewport viewport;
    PlayheadLine playhead;
    juce::TextButton addTrack{"+ Track"}, stemsFold;
    std::vector<live::PlaybackLane> lanes;
    juce::TextButton playbackMute{"M"}, playbackSolo{"S"};
    juce::Slider playbackGain;
    std::vector<std::unique_ptr<StemMix>> stemControls;
    juce::String stemControlsKey;
    juce::var state;
    juce::String signature, revealedItem;
    std::vector<Item> items;
    std::string activeSection;
    std::uint64_t focusRevision{};
    int channelWidth{150};
    int observedWaveform{};
    mrs::PlaybackState observedPlayback{mrs::PlaybackState::stopped};
};
} // namespace ui
