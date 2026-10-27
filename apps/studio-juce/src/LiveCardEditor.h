#pragma once
#include "Desktop.h"
#include "LiveContext.h"
#include "LiveTheme.h"

namespace ui {
class LiveCardEditor final : public juce::Component {
  public:
    LiveCardEditor(Desktop &desktop, juce::var fallback)
        : owner(&desktop), defaults(std::move(fallback)), stage(*this) {
        configureLiveContext(theme, static_cast<float>(desktop.view.liveMenuOpacity));
        setLookAndFeel(&theme);
        setSize(620, 360);
        setName("Item button layout");
        draft = desktop.liveDocument->interfaceState["performance"]["itemCardLayout"].clone();
        if (!draft.isObject())
            draft = defaults.clone();
        addAndMakeVisible(stage);
        addAndMakeVisible(fields);
        fields.addItemList({"Name", "Metadata", "Progress", "Priority", "Status", "Saved dot"}, 1);
        fields.setSelectedId(1, juce::dontSendNotification);
        fields.onChange = [this] {
            selected = fields.getSelectedItemIndex();
            sync();
        };
        const juce::StringArray captions{"Height, px", "X, %", "Y, px", "Width, %", "Height, px"};
        for (std::size_t i = 0; i < values.size(); ++i) {
            addAndMakeVisible(values[i]);
            addAndMakeVisible(labels[i]);
            labels[i].setText(captions[static_cast<int>(i)], juce::dontSendNotification);
            values[i].setTitle(captions[static_cast<int>(i)]);
            values[i].onReturnKey = [this, i] { applyNumber(i); };
            values[i].onFocusLost = [this, i] { applyNumber(i); };
        }
        for (auto *button : std::array<juce::Component *, 4>{&visible, &reset, &save, &cancel})
            addAndMakeVisible(button);
        addAndMakeVisible(error);
        visible.onClick = [this] {
            if (selected != 0) {
                auto &box = selectedField();
                live::put(box, "visible", visible.getToggleState());
                stage.repaint();
            }
        };
        reset.onClick = [this] {
            draft = defaults.clone();
            sync();
        };
        cancel.onClick = [this] {
            if (close)
                close();
        };
        save.onClick = [this] {
            if (!owner)
                return;
            for (std::size_t i = 0; i < values.size(); ++i)
                applyNumber(i);
            bool success = false;
            owner->run([&] {
                live::validateLayout(draft);
                owner->editLiveInterface("Item button layout", [&](auto &stateData) {
                    live::put(live::performance(stateData), "itemCardLayout", draft.clone());
                });
                success = true;
            });
            if (success) {
                if (close)
                    close();
            } else
                error.setText(owner->message, juce::dontSendNotification);
        };
        sync();
    }
    ~LiveCardEditor() override { setLookAndFeel(nullptr); }
    std::function<void()> close;
    void paint(juce::Graphics &) override {}
    void resized() override {
        auto area = getLocalBounds().reduced(12);
        fields.setBounds(area.removeFromTop(28));
        area.removeFromTop(10);
        stage.setBounds(area.removeFromTop(130));
        area.removeFromTop(8);
        auto row = area.removeFromTop(54);
        for (std::size_t i = 0; i < values.size(); ++i) {
            auto cell = row.removeFromLeft(98);
            labels[i].setBounds(cell.removeFromTop(22));
            values[i].setBounds(cell.reduced(2));
        }
        visible.setBounds(row.reduced(2));
        error.setBounds(area.removeFromTop(28));
        auto actions = area.removeFromBottom(32);
        reset.setBounds(actions.removeFromLeft(100));
        cancel.setBounds(actions.removeFromRight(100));
        save.setBounds(actions.removeFromRight(100).reduced(4, 0));
    }

  private:
    static constexpr std::array<const char *, 6> keys{"name", "metadata", "progress", "priority", "status", "saved"};
    class Preview final : public juce::Component {
      public:
        explicit Preview(LiveCardEditor &source) : editor(source) {
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        }
        juce::Rectangle<float> fieldBounds(const juce::var &field) const {
            const float scale = getHeight() / static_cast<float>(editor.draft["height"]);
            return {static_cast<float>(field["x"]) * getWidth() / 100.f, static_cast<float>(field["y"]) * scale,
                    static_cast<float>(field["w"]) * getWidth() / 100.f, static_cast<float>(field["h"]) * scale};
        }
        void paint(juce::Graphics &g) override {
            g.fillAll(editor.theme.role(ColourRole::panel, surface));
            for (std::size_t i = 0; i < keys.size(); ++i) {
                const auto value = editor.draft["fields"][keys[i]];
                auto box = fieldBounds(value).reduced(.5f);
                const auto ink = editor.theme.role(ColourRole::text, primaryText)
                                     .withAlpha(static_cast<bool>(value["visible"]) ? 1.f : .3f);
                g.setColour(ink);
                g.drawRect(box, static_cast<int>(i) == editor.selected ? 2.f : .7f);
                g.setFont(11.f);
                g.drawText(keys[i], box.reduced(3), juce::Justification::centredLeft);
                g.fillRect(box.getRight() - 5, box.getBottom() - 5, 5.f, 5.f);
            }
        }
        void mouseDown(const juce::MouseEvent &event) override {
            for (int i = static_cast<int>(keys.size()); --i >= 0;)
                if (fieldBounds(editor.draft["fields"][keys[static_cast<std::size_t>(i)]]).contains(event.position)) {
                    editor.selected = i;
                    editor.fields.setSelectedItemIndex(i, juce::dontSendNotification);
                    original = editor.selectedField().clone();
                    origin = event.position;
                    const auto bounds = fieldBounds(original);
                    resizing = event.x > bounds.getRight() - 9 && event.y > bounds.getBottom() - 9;
                    editor.sync();
                    break;
                }
        }
        void mouseDrag(const juce::MouseEvent &event) override {
            if (!original.isObject())
                return;
            const double dx = (event.position.x - origin.x) * 100. / getWidth(),
                         dy = (event.position.y - origin.y) * static_cast<double>(editor.draft["height"]) / getHeight();
            auto field = original.clone();
            if (resizing) {
                live::put(field, "w", static_cast<double>(original["w"]) + dx);
                live::put(field, "h", static_cast<double>(original["h"]) + dy);
            } else {
                live::put(field, "x", static_cast<double>(original["x"]) + dx);
                live::put(field, "y", static_cast<double>(original["y"]) + dy);
            }
            editor.clamp(field);
            editor.selectedField() = field;
            editor.sync();
        }
        void mouseUp(const juce::MouseEvent &) override { original = juce::var(); }

      private:
        LiveCardEditor &editor;
        juce::var original;
        juce::Point<float> origin;
        bool resizing{};
    };
    juce::var &selectedField() {
        return live::property(live::property(draft, "fields"), keys[static_cast<std::size_t>(selected)]);
    }
    void clamp(juce::var &value) {
        const double h = draft["height"], w = juce::jlimit(2., 100., static_cast<double>(value["w"])),
                     height = juce::jlimit(4., h, static_cast<double>(value["h"]));
        live::put(value, "w", w);
        live::put(value, "h", height);
        live::put(value, "x", juce::jlimit(0., 100. - w, static_cast<double>(value["x"])));
        live::put(value, "y", juce::jlimit(0., h - height, static_cast<double>(value["y"])));
    }
    void sync() {
        syncing = true;
        values[0].setText(draft["height"].toString(), false);
        const auto field = selectedField();
        const std::array<const char *, 4> dimensions{"x", "y", "w", "h"};
        for (std::size_t i = 0; i < dimensions.size(); ++i)
            values[i + 1].setText(field[dimensions[i]].toString(), false);
        visible.setToggleState(static_cast<bool>(field["visible"]), juce::dontSendNotification);
        visible.setEnabled(selected != 0);
        syncing = false;
        stage.repaint();
    }
    void applyNumber(std::size_t index) {
        if (syncing)
            return;
        const auto text = values[index].getText().trim();
        if (text.isEmpty())
            return;
        const double value = text.getDoubleValue();
        if (!std::isfinite(value))
            return;
        if (index == 0) {
            live::put(draft, "height", juce::jlimit(48., 240., value));
            for (const auto *key : keys)
                clamp(live::property(live::property(draft, "fields"), key));
        } else {
            const std::array<const char *, 4> dimensions{"x", "y", "w", "h"};
            live::put(selectedField(), dimensions[index - 1], value);
            clamp(selectedField());
        }
        sync();
    }
    juce::Component::SafePointer<Desktop> owner;
    juce::var defaults, draft;
    Theme theme;
    Preview stage;
    juce::ComboBox fields;
    std::array<juce::TextEditor, 5> values;
    std::array<juce::Label, 5> labels;
    juce::ToggleButton visible{"Visible"};
    juce::TextButton reset{"Reset"}, save{"Save"}, cancel{"Cancel"};
    juce::Label error;
    int selected{};
    bool syncing{};
};
inline void openLiveCardEditor(Desktop &owner, const juce::var &defaults) {
    if (!owner.liveDocument)
        return;
    auto content = std::make_unique<LiveCardEditor>(owner, defaults);
    auto *editor = content.get();
    const auto target = liveContextTarget(juce::Desktop::getMousePosition());
    auto &box = showLiveContext(std::move(content), owner, target);
    juce::Component::SafePointer<juce::CallOutBox> safe(&box);
    editor->close = [safe] {
        if (safe)
            safe->dismiss();
    };
}
} // namespace ui
