#pragma once
#include "LiveLayout.h"
#include "LiveTheme.h"

namespace ui {
inline void configureLiveContext(Theme &theme, float opacity = .9f) {
    theme.applyPreview(liveReferenceTokens());
    theme.useLiveContextStyle(true);
    theme.setPopupOpacity(opacity);
}
// Manager assets are geometry/glyphs only. Native controls supply the actual
// values.
struct LiveContextArt {
    std::array<std::unique_ptr<juce::Drawable>, 4> pills, glyphs;
    std::unique_ptr<juce::Drawable> track, organizer, addTask, checklist, expand;
    LiveContextArt() {
        const std::array<juce::uint32, 4> colours{0xff444541, 0xff725a23, 0xff355f83, 0xff285e4d};
        const std::array<const char *, 4> names{"todo_glyph.svg", "wait_glyph.svg", "wip_glyph.svg", "done_glyph.svg"};
        for (std::size_t i = 0; i < 4; ++i) {
            pills[i] = LiveLayout::drawable("pill_base.svg");
            pills[i]->replaceColour(juce::Colour(0xff444541), juce::Colour(colours[i]));
            glyphs[i] = LiveLayout::drawable(names[i]);
        }
        track = LiveLayout::drawable("track.svg");
        organizer = LiveLayout::drawable("organizer.svg");
        addTask = LiveLayout::drawable("add_task.svg");
        checklist = LiveLayout::drawable("checklist.svg");
        expand = LiveLayout::drawable("expand.svg");
    }
};
class LiveStatusBox final : public juce::ComboBox {
  public:
    LiveStatusBox() { setColour(juce::ComboBox::textColourId, juce::Colour(0xffdae5f4)); }
    void paint(juce::Graphics &g) override {
        const auto value = getText();
        const std::size_t i = value == "Wait" ? 1u : value == "Wip" ? 2u : value == "Done" ? 3u : 0u;
        const auto bounds = getLocalBounds().toFloat().reduced(1.f, 4.f);
        art->pills[i]->drawWithin(g, bounds, juce::RectanglePlacement::stretchToFit, 1.f);
        art->glyphs[i]->drawWithin(g, {bounds.getX() + 4.f, bounds.getCentreY() - 6.f, 12.f, 12.f},
                                   juce::RectanglePlacement::centred, 1.f);
        g.setColour(juce::Colour(0xffdae5f4));
        juce::Path arrow;
        const auto x = bounds.getRight() - 8.f, y = bounds.getCentreY();
        arrow.startNewSubPath(x - 3.f, y - 1.f);
        arrow.lineTo(x, y + 2.f);
        arrow.lineTo(x + 3.f, y - 1.f);
        g.strokePath(arrow, juce::PathStrokeType(1.f));
    }
    void resized() override {
        juce::ComboBox::resized();
        for (auto *child : getChildren())
            if (auto *valueLabel = dynamic_cast<juce::Label *>(child))
                valueLabel->setBounds(19, 0, juce::jmax(0, getWidth() - 35), getHeight());
    }

  private:
    juce::SharedResourcePointer<LiveContextArt> art;
};
inline void paintLiveProgress(juce::Graphics &g, juce::Rectangle<int> bounds, double percent, LiveContextArt &art) {
    auto bar =
        bounds.toFloat().reduced(4.f, 0.f).withWidth(juce::jmax(1.f, static_cast<float>(bounds.getWidth()) - 34.f));
    bar = bar.withY(bar.getCentreY() - 2.f).withHeight(4.f);
    art.track->drawWithin(g, bar, juce::RectanglePlacement::stretchToFit, 1.f);
    g.setColour(juce::Colour(0xff59ad87));
    bar.setWidth(bar.getWidth() * static_cast<float>(juce::jlimit(0., 100., percent)) / 100.f);
    g.fillRoundedRectangle(bar, 2.f);
}
inline void configureLiveProgress(juce::TextEditor &editor) {
    editor.getProperties().set("liveTableCell", true);
    editor.setJustification(juce::Justification::centredRight);
    editor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    editor.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
}
class LiveContextViewport final : public juce::Component {
  public:
    LiveContextViewport(std::unique_ptr<juce::Component> content, int width, int height, float opacity) {
        configureLiveContext(theme, opacity);
        setLookAndFeel(&theme);
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(content.release(), true);
        viewport.setScrollBarsShown(true, false);
        setSize(width, height);
    }
    ~LiveContextViewport() override { setLookAndFeel(nullptr); }
    Theme &contextTheme() { return theme; }
    void resized() override {
        viewport.setBounds(getLocalBounds());
        auto *content = viewport.getViewedComponent();
        content->setSize(viewport.getMaximumVisibleWidth(), content->getHeight());
    }

  private:
    Theme theme;
    juce::Viewport viewport;
};
inline juce::Rectangle<int> liveContextBounds(juce::Point<int> pointer, juce::Point<int> size,
                                              juce::Rectangle<int> area) {
    const int width = juce::jmin(size.x, area.getWidth());
    const int height = juce::jmin(size.y, area.getHeight());
    return {juce::jlimit(area.getX(), area.getRight() - width, pointer.x + 2),
            juce::jlimit(area.getY(), area.getBottom() - height, pointer.y + 2), width, height};
}
// Rectangle(Point, Point) means two corners in JUCE, not position and size.
inline juce::Rectangle<int> liveContextTarget(juce::Point<int> screen) {
    return {screen.x, screen.y, 1, 1};
}
inline juce::PopupMenu::Options liveMenuOptions(juce::Component &owner) {
    return juce::PopupMenu::Options()
        .withTargetComponent(&owner)
        .withTargetScreenArea(liveContextTarget(juce::Desktop::getMousePosition()))
        .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards);
}
inline juce::CallOutBox &showLiveContext(std::unique_ptr<juce::Component> content, juce::Component &owner,
                                         juce::Rectangle<int> target) {
    auto *parent = owner.getTopLevelComponent();
    auto *style = &owner;
    for (auto *child : owner.getChildren())
        if (child->getComponentID() == "mr-live-workspace")
            style = child;
    const int width = juce::jmin(content->getWidth(), juce::jmax(200, parent->getWidth() - 32));
    const int height = juce::jmin(content->getHeight(), juce::jmax(180, parent->getHeight() - 32));
    auto *sourceTheme = dynamic_cast<Theme *>(&style->getLookAndFeel());
    auto viewport = std::make_unique<LiveContextViewport>(std::move(content), width, height,
                                                          sourceTheme ? sourceTheme->popupOpacity() : .9f);
    auto *contextTheme = &viewport->contextTheme();
    auto &box =
        juce::CallOutBox::launchAsynchronously(std::move(viewport), parent->getLocalArea(nullptr, target), parent);
    box.setLookAndFeel(contextTheme);
    box.setArrowSize(0.f);
    box.resized();
    box.setTopLeftPosition(liveContextBounds(parent->getLocalPoint(nullptr, target.getTopLeft()),
                                             {box.getWidth(), box.getHeight()}, parent->getLocalBounds()).getPosition());
    box.getProperties().set("mrLiveContext", true);
    box.setDismissalMouseClicksAreAlwaysConsumed(true);
    return box;
}
class LiveContextPrompt final : public juce::Component {
  public:
    LiveContextPrompt(const juce::String &title, const juce::String &initial) {
        label.setText(title, juce::dontSendNotification);
        field.setText(initial, false);
        for (auto *c : std::array<juce::Component *, 4>{&label, &field, &apply, &error})
            addAndMakeVisible(c);
        apply.setButtonText(juce::String::fromUTF8("Применить"));
        apply.onClick = [this] {
            if (commit)
                commit(field.getText());
        };
        field.onReturnKey = apply.onClick;
        setSize(340, 120);
    }
    void resized() override {
        auto area = getLocalBounds().reduced(8);
        label.setBounds(area.removeFromTop(22));
        field.setBounds(area.removeFromTop(30));
        error.setBounds(area.removeFromTop(22));
        apply.setBounds(area.removeFromRight(110));
    }
    std::function<void(const juce::String &)> commit;
    juce::Label error;

  private:
    juce::Label label;
    juce::TextEditor field;
    juce::TextButton apply;
};
template <typename Owner, typename Commit>
void promptLiveContext(Owner &owner, const juce::String &title, const juce::String &initial, Commit commit) {
    auto content = std::make_unique<LiveContextPrompt>(title, initial);
    auto *prompt = content.get();
    auto &box = showLiveContext(std::move(content), owner, liveContextTarget(juce::Desktop::getMousePosition()));
    juce::Component::SafePointer<juce::CallOutBox> popup(&box);
    juce::Component::SafePointer<Owner> safe(&owner);
    juce::Component::SafePointer<LiveContextPrompt> promptPanel(prompt);
    prompt->commit = [safe, popup, promptPanel, commit](const juce::String &text) {
        if (!safe)
            return;
        bool success = false;
        safe->run([&] {
            commit(text);
            success = true;
        });
        if (success) {
            if (popup)
                popup->dismiss();
        } else if (promptPanel)
            promptPanel->error.setText(safe->message, juce::dontSendNotification);
    };
}
class LiveContextAction final : public juce::TextButton {
  public:
    void setArt(juce::Drawable *value) { glyph = value; }
    void paintButton(juce::Graphics &g, bool hover, bool down) override {
        juce::TextButton::paintButton(g, hover, down);
        if (glyph)
            glyph->drawWithin(g, {4.f, static_cast<float>(getHeight()) / 2.f - 7.f, 14.f, 14.f},
                              juce::RectanglePlacement::centred, isEnabled() ? 1.f : .4f);
    }

  private:
    juce::Drawable *glyph{};
};
} // namespace ui
