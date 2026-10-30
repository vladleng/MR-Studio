#pragma once
#include "LiveContext.h"

namespace ui {
// The item-context surface already handles transformed workspaces and clamps to
// the window. Use it for flat menus as well, with an explicit event/button anchor.
class LiveActionMenu final : public juce::Component {
  public:
    explicit LiveActionMenu(const juce::PopupMenu &menu) {
        setComponentID("live-setlist-context");
        setWantsKeyboardFocus(true);
        int y = 4;
        for (juce::PopupMenu::MenuItemIterator it(menu); it.next();) {
            const auto &item = it.getItem();
            if (item.isSeparator) { separators.push_back(y + 4); y += 8; continue; }
            auto button = std::make_unique<Entry>();
            button->setButtonText(item.text); button->setEnabled(item.isEnabled);
            button->setToggleState(item.isTicked, juce::dontSendNotification);
            button->setBounds(4, y, 212, 30);
            button->onClick = [this, id = item.itemID] { if (select) select(id); };
            addAndMakeVisible(*button); buttons.push_back(std::move(button)); y += 30;
        }
        setSize(220, y + 4);
    }
    std::function<void(int)> select;
    void paint(juce::Graphics &g) override {
        g.setColour(liveReferenceTokens().colour(ColourRole::border).withAlpha(.5f));
        for (int y : separators) g.drawHorizontalLine(y, 6.f, static_cast<float>(getWidth() - 6));
    }
    void focusFirst() { for (auto &button : buttons) if (button->isEnabled()) { button->grabKeyboardFocus(); break; } }
    bool keyPressed(const juce::KeyPress &key) override {
        if (key == juce::KeyPress::escapeKey) { if (select) select(0); return true; }
        const bool down = key == juce::KeyPress::downKey, up = key == juce::KeyPress::upKey;
        if (!down && !up) return false;
        int at = up ? 0 : -1;
        for (int i = 0; i < static_cast<int>(buttons.size()); ++i) if (buttons[static_cast<std::size_t>(i)]->hasKeyboardFocus(true)) at = i;
        for (std::size_t tries = 0; tries < buttons.size(); ++tries) {
            at = (at + (down ? 1 : -1) + static_cast<int>(buttons.size())) % static_cast<int>(buttons.size());
            auto &button = *buttons[static_cast<std::size_t>(at)];
            if (!button.isEnabled()) continue;
            button.grabKeyboardFocus();
            if (auto *viewport = findParentComponentOfClass<juce::Viewport>())
                if (!viewport->getViewArea().contains(button.getBounds())) viewport->setViewPosition(0, button.getY());
            break;
        }
        return true;
    }
  private:
    class Entry final : public juce::TextButton {
        void paintButton(juce::Graphics &g, bool hover, bool down) override {
            getLookAndFeel().drawPopupMenuItem(g, getLocalBounds(), false, isEnabled(),
                hover || down || hasKeyboardFocus(true), getToggleState(), false,
                getButtonText(), {}, nullptr, nullptr);
        }
    };
    std::vector<std::unique_ptr<Entry>> buttons;
    std::vector<int> separators;
};
inline void showLiveActionMenu(const juce::PopupMenu &menu, juce::Component &owner, juce::Rectangle<int> target,
                               std::function<void(int)> action) {
    auto content = std::make_unique<LiveActionMenu>(menu);
    juce::Component::SafePointer<LiveActionMenu> safeMenu(content.get());
    auto &box = showLiveContext(std::move(content), owner, target);
    juce::Component::SafePointer<juce::CallOutBox> safeBox(&box);
    safeMenu->select = [safeBox, action = std::move(action)](int id) {
        if (safeBox) safeBox->dismiss();
        if (id) juce::MessageManager::callAsync([action, id] { action(id); });
    };
    juce::MessageManager::callAsync([safeMenu] { if (safeMenu) safeMenu->focusFirst(); });
}
} // namespace ui
