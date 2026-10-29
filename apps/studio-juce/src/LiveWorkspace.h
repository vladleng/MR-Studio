#pragma once
#include "Desktop.h"
#include "LiveAudioPanel.h"
#include "LiveInputPilotPanel.h"
#include "LiveChannels.h"
#include "LiveContext.h"
#include "LiveLayout.h"
#include "LiveMixer.h"
#include "LivePlaybackPanel.h"
#include "LivePreflightPanel.h"
#include "LiveSetlist.h"
#include "LiveTheme.h"
#include "LiveTimeline.h"

namespace ui {
class LiveWorkspace final : public juce::Component, private juce::Timer {
  public:
    explicit LiveWorkspace(Desktop &desktop)
        : owner(desktop), setlist(desktop, false, reference.defaults["itemCardLayout"]),
          browserItems(desktop, true, reference.defaults["itemCardLayout"]), timeline(desktop), legacyChannels(desktop),
          playbackPanel(desktop), mixer(desktop) {
        setComponentID("mr-live-workspace");
        setName("MR Live");
        configureLiveContext(nativeTheme, static_cast<float>(owner.view.liveMenuOpacity));
        nativeTheme.useConceptAssets(true);
        nativeTheme.setPopupOpacity(static_cast<float>(owner.view.liveMenuOpacity));
        setLookAndFeel(&nativeTheme);
        addMouseListener(this, true);
        for (auto *component :
             std::array<juce::Component *, 12>{&setlist, &files, &options, &play, &pause, &stop, &record, &previous,
                                               &next, &loop, &undoButton, &redoButton})
            addAndMakeVisible(component);
        addChildComponent(browserItems);
        addAndMakeVisible(timeline);
        addAndMakeVisible(playbackPanel);
        addChildComponent(legacyChannels);
        addChildComponent(mixer);
        addAndMakeVisible(mixerButton);
        mixerButton.setComponentID("live-mixer-toggle");
        for(auto* button:{&pointerTool,&scissorsTool})addAndMakeVisible(button);
        pointerTool.setComponentID("live-pointer-tool");scissorsTool.setComponentID("live-scissors-tool");
        pointerTool.getProperties().set("mrHostIcon","tool_pointer");scissorsTool.getProperties().set("mrHostIcon","tool_split");
        pointerTool.onClick=[this]{timeline.setClipScissors(false);};scissorsTool.onClick=[this]{timeline.setClipScissors(true);};
        mixerButton.onClick = [this] { mixerVisible = !mixerVisible; mixer.setVisible(mixerVisible); resized(); };
        playbackPanel.layoutChanged = [this] { resized(); };
        files.onClick = [this] { filesMenu(); };
        options.onClick = [this] { optionsMenu(); };
        play.onClick = [this] { owner.action(30); };
        pause.onClick = [this] { owner.action(31); };
        stop.onClick = [this] { owner.action(32); };
        for (auto *button : {&play, &pause, &stop, &record, &previous, &next, &loop})
            button->getProperties().set("mrLiveTransport", true);
        tunerPlaceholder = LiveLayout::drawable("live_tuner.svg");
        record.onClick = [this] { owner.action(33); };
        previous.onClick = [this] { owner.action(34); };
        next.onClick = [this] { owner.action(35); };
        loop.onClick = [this] { owner.action(36); };
        undoButton.onClick = [this] { owner.action(10); };
        redoButton.onClick = [this] { owner.action(11); };
        undoButton.getProperties().set("mrHostIcon", "action_undo");
        redoButton.getProperties().set("mrHostIcon", "action_redo");
        const auto guard = [this] { dragStartedAt = juce::Time::getMillisecondCounter(); };
        const auto blocked = [this] {
            return live::clickBlockedAfterDrag(dragStartedAt,juce::Time::getMillisecondCounter());
        };
        setlist.toggleBrowser = [this] { showBrowser(!browserVisible); };
        for (auto *list : {&setlist, &browserItems}) {
            list->dragGuard = guard;
            list->activationBlocked = blocked;
        }
        startTimerHz(15);
    }
    ~LiveWorkspace() override {
        stopTimer();
        removeMouseListener(this);
        setLookAndFeel(nullptr);
    }
    std::function<void()> appearanceChanged, fitWindowRequested;
    void showBrowser(bool visible) {
        browserVisible = visible;
        if (!visible) {
            owner.liveBrowserItem.clear();
            owner.app.stop_library_preview();
        }
        browserItems.setVisible(visible);
        resized();
        if (visible)
            browserItems.toFront(false);
    }
    bool itemsBrowserVisible() const { return browserVisible; }
    void resized() override {
        auto playbackBounds = reference.bounds("playback.stems", getWidth(), getHeight());
        playbackBounds.setY(playbackBounds.getBottom() - playbackPanel.preferredHeight());
        playbackBounds.setHeight(playbackPanel.preferredHeight());
        playbackPanel.setBounds(playbackBounds);
        const auto size = getLocalBounds();
        const auto toolbar = reference.bounds("control.toolbar", size.getWidth(), size.getHeight());
        auto controls = toolbar.reduced(6).withHeight(32);
        files.setBounds(controls.removeFromLeft(44).reduced(1));
        options.setBounds(controls.removeFromLeft(52).reduced(1));
        undoButton.setBounds(controls.removeFromLeft(26));
        redoButton.setBounds(controls.removeFromLeft(26));
        mixerButton.setBounds(controls.removeFromLeft(58));
        pointerTool.setBounds(controls.removeFromLeft(28));scissorsTool.setBounds(controls.removeFromLeft(28));
        auto setlistBounds = reference.bounds("show.setlist", getWidth(), getHeight());
        setlistBounds.setBottom(playbackPanel.getY());
        setlist.setBounds(setlistBounds);
        const auto channelBounds = reference.bounds("track.channels", getWidth(), getHeight());
        const auto timelineBounds = reference.bounds("song.timeline", getWidth(), getHeight());
        legacyChannels.setBounds(channelBounds);
        const int dockHeight = juce::jmin(340, timelineBounds.getHeight() / 2);
        auto timelineArea = showLegacyChannels ? timelineBounds : channelBounds.getUnion(timelineBounds);
        if (mixerVisible) timelineArea.setBottom(timelineArea.getBottom() - dockHeight);
        timeline.setBounds(timelineArea);
        timeline.setChannelWidth(showLegacyChannels ? 0 : timelineBounds.getX() - channelBounds.getX());
        const auto header = reference.bounds("control.header", getWidth(), getHeight());
        auto transport = juce::Rectangle<int>(header.getCentreX() - 178, header.getCentreY() - 16, 224, 32);
        for (auto *button : {&previous, &play, &pause, &stop, &next, &loop, &record})
            button->setBounds(transport.removeFromLeft(32).reduced(2));
        const int width = juce::jmin(380, juce::jmax(220, getWidth() - setlist.getRight() - 16));
        browserItems.setBounds(setlist.getRight() + 2, setlist.getY(), width, setlist.getHeight());
        if (browserVisible) browserItems.toFront(false);
        mixer.setBounds(timelineBounds.getX(), timelineBounds.getBottom() - dockHeight,
                        timelineBounds.getWidth(), dockHeight);
        if (mixerVisible) mixer.toFront(false);
    }
    void mouseDown(const juce::MouseEvent &event) override {
        if (!event.mods.isLeftButtonDown())
            return;
        auto *origin = event.originalComponent;
        // Playback/Stems, popup controls and transport must not close Browser.
        // Only an intentional Timeline click dismisses this inspection view.
        const bool inTimeline = origin == &timeline || timeline.isParentOf(origin);
        if (browserVisible && inTimeline)
            showBrowser(false);
    }
    void paint(juce::Graphics &g) override {
        const auto tokens = liveReferenceTokens();
        g.fillAll(tokens.colour(ColourRole::canvas));
        const auto fill = [&](const char *type) {
            const auto bounds = reference.bounds(type, getWidth(), getHeight());
            g.setColour(tokens.colour(ColourRole::panel));
            g.fillRect(bounds.reduced(1));
        };
        for (const auto *type :
             {"control.toolbar", "control.header", "song.timeline", "playback.stems", "system.status"})
            fill(type);
        const auto header = reference.bounds("control.header", getWidth(), getHeight());
        g.setColour(tokens.colour(ColourRole::text));
        g.setFont(owner.theme.font(12));
        const auto title = owner.projectCaption();
        g.drawText(title, header.getX() + 10, header.getY() + 4,
                   juce::jmax(0, header.getCentreX() - 190 - header.getX()), header.getHeight() - 8,
                   juce::Justification::centredLeft, true);

        const auto head = owner.displayState();
        g.setColour(tokens.colour(ColourRole::text));
        g.setFont(owner.theme.font(14));
        g.drawText(juce::String(static_cast<double>(head.sample) / owner.project()->sample_rate, 2) + " s",
                   header.getCentreX() + 54, header.getCentreY() - 20, 120, 24, juce::Justification::left);
        g.setFont(owner.theme.font(9));
        g.setColour(tokens.colour(ColourRole::muted));
        g.drawText(head.playback == mrs::PlaybackState::playing  ? "PLAYING"
                   : head.playback == mrs::PlaybackState::paused ? "PAUSED"
                                                                 : "STOPPED",
                   header.getCentreX() + 54, header.getCentreY() + 4, 120, 18, juce::Justification::left);
        if (tunerPlaceholder) {
            const float tunerHeight = juce::jmin(64.f, static_cast<float>(header.getHeight()));
            const float tunerWidth = 264.f * tunerHeight / 64.f;
            tunerPlaceholder->drawWithin(g,
                                         {static_cast<float>(header.getRight()) - tunerWidth - 4.f,
                                          static_cast<float>(header.getCentreY()) - tunerHeight / 2.f, tunerWidth,
                                          tunerHeight},
                                         juce::RectanglePlacement::centred, 1.f);
        }
        g.setColour(tokens.colour(ColourRole::text));
        g.setFont(owner.theme.font(12));
        auto statusArea = reference.bounds("system.status", getWidth(), getHeight()).reduced(10);
        g.setColour(owner.message.isEmpty() ? tokens.colour(ColourRole::muted) : tokens.colour(ColourRole::warning));
        g.setFont(owner.theme.font(11));
        auto statusText = owner.footerStatus();
        if (owner.livePendingItem.isNotEmpty())
            statusText +=
                "\nPreparing item: " +
                live::find(owner.liveDocument->interfaceState, "items", owner.livePendingItem)["name"].toString();
        else if (owner.livePreloadError.isNotEmpty())
            statusText += "\n" + owner.livePreloadError;
        else if (owner.liveNextStatus.isNotEmpty())
            statusText += "\n" + owner.liveNextStatus;
        if (owner.livePlaybackReady()) {
            const auto &context = owner.app.musical().state();
            const auto project = owner.project();
            const mrs::Timeline clock(project->time, project->sample_rate);
            const auto pos = clock.musical_position(clock.to_ticks(head.sample));
            const double elapsed = static_cast<double>(head.sample) / project->sample_rate;
            mrs::Sample end = 0;
            for (const auto &clip : project->clips)
                end = juce::jmax(end, clip.start + clip.length);
            const double total = static_cast<double>(end) / project->sample_rate;
            statusText += "\nBar " + juce::String(pos.bar) + ":" + juce::String(pos.beat) + ":" +
                          juce::String(pos.tick) + "  |  " + juce::String(elapsed, 1) + " s  |  remaining " +
                          juce::String(juce::jmax(0., total - elapsed), 1) + " s";
            if (context.current_section)
                statusText += "\n" + juce::String::fromUTF8(context.current_section->name.c_str());
            if (context.next_section)
                statusText += " -> " + juce::String::fromUTF8(context.next_section->name.c_str());
            if (context.current_chord)
                statusText += "  |  " + juce::String::fromUTF8(context.current_chord->symbol.c_str());
        }
        g.drawFittedText(statusText, statusArea, juce::Justification::topLeft, 5);
    }

  private:
    void filesMenu() {
        juce::PopupMenu popup;
        popup.setLookAndFeel(&nativeTheme);
        popup.addItem(701, "New MR Live show");
        popup.addItem(2, "Open...");
        popup.addItem(3, "Save");
        popup.addItem(4, "Save as...");
        popup.addItem(702, "Import editor show JSON...");
        juce::Component::SafePointer<Desktop> safe(&owner);
        popup.showMenuAsync(liveMenuOptions(files), [safe](int action) {
            if (safe && action)
                safe->action(action);
        });
    }
    void optionsMenu() {
        juce::PopupMenu popup;
        popup.setLookAndFeel(&nativeTheme);
        popup.addItem(2, "Item button layout...", owner.liveDocument.has_value());
        juce::PopupMenu appearance, zoom, menus, window;
        zoom.addItem(100, "Fit to window", true, owner.view.liveUiZoom == 0);
        for (const int percent : {75, 100, 125, 150})
            zoom.addItem(1000 + percent, juce::String(percent) + "%", true, owner.view.liveUiZoom == percent / 100.);
        for (const int percent : {70, 85, 90, 100})
            menus.addItem(2000 + percent, juce::String(100 - percent) + "% transparency", true,
                          std::abs(owner.view.liveMenuOpacity - percent / 100.) < .001);
        for (const int percent : {80, 90, 100})
            window.addItem(3000 + percent, juce::String(100 - percent) + "% transparency", true,
                           std::abs(owner.view.liveWindowOpacity - percent / 100.) < .001);
        appearance.addSectionHeader("Graphite");
        appearance.addSubMenu("UI scale", zoom);
        appearance.addSubMenu("Menu transparency", menus);
        appearance.addSubMenu("Window transparency", window);
        appearance.addItem(101, "Reset appearance");
        popup.addSubMenu("Appearance", appearance);
        popup.addItem(102, "Fit window to screen");
        popup.addItem(103, "Open last show on startup", true, owner.view.liveRestoreLastShow);
        popup.addSeparator();
        popup.addItem(
            42, "Previous item",
            owner.liveDocument &&
                live::neighborItem(owner.liveDocument->interfaceState, -1, owner.liveCurrentEntry).isNotEmpty());
        popup.addItem(
            43, "Next item",
            owner.liveDocument &&
                live::neighborItem(owner.liveDocument->interfaceState, 1, owner.liveCurrentEntry).isNotEmpty());
        popup.addItem(41, "Playback / Click / Cue...", owner.liveDocument.has_value());
        popup.addItem(105,"Show / source check...",owner.liveDocument.has_value());
        popup.addItem(106,"Live Input (pilot)...",owner.liveDocument.has_value());
        popup.addItem(104,"Back to start on Stop",owner.liveDocument.has_value(),
            owner.liveDocument&&static_cast<bool>(owner.liveDocument->interfaceState["performance"]["backToStart"]));
        popup.addItem(40, "Audio settings...");
        juce::Component::SafePointer<LiveWorkspace> safe(this);
        popup.showMenuAsync(liveMenuOptions(options), [safe](int action) {
            if (!safe)
                return;
            if (action == 2)
                openLiveCardEditor(safe->owner, safe->reference.defaults["itemCardLayout"]);
            else if(action==104){safe->owner.run([&]{safe->owner.editLiveInterface("Back to start",[&](auto& state){
                live::put(live::performance(state),"backToStart",!static_cast<bool>(state["performance"]["backToStart"]));});});}
            else if (action == 103) {
                safe->owner.view.liveRestoreLastShow = !safe->owner.view.liveRestoreLastShow;
                safe->owner.run([&] { safe->owner.saveSettings(); });
            } else if (action == 42 || action == 43)
                safe->owner.run([&] { safe->owner.stepLiveItem(action == 43 ? 1 : -1); });
            else if(action==105)openLivePreflight(safe->owner);
            else if(action==106)openLiveInputPilot(safe->owner);
            else if (action == 41)
                openLiveAudio(safe->owner);
            else if (action == 40)
                safe->owner.action(40);
            else if (action == 102) {
                if (safe->fitWindowRequested)
                    safe->fitWindowRequested();
            } else if (action == 100 || action == 101 || (action >= 1075 && action <= 3150)) {
                auto &view = safe->owner.view;
                if (action == 100 || action == 101)
                    view.liveUiZoom = 0;
                if (action == 101) {
                    view.liveMenuOpacity = .9;
                    view.liveWindowOpacity = 1.;
                }
                if (action >= 1075 && action <= 1150)
                    view.liveUiZoom = (action - 1000) / 100.;
                if (action >= 2070 && action <= 2100)
                    view.liveMenuOpacity = (action - 2000) / 100.;
                if (action >= 3080 && action <= 3100)
                    view.liveWindowOpacity = (action - 3000) / 100.;
                safe->nativeTheme.setPopupOpacity(static_cast<float>(view.liveMenuOpacity));
                if (safe->appearanceChanged)
                    safe->appearanceChanged();
                safe->owner.run([&] { safe->owner.saveSettings(); });
            }
        });
    }
    void timerCallback() override {
        if (!isVisible())
            return;
        if (!owner.isDragAndDropActive()) {
            setlist.refresh();
            browserItems.refresh();
        }
        // Only the non-application compatibility adapter can expose real Core rows
        // without show channel definitions. Dedicated MR Live always uses the show.
        const bool legacy = !owner.standaloneLive && owner.liveDocument && owner.liveDocument->currentLinked() &&
                            owner.liveDocument->interfaceState["channels"].size() == 0;
        if (legacy != showLegacyChannels) {
            showLegacyChannels = legacy;
            legacyChannels.setVisible(legacy);
            resized();
        }
        if (legacy)
            legacyChannels.refresh();
        timeline.refresh();
        playbackPanel.sync();
        if (mixerVisible) mixer.sync();
        mixerButton.setToggleState(mixerVisible, juce::dontSendNotification);
        const auto projectState = owner.app.services().projects->state();
        const bool showHistory = owner.liveShowEditing || (owner.liveDocument && owner.liveDocument->projects.empty());
        undoButton.setEnabled(showHistory ? owner.liveUndo.canUndo() : projectState.can_undo);
        redoButton.setEnabled(showHistory ? owner.liveUndo.canRedo() : projectState.can_redo);
        const auto state = owner.displayState();
        const bool linked = !owner.liveDocument || owner.liveDocument->currentLinked();
        play.setEnabled(linked || owner.liveTransportReady());
        record.setEnabled(linked);
        const bool sectionsReady = owner.liveTransportReady() && !owner.project()->sections.empty();
        previous.setEnabled((linked || sectionsReady) && !owner.app.recording());
        next.setEnabled((linked || sectionsReady) && !owner.app.recording());
        loop.setEnabled((linked || (sectionsReady && (owner.app.musical().state().current_section ||
                                                      owner.app.engine()->state().loop))) &&
                        !owner.app.recording());
        play.setToggleState(state.playback == mrs::PlaybackState::playing, juce::dontSendNotification);
        pause.setToggleState(state.playback == mrs::PlaybackState::paused, juce::dontSendNotification);
        record.setToggleState(owner.app.recording(), juce::dontSendNotification);
        loop.setToggleState(owner.app.engine()->state().loop.has_value(), juce::dontSendNotification);
        pointerTool.setToggleState(!timeline.clipScissors(),juce::dontSendNotification);
        scissorsTool.setToggleState(timeline.clipScissors(),juce::dontSendNotification);
        repaint();
    }
    Desktop &owner;
    LiveLayout reference;
    Theme nativeTheme;
    LiveSetlist setlist, browserItems;
    LiveTimeline timeline;
    LiveChannels legacyChannels;
    LivePlaybackPanel playbackPanel;
    LiveMixer mixer;
    juce::TextButton mixerButton{"Mixer"},pointerTool{"Pointer"},scissorsTool{"Scissors"};
    bool mixerVisible{};
    bool showLegacyChannels{};
    std::unique_ptr<juce::Drawable> tunerPlaceholder;
    juce::TextButton files{"Files"}, options{"Options"}, play{"Play"}, pause{"Pause"}, stop{"Stop"},
        record{"Record (R)"}, previous{"< Section"}, next{"Section >"}, loop{"Loop section"}, undoButton{"Undo"},
        redoButton{"Redo"};
    bool browserVisible{};
    std::optional<juce::uint32> dragStartedAt;
};
} // namespace ui
