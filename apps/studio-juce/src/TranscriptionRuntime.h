#pragma once
#include "SeparationModels.h"
#include <stop_token>
namespace ui {
// Separate installation and dependencies; only generic installer primitives
// are shared with Separation. No Separation model/runtime files are modified.
class TranscriptionRuntime {
public:
    TranscriptionRuntime(juce::File root,juce::File requirements):root_(std::move(root)),requirements_(std::move(requirements)){}
    juce::File directory()const{return root_.getChildFile("python-medium-v1");}
    juce::File python()const{return directory().getChildFile("python.exe");}
    bool present()const{return directory().exists();}
    void install(std::stop_token)const;
    void verify(std::stop_token)const;
    void remove()const;
    std::shared_ptr<void> acquire()const;
private:
    std::shared_ptr<void> lock(bool exclusive)const;
    juce::File root_,requirements_;
};
}
