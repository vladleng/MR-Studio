#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <filesystem>
#include <functional>
#include <mrs/drum_maps.hpp>

namespace ui {
std::unique_ptr<juce::DocumentWindow> makeDrumMapEditor(juce::Component& owner, std::filesystem::path library,
                         std::function<bool(int, int)> audition, std::optional<mrs::DrumMapProfile> initial, std::function<int()> feedback);
void drumMapEditorSmoke();
}
