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
            name.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
            name.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            percent.setText(juce::String(juce::roundToInt(live::taskProgress(values, taskId))), false);
            percent.setTitle("Task progress percent");
            configureLiveProgress(percent);
            bool hasChildren = false;
            if (const auto *entries = values.getArray())
                for (const auto &entry : *entries)
                    hasChildren = hasChildren || entry["parent"].toString() == taskId;
            percent.setEnabled(!hasChildren && value["status"].toString() != "Done");
            state.onChange = [this] { change("Task status", "status", state.getText()); };
            name.onReturnKey = [this] { changeName(); };
            name.onFocusLost = [this] { changeName(); };
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
            paintLiveProgress(g, percent.getBounds(), percent.getText().getDoubleValue(), *art);
        }
        void resized() override {
            auto area = getLocalBounds().reduced(1);
            state.setBounds(area.removeFromLeft(108));
            area.removeFromLeft(4);
            percent.setBounds(area.removeFromRight(80).reduced(2, 0));
            remove.setBounds(area.removeFromRight(24));
            add.setBounds(area.removeFromRight(24));
            name.setBounds(area.reduced(2, 0));
        }

      private:
        void change(const juce::String &label, const juce::Identifier &field, const juce::var &value) {
            host.edit(label, [&](auto &stateData) {
                auto &tasksMap = live::map(live::performance(stateData), "itemTasks");
                auto &task = live::require(tasksMap, host.itemId, taskId);
                if (task[field] != value)
                    live::put(task, field, value);
            });
        }
        void changeName() {
            const auto text = name.getText().trim();
            if (text.isNotEmpty())
                change("Task title", "title", text);
        }
        void changeProgress() {
            host.edit("Task progress", [&](auto &stateData) {
                auto &task = live::require(live::map(live::performance(stateData), "itemTasks"), host.itemId, taskId);
                const double value = percent.getText().getDoubleValue();
                if (!std::isfinite(value) || value < 0 || value > 100)
                    throw std::runtime_error("Progress must be 0..100");
                live::put(task, "progress", value);
            });
        }
        LiveOrganizer &host;
        juce::String taskId;
        juce::SharedResourcePointer<LiveContextArt> art;
        LiveStatusBox state;
        juce::TextEditor name, percent;
        juce::TextButton add{"+"}, remove{"x"};
    };
    juce::var data() const { return owner && owner->liveDocument ? owner->liveDocument->interfaceState : juce::var(); }
    void edit(const juce::String &label, std::function<void(juce::var &)> action) {
        if (owner)
            owner->run([&] { owner->editLiveInterface(label, std::move(action)); });
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
    }
    void timerCallback() override {
        if (owner && isShowing() && observed != owner->liveUiRevision)
            rebuild();
    }
    juce::Component::SafePointer<Desktop> owner;
    juce::String itemId;
    juce::TreeView tree;
    std::unique_ptr<Node> root;
    juce::SharedResourcePointer<LiveContextArt> art;
    LiveContextAction addRoot, useTemplate;
    std::uint64_t observed{};
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
        original = properties(state, id);
        name.setText(item["name"].toString(), false);
        name.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        name.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        tempo.setText(item["tempo"].toString(), false);
        meter.addItemList({"2/4", "3/4", "4/4", "5/4", "6/8", "7/8", "12/8"}, 1);
        meter.setText(item["meter"].toString(), juce::dontSendNotification);
        key.setText(item["key"].toString(), false);
        duration.setText(item["duration"].toString(), false);
        notes.setMultiLine(true);
        notes.setText(item["notes"].toString(), false);
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
        for (auto *button : {static_cast<juce::TextButton *>(&apply), &saveAll, &addToSetlist})
            addAndMakeVisible(button);
        const juce::StringArray actionNames{"Расположение элементов...",
                                            "Priority / Progress...",
                                            "Свойства",
                                            "Убрать из setlist",
                                            "↑",
                                            "↓",
                                            "Создать разделитель",
                                            "Undo show",
                                            "Redo show"};
        for (int i = 0; i < actionNames.size(); ++i) {
            auto button = std::make_unique<LiveContextAction>();
            button->setButtonText(actionNames[i]);
            button->setArt(i == 0 ? art->expand.get() : i == 1 ? art->organizer.get() : nullptr);
            button->onClick = [this, i] {
                if (i == 1) {
                    priority.grabKeyboardFocus();
                    return;
                }
                if (i == 2) {
                    notesVisible = !notesVisible;
                    setSize(getWidth(), notesVisible ? 580 : 520);
                    return;
                }
                if (i == 7 || i == 8) {
                    if (owner)
                        owner->undoLiveInterface(i == 8);
                    return;
                }
                const std::array<int, 9> commands{13, 0, 0, 6, 30, 31, 12, 20, 21};
                if (action)
                    action(commands[static_cast<std::size_t>(i)]);
            };
            addAndMakeVisible(*button);
            actions.push_back(std::move(button));
        }
        apply.setArt(art->checklist.get());
        labels[0].setText("Title", juce::dontSendNotification);
        labels[3].setText("Progress", juce::dontSendNotification);
        addAndMakeVisible(error);
        apply.setButtonText(juce::String::fromUTF8("Применить"));
        apply.onClick = [this] { applyChanges(); };
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
        addToSetlist.onClick = [this] {
            if (owner)
                owner->run([&] {
                    owner->editLiveInterface("Add item to setlist",
                                             [&](auto &stateData) { live::insertEntry(stateData, itemId); });
                });
        };
        name.onReturnKey = [this] { commitRootName(); };
        name.onFocusLost = name.onReturnKey;
        status.onChange = [this] { commitRootWorkflow(); };
        progress.onReturnKey = [this] { commitRootWorkflow(); };
        progress.onFocusLost = progress.onReturnKey;
        startTimerHz(10);
        setActions({}, false);
        setSize(660, 520);
    }
    ~LiveItemEditor() override {
        stopTimer();
        name.onFocusLost = {};
        progress.onFocusLost = {};
        setLookAndFeel(nullptr);
    }
    std::function<void()> close;
    void setActions(std::function<void(int)> handler, bool entryActions) {
        action = std::move(handler);
        for (const int i : {0, 3, 4, 5, 6})
            actions[static_cast<std::size_t>(i)]->setEnabled(static_cast<bool>(action) &&
                                                             (i == 0 || i == 6 || entryActions));
    }
    void paint(juce::Graphics &g) override {
        g.setColour(theme.role(ColourRole::border, outline));
        g.drawHorizontalLine(28, 8.f, static_cast<float>(getWidth() - 8));
        paintLiveProgress(g, progress.getBounds(), progress.getText().getDoubleValue(), *art);
        art->organizer->drawWithin(g, {8.f, 6.f, 14.f, 14.f}, juce::RectanglePlacement::centred, 1.f);
    }
    void resized() override {
        auto area = getLocalBounds().reduced(8);
        auto headings = area.removeFromTop(24);
        labels[1].setBounds(headings.removeFromLeft(112).withTrimmedLeft(18));
        labels[3].setBounds(headings.removeFromRight(80));
        labels[0].setBounds(headings);
        auto rootRow = area.removeFromTop(32);
        status.setBounds(rootRow.removeFromLeft(108));
        rootRow.removeFromLeft(4);
        progress.setBounds(rootRow.removeFromRight(80).reduced(2, 0));
        name.setBounds(rootRow.reduced(2, 0));
        organizer.setBounds(area.removeFromTop(180));
        const int third = area.getWidth() / 3;
        auto first = area.removeFromTop(50);
        place(labels[2], priority, first.removeFromLeft(third));
        place(labels[4], tempo, first.removeFromLeft(third));
        place(labels[5], meter, first);
        auto second = area.removeFromTop(50);
        place(labels[6], key, second.removeFromLeft(third));
        place(labels[7], duration, second.removeFromLeft(third));
        apply.setBounds(second.withTrimmedTop(20).reduced(2, 0));
        labels[8].setVisible(notesVisible);
        notes.setVisible(notesVisible);
        if (notesVisible)
            place(labels[8], notes, area.removeFromTop(58));
        error.setBounds(area.removeFromTop(20));
        auto buttons = area;
        std::vector<juce::TextButton *> cells{actions[0].get(), actions[1].get(), &saveAll,         actions[2].get(),
                                              &addToSetlist,    actions[3].get(), actions[4].get(), actions[5].get(),
                                              actions[6].get(), actions[7].get(), actions[8].get()};
        for (std::size_t i = 0; i < cells.size(); ++i) {
            const int col = static_cast<int>(i % 3), row = static_cast<int>(i / 3);
            cells[i]->setBounds(buttons.getX() + col * third, buttons.getY() + row * 28, third - 3, 25);
        }
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
            owner->editLiveInterface("Item title", [&](auto &state) {
                live::put(live::require(state, "items", itemId), "name", name.getText().trim());
            });
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
                live::setWorkflow(state, itemId, status.getText(), flow["priority"].toString(), percent);
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
            actions[7]->setEnabled(owner->liveUndo.canUndo());
            actions[8]->setEnabled(owner->liveUndo.canRedo());
        }
        if (live::tasks(state, itemId).size()) {
            progress.setText(juce::String(live::progress(state, itemId)), false);
            progress.setEnabled(false);
        } else
            progress.setEnabled(true);
        repaint();
    }
    static void place(juce::Label &label, juce::Component &field, juce::Rectangle<int> bounds) {
        label.setBounds(bounds.removeFromTop(20));
        field.setBounds(bounds.reduced(3, 0));
    }
    static juce::String properties(const juce::var &state, const juce::String &id) {
        auto result = live::object();
        const auto item = live::find(state, "items", id);
        for (const auto *field : {"tempo", "meter", "key", "duration", "notes"})
            live::put(result, field, item[field]);
        live::put(result, "priority", live::workflow(state, id)["priority"]);
        return live::canonical(result);
    }
    static double numeric(const juce::String &text) {
        const auto bytes = text.trim().toStdString();
        std::size_t used = 0;
        const auto value = std::stod(bytes, &used);
        if (used != bytes.size() || !std::isfinite(value))
            throw std::runtime_error("Enter a finite number");
        return value;
    }
    void applyChanges() {
        if (!owner)
            return;
        bool success = false;
        owner->run([&] {
            if (properties(owner->liveDocument->interfaceState, itemId) != original)
                throw std::runtime_error("Item properties changed; reopen the card before applying");
            const auto bpm = numeric(tempo.getText()), seconds = numeric(duration.getText()),
                       percent = numeric(progress.getText());
            owner->editLiveInterface("Item properties", [&](auto &stateData) {
                auto &item = live::require(stateData, "items", itemId);
                live::put(item, "name", name.getText().trim());
                live::put(item, "tempo", bpm);
                live::put(item, "meter", meter.getText());
                live::put(item, "key", key.getText());
                live::put(item, "duration", seconds);
                live::put(item, "notes", notes.getText());
                const int manual = live::tasks(stateData, itemId).size()
                                       ? static_cast<int>(live::workflow(stateData, itemId)["progress"])
                                       : juce::roundToInt(percent);
                live::setWorkflow(stateData, itemId, status.getText(), priority.getText(), manual);
            });
            success = true;
        });
        if (success) {
            original = properties(owner->liveDocument->interfaceState, itemId);
            error.setText({}, juce::dontSendNotification);
        } else
            error.setText(owner->message, juce::dontSendNotification);
    }
    juce::Component::SafePointer<Desktop> owner;
    juce::String itemId, original;
    std::function<void(int)> action;
    Theme theme;
    LiveOrganizer organizer;
    std::array<juce::Label, 9> labels;
    juce::TextEditor name, tempo, key, duration, notes, progress;
    LiveStatusBox status;
    juce::ComboBox priority, meter;
    juce::Label error;
    juce::SharedResourcePointer<LiveContextArt> art;
    bool notesVisible{};
    std::uint64_t observedRevision{~std::uint64_t{0}};
    std::vector<std::unique_ptr<LiveContextAction>> actions;
    LiveContextAction apply;
    juce::TextButton cancel{"Cancel"}, saveAll{"Save for All"}, addToSetlist{"Add to Setlist"};
};
inline void openLiveItemEditor(Desktop &owner, const juce::String &id, juce::Component *anchor = nullptr,
                               juce::Rectangle<int> point = {}, std::function<void(int)> action = {},
                               bool entryActions = false) {
    if (!owner.liveDocument)
        return;
    if (!anchor)
        anchor = &owner;
    if (point.isEmpty())
        point = anchor->getScreenBounds().withSizeKeepingCentre(1, 1);
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
