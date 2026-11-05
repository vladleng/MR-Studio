#include "SeparationView.h"
#include "SeparationModelsView.h"
#include "Desktop.h"
#include "GraphiteWindow.h"
#include <mrs/sha256.hpp>
#include <bit>
#include <fstream>

namespace ui {
namespace {
using namespace mr::separation;
juce::String js(const std::string& s){return juce::String::fromUTF8(s.c_str());}
void ensure(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
juce::String jobError(const juce::String& error){
    if(error=="model_missing")return "Model files are missing or damaged. Open Model Manager.";
    if(error=="python_missing"||error=="installation_missing")return "Processing runtime is missing. Open Model Manager.";
    if(error=="stale_source")return "The source file changed. Start separation again.";
    if(error=="invalid_output"||error=="integrity_failure")return "Result verification failed; no stems were added.";
    if(error=="invalid_input"||error=="unsupported_input")return "Use a mono/stereo WAV at the project rate, up to 10 minutes.";
    if(error=="timeout")return "Processing timed out. Try a shorter clip.";
    if(error=="resource_limit")return "Processing reached its resource limit. Try a shorter clip.";
    if(error=="worker_crash"||error=="backend_failure")return "The processing worker stopped unexpectedly. You can retry.";
    return error;
}
class SeparationPanel final:public juce::Component,private juce::Timer {
public:
    SeparationPanel(Desktop& owner,std::optional<mrs::Id> clip,std::shared_ptr<ModelManager> manager,std::shared_ptr<ProjectJob> job)
        :owner_(owner),source_(std::move(clip)),manager_(std::move(manager)),job_(std::move(job)),progress_(progressValue_){
        title_.setText("Separate into Stems",juce::dontSendNotification);
        mode_.setText("Local CPU - outputs depend on the selected model",juce::dontSendNotification);
        help_.setText("Uses the original mono/stereo WAV range. Track processing is not rendered.\nReset Time / Pitch and detach ARA before separating this clip.",juce::dontSendNotification);
        resultsTitle_.setText("Hidden results in this project - tracks are optional",juce::dontSendNotification);
        footer_.setText("Save the project to persist results. Undo/Redo keeps the same files.",juce::dontSendNotification);
        for(auto* label:{&title_,&sourceInfo_,&mode_,&help_,&status_,&resultsTitle_,&resultInfo_,&footer_,&cpuLabel_,&timing_}){addAndMakeVisible(label);label->setColour(juce::Label::textColourId,owner_.theme.role(ColourRole::text,primaryText));label->setMinimumHorizontalScale(1.0f);}
        for(auto* label:{&help_,&footer_})label->setColour(juce::Label::textColourId,owner_.theme.role(ColourRole::muted,secondaryText));
        for(const auto& model:manager_->status().models)models_.addItem(model.record.title,models_.getNumItems()+1);
        cpuLabel_.setText("CPU threads",juce::dontSendNotification);addAndMakeVisible(cpuLabel_);
        addAndMakeVisible(cpu_);cpu_.addItem("Auto ("+juce::String(chooseCpuThreads())+")",1);
        for(const auto n:{1,2,4,8,16,32})cpu_.addItem(juce::String(n),n+1);
        cpu_.setSelectedId(1,juce::dontSendNotification);cpu_.setTitle("CPU threads for stem separation");
        cpu_.setTooltip("Auto keeps the previous limit of four threads. Manual choices are capped to process affinity (maximum 32). PyTorch selects CPU instructions such as AVX2 automatically.");
        addAndMakeVisible(timing_);timing_.setTitle("Operation elapsed time and configured CPU threads");
        addAndMakeVisible(chunks_);chunks_.addItem("Default chunk",1);chunks_.addItem("Smaller chunk (less memory)",2);chunks_.setSelectedId(1,juce::dontSendNotification);chunks_.setTitle("Processing chunk size");
        addAndMakeVisible(stemsTitle_);stemsTitle_.setText("Outputs to keep",juce::dontSendNotification);addAndMakeVisible(stemsViewport_);stemsViewport_.setViewedComponent(&stemsBody_,false);stemsViewport_.setScrollBarsShown(true,false);
        addAndMakeVisible(selectAll_);selectAll_.setButtonText("All");selectAll_.onClick=[this]{setStemSelection(true);};addAndMakeVisible(selectNone_);selectNone_.setButtonText("None");selectNone_.onClick=[this]{setStemSelection(false);};
        addAndMakeVisible(models_);models_.setSelectedId(1,juce::dontSendNotification);models_.setTitle("Installed separation model");models_.onChange=[this]{message_.clear();update();};
        addAndMakeVisible(results_);results_.setTitle("Hidden separation result");results_.onChange=[this]{update();};
        struct Action{juce::TextButton* button;const char* name;};int focus=1;
        for(const auto& a:std::initializer_list<Action>{{&managerButton_,"Model Manager"},{&start_,"Separate"},{&cancel_,"Cancel operation"},{&place_,"Place on separate tracks"},{&verify_,"Verify result"},{&remove_,"Remove result"}}){addAndMakeVisible(*a.button);a.button->setButtonText(a.name);a.button->setTitle(a.name);a.button->setExplicitFocusOrder(focus++);}
        managerButton_.onClick=[this]{showSeparationModels(owner_,manager_);};start_.onClick=[this]{invoke([&]{beginSeparation();});};cancel_.onClick=[this]{const auto s=job_->status().state;if(s==State::running||s==State::ready){job_->cancel();message_="Cancelling...";}update();};
        place_.onClick=[this]{invoke([&]{beginResult(true);});};verify_.onClick=[this]{invoke([&]{beginResult(false);});};
        remove_.onClick=[this]{invoke([&]{const auto result=selectedResult();ensure(result.has_value(),"Select a result");owner_.app.remove_separation(result->id);owner_.refresh();message_="Result removed. Placed clips and Undo media were preserved.";});};
        details_.setMultiLine(true);details_.setReadOnly(true);details_.setCaretVisible(false);details_.setScrollbarsShown(true);details_.setTitle("Result provenance and file references");addAndMakeVisible(details_);
        addAndMakeVisible(progress_);progress_.setColour(juce::ProgressBar::foregroundColourId,owner_.theme.role(ColourRole::focus,accent));progress_.setTitle("Completed processing chunks");
        setSize(960,920);manager_->refreshIfNeeded();startTimerHz(8);tick();
    }
    ~SeparationPanel()override{stopTimer();job_->cancel();}
    void setSource(std::optional<mrs::Id> clip){if(job_->status().state!=State::running){source_=std::move(clip);message_.clear();update();}}
    void paint(juce::Graphics& g)override{g.fillAll(owner_.theme.role(ColourRole::window,panel));g.setColour(owner_.theme.role(ColourRole::panel,contentSurface));g.fillRoundedRectangle(16.0f,92.0f,static_cast<float>(getWidth()-32),112.0f,6.0f);}
    void resized()override{
        const auto w=getWidth();title_.setBounds(24,12,w-48,28);sourceInfo_.setBounds(24,48,w-48,40);mode_.setBounds(24,96,w-360,24);cpuLabel_.setBounds(w-316,94,108,28);cpu_.setBounds(w-204,94,180,28);
        models_.setBounds(24,128,w-280,28);managerButton_.setBounds(w-240,128,216,28);help_.setBounds(24,160,w-328,40);chunks_.setBounds(w-288,164,264,28);
        stemsTitle_.setBounds(24,208,w-240,24);selectAll_.setBounds(w-200,208,80,24);selectNone_.setBounds(w-112,208,88,24);stemsViewport_.setBounds(24,236,w-48,112);layoutStems();
        start_.setBounds(24,368,192,32);cancel_.setBounds(228,368,160,32);progress_.setBounds(408,372,w-432,24);status_.setBounds(24,408,w-48,40);
        timing_.setBounds(24,448,w-48,24);
        resultsTitle_.setBounds(24,484,w-48,24);results_.setBounds(24,516,w-48,28);resultInfo_.setBounds(24,552,w-48,40);
        place_.setBounds(24,604,248,32);verify_.setBounds(284,604,136,32);remove_.setBounds(432,604,160,32);
        details_.setBounds(24,652,w-48,juce::jmax(40,getHeight()-708));footer_.setBounds(24,getHeight()-44,w-48,28);
    }
    bool keyPressed(const juce::KeyPress& key)override{if(key==juce::KeyPress::escapeKey){job_->cancel();return true;}return false;}
private:
    friend void ::ui::separationProjectSmoke();
    template<class F>void invoke(F action){try{action();}catch(const std::exception& e){message_=juce::String::fromUTF8(e.what());}update();}
    std::optional<mrs::Clip> sourceClip()const{if(!source_)return {};for(const auto& c:owner_.project()->clips)if(c.id==*source_&&!c.midi)return c;return {};}
    std::optional<mrs::SeparationResult> selectedResult()const{const auto id=results_.getSelectedId();if(id<=0||static_cast<std::size_t>(id)>resultIds_.size())return {};for(const auto& r:owner_.project()->separation_results)if(r.id==resultIds_[static_cast<std::size_t>(id-1)])return r;return {};}
    void context(){activeStore_=owner_.app.services().projects;activePath_=owner_.app.path();}
    void beginSeparation(){
        ensure(job_->status().state!=State::running&&!owner_.app.recording(),"Stop recording before separating");
        const auto clip=sourceClip();ensure(clip.has_value(),"Select an audio clip in Arrange");
        ensure(!clip->stretch,"Reset this clip's Time / Pitch before separating");
        ensure(std::none_of(owner_.project()->ara_bindings.begin(),owner_.project()->ara_bindings.end(),[&](const auto& b){return b.source==clip->source;}),"Detach ARA before separating this source");
        const auto models=manager_->status();const auto index=models_.getSelectedId()-1;ensure(index>=0&&static_cast<std::size_t>(index)<models.models.size(),"Select a supported model");
        ProjectInput input;input.cpuThreads=static_cast<unsigned>(juce::jmax(1,cpu_.getSelectedId())-1);input.clip=*clip;input.source=owner_.app.source_file(clip->source);input.projectFile=owner_.app.path();input.rate=owner_.project()->sample_rate;
        input.session=juce::Uuid().toString();input.revision=juce::String(owner_.app.services().projects->state().revision);input.model=models.models[static_cast<std::size_t>(index)].record.id;input.chunk=chunks_.getSelectedId()==2?"small":"default";if(models.models[static_cast<std::size_t>(index)].record.architecture=="MDX")input.chunk="default";input.sourceLifetime=owner_.app.source_lifetime(clip->source);input.sourceGuard=owner_.app.separation_media_guard();
        input.selectedStems=stemSelection();ensure(!input.selectedStems.isEmpty(),"Select at least one output");
        context();expected_=*clip;placeWhenReady_=false;ensure(job_->separate(std::move(input)),"Separation service is busy");message_="Separating the selected clip...";
    }
    void beginResult(bool place){
        ensure(!owner_.app.recording()&&job_->status().state!=State::running,"Finish the current operation first");
        if(place)ensure(owner_.displayState().playback!=mrs::PlaybackState::playing,"Pause playback before placing tracks");
        const auto record=selectedResult();ensure(record.has_value(),"Select a hidden result");std::vector<std::filesystem::path> paths;
        for(const auto& stem:record->stems)paths.push_back(owner_.app.source_file(stem.source));paths.push_back(owner_.app.source_file(record->manifest.source));
        context();expected_.reset();placeWhenReady_=place;ensure(job_->preparePlacement(*record,std::move(paths),owner_.app.separation_media_guard()),"Separation service is busy");message_="Checking project assets...";
    }
    void timerCallback()override{tick();}
    void tick(){
        try{
            if(activeStore_&&(activeStore_!=owner_.app.services().projects||activePath_!=owner_.app.path())){job_->cancel();activeStore_.reset();message_="Project changed; operation cancelled.";source_.reset();expected_.reset();}
            if(expected_&&activeStore_){const auto clip=sourceClip();if(!clip||*clip!=*expected_){job_->cancel();activeStore_.reset();expected_.reset();message_="Source clip changed; separate it again.";}}
            auto state=job_->status();
            if(state.state==State::ready&&!owner_.app.recording()){
                auto out=job_->take();if(out&&activeStore_==owner_.app.services().projects&&activePath_==owner_.app.path()){
                    if(out->result){auto result=out->result;try{owner_.app.keep_separation(result->expected,result->record,result->media);result->media->accepted=true;}
                        catch(...){for(const auto& r:owner_.project()->separation_results)if(r.id==result->record.id)result->media->accepted=true;throw;}
                        message_="Stems kept as hidden project assets. Save the project to persist them.";resultIds_.clear();preferredResult_=result->record.id;owner_.refresh();}
                    else if(out->placement){if(placeWhenReady_){owner_.app.place_separation(out->placement->expected,std::move(out->placement->audio));message_="Stem tracks created. The original clip is unchanged.";owner_.refresh();}else message_="All stem files and the manifest passed verification.";}
                }activeStore_.reset();expected_.reset();
            }else if(state.state==State::failed&&activeStore_){message_="Separation failed: "+jobError(state.error);activeStore_.reset();expected_.reset();}
            else if(state.state==State::cancelled&&activeStore_){message_="Operation cancelled. No result was added.";activeStore_.reset();expected_.reset();}
        }catch(const std::exception& e){message_=juce::String::fromUTF8(e.what());activeStore_.reset();expected_.reset();}
        update();
    }
    juce::StringArray stemSelection()const{juce::StringArray selected;for(const auto& button:stemButtons_)if(button->getToggleState())selected.add(button->getButtonText());return selected;}
    void setStemSelection(bool enabled){for(auto& button:stemButtons_)button->setToggleState(enabled,juce::dontSendNotification);update();}
    void layoutStems(){const int width=juce::jmax(1,stemsViewport_.getWidth()-16),column=width/3;stemsBody_.setSize(width,static_cast<int>((stemButtons_.size()+2)/3)*28);for(std::size_t i=0;i<stemButtons_.size();++i)stemButtons_[i]->setBounds(static_cast<int>(i%3)*column,static_cast<int>(i/3)*28,column,26);}
    void showStems(const ModelRecord& model){if(selectionModel_==model.id)return;selectionModel_=model.id;stemButtons_.clear();for(const auto& role:model.stems){auto button=std::make_unique<juce::ToggleButton>(role);button->setTitle("Keep "+role);button->setToggleState(model.defaultStems.contains(role),juce::dontSendNotification);button->onClick=[this]{update();};stemsBody_.addAndMakeVisible(*button);stemButtons_.push_back(std::move(button));}layoutStems();}
    void update(){update(job_->status());}
    void update(const Status& state){
        const auto project=owner_.project();const bool busy=state.state==State::running||state.state==State::ready;
        const auto clip=sourceClip();sourceInfo_.setText(clip?"Source: "+js(clip->name)+"\nDuration: "+juce::String(static_cast<double>(clip->length)/project->sample_rate,3)+" s | Selected range: "+juce::String(clip->length)+" frames; offset "+juce::String(clip->source_offset):"Select an audio clip in Arrange, then choose Separate into Stems.",juce::dontSendNotification);
        const auto installed=manager_->status();const int index=models_.getSelectedId()-1;
        const bool modelReady=index>=0&&static_cast<std::size_t>(index)<installed.models.size()&&installed.models[static_cast<std::size_t>(index)].state==InstallationState::installed;
        const bool validModel=index>=0&&static_cast<std::size_t>(index)<installed.models.size();
        cpu_.setEnabled(!busy);
        chunks_.setEnabled(!busy&&validModel&&installed.models[static_cast<std::size_t>(index)].record.chunks.contains("small"));
        if(validModel&&!installed.models[static_cast<std::size_t>(index)].record.chunks.contains("small"))chunks_.setSelectedId(1,juce::dontSendNotification);
        mode_.setText(validModel?"Local CPU - "+(installed.models[static_cast<std::size_t>(index)].record.stems.size()>6?juce::String(installed.models[static_cast<std::size_t>(index)].record.stems.size())+" instrument categories":installed.models[static_cast<std::size_t>(index)].record.outputs):"Local CPU",juce::dontSendNotification);
        if(validModel)showStems(installed.models[static_cast<std::size_t>(index)].record);
        const bool hasOutputs=!stemSelection().isEmpty();for(auto& button:stemButtons_)button->setEnabled(!busy);selectAll_.setEnabled(!busy);selectNone_.setEnabled(!busy);
        const bool inputReady=clip&&clip->source!="mrs:demo-tone"&&!clip->stretch&&std::none_of(project->ara_bindings.begin(),project->ara_bindings.end(),[&](const auto& b){return b.source==clip->source;});
        start_.setEnabled(!busy&&!owner_.app.recording()&&!installed.busy&&inputReady&&hasOutputs&&modelReady&&installed.runtime==InstallationState::installed);cancel_.setEnabled(busy);models_.setEnabled(!busy);managerButton_.setEnabled(!busy);progress_.setVisible(busy);
        auto timing=juce::String(busy?"Elapsed: ":"Last operation: ")+juce::String(state.elapsedSeconds,2)+" s";
        if(state.cpuThreads)timing+=" | "+juce::String(state.cpuThreads)+" CPU threads";
        if(state.cpuCapability.isNotEmpty())timing+=" | PyTorch CPU ISA: "+state.cpuCapability;
        if(!busy&&state.elapsedSeconds==0&&state.cpuThreads==0)timing="Not started";
        timing_.setText(timing,juce::dontSendNotification);
        juce::String instruction;
        if(!clip)instruction="Select an audio clip in Arrange.";
        else if(clip->source=="mrs:demo-tone")instruction="Import a stereo WAV first; the demo tone is not a file-backed source.";
        else if(!inputReady)instruction="Reset Time / Pitch and detach ARA before separating this source.";
        else if(owner_.app.recording())instruction="Stop recording before starting or keeping a result.";
        else if(index>=0&&static_cast<std::size_t>(index)<installed.models.size()&&installed.models[static_cast<std::size_t>(index)].state==InstallationState::unsupported)instruction=installed.models[static_cast<std::size_t>(index)].reason;
        else if(!hasOutputs)instruction="Select at least one output to keep.";
        else if(!modelReady||installed.runtime!=InstallationState::installed)instruction="Install or repair the CPU runtime and selected model in Model Manager.";
        else instruction="Ready. CPU processing may take a long time. GPU runtime is not installed.";
        progressValue_=busy&&state.total>0?(static_cast<double>(state.currentPass-1)+static_cast<double>(state.completed)/static_cast<double>(state.total))/static_cast<double>(state.passes):-1.0;
        auto processing="Processing: "+state.phase;
        if(state.total>0)processing+=" - pass "+juce::String(state.currentPass)+"/"+juce::String(state.passes)+", chunks "+juce::String(state.completed)+"/"+juce::String(state.total);
        status_.setText(busy?processing:message_.isNotEmpty()?message_:instruction,juce::dontSendNotification);
        std::vector<mrs::Id> ids;for(const auto& r:project->separation_results)ids.push_back(r.id);
        if(ids!=resultIds_){const auto old=selectedResult();results_.clear(juce::dontSendNotification);resultIds_=ids;int selected=0;for(std::size_t i=0;i<ids.size();++i){const auto& r=project->separation_results[i];juce::String name="Source removed";for(const auto& c:project->clips)if(c.id==r.source_clip)name=js(c.name);results_.addItem(name+" - "+js(r.model)+" - "+js(r.id.value.substr(0,8)),static_cast<int>(i)+1);if((preferredResult_&&r.id==*preferredResult_)||(old&&r.id==old->id))selected=static_cast<int>(i)+1;}
            if(!selected&&!ids.empty())selected=static_cast<int>(ids.size());results_.setSelectedId(selected,juce::dontSendNotification);preferredResult_.reset();}
        const auto record=selectedResult();results_.setEnabled(!busy&&!resultIds_.empty());
        place_.setEnabled(record.has_value()&&!busy&&!owner_.app.recording()&&owner_.displayState().playback!=mrs::PlaybackState::playing);verify_.setEnabled(record.has_value()&&!busy&&!owner_.app.recording());remove_.setEnabled(record.has_value()&&!busy&&!owner_.app.recording());
        juce::StringArray resultRoles;if(record)for(const auto& stem:record->stems)resultRoles.add(js(stem.role));
        resultInfo_.setText(record?resultRoles.joinIntoString(" / ")+" - "+juce::String(record->frames)+" frames at "+juce::String(record->sample_rate)+" Hz\nTimeline start: "+juce::String(record->start)+"; stored independently of Arrange tracks.":"No hidden separation results in this project.",juce::dontSendNotification);
        juce::String details;if(record){details="Model: "+js(record->model)+" / "+js(record->model_version)+"\nSource SHA256: "+js(record->source_sha256)+"\n";for(const auto& f:record->stems)details+="\n"+js(f.role)+": "+js(f.source)+"\nSHA256: "+js(f.sha256)+"\n";details+="\nManifest: "+js(record->manifest.source)+"\n\n"+js(record->provenance);}
        if(details_.getText()!=details)details_.setText(details,false);
    }
    Desktop& owner_;std::optional<mrs::Id> source_,preferredResult_;std::shared_ptr<ModelManager> manager_;std::shared_ptr<ProjectJob> job_;
    std::shared_ptr<mrs::IProjectStore> activeStore_;std::filesystem::path activePath_;std::optional<mrs::Clip> expected_;bool placeWhenReady_{};
    std::vector<mrs::Id> resultIds_;juce::String message_;
    juce::Label title_,sourceInfo_,mode_,help_,status_,resultsTitle_,resultInfo_,footer_;juce::ComboBox models_,results_,chunks_,cpu_;
    juce::Label cpuLabel_,timing_;
    juce::Label stemsTitle_;juce::Viewport stemsViewport_;juce::Component stemsBody_;std::vector<std::unique_ptr<juce::ToggleButton>> stemButtons_;juce::String selectionModel_;juce::TextButton selectAll_,selectNone_;
    juce::TextButton managerButton_,start_,cancel_,place_,verify_,remove_;juce::TextEditor details_;double progressValue_{-1};juce::ProgressBar progress_;
};
class SeparationWindow final:public juce::DocumentWindow {
public:
    SeparationWindow(Desktop& owner,std::optional<mrs::Id> source,std::shared_ptr<ModelManager> manager,std::shared_ptr<ProjectJob> job):DocumentWindow("Separate into Stems",juce::Colour(panel),closeButton),owner_(owner){setUsingNativeTitleBar(true);setLookAndFeel(&owner.theme);setContentOwned(new SeparationPanel(owner,std::move(source),std::move(manager),std::move(job)),true);setResizable(true,false);setResizeLimits(800,820,1280,1200);centreAroundComponent(&owner,960,920);setVisible(true);applyGraphiteWindowCaption(*this);}
    ~SeparationWindow()override{setLookAndFeel(nullptr);}
    void closeButtonPressed()override{owner_.separationWindow.reset();}
private:Desktop& owner_;
};
}
void showSeparation(Desktop& owner,std::optional<mrs::Id> source,std::shared_ptr<ModelManager> manager,std::shared_ptr<ProjectJob> job){
    if(owner.standaloneLive)return;
    if(!source)for(const auto& c:owner.project()->clips)if(!c.midi&&owner.selectedClips.contains(c.id.value)){source=c.id;break;}
    if(owner.separationWindow){if(auto* panelView=dynamic_cast<SeparationPanel*>(owner.separationWindow->getContentComponent()))panelView->setSource(source);owner.separationWindow->toFront(true);return;}
    if(!manager)manager=separationModelManager();if(job)owner.separationJob=std::move(job);if(!owner.separationJob)owner.separationJob=std::make_shared<ProjectJob>(manager);
    owner.separationWindow=std::make_unique<SeparationWindow>(owner,std::move(source),std::move(manager),owner.separationJob);
}
// GUI smoke below uses real handlers/project commands and a synthetic processor.
void separationProjectSmoke(){
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mr-separation-gui-"+juce::Uuid().toString());ensure(root.createDirectory().wasOk(),"Create GUI fixture");
    struct Cleanup{juce::File root;~Cleanup(){root.deleteRecursively();}} cleanup{root};
    const auto wav=[](const juce::File& file,int frames){std::ofstream o(std::filesystem::path(file.getFullPathName().toWideCharPointer()),std::ios::binary);const auto put=[&](std::uint32_t v,int n){for(int i=0;i<n;++i)o.put(static_cast<char>((v>>(8*i))&255));};o.write("RIFF",4);put(36+static_cast<std::uint32_t>(frames)*8,4);o.write("WAVEfmt ",8);put(16,4);put(3,2);put(2,2);put(48000,4);put(384000,4);put(8,2);put(32,2);o.write("data",4);put(static_cast<std::uint32_t>(frames)*8,4);for(int i=0;i<frames*2;++i)put(std::bit_cast<std::uint32_t>(i%2?.25f:-.25f),4);ensure(static_cast<bool>(o),"Write GUI WAV fixture");};
    const auto input=root.getChildFile("source.wav");wav(input,9600);Desktop owner(true);owner.app.import_wav(std::filesystem::path(input.getFullPathName().toWideCharPointer()));auto clip=owner.project()->clips.front();
    struct Range final:mrs::ICommand{mrs::Clip c;explicit Range(mrs::Clip value):c(std::move(value)){}std::string_view name()const override{return "Fixture range";}void apply(mrs::Project& p)const override{p.clips.front()=c;}};
    clip.start=12345;clip.source_offset=1200;clip.length=4800;owner.app.services().projects->execute(Range{clip});
    auto manager=std::make_shared<ModelManager>(root.getChildFile("models"),juce::File{},juce::File{});
    auto slow=std::make_shared<std::atomic<bool>>(false);auto calls=std::make_shared<std::atomic<int>>(0);auto seenThreads=std::make_shared<std::atomic<unsigned>>(0);
    auto processor=[root,wav,slow,calls,seenThreads](const ProjectInput& in,std::stop_token stop){
        seenThreads->store(in.cpuThreads);++*calls;while(slow->load()&&!stop.stop_requested())std::this_thread::sleep_for(std::chrono::milliseconds(2));
        if(stop.stop_requested())throw std::runtime_error("Fixture cancelled");
        const auto id=juce::Uuid().toString().removeCharacters("-");const auto dir=root.getChildFile("mrs-separation-"+id).getChildFile("result");ensure(dir.createDirectory().wasOk(),"Create fixture result");for(const auto& role:selectedOutputs(modelProfile(in.model),in.selectedStems))wav(dir.getChildFile(role.toLowerCase()+".wav"),static_cast<int>(in.clip.length));
        auto json=juce::var(new juce::DynamicObject);const auto set=[&](const char* key,juce::var value){json.getDynamicObject()->setProperty(key,std::move(value));};
        set("model",in.model);set("source_start",juce::String(in.clip.source_offset));set("source_frames",juce::String(in.clip.length));set("project_rate",48000);set("source_rate",48000);set("source_sha256",js(mrs::sha256_file(in.source)));
        juce::Array<juce::var> outputs;for(const auto& label:selectedOutputs(modelProfile(in.model),in.selectedStems)){const auto role=label.toLowerCase();const auto file=dir.getChildFile(role+".wav");auto item=juce::var(new juce::DynamicObject);item.getDynamicObject()->setProperty("stem",role);item.getDynamicObject()->setProperty("path",juce::String(role)+".wav");item.getDynamicObject()->setProperty("frames",juce::String(in.clip.length));item.getDynamicObject()->setProperty("bytes",juce::String(file.getSize()));item.getDynamicObject()->setProperty("sha256",js(mrs::sha256_file(std::filesystem::path(file.getFullPathName().toWideCharPointer()))));outputs.add(item);}set("outputs",juce::var(outputs));ensure(dir.getChildFile("manifest.json").replaceWithText(juce::JSON::toString(json)),"Write fixture manifest");return Result{id,in.session,in.revision,dir,json};
    };
    auto job=std::make_shared<ProjectJob>(manager,processor);showSeparation(owner,clip.id,manager,job);auto* panelView=dynamic_cast<SeparationPanel*>(owner.separationWindow->getContentComponent());ensure(panelView!=nullptr,"Separation window");
    const auto finish=[&]{for(int i=0;i<4000;++i){panelView->tick();if(job->status().state!=State::running&&job->status().state!=State::ready)break;std::this_thread::sleep_for(std::chrono::milliseconds(2));}panelView->tick();ensure(job->status().state!=State::running,"GUI job deadline");};
    ensure(!panelView->start_.isEnabled(),"Missing runtime must disable production Start");
    panelView->cpu_.setSelectedId(2,juce::dontSendNotification);slow->store(true);panelView->start_.onClick();
    for(int i=0;i<500&&seenThreads->load()!=1;++i)std::this_thread::sleep_for(std::chrono::milliseconds(2));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));panelView->tick();
    ensure(seenThreads->load()==1&&job->status().cpuThreads==1&&!panelView->cpu_.isEnabled(),"CPU selection must reach the job and stay disabled while processing");
    ensure(job->status().elapsedSeconds>0&&panelView->timing_.getText().contains("Elapsed:"),"Live operation timer");
    slow->store(false);finish();const auto frozen=job->status().elapsedSeconds;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));panelView->tick();
    ensure(frozen>0&&job->status().elapsedSeconds==frozen&&panelView->timing_.getText().contains("Last operation:")&&panelView->cpu_.isEnabled(),"Completed timer must freeze and CPU selection re-enable");
    ensure(panelView->sourceInfo_.getText().contains("Duration: 0.100 s"),"Exact trimmed clip duration");
    ensure(owner.project()->separation_results.size()==1&&owner.project()->tracks.size()==1&&owner.project()->clips.front()==clip,"Hidden result/default source integrity");
    for(const auto size:{juce::Point<int>{800,820},juce::Point<int>{960,920},juce::Point<int>{1280,1200}}){panelView->setSize(size.x,size.y);for(auto* component:panelView->getChildren())if(component->isVisible())ensure(panelView->getLocalBounds().contains(component->getBounds()),"Separation control bounds");for(float scale:{1.f,1.25f,1.5f,2.f}){const auto image=panelView->createComponentSnapshot(panelView->getLocalBounds(),true,scale);const auto name="separation-project-"+juce::String(size.x)+"x"+juce::String(size.y)+"-"+juce::String(juce::roundToInt(scale*100))+".png";auto out=juce::File::getCurrentWorkingDirectory().getChildFile(name).createOutputStream();ensure(out&&juce::PNGImageFormat().writeImageToStream(image,*out),"Separation GUI snapshot");}}
    Status counted{State::running,{},"inference",{},"cpu"};counted.cpuThreads=8;counted.elapsedSeconds=1.25;counted.cpuCapability="AVX2 (fixture)";counted.completed=5;counted.total=10;counted.currentPass=1;counted.passes=1;panelView->update(counted);
    ensure(panelView->timing_.getText().contains("1.25 s")&&panelView->timing_.getText().contains("8 CPU threads")&&panelView->timing_.getText().contains("AVX2 (fixture)"),"Elapsed/thread/runtime ISA GUI binding");
    ensure(panelView->progressValue_==0.5&&panelView->status_.getText().contains("chunks 5/10"),"Chunk progress GUI binding");
    {const auto image=panelView->createComponentSnapshot(panelView->getLocalBounds());auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("separation-project-progress-50.png").createOutputStream();ensure(stream&&juce::PNGImageFormat().writeImageToStream(image,*stream),"Progress GUI snapshot");}
    counted.completed=1;counted.total=4;counted.currentPass=2;counted.passes=2;panelView->update(counted);ensure(panelView->progressValue_==0.625&&panelView->status_.getText().contains("pass 2/2"),"MDX pass progress GUI binding");
    counted.total=0;counted.phase="verifying";panelView->update(counted);ensure(panelView->progressValue_<0,"Verification must not claim completed inference is a ready result");panelView->update();
    panelView->place_.onClick();finish();ensure(owner.project()->tracks.size()==3&&owner.project()->clips.size()==3,"Place action binding");const auto placed=owner.project()->clips;ensure(owner.app.undo()&&owner.project()->clips.size()==1,"Place one Undo");ensure(owner.app.redo()&&owner.project()->clips==placed,"Place Redo IDs");
    panelView->remove_.onClick();ensure(owner.project()->separation_results.empty()&&owner.project()->clips==placed,"Remove hidden result preserves tracks");ensure(owner.app.undo()&&owner.project()->separation_results.size()==1,"Undo result cleanup");
    slow->store(true);const auto before=*owner.project();panelView->start_.onClick();panelView->cancel_.onClick();finish();ensure(*owner.project()==before,"Cancellation modified project");slow->store(false);
    owner.separationWindow.reset();showSeparation(owner,clip.id,manager,job);panelView=dynamic_cast<SeparationPanel*>(owner.separationWindow->getContentComponent());ensure(panelView&&owner.project()->separation_results.size()==1,"Close/reopen changed results");
    const auto savedFile=root.getChildFile("saved/song.mrsproject");owner.app.save_project(std::filesystem::path(savedFile.getFullPathName().toWideCharPointer()));
    const auto backing=owner.project()->separation_results.front().stems.front().source;
    for(int i=0;i<1000&&!owner.app.waveform(backing);++i){owner.app.poll();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    owner.app.disconnect();
    const auto saved=mrs::persistence::load_project(std::filesystem::path(savedFile.getFullPathName().toWideCharPointer()));const auto lost=savedFile.getParentDirectory().getChildFile(js(saved.project.separation_results.front().stems.front().source));ensure(lost.deleteFile(),"Remove GUI missing-media fixture");
    owner.run([&]{owner.openFile(savedFile);});ensure(owner.project()->separation_results.size()==1&&owner.message.isNotEmpty(),"Missing media must leave project inspectable with an error");
    panelView->tick();panelView->verify_.onClick();finish();ensure(job->status().state==State::failed&&owner.project()->separation_results.size()==1,"Missing stem verification must fail without removing result");
    slow->store(true);panelView->start_.onClick();owner.app.new_project(48000,"New project");panelView->tick();finish();slow->store(false);ensure(owner.project()->separation_results.empty()&&owner.project()->tracks.empty(),"Late result entered another project");owner.separationWindow.reset();
    for(const auto& model:modelCatalogue())if(model.stems.size()>2){
        owner.app.new_project(48000,"Model selection fixture");
        owner.app.import_wav(std::filesystem::path(input.getFullPathName().toWideCharPointer()));
        const auto source=owner.project()->clips.front();showSeparation(owner,source.id,manager,job);panelView=dynamic_cast<SeparationPanel*>(owner.separationWindow->getContentComponent());
        int index=1;for(const auto& candidate:modelCatalogue()){if(candidate.id==model.id)break;++index;}
        panelView->models_.setSelectedId(index,juce::sendNotificationSync);
        ensure(panelView->stemSelection()==model.defaultStems,"Model change updates output checkboxes");
        panelView->start_.onClick();finish();
        ensure(owner.project()->separation_results.size()==1&&owner.project()->separation_results.front().stems.size()==static_cast<std::size_t>(model.defaultStems.size())&&owner.project()->clips.size()==1,"Extended hidden outputs");
        panelView->place_.onClick();finish();ensure(owner.project()->clips.size()==static_cast<std::size_t>(model.defaultStems.size()+1),"Extended Place binding");
        ensure(owner.app.undo()&&owner.project()->clips.size()==1,"Extended placement Undo");ensure(owner.app.redo(),"Extended placement Redo");
        const auto beforeSubset=owner.project()->clips.size();const auto beforeCalls=calls->load();panelView->selectNone_.onClick();panelView->start_.onClick();ensure(calls->load()==beforeCalls&&panelView->message_.contains("Select at least one"),"Empty stem selection must not start inference");
        const auto targetRole=model.stems.contains("Bass")?juce::String("Bass"):juce::String("bass");
        for(auto& button:panelView->stemButtons_)button->setToggleState(button->getButtonText()==targetRole,juce::dontSendNotification);
        panelView->start_.onClick();finish();ensure(owner.project()->separation_results.size()==2&&owner.project()->separation_results.back().stems.size()==1&&owner.project()->separation_results.back().stems.front().role==targetRole.toStdString(),"Bass-only hidden result");
        panelView->place_.onClick();finish();ensure(owner.project()->clips.size()==beforeSubset+1&&owner.project()->clips.back().name==targetRole.toStdString(),"Bass-only placement");ensure(owner.app.undo()&&owner.project()->clips.size()==beforeSubset,"Bass-only placement Undo");ensure(owner.app.redo(),"Bass-only placement Redo");
        panelView->setSize(960,920);const auto image=panelView->createComponentSnapshot(panelView->getLocalBounds());auto out=juce::File::getCurrentWorkingDirectory().getChildFile("separation-project-"+model.id+".png").createOutputStream();ensure(out&&juce::PNGImageFormat().writeImageToStream(image,*out),"Extended GUI snapshot");
        owner.separationWindow.reset();
    }
}
} // namespace ui
