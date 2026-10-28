#pragma once
#include "Desktop.h"
#include "LiveContext.h"
#include "LiveMusicalState.h"
#include "LiveMusicalEditor.h"
#include "LiveClipEditor.h"
#include "LiveActionMenu.h"
#include "LivePerformanceUI.h"
#include "LivePlaybackView.h"
#include "LiveShowState.h"
#include "LiveTheme.h"
#include "LiveWaveformCache.h"
#include "WaveformView.h"

namespace ui {
juce::MouseCursor noteTrimCursor(bool right);
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
    void setClipScissors(bool enabled){body.scissors=enabled;body.repaint();}
    bool clipScissors() const{return body.scissors;}
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
            const auto clip = owner.app.playback_sources().front();
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
        const auto chord = owner.app.musical().state().current_chord;
        const auto nextChord = chord ? chord->id.value : std::string();
        if (nextChord != activeChord) { activeChord = nextChord; body.repaint(); }
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
            else if (started)
                revealTransport();
            return;
        }
        const auto selectedItem = next["currentItem"].toString();
        if (body.editing) body.cancelEdit();
        const bool selectionChanged = selectedItem != revealedItem;
        revealedItem = selectedItem;
        signature = key;
        state = next;
        layout();
        syncChannel();
        if (selectionChanged || focused)
            revealSelection();
        else if (started)
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
    juce::Rectangle<int> itemBounds(const juce::String &id) const {
        for (const auto &item : items) if (item.value["id"].toString() == id) return item.bounds;
        return {};
    }
    juce::Rectangle<int> playbackStemLaneBounds(const juce::String &channel) const {
        const int directY=channelY(channel);
        const int y=directY>=lanesTop?directY:channel==live::playbackContainer(state)?containerY():directY;
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
        juce::String entry;
        bool collapsed{};
    };
    bool currentEntry(const Item &item) const {
        if (item.value["id"] != state["currentItem"])
            return false;
        return owner.liveCurrentEntry.isEmpty() || item.entry == owner.liveCurrentEntry;
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
    class Body final : public juce::Component, public juce::SettableTooltipClient {
      public:
        explicit Body(LiveTimeline &host) : parent(host) { setComponentID("live-timeline-items"); setWantsKeyboardFocus(true); }
        void paint(juce::Graphics &g) override {
            parent.paintItems(g);
            if(guideY>=0){g.setColour(liveReferenceTokens().colour(ColourRole::focus));g.drawVerticalLine(juce::roundToInt(guideX),static_cast<float>(guideY+4),static_cast<float>(guideY+rowHeight-4));}
        }
        void mouseMove(const juce::MouseEvent &event) override {
            guideY=-1;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            if(hoverClip(event)){repaint();return;}
            repaint();
            setTooltip("Double-click Sections or Chord Track to add; right-click for properties");
            for (const auto &item : parent.items) if ((!item.collapsed || event.y<headerHeight) && item.bounds.contains(event.getPosition())) {
                if(event.y<30 && event.x>=item.bounds.getRight()-48 && event.x<item.bounds.getRight()-26){
                    setMouseCursor(juce::MouseCursor::PointingHandCursor);
                    setTooltip(live::performanceModeLabel(live::itemPerformanceMode(parent.state,item.value["id"].toString())));return;
                }
                if(event.y>=32 && event.y<56 && event.x>=item.bounds.getX()+6 && event.x<item.bounds.getX()+54) {
                    setMouseCursor(juce::MouseCursor::PointingHandCursor);setTooltip("− / + : timeline zoom");return;
                }
                if (event.y >= headerHeight+rulerHeight && event.y < lanesTop) {
                    const auto lane = event.y < lanesTop-chordHeight ? live::MusicalLane::sections : live::MusicalLane::chords;
                    const auto target = live::musicalHit(live::musicalRows(parent.state,item.value["id"].toString(),lane),
                                                        barAt(item,event.position.x),5./item.scale);
                    if (target.id.isNotEmpty()) {
                        if (target.mode == live::SpanGesture::joined) {
                            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);setTooltip("Move shared boundary");
                        } else if (target.mode != live::SpanGesture::move) {
                            setMouseCursor(noteTrimCursor(target.mode == live::SpanGesture::right));setTooltip("Trim musical event");
                        }
                        if(lane==live::MusicalLane::sections && target.mode==live::SpanGesture::move)
                            setTooltip(live::performanceModeLabel(live::itemPerformanceMode(parent.state,target.id)));
                        return;
                    }
                }
                if(event.y>=headerHeight && (event.x>=item.bounds.getRight()-5 ||
                    std::abs(event.position.x-(item.bounds.getX()+1+item.lead*item.scale))<=5)) {
                    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                    setTooltip("Drag origin to reveal lead-in; drag right boundary to change item end");return;
                }
            }
        }
        void mouseDown(const juce::MouseEvent &event) override {
            if(event.mods.isLeftButtonDown())++parent.owner.liveSelectionRevision;
            if (beginClipEdit(event)) return;
            if (beginEdit(event)) return;
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

        void mouseDrag(const juce::MouseEvent &event) override {if(editing)previewEdit(event);}
        void mouseUp(const juce::MouseEvent &) override {if(editing)finishEdit();}
        bool keyPressed(const juce::KeyPress &key) override {
            if (key == juce::KeyPress::escapeKey) { cancelEdit(); return true; }
            if (clipSelection.isNotEmpty()) {
                if(key==juce::KeyPress::deleteKey || key==juce::KeyPress::backspaceKey){removeClip();return true;}
                if(key==juce::KeyPress::returnKey){clipProperties();return true;}
            }
            if (selection.isEmpty()) return false;
            if (key == juce::KeyPress::returnKey) { properties(); return true; }
            if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) { removeSelected(); return true; }
            return false;
        }
        void focusLost(FocusChangeType) override { cancelEdit(); }
        void mouseDoubleClick(const juce::MouseEvent &event) override {
            cancelEdit();
            if(event.y>=lanesTop){if(beginClipEdit(event)){cancelEdit();if(!scissors)clipProperties(event.getScreenPosition());}return;}
            for (const auto &item : parent.items) if (!item.collapsed && item.bounds.contains(event.getPosition())) {
                if (event.y < headerHeight + rulerHeight || event.y >= lanesTop) return;
                selectedItem = item.value["id"].toString();
                selectedLane = event.y < lanesTop - chordHeight ? live::MusicalLane::sections : live::MusicalLane::chords;
                const double at = barAt(item,event.position.x);
                selection = hit(item,selectedLane,at);
                if(selection.isNotEmpty()) properties(event.getScreenPosition());
                else create(item,selectedLane,at,"Verse");
                return;
            }
        }
        juce::var regionRows(const Item& item,int index) const {
            const auto id=item.value["id"].toString();const auto rows=live::playbackRows(parent.state,id);
            auto regions=live::playbackRegions(parent.state,id,rows[static_cast<std::size_t>(index)]);
            if(regions.isVoid())regions=live::defaultPlaybackRegions(rows[static_cast<std::size_t>(index)],sourceBars(item,index),live::playbackSecondsPerBar(parent.state,id));
            return regions;
        }
        double sourceBars(const Item& item,int index) const {
            const auto source=live::playbackWaveform(parent.owner,parent.waveforms,parent.state,item.value["id"].toString(),static_cast<std::size_t>(index));
            return source.wave && source.rate>0?static_cast<double>(source.wave->frames())/source.rate/live::playbackSecondsPerBar(parent.state,item.value["id"].toString()):0.;
        }
        int clipLane(const Item& item,int y) const {
            const auto* lane=parent.laneAt(y);if(!lane)return -1;
            const auto rows=live::playbackRows(parent.state,item.value["id"].toString());
            if(lane->container && rows.size()!=1)return -1;
            return parent.playbackIndexForChannel(item.value["id"].toString(),lane->channel);
        }
        juce::String clipAt(const Item& item,int index,float x,int* edge=nullptr) const {
            juce::String result;double distance=6.;
            const auto regions=regionRows(item,index);
            // Last-painted overlapping region wins consistently.
            for(const auto& row:*regions.getArray()){
                const double left=item.bounds.getX()+1+(item.lead+static_cast<double>(row["start"]))*item.scale;
                const double right=left+static_cast<double>(row["length"])*item.scale;
                if(x>=left-5 && x<=right+5){
                    const double d=(std::min)(std::abs(x-left),std::abs(x-right));
                    if(d<=5 && d<=distance){result=row["id"].toString();distance=d;if(edge)*edge=std::abs(x-left)<std::abs(x-right)?-1:1;}
                    else if(distance>5 && x>=left && x<right){result=row["id"].toString();if(edge)*edge=0;}
                }
            }
            return result;
        }
        bool hoverClip(const juce::MouseEvent& event){
            if(event.y<lanesTop)return false;
            for(const auto& item:parent.items)if(!item.collapsed && item.bounds.contains(event.getPosition())){
                const int index=clipLane(item,event.y);if(index<0 || sourceBars(item,index)<=0)return false;
                int edge=0;const auto id=clipAt(item,index,event.position.x,&edge);
                if(id.isNotEmpty()){
                    if(edge)setMouseCursor(noteTrimCursor(edge>0));
                    else if(scissors){setMouseCursor(juce::MouseCursor::CrosshairCursor);guideX=static_cast<float>(item.bounds.getX()+1+(item.lead+live::clipSnap(barAt(item,event.position.x)))*item.scale);guideY=lanesTop+((event.y-lanesTop)/rowHeight)*rowHeight;}
                    setTooltip(edge?"Trim Playback region":scissors?"Cut at quarter-bar grid":"Drag Playback region; right-click for properties");
                }
                return true;
            }return false;
        }
        bool beginClipEdit(const juce::MouseEvent& event){
            if(event.y<lanesTop)return false;
            for(const auto& item:parent.items)if(!item.collapsed && item.bounds.contains(event.getPosition())){
                const int index=clipLane(item,event.y);if(index<0)return false;
                const double extent=sourceBars(item,index);if(extent<=0)return true;
                const auto id=item.value["id"].toString();const auto rows=live::playbackRows(parent.state,id);
                clipItem=id;clipStem=index==0?juce::String():rows[static_cast<std::size_t>(index)]["id"].toString();
                clipSourceBars=extent;clipSelection=clipAt(item,index,event.position.x,&clipEdge);selection.clear();
                if(event.mods.isPopupMenu()){clipContext(item,index,event.getScreenPosition());return true;}
                if(!event.mods.isLeftButtonDown())return true;
                grabKeyboardFocus();
                if(clipSelection.isEmpty()){
                    if(parent.currentEntry(item) && parent.owner.livePlaybackReady())parent.owner.run([&]{parent.owner.app.seek(parent.sampleAt(item,event.position.x));});
                    return true;
                }
                // Pointer selection retains the accepted click-to-seek action.
                // A subsequent drag still edits only the preview until release.
                if(!scissors && !clipEdge && parent.currentEntry(item) && parent.owner.livePlaybackReady())
                    parent.owner.run([&]{parent.owner.app.seek(parent.sampleAt(item,event.position.x));});
                editRows=regionRows(item,index).clone();
                if(scissors && !clipEdge){parent.owner.run([&]{commitClips(live::splitPlaybackRegion(editRows,clipSelection,barAt(item,event.position.x)),"Split Playback region");});return true;}
                originalState=parent.state.clone();baseline=live::canonical(parent.owner.liveDocument->interfaceState);
                editingItem=item;dragStart=event.position.x;dragPosition=dragStart;dragScreenX=static_cast<float>(event.getScreenPosition().x);
                const auto screenPoint=event.getScreenPosition().toFloat();
                screenUnits=getLocalPoint(nullptr,screenPoint.translated(1.f,0.f)).x-getLocalPoint(nullptr,screenPoint).x;
                viewKey.clear();copy=false;invalid=false;clipEditing=true;editing=true;repaint();return true;
            }return false;
        }
        void commitClips(const juce::var& next,const juce::String& label){
            parent.owner.run([&]{parent.owner.editLiveInterface(label,[&](auto& state){
                live::setPlaybackRegions(state,clipItem,clipStem,next);
            });parent.owner.syncLivePlayback();});
        }
        juce::var selectedClipRows() const {
            const auto row=live::playbackStem(parent.state,clipItem,clipStem);
            if(!row.isObject())return juce::Array<juce::var>();
            auto regions=live::playbackRegions(parent.state,clipItem,row);
            return regions.isVoid()?live::defaultPlaybackRegions(row,clipSourceBars,live::playbackSecondsPerBar(parent.state,clipItem)):regions;
        }
        void removeClip(){auto rows=selectedClipRows().clone();for(int i=rows.size();--i>=0;)if(rows[i]["id"].toString()==clipSelection)rows.getArray()->remove(i);commitClips(rows,"Remove Playback region");clipSelection.clear();}
        void clipProperties(juce::Point<int> screen=juce::Desktop::getMousePosition()){
            if(clipSelection.isNotEmpty())openLiveClipEditor(parent.owner,clipItem,clipStem,clipSelection,selectedClipRows(),clipSourceBars,screen);
        }
        void clipContext(Item item,int index,juce::Point<int> screen){
            juce::PopupMenu menu;menu.addItem(1,"Pointer",true,!scissors);menu.addItem(2,"Scissors",true,scissors);menu.addSeparator();
            menu.addItem(3,"Region properties...",clipSelection.isNotEmpty());menu.addItem(4,"Delete region",clipSelection.isNotEmpty());menu.addItem(5,"Restore original region");
            juce::Component::SafePointer<Body> safe(this);const auto selected=clipSelection,sourceItem=clipItem,sourceStem=clipStem;
            showLiveActionMenu(menu,parent.owner,liveContextTarget(screen),[safe,item,index,screen,selected,sourceItem,sourceStem](int action){
                if(!safe)return;
                safe->clipItem=sourceItem;safe->clipStem=sourceStem;safe->clipSelection=selected;safe->clipSourceBars=safe->sourceBars(item,index);
                if(action==1||action==2){safe->scissors=action==2;return;}
                if(action==3)safe->clipProperties(screen);
                if(action==4)safe->removeClip();
                if(action==5){auto row=live::object();safe->commitClips(live::defaultPlaybackRegions(row,safe->clipSourceBars,live::playbackSecondsPerBar(safe->parent.state,sourceItem)),"Restore Playback region");}
            });
        }
        void mouseExit(const juce::MouseEvent&)override{guideY=-1;repaint();}
        bool scissors{},clipEditing{};
        juce::String clipItem,clipStem,clipSelection;
        double clipSourceBars{};int clipEdge{},guideY{-1};float guideX{};
        bool beginEdit(const juce::MouseEvent &event) {
            for (const auto &item : parent.items) {
                if(!item.bounds.contains(event.getPosition()) || (item.collapsed && event.y>=headerHeight))continue;
                const auto id=item.value["id"].toString();
                if(event.y<headerHeight && (event.mods.isPopupMenu() || (event.y<30 && event.x>=item.bounds.getRight()-48 && event.x<item.bounds.getRight()-26))){
                    if(event.mods.isPopupMenu() || event.mods.isLeftButtonDown())showLivePerformanceModes(parent.owner,id,id,event.getScreenPosition());
                    return true;
                }
                if(event.y>=32 && event.y<56 && event.x>=item.bounds.getX()+6 && event.x<item.bounds.getX()+54) {
                    if(event.mods.isLeftButtonDown()) {
                        const double scale=juce::jlimit(4.,128.,item.scale*(event.x<item.bounds.getX()+30?.8:1.25));
                        parent.owner.run([&]{parent.owner.editLiveInterface("Timeline zoom",[&](auto &state){live::setItemView(state,id,"scale",scale);});});
                    }
                    return true;
                }
                const bool musical=event.y>=headerHeight+rulerHeight && event.y<lanesTop;
                const bool origin=event.y>=headerHeight && std::abs(event.position.x-(item.bounds.getX()+1+item.lead*item.scale))<=5;
                const bool right=event.y>=headerHeight && event.x>=item.bounds.getRight()-5;
                if(!musical && !origin && !right)return false;
                if(event.mods.isPopupMenu() && musical) {
                    selectedItem=id;selectedLane=event.y<lanesTop-chordHeight?live::MusicalLane::sections:live::MusicalLane::chords;
                    selection=hit(item,selectedLane,barAt(item,event.position.x));
                    context(item,barAt(item,event.position.x),event.getScreenPosition());return true;
                }
                if(!event.mods.isLeftButtonDown())return false;
                grabKeyboardFocus();
                clipEditing=false;
                originalState=parent.state.clone(); baseline=live::canonical(parent.owner.liveDocument->interfaceState);
                editingItem=item; dragStart=event.position.x; dragPosition=dragStart;
                dragScreenX=static_cast<float>(event.getScreenPosition().x);
                const auto screenPoint=event.getScreenPosition().toFloat();
                screenUnits=getLocalPoint(nullptr,screenPoint.translated(1.f,0.f)).x-getLocalPoint(nullptr,screenPoint).x;
                viewKey=!musical?(right?"length":origin?"lead":""):"";
                if(viewKey.isNotEmpty()) { editing=true; return true; }
                clipSelection.clear();
                selectedItem=id;selectedLane=event.y<lanesTop-chordHeight?live::MusicalLane::sections:live::MusicalLane::chords;
                const auto target=live::musicalHit(live::musicalRows(parent.state,id,selectedLane),barAt(item,event.position.x),5./item.scale);
                selection=target.id; mode=target.mode;
                repaint();
                if(selection.isEmpty())return true;
                editRows=live::musicalRows(originalState,id,selectedLane).clone();
                copy=event.mods.isAltDown() && selectedLane==live::MusicalLane::chords && mode==live::SpanGesture::move;
                editing=true;parent.body.repaint();
                if(selectedLane==live::MusicalLane::sections && parent.currentEntry(item) && parent.owner.liveTransportReady()) {
                    // Edge selection is for editing authored geometry. Navigation
                    // resolves against Core's single-valued lane, including old
                    // shows where a later section truncates an overlapping one.
                    const auto project=parent.owner.project();const auto& meter=project->time.meters.front();
                    const double at=barAt(item,event.position.x);
                    const auto tick=static_cast<mrs::Tick>(std::llround(at*static_cast<double>(4*mrs::ppq*meter.numerator/meter.denominator)));
                    for(const auto& section:project->sections)if(section.start<=tick && tick<section.end){
                        parent.owner.run([&]{parent.owner.app.musical().seek_section(section.id);});break;
                    }
                }
                return true;
            }
            return false;
        }
        void previewEdit(const juce::MouseEvent &event) {
            // Bounds/scroll can change during origin/end preview. Screen delta
            // converted once at mouse-down remains stable at any UI zoom.
            dragPosition=dragStart+(static_cast<float>(event.getScreenPosition().x)-dragScreenX)*screenUnits;
            const double delta=(dragPosition-dragStart)/editingItem.scale;
            auto preview=originalState.clone();
            try {
                if(clipEditing) {
                    const auto next=live::editPlaybackRegion(editRows,clipSelection,delta,clipEdge,clipSourceBars);
                    live::setPlaybackRegions(preview,clipItem,clipStem,next);
                } else if(viewKey.isNotEmpty()) {
                    const double initial=viewKey=="lead"?editingItem.lead:editingItem.bars;
                    live::setItemView(preview,editingItem.value["id"].toString(),juce::Identifier(viewKey),
                        juce::jlimit(viewKey=="lead"?0.:1.,4096.,live::barSnap(initial+delta)));
                } else {
                    const auto next=live::editMusicalSpan(editRows,selection,delta,mode,selectedLane,copy);
                    live::setMusicalRows(preview,selectedItem,selectedLane,next);
                }
                invalid=false; parent.state=preview; parent.layout(); parent.playhead.update(); repaint();
            }catch(const std::exception &e){invalid=true;setTooltip(juce::String::fromUTF8(e.what()));repaint();}
        }
        void finishEdit() {
            const bool changed=std::abs(dragPosition-dragStart)>=1.f && !invalid;
            const auto next=parent.state.clone();
            editing=false; parent.state=parent.owner.liveDocument->interfaceState;parent.layout();parent.playhead.update();repaint();
            if(!changed || baseline!=live::canonical(parent.owner.liveDocument->interfaceState))return;
            parent.owner.run([&]{parent.owner.editLiveInterface(clipEditing?"Edit Playback region":viewKey.isNotEmpty()?"Item "+viewKey:"Edit musical span",[&](auto &state){
                if(clipEditing) live::setPlaybackRegions(state,clipItem,clipStem,live::playbackRegions(next,clipItem,live::playbackStem(next,clipItem,clipStem)));
                else if(viewKey.isNotEmpty()) {
                    const auto id=editingItem.value["id"].toString();
                    live::setItemView(state,id,juce::Identifier(viewKey),static_cast<double>(next["performance"]["arrangement"][juce::Identifier(id)][juce::Identifier(viewKey)]));
                } else live::setMusicalRows(state,selectedItem,selectedLane,live::musicalRows(next,selectedItem,selectedLane));
            });if(clipEditing)parent.owner.syncLivePlayback();});
            if (!clipEditing && viewKey.isEmpty() && copy) {
                const auto committed=live::musicalRows(parent.owner.liveDocument->interfaceState,selectedItem,selectedLane);
                if (const auto *values=committed.getArray()) for(const auto &row:*values)
                    if (!live::spanById(editRows,row["id"].toString()).isObject()) selection=row["id"].toString();
                repaint();
            }
        }
        void cancelEdit() {
            if(!editing)return;
            editing=false;invalid=false;
            if(parent.owner.liveDocument)parent.state=parent.owner.liveDocument->interfaceState;
            parent.layout();parent.playhead.update();repaint();
        }
        void properties(juce::Point<int> screen=juce::Desktop::getMousePosition()) {
            if(selection.isNotEmpty())openLiveMusicalEditor(parent.owner,selectedItem,selectedLane,selection,screen);
        }
        void removeSelected() {
            if(selection.isEmpty())return;
            parent.owner.run([&]{parent.owner.editLiveInterface("Remove musical event",[&](auto &state){
                auto values=live::musicalRows(state,selectedItem,selectedLane).clone();
                for(int i=values.size();--i>=0;)if(values[i]["id"].toString()==selection)values.getArray()->remove(i);
                live::setMusicalRows(state,selectedItem,selectedLane,values);
            });});selection.clear();
        }
        void create(Item item,live::MusicalLane lane,double at,const juce::String &name) {
            const auto id=item.value["id"].toString();
            parent.owner.run([&]{parent.owner.editLiveInterface("Add musical event",[&](auto &state){
                auto next=live::newMusicalSpan(state,id,lane,at,item.bars,name);
                selection=next[next.size()-1]["id"].toString();selectedItem=id;selectedLane=lane;
                live::setMusicalRows(state,id,lane,next);
            });});
        }
        void context(Item item,double at,juce::Point<int> screen) {
            juce::PopupMenu menu;
            menu.addItem(1,"Properties / color...",selection.isNotEmpty());
            menu.addItem(2,"Duplicate",selection.isNotEmpty());menu.addItem(3,"Remove",selection.isNotEmpty());
            if(selectedLane==live::MusicalLane::sections && selection.isNotEmpty()){
                menu.addSeparator();addLivePerformanceModes(menu,parent.state,selection,50);
            }
            menu.addSeparator();
            const juce::StringArray names{"Intro","Verse","Chorus","Bridge","Solo","Outro"};
            if(selectedLane==live::MusicalLane::chords)menu.addItem(10,"Add chord");
            else for(int i=0;i<names.size();++i)menu.addItem(10+i,"Add "+names[i]);
            juce::Component::SafePointer<Body> safe(this);
            showLiveActionMenu(menu,parent.owner,liveContextTarget(screen),[safe,item,at,screen,names,modeTarget=selection](int result){
                if(!safe)return;
                if(result==1)safe->properties(screen);
                if(result==3)safe->removeSelected();
                if(result>=50 && result<54){safe->parent.owner.run([&]{safe->parent.owner.editLiveInterface("Section performance mode",[&](auto& state){
                    live::setPerformanceMode(state,item.value["id"].toString(),modeTarget,static_cast<mrs::PerformanceMode>(result-50));
                });});return;}
                if(result>=10 && result<16)safe->create(item,safe->selectedLane,at,names[result-10]);
                if(result==2) {
                    const auto original=live::spanById(live::musicalRows(safe->parent.state,safe->selectedItem,safe->selectedLane),safe->selection);
                    safe->parent.owner.run([&]{safe->parent.owner.editLiveInterface("Duplicate musical event",[&](auto &state){
                        const auto values=live::musicalRows(state,safe->selectedItem,safe->selectedLane);
                        live::setMusicalRows(state,safe->selectedItem,safe->selectedLane,
                            live::editMusicalSpan(values,safe->selection,static_cast<double>(original["length"]),live::SpanGesture::move,safe->selectedLane,true));
                    });});
                }
            });
        }
        static double barAt(const Item &item,float x) {return (x-item.bounds.getX()-1.)/item.scale-item.lead;}
        juce::String hit(const Item &item,live::MusicalLane lane,double at) const {
            juce::String result;
            const auto values=live::musicalRows(parent.state,item.value["id"].toString(),lane);
            if(const auto *entries=values.getArray())for(const auto &row:*entries)
                if(at>=static_cast<double>(row["start"]) && at<static_cast<double>(row["start"])+static_cast<double>(row["length"]))result=row["id"].toString();
            return result;
        }
        bool editing{}; bool invalid{},copy{};
        float dragScreenX{},screenUnits{1.f};
        Item editingItem{};
        live::MusicalLane selectedLane{live::MusicalLane::sections};
        live::SpanGesture mode{live::SpanGesture::move};
        juce::String selectedItem,selection,viewKey,baseline;
        juce::var originalState,editRows;

      private:
        LiveTimeline &parent;
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
        // Waveform completion can widen preceding items. Retain the visible
        // selected entry (or the browsed entry) at the same viewport position.
        const auto visible=viewport.getViewArea();const Item* anchor=nullptr;
        for(const auto& item:items)if(item.bounds.intersects(visible)){
            if(!anchor)anchor=&item;
            if(currentEntry(item)){anchor=&item;break;}
        }
        const auto anchorEntry=anchor?anchor->entry:juce::String();
        const auto anchorItem=anchor?anchor->value["id"].toString():juce::String();
        const int anchorScreenX=anchor?anchor->bounds.getX()-visible.getX():0;
        items.clear();
        int x = 0;
        lanes = live::playbackLanes(state);
        const int height = lanesTop + static_cast<int>(lanes.size()) * rowHeight;
        auto entries = live::activeSetlist(state)["entries"].clone();
        if (const auto *values = entries.getArray())
            for (const auto &entry : *values) {
                const auto item = live::find(state, "items", entry["itemId"].toString());
                if (!item.isObject())
                    continue;
                const auto view = state["performance"]["arrangement"][juce::Identifier(item["id"].toString())];
                const double scale = numeric(view["scale"], 16, 4, 128);
                const double lead = numeric(view["lead"], 0, 0, 4096);
                double bars = numeric(view["length"], numeric(item["bars"], 16, 1, 4096), 1, 4096);
                double audioBars = 0.;
                if (!view.hasProperty("length") && item["id"] == state["currentItem"] && owner.livePlaybackReady()) {
                    const auto project = owner.project();
                    mrs::Sample end = 0;
                    for (const auto &clip : project->clips)
                        end = juce::jmax(end, clip.start + clip.length);
                    const auto &meter = project->time.meters.front();
                    const mrs::Timeline clock(project->time, project->sample_rate);
                    audioBars = static_cast<double>(clock.to_ticks(end)) /
                                                      (4. * mrs::ppq * meter.numerator / meter.denominator);
                }
                if (!view.hasProperty("length")) for (const auto &row : live::playbackRows(state, item["id"].toString())) {
                    const auto regions=live::playbackRegions(state,item["id"].toString(),row);
                    if(const auto* regionValues=regions.getArray())for(const auto& region:*regionValues)
                        audioBars=juce::jmax(audioBars,static_cast<double>(region["start"])+static_cast<double>(region["length"]));
                    if(regions.isArray())continue;
                    const auto *cached = waveforms.find(row["file"].toString());
                    if (!cached || !cached->wave)
                        continue;
                    const auto projection = live::musicalProjection(live::musicalVersion(state, item["id"].toString()));
                    const double trimEnd = row["trimEnd"].isVoid()
                                               ? static_cast<double>(cached->wave->frames()) / cached->rate
                                               : static_cast<double>(row["trimEnd"]);
                    const double seconds = juce::jmax(0., trimEnd - static_cast<double>(row["trimStart"]));
                    const mrs::Timeline clock(projection.time, cached->rate);
                    const auto &meter = projection.time.meters.front();
                    audioBars = juce::jmax(audioBars, static_cast<double>(clock.to_ticks(static_cast<mrs::Sample>(
                        std::llround(seconds * cached->rate)))) / (4. * mrs::ppq * meter.numerator / meter.denominator));
                }
                if (!view.hasProperty("length") && audioBars > 0.) bars = juce::jmin(4096., audioBars);
                // The model permits 1000 entries, 4096 bars/lead and 128 px/bar.
                // Keep their complete geometry within a signed-int scroll surface.
                const bool collapsed = static_cast<bool>(state["performance"]["collapsedItems"][juce::Identifier(entry["id"].toString())]);
                const int width = collapsed ? 160 : juce::jlimit(6, 1048578, juce::roundToInt((bars + lead) * scale) + 2);
                items.push_back({item,
                                 {x, 0, width, height},
                                 scale,
                                 lead,
                                 bars,
                                 entry["id"].toString(), collapsed});
                x += width;
            }
        // Even a short final item (including a collapsed entry) must
        // be scrollable to the left edge, rather than clamped at the far right
        // behind the preceding song. This is view space, not show duration.
        const int focusWidth = items.empty() ? 0 : items.back().bounds.getX() + viewport.getMaximumVisibleWidth();
        body.setSize(juce::jmax(x, juce::jmax(focusWidth, viewport.getMaximumVisibleWidth())),
                     juce::jmax(height, viewport.getMaximumVisibleHeight()));
        if(anchorItem.isNotEmpty())for(const auto& item:items)
            if(item.entry==anchorEntry&&item.value["id"].toString()==anchorItem){
                viewport.setViewPosition(juce::jmax(0,juce::jmin(item.bounds.getRight()-1,item.bounds.getX()-anchorScreenX)),visible.getY());break;
            }
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
                 item.value["name"].toString(),
                 {x + 10, 4, juce::jmax(0, width - 64), 26});
            text(g,
                 item.value["tempo"].toString() + " BPM · " + item.value["meter"].toString() + " · " +
                     juce::String(item.bars) + " bars",
                 {x + 58, 30, juce::jmax(0, width - 68), 22}, true);
            text(g, "−   +", {x + 8, 32, 48, 22});
            performanceArt->draw(g,live::itemPerformanceMode(state,id),{x+width-48,8,18,18});
            text(g, item.collapsed ? ">" : "v", {x + width - 24, 4, 20, 26});
            if (item.collapsed) { line(g, item.bounds); continue; }
            g.setColour(tokens.colour(ColourRole::focus).withAlpha(.7f));
            g.drawVerticalLine(x + 1 + juce::roundToInt(item.lead * item.scale), static_cast<float>(headerHeight), static_cast<float>(item.bounds.getHeight()));
            g.fillRect(x + width - 4, headerHeight, 4, item.bounds.getHeight() - headerHeight);
            const int step = juce::jmax(1, static_cast<int>(std::ceil(item.bars / 16)));
            for (int bar = -static_cast<int>(std::ceil(item.lead)); bar < static_cast<int>(std::ceil(item.bars)); ++bar) {
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
                    block(g, area, {}, section["color"]);
                    if(area.getWidth()>=30){
                        text(g,section["name"].toString(),area.reduced(6,0).withTrimmedRight(20));
                        performanceArt->draw(g,live::itemPerformanceMode(state,section["id"].toString()),{area.getRight()-24,area.getCentreY()-8,16,16});
                    }else if(area.getWidth()>12)text(g,section["name"].toString(),area.reduced(4,0));
                    if (body.selectedItem == id && body.selection == section["id"].toString()) {
                        g.setColour(tokens.colour(body.editing && body.invalid ? ColourRole::error : ColourRole::focus)); g.drawRect(area, 2);
                    }
                    if (selected && section["id"].toString().toStdString() == activeSection) {
                        g.setColour(tokens.colour(ColourRole::focus));
                        g.drawRect(area, 2);
                    }
                }
            const auto chords = state["performance"]["chords"][juce::Identifier(id)];
            if (const auto *values = chords.getArray())
                for (const auto &chord : *values) {
                    block(g, extent(item, chord, lanesTop - chordHeight + 2, chordHeight - 4),
                          chord["symbol"].toString(), chord["color"]);
                    if ((body.selectedItem == id && body.selection == chord["id"].toString()) ||
                        (selected && chord["id"].toString().toStdString() == activeChord)) {
                        g.setColour(tokens.colour(body.editing && body.invalid && body.selection == chord["id"].toString()
                                                     ? ColourRole::error : ColourRole::focus));
                        g.drawRect(extent(item, chord, lanesTop - chordHeight + 2, chordHeight - 4), 2);
                    }
                }
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
        if(rows.size()==1){paintAudio(g,item,y,0);return;}
        std::vector<live::PlaybackWaveformSource> sources;
        double end = 0.; int ready = 0, unavailable = 0;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto source = live::playbackWaveform(owner, waveforms, state, id, i);
            sources.push_back(source);
            end = (std::max)(end, source.start + source.length);
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
        const juce::Rectangle<float> area{x, static_cast<float>(y + 8), (std::max)(1.f, right - x), static_cast<float>(rowHeight - 16)};
        if (ready) block(g, area.getSmallestIntegerContainer(), {}, "blue");
        text(g, "Playback · " + juce::String(static_cast<int>(rows.size())) + " stems" +
                     (unavailable ? " · file unavailable" : ready < static_cast<int>(rows.size()) ? " · preparing waveforms" : ""),
             {item.bounds.getX() + 8, y + 10, item.bounds.getWidth() - 16, 24});
        if (ready && end > 0.) {
            const auto waveArea = area.withTrimmedTop(28).reduced(1.f, 4.f);
            g.setColour(juce::Colour(0xff17b5da));
            drawPeakEnvelope(g, 1, [&](double begin, double finish, std::uint32_t) {
                return live::averagePlaybackEnvelope(sources, begin, finish);
            }, waveArea, 0., end / (std::max)(1.f, waveArea.getWidth()));
        }
    }
    void paintAudio(juce::Graphics& g,const Item& item,int y,std::size_t index) const {
        const auto id=item.value["id"].toString();const auto rows=live::playbackRows(state,id);if(index>=rows.size())return;
        const auto& row=rows[index];const auto source=live::playbackWaveform(owner,waveforms,state,id,index);
        const double seconds=live::playbackSecondsPerBar(state,id);
        auto regions=live::playbackRegions(state,id,row);
        if(regions.isVoid() && source.wave)regions=live::defaultPlaybackRegions(row,static_cast<double>(source.wave->frames())/source.rate/seconds,seconds);
        if(!regions.isArray()){
            const auto* cached=waveforms.find(row["file"].toString());
            text(g,cached && cached->error.isNotEmpty()?cached->error:"Preparing waveform...",{item.bounds.getX()+8,y+42,item.bounds.getWidth()-16,28},true);return;
        }
        if(regions.size()==0){text(g,"No regions · right-click to restore",{item.bounds.getX()+8,y+42,item.bounds.getWidth()-16,28},true);return;}
        for(const auto& region:*regions.getArray()){
            const double start=region["start"],length=region["length"],offset=region["offset"];
            const juce::Rectangle<float> area{static_cast<float>(item.bounds.getX()+1+(item.lead+start)*item.scale),static_cast<float>(y+10),
                static_cast<float>((std::max)(1.,length*item.scale)),static_cast<float>(rowHeight-20)};
            block(g,area.getSmallestIntegerContainer(),{},"blue");
            text(g,juce::File(row["file"].toString()).getFileName(),area.getSmallestIntegerContainer().reduced(6).withHeight(24));
            if(source.wave){const auto waveArea=area.withTrimmedTop(30).reduced(1.f,4.f);g.setColour(juce::Colour(0xff17b5da));
                drawWaveform(g,*source.wave,waveArea,offset*seconds*source.rate,length*seconds*source.rate/juce::jmax(1.f,waveArea.getWidth()));}
            if(body.clipItem==id && body.clipStem==(index==0?juce::String():row["id"].toString()) && body.clipSelection==region["id"].toString()){
                g.setColour(liveReferenceTokens().colour(body.invalid?ColourRole::error:ColourRole::focus));g.drawRect(area,2.f);
            }
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
    juce::SharedResourcePointer<LivePerformanceArt> performanceArt;
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
    std::string activeSection, activeChord;
    std::uint64_t focusRevision{};
    int channelWidth{150};
    int observedWaveform{};
    mrs::PlaybackState observedPlayback{mrs::PlaybackState::stopped};
};
} // namespace ui
