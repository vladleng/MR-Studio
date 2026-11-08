#pragma once
#include <mrs/diagnostics.hpp>
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
#include "LiveChannelControls.h"
#include "LiveWaveformCache.h"
#include "WaveformView.h"

namespace ui {
juce::MouseCursor noteTrimCursor(bool right);
// Native projection of the editor show. Geometry is metadata, never an audio clock.
class LiveTimeline final : public juce::Component, public juce::DragAndDropTarget {
  public:
    explicit LiveTimeline(Desktop &desktop)
        : owner(desktop), body(*this), viewport(*this), playhead([this] { return playheadBounds(); }) {
        setComponentID("live-show-timeline");
        setName("MR Live item timeline");
        addAndMakeVisible(viewport);
        addAndMakeVisible(stemsFold);
        stemsFold.setComponentID("live-stems-fold");
        stemsFold.setTitle("Expand or collapse Playback stems");
        stemsFold.onClick = [this] {
            owner.run([&] {
                if(!owner.liveDocument)return;
                owner.setLivePlaybackStemsExpanded(!live::playbackExpanded(owner.liveDocument->interfaceState));
                refresh(); // Recompute only Timeline lanes; keep Setlist unchanged.
            });
        };
        for(auto* button:{&zoomOut,&zoomIn})addAndMakeVisible(button);
        zoomOut.setComponentID("live-zoom-out");zoomIn.setComponentID("live-zoom-in");
        zoomOut.setTooltip("Zoom out");zoomIn.setTooltip("Zoom in");
        zoomOut.onClick=[this]{zoom(.8);};zoomIn.onClick=[this]{zoom(1.25);};
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
        zoomOut.setVisible(channelWidth>=56);zoomIn.setVisible(channelWidth>=56);
        zoomOut.setBounds(juce::jmax(0,channelWidth-54),36,24,24);
        zoomIn.setBounds(juce::jmax(0,channelWidth-28),36,24,24);
        viewport.setBounds(getLocalBounds().withTrimmedLeft(channelWidth));
        layout();
        syncChannel();
    }
    // Same horizontal zoom increments as MR Studio (+25%, -20%).
    // Keep the scale per current .mrlive item, preserving Undo and Save/Open.
    void zoom(double factor) {
        if (!owner.liveDocument) return;
        owner.run([&] {
            const auto& show=owner.liveDocument->interfaceState;
            const auto itemId=show["currentItem"].toString();
            if (!live::find(show,"items",itemId).isObject()) return;
            const auto view=show["performance"]["arrangement"][juce::Identifier(itemId)];
            const double oldScale=numeric(view["scale"],16.,4.,128.);
            owner.editLiveItemView(itemId,"scale",juce::jlimit(4.,128.,oldScale*factor));
        });
    }
    void zoomFit() {
        if (!owner.liveDocument) return;
        owner.run([&] {
            const auto& show=owner.liveDocument->interfaceState;
            const auto itemId=show["currentItem"].toString();
            const auto item=live::find(show,"items",itemId);
            if (!item.isObject()) return;
            const auto view=show["performance"]["arrangement"][juce::Identifier(itemId)];
            const double bars=numeric(view["length"],numeric(item["bars"],16.,1.,4096.),.25,4096.);
            const double start=numeric(view["start"],0.,0.,4095.75);
            const double lead=numeric(view["lead"],0.,0.,4096.);
            const double width=static_cast<double>(juce::jmax(60,viewport.getMaximumVisibleWidth()-12));
            const double scale=juce::jlimit(4.,128.,width/(bars-start+lead));
            owner.editLiveItemView(itemId,"scale",scale);
        });
    }
    void setClipScissors(bool enabled){body.scissors=enabled;body.repaint();}
    bool clipScissors() const{return body.scissors;}
    // View-only transport aid. Never changes .mrlive, Core or Undo history.
    void setFollowPlayhead(bool enabled) {
        followEnabled=enabled;
        followEngaged=false;
        followSuspended=false;
    }
    bool followsPlayhead() const noexcept {return followEnabled;}
    bool followPausedByNavigation() const noexcept {return followSuspended;}
    // The playhead advances freely until it reaches the viewport midpoint;
    // once there, only positive scroll is needed. Clamp at the timeline end.
    static int followScrollX(int playheadX,int scrollX,int visibleWidth,int maximumScroll,
                             bool alreadyCentred=false) {
        if(visibleWidth<=0 || maximumScroll<=0)return juce::jlimit(0,juce::jmax(0,maximumScroll),scrollX);
        const int current=juce::jlimit(0,maximumScroll,scrollX);
        const int centre=visibleWidth/2;
        const int offset=playheadX-current;
        // Looping or a backwards seek must not pull the viewport abruptly
        // to the left; wait until playback reaches its midpoint again.
        if(offset<centre)return current;
        const int desired=juce::jlimit(0,maximumScroll,playheadX-centre);
        if(desired<=current)return current;
        // A distant initial seek must not snap the viewport hundreds of px.
        return alreadyCentred?desired:juce::jmin(desired,current+64);
    }
    // Extract a single lane rather than JUCE's default full-component drag
    // snapshot (which previously displayed every channel simultaneously).
    // Keep the source rectangle in timeline-local screen coordinates, including
    // vertical viewport scrolling; do not capture waveform/item strips.
    juce::Rectangle<int> channelDragPreviewBounds(const juce::String& id,
                                                    int draggedLaneIndex=-1) const {
        int index=draggedLaneIndex;
        if(index<0 || index>=static_cast<int>(lanes.size()) ||
           lanes[static_cast<std::size_t>(index)].channel!=id) {
            index=-1;
            // A loaded primary Playback is drawn twice (group + WAV child)
            // with the same channel ID. Prefer the group for a root drag.
            for(std::size_t i=0;i<lanes.size();++i)
                if(lanes[i].channel==id && !lanes[i].child) {
                    index=static_cast<int>(i);break;
                }
            if(index<0)for(std::size_t i=0;i<lanes.size();++i)
                if(lanes[i].channel==id) {index=static_cast<int>(i);break;}
        }
        if(index<0)return {};
        const int rowY=lanesTop+index*rowHeight-viewport.getViewPositionY();
        return juce::Rectangle<int>(0,rowY,juce::jmax(0,channelWidth),rowHeight)
            .getIntersection(getLocalBounds());
    }
    juce::Image channelDragPreview(const juce::String& id,int draggedLaneIndex=-1) {
        const auto r=channelDragPreviewBounds(id,draggedLaneIndex);
        if(r.isEmpty())return {};
        // Match JUCE's default Mixer DnD ghost: the default snapshot is
        // converted to ARGB and shown at 60% opacity. Our cropped Timeline
        // snapshot bypasses that automatic treatment unless applied here.
        auto image=createComponentSnapshot(r,true).convertedToFormat(juce::Image::ARGB);
        image.multiplyAllAlphas(0.6f);
        return image;
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
        // A 16 MiB VST3 preset lives in channel metadata. Never stringify the
        // entire show on every Timeline refresh: show edits, Undo and Open all
        // advance these revisions, while transport/focus have separate keys.
        auto key=juce::String(static_cast<juce::int64>(owner.liveDocumentGeneration))+"|"+
                 juce::String(static_cast<juce::int64>(owner.liveUiRevision))+"|"+
                 juce::String(static_cast<juce::int64>(owner.livePlaybackFoldRevision))+"|"+
                 juce::String(static_cast<juce::int64>(owner.liveEdits.timeline))+"|"+
                 next["currentItem"].toString()+"|"+next["activeSetlist"].toString();
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
        const bool startingPlay=playback==mrs::PlaybackState::playing &&
                                observedPlayback!=mrs::PlaybackState::playing;
        if(startingPlay) {followEngaged=false;followSuspended=false;}
        else if(playback!=mrs::PlaybackState::playing)followEngaged=false;
        observedPlayback = playback;
        const bool focused = focusRevision != owner.liveTimelineFocusRevision;
        focusRevision = owner.liveTimelineFocusRevision;
        if(focused && playback==mrs::PlaybackState::playing && followEnabled)
            followSuspended=true;
        if (key == signature) {
            // Scalar M/S/gain/pan changes do not rebuild the entire Timeline.
            if(observedMixRevision!=owner.liveChannelMixRevision) {
                observedMixRevision=owner.liveChannelMixRevision;
                state=next;
                syncChannel();
                body.repaint();
            }
            if(observedPaintRevision!=owner.liveEdits.paint) {
                observedPaintRevision=owner.liveEdits.paint;state=next;
                for(auto& item:items)item.value=live::find(next,"items",item.value["id"].toString());
                body.repaint();
            }
            if (focused)
                revealSelection();
            else if (started && !followEnabled)
                revealTransport();
            updatePlayheadFollow(playback);
            return;
        }
        const auto selectedItem = next["currentItem"].toString();
        if (body.editing) body.cancelEdit();
        const bool selectionChanged = selectedItem != revealedItem;
        revealedItem = selectedItem;
        signature = key;
        observedMixRevision=owner.liveChannelMixRevision;
        state = next;
        observedPaintRevision=owner.liveEdits.paint;
        layout();
        syncChannel();
        if (selectionChanged || focused) {
            if(followEnabled && playback==mrs::PlaybackState::playing)
                followSuspended=true;
            revealSelection();
        }else if (started && !followEnabled)
            revealTransport();
        updatePlayheadFollow(playback);
        playhead.update();
        body.repaint();
        repaint();
    }
    // Shared selection entrypoint for Timeline hits and UI regression tests.
    // PB selection is transient; it must not mutate show history.
    void selectPlaybackChannel(const juce::String &id) {
        if(!owner.liveDocument)return;
        const auto channel=live::find(state,"channels",id);
        if(channel["type"].toString()!="Playback")return;
        const auto item=state["currentItem"].toString();
        owner.livePlaybackSelectedChannel=live::playbackGroupForChannel(state,item,id);
        owner.livePlaybackStem=live::playbackStemForChannel(state,item,id);
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
            const double x = static_cast<double>(item.bounds.getX()) + 1. + (item.lead - item.start + bar) * item.scale;
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
            // Reserve the title's right edge for the editor icon (Instrument).
            const bool instrumentTitle=channel["type"].toString()=="Instrument";
            text(g, label, {indent, y + 4, juce::jmax(0,channelWidth - indent - (instrumentTitle?36:8)), 20});
            if(lane.child || channel["type"].toString()=="Playback")text(g, lane.child ? "Stem" : channel["type"].toString(), {indent, y + 34, channelWidth - indent - 8, 20}, true);
            if (lane.child) {
                g.setColour(tokens.colour(ColourRole::border));
                g.drawVerticalLine(12, static_cast<float>(y), static_cast<float>(y + rowHeight));
                g.drawHorizontalLine(y + 20, 12.f, 19.f);
            }
            line(g, {0, y, channelWidth, rowHeight});
            if (channel["type"].toString()=="Playback" &&
                owner.livePlaybackSelectedChannel==lane.channel) {
                g.setColour(tokens.colour(ColourRole::focus));
                g.fillRect(0,y+2,3,rowHeight-4);
            }
            y += rowHeight;
        }
    }

    void mouseDown(const juce::MouseEvent &event) override {
        dragChannel.clear();
        dragLaneIndex=-1;
        if(event.x>=channelWidth)return;
        const int y=event.y+viewport.getViewPositionY();
        const auto *lane=laneAt(y);
        if(event.mods.isPopupMenu()) {
            if(lane)channelMenu(lane->channel);
            return;
        }
        if(lane && event.mods.isLeftButtonDown()) {
            selectPlaybackChannel(lane->channel);
            if(!owner.app.recording() && event.getNumberOfClicks()==1 &&
               !(lane->child && lane->channel==live::primaryPlaybackId(state))) {
                dragChannel=lane->channel;
                dragLaneIndex=(y-lanesTop)/rowHeight;
            }
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if(dragChannel.isNotEmpty() && e.getDistanceFromDragStart()>5)
            if(auto* container=juce::DragAndDropContainer::findParentDragContainerFor(this))
                if(!container->isDragAndDropActive()) {
                    auto id=dragChannel;dragChannel.clear();
                    const auto sourceLane=dragLaneIndex;dragLaneIndex=-1;
                    const auto image=channelDragPreview(id,sourceLane);
                    if(!image.isNull())
                        // JUCE 8 requires a scale-aware image; a 1x cropped
                        // lane keeps the correct native drag size and target.
                        container->startDragging("mr-live-channel:"+id,this,
                                                 juce::ScaledImage(image,1.0),
                                                 false,nullptr,&e.source);
                }
    }
    void mouseUp(const juce::MouseEvent&) override {dragChannel.clear();dragLaneIndex=-1;}
    bool isInterestedInDragSource(const SourceDetails& s) override {
        return owner.liveDocument && !owner.app.recording() &&
            s.description.toString().startsWith("mr-live-channel:");
    }
    void itemDragMove(const SourceDetails& s) override {
        dragDropY.reset();
        if(isInterestedInDragSource(s))
            if(const auto target=dropTarget(s.localPosition))
                dragDropY=target->boundary;
        repaint();
    }
    void itemDragExit(const SourceDetails&) override {dragDropY.reset();repaint();}
    void itemDropped(const SourceDetails& s) override {
        const auto target=isInterestedInDragSource(s)?dropTarget(s.localPosition):std::nullopt;
        dragDropY.reset();repaint();
        if(!target || !owner.liveDocument)return;
        const auto source=s.description.toString().fromFirstOccurrenceOf("mr-live-channel:",false,false);
        const auto destination=target->id;
        const auto after=target->after;
        const auto generation=owner.liveDocumentGeneration,revision=owner.liveEdits.mutation;
        juce::Component::SafePointer<Desktop> safe(&owner);
        juce::MessageManager::callAsync([safe,generation,revision,source,destination,after] {
            if(safe && safe->liveDocument && safe->liveDocumentGeneration==generation &&
               safe->liveEdits.mutation==revision)
                safe->run([&]{safe->reorderLiveChannel(source,destination,after);});
        });
    }
    void paintOverChildren(juce::Graphics& g) override {
        if(!dragDropY)return;
        const int y=*dragDropY-viewport.getViewPositionY();
        if(y<lanesTop-viewport.getViewPositionY() || y>=getHeight())return;
        g.setColour(liveReferenceTokens().colour(ColourRole::focus));
        g.fillRect(2,y-1,juce::jmax(0,channelWidth-4),3);
    }

  private:
    struct ChannelDrop {
        juce::String id;
        bool after{};
        int boundary{};
    };
    std::optional<ChannelDrop> dropTarget(juce::Point<int> p) const {
        if(!getLocalBounds().contains(p) || p.y<0)return {};
        const int y=p.y+viewport.getViewPositionY();
        const auto lane=laneAt(y);
        // The visible primary WAV is a child display of its own root;
        // drops there must not be mistaken for a separate movable channel.
        if(!lane || (lane->child && lane->channel==live::primaryPlaybackId(state)))return {};
        const auto after=(y-lanesTop)%rowHeight>=rowHeight/2;
        const auto index=(y-lanesTop)/rowHeight;
        return ChannelDrop{lane->channel,after,lanesTop+(index+(after?1:0))*rowHeight};
    }
    // Patch bank commands belong to the track header's right-click menu, not
    // the FX / VSTi insert menu. This uses the existing channel-owned model.
    void handleTrackPatchAction(const juce::String& channelId,const juce::var& patches,
                                const juce::String& itemId,int action) {
        if(!owner.liveDocument)return;
        const auto channel=live::find(owner.liveDocument->interfaceState,"channels",channelId);
        if(!channel.isObject() || channel["type"].toString()=="Playback")return;
        const bool stopped=!owner.app.recording() &&
            owner.displayState().playback!=mrs::PlaybackState::playing;
        const auto patchAt=[&](int index)->juce::var {
            const auto* bank=patches.getArray();
            return bank && index>=0 && index<bank->size()?(*bank)[index]:juce::var{};
        };
        juce::Component::SafePointer<Desktop> safe(&owner);
        if(action==7000) {
            if(!stopped || !channel["liveInput"].isObject())return;
            safe->textDialog("Patch name","Live patch",[safe,channelId](juce::String name){
                if(safe && name.trim().isNotEmpty())
                    safe->run([&]{safe->captureLiveChannelPatch(channelId,name.trim(),"blue");});
            });
            return;
        }
        if(action>=20000&&action<30000) {
            const auto patch=patchAt((action-20000)/64);
            const int slot=(action-20000)%64;
            if(!stopped || !patch.isObject() || live::legacyPatch(patch))return;
            const auto patchId=patch["id"].toString();
            if(slot==0)owner.applyLiveChannelSubPatch(channelId,patchId,{});
            else if(const auto* subs=patch["subPatches"].getArray()) {
                if(slot-1<subs->size())
                    owner.applyLiveChannelSubPatch(channelId,patchId,
                        (*subs)[slot-1]["id"].toString());
            }
            return;
        }
        if(action>=30000&&action<31000) {
            const auto patch=patchAt(action-30000);
            if(!stopped || !patch.isObject() || live::legacyPatch(patch))return;
            const auto patchId=patch["id"].toString();
            safe->textDialog("Sub-patch name","Rhythm",
                [safe,channelId,patchId](juce::String name){
                    if(safe && name.trim().isNotEmpty())
                        safe->run([&]{safe->captureLiveChannelSubPatch(channelId,patchId,name.trim(),"violet");});
                });
            return;
        }
        if(action>=6000&&action<7000) {
            const int index=(action-6000)/8,colour=(action-6000)%8;
            const auto patch=patchAt(index);
            if(patch.isObject()&&colour<static_cast<int>(live::patchColours.size()))
                owner.setLiveChannelPatchColour(channelId,patch["id"].toString(),
                    juce::String(live::patchColours[static_cast<std::size_t>(colour)]));
            return;
        }
        if(action<5000 || action>=6000)return;
        const int index=(action-5000)/8,operation=(action-5000)%8;
        const auto patch=patchAt(index);
        if(!patch.isObject())return;
        const auto patchId=patch["id"].toString();
        if(operation==0) {
            if(stopped && !live::legacyPatch(patch))owner.applyLiveChannelPatch(channelId,patchId);
        } else if(operation==1)owner.assignLiveItemPatch(channelId,patchId);
        else if(operation==2 && itemId.isNotEmpty())owner.assignLiveItemPatch(channelId,{});
        else if(operation==3) {
            safe->textDialog("Patch name",patch["name"].toString(),
                [safe,channelId,patchId](juce::String name){
                    if(safe && name.trim().isNotEmpty())
                        safe->run([&]{safe->renameLiveChannelPatch(channelId,patchId,name.trim());});
                });
        } else if(operation==4)owner.deleteLiveChannelPatch(channelId,patchId);
        else if(operation==5 && !live::legacyPatch(patch))owner.addLivePatchBlock(channelId,patchId);
        else if(operation==6 && !live::legacyPatch(patch))owner.removeLivePatchBlock(channelId,patchId);
    }
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
            if (const auto *channels=state["channels"].getArray())
                for(const auto &channel:*channels)
                    used=used || channel["playbackParent"].toString()==id;
            menu.addItem(11, "Remove empty channel", !used);
        }
        const auto channel=live::find(state,"channels",id);
        const auto patches=channel["patches"]; // no deep copy of VST3 state on menu open
        const auto currentItem=state["currentItem"].toString();
        if(channel.isObject() && channel["type"].toString()!="Playback") {
            const auto track=live::channelItemTrack(state,currentItem,id);
            const auto defaultPatch=track["patchId"].toString();
            const auto currentSub=track["subPatchId"].toString();
            const bool stopped=!owner.app.recording() &&
                owner.displayState().playback!=mrs::PlaybackState::playing;
            const bool canApply=stopped && owner.liveInputTrack(id).has_value() &&
                channel["liveInput"].isObject();
            juce::PopupMenu bankMenu;
            if(const auto* bank=patches.getArray())
                for(int i=0;i<bank->size();++i) {
                    const auto patch=(*bank)[i];
                    const auto patchId=patch["id"].toString();
                    const bool legacy=live::legacyPatch(patch);
                    juce::PopupMenu entry;
                    entry.addItem(5000+i*8,"Apply now",canApply&&!legacy,defaultPatch==patchId);
                    juce::PopupMenu subs;
                    subs.addItem(20000+i*64,"Base",canApply&&!legacy,
                                 defaultPatch==patchId&&currentSub.isEmpty());
                    if(const auto* list=patch["subPatches"].getArray())
                        for(int j=0;j<list->size()&&j<31;++j) {
                            const auto sub=(*list)[j];
                            subs.addItem(20000+i*64+j+1,sub["name"].toString(),canApply&&!legacy,
                                         defaultPatch==patchId&&currentSub==sub["id"].toString());
                        }
                    entry.addSubMenu("Sub-patches",subs);
                    if(!legacy)entry.addItem(30000+i,"Capture current as sub-patch…",canApply);
                    entry.addItem(5005+i*8,"Add block to current item",!legacy);
                    entry.addItem(5006+i*8,"Remove block from current item",!legacy);
                    entry.addItem(5001+i*8,"Set as item default",true,defaultPatch==patchId);
                    if(defaultPatch==patchId)entry.addItem(5002+i*8,"Clear item default");
                    entry.addItem(5003+i*8,"Rename…");
                    juce::PopupMenu colours;
                    for(int c=0;c<static_cast<int>(live::patchColours.size());++c) {
                        const auto colour=juce::String(live::patchColours[static_cast<std::size_t>(c)]);
                        colours.addItem(6000+i*8+c,
                            colour.substring(0,1).toUpperCase()+colour.substring(1),true,
                            patch["color"].toString()==colour);
                    }
                    entry.addSubMenu("Colour",colours);
                    entry.addItem(5004+i*8,"Delete…");
                    bankMenu.addSubMenu(patch["name"].toString()+(legacy?" (legacy)":""),entry);
                }
            if(bankMenu.getNumItems()>0)bankMenu.addSeparator();
            bankMenu.addItem(7000,"Capture current chain as patch…",canApply);
            menu.addSeparator();
            menu.addSubMenu("Patches",bankMenu);
        }
        const auto generation=owner.liveDocumentGeneration;
        const auto mutation=owner.liveEdits.mutation;
        juce::Component::SafePointer<Desktop> safe(&owner);
        juce::Component::SafePointer<LiveTimeline> self(this);
        menu.showMenuAsync(liveMenuOptions(*this),[safe,self,id,types,patches,currentItem,generation,mutation](int result){
            if(!safe || !result || !safe->liveDocument || safe->liveDocumentGeneration!=generation)return;
            if(result>=5000) {
                if(!self || safe->liveEdits.mutation!=mutation ||
                   safe->liveDocument->interfaceState["currentItem"].toString()!=currentItem)return;
                safe->run([&]{self->handleTrackPatchAction(id,patches,currentItem,result);});
                return;
            }
            if (result == 10) {
                const auto channel = live::find(safe->liveDocument->interfaceState, "channels", id);
                safe->textDialog("Channel name", channel["name"].toString(), [safe, id](auto name) {
                    if (safe && name.trim().isNotEmpty()) safe->run([&] { safe->renameLiveObject("channels",id,name); });
                }); return;
            }
            safe->run([&] {
                juce::String added;
                safe->editLiveInterface(result == 11 ? "Remove channel" : "Add channel", [&](auto &model) {
                    if (result >= 1 && result <= types.size()) {
                        added=live::addChannel(model,types[result-1],types[result-1]);
                        if(types[result-1]=="Playback") {
                            auto created=live::find(model,"channels",added);
                            live::put(created,"playbackIndependent",true);
                        }
                        return;
                    }
                    if (result != 11) return;
                    auto *channels = live::property(model, "channels").getArray();
                    for (int i = channels->size(); --i >= 0;)
                        if ((*channels)[i]["id"].toString() == id) channels->remove(i);
                    if(model["performance"]["primaryPlaybackChannel"].toString()==id)
                        live::performance(model).getDynamicObject()->removeProperty("primaryPlaybackChannel");
                    for (auto &item : *live::property(model, "items").getArray())
                        if (auto *tracks = live::property(item, "tracks").getArray())
                            for (int i = tracks->size(); --i >= 0;)
                                if ((*tracks)[i]["channelId"].toString() == id) tracks->remove(i);
                });
                if(added.isNotEmpty()) {
                    const auto created=live::find(safe->liveDocument->interfaceState,"channels",added);
                    if(created["type"].toString()=="Playback") {
                        safe->livePlaybackSelectedChannel=added;
                        safe->livePlaybackStem.clear();
                    }
                }
            });
        });
    }
    class ZoomGlyph final:public juce::Button {
      public:
        explicit ZoomGlyph(bool positive):juce::Button(positive?"Zoom in":"Zoom out"),plus(positive){}
        void paintButton(juce::Graphics& g,bool hover,bool down)override{
            g.setColour(liveReferenceTokens().colour(hover||down||hasKeyboardFocus(true)?ColourRole::focus:ColourRole::text).withMultipliedAlpha(isEnabled()?1.f:.35f));
            const auto c=getLocalBounds().toFloat().getCentre();
            g.drawLine(c.x-4.f,c.y,c.x+4.f,c.y,1.4f);
            if(plus)g.drawLine(c.x,c.y-4.f,c.x,c.y+4.f,1.4f);
            if(hasKeyboardFocus(true))g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.f),2.f,1.f);
        }
      private:bool plus;
    };
    ZoomGlyph zoomOut{false},zoomIn{true};
    struct Item {
        juce::var value;
        juce::Rectangle<int> bounds;
        double scale{}, lead{}, bars{}, start{};
        juce::String entry;
        bool collapsed{};
    };
    bool currentEntry(const Item &item) const {
        if (item.value["id"] != state["currentItem"])
            return false;
        return owner.liveCurrentEntry.isEmpty() || item.entry == owner.liveCurrentEntry;
    }
    // The compact no-frame piano glyph shares the MR UDS track_piano_compact
    // motif without creating another SVG asset or altering vendor-owned GUI.
    class InstrumentEditorIcon final : public juce::Button {
      public:
        InstrumentEditorIcon():juce::Button("Open VST3 Instrument editor") {
            setTitle("Open instrument editor");
            setWantsKeyboardFocus(true);
        }
        void paintButton(juce::Graphics& g,bool hovered,bool down) override {
            const auto tokens=liveReferenceTokens();
            const auto color=tokens.colour(down||hovered?ColourRole::focus:ColourRole::text)
                                  .withMultipliedAlpha(isEnabled()?1.f:.35f);
            g.setColour(color);
            const auto bounds=getLocalBounds().toFloat().reduced(4.f,5.f);
            g.drawRoundedRectangle(bounds,1.4f,1.2f);
            for(int i=1;i<5;++i) {
                const auto x=bounds.getX()+bounds.getWidth()*static_cast<float>(i)/5.f;
                g.drawVerticalLine(juce::roundToInt(x),bounds.getY()+1.f,bounds.getBottom()-1.f);
            }
            for(int i : {1,2,4}) {
                const auto x=bounds.getX()+bounds.getWidth()*static_cast<float>(i)/5.f;
                g.fillRect(x-1.f,bounds.getY()+1.f,2.f,bounds.getHeight()*.52f);
            }
            if(hasKeyboardFocus(true)) {
                g.setColour(tokens.colour(ColourRole::focus));
                g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.75f),2.f,1.f);
            }
        }
    };
    struct EditorGlyph {
        juce::String channel;
        std::unique_ptr<InstrumentEditorIcon> button;
    };
    class StemMix final : public juce::Component {
      public:
        StemMix(LiveTimeline &timeline, juce::String stem) : key(std::move(stem)), parent(timeline), inputs(timeline.owner,key,LiveChannelControls::Surface::timeline) {
            addAndMakeVisible(inputs);addAndMakeVisible(instrumentSelector);
            instrumentSelector.setComponentID("live-timeline-instrument-" + key);
            instrumentSelector.onClick=[this]{inputs.showInstrumentPicker(instrumentSelector);};
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
        void mouseDown(const juce::MouseEvent &event) override {
            if(event.mods.isLeftButtonDown())parent.selectPlaybackChannel(key);
        }
        void resized() override {
            inputs.setBounds(2,0,juce::jmax(0,getWidth()-4),44);
            // Timeline Instrument channels use only the first row of input
            // selectors; place VSTi choice in the free second row.
            instrumentSelector.setBounds(2,22,juce::jmax(0,getWidth()-4),22);
            mute.setBounds(8, 46, 26, 22);solo.setBounds(36, 46, 26, 22);
            arm.setBounds(64, 46, 26, 22);
            gain.setBounds(6, 72, juce::jmax(0, getWidth() - 12), 24);
        }
        void sync(const juce::var &row) {
            const bool live=row["type"].toString()!="Playback";
            arm.setVisible(live);inputs.setVisible(live);inputs.sync();
            const auto profile=row["liveInput"];
            const bool instrument=row["type"].toString()=="Instrument" &&
                (!profile.isObject() || static_cast<int>(profile["version"])==2);
            instrumentSelector.setVisible(instrument);
            const auto name=profile.isObject() && profile["inserts"].size()>0
                ?profile["inserts"][0]["name"].toString():juce::String();
            instrumentSelector.setButtonText(name.isEmpty()?"Choose VSTi":name);
            instrumentSelector.setTooltip(name.isEmpty()
                ?"Select a VST3 instrument for this channel (shared catalog)"
                :"Replace VST3 instrument: "+name);
            instrumentSelector.setEnabled(instrument && profile.isObject() &&
                !parent.owner.app.recording() &&
                parent.owner.displayState().playback!=mrs::PlaybackState::playing);
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
        LiveChannelControls inputs;
        juce::TextButton mute{"M"}, solo{"S"}, arm{"R"};
        juce::TextButton instrumentSelector{"Choose VSTi"};
        juce::Slider gain;
    };
    class Viewport final : public juce::Viewport {
      public:
        explicit Viewport(LiveTimeline &host) : parent(host) {}
        void visibleAreaChanged(const juce::Rectangle<int>& area) override {
            if(lastX>=0 && lastX!=area.getX() &&
               parent.followEnabled && !parent.followProgrammaticScroll &&
               parent.owner.displayState().playback==mrs::PlaybackState::playing)
                parent.followSuspended=true; // User scrollbar/wheel navigation wins.
            lastX=area.getX();
            parent.syncChannel();
            parent.repaint();
        }

      private:
        LiveTimeline &parent;
        int lastX{-1};
    };
    class Body final : public juce::Component, public juce::SettableTooltipClient {
      public:
        explicit Body(LiveTimeline &host) : parent(host) { setComponentID("live-timeline-items"); setWantsKeyboardFocus(true); }
        struct PatchGesture {
            enum class Mode { none, body, left, right, subBody, subLeft, subRight };
            Mode mode{Mode::none};
            juce::String itemId, channelId, patchId, subId;
            juce::var originSpans, previewSpans;
            int startX{}, rowTop{};
            double originStart{}, originLength{}, lastPreviewStart{}, lastPreviewLength{};
            bool previewCalculated{};
        };
        PatchGesture patchGesture;
        juce::String hoveredPatchId, hoveredSubId;
        PatchGesture::Mode hoveredPatchMode{PatchGesture::Mode::none};
        // Popup menus are opened on mouse-up: showing them from mouse-down let the same click
        // dismiss the menu immediately, so right-click appeared to do nothing.
        bool popupPending{};
        juce::Point<int> popupPosition, popupScreen;
        void paint(juce::Graphics &g) override {
            parent.paintItems(g);
            paintPatchPreview(g);
            if(guideY>=0){g.setColour(liveReferenceTokens().colour(ColourRole::focus));g.drawVerticalLine(juce::roundToInt(guideX),static_cast<float>(guideY+4),static_cast<float>(guideY+rowHeight-4));}
        }
        void mouseMove(const juce::MouseEvent &event) override {
            guideY=-1;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            if(patchGesture.mode==PatchGesture::Mode::none&&event.y>=lanesTop) {
                PatchGesture updateTarget;
                if(patchUpdateHit(event.getPosition(),updateTarget)) {
                    setMouseCursor(juce::MouseCursor::PointingHandCursor);
                    setTooltip(updateTarget.subId.isEmpty()?
                        "Update this Patch from the current channel sound":
                        "Update this Sub-patch parameter set");
                    return;
                }
                PatchGesture hover;
                if(patchHitTest(event.getPosition(),hover)) {
                    if(hoveredPatchId!=hover.patchId || hoveredSubId!=hover.subId ||
                       hoveredPatchMode!=hover.mode) {
                        hoveredPatchId=hover.patchId;hoveredSubId=hover.subId;hoveredPatchMode=hover.mode;
                        repaint();
                    }
                    const bool edge=hover.mode==PatchGesture::Mode::left||hover.mode==PatchGesture::Mode::right||
                                    hover.mode==PatchGesture::Mode::subLeft||hover.mode==PatchGesture::Mode::subRight;
                    if(!edge) {
                        // Do not advertise a patch drag while the Item's
                        // independent right-end trim has priority.
                        for(const auto& item:parent.items)
                            if(!item.collapsed && item.bounds.contains(event.getPosition()) &&
                               (event.x>=item.bounds.getRight()-5 ||
                                std::abs(event.position.x-(item.bounds.getX()+1+item.lead*item.scale))<=5)) {
                                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                                setTooltip("Drag origin or Item end");
                                return;
                            }
                    }
                    setMouseCursor(edge?juce::MouseCursor::LeftRightResizeCursor:juce::MouseCursor::DraggingHandCursor);
                    setTooltip(edge?"Drag upper corner to resize block":"Drag patch block");
                    return;
                }
            }
            if(hoveredPatchMode!=PatchGesture::Mode::none) {
                hoveredPatchId.clear();hoveredSubId.clear();hoveredPatchMode=PatchGesture::Mode::none;
                repaint();
            }
            if(hoverClip(event)){repaint();return;}
            repaint();
            setTooltip("Double-click Sections or Chord Track to add; right-click for properties");
            for (const auto &item : parent.items) if ((!item.collapsed || event.y<headerHeight) && item.bounds.contains(event.getPosition())) {
                if(event.y<headerHeight && !item.collapsed &&
                   (event.x<item.bounds.getX()+6||event.x>=item.bounds.getRight()-5)) {
                    const bool right=!item.collapsed && event.x>=item.bounds.getRight()-5;
                    setMouseCursor(noteTrimCursor(right));setTooltip(right?"Trim Item end":"Trim Item start");return;
                }
                if(event.y<30 && event.x>=item.bounds.getRight()-48 && event.x<item.bounds.getRight()-26){
                    setMouseCursor(juce::MouseCursor::PointingHandCursor);
                    setTooltip(live::performanceModeLabel(live::itemPerformanceMode(parent.state,item.value["id"].toString())));return;
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
            if(event.mods.isLeftButtonDown()) {
                popupPending=false;
                ++parent.owner.liveSelectionRevision;
                if(event.y>=lanesTop)
                    if(const auto *lane=parent.laneAt(event.y))
                        parent.selectPlaybackChannel(lane->channel);
            }
            if (event.mods.isPopupMenu()) {
                if (beginClipEdit(event)) return;
                if (beginEdit(event)) return;
                // Open at release to avoid the right-button-up dismissing the
                // popup. The old 120 ms timer made the UI feel unresponsive.
                popupPosition=event.getPosition();
                popupScreen=event.getScreenPosition();
                popupPending=true;
                return;
            }
            if (beginClipEdit(event)) return;
            if(event.mods.isLeftButtonDown() && !scissors) {
                PatchGesture updateTarget;
                if(patchUpdateHit(event.getPosition(),updateTarget)) {
                    const auto channelId=updateTarget.channelId,patchId=updateTarget.patchId,
                        subId=updateTarget.subId,itemId=updateTarget.itemId;
                    parent.owner.run([&]{
                        parent.owner.confirmLivePatchUpdate(channelId,patchId,subId,itemId);
                    });
                    return;
                }
            }
            // Only the explicit upper-corner patch handle beats the Item end trim.
            // On the remainder of the Item boundary, preserve the existing Item
            // resize gesture (#31 smoke: right grip at y=180).
            PatchGesture grip;
            if(patchHitTest(event.getPosition(),grip) &&
               (grip.mode==PatchGesture::Mode::left || grip.mode==PatchGesture::Mode::right ||
                grip.mode==PatchGesture::Mode::subLeft || grip.mode==PatchGesture::Mode::subRight)) {
                if(beginPatchEdit(event))return;
            }
            if (beginEdit(event)) return;
            if (beginPatchEdit(event)) return;
            if (!event.mods.isLeftButtonDown())
                return;
            for (const auto &item : parent.items) {
                if (event.y < headerHeight && event.x >= item.bounds.getRight() - 24 && item.bounds.contains(event.getPosition()) && item.entry.isNotEmpty()) {
                    parent.owner.run([&] { parent.owner.setLivePresentation("collapsedItems",!item.collapsed,item.entry); });
                    return;
                }
            }
            if (event.y >= headerHeight && event.y < headerHeight + rulerHeight) {
                for (const auto &item : parent.items) {
                    if (!item.bounds.contains(event.getPosition()) || item.value["id"] != parent.state["currentItem"] ||
                        !parent.owner.livePlaybackReady())
                        continue;
                    const auto project = parent.owner.project();
                    const auto &meter = project->time.meters.front();
                    const double bar = (event.x - item.bounds.getX() - 1.) / item.scale - item.lead + item.start;
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
            if(patchGesture.mode!=PatchGesture::Mode::none){previewPatchEdit(event);return;}
            if(editing)previewEdit(event);
        }
        void mouseUp(const juce::MouseEvent &) override {
            if(popupPending) {
                popupPending=false;
                if(!patchContextMenu(popupPosition,popupScreen))
                    addPatchBlockMenu(popupPosition,popupScreen);
                return;
            }
            if(patchGesture.mode!=PatchGesture::Mode::none){finishPatchEdit();return;}
            if(editing)finishEdit();
        }
        bool patchHitTest(juce::Point<int> position, PatchGesture &out) const {
            if(position.y<lanesTop)return false;
            for(const auto &item:parent.items) {
                if(item.collapsed||!item.bounds.contains(position))continue;
                const int laneIndex=(position.y-lanesTop)/rowHeight;
                if(laneIndex<0||laneIndex>=static_cast<int>(parent.lanes.size()))continue;
                const auto &lane=parent.lanes[static_cast<std::size_t>(laneIndex)];
                if(lane.container)continue;
                const auto channel=live::find(parent.state,"channels",lane.channel);
                if(!channel.isObject()||channel["type"].toString()=="Playback")continue;
                const auto track=live::channelItemTrack(parent.state,item.value["id"].toString(),lane.channel);
                if(!track.isObject())continue;
                const auto *order=track["patchOrder"].getArray();
                if(order==nullptr)continue;
                const int y=lanesTop+laneIndex*rowHeight;
                for(const auto &patchIdVar:*order) {
                    const auto patchId=patchIdVar.toString();
                    const auto span=track["spans"][juce::Identifier(patchId)];
                    if(!span.isObject())continue;
                    if(const auto *subs=span["subs"].getDynamicObject())
                        for(const auto &pair:subs->getProperties()) {
                            if(!pair.value.isObject())continue;
                            const auto rect=parent.extent(item,pair.value,y+64,26,live::blockStart(span));
                            if(!rect.contains(position))continue;
                            const bool upperGrip=position.y<rect.getY()+8;
                            out.mode=upperGrip&&position.x<=rect.getX()+8?PatchGesture::Mode::subLeft:
                                     upperGrip&&position.x>=rect.getRight()-8?PatchGesture::Mode::subRight:
                                     PatchGesture::Mode::subBody;
                            out.itemId=item.value["id"].toString();out.channelId=lane.channel;
                            out.patchId=patchId;out.subId=pair.name.toString();
                            out.originSpans=track["spans"];
                            out.originStart=live::blockStart(pair.value);
                            out.originLength=live::blockLength(pair.value);
                            out.startX=position.x;out.rowTop=y;
                            return true;
                        }
                    const auto rect=parent.extent(item,span,y+10,rowHeight-20);
                    if(!rect.contains(position))continue;
                    const bool upperGrip=position.y<rect.getY()+8;
                    out.mode=upperGrip&&position.x<=rect.getX()+9?PatchGesture::Mode::left:
                             upperGrip&&position.x>=rect.getRight()-9?PatchGesture::Mode::right:
                             PatchGesture::Mode::body;
                    out.itemId=item.value["id"].toString();out.channelId=lane.channel;
                    out.patchId=patchId;out.subId={};
                    out.originSpans=track["spans"];
                    out.originStart=live::blockStart(span);
                    out.originLength=live::blockLength(span);
                    out.startX=position.x;out.rowTop=y;
                    return true;
                }
            }
            return false;
        }
        bool patchUpdateHit(juce::Point<int> position,PatchGesture& out) const {
            if(!patchHitTest(position,out))return false;
            const auto item=std::find_if(parent.items.begin(),parent.items.end(),
                [&](const auto& candidate){return candidate.value["id"].toString()==out.itemId;});
            if(item==parent.items.end())return false;
            const auto span=out.originSpans[juce::Identifier(out.patchId)];
            const auto geometry=out.subId.isEmpty()?span:
                span["subs"][juce::Identifier(out.subId)];
            if(!geometry.isObject())return false;
            const auto bounds=out.subId.isEmpty()?
                parent.extent(*item,geometry,out.rowTop+10,rowHeight-20):
                parent.extent(*item,geometry,out.rowTop+64,26,live::blockStart(span));
            return parent.patchUpdateBounds(bounds).contains(position);
        }
        bool beginPatchEdit(const juce::MouseEvent &event) {
            if(!event.mods.isLeftButtonDown())return false;
            PatchGesture hit;
            if(!patchHitTest(event.getPosition(),hit))return false;
            // Only a real edit needs its own snapshot. Hover must not deep-copy
            // patch spans (the old path did so on every pointer movement).
            hit.originSpans=hit.originSpans.clone();
            patchGesture=std::move(hit);
            repaint();
            return true;
        }
        void previewPatchEdit(const juce::MouseEvent &event) {
            auto &g=patchGesture;
            if(g.mode==PatchGesture::Mode::none)return;
            const auto item=std::find_if(parent.items.begin(),parent.items.end(),
                [&](const auto &candidate){return candidate.value["id"].toString()==g.itemId;});
            if(item==parent.items.end())return;
            const double dx=live::barSnap((event.x-g.startX)/juce::jmax(1.e-9,item->scale));
            double start=g.originStart,length=g.originLength;
            bool move=false,adjust=false;
            switch(g.mode) {
                case PatchGesture::Mode::body: move=true;start+=dx;break;
                case PatchGesture::Mode::left: start+=dx;length-=dx;adjust=true;break;
                case PatchGesture::Mode::right: length+=dx;adjust=true;break;
                case PatchGesture::Mode::subBody: move=true;start+=dx;break;
                case PatchGesture::Mode::subLeft: start+=dx;length-=dx;break;
                case PatchGesture::Mode::subRight: length+=dx;break;
                default: return;
            }
            if(g.previewCalculated && start==g.lastPreviewStart && length==g.lastPreviewLength)
                return; // One preview per grid step, not per mouse event.
            g.previewCalculated=true;g.lastPreviewStart=start;g.lastPreviewLength=length;
            if(g.subId.isEmpty())
                g.previewSpans=live::editedPatchSpans(g.originSpans,item->bars,g.patchId,start,length,adjust,move);
            else
                g.previewSpans=live::editedSubPatchSpan(g.originSpans,g.patchId,g.subId,start,length,move);
            repaint();
        }
        void finishPatchEdit() {
            if(patchGesture.mode==PatchGesture::Mode::none)return;
            const auto spans=patchGesture.previewSpans;
            const bool changed=spans.isObject()&&!live::sameShowValue(spans,patchGesture.originSpans);
            const auto channelId=patchGesture.channelId,patchId=patchGesture.patchId;
            const auto itemId=patchGesture.itemId;
            const bool sub=patchGesture.subId.isNotEmpty();
            const bool bodyMove=patchGesture.mode==PatchGesture::Mode::body||
                                patchGesture.mode==PatchGesture::Mode::subBody;
            patchGesture={};
            if(changed)
                parent.owner.run([&]{
                    parent.owner.setLivePatchSpans(channelId,patchId,spans,
                        sub?(bodyMove?"Move sub-patch strip":"Resize sub-patch strip")
                           :(bodyMove?"Move patch block":"Resize patch block"),itemId);
                });
            repaint();
        }
        void cancelPatchEdit() {
            if(patchGesture.mode==PatchGesture::Mode::none)return;
            patchGesture={};
            repaint();
        }
        void paintPatchPreview(juce::Graphics &g) {
            if(patchGesture.mode==PatchGesture::Mode::none||!patchGesture.previewSpans.isObject())return;
            for(const auto &item:parent.items)
                if(item.value["id"].toString()==patchGesture.itemId) {
                    const auto span=patchGesture.previewSpans[juce::Identifier(patchGesture.patchId)];
                    if(!span.isObject())return;
                    juce::Rectangle<int> rect;
                    if(patchGesture.subId.isEmpty())
                        rect=parent.extent(item,span,patchGesture.rowTop+10,rowHeight-20);
                    else {
                        const auto sub=span["subs"][juce::Identifier(patchGesture.subId)];
                        if(!sub.isObject())return;
                        rect=parent.extent(item,sub,patchGesture.rowTop+64,26,live::blockStart(span));
                    }
                    g.setColour(liveReferenceTokens().colour(ColourRole::focus));
                    g.drawRect(rect,2);
                    return;
                }
        }
        bool patchContextMenu(juce::Point<int> position, juce::Point<int> screen) {
            PatchGesture hit;
            if(!patchHitTest(position,hit))return false;
            const auto channel=live::find(parent.state,"channels",hit.channelId);
            const auto patch=live::find(channel,"patches",hit.patchId);
            const auto span=hit.originSpans[juce::Identifier(hit.patchId)];
            const auto subs=span["subs"];
            juce::PopupMenu menu;menu.setLookAndFeel(&parent.owner.theme);
            juce::PopupMenu replaceMenu;
            std::vector<juce::String> replacementIds;
            if(const auto *bank=channel["patches"].getArray())
                for(const auto &candidate:*bank) {
                    const auto id=candidate["id"].toString();
                    if(id.isEmpty()||id==hit.patchId||live::legacyPatch(candidate))continue;
                    replaceMenu.addItem(2000+static_cast<int>(replacementIds.size()),
                                        candidate["name"].toString());
                    replacementIds.push_back(id);
                }
            if(!replacementIds.empty())menu.addSubMenu("Replace patch with\u2026",replaceMenu);
            menu.addItem(1,"Remove patch block");
            menu.addItem(3,"Update this Patch from current channel sound...");
            if(hit.subId.isNotEmpty())
                menu.addItem(4,"Update this Sub-patch parameters...");
            std::vector<juce::String> subIds;
            if(const auto *list=patch["subPatches"].getArray())
                for(const auto &sub:*list) {
                    const auto subId=sub["id"].toString();
                    const bool present=subs[juce::Identifier(subId)].isObject();
                    menu.addItem(100+static_cast<int>(subIds.size()),
                                 juce::String(present?"Remove strip: ":"Add strip: ")+sub["name"].toString(),
                                 true,present);
                    subIds.push_back(subId);
                }
            juce::Component::SafePointer<Desktop> safe(&parent.owner);
            juce::Component::SafePointer<Body> self(this);
            const auto channelId=hit.channelId,patchId=hit.patchId,itemId=hit.itemId;
            const auto generation=parent.owner.liveDocumentGeneration;
            menu.showMenuAsync(liveMenuOptions(*this).withTargetScreenArea(juce::Rectangle<int>(screen.x,screen.y,1,1)),
                [safe,self,channelId,patchId,itemId,hitSubId=hit.subId,generation,subIds,replacementIds](int action){
                if(!safe||!action||!safe->liveDocument||safe->liveDocumentGeneration!=generation)return;
                safe->run([&]{
                    if(action==1){safe->removeLivePatchBlock(channelId,patchId,itemId);return;}
                    if(action==3 || action==4) {
                        safe->confirmLivePatchUpdate(channelId,patchId,
                            action==4?hitSubId:juce::String(),itemId);
                        return;
                    }
                    if(action>=2000&&action<3000){
                        const auto at=static_cast<std::size_t>(action-2000);
                        if(at<replacementIds.size())
                            safe->replaceLivePatchBlock(channelId,patchId,replacementIds[at],itemId);
                        return;
                    }
                    const auto index=action-100;
                    if(index<0||index>=static_cast<int>(subIds.size()))return;
                    const auto subId=subIds[static_cast<std::size_t>(index)];
                    const auto track=live::channelItemTrack(safe->liveDocument->interfaceState,itemId,channelId);
                    const auto span=track["spans"][juce::Identifier(patchId)];
                    if(span["subs"][juce::Identifier(subId)].isObject())
                        safe->removeLiveSubPatchBlock(channelId,patchId,subId,itemId);
                    else
                        safe->addLiveSubPatchBlock(channelId,patchId,subId,itemId);
                });
                if(self)self->repaint();
            });
            return true;
        }
        // Right-click on an empty spot of a live channel lane inserts a patch block from that
        // channel's patch library; existing blocks keep their own menu.
        bool addPatchBlockMenu(juce::Point<int> position, juce::Point<int> screen) {
            const auto *lane=parent.laneAt(position.y);
            if(lane==nullptr)return false;
            const auto channel=live::find(parent.state,"channels",lane->channel);
            if(!channel.isObject()||channel["type"].toString()=="Playback")return false;
            // The clicked rectangle, not show.currentItem or the playing
            // transport, owns the new block. This works on inactive Items.
            const auto at=std::find_if(parent.items.begin(),parent.items.end(),
                [&](const auto& item){return !item.collapsed&&item.bounds.contains(position);});
            if(at==parent.items.end())return false;
            const auto itemId=at->value["id"].toString();
            const double preferredBar=barAt(*at,static_cast<float>(position.x));
            const auto generation=parent.owner.liveDocumentGeneration;
            // The channel patch bank is an array (same shape the Audio FX channel menu uses),
            // not an id-keyed object.
            const auto *patches=channel["patches"].getArray();
            std::vector<juce::String> ids,names;
            if(patches!=nullptr)
                for(const auto &patch:*patches) {
                    if(!patch.isObject()||live::legacyPatch(patch))continue;
                    const auto id=patch["id"].toString();
                    if(id.isEmpty())continue;
                    ids.push_back(id);
                    names.push_back(patch["name"].toString());
                }
            juce::PopupMenu menu;menu.setLookAndFeel(&parent.owner.theme);
            std::vector<std::size_t> order(ids.size());
            for(std::size_t i=0;i<order.size();++i)order[i]=i;
            std::sort(order.begin(),order.end(),[&](std::size_t a,std::size_t b){
                return names[a].compareIgnoreCase(names[b])<0;});
            menu.addSectionHeader(ids.empty()?"Patches: none captured yet":"Patches");
            if(ids.empty()) {
                menu.addItem(1,"Capture the channel chain first",false,false);
            } else {
                for(std::size_t i=0;i<order.size();++i)
                    menu.addItem(static_cast<int>(i)+1,names[order[i]]);
            }
            menu.addSeparator();
            menu.addItem(9000,"Capture current chain as patch...");
            juce::Component::SafePointer<Desktop> safe(&parent.owner);
            juce::Component::SafePointer<Body> self(this);
            const auto channelId=lane->channel;
            menu.showMenuAsync(liveMenuOptions(*this).withTargetScreenArea(juce::Rectangle<int>(screen.x,screen.y,1,1)),
                [safe,self,channelId,itemId,generation,ids,order,preferredBar](int action){
                    if(!safe||action<=0||!safe->liveDocument||safe->liveDocumentGeneration!=generation)return;
                    if(action==9000) {
                        safe->textDialog("Patch name","Patch",
                            [safe,channelId](juce::String name){
                                if(!safe||name.trim().isEmpty())return;
                                safe->run([&]{safe->captureLiveChannelPatch(channelId,name.trim(),"blue");});
                            });
                        return;
                    }
                    const auto index=static_cast<std::size_t>(action-1);
                    if(index>=order.size())return;
                    const auto patchId=ids[order[index]];
                    safe->run([&]{safe->addLivePatchBlock(channelId,patchId,preferredBar,itemId);});
                    if(self)self->repaint();
                });
            return true;
        }
        bool keyPressed(const juce::KeyPress &key) override {
            if (key == juce::KeyPress::escapeKey) { cancelPatchEdit(); cancelEdit(); return true; }
            if (clipSelection.isNotEmpty()) {
                if(key==juce::KeyPress::deleteKey || key==juce::KeyPress::backspaceKey){removeClip();return true;}
                if(key==juce::KeyPress::returnKey){clipProperties();return true;}
            }
            if (selection.isEmpty()) return false;
            if (key == juce::KeyPress::returnKey) { properties(); return true; }
            if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) { removeSelected(); return true; }
            return false;
        }
        void focusLost(FocusChangeType) override { cancelPatchEdit(); cancelEdit(); }
        void mouseDoubleClick(const juce::MouseEvent &event) override {
            cancelEdit();
            if(event.y>=lanesTop){if(beginClipEdit(event)){cancelEdit();if(!scissors)clipProperties(event.getScreenPosition());}return;}
            for (const auto &item : parent.items) if (!item.collapsed && item.bounds.contains(event.getPosition())) {
                if (event.y < headerHeight + rulerHeight || event.y >= lanesTop) return;
                selectedItem = item.value["id"].toString();
                selectedLane = event.y < lanesTop - chordHeight ? live::MusicalLane::sections : live::MusicalLane::chords;
                const double at = barAt(item,event.position.x);
                selection = hit(item,selectedLane,at);
                if(selection.isEmpty()) create(item,selectedLane,at,"Verse");
                else if(selectedLane==live::MusicalLane::sections && parent.currentEntry(item) && parent.owner.liveTransportReady()) {
                    const auto project=parent.owner.project();const auto& meter=project->time.meters.front();
                    const auto tick=static_cast<mrs::Tick>(std::llround(at*static_cast<double>(4*mrs::ppq*meter.numerator/meter.denominator)));
                    for(const auto& section:project->sections)if(section.start<=tick&&tick<section.end){
                        parent.owner.run([&]{parent.owner.app.musical().seek_section(section.id);});break;
                    }
                }
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
                const double left=item.bounds.getX()+1+(item.lead-item.start+static_cast<double>(row["start"]))*item.scale;
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
                    else if(scissors){setMouseCursor(juce::MouseCursor::CrosshairCursor);guideX=static_cast<float>(item.bounds.getX()+1+(item.lead-item.start+live::clipSnap(barAt(item,event.position.x)))*item.scale);guideY=lanesTop+((event.y-lanesTop)/rowHeight)*rowHeight;}
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
                // Click-to-select/seek must never serialize or clone the whole
                // show (which can contain multi-megabyte VST3 state).
                originalState=parent.state;
                editGeneration=parent.owner.liveDocumentGeneration;
                editRevision=parent.owner.liveUiRevision;editMutation=parent.owner.liveEdits.mutation;
                editingItem=item;dragStart=event.position.x;dragPosition=dragStart;dragScreenX=static_cast<float>(event.getScreenPosition().x);
                const auto screenPoint=event.getScreenPosition().toFloat();
                screenUnits=getLocalPoint(nullptr,screenPoint.translated(1.f,0.f)).x-getLocalPoint(nullptr,screenPoint).x;
                viewKey.clear();copy=false;invalid=false;clipEditing=true;editing=true;repaint();return true;
            }return false;
        }
        void commitClips(const juce::var& next,const juce::String& label){
            parent.owner.run([&]{parent.owner.setLivePlaybackRegions(clipItem,clipStem,next,label);});
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
            const auto generation=parent.owner.liveDocumentGeneration,sourceEpoch=parent.owner.liveSourceCheckEpoch;
            const auto mediaKey=live::playbackMediaKey(parent.state,sourceItem),regionsKey=live::playbackRegionsKey(parent.state,sourceItem);
            showLiveActionMenu(menu,parent.owner,liveContextTarget(screen),[safe,item,index,screen,selected,sourceItem,sourceStem,generation,sourceEpoch,mediaKey,regionsKey](int action){
                if(!safe||!safe->parent.owner.liveDocument||safe->parent.owner.liveDocumentGeneration!=generation||
                   safe->parent.owner.liveSourceCheckEpoch!=sourceEpoch)return;
                const auto& current=safe->parent.owner.liveDocument->interfaceState;
                if(mediaKey!=live::playbackMediaKey(current,sourceItem)||regionsKey!=live::playbackRegionsKey(current,sourceItem))return;
                safe->clipItem=sourceItem;safe->clipStem=sourceStem;safe->clipSelection=selected;safe->clipSourceBars=safe->sourceBars(item,index);
                if(action==1||action==2){safe->scissors=action==2;return;}
                if(action==3)safe->clipProperties(screen);
                if(action==4)safe->removeClip();
                if(action==5){auto row=live::object();safe->commitClips(live::defaultPlaybackRegions(row,safe->clipSourceBars,live::playbackSecondsPerBar(safe->parent.state,sourceItem)),"Restore Playback region");}
            });
        }
        void mouseExit(const juce::MouseEvent&)override{
            guideY=-1;hoveredPatchId.clear();hoveredSubId.clear();
            hoveredPatchMode=PatchGesture::Mode::none;repaint();
        }
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
                const bool musical=event.y>=headerHeight+rulerHeight && event.y<lanesTop;
                const bool trimStart=!item.collapsed && event.y<headerHeight && event.x<item.bounds.getX()+6;
                const bool origin=event.y>=headerHeight && std::abs(event.position.x-(item.bounds.getX()+1+item.lead*item.scale))<=5;
                const bool right=!item.collapsed && event.x>=item.bounds.getRight()-5;
                if(!musical && !origin && !right && !trimStart)return false;
                if(event.mods.isPopupMenu() && musical) {
                    selectedItem=id;selectedLane=event.y<lanesTop-chordHeight?live::MusicalLane::sections:live::MusicalLane::chords;
                    selection=hit(item,selectedLane,barAt(item,event.position.x));
                    context(item,barAt(item,event.position.x),event.getScreenPosition());return true;
                }
                if(!event.mods.isLeftButtonDown())return false;
                grabKeyboardFocus();
                clipEditing=false;
                editingItem=item; dragStart=event.position.x; dragPosition=dragStart;
                dragScreenX=static_cast<float>(event.getScreenPosition().x);
                const auto screenPoint=event.getScreenPosition().toFloat();
                screenUnits=getLocalPoint(nullptr,screenPoint.translated(1.f,0.f)).x-getLocalPoint(nullptr,screenPoint).x;
                viewKey=!musical?(trimStart?"start":right?"length":origin?"lead":""):"";
                if(viewKey.isNotEmpty()) {
                    // Item edges only change displayed geometry during drag.
                    // No complete show clone/JSON serialization on mouse-down.
                    previewViewValue=viewKey=="lead"?item.lead:viewKey=="start"?item.start:item.bars;
                    editGeneration=parent.owner.liveDocumentGeneration;
                    editRevision=parent.owner.liveUiRevision;editMutation=parent.owner.liveEdits.mutation;
                    invalid=false;editing=true;return true;
                }
                // Defer full show copy to a *real drag*. A simple mouse-down
                // on section/chord must stay O(1) with respect to VST3 state.
                originalState=parent.state;
                editGeneration=parent.owner.liveDocumentGeneration;
                editRevision=parent.owner.liveUiRevision;editMutation=parent.owner.liveEdits.mutation;
                clipSelection.clear();
                selectedItem=id;selectedLane=event.y<lanesTop-chordHeight?live::MusicalLane::sections:live::MusicalLane::chords;
                const auto target=live::musicalHit(live::musicalRows(parent.state,id,selectedLane),barAt(item,event.position.x),5./item.scale);
                selection=target.id; mode=target.mode;
                repaint();
                if(selection.isEmpty())return true;
                editRows=live::musicalRows(originalState,id,selectedLane).clone();
                copy=event.mods.isAltDown() && selectedLane==live::MusicalLane::chords && mode==live::SpanGesture::move;
                editing=true;parent.body.repaint();
                return true;
            }
            return false;
        }
        void previewEdit(const juce::MouseEvent &event) {
            // Bounds/scroll can change during origin/end preview. Screen delta
            // converted once at mouse-down remains stable at any UI zoom.
            dragPosition=dragStart+(static_cast<float>(event.getScreenPosition().x)-dragScreenX)*screenUnits;
            const double delta=(dragPosition-dragStart)/editingItem.scale;
            if(viewKey.isNotEmpty()) {
                const double initial=viewKey=="lead"?editingItem.lead:viewKey=="start"?editingItem.start:editingItem.bars;
                const double value=juce::jlimit(viewKey=="lead"||viewKey=="start"?0.:editingItem.start+.25,viewKey=="start"?editingItem.bars-.25:4096.,live::barSnap(initial+delta));
                if(value==previewViewValue)return;
                previewViewValue=value;
                invalid=false;
                parent.layout();parent.playhead.update();repaint();
                return;
            }
            try {
                live::EditPath path;
                juce::var changedRows;
                if(clipEditing) {
                    const auto next=live::editPlaybackRegion(editRows,clipSelection,delta,clipEdge,clipSourceBars);
                    live::validatePlaybackRegions(next);
                    const auto stem=live::playbackStem(originalState,clipItem,clipStem);
                    const auto track=live::playbackTrackId(originalState,clipItem,stem);
                    if(track.isNotEmpty())path={"performance","clips",track};
                    else {
                        path={"performance","nativePlayback",clipItem};
                        if(clipStem.isNotEmpty()){path.emplace_back("stems");path.push_back(live::EditStep::id(clipStem));}
                        path.emplace_back("regions");
                    }
                    changedRows=next;
                } else {
                    const auto next=live::editMusicalSpan(editRows,selection,delta,mode,selectedLane,copy);
                    live::validateMusicalRows(next,selectedLane);
                    path=selectedLane==live::MusicalLane::sections?
                        live::EditPath{"items",live::EditStep::id(selectedItem),"sections"}:
                        live::EditPath{"performance","chords",selectedItem};
                    changedRows=next;
                }
                const auto patch=live::valuePatch(originalState,std::move(path),changedRows);
                invalid=false;parent.state=patch.apply(originalState,true);
                if(clipEditing)parent.layout();
                else for(auto& item:parent.items)if(item.value["id"].toString()==selectedItem)
                    item.value=live::find(parent.state,"items",selectedItem);
                parent.playhead.update();repaint();
            }catch(const std::exception &e){invalid=true;setTooltip(juce::String::fromUTF8(e.what()));repaint();}
        }
        void finishEdit() {
            const bool changed=std::abs(dragPosition-dragStart)>=1.f && !invalid;
            if(viewKey.isNotEmpty()) {
                const double result=previewViewValue;
                const double initial=viewKey=="lead"?editingItem.lead:viewKey=="start"?editingItem.start:editingItem.bars;
                const auto id=editingItem.value["id"].toString();
                const auto key=viewKey;
                editing=false;parent.layout();parent.playhead.update();repaint();
                if(!changed || result==initial || !parent.owner.liveDocument ||
                   parent.owner.liveDocumentGeneration!=editGeneration ||
                   parent.owner.liveUiRevision!=editRevision||parent.owner.liveEdits.mutation!=editMutation)return;
                if(key=="lead")
                    parent.owner.run([&]{parent.owner.editLiveItemView(id,"lead",result);});
                else parent.owner.run([&]{parent.owner.setLiveItemExtent(id,key=="start"?result:editingItem.start,
                                                                        key=="length"?result:editingItem.bars);});
                return;
            }
            // No drag: keep selection/seek, with no show clone or JSON
            // comparison on mouse-up. For a real drag revision guards protect
            // against stale edits while the user interacted.
            editing=false;
            if(!changed) {
                restoreModelState();
                return;
            }
            const auto next=parent.state;
            restoreModelState();
            if(!parent.owner.liveDocument ||
               parent.owner.liveDocumentGeneration!=editGeneration ||
               parent.owner.liveUiRevision!=editRevision||parent.owner.liveEdits.mutation!=editMutation)return;
            parent.owner.run([&]{
                if(clipEditing){
                    parent.owner.setLivePlaybackRegions(clipItem,clipStem,
                        live::playbackRegions(next,clipItem,live::playbackStem(next,clipItem,clipStem)),"Edit Playback region");
                }else parent.owner.setLiveMusicalRows(selectedItem,selectedLane==live::MusicalLane::sections,
                                                      live::musicalRows(next,selectedItem,selectedLane));
            });
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
            restoreModelState();
        }
        void restoreModelState() {
            if(parent.owner.liveDocument)parent.state=parent.owner.liveDocument->interfaceState;
            if(clipEditing||viewKey.isNotEmpty())parent.layout();
            else for(auto& item:parent.items)
                item.value=live::find(parent.state,"items",item.value["id"].toString());
            parent.playhead.update();repaint();
        }
        void properties(juce::Point<int> screen=juce::Desktop::getMousePosition()) {
            if(selection.isNotEmpty())for(const auto &item:parent.items)
                if(item.value["id"].toString()==selectedItem) {
                    const auto row=live::spanById(live::musicalRows(parent.state,selectedItem,selectedLane),selection);
                    context(item,static_cast<double>(row["start"]),screen);return;
                }
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
            menu.addItem(2,"Duplicate",selection.isNotEmpty());menu.addItem(3,"Remove",selection.isNotEmpty());
            if(selectedLane==live::MusicalLane::sections && selection.isNotEmpty()){
                menu.addSeparator();addLivePerformanceModes(menu,parent.state,selection,50);
            }
            menu.addSeparator();
            const juce::StringArray names{"Intro","Verse","Chorus","Bridge","Solo","Outro"};
            if(selectedLane==live::MusicalLane::chords)menu.addItem(10,"Add chord");
            else for(int i=0;i<names.size();++i)menu.addItem(10+i,"Add "+names[i]);
            juce::Component::SafePointer<Body> safe(this);
            const auto targetItem=selectedItem,targetEvent=selection;
            const auto targetLane=selectedLane;
            const auto generation=parent.owner.liveDocumentGeneration;
            auto invoke=[safe,item,at,names,targetItem,targetEvent,targetLane,generation](int result){
                if(!safe||!safe->parent.owner.liveDocument||
                   generation!=safe->parent.owner.liveDocumentGeneration)return;
                safe->parent.owner.run([&]{
                    if(result==3)safe->parent.owner.editLiveInterface("Remove musical event",[&](auto &state){
                        auto rows=live::musicalRows(state,targetItem,targetLane).clone();
                        if(!rows.isArray())throw std::runtime_error("Musical event no longer exists");
                        for(int i=rows.size();--i>=0;)
                            if(rows[i]["id"].toString()==targetEvent)rows.getArray()->remove(i);
                        live::setMusicalRows(state,targetItem,targetLane,rows);
                    });
                    if(result>=50&&result<54)safe->parent.owner.setLivePerformanceMode(targetItem,targetEvent,static_cast<mrs::PerformanceMode>(result-50));
                    if(result>=10&&result<16)safe->create(item,targetLane,at,names[result-10]);
                    if(result==2)safe->parent.owner.editLiveInterface("Duplicate musical event",[&](auto &state){
                        const auto rows=live::musicalRows(state,targetItem,targetLane);
                        const auto source=live::spanById(rows,targetEvent);
                        if(!source.isObject())throw std::runtime_error("Musical event no longer exists");
                        live::setMusicalRows(state,targetItem,targetLane,
                            live::editMusicalSpan(rows,targetEvent,static_cast<double>(source["length"]),
                                                 live::SpanGesture::move,targetLane,true));
                    });
                });
            };
            if(selection.isNotEmpty())
                openLiveMusicalEditor(parent.owner,selectedItem,selectedLane,selection,screen,menu,std::move(invoke));
            else showLiveActionMenu(menu,parent.owner,liveContextTarget(screen),std::move(invoke));
        }
        static double barAt(const Item &item,float x) {return (x-item.bounds.getX()-1.)/item.scale-item.lead+item.start;}
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
        juce::String selectedItem,selection,viewKey;
        double previewViewValue{};
        std::uint64_t editGeneration{},editRevision{},editMutation{};
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
    void updatePlayheadFollow(mrs::PlaybackState playback) {
        if(!followEnabled || followSuspended ||
           playback!=mrs::PlaybackState::playing || !owner.liveTransportReady())return;
        const auto line=playheadBounds();
        if(line.isEmpty())return;
        const int width=viewport.getMaximumVisibleWidth();
        const int current=viewport.getViewPositionX();
        const int maximum=juce::jmax(0,body.getWidth()-width);
        const int next=followScrollX(juce::roundToInt(line.getX()),current,width,maximum,
                                     followEngaged);
        if(next!=current) {
            const juce::ScopedValueSetter<bool> guard(followProgrammaticScroll,true);
            viewport.setViewPosition(next,viewport.getViewPositionY());
        }
        if(juce::roundToInt(line.getX())-next>=width/2 &&
           next==juce::jlimit(0,maximum,juce::roundToInt(line.getX())-width/2))
            followEngaged=true;
        else if(juce::roundToInt(line.getX())<next+width/2)
            followEngaged=false;
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
                double lead = numeric(view["lead"], 0, 0, 4096);
                double start=numeric(view["start"],0,0,4095.75);
                double bars = numeric(view["length"], numeric(item["bars"], 16, 1, 4096), .25, 4096);
                const bool previewThisItem = body.editing && body.viewKey.isNotEmpty() &&
                    item["id"] == body.editingItem.value["id"];
                const bool previewLength = previewThisItem && body.viewKey == "length";
                if(previewThisItem&&body.viewKey=="start")start=body.previewViewValue;
                if (previewThisItem && body.viewKey == "lead") lead = body.previewViewValue;
                // During the gesture, retain the original authored/audio-derived
                // width instead of walking Playback sources on every mouse move.
                if (previewThisItem) bars = previewLength ? body.previewViewValue : body.editingItem.bars;
                double audioBars = 0.;
                if (!view.hasProperty("length") && !previewThisItem && item["id"] == state["currentItem"] && owner.livePlaybackReady()) {
                    const auto project = owner.project();
                    mrs::Sample end = 0;
                    for (const auto &clip : project->clips)
                        end = juce::jmax(end, clip.start + clip.length);
                    const auto &meter = project->time.meters.front();
                    const mrs::Timeline clock(project->time, project->sample_rate);
                    audioBars = static_cast<double>(clock.to_ticks(end)) /
                                                      (4. * mrs::ppq * meter.numerator / meter.denominator);
                }
                if (!view.hasProperty("length") && !previewThisItem) for (const auto &row : live::playbackRows(state, item["id"].toString())) {
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
                if (!view.hasProperty("length") && !previewThisItem)
                    bars=juce::jmin(4096.,juce::jmax(audioBars>0.?audioBars:bars,live::authoredMusicalEndBars(state,item["id"].toString())));
                // The model permits 1000 entries, 4096 bars/lead and 128 px/bar.
                // Keep their complete geometry within a signed-int scroll surface.
                const bool collapsed = static_cast<bool>(state["performance"]["collapsedItems"][juce::Identifier(entry["id"].toString())]);
                const int width = collapsed ? 160 : juce::jlimit(6, 1048578, juce::roundToInt((bars - start + lead) * scale) + 2);
                items.push_back({item,
                                 {x, 0, width, height},
                                 scale,
                                 lead,
                                 bars, start,
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
        return {item.bounds.getX() + 1 + juce::roundToInt((item.lead - item.start + start) * item.scale), y,
                juce::jmax(1, juce::roundToInt(length * item.scale)), height};
    }
    static juce::Rectangle<int> patchUpdateBounds(juce::Rectangle<int> blockBounds) {
        if(blockBounds.getWidth()<52)return {};
        const int top=blockBounds.getY()+
            juce::jmax(3,juce::jmin(8,(blockBounds.getHeight()-16)/2));
        return {blockBounds.getRight()-31,top,16,16};
    }
    static void paintPatchUpdate(juce::Graphics& g,juce::Rectangle<int> blockBounds) {
        const auto button=patchUpdateBounds(blockBounds);
        if(button.isEmpty())return;
        const auto centre=button.getCentre().toFloat();
        g.setColour(juce::Colours::white.withAlpha(.83f));
        juce::Path refresh;
        refresh.addCentredArc(centre.x,centre.y,5.2f,5.2f,0.f,.5f,5.4f,true);
        g.strokePath(refresh,juce::PathStrokeType(1.6f));
        g.drawLine(centre.x+1.7f,centre.y-4.9f,centre.x+5.3f,centre.y-5.9f,1.6f);
        g.drawLine(centre.x+5.3f,centre.y-5.9f,centre.x+5.3f,centre.y-2.2f,1.6f);
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
                     juce::String(item.bars-item.start) + " bars",
                 {x + 10, 30, juce::jmax(0, width - 20), 22}, true);
            performanceArt->draw(g,live::itemPerformanceMode(state,id),{x+width-48,8,18,18});
            text(g, item.collapsed ? ">" : "v", {x + width - 24, 4, 20, 26});
            if (item.collapsed) { line(g, item.bounds); continue; }
            g.setColour(tokens.colour(ColourRole::focus).withAlpha(.7f));
            g.drawVerticalLine(x + 1 + juce::roundToInt(item.lead * item.scale), static_cast<float>(headerHeight), static_cast<float>(item.bounds.getHeight()));
            g.fillRect(x + width - 4, headerHeight, 4, item.bounds.getHeight() - headerHeight);
            const int step = juce::jmax(1, static_cast<int>(std::ceil(item.bars / 16)));
            for (int bar = static_cast<int>(std::floor(item.start-item.lead)); bar < static_cast<int>(std::ceil(item.bars)); ++bar) {
                const int bx = x + 1 + juce::roundToInt((item.lead - item.start + bar) * item.scale);
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
        for (std::size_t i=0;i<rows.size();++i)
            if(live::playbackChannelId(state,rows[i])==channelId)
                return static_cast<int>(i);
        return -1;
    }
    int playbackLaneY() const {
        return channelY(live::primaryPlaybackId(state));
    }
    mrs::Sample sampleAt(const Item &item, double x) const {
        const auto project = owner.project();
        const auto &meter = project->time.meters.front();
        const double bar = juce::jmax(0., (x - item.bounds.getX() - 1.) / item.scale - item.lead + item.start);
        const auto tick = static_cast<mrs::Tick>(
            std::llround(bar * static_cast<double>(4 * mrs::ppq * meter.numerator / meter.denominator)));
        return mrs::Timeline(project->time, project->sample_rate).to_samples(tick);
    }
    void paintPlaybackContainer(juce::Graphics &g, const Item &item, int y) const {
        const auto id = item.value["id"].toString();
        const auto rows = live::playbackRows(state, id);
        const auto primary=live::playbackContainer(state);
        std::vector<std::size_t> own;
        for(std::size_t i=0;i<rows.size();++i)
            if(live::playbackGroupForRow(state,id,rows[i])==primary)
                own.push_back(i);
        if(own.empty()) {
            if (const auto *tracks = item.value["tracks"].getArray()) for (const auto &track : *tracks)
                if (track["channelId"].toString() == primary) {
                    paintTrack(g, item, primaryChannel(), track, y); break;
                }
            return;
        }
        if(own.size()==1){paintAudio(g,item,y,own.front());return;}
        std::vector<live::PlaybackWaveformSource> sources;
        double end = 0.; int ready = 0, unavailable = 0;
        for (const auto i : own) {
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
        const float x = static_cast<float>(item.bounds.getX() + 1. + (item.lead-item.start) * item.scale);
        const float right = static_cast<float>(x + static_cast<double>(clock.to_ticks(static_cast<mrs::Sample>(std::llround(end * 48000)))) / ticksPerBar * item.scale);
        const juce::Rectangle<float> area{x, static_cast<float>(y + 8), (std::max)(1.f, right - x), static_cast<float>(rowHeight - 16)};
        if (ready) block(g, area.getSmallestIntegerContainer(), {}, "blue");
        text(g, "Playback · " + juce::String(static_cast<int>(own.size())) + " stems" +
                     (unavailable ? " · file unavailable" : ready < static_cast<int>(own.size()) ? " · preparing waveforms" : ""),
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
            const juce::Rectangle<float> area{static_cast<float>(item.bounds.getX()+1+(item.lead-item.start+start)*item.scale),static_cast<float>(y+10),
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
        return live::find(state,"channels",live::primaryPlaybackId(state));
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
        if (const auto *entries = channels.getArray()) for (const auto &channel : *entries)
            channelsKey += channel["id"].toString()+"#"+channel["type"].toString()+"|";
        if (channelsKey != stemControlsKey) {
            stemControlsKey = channelsKey;
            stemControls.clear();
            editorGlyphs.clear();
            if (const auto *entries = channels.getArray()) for (const auto &channel : *entries) {
                if (channel["id"] == primaryChannel()["id"]) continue;
                const auto channelId=channel["id"].toString();
                auto control=std::make_unique<StemMix>(*this,channelId);
                addAndMakeVisible(*control);stemControls.push_back(std::move(control));
                if(channel["type"].toString()=="Instrument") {
                    EditorGlyph glyph;
                    glyph.channel=channelId;
                    glyph.button=std::make_unique<InstrumentEditorIcon>();
                    glyph.button->setComponentID("live-timeline-editor-"+channelId);
                    glyph.button->onClick=[this,channelId] {
                        owner.run([&] {
                            if(!owner.liveDocument)return;
                            const auto row=live::find(owner.liveDocument->interfaceState,"channels",channelId);
                            const auto profile=row["liveInput"];
                            if(!profile.isObject() || static_cast<int>(profile["version"])!=2)
                                throw std::runtime_error("Choose a MIDI/VST3 instrument in Mixer first");
                            const auto loaded=live::decodeLiveInputProfile(profile);
                            if(loaded.inserts.empty() || loaded.inserts.front().kind!=mrs::InsertKind::vst3)
                                throw std::runtime_error("Choose a VST3 instrument in Mixer first");
                            owner.openLiveInsertEditor(channelId,loaded.inserts.front().id);
                        });
                    };
                    addAndMakeVisible(*glyph.button);
                    editorGlyphs.push_back(std::move(glyph));
                }
            }
        }
        for(auto& glyph:editorGlyphs) {
            const auto y=channelY(glyph.channel)-viewport.getViewPositionY();
            glyph.button->setBounds(juce::jmax(0,channelWidth-30),y+2,24,22);
            glyph.button->setVisible(y>=lanesTop && y+rowHeight>0 && y<getHeight());
            const auto row=live::find(owner.liveDocument->interfaceState,"channels",glyph.channel);
            const auto profile=row["liveInput"];
            const bool active=profile.isObject() && static_cast<int>(profile["version"])==2
                && profile["inserts"].size()>0;
            glyph.button->setEnabled(active);
            glyph.button->setTooltip(active?
                "Open VST3 instrument editor: "+profile["inserts"][0]["name"].toString():
                "Choose a VST3 instrument from Mixer first");
        }
        for (auto &control : stemControls) {
            const int y = channelY(control->key);
            const int top = y + 26 - viewport.getViewPositionY();
            const auto lane = laneAt(y);
            const int indent = lane && lane->child ? 14 : 0;
            control->setBounds(indent, top, juce::jmax(0, channelWidth - indent), 98);
            control->setVisible(y >= lanesTop && top + 98 > 0 && top < getHeight());
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
                // Match Section/Chord grips: brackets appear only when the upper
                // corner is hovered; Item trim must not steal these gestures.
                if(body.hoveredPatchId==patchId.toString()&&body.hoveredSubId.isEmpty()) {
                    const bool left=body.hoveredPatchMode==Body::PatchGesture::Mode::left;
                    const bool right=body.hoveredPatchMode==Body::PatchGesture::Mode::right;
                    if(left||right) {
                        g.setColour(juce::Colours::white.withAlpha(.85f));
                        const int edge=left?patchBounds.getX()+2:patchBounds.getRight()-3;
                        const int end=edge+(left?7:-7);
                        g.drawLine(static_cast<float>(edge),static_cast<float>(patchBounds.getY()+2),
                                   static_cast<float>(edge),static_cast<float>(patchBounds.getY()+14),2.f);
                        g.drawLine(static_cast<float>(edge),static_cast<float>(patchBounds.getY()+3),
                                   static_cast<float>(end),static_cast<float>(patchBounds.getY()+3),2.f);
                    }
                }
                text(g, patch["name"].toString(), patchBounds.reduced(6).withHeight(30));
                paintPatchUpdate(g,patchBounds);
                if (const auto *subs = patch["subPatches"].getArray())
                    for (const auto &sub : *subs) {
                        const auto geometry = span["subs"][juce::Identifier(sub["id"].toString())];
                        if (geometry.isObject()) {
                            const auto subBounds = extent(item, geometry, y + 64, 26, numeric(span["start"], 0, 0, 4096));
                            block(g, subBounds, sub["name"].toString(),
                                  sub["color"].toString() == "inherit" ? patch["color"] : sub["color"]);
                            paintPatchUpdate(g,subBounds);
                            if(body.hoveredPatchId==patchId.toString() &&
                               body.hoveredSubId==sub["id"].toString()) {
                                const bool left=body.hoveredPatchMode==Body::PatchGesture::Mode::subLeft;
                                const bool right=body.hoveredPatchMode==Body::PatchGesture::Mode::subRight;
                                if(left||right) {
                                    g.setColour(juce::Colours::white.withAlpha(.85f));
                                    const int edge=left?subBounds.getX()+2:subBounds.getRight()-3;
                                    const int end=edge+(left?6:-6);
                                    g.drawLine(static_cast<float>(edge),static_cast<float>(subBounds.getY()+2),
                                               static_cast<float>(edge),static_cast<float>(subBounds.getY()+12),2.f);
                                    g.drawLine(static_cast<float>(edge),static_cast<float>(subBounds.getY()+3),
                                               static_cast<float>(end),static_cast<float>(subBounds.getY()+3),2.f);
                                }
                            }
                        }
                    }
            }
    }
    Desktop &owner;
    juce::String dragChannel;
    int dragLaneIndex{-1};
    std::optional<int> dragDropY;
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
    std::vector<EditorGlyph> editorGlyphs;
    juce::String stemControlsKey;
    juce::var state;
    juce::String signature, revealedItem;
    std::uint64_t observedMixRevision{~std::uint64_t{0}},observedPaintRevision{~std::uint64_t{0}};
    std::vector<Item> items;
    std::string activeSection, activeChord;
    bool followEnabled{}, followEngaged{}, followSuspended{}, followProgrammaticScroll{};
    std::uint64_t focusRevision{};
    int channelWidth{150};
    int observedWaveform{};
    mrs::PlaybackState observedPlayback{mrs::PlaybackState::stopped};
};
} // namespace ui
