#include "LibraryBrowserView.h"
#include "Desktop.h"
#include "Gallery.h"
#include "BindingCheck.h"
#include "ReverbKit.h"
#include "ShellChrome.h"
#include "AssetDesign.h"
#include "J1Smoke.h"
#include "ReverbBrowser.h"
#include "ToneBrowser.h"
#include "RigPanel.h"
#include <mrs/version.hpp> // Shared application/package version (compact note editor update).
#include <mrs/diagnostics.hpp>
#include <stdexcept>
namespace {
class Window final : public juce::DocumentWindow {
public:
    explicit Window(bool kitPilot=false,bool fixture=false,bool shell=false,bool assets=false,juce::String trackLayoutPath={}):DocumentWindow("Moon River Studio "+juce::String(mrs::desktop::application_version.data())+" MIDI — JUCE J3",juce::Colour(ui::surface),allButtons) {
        setUsingNativeTitleBar(true);setContentOwned(new ui::Desktop(fixture),true);
        setResizable(true,false);setResizeLimits(1200,700,3840,2160);centreWithSize(1400,850);auto* desktop=dynamic_cast<ui::Desktop*>(getContentComponent());
        if(desktop){desktop->reverbKitPilot=kitPilot;desktop->setShellDesign(shell);}
        if(shell)setName("MR Studio / UDS Shell P1");
        if(desktop&&assets){desktop->setAssetDesign(true);setName("MR Studio / SVG Controls");}
        if(desktop&&trackLayoutPath.isNotEmpty()){
            juce::String error;
            const auto path=juce::File::getCurrentWorkingDirectory().getChildFile(trackLayoutPath);
            if(!desktop->enableTrackHeaderLayout(path,error))
                juce::Logger::writeToLog("MR Design Lab layout rejected; legacy Track Header retained: "+error);
            else setName("MR Studio / Track Header JSON v1 preview");
        }
        if(desktop&&!fixture&&!desktop->view.windowState.isEmpty())restoreWindowStateFromString(desktop->view.windowState);
        setVisible(true);
        if(fixture&&desktop&&kitPilot)ui::setupReverbKitPreview(*desktop);
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
    const juce::String getApplicationVersion() override {return juce::String(mrs::desktop::application_version.data());}
    void initialise(const juce::String& args) override {
        if(args.contains("--piano214-smoke")){try{ui::pianoRoll214Smoke();setApplicationReturnValue(0);}catch(const std::exception& e){juce::File::getCurrentWorkingDirectory().getChildFile("piano214-failure.txt").replaceWithText(juce::String::fromUTF8(e.what()));setApplicationReturnValue(1);}quit();return;}
        if(args.contains("--channel213-smoke")){try{ui::channel213Smoke();setApplicationReturnValue(0);}catch(const std::exception& e){juce::File::getCurrentWorkingDirectory().getChildFile("channel213-failure.txt").replaceWithText(juce::String::fromUTF8(e.what()));setApplicationReturnValue(1);}quit();return;}
        if(args.contains("--compact-fader215-smoke")){try{ui::compactFader215Smoke();setApplicationReturnValue(0);}catch(const std::exception& e){juce::File::getCurrentWorkingDirectory().getChildFile("compact215-failure.txt").replaceWithText(juce::String::fromUTF8(e.what()));setApplicationReturnValue(1);}quit();return;}
        if(args.contains("--interface212-smoke")){try{ui::interface212Smoke();setApplicationReturnValue(0);}catch(const std::exception& e){juce::File::getCurrentWorkingDirectory().getChildFile("interface212-failure.txt").replaceWithText(juce::String::fromUTF8(e.what()));setApplicationReturnValue(1);}quit();return;}
        if(args.contains("--track-layout-smoke")){
            try{
                const auto tokens=juce::StringArray::fromTokens(args,true);
                const int at=tokens.indexOf("--track-layout-smoke");
                if(at<0||at+1>=tokens.size())throw std::runtime_error("missing Track Header layout path");
                juce::String error;
                const auto file=juce::File::getCurrentWorkingDirectory().getChildFile(tokens[at+1].unquoted());
                const auto loaded=ui::TrackHeaderLayout::readFile(file,error);
                if(!loaded)throw std::runtime_error(error.toStdString());
                for(const auto h:{38,60,83,84,110,123,124,166,358}){
                    const auto* name=loaded->find(h,"name");
                    const auto* gain=loaded->find(h,"gain");
                    if(!name||!gain)throw std::runtime_error("profile semantic controls missing");
                    const auto rect=loaded->rectangle(h,244,*gain,"gain");
                    if(rect.isEmpty()||rect.getBottom()>h||rect.getRight()>244)
                        throw std::runtime_error("Track Header bounds out of range");
                }
                juce::String invalidError;
                if(ui::TrackHeaderLayout::parse("{}",invalidError))
                    throw std::runtime_error("invalid layout accepted");
                // Verify the new top-level Options menu and the toggle contract
                // using a local/offline Desktop fixture; no ASIO device required.
                ui::Desktop desktop(true);
                if(desktop.getMenuBarNames()[1]!="Options" || desktop.getMenuForIndex(1,{}).getNumItems()!=3)
                    throw std::runtime_error("Options menu or Track Header actions missing");
                if(desktop.isTrackHeaderReloadEnabled())
                    throw std::runtime_error("Hot Reload must default OFF in Release");
                if(!desktop.trackLayout())
                    throw std::runtime_error("Embedded Release Track Header is not active by default");
                const auto& profile=desktop.trackLayout()->profile(60);
                if(profile.referenceHeight!=60 || !desktop.trackLayout()->find(110,"pan")
                   || !desktop.trackLayout()->find(166,"output"))
                    throw std::runtime_error("Embedded default layout profiles are incorrect");
                if(!desktop.enableTrackHeaderLayout(file,error))
                    throw std::runtime_error("cannot activate valid layout in Desktop: "+error.toStdString());
                const auto* lastGood=desktop.trackLayout();
                juce::String failure;
                if(desktop.enableTrackHeaderLayout(file.getSiblingFile("not-a-real-layout.json"),failure)
                   || desktop.trackLayout()!=lastGood)
                    throw std::runtime_error("invalid layout replaced the last-good GUI state");
                desktop.action(711);
                if(!desktop.isTrackHeaderReloadEnabled())
                    throw std::runtime_error("Options Hot Reload toggle failed to enable");
                desktop.action(711);
                if(desktop.isTrackHeaderReloadEnabled())
                    throw std::runtime_error("Options Hot Reload toggle failed to disable");
                desktop.clearTrackHeaderLayout();
                if(!desktop.trackLayout() || desktop.trackLayout()->profile(60).referenceHeight!=60
                   || desktop.isTrackHeaderReloadEnabled())
                    throw std::runtime_error("Options restore bundled default failed");
                setApplicationReturnValue(0);
            }catch(const std::exception& e){
                juce::File::getCurrentWorkingDirectory().getChildFile("track-layout-smoke-failure.txt").replaceWithText(juce::String::fromUTF8(e.what()));
                setApplicationReturnValue(1);
            }
            quit();return;
        }
        if(args.contains("--uds-assets-smoke")){try{ui::assetDesignSmoke();setApplicationReturnValue(0);}catch(const std::exception& e){juce::File::getCurrentWorkingDirectory().getChildFile("assets-failure.txt").replaceWithText(juce::String::fromUTF8(e.what()));setApplicationReturnValue(1);}quit();return;}
        if(args.contains("--uds-assets-preview")){window=std::make_unique<Window>(false,true,true,true);return;}
        if(args.contains("--uds-shell-smoke")){try{ui::shellDesignSmoke();setApplicationReturnValue(0);}catch(const std::exception& e){juce::File::getCurrentWorkingDirectory().getChildFile("shell205-failure.txt").replaceWithText(juce::String::fromUTF8(e.what()));setApplicationReturnValue(1);}quit();return;}
        if(args.contains("--uds-shell-preview")){window=std::make_unique<Window>(false,true,true);return;}
        if(args.contains("--reverb-kit-smoke")){try{ui::reverbKitSmoke();setApplicationReturnValue(0);}catch(const std::exception& e){juce::File::getCurrentWorkingDirectory().getChildFile("pilot195-failure.txt").replaceWithText(juce::String::fromUTF8(e.what()));setApplicationReturnValue(1);}quit();return;}
        if(args.contains("--reverb-kit-preview")){window=std::make_unique<Window>(true,true);return;}
        if(args.contains("--control-binding-smoke")){try{ui::controlBindingSmoke();setApplicationReturnValue(0);}catch(const std::exception& e){juce::File::getCurrentWorkingDirectory().getChildFile("binding194-failure.txt").replaceWithText(e.what());setApplicationReturnValue(1);}quit();return;}
        if(args.contains("--control-binding-preview")){gallery=ui::createBindingCheckWindow([this]{quit();});return;}
        // Explicit design-only entry: return before diagnostics, Desktop and audio setup.
        if(args.contains("--ui-gallery-smoke")){try{ui::gallerySmoke();setApplicationReturnValue(0);}catch(const std::exception& e){juce::File::getCurrentWorkingDirectory().getChildFile("gallery193-entry-failure.txt").replaceWithText(e.what());setApplicationReturnValue(1);}quit();return;}
        if(args.contains("--ui-gallery")){gallery=ui::createGalleryWindow([this]{quit();});return;}
        if(args.contains("--ara-smoke")){juce::File::getCurrentWorkingDirectory().getChildFile("ara179-gui-failure.txt").deleteFile();try{const auto tokens=juce::StringArray::fromTokens(args,true);ui::araEditorSmoke(juce::File(tokens[tokens.indexOf("--ara-smoke")+1].unquoted()));setApplicationReturnValue(0);}catch(const std::exception& e){juce::File::getCurrentWorkingDirectory().getChildFile("ara179-gui-failure.txt").replaceWithText(juce::String(e.what()));setApplicationReturnValue(1);}quit();return;}
        if(args.contains("--library-browser-smoke")){
            juce::File::getCurrentWorkingDirectory().getChildFile("library261-failure.txt").deleteFile();
            try{ui::libraryBrowserSmoke();setApplicationReturnValue(0);}
            catch(const std::exception& e){juce::File::getCurrentWorkingDirectory().getChildFile("library261-failure.txt").replaceWithText(e.what());setApplicationReturnValue(1);}
            quit();return;
        }
        const bool aiSmoke=args.contains("--rig-shell-smoke")||args.contains("--tone-library-smoke")||args.contains("--ai-permissions-smoke")||args.contains("--ai-assistant-smoke");
        const bool smoke=aiSmoke||(args.contains("--cab-editor-smoke")||args.contains("--native-editors-smoke"))||(args.contains("--reverb-browser-smoke")||args.contains("--reverb-editor-smoke"))||args.contains("--j3-smoke")||args.contains("--j1-smoke")||args.contains("--smoke-test")||args.contains("--plugin-smoke")||args.contains("--thu-diagnostic");
        if(!smoke||args.contains("--diagnostics-smoke")){
            const auto logs=args.contains("--diagnostics-smoke")?juce::File::getCurrentWorkingDirectory().getChildFile("diagnostics-gui-smoke"):
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MR Studio").getChildFile("Logs");
            const auto helper=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("mrs_crash_reporter.exe");
            diagnostics=std::make_unique<mrs::diagnostics::Session>(std::filesystem::path(logs.getFullPathName().toWideCharPointer()),std::filesystem::path(helper.getFullPathName().toWideCharPointer()),mrs::desktop::application_version.data());
            mrs::diagnostics::event("application_initialise");
        }
        if((args.contains("--cab-editor-smoke")||args.contains("--native-editors-smoke")) || aiSmoke || (args.contains("--reverb-browser-smoke")||args.contains("--reverb-editor-smoke")) || args.contains("--thu-diagnostic") || args.contains("--j3-smoke") || args.contains("--smoke-test") || args.contains("--j1-smoke") || args.contains("--plugin-smoke")) {
            juce::File::getCurrentWorkingDirectory().getChildFile("juce-j2-failure.txt").deleteFile();
            try {
                if(args.contains("--rig-shell-smoke")){ui::rigShellSmoke();}
                else if(args.contains("--tone-library-smoke")){ui::toneLibrarySmoke();}
                else if(aiSmoke){ui::Desktop desktop(true);if(args.contains("--ai-assistant-smoke"))ui::aiAssistantSmoke(desktop);else ui::aiPermissionsSmoke(desktop);}
                else if(args.contains("--native-editors-smoke")){ui::nativeEditorSmoke();}
                else if(args.contains("--cab-editor-smoke")){ui::cabEditorSmoke();}
                else if((args.contains("--reverb-browser-smoke")||args.contains("--reverb-editor-smoke"))){if(args.contains("--reverb-editor-smoke"))ui::reverbEditorSmoke();else ui::reverbBrowserSmoke();}
                else if(args.contains("--thu-diagnostic")){auto tokens=juce::StringArray::fromTokens(args,true);int at=tokens.indexOf("--thu-diagnostic");ui::thuDiagnostic(juce::File(tokens[at+1].unquoted()));}
                else if(args.contains("--j1-smoke")){ui::Surface surface;ui::smoke(surface);}
                else if(args.contains("--plugin-smoke")){auto tokens=juce::StringArray::fromTokens(args,true);int at=tokens.indexOf("--plugin-smoke");ui::Desktop surface(true);ui::j2PluginSmoke(surface,juce::File(tokens[at+1].unquoted()));}
                else {ui::Desktop surface(true);auto tokens=juce::StringArray::fromTokens(args,true);int at=tokens.indexOf("--fixture");
                    ui::j2Smoke(surface,at>=0&&at+1<tokens.size()?juce::File(tokens[at+1].unquoted()):juce::File());if(args.contains("--j3-smoke")){ui::j3Smoke(surface);ui::midi4aSmoke(surface,at>=0&&at+1<tokens.size()?juce::File(tokens[at+1].unquoted()):juce::File());ui::midi4bSmoke(surface,at>=0&&at+1<tokens.size()?juce::File(tokens[at+1].unquoted()):juce::File());ui::midi4cSmoke(surface,at>=0&&at+1<tokens.size()?juce::File(tokens[at+1].unquoted()):juce::File());}auto image=surface.createComponentSnapshot(surface.getLocalBounds(),true,1.f,juce::SoftwareImageType{});
                    for(const auto& clip:surface.project()->clips)if(clip.source=="mrs:demo-tone"){
                        auto r=surface.arrangement->clipRect(clip);int x=surface.arrangeArea.getX()+static_cast<int>(r.getX())+100;
                        int y=surface.arrangeArea.getY()+static_cast<int>(r.getY()+22+(r.getHeight()-22)/4);
                        if(image.getPixelAt(x,y)!=juce::Colour(0xff8ac3ff))throw std::runtime_error("visible waveform pixels at "+std::to_string(x)+","+std::to_string(y)+" color="+image.getPixelAt(x,y).toString().toStdString()+" clip="+clip.name+" arrangement bottom="+std::to_string(surface.arrangeArea.getBottom()));
                    }
                    auto file=juce::File::getCurrentWorkingDirectory().getChildFile("juce-j2-preview.png");auto stream=file.createOutputStream();if(stream){stream->setPosition(0);stream->truncate();}
                    if(!stream)throw std::runtime_error("snapshot stream");stream->setPosition(0);stream->truncate();
                    if(!juce::PNGImageFormat().writeImageToStream(image,*stream))throw std::runtime_error("snapshot export");}
                setApplicationReturnValue(0);
            }catch(const std::exception& e){const auto message=juce::String::fromUTF8(e.what());const auto folder=juce::File::getCurrentWorkingDirectory();folder.getChildFile("juce-j2-failure.txt").replaceWithText(message);
                // Later smoke invocations clear the shared file. Keep each failure
                // so a full CTest run cannot erase its earliest useful diagnostic.
                const auto scenario=aiSmoke?"ai":args.contains("--j3-smoke")?"j3":args.contains("--smoke-test")?"j2":"other";
                folder.getChildFile("juce-smoke-failure-"+juce::String(scenario)+"-"+juce::String(juce::Time::currentTimeMillis())+".txt").replaceWithText(message);setApplicationReturnValue(1);}
            quit();return;
        }
        // Explicit, session-scoped prototype: --track-layout "C:\\path\\mr-track-panel-layout-v1.json".
        // No browser/localStorage/network access, and no changes to project persistence.
        const auto tokens=juce::StringArray::fromTokens(args,true);
        const int layoutArg=tokens.indexOf("--track-layout");
        const auto layoutFile=layoutArg>=0&&layoutArg+1<tokens.size() ? tokens[layoutArg+1].unquoted() : juce::String();
        window=std::make_unique<Window>(args.contains("--reverb-kit"),false,args.contains("--uds-shell")||!args.contains("--legacy-ui"),!args.contains("--legacy-ui"),layoutFile);
        if(diagnostics){
            if(!diagnostics->available()||!diagnostics->crash_capture_available())juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Diagnostics",juce::String(mrs::diagnostics::status()),"OK");
            else if(const auto report=diagnostics->previous_report();!report.empty())juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Crash report available","A previous session ended unexpectedly. A local report is available.\nFile > Open diagnostics folder. Reports may contain project/plugin paths and private stack data. No automatic upload.","OK",nullptr,juce::ModalCallbackFunction::create([report](int){if(!mrs::diagnostics::acknowledge_report(report))mrs::diagnostics::event("crash_notice_acknowledge_failed");}));
        }
    }
    void shutdown() override {mrs::diagnostics::event("application_shutdown_begin");gallery.reset();window.reset();mrs::diagnostics::event("application_shutdown_end");diagnostics.reset();}
private:
    std::unique_ptr<juce::DocumentWindow> gallery;
    std::unique_ptr<Window> window;
    std::unique_ptr<mrs::diagnostics::Session> diagnostics;
};
}
START_JUCE_APPLICATION(Studio)
