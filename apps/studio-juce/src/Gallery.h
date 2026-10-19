#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace ui {
std::unique_ptr<juce::DocumentWindow> createGalleryWindow(std::function<void()> onClose);
void gallerySmoke();
}
