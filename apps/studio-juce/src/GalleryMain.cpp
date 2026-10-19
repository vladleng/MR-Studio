#include "Gallery.h"
#include <iostream>
namespace {
class Application final : public juce::JUCEApplication {
    std::unique_ptr<juce::DocumentWindow> window;
public:
    const juce::String getApplicationName()override{return "MR UI Gallery";}
    const juce::String getApplicationVersion()override{return "0.3.0";}
    bool moreThanOneInstanceAllowed()override{return true;}
    void initialise(const juce::String& args)override{if(args.contains("--smoke")){try{ui::gallerySmoke();setApplicationReturnValue(0);}catch(const std::exception& e){juce::File::getCurrentWorkingDirectory().getChildFile("gallery193-failure.txt").replaceWithText(e.what());setApplicationReturnValue(1);}quit();return;}window=ui::createGalleryWindow([this]{quit();});}
    void shutdown()override{window.reset();}
};
}
START_JUCE_APPLICATION(Application)
