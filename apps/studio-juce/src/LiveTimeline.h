#pragma once
#include "Desktop.h"
#include "LiveShowState.h"
#include "LiveTheme.h"

namespace ui {
// Native projection of the editor show. Geometry is metadata, never an audio clock.
class LiveTimeline final : public juce::Component {
  public:
    explicit LiveTimeline(Desktop &desktop)
        : owner(desktop), body(*this), viewport(*this), playhead([this] { return playheadBounds(); }) {
        setComponentID("live-show-timeline");
        setName("MR Live item timeline");
        addAndMakeVisible(viewport);
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
    }
    void refresh() {
        const auto next = owner.liveDocument ? owner.liveDocument->interfaceState : juce::var();
        const auto key = juce::JSON::toString(next, true);
        const auto playback = owner.displayState().playback;
        const bool started = playback != observedPlayback &&
                             (playback == mrs::PlaybackState::playing || playback == mrs::PlaybackState::stopped);
        observedPlayback = playback;
        if (key == signature) {
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
        if (selectionChanged)
            for (const auto &item : items)
                if (item.value["id"] == state["currentItem"]) {
                    viewport.setViewPosition(juce::jmax(0, item.bounds.getX() - 12), viewport.getViewPositionY());
                    break;
                }
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
            const auto meter = item.value["meter"].toString();
            const double numerator = meter.upToFirstOccurrenceOf("/", false, false).getDoubleValue();
            const double denominator = meter.fromFirstOccurrenceOf("/", false, false).getDoubleValue();
            const double bpm = static_cast<double>(item.value["tempo"]);
            if (numerator <= 0 || denominator <= 0 || bpm <= 0)
                return {};
            const double samplesPerBar = owner.project()->sample_rate * 60. / bpm * numerator * 4. / denominator;
            const double bar = owner.visualSample() / samplesPerBar;
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
    };
    class Viewport final : public juce::Viewport {
      public:
        explicit Viewport(LiveTimeline &host) : parent(host) {}
        void visibleAreaChanged(const juce::Rectangle<int> &) override { parent.repaint(); }

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

      private:
        LiveTimeline &parent;
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
        const auto entries = live::activeSetlist(state)["entries"];
        if (const auto *values = entries.getArray())
            for (const auto &entry : *values) {
                const auto item = live::find(state, "items", entry["itemId"].toString());
                if (!item.isObject())
                    continue;
                const auto view = state["performance"]["arrangement"][juce::Identifier(item["id"].toString())];
                const double scale = numeric(view["scale"], 16, 4, 128);
                const double lead = numeric(view["lead"], 0, 0, 4096);
                const double bars = numeric(view["length"], numeric(item["bars"], 1, 1, 4096), 1, 4096);
                // The model permits 1000 entries, 4096 bars/lead and 128 px/bar.
                // Keep their complete geometry within a signed-int scroll surface.
                const int width = juce::jlimit(6, 1048578, juce::roundToInt((bars + lead) * scale) + 2);
                items.push_back({item, {x, 0, width, height}, scale, lead, bars});
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
            text(g, item.value["name"].toString(), {x + 10, 4, juce::jmax(0, width - 20), 26});
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
                for (const auto &section : *sections)
                    block(g, extent(item, section, headerHeight + rulerHeight + 1, sectionHeight - 2),
                          section["name"].toString(), section["color"]);
            const auto chords = state["performance"]["chords"][juce::Identifier(id)];
            if (const auto *values = chords.getArray())
                for (const auto &chord : *values)
                    block(g, extent(item, chord, lanesTop - chordHeight + 2, chordHeight - 4),
                          chord["symbol"].toString(), chord["color"]);
            int y = lanesTop;
            if (const auto *channels = state["channels"].getArray())
                for (const auto &channel : *channels) {
                    if (const auto *tracks = item.value["tracks"].getArray())
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
    juce::var state;
    juce::String signature, revealedItem;
    std::vector<Item> items;
    int channelWidth{150};
    mrs::PlaybackState observedPlayback{mrs::PlaybackState::stopped};
};
} // namespace ui
