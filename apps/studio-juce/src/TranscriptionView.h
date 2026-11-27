#pragma once
#include <juce_core/juce_core.h>
#include <mrs/audio_to_midi.hpp>
#include <memory>
namespace juce { class Component; }
namespace ui {
class Desktop;
class TranscriptionSetup;
void showTranscription(Desktop&,std::optional<mrs::Id> = {},mrs::TranscriptionPlacement = {});
void showTranscriptionPreferences(Desktop&);
std::unique_ptr<juce::Component> makeTranscriptionPreferencesPage(Desktop&);
void showTranscriptionModels(Desktop&);
void closeTranscription(Desktop&);
void transcriptionSmoke(const juce::File& fake_engine);
}
