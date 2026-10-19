#include "BindingCheck.h"
namespace {
class Application final : public juce::JUCEApplication {
    std::unique_ptr<juce::DocumentWindow> window;
public:
    const juce::String getApplicationName()override{return "MR Host Binding Check";}
    const juce::String getApplicationVersion()override{return "0.4.0";}
    bool moreThanOneInstanceAllowed()override{return true;}
    void initialise(const juce::String& args)override{if(args.contains("--smoke")){try{ui::controlBindingSmoke();setApplicationReturnValue(0);}catch(const std::exception& e){juce::File::getCurrentWorkingDirectory().getChildFile("binding194-failure.txt").replaceWithText(juce::String::fromUTF8(e.what()));setApplicationReturnValue(1);}quit();return;}window=ui::createBindingCheckWindow([this]{quit();});}
    void shutdown()override{window.reset();}
};
}
START_JUCE_APPLICATION(Application)
