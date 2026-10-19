#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace ui {
void controlBindingSmoke();
std::unique_ptr<juce::DocumentWindow> createBindingCheckWindow(std::function<void()> close);
}
