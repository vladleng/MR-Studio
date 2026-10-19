#include "Desktop.h"

namespace ui {
void Desktop::setShellDesign(bool enabled) {
    if (enabled == (shellTheme != nullptr)) return;
    if (enabled) {
        shellTheme = std::make_unique<Theme>();
        shellTheme->applyPreview(DesignTokens::target());
    }
    // Keep the Desktop parent, command callbacks, focus order and native owners.
    // Only these components borrow the scoped instance of the existing Theme.
    juce::Component* controls[]{&pointerTool,&knifeTool,&undo,&redo,&split,&duplicate,
        &zoomIn,&zoomOut,&fit,&snapButton,&clickButton,&countButton,
        &play,&pause,&stop,&record,&previous,&next,&loop,&editButton,&mixButton,&brows,
        &navigationMode,&projectTitle,&contextLabels[0],&contextLabels[1],&contextLabels[2],
        &cpuReadout,&latencyReadout,&shellStatus,&shellTime};
    for (auto* control : controls) control->setLookAndFeel(enabled ? shellTheme.get() : nullptr);
    const auto tokens = DesignTokens::target();
    for (auto* button : {&play,&pause,&stop,&record,&previous,&next,&loop})
        button->getProperties().set("mrShellTransport", enabled);
    for (auto* button : {&play,&pause,&stop,&record}) {
        if (enabled) {
            const auto role = button == &record ? ColourRole::error : button == &play ? ColourRole::on : button == &pause ? ColourRole::warning : ColourRole::raised;
            button->setColour(juce::TextButton::buttonOnColourId, tokens.colour(role));
        } else button->removeColour(juce::TextButton::buttonOnColourId);
    }
    for (auto& context : contextLabels) context.setColour(juce::Label::textColourId, enabled ? tokens.colour(ColourRole::text) : juce::Colours::lightgrey);
    shellTime.setJustificationType(juce::Justification::centred);
    if (!enabled) {
        play.setToggleState(false, juce::dontSendNotification);
        pause.setToggleState(false, juce::dontSendNotification);
        stop.setToggleState(false, juce::dontSendNotification);
        shellTheme.reset(); // All borrowers detached before the Theme is destroyed.
    } else refreshShell();
    resized();
    repaint();
}

void Desktop::layoutShell() {
    int x = 12;
    const auto place = [&x](juce::Component& control, int width) { control.setBounds(x, 32, width, 32); x += width + 4; };
    split.setVisible(false);place(pointerTool, 32); place(knifeTool, 32); x += 12;
    place(undo, 32); place(redo, 32); x += 8;
    place(duplicate, 32); x += 8;
    place(zoomIn, 32); place(zoomOut, 32); place(fit, 32); place(snapButton, 32);

    for (std::size_t i = 0; i < contextLabels.size(); ++i)
        contextLabels[i].setBounds(12 + static_cast<int>(i) * (getWidth() - 24) / 3, 68, (getWidth() - 24) / 3 - 8, 22);
    const int y = getHeight() - 36;
    x = juce::jmax(400,(getWidth() - 520) / 2);
    navigationMode.setBounds(x, y, 88, 32); x += 96;
    for (auto* button : {&play,&pause,&stop,&record,&previous,&next,&loop}) { button->setBounds(x, y, 36, 32); x += 40; }
    for(auto* button:{&clickButton,&countButton}){button->setBounds(x,y,32,32);x+=36;}
    shellTime.setBounds(x + 4, y, 72, 32);
    x = getWidth() - 252;
    for (auto* button : {&editButton,&mixButton,&brows}) { button->setBounds(x, y, 76, 32); x += 80; }
    shellStatus.setBounds(12, getHeight() - 38, 360, 18);
    cpuReadout.setBounds(12, getHeight() - 20, 72, 16);
    audioCpuBar.setBounds(88, getHeight() - 14, 80, 5);
    latencyReadout.setBounds(180, getHeight() - 20, 208, 16);
}

void Desktop::refreshShell() {
    if (!shellTheme) return;
    const auto head = displayState();
    const auto tokens = DesignTokens::target();
    auto colour = tokens.colour(ColourRole::text);
    juce::String state;
    if (app.engine()->metrics().processing_fault) { state = "PROCESSING FAULT"; colour = tokens.colour(ColourRole::error); }
    else if (message.isNotEmpty()) { state = "NOTICE / " + message; colour = tokens.colour(ColourRole::warning); }
    else if (head.count_remaining) { state = "COUNT-IN / " + juce::String(static_cast<double>(head.count_remaining) / project()->sample_rate, 1) + " s"; colour = tokens.colour(ColourRole::warning); }
    else if (app.recording()) { state = "RECORDING"; colour = tokens.colour(ColourRole::error); }
    else if (head.playback == mrs::PlaybackState::playing) { state = "PLAYING"; colour = tokens.colour(ColourRole::on); }
    else if (head.playback == mrs::PlaybackState::paused) { state = "PAUSED"; colour = tokens.colour(ColourRole::warning); }
    else state = "STOPPED";
    state += " / " + juce::String::fromUTF8(app.audio_name().c_str());
    shellStatus.setColour(juce::Label::textColourId, colour);
    shellStatus.setText(state, juce::dontSendNotification);
    shellStatus.setTooltip(state);
    shellStatus.setDescription(state);
    shellTime.setText(juce::String(static_cast<double>(head.sample) / project()->sample_rate, 2) + " s", juce::dontSendNotification);
    play.setToggleState(head.playback == mrs::PlaybackState::playing, juce::dontSendNotification);
    pause.setToggleState(head.playback == mrs::PlaybackState::paused, juce::dontSendNotification);
    stop.setToggleState(head.playback == mrs::PlaybackState::stopped, juce::dontSendNotification);
}

void Desktop::paintShell(juce::Graphics& g) {
    const auto tokens = DesignTokens::target();
    const int footer = getWidth() < 1500 ? 58 : 42;
    g.setColour(tokens.colour(ColourRole::window));
    g.fillRect(0, 26, getWidth(), 68);
    // Leave the fractional-scale boundary pixel owned by the accepted content.
    g.fillRect(0, getHeight() - footer + 1, getWidth(), footer - 1);
    g.setColour(tokens.colour(ColourRole::border));
    g.drawHorizontalLine(93, 8.f, static_cast<float>(getWidth() - 8));
    g.drawHorizontalLine(getHeight() - footer + 1, 8.f, static_cast<float>(getWidth() - 8));
    for (const auto x : {undo.getX() - 6, split.getX() - 6, zoomIn.getX() - 6}) g.drawVerticalLine(x, 36.f, 60.f);
    if(assetSkin())return; // Icon-only transport in the user-approved SVG slice.
    const auto transport = navigationMode.getBounds().getUnion(loop.getBounds()).toFloat().expanded(6, 2);
    g.setColour(tokens.colour(ColourRole::deep));
    g.fillRoundedRectangle(transport, shellTheme->radius());
    g.setColour(tokens.colour(ColourRole::border));
    g.drawRoundedRectangle(transport, shellTheme->radius(), shellTheme->stroke());
}
}
