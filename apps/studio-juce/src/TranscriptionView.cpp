#include "TranscriptionView.h"
#include "Desktop.h"
#include "GraphiteWindow.h"
#include <mrs/sha256.hpp>
#include <transcription_engine.h>
#include <algorithm>
#include <atomic>
#include <fstream>
#include <map>

namespace ui {
namespace {
namespace fs=std::filesystem;
using namespace mrs::transcription;
fs::path path(const juce::File& f){return fs::path(f.getFullPathName().toWideCharPointer());}
juce::String text(const std::string& s){return juce::String::fromUTF8(s.c_str());}
void need(bool b,const char* error){if(!b)throw std::runtime_error(error);}
bool busy(JobState s){return s==JobState::running||s==JobState::cancelling;}
bool preview(JobState s){return s==JobState::ready||s==JobState::empty;}
juce::String failure(const std::string& e){
    if(e=="integrity_failure"||e=="invalid_hash")return "Verification failed. Verify the model or reinstall this MR Studio build.";
    if(e=="process_exit"||e=="process_start")return "The native engine could not finish. Check available memory or try a shorter source WAV.";
    if(e=="invalid_output"||e=="invalid_midi")return "The engine returned an invalid result. No MIDI was placed.";
    if(e=="timeout")return "Conversion timed out. Try a shorter source WAV.";
    return text(e);
}
}

// Desktop-owned, persistent across utility-window close/reopen. Background work
// captures values/shared progress, never a Component or Application pointer.
class TranscriptionSetup {
public:
    struct Status {bool busy{},model{},engine{},present{};double progress{};juce::String message;};
    TranscriptionSetup(ModelStore models,Request engine):models_(std::move(models)),engine_(std::move(engine)){check();}
    ~TranscriptionSetup(){shutdown();}
    ModelStore models()const{return models_;}
    Request engine()const{return engine_;}
    Status status()const{auto s=status_;if(job_){const auto total=job_->total.load();s.progress=total?static_cast<double>(job_->completed.load())/static_cast<double>(total):-1.;}return s;}
    void poll(){
        if(!job_||job_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        auto job=std::move(job_);try{status_=job->future.get();}catch(...){status_={false,false,false,false,0,"Model operation failed."};}
    }
    void cancel(){if(job_){job_->stop.request_stop();status_.message="Cancelling model operation...";}}
    void shutdown(){if(job_){job_->stop.request_stop();job_->future.wait();job_.reset();}}
    void check(){begin("Checking installed files...",[](ModelStore,std::stop_token,const ModelDownloadProgress&){});}
    void local(fs::path source){begin("Installing authorized local model...",[source=std::move(source)](ModelStore store,std::stop_token stop,const ModelDownloadProgress&){store.install(source,true,stop);});}
    void remove(){begin("Removing model...",[](ModelStore store,std::stop_token,const ModelDownloadProgress&){store.remove();});}
    void download(std::string token,ModelFetch fixture={}){
        const auto temp=models_.directory().parent_path()/"downloads";
        begin("Downloading official MuScriptor Medium...",[temp,token=std::move(token),fixture=std::move(fixture)](ModelStore store,std::stop_token stop,const ModelDownloadProgress& report)mutable{
            auto fetch=fixture?std::move(fixture):authenticated_model_fetch(std::move(token));token.clear();
            store.download(temp,true,fetch,stop,report);
        });
    }
private:
    struct Job {std::stop_source stop;std::atomic<std::uint64_t> completed{},total{};std::future<Status> future;};
    void begin(juce::String phase,std::function<void(ModelStore,std::stop_token,const ModelDownloadProgress&)> operation){
        poll();need(!job_,"A model operation is already running");auto job=std::make_unique<Job>();
        const auto models=models_;const auto engine=engine_;auto* progress=job.get();const auto stop=job->stop.get_token();
        job->future=std::async(std::launch::async,[models,engine,operation=std::move(operation),progress,stop]()mutable{
            Status out;
            try{operation(models,stop,[progress](auto completed,auto total){progress->total.store(total);progress->completed.store(completed);});}
            catch(const std::exception& e){out.message=stop.stop_requested()?"Model operation cancelled.":failure(e.what());}
            if(stop.stop_requested()){out.message="Model operation cancelled.";return out;}
            try{out.present=fs::exists(models.directory());if(out.present){models.verify(stop);out.model=true;}}
            catch(const std::exception& e){out.message=failure(e.what());}
            try{out.engine=!engine.executable_sha256.empty()&&fs::is_regular_file(engine.executable)&&fs::file_size(engine.executable)<32*1024*1024&&mrs::sha256_file(engine.executable,stop)==engine.executable_sha256;}
            catch(...){out.engine=false;}
            if(out.message.isEmpty())out.message=out.model?"Model verified and available offline.":"Download Medium or choose an authorized local folder.";
            out.progress=1;return out;
        });
        status_.busy=true;status_.message=std::move(phase);job_=std::move(job);
    }
    ModelStore models_;Request engine_;Status status_;std::unique_ptr<Job> job_;
};

namespace {
std::shared_ptr<TranscriptionSetup> setup(){
    const auto app=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    const auto root=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("MoonRiverStudio").getChildFile("transcription");
    Request settings;settings.executable=path(app.getChildFile("ace-midi.exe"));settings.executable_sha256=mrs::transcription_engine_sha256;
    settings.temporary_root=path(root.getChildFile("jobs"));
    // Worker requires a pre-existing trusted temp root; no model download here.
    need(root.getChildFile("jobs").createDirectory().wasOk(),"Cannot create transcription job folder");
    return std::make_shared<TranscriptionSetup>(ModelStore(path(root.getChildFile("models"))),std::move(settings));
}
class TranscriptionPanel final:public juce::Component,private juce::Timer,private juce::ListBoxModel {
public:
    TranscriptionPanel(Desktop& owner,std::optional<mrs::Id> source,std::shared_ptr<TranscriptionSetup> model_setup)
        :owner_(owner),source_(std::move(source)),setup_(std::move(model_setup)),progress_(progress_value_){
        setLookAndFeel(&owner_.theme);
        for(auto* label:{&title_,&source_info_,&model_info_,&help_,&token_label_,&parts_label_,&status_,&message_,&footer_}){addAndMakeVisible(*label);label->setJustificationType(juce::Justification::topLeft);label->setMinimumHorizontalScale(1.f);}
        title_.setText("Convert to MIDI",juce::dontSendNotification);title_.setFont(owner_.theme.font(20.f));
        help_.setText("Analyzes the source WAV and keeps notes inside this clip. Creates ordinary editable MIDI tracks.\nVelocity is the default 100. Render Time/Pitch or ARA processing to a WAV first.",juce::dontSendNotification);
        token_label_.setText("HF read token",juce::dontSendNotification);parts_label_.setText("Recognized parts — Ctrl/Shift to select multiple",juce::dontSendNotification);
        footer_.setText("Place parts to keep them. Closing cancels work and discards an unplaced preview. Source audio is preserved.",juce::dontSendNotification);
        terms_.setButtonText("I have authorized access and accept the official MuScriptor model terms.");terms_.setTitle("Accept model terms");terms_.onClick=[this]{update();};addAndMakeVisible(terms_);
        token_.setPasswordCharacter('*');token_.setTitle("Hugging Face read token, never saved");token_.setTextToShowWhenEmpty("Used only for this official download; never saved",owner_.theme.role(ColourRole::muted,secondaryText));token_.onTextChange=[this]{update();};addAndMakeVisible(token_);
        struct Action{juce::TextButton* button;const char* text;};int focus=1;
        for(const auto& a:std::initializer_list<Action>{{&terms_link_,"Read official terms"},{&download_,"Download Medium"},{&local_,"Install local folder"},{&verify_,"Verify"},{&remove_,"Remove model"},{&start_,"Convert to MIDI"},{&cancel_,"Cancel"},{&discard_,"Discard preview"},{&place_,"Place selected parts"}}){addAndMakeVisible(*a.button);a.button->setButtonText(a.text);a.button->setTitle(a.text);a.button->setExplicitFocusOrder(focus++);}
        terms_link_.onClick=[]{juce::URL("https://huggingface.co/MuScriptor/muscriptor-medium").launchInDefaultBrowser();};
        download_.onClick=[this]{invoke([&]{need(terms_.getToggleState(),"Accept the official model terms first");auto token=token_.getText().trim().toStdString();token_.clear();setup_->download(std::move(token),fixture_fetch_);});};
        local_.onClick=[this]{invoke([&]{need(terms_.getToggleState(),"Accept the official model terms first");chooser_=std::make_unique<juce::FileChooser>("Choose an authorized official Medium folder");juce::Component::SafePointer<TranscriptionPanel> safe(this);chooser_->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe](const juce::FileChooser& chooser){if(safe&&chooser.getResult().isDirectory())safe->invoke([&]{need(safe->terms_.getToggleState(),"Accept the official model terms first");safe->setup_->local(path(chooser.getResult()));});});});};
        verify_.onClick=[this]{invoke([&]{setup_->check();});};remove_.onClick=[this]{invoke([&]{setup_->remove();});};
        start_.onClick=[this]{invoke([&]{need(source_.has_value(),"Select an audio clip");owner_.app.queue_transcription(*source_,setup_->models(),setup_->engine());message_.setText({},juce::dontSendNotification);});};
        cancel_.onClick=[this]{setup_->cancel();owner_.app.cancel_transcription();update();};
        discard_.onClick=[this]{owner_.app.cancel_transcription();update();};
        place_.onClick=[this]{invoke([&]{std::vector<std::string> roles;const auto rows=parts_.getSelectedRows();for(int i=0;i<rows.size();++i)roles.push_back(roles_.at(static_cast<std::size_t>(rows[i])));owner_.app.place_transcription(roles);owner_.refresh();});};
        parts_.setModel(this);parts_.setMultipleSelectionEnabled(true);parts_.setRowHeight(26);parts_.setTitle("Recognized instrument parts");addAndMakeVisible(parts_);
        addAndMakeVisible(progress_);progress_.setTitle("Model download or transcription progress");setSize(760,680);startTimerHz(8);tick();
    }
    ~TranscriptionPanel()override{stopTimer();setup_->cancel();owner_.app.cancel_transcription();parts_.setModel(nullptr);setLookAndFeel(nullptr);}
    void setSource(std::optional<mrs::Id> source){const auto s=owner_.app.transcription_status().state;if(source_!=source&&(busy(s)||preview(s))){message_.setText("Place or discard the current conversion before choosing another clip.",juce::dontSendNotification);return;}source_=std::move(source);update();}
    void paint(juce::Graphics& g)override{g.fillAll(owner_.theme.role(ColourRole::window,panel));}
    void resized()override{
        const auto w=getWidth();title_.setBounds(20,12,w-40,28);source_info_.setBounds(20,48,w-40,38);model_info_.setBounds(20,94,w-40,38);help_.setBounds(20,140,w-40,42);
        terms_.setBounds(20,190,w-190,28);terms_link_.setBounds(w-166,190,146,28);token_label_.setBounds(20,230,104,28);token_.setBounds(132,228,w-152,28);
        download_.setBounds(20,266,160,30);local_.setBounds(188,266,160,30);verify_.setBounds(356,266,90,30);remove_.setBounds(454,266,134,30);
        progress_.setBounds(20,308,w-40,22);status_.setBounds(20,340,w-40,38);parts_label_.setBounds(20,386,w-40,24);parts_.setBounds(20,414,w-40,std::max(70,getHeight()-566));
        const auto y=getHeight()-142;start_.setBounds(20,y,158,32);cancel_.setBounds(186,y,90,32);discard_.setBounds(284,y,142,32);place_.setBounds(w-218,y,198,32);
        message_.setBounds(20,getHeight()-100,w-40,42);footer_.setBounds(20,getHeight()-50,w-40,40);
    }
private:
    friend void ::ui::transcriptionSmoke(const juce::File&);
    int getNumRows()override{return static_cast<int>(roles_.size());}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected)override{
        if(row<0||row>=getNumRows())return;
        g.fillAll(owner_.theme.role(selected?ColourRole::focus:ColourRole::panel,selected?accent:surface).withAlpha(selected?.25f:1.f));g.setColour(owner_.theme.role(ColourRole::text,primaryText));g.setFont(owner_.theme.font(14.f));
        const auto i=static_cast<std::size_t>(row);g.drawText(text(roles_[i]).replaceCharacter('_',' ')+"  —  "+juce::String(static_cast<juce::int64>(counts_[i]))+" notes",8,0,width-16,height,juce::Justification::centredLeft);
    }
    void selectedRowsChanged(int)override{update();}
    template<class F>void invoke(F f){try{f();message_.setText({},juce::dontSendNotification);}catch(const std::exception& e){message_.setText(failure(e.what()),juce::dontSendNotification);}update();}
    void timerCallback()override{tick();}
    void tick(){setup_->poll();owner_.app.poll();update();}
    std::optional<mrs::Clip> source()const{if(source_)for(const auto& c:owner_.project()->clips)if(c.id==*source_&&!c.midi)return c;return {};}
    void update(){
        const auto model=setup_->status();const auto job=owner_.app.transcription_status();const auto clip=source();const auto* result=owner_.app.transcription_preview();
        source_info_.setText(clip?"Source: "+text(clip->name)+"   |   "+juce::String(static_cast<double>(clip->length)/owner_.project()->sample_rate,2)+" s clip":"Select an audio clip in Arrange.",juce::dontSendNotification);
        model_info_.setText("MuScriptor Medium — CPU — 1.23 GB — CC BY-NC 4.0 (non-commercial)\n"+juce::String(model.model?"Model verified/offline":"Model not installed or not verified")+"   |   "+(model.engine?"Native engine verified":"Native engine unavailable; reinstall this build"),juce::dontSendNotification);
        const auto signature=result?result->midi_sha256+result->clip_id.value:std::string{};
        if(signature!=signature_){signature_=signature;roles_.clear();counts_.clear();if(result)for(const auto& role:mrs::ace_midi_instruments()){const auto count=static_cast<std::size_t>(std::count_if(result->notes.begin(),result->notes.end(),[&](const auto& n){return n.instrument==role.role;}));if(count){roles_.push_back(std::string(role.role));counts_.push_back(count);}}parts_.updateContent();parts_.deselectAllRows();if(!roles_.empty())parts_.selectRangeOfRows(0,getNumRows()-1);}
        juce::String status;
        switch(job.state){
        case JobState::running:status=job.total?"Recognizing chunk "+juce::String(job.completed)+" / "+juce::String(job.total):"Preparing source and verifying files...";break;
        case JobState::cancelling:status="Stopping conversion...";break;
        case JobState::ready:status="Recognition ready. Select parts to place on new MIDI tracks.";break;
        case JobState::empty:status="No notes found inside this clip range.";break;
        case JobState::placed:status="Selected MIDI parts placed. Undo/Redo is available.";break;
        case JobState::stale:status="Project or source changed. Start conversion again.";break;
        case JobState::failed:case JobState::timeout:status=failure(job.error);break;
        case JobState::cancelled:status="Conversion cancelled.";break;
        default:status="Ready when a verified model, engine and audio clip are available.";break;
        }
        status_.setText(model.busy?model.message:status,juce::dontSendNotification);
        progress_value_=model.busy?model.progress:job.state==JobState::running?(job.total?static_cast<double>(job.completed)/job.total:-1.):(preview(job.state)||job.state==JobState::placed?1.:0.);
        const bool occupied=busy(job.state),pending=preview(job.state),recording=owner_.app.recording();
        const bool processed=clip&&(clip->stretch||std::any_of(owner_.project()->ara_bindings.begin(),owner_.project()->ara_bindings.end(),[&](const auto& b){return b.source==clip->source;}));
        terms_.setEnabled(!model.busy&&!occupied);token_.setEnabled(!model.busy&&!occupied);
        download_.setEnabled(!model.busy&&!occupied&&!model.present&&terms_.getToggleState()&&!token_.getText().trim().isEmpty());local_.setEnabled(!model.busy&&!occupied&&!model.present&&terms_.getToggleState());
        verify_.setEnabled(!model.busy&&!occupied);remove_.setEnabled(!model.busy&&!occupied&&model.present);
        start_.setEnabled(!model.busy&&!occupied&&!pending&&!recording&&model.model&&model.engine&&clip.has_value()&&!processed);
        cancel_.setEnabled(model.busy||occupied);discard_.setEnabled(pending&&!occupied);place_.setEnabled(job.state==JobState::ready&&!recording&&owner_.displayState().playback!=mrs::PlaybackState::playing&&parts_.getNumSelectedRows()>0);
        if(!model.busy&&model.message!=last_model_message_){last_model_message_=model.message;message_.setText(model.message,juce::dontSendNotification);}
    }
    Desktop& owner_;std::optional<mrs::Id> source_;std::shared_ptr<TranscriptionSetup> setup_;ModelFetch fixture_fetch_;
    juce::Label title_,source_info_,model_info_,help_,token_label_,parts_label_,status_,message_,footer_;
    juce::ToggleButton terms_;juce::TextEditor token_;juce::TextButton terms_link_,download_,local_,verify_,remove_,start_,cancel_,discard_,place_;
    juce::ListBox parts_;std::vector<std::string> roles_;std::vector<std::size_t> counts_;std::string signature_;juce::String last_model_message_;
    double progress_value_{};juce::ProgressBar progress_;std::unique_ptr<juce::FileChooser> chooser_;
};
class TranscriptionWindow final:public juce::DocumentWindow {
public:
    TranscriptionWindow(Desktop& owner,std::optional<mrs::Id> source):DocumentWindow("Convert to MIDI",juce::Colour(panel),closeButton),owner_(owner){setUsingNativeTitleBar(true);setLookAndFeel(&owner.theme);setContentOwned(new TranscriptionPanel(owner,std::move(source),owner.transcriptionSetup),true);setResizable(true,false);setResizeLimits(740,680,1100,900);centreAroundComponent(&owner,760,680);setVisible(true);applyGraphiteWindowCaption(*this);}
    ~TranscriptionWindow()override{setLookAndFeel(nullptr);}
    void closeButtonPressed()override{owner_.transcriptionWindow.reset();}
private:Desktop& owner_;
};
}
void showTranscription(Desktop& owner,std::optional<mrs::Id> source){
    if(owner.standaloneLive)return;
    if(!source)for(const auto& c:owner.project()->clips)if(!c.midi&&owner.selectedClips.contains(c.id.value)){source=c.id;break;}
    if(owner.transcriptionWindow){if(auto* view=dynamic_cast<TranscriptionPanel*>(owner.transcriptionWindow->getContentComponent()))view->setSource(source);owner.transcriptionWindow->toFront(true);return;}
    if(!owner.transcriptionSetup)owner.transcriptionSetup=setup();owner.transcriptionWindow=std::make_unique<TranscriptionWindow>(owner,std::move(source));
}
void closeTranscription(Desktop& owner){owner.transcriptionWindow.reset();if(owner.transcriptionSetup)owner.transcriptionSetup->shutdown();owner.transcriptionSetup.reset();}
void transcriptionSmoke(const juce::File& fake){
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mrs-transcription-gui-"+juce::Uuid().toString());need(root.createDirectory().wasOk(),"Create transcription smoke root");
    struct Cleanup{juce::File root;~Cleanup(){root.deleteRecursively();}} cleanup{root};
    const auto source=root.getChildFile("source.wav");
    const auto wav=[](const juce::File& f,int mode){std::ofstream out(path(f),std::ios::binary);const auto put=[&](unsigned n,int count){for(int i=0;i<count;++i)out.put(static_cast<char>((n>>(8*i))&255));};out.write("RIFF",4);put(32036,4);out.write("WAVEfmt ",8);put(16,4);put(1,2);put(1,2);put(16000,4);put(32000,4);put(2,2);put(16,2);out.write("data",4);put(32000,4);out.put(static_cast<char>(mode));for(int i=1;i<32000;++i)out.put(0);};wav(source,0);
    const auto authorized=root.getChildFile("authorized");need(authorized.createDirectory().wasOk(),"Create model fixture");
    need(authorized.getChildFile("config.json").replaceWithText("{}"),"Fixture config");need(authorized.getChildFile("model.safetensors").replaceWithText("fixture"),"Fixture weights");need(authorized.getChildFile("README.md").replaceWithText("Self-generated fixture"),"Fixture terms");
    ModelSpec spec{"gui-fixture",std::string(40,'a'),"Self-generated fixture terms",{}};
    for(const auto* name:{"config.json","model.safetensors","README.md"}){const auto file=path(authorized.getChildFile(name));spec.files.push_back({name,mrs::sha256_file(file),fs::file_size(file)});}
    const auto jobs=root.getChildFile("jobs");need(jobs.createDirectory().wasOk(),"Fixture job folder");
    Request engine;engine.executable=path(fake);engine.executable_sha256=mrs::sha256_file(engine.executable);engine.temporary_root=path(jobs);engine.timeout=std::chrono::seconds(5);
    Desktop owner(true);owner.app.new_project(16000);owner.app.import_wavs({path(source)});owner.refresh();
    const auto clip=owner.project()->clips.front().id;const auto before=*owner.project();
    owner.transcriptionSetup=std::make_shared<TranscriptionSetup>(ModelStore(path(root.getChildFile("models")),spec),engine);
    showTranscription(owner,clip);auto* view=dynamic_cast<TranscriptionPanel*>(owner.transcriptionWindow->getContentComponent());need(view!=nullptr,"Transcription panel missing");
    const auto wait=[&](const std::function<bool()>& pending){const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(pending()){need(std::chrono::steady_clock::now()<until,"Transcription GUI timeout");view->tick();juce::Thread::sleep(2);}};
    wait([&]{return owner.transcriptionSetup->status().busy;});view->tick();need(!view->start_.isEnabled()&&!view->download_.isEnabled(),"Uninstalled/unchecked state");
    view->fixture_fetch_=[authorized](const std::string& url,const fs::path& destination,std::uint64_t bytes,std::stop_token stop,const ModelDownloadProgress& progress){need(model_download_target(url).authorize,"Fixture official origin");need(!stop.stop_requested(),"Fixture download cancelled");const auto source_file=path(authorized.getChildFile(juce::String(destination.filename().wstring())));fs::copy_file(source_file,destination);if(progress)progress(bytes,bytes);};
    view->terms_.setToggleState(true,juce::dontSendNotification);view->token_.setText("fake_test_token",false);view->update();need(view->download_.isEnabled(),"Explicit model download action disabled");view->download_.onClick();need(view->token_.getText().isEmpty(),"Token editor not cleared");
    wait([&]{return owner.transcriptionSetup->status().busy;});view->tick();need(view->start_.isEnabled(),"Verified engine/model should enable conversion");
    view->start_.onClick();wait([&]{return busy(owner.app.transcription_status().state);});view->tick();need(owner.app.transcription_status().state==JobState::ready&&view->roles_.size()==1&&view->place_.isEnabled(),"Recognition/parts selection missing");
    view->parts_.deselectAllRows();view->update();need(!view->place_.isEnabled(),"Empty selection must not place");view->parts_.selectRow(0);view->update();
    for(const auto scale:{1.,1.25,1.5,2.}){
        auto image=view->createComponentSnapshot(view->getLocalBounds(),true,static_cast<float>(scale));need(image.isValid(),"Transcription software preview");
        juce::PNGImageFormat format;const auto file=juce::File::getCurrentWorkingDirectory().getChildFile("transcription-"+juce::String(static_cast<int>(scale*100))+".png");auto out=file.createOutputStream();need(out&&format.writeImageToStream(image,*out),"Write transcription preview");
    }
    for(const auto size:{std::pair{740,680},std::pair{760,720},std::pair{1000,820}}){view->setSize(size.first,size.second);for(int i=0;i<view->getNumChildComponents();++i){auto* c=view->getChildComponent(i);need(view->getLocalBounds().contains(c->getBounds()),"Transcription control outside panel");}}
    view->place_.onClick();need(owner.app.transcription_status().state==JobState::placed&&owner.project()->clips.size()==before.clips.size()+1,"MIDI placement handler failed");const auto after=*owner.project();need(owner.app.undo()&&*owner.project()==before&&owner.app.redo()&&*owner.project()==after,"GUI MIDI Undo/Redo");
    view->start_.onClick();wait([&]{return busy(owner.app.transcription_status().state);});owner.app.add_audio_track("Changed");view->tick();need(owner.app.transcription_status().state==JobState::stale&&!view->place_.isEnabled(),"Stale result should not place");
    wav(source,1);owner.app.new_project(16000);owner.app.import_wavs({path(source)});const auto new_clip=owner.project()->clips.front().id;view->setSource(new_clip);view->tick();view->start_.onClick();view->cancel_.onClick();wait([&]{return busy(owner.app.transcription_status().state);});need(owner.project()->clips.size()==1,"Cancel mutated project");
    owner.transcriptionWindow.reset();showTranscription(owner,new_clip);need(owner.transcriptionWindow!=nullptr,"Transcription reopen failed");closeTranscription(owner);
    need(fs::is_empty(path(jobs)),"Transcription GUI job cleanup");
}
}
