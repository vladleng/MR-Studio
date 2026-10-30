#pragma once
#include "Desktop.h"
#include "LiveContext.h"
#include "LiveTheme.h"

namespace ui {
class LiveOrganizer final : public juce::Component, private juce::Timer {
  public:
    LiveOrganizer(Desktop &desktop, juce::String item) : owner(&desktop), itemId(std::move(item)) {
        addAndMakeVisible(tree);
        addAndMakeVisible(addRoot);
        addAndMakeVisible(useTemplate);
        tree.setRootItemVisible(false);
        tree.setDefaultOpenness(true);
        tree.setIndentSize(12);
        addRoot.setArt(art->addTask.get());
        useTemplate.setArt(art->checklist.get());
        addRoot.setButtonText("   + New sub-item");
        useTemplate.setButtonText("   Create optional template");
        addRoot.onClick = [this] {
            edit("Add task", [&](auto &state) { live::addTask(state, itemId, {}, "New sub-item"); });
        };
        useTemplate.onClick = [this] {
            edit("Task template", [&](auto &state) {
                for (const auto &name : juce::StringArray::fromTokens(
                         juce::String::fromUTF8("Композиция|Гармония|Инструментовка|Рыба"), "|", "")) {
                    const auto taskId = live::addTask(state, itemId, {}, name);
                    if (name == juce::String::fromUTF8("Рыба")) {
                        live::addTask(state, itemId, taskId, "Drums");
                        live::addTask(state, itemId, taskId, "Bass");
                    }
                }
            });
        };
        startTimerHz(10);
        rebuild();
    }
    ~LiveOrganizer() override {
        stopTimer();
        tree.setRootItem(nullptr);
    }
    void resized() override {
        auto area = getLocalBounds();
        auto footer = area.removeFromBottom(30);
        addRoot.setBounds(footer.removeFromLeft(140).reduced(2));
        useTemplate.setBounds(footer.reduced(2));
        tree.setBounds(area);
    }

  private:
    struct Node final : juce::TreeViewItem {
        LiveOrganizer &host;
        juce::String taskId;
        bool root{};
        Node(LiveOrganizer &source, juce::String id, bool isRoot = false)
            : host(source), taskId(std::move(id)), root(isRoot) {}
        bool mightContainSubItems() override { return getNumSubItems() > 0; }
        juce::String getUniqueName() const override { return root ? "organizer-root" : taskId; }
        int getItemHeight() const override { return 32; }
        std::unique_ptr<juce::Component> createItemComponent() override {
            return root ? nullptr : std::make_unique<TaskRow>(host, taskId);
        }
    };
    class TaskRow final : public juce::Component {
      public:
        TaskRow(LiveOrganizer &source, juce::String id) : host(source), taskId(std::move(id)) {
            for (auto *component : std::array<juce::Component *, 5>{&state, &name, &percent, &add, &remove})
                addAndMakeVisible(component);
            const auto doc = host.data();
            const auto values = live::tasks(doc, host.itemId);
            juce::var value;
            if (const auto *entries = values.getArray())
                for (const auto &entry : *entries)
                    if (entry["id"].toString() == taskId)
                        value = entry;
            state.addItemList({"Done", "Wait", "Todo", "Wip"}, 1);
            state.setSelectedItemIndex(
                juce::StringArray{"Done", "Wait", "Todo", "Wip"}.indexOf(value["status"].toString()),
                juce::dontSendNotification);
            name.setText(value["title"].toString(), false);
            name.setInputRestrictions(120);
            name.setTitle("Task title");
            name.getProperties().set("liveTableCell", true);
            name.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
            name.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            name.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
            percent.setText(juce::String(juce::roundToInt(live::taskProgress(values, taskId))), false);
            percent.setTitle("Task progress percent");
            configureLiveProgress(percent);
            bool hasChildren = false;
            if (const auto *entries = values.getArray())
                for (const auto &entry : *entries)
                    hasChildren = hasChildren || entry["parent"].toString() == taskId;
            percent.setEnabled(!hasChildren && value["status"].toString() != "Done");
            state.onChange = [this] { change("Task status", "status", state.getText()); };
            name.onTextChange = [this] { if (name.getText().trim().isNotEmpty()) changeName(); };
            name.onReturnKey = [this] { changeName(); };
            name.onFocusLost = [this] { changeName(); };
            percent.onTextChange = [this] { if (percent.getText().trim().isNotEmpty()) changeProgress(); };
            percent.onReturnKey = [this] { changeProgress(); };
            percent.onFocusLost = [this] { changeProgress(); };
            add.setTitle("Add child task");
            remove.setTitle("Remove task and children");
            add.onClick = [this] {
                host.edit("Add child task",
                          [&](auto &stateData) { live::addTask(stateData, host.itemId, taskId, "New sub-item"); });
            };
            remove.onClick = [this] {
                host.edit("Remove task", [&](auto &stateData) { live::removeTask(stateData, host.itemId, taskId); });
            };
        }
        void paint(juce::Graphics &g) override {
            g.setColour(juce::Colour(0xff343638));
            g.drawHorizontalLine(getHeight() - 1, 0.f, static_cast<float>(getWidth()));
            paintLiveProgress(g, percent.getBounds(), percent.getText().getDoubleValue(), *art);
        }
        void resized() override {
            auto area = getLocalBounds().reduced(1);
            state.setBounds(area.removeFromLeft(108));
            area.removeFromLeft(4);
            percent.setBounds(area.removeFromRight(128).reduced(2, 0));
            remove.setBounds(area.removeFromRight(24));
            add.setBounds(area.removeFromRight(24));
            name.setBounds(area.reduced(2, 0));
        }

      private:
        void change(const juce::String &label, const juce::Identifier &field, const juce::var &value) {
            host.edit(label + " " + taskId, [&](auto &stateData) {
                auto &tasksMap = live::map(live::performance(stateData), "itemTasks");
                auto &task = live::require(tasksMap, host.itemId, taskId);
                if (task[field] != value)
                    live::put(task, field, value);
            }, field == juce::Identifier("title"));
        }
        void changeName() {
            const auto text = name.getText().trim();
            if (text.isNotEmpty())
                change("Task title", "title", text);
        }
        void changeProgress() {
            host.edit("Task progress " + taskId, [&](auto &stateData) {
                auto &task = live::require(live::map(live::performance(stateData), "itemTasks"), host.itemId, taskId);
                const auto text = percent.getText().trim().toStdString();
                std::size_t used = 0;
                const double value = std::stod(text, &used);
                if (used != text.size() || !std::isfinite(value) || value < 0 || value > 100)
                    throw std::runtime_error("Progress must be 0..100");
                live::put(task, "progress", value);
            }, true);
        }
        LiveOrganizer &host;
        juce::String taskId;
        juce::SharedResourcePointer<LiveContextArt> art;
        LiveStatusBox state;
        juce::TextEditor name, percent;
        juce::TextButton add{"+"}, remove{"x"};
    };
    juce::var data() const { return owner && owner->liveDocument ? owner->liveDocument->interfaceState : juce::var(); }
    void edit(const juce::String &label, std::function<void(juce::var &)> action, bool typing = false) {
        if (owner) owner->run([&] { owner->editLiveInterface(label, std::move(action), typing); });
    }
    void append(Node &parent, const juce::var &entries, const juce::var &parentId, int depth) {
        if (depth > 8)
            return;
        if (const auto *tasksArray = entries.getArray())
            for (const auto &task : *tasksArray)
                if (task["parent"] == parentId) {
                    auto *node = new Node(*this, task["id"].toString());
                    parent.addSubItem(node);
                    append(*node, entries, task["id"], depth + 1);
                }
    }
    void rebuild() {
        auto openness = tree.getOpennessState(false);
        tree.setRootItem(nullptr);
        root = std::make_unique<Node>(*this, "", true);
        const auto entries = live::tasks(data(), itemId);
        append(*root, entries, {}, 0);
        tree.setRootItem(root.get());
        root->setOpen(true);
        if (openness)
            tree.restoreOpennessState(*openness, false);
        useTemplate.setVisible(entries.size() == 0);
        observed = owner ? owner->liveUiRevision : 0;
        taskSignature = live::canonical(entries);
    }
    void timerCallback() override {
        const auto *focused = juce::Component::getCurrentlyFocusedComponent();
        const bool typing = dynamic_cast<const juce::TextEditor *>(focused) && tree.isParentOf(focused);
        if (owner && isShowing() && !typing && observed != owner->liveUiRevision) {
            observed = owner->liveUiRevision;
            if (taskSignature != live::canonical(live::tasks(data(), itemId))) rebuild();
        }
    }
    juce::Component::SafePointer<Desktop> owner;
    juce::String itemId;
    juce::TreeView tree;
    std::unique_ptr<Node> root;
    juce::SharedResourcePointer<LiveContextArt> art;
    LiveContextAction addRoot, useTemplate;
    std::uint64_t observed{};
    juce::String taskSignature;
};
class LiveItemEditor final : public juce::Component, private juce::Timer {
  public:
    LiveItemEditor(Desktop &desktop, const juce::String &id) : owner(&desktop), itemId(id), organizer(desktop, id) {
        setName("Item context panel");
        setComponentID("live-item-context");
        getProperties().set("mrLiveShowEditor", true);
        configureLiveContext(theme, static_cast<float>(desktop.view.liveMenuOpacity));
        setLookAndFeel(&theme);
        const auto state = desktop.liveDocument->interfaceState;
        const auto item = live::find(state, "items", id);

        name.setText(item["name"].toString(), false);
        name.getProperties().set("liveTableCell", true);
        name.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        name.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        name.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        tempo.setText(item["tempo"].toString(), false);
        meter.addItemList({"2/4", "3/4", "4/4", "5/4", "6/8", "7/8", "12/8"}, 1);
        meter.setText(item["meter"].toString(), juce::dontSendNotification);
        key.setText(item["key"].toString(), false);
        duration.setText(item["duration"].toString(), false);
        notes.setMultiLine(true);
        notes.setReturnKeyStartsNewLine(true);
        notes.setText(item["notes"].toString(), false);
        observedNotes = notes.getText();
        const auto workflow = live::workflow(state, id);
        status.addItemList({"Done", "Wait", "Todo", "Wip"}, 1);
        status.setSelectedItemIndex(
            juce::StringArray{"Done", "Wait", "Todo", "Wip"}.indexOf(workflow["status"].toString()),
            juce::dontSendNotification);
        priority.addItemList({"P1", "P2", "P3", "P4"}, 1);
        priority.setSelectedItemIndex(
            juce::StringArray{"P1", "P2", "P3", "P4"}.indexOf(workflow["priority"].toString()),
            juce::dontSendNotification);
        progress.setText(juce::String(live::progress(state, id)), false);
        configureLiveProgress(progress);
        progress.setEnabled(live::tasks(state, id).size() == 0);
        const std::array<juce::Component *, 10> fields{&name,  &status, &priority, &progress, &tempo,
                                                       &meter, &key,    &duration, &notes,    &organizer};
        for (auto *component : fields)
            addAndMakeVisible(component);
        const juce::StringArray captions{"Name",  "Status", "Priority",          "Progress %", "Tempo",
                                         "Meter", "Key",    "Duration, seconds", "Notes"};
        for (int i = 0; i < captions.size(); ++i) {
            labels[static_cast<std::size_t>(i)].setText(captions[i], juce::dontSendNotification);
            addAndMakeVisible(labels[static_cast<std::size_t>(i)]);
            fields[static_cast<std::size_t>(i)]->setTitle(captions[i]);
        }
        addAndMakeVisible(saveAll);
        // Compact context: actions duplicated by direct fields or toolbar are omitted.
        const juce::StringArray actionNames{juce::String::fromUTF8("Расположение элементов..."),
                                            juce::String::fromUTF8("Убрать из setlist"),
                                            juce::String::fromUTF8("Создать разделитель")};
        for (int i = 0; i < actionNames.size(); ++i) {
            auto button = std::make_unique<LiveContextAction>();
            button->setButtonText(actionNames[i]);
            button->setArt(i == 0 ? art->expand.get() : nullptr);
            button->onClick = [this, i] {
                const std::array<int, 3> commands{13, 6, 12};
                if (action) action(commands[static_cast<std::size_t>(i)]);
            };
            addAndMakeVisible(*button);
            actions.push_back(std::move(button));
        }
        labels[0].setText("Title", juce::dontSendNotification);
        labels[3].setText("Progress", juce::dontSendNotification);
        addAndMakeVisible(error);
        cancel.onClick = [this] {
            if (close)
                close();
        };
        saveAll.onClick = [this] {
            if (owner)
                owner->run([&] {
                    owner->editLiveInterface("Save item for all",
                                             [&](auto &stateData) { live::saveItem(stateData, itemId); });
                });
        };
        name.setComponentID("live-item-name");
        notes.setComponentID("live-item-notes");
        tempo.setComponentID("live-item-tempo");
        notes.setInputRestrictions(4000);
        name.onTextChange = [this] { if (name.getText().trim().isNotEmpty()) commitRootName(); };
        for (auto *field : {&tempo, &key, &duration, &notes}) {
            field->onTextChange = [this, field] {
                if (field == &notes) updateNotesHeight();
                commitProperty(*field);
            };
            field->onFocusLost = [this, field] { commitProperty(*field); };
        }
        meter.onChange = [this] { commitMeter(); };
        priority.onChange = [this] { commitRootWorkflow(); };
        name.onReturnKey = [this] { commitRootName(); };
        name.onFocusLost = name.onReturnKey;
        status.onChange = [this] { commitRootWorkflow(); };
        progress.onTextChange = [this] { if (progress.getText().trim().isNotEmpty()) commitRootWorkflow(); };
        progress.onReturnKey = [this] { commitRootWorkflow(); };
        progress.onFocusLost = progress.onReturnKey;
        startTimerHz(10);
        setActions({}, false);
        setSize(600, 524);
    }
    ~LiveItemEditor() override {
        stopTimer();
        name.onFocusLost = {};
        progress.onFocusLost = {};
        for (auto *field : {&tempo, &key, &duration, &notes}) field->onFocusLost = {};
        setLookAndFeel(nullptr);
    }
    std::function<void()> close;
    void setActions(std::function<void(int)> handler, bool entryActions) {
        action = std::move(handler);
        for (const int i : {0, 1, 2})
            actions[static_cast<std::size_t>(i)]->setEnabled(static_cast<bool>(action) &&
                                                             (i != 1 || entryActions));
    }
    void paint(juce::Graphics &g) override {
        g.setColour(theme.role(ColourRole::border, outline));
        g.drawHorizontalLine(28, 8.f, static_cast<float>(getWidth() - 8));
        g.drawHorizontalLine(63, 8.f, static_cast<float>(getWidth() - 8));
        paintLiveProgress(g, progress.getBounds(), progress.getText().getDoubleValue(), *art);
        art->organizer->drawWithin(g, {8.f, 6.f, 14.f, 14.f}, juce::RectanglePlacement::centred, 1.f);
    }
    void resized() override {
        auto area = getLocalBounds().reduced(8);
        auto headings = area.removeFromTop(24);
        labels[1].setBounds(headings.removeFromLeft(98).withTrimmedLeft(16));
        labels[3].setBounds(headings.removeFromRight(128));
        labels[0].setBounds(headings);
        auto rootRow = area.removeFromTop(32);
        status.setBounds(rootRow.removeFromLeft(94));
        rootRow.removeFromLeft(4);
        progress.setBounds(rootRow.removeFromRight(128).reduced(2, 0));
        name.setBounds(rootRow.reduced(2, 0));
        organizer.setBounds(area.removeFromTop(180));
        place(labels[8], notes, area.removeFromTop(notesBlockHeight));
        const int third = area.getWidth() / 3;
        auto first = area.removeFromTop(50);
        place(labels[2], priority, first.removeFromLeft(third));
        place(labels[4], tempo, first.removeFromLeft(third));
        place(labels[5], meter, first);
        auto second = area.removeFromTop(50);
        place(labels[6], key, second.removeFromLeft(third));
        place(labels[7], duration, second.removeFromLeft(third));
        error.setBounds(area.removeFromTop(20));
        auto buttons = area;
        const int half = buttons.getWidth() / 2;
        const std::array<juce::TextButton *, 4> cells{actions[0].get(), &saveAll,
                                                      actions[1].get(), actions[2].get()};
        for (std::size_t i = 0; i < cells.size(); ++i) {
            const int col = static_cast<int>(i % 2), row = static_cast<int>(i / 2);
            cells[i]->setBounds(buttons.getX() + col * half, buttons.getY() + row * 28,
                                (col == 0 ? half : buttons.getWidth() - half) - 3, 25);
        }
        // Resize content (not the popover) so its parent viewport scrolls when Notes is long.
        updateNotesHeight();
    }

  private:
    void commitRootName() {
        if (!owner || !owner->liveDocument)
            return;
        if (name.getText().trim().isEmpty()) {
            name.setText(live::find(owner->liveDocument->interfaceState, "items", itemId)["name"].toString(), false);
            return;
        }
        owner->run([&] {
            owner->editLiveInterface("Item title " + itemId, [&](auto &state) {
                live::put(live::require(state, "items", itemId), "name", name.getText().trim());
            }, true);
        });
    }
    void commitRootWorkflow() {
        if (!owner || !owner->liveDocument)
            return;
        owner->run([&] {
            owner->editLiveInterface("Item status / progress", [&](auto &state) {
                const auto flow = live::workflow(state, itemId);
                const int percent = live::tasks(state, itemId).size() ? static_cast<int>(flow["progress"])
                                                                      : juce::roundToInt(numeric(progress.getText()));
                live::setWorkflow(state, itemId, status.getText(), priority.getText(), percent);
            });
        });
    }
    void timerCallback() override {
        if (!owner || !owner->liveDocument)
            return;
        const auto state = owner->liveDocument->interfaceState;
        if (!live::find(state, "items", itemId).isObject()) {
            if (close)
                close();
            return;
        }
        if (observedRevision != owner->liveUiRevision) {
            observedRevision = owner->liveUiRevision;
            if (!name.hasKeyboardFocus(true))
                name.setText(live::find(state, "items", itemId)["name"].toString(), false);
            if (!status.hasKeyboardFocus(true))
                status.setText(live::workflow(state, itemId)["status"].toString(), juce::dontSendNotification);
            if (!progress.hasKeyboardFocus(true) && !live::tasks(state, itemId).size())
                progress.setText(juce::String(live::progress(state, itemId)), false);
            const auto item = live::find(state, "items", itemId);
            for (auto *field : {&tempo, &key, &duration, &notes}) {
                const char *k = field == &tempo ? "tempo" : field == &key ? "key" : field == &duration ? "duration" : "notes";
                if (field == &notes) {
                    // TextEditor posts change notifications asynchronously.
                    // Pending input wins; otherwise Undo must update even a
                    // focused Notes field before a later focus-loss callback.
                    if (notes.getText() != observedNotes) continue;
                    notes.setText(item[k].toString(), false);
                    observedNotes = notes.getText();
                    updateNotesHeight();
                } else if (!field->hasKeyboardFocus(true)) field->setText(item[k].toString(), false);
            }
            if (!meter.hasKeyboardFocus(true)) meter.setText(item["meter"].toString(), juce::dontSendNotification);
            if (!priority.hasKeyboardFocus(true)) priority.setText(live::workflow(state, itemId)["priority"].toString(), juce::dontSendNotification);
        }
        if (live::tasks(state, itemId).size()) {
            progress.setText(juce::String(live::progress(state, itemId)), false);
            progress.setEnabled(false);
        } else
            progress.setEnabled(true);
        // A TextEditor may report its new layout after a queued text change.
        // Reconcile on the UI timer as well as in onTextChange and Undo refresh.
        updateNotesHeight();
        repaint();
    }
    void updateNotesHeight() {
        if (notes.getWidth() <= 0) return;
        const auto text = notes.getText();
        int explicitLines = 1;
        for (int i = 0; i < text.length(); ++i)
            if (text[i] == '\n') ++explicitLines;
        // TextEditor can report the previous laid-out height during a text
        // notification. At minimum honour explicit line breaks immediately,
        // and use the current wrapped layout whenever it is available.
        const int lineHeight = juce::jmax(15, juce::roundToInt(
            notes.getFont().getHeight() * notes.getLineSpacing()));
        const int laidOutHeight = text.isEmpty() ? lineHeight
            : juce::jmax(notes.getTextHeight(), explicitLines * lineHeight);
        const int needed = juce::jlimit(72, 520, laidOutHeight + 36);
        if (notesBlockHeight == needed) return;
        notesBlockHeight = needed;
        const int desiredContentHeight = 524 + notesBlockHeight - 72;
        if (getHeight() != desiredContentHeight) setSize(getWidth(), desiredContentHeight);
    }
    static void place(juce::Label &label, juce::Component &field, juce::Rectangle<int> bounds) {
        label.setBounds(bounds.removeFromTop(20));
        field.setBounds(bounds.reduced(3, 0));
    }
    static double numeric(const juce::String &text) {
        const auto bytes = text.trim().toStdString();
        std::size_t used = 0;
        const auto value = std::stod(bytes, &used);
        if (used != bytes.size() || !std::isfinite(value))
            throw std::runtime_error("Enter a finite number");
        return value;
    }
    void commitProperty(juce::TextEditor &field) {
        if (!owner || !owner->liveDocument) return;
        // Focus changes and queued TextEditor notifications can arrive after
        // show Undo, before the timer has mirrored the restored model. Only
        // newly typed text may commit; an unchanged view must not redo Undo.
        if (&field == &notes && field.getText() == observedNotes) return;
        try {
            const char *keyName = &field == &tempo ? "tempo" : &field == &duration ? "duration" :
                                  &field == &key ? "key" : "notes";
            const auto value = (&field == &tempo || &field == &duration)
                                   ? juce::var(numeric(field.getText())) : juce::var(field.getText());
            // JSON distinguishes 100 from 100.0. Merely leaving a numeric
            // editor must not convert its stored type and insert an Undo step.
            if (&field == &tempo || &field == &duration) {
                const auto previous = live::find(owner->liveDocument->interfaceState, "items", itemId)[keyName];
                if (live::number(previous) && static_cast<double>(previous) == static_cast<double>(value)) {
                    error.setText({}, juce::dontSendNotification);
                    return;
                }
            }
            if (&field == &notes) {
                owner->setLiveItemNotes(itemId, field.getText());
                observedNotes = field.getText();
            }
            else owner->editLiveInterface("Item " + itemId + " " + juce::String(keyName), [&](auto &state) {
                live::put(live::require(state, "items", itemId), keyName, value);
            }, true);
            error.setText({}, juce::dontSendNotification);
        } catch (const std::exception &e) {
            error.setText(juce::String::fromUTF8(e.what()), juce::dontSendNotification);
        }
    }
    void commitMeter() {
        if (owner) owner->run([&] { owner->editLiveInterface("Item meter", [&](auto &state) {
            live::put(live::require(state, "items", itemId), "meter", meter.getText());
        }); });
    }
    juce::Component::SafePointer<Desktop> owner;
    juce::String itemId;
    std::function<void(int)> action;
    juce::String observedNotes;
    int notesBlockHeight{72};
    Theme theme;
    LiveOrganizer organizer;
    std::array<juce::Label, 9> labels;
    juce::TextEditor name, tempo, key, duration, notes, progress;
    LiveStatusBox status;
    juce::ComboBox priority, meter;
    juce::Label error;
    juce::SharedResourcePointer<LiveContextArt> art;
    std::uint64_t observedRevision{~std::uint64_t{0}};
    std::vector<std::unique_ptr<LiveContextAction>> actions;
    juce::TextButton cancel{"Cancel"}, saveAll{"Save for All"};
};
inline void openLiveItemEditor(Desktop &owner, const juce::String &id, juce::Component *anchor = nullptr,
                               juce::Rectangle<int> point = {}, std::function<void(int)> action = {},
                               bool entryActions = false) {
    if (!owner.liveDocument)
        return;
    if (!anchor)
        anchor = &owner;
    if (point.isEmpty())
        point = liveContextTarget(juce::Desktop::getMousePosition());
    auto content = std::make_unique<LiveItemEditor>(owner, id);
    content->setActions(std::move(action), entryActions);
    auto *editor = content.get();
    auto &box = showLiveContext(std::move(content), owner, point);
    juce::Component::SafePointer<juce::CallOutBox> safe(&box);
    editor->close = [safe] {
        if (safe)
            safe->dismiss();
    };
}
} // namespace ui
