#pragma once
#include <juce_core/juce_core.h>
#include <mrs/core.hpp>
namespace ui {
class Desktop;
class TranscriptionSetup;
void showTranscription(Desktop&,std::optional<mrs::Id> = {});
void closeTranscription(Desktop&);
void transcriptionSmoke(const juce::File& fake_engine);
}
