#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace ui::live {
// All coordinates are JUCE logical pixels; Windows DPI scaling is already applied.
inline juce::Rectangle<int> fitWindow(juce::Rectangle<int> desired, juce::Rectangle<int> work,
                                      juce::BorderSize<int> frame, bool centre) {
    const auto available = frame.subtractedFrom(work.reduced(4));
    const int width = juce::jmax(1, juce::jmin(desired.getWidth(), available.getWidth()));
    const int height = juce::jmax(1, juce::jmin(desired.getHeight(), available.getHeight()));
    const int x = centre ? available.getCentreX() - width / 2
                         : juce::jlimit(available.getX(), available.getRight() - width, desired.getX());
    const int y = centre ? available.getCentreY() - height / 2
                         : juce::jlimit(available.getY(), available.getBottom() - height, desired.getY());
    return {x, y, width, height};
}
inline float fitUiScale(int width, int height, double requested) {
    if (requested > 0)
        return static_cast<float>(juce::jlimit(.75, 1.5, requested));
    return juce::jmax(.1f, juce::jmin(1.f, static_cast<float>(width) / 1200.f, static_cast<float>(height) / 700.f));
}
} // namespace ui::live
