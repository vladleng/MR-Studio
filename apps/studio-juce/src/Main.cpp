#include "Desktop.h"
#include "J1Smoke.h"
namespace {
class Window final : public juce::DocumentWindow {
public:
    Window():DocumentWindow("Moon River Studio 0.1p fix1 — JUCE J3",juce::Colour(ui::background),allButtons) {
        setUsingNativeTitleBar(true);setContentOwned(new ui::Desktop,true);
        setResizable(true,false);setResizeLimits(1200,700,3840,2160);centreWithSize(1400,850);auto* desktop=dynamic_cast<ui::Desktop*>(getContentComponent());
        if(desktop&&!desktop->view.windowState.isEmpty())restoreWindowStateFromString(desktop->view.windowState);
        setVisible(true);
    }
    ~Window() override {auto* desktop=dynamic_cast<ui::Desktop*>(getContentComponent());if(desktop){desktop->view.windowState=getWindowStateAsString();try{desktop->saveSettings();}catch(...){} }}
    void closeButtonPressed() override {
        auto* desktop=dynamic_cast<ui::Desktop*>(getContentComponent());
        if(desktop)desktop->confirmDiscard([this,desktop]{desktop->run([&]{desktop->view.windowState=getWindowStateAsString();desktop->saveSettings();juce::JUCEApplication::getInstance()->quit();});});else juce::JUCEApplication::getInstance()->quit();
    }
};
class Studio final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override {return "Moon River Studio JUCE";}
    const juce::String getApplicationVersion() override {return "0.1p fix1 J3";}
    void initialise(const juce::String& args) override {
        if(args.contains("--thu-diagnostic") || args.contains("--j3-smoke") || args.contains("--smoke-test") || args.contains("--j1-smoke") || args.contains("--plugin-smoke")) {
            juce::File::getCurrentWorkingDirectory().getChildFile("juce-j2-failure.txt").deleteFile();
            try {
                if(args.contains("--thu-diagnostic")){auto tokens=juce::StringArray::fromTokens(args,true);int at=tokens.indexOf("--thu-diagnostic");ui::thuDiagnostic(juce::File(tokens[at+1].unquoted()));}
                else if(args.contains("--j1-smoke")){ui::Surface surface;ui::smoke(surface);}
                else if(args.contains("--plugin-smoke")){auto tokens=juce::StringArray::fromTokens(args,true);int at=tokens.indexOf("--plugin-smoke");ui::Desktop surface(true);ui::j2PluginSmoke(surface,juce::File(tokens[at+1].unquoted()));}
                else {ui::Desktop surface(true);auto tokens=juce::StringArray::fromTokens(args,true);int at=tokens.indexOf("--fixture");
                    ui::j2Smoke(surface,at>=0&&at+1<tokens.size()?juce::File(tokens[at+1].unquoted()):juce::File());if(args.contains("--j3-smoke"))ui::j3Smoke(surface);auto image=surface.createComponentSnapshot(surface.getLocalBounds(),true,1.f,juce::SoftwareImageType{});
                    for(const auto& clip:surface.project()->clips)if(clip.source=="mrs:demo-tone"){
                        auto r=surface.arrangement->clipRect(clip);int x=surface.arrangeArea.getX()+static_cast<int>(r.getX())+100;
                        int y=surface.arrangeArea.getY()+static_cast<int>(r.getY()+22+(r.getHeight()-22)/4);
                        if(image.getPixelAt(x,y)!=juce::Colour(0xff8ac3ff))throw std::runtime_error("visible waveform pixels");
                    }
                    auto file=juce::File::getCurrentWorkingDirectory().getChildFile("juce-j2-preview.png");auto stream=file.createOutputStream();
                    if(!stream)throw std::runtime_error("snapshot stream");stream->setPosition(0);stream->truncate();
                    if(!juce::PNGImageFormat().writeImageToStream(image,*stream))throw std::runtime_error("snapshot export");}
                setApplicationReturnValue(0);
            }catch(const std::exception& e){juce::File::getCurrentWorkingDirectory().getChildFile("juce-j2-failure.txt").replaceWithText(e.what());setApplicationReturnValue(1);}
            quit();return;
        }
        window=std::make_unique<Window>();
    }
    void shutdown() override {window.reset();}
private:
    std::unique_ptr<Window> window;
};
}
START_JUCE_APPLICATION(Studio)
