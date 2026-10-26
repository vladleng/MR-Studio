#pragma once
#include "Desktop.h"
#include "LiveShowState.h"
#include "LiveTheme.h"
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
        viewport.setBounds(getLocalBounds().withTrimmedLeft(channelWidth));
        layout();
        syncChannel();
    }
    void refresh() {
        syncChannel();
        const bool waveReady = owner.livePlaybackReady() && owner.app.clip_waveform(owner.project()->clips.front());
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
    // Body coordinates. Position is observed from Core; no independent UI clock.
    juce::Rectangle<float> playheadBounds() {
        if (!owner.livePlaybackReady())
            return {};
        for (const auto &item : items) {
            if (item.value["id"] != state["currentItem"])
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
        if (const auto *channels = state["channels"].getArray()) {
            int y = lanesTop;
            for (const auto &channel : *channels) {
                text(g, channel["name"].toString(), {8, y + 8, channelWidth - 16, 24});
                text(g, channel["type"].toString(), {8, y + 34, channelWidth - 16, 20}, true);
                // Host bindings arrive separately; these controls must not simulate mute/record.
                if (y != playbackLaneY())
                    text(g, channel["type"].toString() == "Playback" ? "M   S" : "M   S   R",
                         {8, y + 58, channelWidth - 16, 22}, true);
                line(g, {0, y, channelWidth, rowHeight});
                y += rowHeight;
            }
        }
    }

  private:
    struct Item {
        juce::var value;
        juce::Rectangle<int> bounds;
        double scale{}, lead{}, bars{};
        bool preview{};
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
            if (parent.owner.livePlaybackReady() && event.y >= parent.playbackLaneY() + 10 &&
                event.y < parent.playbackLaneY() + rowHeight - 10) {
                for (const auto &item : parent.items) {
                    if (item.value["id"] != parent.state["currentItem"] || !item.bounds.contains(event.getPosition()))
                        continue;
                    const auto clip = parent.owner.project()->clips.front();
                    const auto bounds = parent.audioBounds(item, clip, parent.playbackLaneY());
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
                    const auto id = item.value["id"].toString();
                    juce::Component::SafePointer<Desktop> safe(&parent.owner);
                    parent.owner.confirmDiscard(
                        [safe, id] {
                            if (safe)
                                safe->run([&] { safe->selectLiveItem(id); });
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
                parent.state["currentItem"].toString() != id || parent.owner.project()->clips.front() != dragClip)
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
            if (item.value["id"] == state["currentItem"]) {
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
        const int height = lanesTop + channelCount() * rowHeight;
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
                    const auto clip = project->clips.front();
                    const auto &meter = project->time.meters.front();
                    const mrs::Timeline clock(project->time, project->sample_rate);
                    bars = juce::jmax(bars, std::ceil(static_cast<double>(clock.to_ticks(clip.start + clip.length)) /
                                                      (4. * mrs::ppq * meter.numerator / meter.denominator)));
                    bars = juce::jmin(bars, 4096.); // Keep the existing bounded scroll-surface contract.
                }
                // The model permits 1000 entries, 4096 bars/lead and 128 px/bar.
                // Keep their complete geometry within a signed-int scroll surface.
                const int width = juce::jlimit(6, 1048578, juce::roundToInt((bars + lead) * scale) + 2);
                items.push_back(
                    {item, {x, 0, width, height}, scale, lead, bars, static_cast<bool>(entry["browserPreview"])});
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
            if (const auto *channels = state["channels"].getArray())
                for (const auto &channel : *channels) {
                    if (y == playbackLaneY() && live::playback(state, id).isObject()) {
                        paintAudio(g, item, y);
                    } else if (const auto *tracks = item.value["tracks"].getArray())
                        for (const auto &track : *tracks)
                            if (track["channelId"] == channel["id"])
                                paintTrack(g, item, channel, track, y);
                    line(g, {x, y, width, rowHeight});
                    y += rowHeight;
                }
            g.setColour(selected ? tokens.colour(ColourRole::focus) : itemColour);
            g.drawRect(item.bounds);
        }
    }
    int playbackLaneY() const {
        if (const auto *channels = state["channels"].getArray())
            for (int i = 0; i < channels->size(); ++i)
                if ((*channels)[i]["type"].toString() == "Playback")
                    return lanesTop + i * rowHeight;
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
    void paintAudio(juce::Graphics &g, const Item &item, int y) const {
        const auto row = live::playback(state, item.value["id"].toString());
        if (item.value["id"] != state["currentItem"] || !owner.livePlaybackReady()) {
            text(g, juce::File(row["file"].toString()).getFileName(),
                 {item.bounds.getX() + 8, y + 10, item.bounds.getWidth() - 16, 28});
            text(g,
                 item.value["id"] != state["currentItem"]
                     ? "Select item to load audio"
                     : (owner.livePlaybackLoading ? "Loading WAV..." : owner.livePlaybackError),
                 {item.bounds.getX() + 8, y + 42, item.bounds.getWidth() - 16, 28}, true);
            return;
        }
        const auto project = owner.project();
        const auto clip = project->clips.front();
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
        }
        if (body.trimming()) {
            g.setColour(liveReferenceTokens().colour(ColourRole::focus));
            g.drawVerticalLine(juce::roundToInt(body.trimX()), area.getY(), area.getBottom());
        }
    }
    void commitMix() {
        if (owner.livePlaybackReady())
            owner.run([&] {
                owner.setLivePlaybackMix(playbackGain.getValue(), playbackMute.getToggleState(),
                                         playbackSolo.getToggleState());
            });
    }
    void syncChannel() {
        const auto row = owner.liveDocument
                             ? live::playback(owner.liveDocument->interfaceState,
                                              owner.liveDocument->interfaceState["currentItem"].toString())
                             : juce::var();
        const auto y = playbackLaneY() - viewport.getViewPositionY();
        const bool visible = y >= lanesTop - viewport.getViewPositionY() && y + rowHeight > 0 && y < getHeight();
        for (auto *control : std::array<juce::Component *, 3>{&playbackMute, &playbackSolo, &playbackGain}) {
            control->setVisible(visible && playbackLaneY() >= lanesTop);
            control->setEnabled(owner.livePlaybackReady());
        }
        playbackMute.setBounds(8, y + 58, 26, 22);
        playbackSolo.setBounds(36, y + 58, 26, 22);
        playbackGain.setBounds(6, y + 84, juce::jmax(0, channelWidth - 12), 24);
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
    Body body;
    Viewport viewport;
    PlayheadLine playhead;
    juce::TextButton playbackMute{"M"}, playbackSolo{"S"};
    juce::Slider playbackGain;
    juce::var state;
    juce::String signature, revealedItem;
    std::vector<Item> items;
    std::string activeSection;
    std::uint64_t focusRevision{};
    int channelWidth{150};
    bool observedWaveform{};
    mrs::PlaybackState observedPlayback{mrs::PlaybackState::stopped};
};
} // namespace ui
