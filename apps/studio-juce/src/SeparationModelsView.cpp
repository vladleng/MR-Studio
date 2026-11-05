#include "SeparationModelsView.h"
#include "SeparationModels.h"
#include "Desktop.h"
#include "GraphiteWindow.h"
#include <juce_cryptography/juce_cryptography.h>

namespace ui {
namespace {
using namespace mr::separation;
juce::String stateName(InstallationState state) {
    switch(state){case InstallationState::installed:return "Installed";case InstallationState::corrupt:return "Damaged or incomplete - remove and reinstall";case InstallationState::unsupported:return "Not available in this version";default:return "Not installed";}
}
juce::String errorText(const juce::String& error) {
    if(error.isEmpty())return {};
    if(error=="cancelled")return "Operation cancelled. You can safely retry.";
    if(error=="in_use")return "This installation is in use. Wait for processing to finish.";
    if(error=="busy")return "Another Model Manager operation is already running.";
    if(error=="disk_space")return "Not enough free space for this installation.";
    if(error=="network_error"||error=="http_error")return "Download failed. Check the connection and retry.";
    if(error=="integrity_failure"||error=="download_size")return "Integrity check failed. The incomplete download was discarded.";
    if(error=="unmanaged_files")return "Additional files were found in the cache. Removal stopped to preserve them.";
    if(error=="already_installed")return "Already installed. Use Verify, or remove it before reinstalling.";
    if(error=="unsafe_path"||error=="unmanaged_installation")return "Cache ownership/path check failed. Files were preserved.";
    if(error=="requirements_integrity")return "Processing setup files are missing or damaged. Restore the application package.";
    return "Operation failed. Open Details for the local diagnostic log.";
}
std::shared_ptr<ModelManager> productionManager() {
    static auto manager=[] {
        const auto app=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        return std::make_shared<ModelManager>(ModelManager::defaultRoot(),app.getChildFile("mr_separation_worker.py"),app.getChildFile("mr_separation_requirements.txt"));
    }();return manager;
}
class ModelsPanel final : public juce::Component,private juce::Timer {
public:
    explicit ModelsPanel(std::shared_ptr<ModelManager> manager,Theme& theme):manager_(std::move(manager)),theme_(theme),progress_(progressValue_) {
        title_.setText("Separation Models",juce::dontSendNotification);
        lead_.setText("Install the processing runtime and a model once, then work offline.",juce::dontSendNotification);
        runtimeTitle_.setText("Processing runtime - CPU",juce::dontSendNotification);
        runtimeInfo_.setText("Python 3.11.9 - audio-separator 0.48.0\nSeparate download; several GB of disk space required.",juce::dontSendNotification);
        for(auto* label:{&title_,&lead_,&runtimeTitle_,&runtimeInfo_,&runtimeState_,&modelInfo_,&modelState_,&phase_,&error_}) {
            addAndMakeVisible(label);label->setColour(juce::Label::textColourId,theme_.role(ColourRole::text,primaryText));label->setMinimumHorizontalScale(1.0f);
        }
        title_.setFont(theme_.font(21.0f));lead_.setFont(theme_.font(13.0f));lead_.setColour(juce::Label::textColourId,theme_.role(ColourRole::muted,secondaryText));
        runtimeInfo_.setFont(theme_.font(13.0f));modelInfo_.setFont(theme_.font(13.0f));
        const auto snapshot=manager_->status();for(std::size_t i=0;i<snapshot.models.size();++i)models_.addItem(snapshot.models[i].record.title,static_cast<int>(i)+1);
        addAndMakeVisible(models_);models_.setSelectedId(1,juce::dontSendNotification);models_.setTitle("Separation model");models_.onChange=[this]{update();};
        struct Action{juce::TextButton* button;const char* text;};
        int focus=1;for(const auto& action:std::initializer_list<Action>{{&installRuntime_,"Install runtime"},{&verifyRuntime_,"Verify runtime"},{&removeRuntime_,"Remove runtime"},{&download_,"Download model"},{&remove_,"Remove model"},{&detailsButton_,"Details"},{&cache_,"Open cache"},{&cancel_,"Cancel operation"}}){addAndMakeVisible(*action.button);action.button->setButtonText(action.text);action.button->setTitle(action.text);action.button->setExplicitFocusOrder(focus++);}
        installRuntime_.setTooltip("Explicit download of embedded Python and CPU processing libraries. System Python is unchanged.");
        installRuntime_.onClick=[this]{manager_->installRuntime();update();};verifyRuntime_.onClick=[this]{manager_->verifyRuntime();update();};removeRuntime_.onClick=[this]{manager_->removeRuntime();update();};
        download_.onClick=[this]{const auto s=manager_->status();const auto i=selected(s);if(i>=0){const auto& m=s.models[static_cast<std::size_t>(i)];if(m.state==InstallationState::installed||m.state==InstallationState::corrupt)manager_->verifyModel(m.record.id);else manager_->installModel(m.record.id);}update();};
        remove_.onClick=[this]{const auto s=manager_->status();const auto i=selected(s);if(i>=0)manager_->removeModel(s.models[static_cast<std::size_t>(i)].record.id);update();};
        cancel_.onClick=[this]{manager_->cancel();};cache_.onClick=[this]{manager_->root().revealToUser();};
        detailsButton_.onClick=[this]{details_.setVisible(!details_.isVisible());detailsButton_.setButtonText(details_.isVisible()?"Hide details":"Details");update();};
        details_.setMultiLine(true);details_.setReadOnly(true);details_.setCaretVisible(false);details_.setScrollbarsShown(true);details_.setTitle("Model source, capabilities, integrity and local diagnostics");addChildComponent(details_);
        addAndMakeVisible(progress_);progress_.setColour(juce::ProgressBar::foregroundColourId,theme_.role(ColourRole::focus,accent));progress_.setTitle("Download and installation progress");
        setSize(840,560);manager_->refreshIfNeeded();startTimerHz(8);update();
    }
    ~ModelsPanel()override{stopTimer();manager_->cancel();}
    void paint(juce::Graphics& g)override {
        g.fillAll(theme_.role(ColourRole::window,panel));g.setColour(theme_.role(ColourRole::panel,contentSurface));
        g.fillRoundedRectangle(16.0f,76.0f,static_cast<float>(getWidth()-32),120.0f,6.0f);
        g.fillRoundedRectangle(16.0f,208.0f,static_cast<float>(getWidth()-32),144.0f,6.0f);
    }
    void resized()override {
        const int width=getWidth();title_.setBounds(24,12,width-48,30);lead_.setBounds(24,44,width-48,24);
        runtimeTitle_.setBounds(28,84,width-320,24);runtimeInfo_.setBounds(28,112,width-320,42);runtimeState_.setBounds(28,158,width-320,28);
        installRuntime_.setBounds(width-268,88,236,28);verifyRuntime_.setBounds(width-268,124,112,28);removeRuntime_.setBounds(width-148,124,116,28);
        models_.setBounds(28,220,width-56,28);modelInfo_.setBounds(28,260,width-320,44);modelState_.setBounds(28,310,width-320,28);
        download_.setBounds(width-268,260,236,28);remove_.setBounds(width-268,300,112,28);detailsButton_.setBounds(width-148,300,116,28);
        phase_.setBounds(24,360,width-256,24);progress_.setBounds(24,390,width-48,16);cancel_.setBounds(width-216,356,192,28);
        details_.setBounds(24,416,width-48,juce::jmax(24,getHeight()-492));error_.setBounds(24,getHeight()-72,width-48,28);cache_.setBounds(24,getHeight()-40,136,28);
    }
    void smoke() {
        update();if(!installRuntime_.isEnabled()||!download_.isEnabled()||remove_.isEnabled())throw std::runtime_error("Model Manager initial controls");
        download_.onClick();
        for(int i=0;i<300&&manager_->status().busy;++i){std::this_thread::sleep_for(std::chrono::milliseconds(5));}
        update();if(manager_->status().error.isNotEmpty()||!download_.isEnabled()||download_.getButtonText()!="Verify model"||!remove_.isEnabled())throw std::runtime_error("Model Manager download/verify action state");
        download_.onClick();for(int i=0;i<300&&manager_->status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(5));
        update();if(manager_->status().error.isNotEmpty()||manager_->status().models.front().state!=InstallationState::installed)throw std::runtime_error("Explicit model Verify binding");
        detailsButton_.onClick();if(!details_.isVisible()||!details_.getText().contains("Source")||!details_.getText().contains("Weights license"))throw std::runtime_error("Model Manager details");
        for(const auto size:{juce::Point<int>{800,540},juce::Point<int>{840,560},juce::Point<int>{1280,900}}){
            setSize(size.x,size.y);
            for(auto* component:getChildren())if(component->isVisible()&&!getLocalBounds().contains(component->getBounds()))throw std::runtime_error("Model Manager control outside panel");
            for(const float scale:{1.0f,1.25f,1.5f,2.0f}){
                const auto image=createComponentSnapshot(getLocalBounds(),true,scale);if(!image.isValid()||image.getWidth()!=juce::roundToInt(getWidth()*scale))throw std::runtime_error("Model Manager scale snapshot");
                const auto file=juce::File::getCurrentWorkingDirectory().getChildFile("separation-models-"+juce::String(size.x)+"x"+juce::String(size.y)+"-"+juce::String(juce::roundToInt(scale*100))+".png");auto output=file.createOutputStream();
                if(!output||!juce::PNGImageFormat().writeImageToStream(image,*output))throw std::runtime_error("Model Manager snapshot write");
            }
        }
        setSize(840,560);
        remove_.onClick();for(int i=0;i<300&&manager_->status().busy;++i){std::this_thread::sleep_for(std::chrono::milliseconds(5));}update();
        if(manager_->status().error.isNotEmpty()||!download_.isEnabled())throw std::runtime_error("Model Manager remove binding");
    }
private:
    int selected(const ManagerStatus& s)const{const int i=models_.getSelectedId()-1;return i>=0&&static_cast<std::size_t>(i)<s.models.size()?i:-1;}
    void timerCallback()override{update();}
    void update() {
        const auto state=manager_->status();const auto at=selected(state);const bool runtimeExists=state.runtime==InstallationState::installed||state.runtime==InstallationState::corrupt;
        runtimeState_.setText(stateName(state.runtime),juce::dontSendNotification);
        installRuntime_.setEnabled(!state.busy&&!runtimeExists);verifyRuntime_.setEnabled(!state.busy&&runtimeExists);removeRuntime_.setEnabled(!state.busy&&runtimeExists);
        models_.setEnabled(!state.busy);download_.setEnabled(false);remove_.setEnabled(false);
        if(at>=0){const auto& model=state.models[static_cast<std::size_t>(at)];const bool installed=model.state==InstallationState::installed||model.state==InstallationState::corrupt;
            modelInfo_.setText("Outputs: "+model.record.outputs+"\n"+model.record.hardware,juce::dontSendNotification);modelState_.setText(stateName(model.state),juce::dontSendNotification);
            juce::int64 size{};for(const auto& file:model.record.files)size+=file.bytes;
            download_.setButtonText(installed?"Verify model":"Download model"+(size>0?" - "+juce::File::descriptionOfSizeInBytes(size):juce::String{}));download_.setTitle(download_.getButtonText());download_.setEnabled(!state.busy&&(installed||model.state==InstallationState::downloadable));remove_.setEnabled(!state.busy&&installed);
            if(details_.isVisible()){
                juce::String text="Model ID: "+model.record.id+"\nVersion: "+model.record.version+"\nSource: "+model.record.provenance+"\nBackend: "+model.record.backend+"\nNative sources: "+model.record.nativeOutputs+"\nRuntime: CPython 3.11.9 (PSF-2.0); package notices in the managed cache\nCode license: "+model.record.codeLicense+"\nWeights license: "+model.record.weightsLicense+"\n"+model.record.unsupportedReason+"\n";
                for(const auto& file:model.record.files)text+="\n"+file.url+"\nSHA256: "+file.sha256+"\n";
                const auto log=manager_->diagnostics();if(log.isNotEmpty())text+="\nLocal diagnostics (may contain cache paths):\n"+log;
                if(details_.getText()!=text)details_.setText(text,false);
            }
        }
        const bool cachePresent=runtimeExists||std::any_of(state.models.begin(),state.models.end(),[](const auto& m){return m.state==InstallationState::installed||m.state==InstallationState::corrupt;});
        cache_.setEnabled(cachePresent&&!state.busy);cancel_.setEnabled(state.busy&&state.cancellable);
        phase_.setText(state.busy?state.phase:"Local processing. No audio is uploaded.",juce::dontSendNotification);error_.setText(errorText(state.error),juce::dontSendNotification);error_.setColour(juce::Label::textColourId,theme_.role(state.error=="cancelled"?ColourRole::muted:ColourRole::error,state.error=="cancelled"?secondaryText:DesignTokens{}.colours[static_cast<std::size_t>(ColourRole::error)]));
        progressValue_=state.totalBytes>0?static_cast<double>(state.completedBytes)/static_cast<double>(state.totalBytes):-1.0;progress_.setVisible(state.busy);
    }
    std::shared_ptr<ModelManager> manager_;
    Theme& theme_;
    juce::Label title_,lead_,runtimeTitle_,runtimeInfo_,runtimeState_,modelInfo_,modelState_,phase_,error_;
    juce::ComboBox models_;
    juce::TextButton installRuntime_,verifyRuntime_,removeRuntime_,download_,remove_,detailsButton_,cache_,cancel_;
    juce::TextEditor details_;
    double progressValue_{-1.0};juce::ProgressBar progress_;
};
class ModelsWindow final:public juce::DocumentWindow {
public:
    ModelsWindow(Desktop& owner,std::shared_ptr<ModelManager> manager):DocumentWindow("Separation Models",juce::Colour(panel),closeButton),owner_(owner){
        setUsingNativeTitleBar(true);setLookAndFeel(&owner.theme);setContentOwned(new ModelsPanel(std::move(manager),owner.theme),true);setResizable(true,false);setResizeLimits(800,540,1280,900);centreAroundComponent(&owner,840,560);setVisible(true);applyGraphiteWindowCaption(*this);
    }
    ~ModelsWindow()override{setLookAndFeel(nullptr);}
    void closeButtonPressed()override{owner_.separationModelsWindow.reset();}
private:Desktop& owner_;
};
}
std::shared_ptr<ModelManager> separationModelManager(){return productionManager();}
void showSeparationModels(Desktop& owner,std::shared_ptr<ModelManager> manager){
    if(owner.separationModelsWindow){owner.separationModelsWindow->toFront(true);return;}
    owner.separationModelsWindow=std::make_unique<ModelsWindow>(owner,manager?std::move(manager):productionManager());
}
void separationModelsSmoke(){
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mr-model-ui-"+juce::Uuid().toString());
    struct Cleanup{juce::File root;~Cleanup(){root.deleteRecursively();}} cleanup{root};
    const juce::String data="UI-owned fixture";const auto hash=juce::SHA256(data.toRawUTF8(),data.getNumBytesAsUTF8()).toHexString();auto record=modelCatalogue().front();record.files={{"fixture.bin","https://github.com/fixture",hash,static_cast<juce::int64>(data.getNumBytesAsUTF8())}};
    auto manager=std::make_shared<ModelManager>(root,juce::File{},juce::File{},[data](const auto& artifact,const auto& target,const auto&,auto progress){if(!target.replaceWithText(data))throw std::runtime_error("UI fixture write");progress(artifact.bytes,artifact.bytes);},std::vector<ModelRecord>{record});
    Desktop desktop(true);const auto projectBefore=desktop.project();if(desktop.getMenuForIndex(1,{}).getNumItems()<4)throw std::runtime_error("Model Manager Options entry");showSeparationModels(desktop,manager);
    auto* panelView=dynamic_cast<ModelsPanel*>(desktop.separationModelsWindow->getContentComponent());if(!panelView)throw std::runtime_error("Model Manager window content");
    for(int i=0;i<100&&manager->status().busy;++i)std::this_thread::sleep_for(std::chrono::milliseconds(5));panelView->smoke();desktop.separationModelsWindow.reset();
    showSeparationModels(desktop,manager);if(manager->status().busy)throw std::runtime_error("Reopening Model Manager started another scan");desktop.separationModelsWindow.reset();
    if(desktop.project()!=projectBefore)throw std::runtime_error("Model Manager changed project state");
}
} // namespace ui
