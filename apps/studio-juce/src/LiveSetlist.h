#pragma once
#include "Desktop.h"
#include "LiveActionMenu.h"
#include "LivePerformanceUI.h"
#include "LiveCardEditor.h"
#include "LiveItemEditor.h"
#include "LiveLayout.h"
#include "LiveTheme.h"
#include <MRConceptAssets.h>

namespace ui {
class LiveSetlist final : public juce::Component, public juce::DragAndDropTarget, private juce::Timer {
  public:
    struct RowData {
        juce::String id, itemId, name;
        int depth{};
        bool folder{}, divider{};
    };
    LiveSetlist(Desktop &desktop, bool browserMode, const juce::var &cardDefaults)
        : owner(desktop), browser(browserMode), defaults(cardDefaults.clone()) {
        setName(browser ? "Browser items" : "Setlist");
        setComponentID(browser ? "live-items-browser" : "live-setlist");
        getProperties().set("mrLiveShowEditor", true);
        setWantsKeyboardFocus(true);
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&body, false);
        viewport.setScrollBarsShown(true, false);
        body.setInterceptsMouseClicks(true, true);
        body.addMouseListener(this, false);
        if (browser) {
            addAndMakeVisible(search);
            search.setTextToShowWhenEmpty("Search items", juce::Colours::grey);
            search.setTitle("Search Browser items and folders");
            search.onTextChange = [this] { rebuild(); };
        } else {
            for (auto *component :
                 std::array<juce::Component *, 4>{&setlists, &browserButton, &organizerButton, &menuButton})
                addAndMakeVisible(component);
            browserButton.setTitle("Show Browser items");
            organizerButton.setTitle("Show organizer fields");
            menuButton.setTitle("Setlist actions");
            browserButton.onClick = [this] {
                if (toggleBrowser)
                    toggleBrowser();
            };
            organizerButton.onClick = [this] {
                owner.run([&] {
                    owner.editLiveInterface("Organizer fields", [](auto &state) {
                        auto &perf = live::performance(state);
                        live::put(perf, "showOrganizer", !static_cast<bool>(perf["showOrganizer"]));
                    });
                });
            };
            menuButton.setComponentID("live-setlist-menu");
            menuButton.onClick = [this] { context({}, liveContextTarget(menuButton.getScreenBounds().getBottomLeft())); };
            setlists.onChange = [this] {
                if (syncing || !owner.liveDocument)
                    return;
                const int index = setlists.getSelectedItemIndex();
                const auto state = data();
                if (index < 0 || index >= state["setlists"].size())
                    return;
                live::put(owner.liveDocument->interfaceState, "activeSetlist", state["setlists"][index]["id"]);
                owner.liveShowEditing = true;
                owner.updateLiveDirty();
                ++owner.liveUiRevision;
            };
        }
        rebuild();
    }
    ~LiveSetlist() override {
        cancelActivation();
        body.removeMouseListener(this);
        viewport.setViewedComponent(nullptr, false);
    }
    std::function<void()> toggleBrowser;
    std::function<void()> dragGuard;
    std::function<bool()> activationBlocked;
    juce::var data() const { return owner.liveDocument ? owner.liveDocument->interfaceState : juce::var(); }
    juce::var layout() const {
        const auto value = data()["performance"]["itemCardLayout"];
        return value.isObject() ? value : defaults;
    }
    bool hasPendingActivation()const{return pendingActivation.has_value();}
    void refresh() {
        if (revision != owner.liveUiRevision)
            rebuild();
    }
    void resized() override {
        auto area = getLocalBounds().reduced(6);
        if (browser)
            search.setBounds(area.removeFromTop(28));
        else {
            auto header = area.removeFromTop(32);
            menuButton.setBounds(header.removeFromRight(27).reduced(1));
            organizerButton.setBounds(header.removeFromRight(35).reduced(1));
            browserButton.setBounds(header.removeFromRight(64).reduced(1));
            setlists.setBounds(header.reduced(1));
        }
        area.removeFromTop(6);
        viewport.setBounds(area);
        layoutRows();
    }
    void paint(juce::Graphics &g) override {
        const auto tokens = liveReferenceTokens();
        g.fillAll(tokens.colour(ColourRole::panel));
        g.setColour(tokens.colour(ColourRole::border));
        g.drawRect(getLocalBounds(), 1);
        if (rows.empty()) {
            g.setColour(tokens.colour(ColourRole::muted));
            g.setFont(12.f);
            g.drawText(owner.liveDocument ? (browser ? "No matching items" : "Add an item")
                                          : "Create or open a MR Live show",
                       viewport.getBounds().reduced(12), juce::Justification::centred, true);
        }
    }
    void paintOverChildren(juce::Graphics &g) override {
        if (dropY >= 0) {
            g.setColour(liveReferenceTokens().colour(ColourRole::focus));
            g.fillRect(viewport.getX(), viewport.getY() + dropY - viewport.getViewPositionY(), viewport.getWidth(), 2);
        }
    }
    void mouseDown(const juce::MouseEvent &event) override {
        if (event.originalComponent != this && event.originalComponent != &body && event.originalComponent != &viewport)
            return;
        const auto pointer = event.getScreenPosition();
        cancelActivation();++owner.liveSelectionRevision;
        owner.liveShowEditing = true;
        grabKeyboardFocus();
        if (event.mods.isPopupMenu()) context({}, liveContextTarget(pointer));
    }
    bool keyPressed(const juce::KeyPress &key) override {
        if (key.getModifiers().isCtrlDown() && (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z')) {
            owner.undoLiveInterface(key.getModifiers().isShiftDown());
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::deleteKey && !browser && focusIndex >= 0 &&
            focusIndex < static_cast<int>(rows.size())) {
            const auto row = rows[static_cast<std::size_t>(focusIndex)];
            owner.run([&] { handle(6, row); });
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::upKey || key.getKeyCode() == juce::KeyPress::downKey) {
            ++owner.liveSelectionRevision;cancelActivation();
            focusIndex = juce::jlimit(0, juce::jmax(0, static_cast<int>(rows.size()) - 1),
                                      focusIndex + (key.getKeyCode() == juce::KeyPress::upKey ? -1 : 1));
            focusRow();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::returnKey && focusIndex >= 0 &&
            focusIndex < static_cast<int>(rows.size())) {
            activate(rows[static_cast<std::size_t>(focusIndex)]);
            return true;
        }
        return false;
    }
    bool isInterestedInDragSource(const SourceDetails &source) override {
        return source.description.isObject() && source.description["mrLiveRow"].isString() &&
               owner.liveDocument.has_value();
    }
    void itemDragMove(const SourceDetails &source) override {
        viewport.autoScroll(source.localPosition.x - viewport.getX(), source.localPosition.y - viewport.getY(), 24, 12);
        dropY = body.getHeight();
        const int y = source.localPosition.y - viewport.getY() + viewport.getViewPositionY();
        for (const auto &row : cards)
            if (y < row->getBottom()) {
                dropY = y < row->getBounds().getCentreY() ? row->getY() : row->getBottom();
                break;
            }
        repaint();
    }
    void itemDragExit(const SourceDetails &) override {
        dropY = -1;
        repaint();
    }
    void itemDropped(const SourceDetails &source) override {
        if (dragGuard)
            dragGuard();
        const int y = source.localPosition.y - viewport.getY() + viewport.getViewPositionY();
        juce::String target;
        bool after = false;
        RowData targetRow;
        for (std::size_t i = 0; i < cards.size(); ++i)
            if (y < cards[i]->getBottom()) {
                target = rows[i].id;
                after = y >= cards[i]->getBounds().getCentreY();
                targetRow = rows[i];
                break;
            }
        const auto payload = source.description;
        const auto id = payload["mrLiveRow"].toString();
        const auto kind = payload["kind"].toString();
        const auto itemId = payload["itemId"].toString();
        owner.run([&] {
            owner.editLiveInterface(browser ? "Move Browser item" : "Setlist order", [&](auto &state) {
                if (browser) {
                    const juce::var parent = targetRow.folder ? juce::var(targetRow.id) : juce::var();
                    if (kind == "folder")
                        live::put(live::require(state, "folders", id), "parent", parent);
                    else if (itemId.isNotEmpty())
                        live::put(live::require(state, "items", itemId), "folder", parent);
                } else if (kind == "entry")
                    live::reorderEntry(state, id, target, after);
                else if (kind == "item")
                    live::insertEntry(state, itemId, target, after);
            });
        });
        dropY = -1;
        repaint();
    }
    void activate(const RowData &row) {
        if (browser && !row.folder && !row.divider) {
            // Library inspection never activates a setlist occurrence, Core graph or timeline.
            if (live::find(data(), "items", row.itemId).isObject()) {
                owner.app.stop_library_preview();
                owner.liveBrowserItem = row.itemId;
                ++owner.liveUiRevision;
            }
            return;
        }
        ++owner.liveSelectionRevision;
        owner.liveShowEditing = true;
        if (row.folder) {
            if (collapsed.contains(row.id))
                collapsed.erase(row.id);
            else
                collapsed.insert(row.id);
            rebuild();
            return;
        }
        if (row.divider)
            return;
        const auto id = row.itemId, entry = browser ? juce::String() : row.id;
        juce::Component::SafePointer<Desktop> safe(&owner);
        owner.confirmDiscard(
            [safe, id, entry] {
                if (safe)
                    safe->run([&] { safe->selectLiveItem(id, entry); });
            },
            true);
    }
    void rename(const RowData &row) {
        if (row.folder || row.divider) {
            juce::Component::SafePointer<Desktop> safe(&owner);
            promptLiveContext(owner, "Rename", row.name, [safe, row](const auto &text) {
                if (!safe)
                    return;
                safe->editLiveInterface("Rename", [&](auto &state) {
                    if (row.folder)
                        live::put(live::require(state, "folders", row.id), "name", text.trim());
                    else {
                        auto &entries = *live::activeEntries(state).getArray();
                        for (auto &entry : entries)
                            if (entry["id"].toString() == row.id)
                                live::put(entry, "name", text.trim());
                    }
                });
            });
        } else {
            const auto item = live::find(data(), "items", row.itemId);
            juce::Component::SafePointer<Desktop> safe(&owner);
            promptLiveContext(owner, "Item name", item["name"].toString(), [safe, id = row.itemId](const auto &text) {
                if (safe)
                    safe->editLiveInterface("Item name", [&](auto &state) {
                        live::put(live::require(state, "items", id), "name", text.trim());
                    });
            });
        }
    }
    void showItemContext(const RowData &row, juce::Component &anchor, juce::Rectangle<int> point = {}) {
        juce::Component::SafePointer<LiveSetlist> safe(this);
        openLiveItemEditor(
            owner, row.itemId, &anchor, point,
            [safe, row](int command) {
                if (safe) {
                    safe->owner.liveShowEditing = true;
                    safe->owner.run([&] { safe->handle(command, row); });
                }
            },
            !browser);
    }
    void context(RowData row, juce::Rectangle<int> target = {}) {
        if (target.isEmpty()) target = liveContextTarget(juce::Desktop::getMousePosition());
        if (!owner.liveDocument)
            return;
        if (row.itemId.isNotEmpty()) {
            showItemContext(row, *this, target);
            return;
        }
        juce::PopupMenu popup;
        popup.setLookAndFeel(&getLookAndFeel());
        if (!browser && !row.folder && !row.divider) {
            popup.addItem(40, "+ Setlist");
            popup.addItem(41, juce::String::fromUTF8("Переименовать setlist"));
            popup.addItem(42, juce::String::fromUTF8("Удалить setlist"), data()["setlists"].size() > 1);
            popup.addSeparator();
        }
        if (row.itemId.isNotEmpty()) {
            popup.addItem(1, "Properties / organizer...");
            popup.addItem(2, "Save for All");
            popup.addItem(4, "Add to Setlist");
            popup.addItem(5, "Rename...");
            addLivePerformanceModes(popup,data(),row.itemId,60);
            if (!browser)
                popup.addItem(6, "Remove this Setlist entry");
            popup.addSeparator();
        } else if (row.folder || row.divider)
            popup.addItem(5, "Rename...");
        popup.addItem(10, "+ Item");
        popup.addItem(11, "New folder...");
        if (!browser)
            popup.addItem(12, juce::String::fromUTF8("Создать разделитель"));
        popup.addItem(13, "Item button layout...");
        popup.addSeparator();
        popup.addItem(20, "Undo show", owner.liveUndo.canUndo());
        popup.addItem(21, "Redo show", owner.liveUndo.canRedo());
        juce::Component::SafePointer<LiveSetlist> safe(this);
        showLiveActionMenu(popup, owner, target, [safe, row](int choice) {
            if (!safe)
                return;
            safe->owner.liveShowEditing = true;
            safe->owner.run([&] {
                if(choice>=60 && choice<64)safe->owner.editLiveInterface("Item performance mode",[&](auto& state){
                    live::setPerformanceMode(state,row.itemId,row.itemId,static_cast<mrs::PerformanceMode>(choice-60));
                });
                else safe->handle(choice, row);
            });
        });
    }

  private:
    class Card final : public juce::Component, public juce::SettableTooltipClient {
      public:
        Card(LiveSetlist &source, RowData value) : host(source), row(std::move(value)) {
            setWantsKeyboardFocus(true);
            setName(row.name);
            setComponentID((host.browser ? "live-browser-" : "live-entry-") + row.id);
            const auto state = host.data();
            cardLayout = host.layout();
            showOrganizer = static_cast<bool>(state["performance"]["showOrganizer"]);
            if (row.itemId.isNotEmpty()) {
                item = host.browser ? live::libraryItem(state, row.itemId) : live::find(state, "items", row.itemId);
                const auto flow = live::workflow(state, row.itemId);
                status = flow["status"].toString();
                priority = flow["priority"].toString();
                percent = live::progress(state, row.itemId);
                dirty = live::itemDirty(state, row.itemId);
                tint = state["performance"]["itemColors"][juce::Identifier(row.itemId)].toString();
                itemBase = LiveLayout::drawable("item_default_base.svg");
                selectedBase = LiveLayout::drawable("item_selected_base.svg");
                itemBase->replaceColour(juce::Colour(0xff78ad88), liveBlockColour(tint));
                selectedBase->replaceColour(juce::Colour(0xffd0aa68), liveBlockColour(tint));
                statusBase = LiveLayout::drawable("pill_base.svg");
                priorityBase = LiveLayout::drawable("badge_base.svg");
                progressBase = LiveLayout::drawable("track.svg");
                statusBase->replaceColour(juce::Colour(0xff444541), juce::Colour(status == "Done"   ? 0xff285e4d
                                                                                 : status == "Wait" ? 0xff725a23
                                                                                 : status == "Wip"  ? 0xff355f83
                                                                                                    : 0xff444541));
                priorityBase->replaceColour(juce::Colour(0xff5a5a57), juce::Colour(priority == "P1"   ? 0xff99564a
                                                                                   : priority == "P2" ? 0xff94613e
                                                                                   : priority == "P3" ? 0xff887142
                                                                                                      : 0xff5a5a57));
                setTooltip(item["name"].toString() +
                           (host.owner.liveDocument->projectFor(row.itemId) >= 0 ? "\nAudio source ready"
                                                                                 : "\nNo playback media") +
                           (dirty ? "\nWorking item differs from Browser" : "\nSaved in Browser"));
            }
            if (row.divider)
                sectionBase = LiveLayout::drawable("set_header.svg");
            if (row.folder)
                folderIcon =
                    juce::Drawable::createFromImageData(MRConceptAssets::folder_svg, MRConceptAssets::folder_svgSize);
        }
        // Pending activation belongs to LiveSetlist and must survive row replacement.
        ~Card() override = default;
        float preferredHeight() const {
            if (row.folder || row.divider)
                return 28.f;
            double height = cardLayout["height"];
            if (!showOrganizer) {
                height = 36;
                for (const auto *key : {"name", "metadata", "saved"}) {
                    const auto field = cardLayout["fields"][key];
                    if (static_cast<bool>(field["visible"]))
                        height =
                            juce::jmax(height, static_cast<double>(field["y"]) + static_cast<double>(field["h"]) + 5.);
                }
            }
            return static_cast<float>(height) - cardGap();
        }
        float cardGap() const {
            const auto fields = cardLayout["fields"];
            const float nameBottom =
                static_cast<float>(fields["name"]["y"]) + juce::jmin(14.f, static_cast<float>(fields["name"]["h"]));
            const float metadataTop = fields["metadata"]["y"];
            return juce::jlimit(0.f, 6.f, metadataTop - nameBottom - 2.f);
        }
        void paint(juce::Graphics &g) override {
            const auto tokens = liveReferenceTokens();
            const int inset = row.depth * 12;
            auto bounds = getLocalBounds().withTrimmedLeft(inset).toFloat().reduced(1);
            g.setColour(tokens.colour(ColourRole::text));
            g.setFont(host.owner.theme.font(row.divider ? 12.f : 13.f));
            if (row.folder) {
                if (folderIcon)
                    folderIcon->drawWithin(g, bounds.removeFromLeft(18).reduced(2), juce::RectanglePlacement::centred,
                                           1.f);
                g.drawText((host.collapsed.contains(row.id) ? "> " : "v ") + row.name, bounds,
                           juce::Justification::centredLeft, true);
                return;
            }
            if (row.divider) {
                sectionBase->drawWithin(g, bounds, juce::RectanglePlacement::stretchToFit, 1.f);
                g.setColour(tokens.colour(ColourRole::text));
                g.drawText(row.name, inset + 14, 0, getWidth() - inset - 18, getHeight(),
                           juce::Justification::centredLeft, true);
                return;
            }
            const auto selected =
                (host.browser ? host.owner.liveBrowserItem == row.itemId :
                    host.data()["currentItem"].toString() == row.itemId &&
                    (host.owner.liveCurrentEntry.isEmpty() || host.owner.liveCurrentEntry == row.id));
            (selected ? selectedBase : itemBase)->drawWithin(g, bounds, juce::RectanglePlacement::stretchToFit, 1.f);
            if (selected) {
                g.setColour(tokens.colour(ColourRole::focus));
                g.drawRoundedRectangle(bounds, 4.f, .7f);
            }
            const auto field = [&](const char *key) {
                const auto value = cardLayout["fields"][key];
                return juce::Rectangle<float>(
                    bounds.getX() + static_cast<float>(value["x"]) * bounds.getWidth() / 100.f,
                    static_cast<float>(value["y"]) - (juce::String(key) == "metadata" ||
                                                              juce::String(key) == "status" ||
                                                              juce::String(key) == "priority"
                                                          ? cardGap()
                                                          : 0.f),
                    static_cast<float>(value["w"]) * bounds.getWidth() / 100.f, static_cast<float>(value["h"]));
            };
            const auto enabled = [&](const char *key) {
                return static_cast<bool>(cardLayout["fields"][key]["visible"]);
            };
            g.setColour(tokens.colour(ColourRole::text));
            g.setFont(host.owner.theme.font(13));
            if (enabled("name"))
                g.drawText(item["name"].toString(), field("name"), juce::Justification::centredLeft, true);
            g.setColour(tokens.colour(ColourRole::muted));
            g.setFont(host.owner.theme.font(10));
            if (enabled("metadata"))
                g.drawText(item["tempo"].toString() + "  |  " + item["meter"].toString() + "  |  " +
                               item["key"].toString(),
                           field("metadata"), juce::Justification::centredLeft, true);
            if (showOrganizer) {
                if (enabled("progress")) {
                    auto bar = field("progress");
                    progressBase->drawWithin(g, bar, juce::RectanglePlacement::stretchToFit, 1.f);
                    bar.setWidth(bar.getWidth() * percent / 100.f);
                    g.setColour(juce::Colour(0xff59ad87));
                    g.fillRoundedRectangle(bar, 2);
                }
                if (enabled("priority")) {
                    priorityBase->drawWithin(g, field("priority"), juce::RectanglePlacement::stretchToFit, 1.f);
                    g.setColour(tokens.colour(ColourRole::text));
                    g.drawText(priority, field("priority"), juce::Justification::centred, true);
                }
                if (enabled("status")) {
                    statusBase->drawWithin(g, field("status"), juce::RectanglePlacement::stretchToFit, 1.f);
                    auto statusArea = field("status");
                    const float dotSize = juce::jmin(4.f, statusArea.getHeight() / 3.f);
                    g.setColour(juce::Colour(status == "Done"   ? 0xff6bc39d
                                             : status == "Wait" ? 0xffd6ae58
                                             : status == "Wip"  ? 0xff72b5e8
                                                                : 0xffa0a39e));
                    g.fillEllipse(statusArea.getX() + 5.f, statusArea.getCentreY() - dotSize / 2.f, dotSize, dotSize);
                    g.setColour(tokens.colour(ColourRole::text));
                    g.drawText(status, statusArea.withTrimmedLeft(10.f), juce::Justification::centred, true);
                }
            }
            if (enabled("saved")) {
                auto dot = field("saved");
                const float size = juce::jmin(5.f, juce::jmin(dot.getWidth(), dot.getHeight()));
                g.setColour(tokens.colour(dirty ? ColourRole::warning : ColourRole::on));
                g.fillEllipse(dot.getCentreX() - size / 2, dot.getCentreY() - size / 2, size, size);
            }
            if (hasKeyboardFocus(true)) {
                g.setColour(tokens.colour(ColourRole::focus));
                g.drawRoundedRectangle(bounds.reduced(1), 4, 1);
            }
        }
        void mouseDown(const juce::MouseEvent &event) override {
            const auto pointer = event.getScreenPosition();
            host.cancelActivation();
            if (!host.browser) { ++host.owner.liveSelectionRevision; host.owner.liveShowEditing = true; }
            dragged = false;
            primaryPressed = event.mods.isLeftButtonDown();
            host.focusIndex = host.indexOf(row.id);
            grabKeyboardFocus();
            if (event.mods.isPopupMenu()) {
                if (row.itemId.isNotEmpty())
                    host.showItemContext(row, *this, liveContextTarget(pointer));
                else
                    host.context(row, liveContextTarget(pointer));
            }
        }
        void mouseDoubleClick(const juce::MouseEvent &event) override {
            if (event.mods.isLeftButtonDown()) {
                host.cancelActivation();
                if (host.browser)
                    host.activate(row);
                else
                    host.rename(row);
            }
        }
        void mouseDrag(const juce::MouseEvent &event) override {
            if (dragged || !event.mods.isLeftButtonDown() || event.getDistanceFromDragStart() < 6)
                return;
            dragged = true;
            host.cancelActivation();
            if (host.dragGuard)
                host.dragGuard();
            auto payload = live::object();
            live::put(payload, "mrLiveRow", row.id);
            live::put(payload, "kind", row.folder ? "folder" : host.browser ? "item" : "entry");
            live::put(payload, "itemId", row.itemId);
            host.owner.startDragging(payload, this);
        }
        void mouseUp(const juce::MouseEvent &event) override {
            const bool click = primaryPressed && !dragged && event.getNumberOfClicks() == 1;
            primaryPressed = false;
            if (click && (!host.activationBlocked || !host.activationBlocked())) {
                if (host.browser)
                    host.activate(row);
                else
                    host.scheduleActivation(row);
            }
        }
        bool keyPressed(const juce::KeyPress &key) override {
            if (key.getKeyCode() == juce::KeyPress::F10Key && key.getModifiers().isShiftDown()) {
                host.context(row);
                return true;
            }
            return host.keyPressed(key);
        }
        void focusLost(FocusChangeType) override {
            // A committed click survives focus changes from rebuilding controls.
            // A newer explicit navigation is detected by the host revision.
            repaint();
        }

      private:
        LiveSetlist &host;
        RowData row;
        juce::var item, cardLayout;
        juce::String status, priority, tint;
        int percent{};
        bool dirty{}, showOrganizer{}, dragged{}, primaryPressed{};
        std::unique_ptr<juce::Drawable> folderIcon, itemBase, selectedBase, sectionBase, statusBase, priorityBase,
            progressBase;
    };
    void scheduleActivation(const RowData& row){
        pendingActivation=row;pendingSetlist=data()["activeSetlist"].toString();pendingSelectionRevision=owner.liveSelectionRevision;startTimer(500);
    }
    void cancelActivation(){stopTimer();pendingActivation.reset();}
    void timerCallback()override{
        const auto target=pendingActivation;const auto list=pendingSetlist;cancelActivation();
        if(!target||pendingSelectionRevision!=owner.liveSelectionRevision||list!=data()["activeSetlist"].toString()||(activationBlocked&&activationBlocked()))return;
        const auto at=indexOf(target->id);
        if(at>=0){const auto row=rows[static_cast<std::size_t>(at)];activate(row);}
    }
    int indexOf(const juce::String &id) const {
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (rows[i].id == id)
                return static_cast<int>(i);
        return -1;
    }
    void focusRow() {
        if (focusIndex < 0 || focusIndex >= static_cast<int>(cards.size()))
            return;
        auto &row = *cards[static_cast<std::size_t>(focusIndex)];
        const auto bounds = row.getBounds();
        int y = viewport.getViewPositionY();
        if (bounds.getY() < y)
            y = bounds.getY();
        else if (bounds.getBottom() > y + viewport.getHeight())
            y = bounds.getBottom() - viewport.getHeight();
        viewport.setViewPosition(0, y);
        row.grabKeyboardFocus();
        row.repaint();
    }
    void libraryRows(const juce::var &state, const juce::var &parent, int depth) {
        if (depth > 32)
            return;
        std::vector<juce::var> folders, items;
        for (const auto &value : *state["folders"].getArray())
            if (value["parent"] == parent)
                folders.push_back(value);
        for (const auto &value : *state["items"].getArray())
            if (value["folder"] == parent)
                items.push_back(live::libraryItem(state, value["id"].toString()));
        const auto order = [](const auto &a, const auto &b) {
            return a["name"].toString().compareNatural(b["name"].toString()) < 0;
        };
        std::stable_sort(folders.begin(), folders.end(), order);
        std::stable_sort(items.begin(), items.end(), order);
        const auto query = search.getText().trim();
        for (const auto &folder : folders) {
            const auto start = rows.size();
            rows.push_back({folder["id"].toString(), {}, folder["name"].toString(), depth, true, false});
            if (query.isNotEmpty() || !collapsed.contains(folder["id"].toString()))
                libraryRows(state, folder["id"], depth + 1);
            if (query.isNotEmpty() && rows.size() == start + 1 && !folder["name"].toString().containsIgnoreCase(query))
                rows.pop_back();
        }
        for (const auto &item : items)
            if (query.isEmpty() || item["name"].toString().containsIgnoreCase(query))
                rows.push_back(
                    {item["id"].toString(), item["id"].toString(), item["name"].toString(), depth, false, false});
    }
    void rebuild() {
        const auto state = data();
        const bool hadFocus = body.isParentOf(juce::Component::getCurrentlyFocusedComponent());
        const auto selected = focusIndex >= 0 && focusIndex < static_cast<int>(rows.size())
                                  ? rows[static_cast<std::size_t>(focusIndex)].id
                                  : juce::String();
        const int oldScroll = viewport.getViewPositionY();
        cards.clear();
        rows.clear();
        if (state.isObject()) {
            if (browser)
                libraryRows(state, {}, 0);
            else {
                const auto entries = live::activeSetlist(state)["entries"];
                if (const auto *values = entries.getArray())
                    for (const auto &entry : *values) {
                        const auto id = entry["itemId"].toString();
                        rows.push_back({entry["id"].toString(), id,
                                        entry["type"].toString() == "divider"
                                            ? entry["name"].toString()
                                            : live::find(state, "items", id)["name"].toString(),
                                        0, false, entry["type"].toString() == "divider"});
                    }
            }
        }
        for (const auto &row : rows) {
            auto card = std::make_unique<Card>(*this, row);
            body.addAndMakeVisible(*card);
            cards.push_back(std::move(card));
        }
        syncing = true;
        if (!browser) {
            setlists.clear(juce::dontSendNotification);
            if (const auto *lists = state["setlists"].getArray())
                for (int i = 0; i < lists->size(); ++i) {
                    setlists.addItem((*lists)[i]["name"].toString(), i + 1);
                    if ((*lists)[i]["id"] == state["activeSetlist"])
                        setlists.setSelectedId(i + 1, juce::dontSendNotification);
                }
            organizerButton.setToggleState(static_cast<bool>(state["performance"]["showOrganizer"]),
                                           juce::dontSendNotification);
            organizerButton.setEnabled(state.isObject());
            menuButton.setEnabled(state.isObject());
        }
        syncing = false;
        revision = owner.liveUiRevision;
        focusIndex = indexOf(selected);
        layoutRows();
        viewport.setViewPosition(0, oldScroll);
        if (hadFocus && focusIndex >= 0)
            focusRow();
        repaint();
    }
    void layoutRows() {
        int y = 0;
        const int width = juce::jmax(1, viewport.getWidth() - viewport.getScrollBarThickness());
        for (auto &row : cards) {
            const int h = juce::roundToInt(row->preferredHeight());
            row->setBounds(0, y, width, h);
            y += h + 4;
        }
        body.setSize(width, juce::jmax(y, viewport.getHeight()));
    }
    void handle(int choice, const RowData &row) {
        if (choice == 40 || choice == 41) {
            juce::Component::SafePointer<Desktop> safe(&owner);
            const auto list = live::activeSetlist(data());
            const auto setId = list["id"].toString();
            promptLiveContext(owner, choice == 40 ? "New setlist" : "Rename setlist",
                              choice == 40 ? juce::String() : list["name"].toString(),
                              [safe, choice, setId](const auto &text) {
                                  if (safe)
                                      safe->editLiveInterface("Setlist", [&](auto &state) {
                                          if (choice == 41)
                                              live::put(live::require(state, "setlists", setId), "name", text.trim());
                                          else {
                                              auto set = live::object();
                                              const auto id = live::freshId();
                                              live::put(set, "id", id);
                                              live::put(set, "name", text.trim());
                                              live::put(set, "entries", juce::Array<juce::var>());
                                              live::array(state, "setlists").add(set);
                                              live::put(state, "activeSetlist", id);
                                          }
                                      });
                              });
            return;
        }
        if (choice == 42) {
            owner.editLiveInterface("Remove setlist", [](auto &state) {
                auto &sets = live::array(state, "setlists");
                if (sets.size() < 2)
                    return;
                for (int i = 0; i < sets.size(); ++i)
                    if (sets[i]["id"] == state["activeSetlist"]) {
                        sets.remove(i);
                        break;
                    }
                live::put(state, "activeSetlist", sets[0]["id"]);
            });
            return;
        }
        if (choice == 1)
            showItemContext(row, *this);
        else if (choice == 2)
            owner.editLiveInterface("Save for All", [&](auto &state) { live::saveItem(state, row.itemId); });
        else if (choice == 3) {
            juce::Component::SafePointer<Desktop> safe(&owner);
            const auto id = row.itemId;
            owner.confirmDiscard(
                [safe, id] {
                    if (safe)
                        safe->choose(
                            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                            [safe, id](const auto &file) {
                                if (safe)
                                    safe->bindLiveItem(id, file);
                            },
                            "*.mrsproject");
                },
                true);
        } else if (choice == 4)
            owner.editLiveInterface("Add to Setlist", [&](auto &state) { live::insertEntry(state, row.itemId); });
        else if (choice == 5)
            rename(row);
        else if (choice == 6)
            owner.editLiveInterface("Remove Setlist entry", [&](auto &state) {
                auto &entries = *live::activeEntries(state).getArray();
                for (int i = entries.size(); --i >= 0;)
                    if (entries[i]["id"].toString() == row.id)
                        entries.remove(i);
            });
        else if (choice == 10 || choice == 11 || choice == 12) {
            juce::Component::SafePointer<Desktop> safe(&owner);
            const auto folder = row.folder ? juce::var(row.id) : juce::var();
            promptLiveContext(owner,
                              choice == 10   ? "New item"
                              : choice == 11 ? "New folder"
                                             : "New divider",
                              "", [safe, choice, folder, target = row.id](const auto &text) {
                                  if (!safe)
                                      return;
                                  safe->editLiveInterface("Create show entry", [&](auto &state) {
                                      if (choice == 10) {
                                          const auto id = live::addItem(state, text, folder);
                                          live::insertEntry(state, id);
                                      } else if (choice == 11) {
                                          auto value = live::object();
                                          live::put(value, "id", live::freshId());
                                          live::put(value, "name", text.trim());
                                          live::put(value, "parent", folder);
                                          live::array(state, "folders").add(value);
                                      } else {
                                          auto value = live::object();
                                          live::put(value, "id", live::freshId());
                                          live::put(value, "type", "divider");
                                          live::put(value, "name", text.trim());
                                          auto &entries = *live::activeEntries(state).getArray();
                                          int at = entries.size();
                                          for (int i = 0; i < entries.size(); ++i)
                                              if (entries[i]["id"].toString() == target)
                                                  at = i;
                                          entries.insert(at, value);
                                      }
                                  });
                              });
        } else if (choice == 13)
            openLiveCardEditor(owner, defaults);
        else if (choice == 30 || choice == 31) {
            owner.editLiveInterface("Move setlist entry", [&](auto &state) {
                auto &entries = *live::activeEntries(state).getArray();
                for (int i = 0; i < entries.size(); ++i)
                    if (entries[i]["id"].toString() == row.id) {
                        const int target = juce::jlimit(0, entries.size() - 1, i + (choice == 30 ? -1 : 1));
                        entries.move(i, target);
                        break;
                    }
            });
        } else if (choice == 20)
            owner.undoLiveInterface();
        else if (choice == 21)
            owner.undoLiveInterface(true);
    }
    Desktop &owner;
    bool browser{};
    juce::var defaults;
    juce::Viewport viewport;
    juce::Component body;
    juce::TextEditor search;
    juce::ComboBox setlists;
    juce::TextButton browserButton{"Browser"}, organizerButton{"Org"}, menuButton{"..."};
    std::vector<RowData> rows;
    std::vector<std::unique_ptr<Card>> cards;
    std::set<juce::String> collapsed;
    std::uint64_t revision{~0ULL};
    int focusIndex{-1}, dropY{-1};
    bool syncing{};
    std::uint64_t pendingSelectionRevision{};
    std::optional<RowData> pendingActivation;
    juce::String pendingSetlist;
};
} // namespace ui
