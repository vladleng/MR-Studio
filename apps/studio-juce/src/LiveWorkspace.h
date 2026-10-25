#pragma once
#include "Desktop.h"
#include "LiveChannels.h"
#include "LiveLayout.h"
#include "LiveSetlist.h"

namespace ui {
class LiveWorkspace final : public juce::Component, private juce::Timer {
  public:
    explicit LiveWorkspace(Desktop &desktop)
        : owner(desktop), setlist(desktop, false, reference.defaults["itemCardLayout"]),
          browserItems(desktop, true, reference.defaults["itemCardLayout"]), channels(desktop) {
        setComponentID("mr-live-workspace");
        setName("MR Live");
        nativeTheme.applyPreview(DesignTokens::target());
        nativeTheme.useConceptAssets(true);
        setLookAndFeel(&nativeTheme);
        addMouseListener(this, true);
        for (auto *component :
             std::array<juce::Component *, 13>{&setlist, &channels, &files, &options, &play, &pause, &stop, &record,
                                               &previous, &next, &loop, &undoButton, &redoButton})
            addAndMakeVisible(component);
        addChildComponent(browserItems);
        files.onClick = [this] { filesMenu(); };
        options.onClick = [this] { optionsMenu(); };
        play.onClick = [this] { owner.action(30); };
        pause.onClick = [this] { owner.action(31); };
        stop.onClick = [this] { owner.action(32); };
        for (auto *button : {&play, &pause, &stop, &record, &previous, &next, &loop})
            button->getProperties().set("mrLiveTransport", true);
        tunerPlaceholder = LiveLayout::placeholder("live_tuner.svg");
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
    void showBrowser(bool visible) {
        browserVisible = visible;
        browserItems.setVisible(visible);
        resized();
        if (visible)
            browserItems.toFront(false);
    }
    bool itemsBrowserVisible() const { return browserVisible; }
    void resized() override {
        const auto size = getLocalBounds();
        const auto toolbar = reference.bounds("control.toolbar", size.getWidth(), size.getHeight());
        auto controls = toolbar.reduced(6).withHeight(32);
        files.setBounds(controls.removeFromLeft(44).reduced(1));
        options.setBounds(controls.removeFromLeft(52).reduced(1));
        undoButton.setBounds(controls.removeFromLeft(26));
        redoButton.setBounds(controls.removeFromLeft(26));
        setlist.setBounds(reference.bounds("show.setlist", getWidth(), getHeight()));
        channels.setBounds(reference.bounds("track.channels", getWidth(), getHeight()));
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
        const auto tokens = DesignTokens::target();
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
        const bool linked = !owner.liveDocument || owner.liveDocument->currentLinked();
        const auto head = owner.displayState();
        g.setColour(tokens.colour(ColourRole::text));
        g.setFont(owner.theme.font(14));
        g.drawText(juce::String(static_cast<double>(head.sample) / owner.project()->sample_rate, 2) + " s",
                   header.getCentreX() + 54, header.getCentreY() - 20, 120, 24, juce::Justification::left);
        g.setFont(owner.theme.font(9));
        g.setColour(tokens.colour(ColourRole::muted));
        g.drawText(head.playback == mrs::PlaybackState::playing ? "PLAYING" : "STOPPED", header.getCentreX() + 54,
                   header.getCentreY() + 4, 120, 18, juce::Justification::left);
        if (tunerPlaceholder) {
            const float tunerHeight = juce::jmin(64.f, static_cast<float>(header.getHeight()));
            const float tunerWidth = 264.f * tunerHeight / 64.f;
            tunerPlaceholder->drawWithin(g,
                                         {static_cast<float>(header.getRight()) - tunerWidth - 4.f,
                                          static_cast<float>(header.getCentreY()) - tunerHeight / 2.f, tunerWidth,
                                          tunerHeight},
                                         juce::RectanglePlacement::centred, 1.f);
        }
        auto timeline = reference.bounds("song.timeline", getWidth(), getHeight()).reduced(12);
        timeline.removeFromTop(34);
        g.setFont(owner.theme.font(13));
        if (linked) {
            for (const auto &label : owner.contextLabels)
                g.drawText(label.getText(), timeline.removeFromTop(28), juce::Justification::left, true);
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
        popup.addItem(701, "New MR Live show...");
        popup.addItem(2, "Open...");
        popup.addItem(3, "Save");
        popup.addItem(4, "Save as...");
        popup.addItem(702, "Import editor show JSON...");
        juce::Component::SafePointer<Desktop> safe(&owner);
        popup.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&files), [safe](int action) {
            if (safe && action)
                safe->action(action);
        });
    }
    void optionsMenu() {
        juce::PopupMenu popup;
        popup.addItem(1, "Browser items", true, browserVisible);
        popup.addItem(2, "Item button layout...", owner.liveDocument.has_value());
        popup.addSeparator();
        popup.addItem(40, "Audio settings...");
        juce::Component::SafePointer<LiveWorkspace> safe(this);
        popup.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&options), [safe](int action) {
            if (!safe)
                return;
            if (action == 1)
                safe->showBrowser(!safe->browserVisible);
            else if (action == 2)
                openLiveCardEditor(safe->owner, safe->reference.defaults["itemCardLayout"]);
            else if (action == 40)
                safe->owner.action(40);
        });
    }
    void timerCallback() override {
        if (!isVisible())
            return;
        if (!owner.isDragAndDropActive()) {
            setlist.refresh();
            browserItems.refresh();
        }
        channels.refresh();
        const auto projectState = owner.app.services().projects->state();
        undoButton.setEnabled(owner.liveShowEditing ? owner.liveUndo.canUndo() : projectState.can_undo);
        redoButton.setEnabled(owner.liveShowEditing ? owner.liveUndo.canRedo() : projectState.can_redo);
        const auto state = owner.displayState();
        const bool linked = !owner.liveDocument || owner.liveDocument->currentLinked();
        play.setEnabled(linked);
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
    LiveChannels channels;
    std::unique_ptr<juce::Drawable> tunerPlaceholder;
    juce::TextButton files{"Files"}, options{"Options"}, play{"Play"}, pause{"Pause"}, stop{"Stop"},
        record{"Record (R)"}, previous{"< Section"}, next{"Section >"}, loop{"Loop section"}, undoButton{"Undo"},
        redoButton{"Redo"};
    bool browserVisible{};
    juce::uint32 blockedUntil{};
};
} // namespace ui
