#pragma once
#include "Desktop.h"
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
            percent.setText(juce::String(juce::roundToInt(live::taskProgress(values, taskId))), false);
            percent.setTitle("Task progress percent");
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
        void resized() override {
            auto area = getLocalBounds().reduced(1);
            state.setBounds(area.removeFromLeft(76));
            area.removeFromLeft(4);
            remove.setBounds(area.removeFromRight(24));
            add.setBounds(area.removeFromRight(24));
            percent.setBounds(area.removeFromRight(46).reduced(2, 0));
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
        juce::ComboBox state;
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
    juce::TextButton addRoot{"+ New sub-item"}, useTemplate{"Create optional template"};
    std::uint64_t observed{};
};
class LiveItemEditor final : public juce::Component {
  public:
    LiveItemEditor(Desktop &desktop, const juce::String &id) : owner(&desktop), itemId(id), organizer(desktop, id) {
        setSize(720, 620);
        setName("Item properties and organizer");
        getProperties().set("mrLiveShowEditor", true);
        theme.applyPreview(liveReferenceTokens());
        setLookAndFeel(&theme);
        const auto state = desktop.liveDocument->interfaceState;
        const auto item = live::find(state, "items", id);
        original = properties(state, id);
        name.setText(item["name"].toString(), false);
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
        for (auto *button : {&apply, &cancel, &saveAll, &addToSetlist})
            addAndMakeVisible(button);
        addAndMakeVisible(error);
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
    }
    ~LiveItemEditor() override { setLookAndFeel(nullptr); }
    std::function<void()> close;
    void paint(juce::Graphics &g) override { g.fillAll(theme.role(ColourRole::window, surface)); }
    void resized() override {
        auto area = getLocalBounds().reduced(12);
        auto first = area.removeFromTop(60);
        place(labels[0], name, first.removeFromLeft(330));
        place(labels[1], status, first.removeFromLeft(110));
        place(labels[2], priority, first.removeFromLeft(100));
        place(labels[3], progress, first);
        area.removeFromTop(4);
        organizer.setBounds(area.removeFromTop(260));
        area.removeFromTop(8);
        auto row = area.removeFromTop(58);
        place(labels[4], tempo, row.removeFromLeft(150));
        place(labels[5], meter, row.removeFromLeft(150));
        place(labels[6], key, row.removeFromLeft(150));
        place(labels[7], duration, row);
        auto actions = area.removeFromBottom(38);
        apply.setBounds(actions.removeFromRight(86).reduced(2));
        cancel.setBounds(actions.removeFromRight(86).reduced(2));
        saveAll.setBounds(actions.removeFromLeft(120).reduced(2));
        addToSetlist.setBounds(actions.reduced(2));
        error.setBounds(area.removeFromBottom(28));
        place(labels[8], notes, area);
    }

  private:
    static void place(juce::Label &label, juce::Component &field, juce::Rectangle<int> bounds) {
        label.setBounds(bounds.removeFromTop(20));
        field.setBounds(bounds.reduced(3, 0));
    }
    static juce::String properties(const juce::var &state, const juce::String &id) {
        auto result = live::object();
        const auto item = live::find(state, "items", id);
        for (const auto *field : {"name", "tempo", "meter", "key", "duration", "notes"})
            live::put(result, field, item[field]);
        live::put(result, "workflow", live::workflow(state, id));
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
            if (close)
                close();
        } else
            error.setText(owner->message, juce::dontSendNotification);
    }
    juce::Component::SafePointer<Desktop> owner;
    juce::String itemId, original;
    Theme theme;
    LiveOrganizer organizer;
    std::array<juce::Label, 9> labels;
    juce::TextEditor name, tempo, key, duration, notes, progress;
    juce::ComboBox status, priority, meter;
    juce::Label error;
    juce::TextButton apply{"Apply"}, cancel{"Cancel"}, saveAll{"Save for All"}, addToSetlist{"Add to Setlist"};
};
inline void openLiveItemEditor(Desktop &owner, const juce::String &id) {
    if (!owner.liveDocument)
        return;
    auto content = std::make_unique<LiveItemEditor>(owner, id);
    auto window = std::make_unique<EditorWindow>("Item", content.release());
    window->setOwner(owner);
    window->getProperties().set("mrLiveShowEditor", true);
    auto *editor = dynamic_cast<LiveItemEditor *>(window->getContentComponent());
    juce::Component::SafePointer<EditorWindow> safe(window.get());
    editor->close = [safe] {
        if (safe)
            safe->closeButtonPressed();
    };
    window->setResizable(true, false);
    window->setResizeLimits(660, 600, 1200, 1200);
    owner.windows.push_back(std::move(window));
}
} // namespace ui
