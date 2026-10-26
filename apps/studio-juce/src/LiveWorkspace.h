#pragma once
#include "Desktop.h"
#include "LiveChannels.h"
#include "LiveContext.h"
#include "LiveLayout.h"
#include "LivePlaybackPanel.h"
#include "LiveSetlist.h"
#include "LiveTheme.h"
#include "LiveTimeline.h"

namespace ui {
class LiveWorkspace final : public juce::Component, private juce::Timer {
  public:
    explicit LiveWorkspace(Desktop &desktop)
        : owner(desktop), setlist(desktop, false, reference.defaults["itemCardLayout"]),
          browserItems(desktop, true, reference.defaults["itemCardLayout"]), timeline(desktop), legacyChannels(desktop),
          playbackPanel(desktop) {
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
        const auto guard = [this] { blockedUntil = juce::Time::getMillisecondCounter() + 250; };
        const auto blocked = [this] {
            return static_cast<juce::int32>(blockedUntil - juce::Time::getMillisecondCounter()) > 0;
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
        browserItems.setVisible(visible);
        resized();
        if (visible)
            browserItems.toFront(false);
    }
    bool itemsBrowserVisible() const { return browserVisible; }
    void resized() override {
        playbackPanel.setBounds(reference.bounds("playback.stems", getWidth(), getHeight()));
        const auto size = getLocalBounds();
        const auto toolbar = reference.bounds("control.toolbar", size.getWidth(), size.getHeight());
        auto controls = toolbar.reduced(6).withHeight(32);
        files.setBounds(controls.removeFromLeft(44).reduced(1));
        options.setBounds(controls.removeFromLeft(52).reduced(1));
        undoButton.setBounds(controls.removeFromLeft(26));
        redoButton.setBounds(controls.removeFromLeft(26));
        setlist.setBounds(reference.bounds("show.setlist", getWidth(), getHeight()));
        const auto channelBounds = reference.bounds("track.channels", getWidth(), getHeight());
        const auto timelineBounds = reference.bounds("song.timeline", getWidth(), getHeight());
        legacyChannels.setBounds(channelBounds);
        timeline.setBounds(showLegacyChannels ? timelineBounds : channelBounds.getUnion(timelineBounds));
        timeline.setChannelWidth(showLegacyChannels ? 0 : timelineBounds.getX() - channelBounds.getX());
        const auto header = reference.bounds("control.header", getWidth(), getHeight());
        auto transport = juce::Rectangle<int>(header.getCentreX() - 178, header.getCentreY() - 16, 224, 32);
        for (auto *button : {&previous, &play, &pause, &stop, &next, &loop, &record})
            button->setBounds(transport.removeFromLeft(32).reduced(2));
        const int width = juce::jmin(380, juce::jmax(220, getWidth() - setlist.getRight() - 16));
        browserItems.setBounds(setlist.getRight() + 2, setlist.getY(), width, setlist.getHeight());
        if (browserVisible)
            browserItems.toFront(false);
    }
    void mouseDown(const juce::MouseEvent &event) override {
        if (!event.mods.isLeftButtonDown())
            return;
        auto *origin = event.originalComponent;
        const bool inSetlist = origin == &setlist || setlist.isParentOf(origin);
        const bool inBrowser = origin == &browserItems || browserItems.isParentOf(origin);
        if (browserVisible && !inSetlist && !inBrowser)
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
        const auto stems = reference.bounds("playback.stems", getWidth(), getHeight()).reduced(10);
        g.setColour(tokens.colour(ColourRole::text));
        g.setFont(owner.theme.font(12));
        g.drawText("Playback / stems", stems.withHeight(24), juce::Justification::left);
        auto statusArea = reference.bounds("system.status", getWidth(), getHeight()).reduced(10);
        g.setColour(owner.message.isEmpty() ? tokens.colour(ColourRole::muted) : tokens.colour(ColourRole::warning));
        g.setFont(owner.theme.font(11));
        g.drawFittedText(owner.footerStatus(), statusArea, juce::Justification::topLeft, 4);
    }

  private:
    void filesMenu() {
        juce::PopupMenu popup;
        popup.setLookAndFeel(&nativeTheme);
        popup.addItem(701, "New MR Live show...");
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
        popup.addItem(1, "Browser items", true, browserVisible);
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
        popup.addSeparator();
        popup.addItem(40, "Audio settings...");
        juce::Component::SafePointer<LiveWorkspace> safe(this);
        popup.showMenuAsync(liveMenuOptions(options), [safe](int action) {
            if (!safe)
                return;
            if (action == 1)
                safe->showBrowser(!safe->browserVisible);
            else if (action == 2)
                openLiveCardEditor(safe->owner, safe->reference.defaults["itemCardLayout"]);
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
        const auto projectState = owner.app.services().projects->state();
        undoButton.setEnabled(owner.liveShowEditing ? owner.liveUndo.canUndo() : projectState.can_undo);
        redoButton.setEnabled(owner.liveShowEditing ? owner.liveUndo.canRedo() : projectState.can_redo);
        const auto state = owner.displayState();
        const bool linked = !owner.liveDocument || owner.liveDocument->currentLinked();
        play.setEnabled(linked || owner.livePlaybackReady());
        record.setEnabled(linked);
        previous.setEnabled(linked && !owner.app.recording());
        next.setEnabled(linked && !owner.app.recording());
        loop.setEnabled(linked && !owner.app.recording());
        play.setToggleState(state.playback == mrs::PlaybackState::playing, juce::dontSendNotification);
        pause.setToggleState(state.playback == mrs::PlaybackState::paused, juce::dontSendNotification);
        record.setToggleState(owner.app.recording(), juce::dontSendNotification);
        loop.setToggleState(owner.app.engine()->state().loop.has_value(), juce::dontSendNotification);
        repaint();
    }
    Desktop &owner;
    LiveLayout reference;
    Theme nativeTheme;
    LiveSetlist setlist, browserItems;
    LiveTimeline timeline;
    LiveChannels legacyChannels;
    LivePlaybackPanel playbackPanel;
    bool showLegacyChannels{};
    std::unique_ptr<juce::Drawable> tunerPlaceholder;
    juce::TextButton files{"Files"}, options{"Options"}, play{"Play"}, pause{"Pause"}, stop{"Stop"},
        record{"Record (R)"}, previous{"< Section"}, next{"Section >"}, loop{"Loop section"}, undoButton{"Undo"},
        redoButton{"Redo"};
    bool browserVisible{};
    juce::uint32 blockedUntil{};
};
} // namespace ui
