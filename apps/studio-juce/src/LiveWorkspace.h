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
             std::array<juce::Component *, 14>{&setlist, &channels, &files, &options, &studio, &play, &pause, &stop,
                                               &record, &previous, &next, &loop, &undoButton, &redoButton})
            addAndMakeVisible(component);
        addChildComponent(browserItems);
        files.onClick = [this] { filesMenu(); };
        options.onClick = [this] { optionsMenu(); };
        studio.onClick = [this] {
            owner.liveShowEditing = false;
            owner.setWorkspace(mrs::desktop::Workspace::arrange);
        };
        play.onClick = [this] { owner.action(30); };
        pause.onClick = [this] { owner.action(31); };
        stop.onClick = [this] { owner.action(32); };
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
        studio.setBounds(controls.reduced(1));
        setlist.setBounds(reference.bounds("show.setlist", getWidth(), getHeight()));
        channels.setBounds(reference.bounds("track.channels", getWidth(), getHeight()));
        const auto header = reference.bounds("control.header", getWidth(), getHeight());
        auto transport = header.withTrimmedLeft(juce::jmin(juce::jmax(0, header.getWidth() - 250), 280)).reduced(4);
        transport = transport.withY(header.getY() + 12).withHeight(38);
        for (auto *button : {&play, &pause, &stop, &record})
            button->setBounds(transport.removeFromLeft(44).reduced(2));
        const auto timeline = reference.bounds("song.timeline", getWidth(), getHeight());
        auto navigation = timeline.withTrimmedTop(4).withHeight(30);
        for (auto *button : {&previous, &next, &loop})
            button->setBounds(navigation.removeFromLeft(42).reduced(2));
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
        const auto state = owner.liveDocument ? owner.liveDocument->interfaceState : juce::var();
        const auto current = live::find(state, "items", state["currentItem"].toString());
        const bool linked = !owner.liveDocument || owner.liveDocument->currentLinked();
        g.setColour(tokens.colour(ColourRole::text));
        g.setFont(owner.theme.font(17));
        g.drawText(current.isObject() ? current["name"].toString() : owner.projectCaption(), header.getX() + 10,
                   header.getY() + 5, juce::jmin(265, header.getWidth() - 20), 28, juce::Justification::left, true);
        g.setFont(owner.theme.font(10));
        g.setColour(tokens.colour(ColourRole::muted));
        g.drawText(owner.projectCaption(), header.getX() + 10, header.getY() + 34,
                   juce::jmin(265, header.getWidth() - 20), 20, juce::Justification::left, true);
        const auto head = owner.displayState();
        g.setFont(owner.theme.font(14));
        g.setColour(tokens.colour(ColourRole::text));
        g.drawText(juce::String(static_cast<double>(head.sample) / owner.project()->sample_rate, 2) + " s",
                   header.getRight() - 105, header.getY() + 12, 95, 38, juce::Justification::right);
        auto timeline = reference.bounds("song.timeline", getWidth(), getHeight()).reduced(12);
        timeline.removeFromTop(34);
        g.setFont(owner.theme.font(13));
        if (!linked) {
            g.drawText("Link a .mrsproject to this item to use its shared timeline and audio",
                       timeline.removeFromTop(46), juce::Justification::topLeft, true);
        } else {
            for (const auto &label : owner.contextLabels)
                g.drawText(label.getText(), timeline.removeFromTop(28), juce::Justification::left, true);
            g.setColour(tokens.colour(ColourRole::muted));
            g.drawText("Open Studio to edit this shared project", timeline.removeFromTop(34), juce::Justification::left,
                       true);
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
        popup.addSeparator();
        popup.addItem(6, "Project home");
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
    juce::TextButton files{"Files"}, options{"Options"}, studio{"Studio"}, play{"Play"}, pause{"Pause"}, stop{"Stop"},
        record{"Record (R)"}, previous{"< Section"}, next{"Section >"}, loop{"Loop section"}, undoButton{"Undo"},
        redoButton{"Redo"};
    bool browserVisible{};
    juce::uint32 blockedUntil{};
};
} // namespace ui
