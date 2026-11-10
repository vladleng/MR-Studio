#include "TranscriptionView.h"
#include "Desktop.h"
#include "GraphiteWindow.h"
#include "TranscriptionRuntime.h"
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
    struct Status {bool busy{},model{},engine{},present{},runtime{},runtime_present{};double progress{};juce::String message;};
    TranscriptionSetup(ModelStore models,Request engine,std::shared_ptr<TranscriptionRuntime> runtime={}):models_(std::move(models)),engine_(std::move(engine)),runtime_(std::move(runtime)){check();}
    ~TranscriptionSetup(){shutdown();}
    ModelStore models()const{return models_;}
    Request engine()const{return engine_;}
    Request pythonEngine()const{
        need(runtime_!=nullptr,"Python runtime unavailable");auto r=engine_;r.backend=Backend::python_medium;
        r.executable=path(runtime_->python());r.executable_sha256=mrs::sha256_file(r.executable);r.memory_bytes=8ULL*1024*1024*1024;
        r.runtime_lifetime=runtime_->acquire();auto runtime=runtime_;r.verify_runtime=[runtime](std::stop_token stop){runtime->verify(stop);};return r;
    }
    void runtimeOperation(int operation){
        need(runtime_!=nullptr,"Python runtime unavailable");auto runtime=runtime_;
        begin(operation==0?"Installing isolated Python Medium runtime (this can take several minutes)...":operation==1?"Verifying Python Medium runtime...":"Removing Python Medium runtime...",[runtime,operation](ModelStore,std::stop_token stop,const ModelDownloadProgress&){if(operation==0)runtime->install(stop);else if(operation==1)runtime->verify(stop);else runtime->remove();});
    }
    Status status()const{auto s=status_;if(job_){const auto total=job_->total.load();s.progress=total?static_cast<double>(job_->completed.load())/static_cast<double>(total):-1.;}return s;}
    void poll(){
        if(!job_||job_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        auto job=std::move(job_);try{status_=job->future.get();}catch(...){status_={};status_.message="Model operation failed.";}
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
        const auto models=models_;const auto engine=engine_;const auto runtime=runtime_;auto* progress=job.get();const auto stop=job->stop.get_token();
        job->future=std::async(std::launch::async,[models,engine,runtime,operation=std::move(operation),progress,stop]()mutable{
            Status out;
            try{operation(models,stop,[progress](auto completed,auto total){progress->total.store(total);progress->completed.store(completed);});}
            catch(const std::exception& e){out.message=stop.stop_requested()?"Model operation cancelled.":failure(e.what());}
            if(stop.stop_requested()){out.message="Model operation cancelled.";return out;}
            try{out.present=fs::exists(models.directory());if(out.present){models.verify(stop);out.model=true;}}
            catch(const std::exception& e){out.message=failure(e.what());}
            try{out.engine=!engine.executable_sha256.empty()&&fs::is_regular_file(engine.executable)&&fs::file_size(engine.executable)<32*1024*1024&&mrs::sha256_file(engine.executable,stop)==engine.executable_sha256;}
            catch(...){out.engine=false;}
            if(out.message.isEmpty())out.message=out.model?"Model verified and available offline.":"Download Medium or choose an authorized local folder.";
            if(runtime){out.runtime_present=runtime->present();out.runtime=runtime->python().existsAsFile()&&runtime->directory().getChildFile("managed.json").existsAsFile();}
            out.progress=1;return out;
        });
        status_.busy=true;status_.message=std::move(phase);job_=std::move(job);
    }
    ModelStore models_;Request engine_;std::shared_ptr<TranscriptionRuntime> runtime_;Status status_;std::unique_ptr<Job> job_;
};

namespace {
std::shared_ptr<TranscriptionSetup> setup(){
    const auto app=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    const auto root=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("MoonRiverStudio").getChildFile("transcription");
    Request settings;settings.executable=path(app.getChildFile("ace-midi.exe"));settings.executable_sha256=mrs::transcription_engine_sha256;settings.cpu_threads=0;
    if(optimized_cpu_available()&&std::string(mrs::transcription_avx2_sha256).size()==64&&app.getChildFile("ace-midi-avx2.exe").existsAsFile()){
        settings.executable=path(app.getChildFile("ace-midi-avx2.exe"));settings.executable_sha256=mrs::transcription_avx2_sha256;
    }
    settings.worker_script=path(app.getChildFile("mr_transcription_worker.py"));settings.worker_sha256=mrs::transcription_worker_sha256;
    settings.temporary_root=path(root.getChildFile("jobs"));
    // Worker requires a pre-existing trusted temp root; no model download here.
    need(root.getChildFile("jobs").createDirectory().wasOk(),"Cannot create transcription job folder");
    return std::make_shared<TranscriptionSetup>(ModelStore(path(root.getChildFile("models"))),std::move(settings),std::make_shared<TranscriptionRuntime>(root,app.getChildFile("mr_transcription_requirements.txt")));
}
class TranscriptionPanel final:public juce::Component,private juce::Timer,private juce::ListBoxModel {
public:
    TranscriptionPanel(Desktop& owner,std::optional<mrs::Id> source,std::shared_ptr<TranscriptionSetup> model_setup)
        :owner_(owner),source_(std::move(source)),setup_(std::move(model_setup)),progress_(progress_value_){
        setLookAndFeel(&owner_.theme);
        for(auto* label:{&title_,&source_info_,&model_info_,&help_,&token_label_,&parts_label_,&status_,&message_,&footer_,&cpu_label_,&stem_label_}){addAndMakeVisible(*label);label->setJustificationType(juce::Justification::topLeft);label->setMinimumHorizontalScale(1.f);}
        title_.setText("Convert to MIDI",juce::dontSendNotification);title_.setFont(owner_.theme.font(20.f));
        help_.setText("Analyzes only this trimmed clip. Creates ordinary editable MIDI tracks; default velocity 100.\nStem instrument constrains decoding, not accuracy. Render Time/Pitch or ARA to a WAV first.",juce::dontSendNotification);
        cpu_label_.setText("CPU threads",juce::dontSendNotification);stem_label_.setText("Stem instrument",juce::dontSendNotification);
        cpu_.addItem("Auto ("+juce::String(choose_cpu_threads())+")",1);for(int n=1;n<=32;++n)cpu_.addItem(juce::String(n),n+1);cpu_.setSelectedId(owner_.view.transcriptionCpuThreads+1,juce::dontSendNotification);cpu_.setTitle("CPU threads for background inference");
        cpu_.setTooltip("Auto reserves one logical CPU, up to 16 threads. Manual choices are capped to available CPUs (maximum 32).");
        stem_.addItem("Auto / full mix",1);int role_id=2;for(const auto& role:mrs::ace_midi_instruments())stem_.addItem(text(std::string(role.role)).replaceCharacter('_',' '),role_id++);stem_.setSelectedId(1,juce::dontSendNotification);stem_.setTitle("Instrument of the input stem");stem_.setTooltip("For an isolated stem, constrain native decoding to this category. No separation, pitch shift or monophonic forcing; accuracy still needs checking.");
        addAndMakeVisible(cpu_);addAndMakeVisible(stem_);
        backend_.addItem("Python MuScriptor Medium (main)",1);backend_.addItem("HOT-Step C++ (experimental A/B)",2);backend_.setSelectedId(owner_.view.transcriptionBackend,juce::dontSendNotification);backend_.setTitle("Recognition backend");addAndMakeVisible(backend_);
        compatibility_.setButtonText("Compatibility CPU (disable AVX2 dispatch)");compatibility_.setToggleState(owner_.view.transcriptionCompatibility,juce::dontSendNotification);addAndMakeVisible(compatibility_);
        const auto persist=[this]{owner_.view.transcriptionCpuThreads=cpu_.getSelectedId()-1;owner_.view.transcriptionBackend=backend_.getSelectedId();owner_.view.transcriptionCompatibility=compatibility_.getToggleState();owner_.saveSettings();update();};
        cpu_.onChange=persist;backend_.onChange=persist;compatibility_.onClick=persist;
        runtime_install_.setButtonText("Install Python runtime");runtime_verify_.setButtonText("Verify runtime");runtime_remove_.setButtonText("Remove runtime");
        for(auto* b:{&runtime_install_,&runtime_verify_,&runtime_remove_})addAndMakeVisible(*b);
        runtime_install_.onClick=[this]{invoke([&]{setup_->runtimeOperation(0);});};runtime_verify_.onClick=[this]{invoke([&]{setup_->runtimeOperation(1);});};runtime_remove_.onClick=[this]{invoke([&]{setup_->runtimeOperation(2);});};
        instruments_.setButtonText("Multiple instruments: Auto");instruments_.setTitle("Choose one or more recognized instruments");addAndMakeVisible(instruments_);instruments_.onClick=[this]{chooseInstruments();};
        for(auto* toggle:{&detect_,&quantize_,&follow_,&trim_,&original_,&import_tempo_})addAndMakeVisible(*toggle);
        detect_.setButtonText("Detect BPM / meter");quantize_.setButtonText("Quantize");follow_.setButtonText("Follow tempo map");follow_.setToggleState(true,juce::dontSendNotification);trim_.setButtonText("Trim leading bars");original_.setButtonText("Place original timing");original_.onClick=[this]{update();};
        for(auto* edit:{&bpm_,&meter_,&strength_,&gaps_,&downbeat_,&scale_}){addAndMakeVisible(*edit);edit->setInputRestrictions(10,"-0123456789.");}
        bpm_.setText("0");bpm_.setTitle("BPM: 0 keeps detected or project tempo");meter_.setText("0");meter_.setTitle("Beats per bar: 0 keeps detected meter");strength_.setText("1");strength_.setTitle("Quantize strength: 0 to 1");gaps_.setText("0");gaps_.setTitle("Fill gaps: maximum beats, 0 disables");downbeat_.setText("-1");downbeat_.setTitle("First downbeat in clip seconds: -1 keeps detected");scale_.setText("1");scale_.setTitle("Beat scale: 0.25, 0.5, 1, 2 or 4");
        for(const auto n:{1,2,3,4,6,8,12,16,24})grid_.addItem(juce::String(n)+" steps/beat",n);grid_.setSelectedId(4,juce::dontSendNotification);grid_.setTitle("Quantize grid (3/6/12 are triplets)");addAndMakeVisible(grid_);
        import_tempo_.setButtonText("Import project tempo / meter");import_tempo_.setTooltip("Explicitly replaces tempo inside this clip range and restores the previous tempo afterwards. Align clip start with a bar boundary to import meter. Undo restores the previous map.");
        music_label_.setText("BPM (0=auto)     Meter    Strength     Fill gaps     Downbeat (s)     Beat scale",juce::dontSendNotification);addAndMakeVisible(music_label_);

        token_label_.setText("HF read token",juce::dontSendNotification);parts_label_.setText("Recognized parts - Ctrl/Shift to select multiple",juce::dontSendNotification);
        footer_.setText("Processed results keep muted original-timing reference tracks. Closing cancels work and discards an unplaced preview. Source audio is preserved.",juce::dontSendNotification);
        terms_.setButtonText("I have authorized access and accept the official MuScriptor model terms.");terms_.setTitle("Accept model terms");terms_.onClick=[this]{update();};addAndMakeVisible(terms_);
        token_.setPasswordCharacter('*');token_.setTitle("Hugging Face read token, never saved");token_.setTextToShowWhenEmpty("Used only for this official download; never saved",owner_.theme.role(ColourRole::muted,secondaryText));token_.onTextChange=[this]{update();};addAndMakeVisible(token_);
        struct Action{juce::TextButton* button;const char* text;};int focus=1;
        for(const auto& a:std::initializer_list<Action>{{&terms_link_,"Read official terms"},{&download_,"Download Medium"},{&local_,"Install local folder"},{&verify_,"Verify"},{&remove_,"Remove model"},{&start_,"Convert to MIDI"},{&cancel_,"Cancel"},{&discard_,"Discard preview"},{&place_,"Place selected parts"}}){addAndMakeVisible(*a.button);a.button->setButtonText(a.text);a.button->setTitle(a.text);a.button->setExplicitFocusOrder(focus++);}
        terms_link_.onClick=[]{juce::URL("https://huggingface.co/MuScriptor/muscriptor-medium").launchInDefaultBrowser();};
        download_.onClick=[this]{invoke([&]{need(terms_.getToggleState(),"Accept the official model terms first");auto token=token_.getText().trim().toStdString();token_.clear();setup_->download(std::move(token),fixture_fetch_);});};
        local_.onClick=[this]{invoke([&]{need(terms_.getToggleState(),"Accept the official model terms first");chooser_=std::make_unique<juce::FileChooser>("Choose an authorized official Medium folder");juce::Component::SafePointer<TranscriptionPanel> safe(this);chooser_->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe](const juce::FileChooser& chooser){if(safe&&chooser.getResult().isDirectory())safe->invoke([&]{need(safe->terms_.getToggleState(),"Accept the official model terms first");safe->setup_->local(path(chooser.getResult()));});});});};
        verify_.onClick=[this]{invoke([&]{setup_->check();});};remove_.onClick=[this]{invoke([&]{setup_->remove();});};
        start_.onClick=[this]{invoke([&]{need(source_.has_value(),"Select an audio clip");auto request=backend_.getSelectedId()==1?setup_->pythonEngine():setup_->engine();request.compatibility_cpu=compatibility_.getToggleState();
            if(request.backend==Backend::native_experimental&&request.compatibility_cpu){const auto app=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();request.executable=path(app.getChildFile("ace-midi.exe"));request.executable_sha256=mrs::transcription_engine_sha256;}
            request.cpu_threads=static_cast<unsigned>((std::max)(1,cpu_.getSelectedId())-1);
            if(stem_.getSelectedId()>1)request.stem_instrument=std::string(mrs::ace_midi_instruments()[static_cast<std::size_t>(stem_.getSelectedId()-2)].role);
            if(request.backend==Backend::python_medium){request.instruments=instruments_selected_;if(request.instruments.empty()&&!request.stem_instrument.empty())request.instruments.push_back(request.stem_instrument);request.stem_instrument.clear();
                request.musical.import_tempo=import_tempo_.getToggleState();request.musical.detect_tempo=detect_.getToggleState();request.musical.quantize=quantize_.getToggleState();request.musical.tempo_map=follow_.getToggleState();request.musical.trim_leading_bars=trim_.getToggleState();request.musical.bpm=bpm_.getText().getDoubleValue();request.musical.meter=static_cast<unsigned>(meter_.getText().getIntValue());request.musical.strength=strength_.getText().getDoubleValue();request.musical.fill_gaps=gaps_.getText().getDoubleValue();request.musical.first_downbeat=downbeat_.getText().getDoubleValue();request.musical.beat_scale=scale_.getText().getDoubleValue();request.musical.subdivision=static_cast<unsigned>(grid_.getSelectedId());
                if(request.musical.bpm==0&&!request.musical.detect_tempo){const auto current=source();need(current.has_value(),"Choose an audio clip");const auto tick=mrs::Timeline(owner_.project()->time,owner_.project()->sample_rate).to_ticks(current->start);for(const auto& tempo:owner_.project()->time.tempos){if(tempo.tick>tick)break;request.musical.bpm=tempo.bpm;}}}
            owner_.app.queue_transcription(*source_,setup_->models(),std::move(request));message_.setText({},juce::dontSendNotification);});};
        cancel_.onClick=[this]{setup_->cancel();owner_.app.cancel_transcription();update();};
        discard_.onClick=[this]{owner_.app.cancel_transcription();update();};
        place_.onClick=[this]{invoke([&]{std::vector<std::string> roles;const auto rows=parts_.getSelectedRows();for(int i=0;i<rows.size();++i)roles.push_back(roles_.at(static_cast<std::size_t>(rows[i])));owner_.app.place_transcription(roles,original_.getToggleState());owner_.refresh();});};
        parts_.setModel(this);parts_.setMultipleSelectionEnabled(true);parts_.setRowHeight(26);parts_.setTitle("Recognized instrument parts");addAndMakeVisible(parts_);
        addAndMakeVisible(progress_);progress_.setTitle("Model download or transcription progress");setSize(800,960);startTimerHz(8);tick();
    }
    ~TranscriptionPanel()override{stopTimer();setup_->cancel();owner_.app.cancel_transcription();parts_.setModel(nullptr);setLookAndFeel(nullptr);}
    void setSource(std::optional<mrs::Id> source){const auto s=owner_.app.transcription_status().state;if(source_!=source&&(busy(s)||preview(s))){message_.setText("Place or discard the current conversion before choosing another clip.",juce::dontSendNotification);return;}source_=std::move(source);update();}
    void paint(juce::Graphics& g)override{g.fillAll(owner_.theme.role(ColourRole::window,panel));}
    void resized()override{
        const auto w=getWidth();title_.setBounds(20,12,w-40,28);source_info_.setBounds(20,48,w-40,38);model_info_.setBounds(20,94,w-40,38);help_.setBounds(20,140,w-40,42);
        terms_.setBounds(20,190,w-190,28);terms_link_.setBounds(w-166,190,146,28);token_label_.setBounds(20,230,104,28);token_.setBounds(132,228,w-152,28);
        download_.setBounds(20,266,160,30);local_.setBounds(188,266,160,30);verify_.setBounds(356,266,90,30);remove_.setBounds(454,266,134,30);
        backend_.setBounds(20,306,350,28);compatibility_.setBounds(384,306,w-404,28);
        cpu_label_.setBounds(20,346,100,28);cpu_.setBounds(124,344,110,28);stem_label_.setBounds(248,346,116,28);stem_.setBounds(370,344,w-390,28);
        instruments_.setBounds(20,382,w-40,28);
        runtime_install_.setBounds(20,420,200,28);runtime_verify_.setBounds(232,420,150,28);runtime_remove_.setBounds(394,420,150,28);
        detect_.setBounds(20,458,180,28);follow_.setBounds(208,458,180,28);quantize_.setBounds(394,458,100,28);grid_.setBounds(504,458,w-524,28);
        music_label_.setBounds(20,498,w-40,22);int x=20;for(auto* edit:{&bpm_,&meter_,&strength_,&gaps_,&downbeat_,&scale_}){edit->setBounds(x,522,100,28);x+=116;}
        trim_.setBounds(20,560,180,28);original_.setBounds(210,560,200,28);import_tempo_.setBounds(420,560,w-440,28);
        progress_.setBounds(20,600,w-40,22);status_.setBounds(20,632,w-40,38);parts_label_.setBounds(20,680,w-40,24);parts_.setBounds(20,708,w-40,(std::max)(70,getHeight()-860));
        const auto y=getHeight()-142;start_.setBounds(20,y,158,32);cancel_.setBounds(186,y,90,32);discard_.setBounds(284,y,142,32);place_.setBounds(w-218,y,198,32);
        message_.setBounds(20,getHeight()-100,w-40,42);footer_.setBounds(20,getHeight()-50,w-40,40);
    }
private:
    void chooseInstruments(){
        juce::PopupMenu menu;menu.addItem(1,"Auto (all instruments)",true,instruments_selected_.empty());int id=2;
        for(const auto& role:mrs::ace_midi_instruments()){const auto name=std::string(role.role);menu.addItem(id++,text(name).replaceCharacter('_',' '),true,std::find(instruments_selected_.begin(),instruments_selected_.end(),name)!=instruments_selected_.end());}
        juce::Component::SafePointer<TranscriptionPanel> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(instruments_),[safe](int selected){if(!safe||!selected)return;
            if(selected==1){safe->instruments_selected_.clear();safe->stem_.setSelectedId(1,juce::dontSendNotification);}else{const auto name=std::string(mrs::ace_midi_instruments()[static_cast<std::size_t>(selected-2)].role);auto& list=safe->instruments_selected_;const auto at=std::find(list.begin(),list.end(),name);if(at==list.end())list.push_back(name);else list.erase(at);}
            juce::StringArray names;for(const auto& name:safe->instruments_selected_)names.add(text(name));safe->instruments_.setButtonText("Multiple instruments: "+(names.isEmpty()?juce::String("Auto"):names.joinIntoString(", ")));safe->update();});
    }
    friend void ::ui::transcriptionSmoke(const juce::File&);
    int getNumRows()override{return static_cast<int>(roles_.size());}
    void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected)override{
        if(row<0||row>=getNumRows())return;
        g.fillAll(owner_.theme.role(selected?ColourRole::focus:ColourRole::panel,selected?accent:surface).withAlpha(selected?.25f:1.f));g.setColour(owner_.theme.role(ColourRole::text,primaryText));g.setFont(owner_.theme.font(14.f));
        const auto i=static_cast<std::size_t>(row);g.drawText(text(roles_[i]).replaceCharacter('_',' ')+"  -  "+juce::String(static_cast<juce::int64>(counts_[i]))+" notes",8,0,width-16,height,juce::Justification::centredLeft);
    }
    void selectedRowsChanged(int)override{update();}
    template<class F>void invoke(F f){try{f();message_.setText({},juce::dontSendNotification);}catch(const std::exception& e){message_.setText(failure(e.what()),juce::dontSendNotification);}update();}
    void timerCallback()override{tick();}
    void tick(){setup_->poll();owner_.app.poll();update();}
    std::optional<mrs::Clip> source()const{if(source_)for(const auto& c:owner_.project()->clips)if(c.id==*source_&&!c.midi)return c;return {};}
    void update(){
        const auto model=setup_->status();const auto job=owner_.app.transcription_status();const auto clip=source();const auto* result=owner_.app.transcription_preview();
        source_info_.setText(clip?"Clip to analyze: "+text(clip->name)+"   |   "+juce::String(static_cast<double>(clip->length)/owner_.project()->sample_rate,2)+" s (trimmed clip only)":"Select an audio clip in Arrange.",juce::dontSendNotification);
        const bool python=backend_.getSelectedId()==1;
        model_info_.setText("MuScriptor Medium - CPU - 1.23 GB - CC BY-NC 4.0 (non-commercial)\n"+juce::String(model.model?"Model verified/offline":"Model not installed or not verified")+"   |   "+(model.engine?(setup_->engine().executable.filename()=="ace-midi-avx2.exe"?"Native CPU AVX2 verified":"Native CPU compatibility verified"):"Native engine unavailable; reinstall this build")+juce::String(python?(model.runtime?" | Python runtime installed":" | Install Python runtime"):" | Experimental A/B"),juce::dontSendNotification);
        const auto signature=result?result->midi_sha256+result->clip_id.value+(original_.getToggleState()?"raw":"processed"):std::string{};
        if(signature!=signature_){signature_=signature;roles_.clear();counts_.clear();if(result)for(const auto& role:mrs::ace_midi_instruments()){const auto count=static_cast<std::size_t>(std::count_if((original_.getToggleState()?result->original_notes:result->notes).begin(),(original_.getToggleState()?result->original_notes:result->notes).end(),[&](const auto& n){return n.instrument==role.role;}));if(count){roles_.push_back(std::string(role.role));counts_.push_back(count);}}parts_.updateContent();parts_.deselectAllRows();if(!roles_.empty())parts_.selectRangeOfRows(0,getNumRows()-1);}
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
        if(busy(job.state)||preview(job.state)||job.state==JobState::placed)status+=" | "+juce::String(static_cast<int>(job.elapsed_seconds))+" s elapsed / "+juce::String(job.cpu_threads)+" threads";
        if(result&&python)status+=" | "+text(result->cpu_capability)+" | load "+juce::String(result->model_load_seconds,1)+" s / inference "+juce::String(result->inference_seconds,1)+" s";
        status_.setText(model.busy?model.message:status,juce::dontSendNotification);
        progress_value_=model.busy?model.progress:job.state==JobState::running?(job.total?static_cast<double>(job.completed)/job.total:-1.):(preview(job.state)||job.state==JobState::placed?1.:0.);
        const bool occupied=busy(job.state),pending=preview(job.state),recording=owner_.app.recording();
        const bool processed=clip&&(clip->stretch||std::any_of(owner_.project()->ara_bindings.begin(),owner_.project()->ara_bindings.end(),[&](const auto& b){return b.source==clip->source;}));
        backend_.setEnabled(!model.busy&&!occupied&&!pending);compatibility_.setEnabled(!model.busy&&!occupied&&!pending);instruments_.setEnabled(python&&!model.busy&&!occupied&&!pending);
        runtime_install_.setEnabled(python&&!model.busy&&!occupied&&!pending&&!model.runtime_present);runtime_verify_.setEnabled(python&&!model.busy&&!occupied&&!pending&&model.runtime_present);runtime_remove_.setEnabled(python&&!model.busy&&!occupied&&!pending&&model.runtime_present);
        for(auto* c:std::initializer_list<juce::Component*>{&detect_,&quantize_,&follow_,&trim_,&bpm_,&meter_,&strength_,&gaps_,&downbeat_,&scale_,&grid_,&import_tempo_})c->setEnabled(python&&!model.busy&&!occupied&&!pending);
        original_.setEnabled(python&&job.state==JobState::ready);
        cpu_.setEnabled(!model.busy&&!occupied&&!pending);stem_.setEnabled(!model.busy&&!occupied&&!pending);
        terms_.setEnabled(!model.busy&&!occupied);token_.setEnabled(!model.busy&&!occupied);
        download_.setEnabled(!model.busy&&!occupied&&!model.present&&terms_.getToggleState()&&!token_.getText().trim().isEmpty());local_.setEnabled(!model.busy&&!occupied&&!model.present&&terms_.getToggleState());
        verify_.setEnabled(!model.busy&&!occupied);remove_.setEnabled(!model.busy&&!occupied&&model.present);
        start_.setEnabled(!model.busy&&!occupied&&!pending&&!recording&&model.model&&(python?model.runtime:model.engine)&&clip.has_value()&&!processed);
        cancel_.setEnabled(model.busy||occupied);discard_.setEnabled(pending&&!occupied);place_.setEnabled(job.state==JobState::ready&&!recording&&owner_.displayState().playback!=mrs::PlaybackState::playing&&parts_.getNumSelectedRows()>0);
        if(!model.busy&&model.message!=last_model_message_){last_model_message_=model.message;message_.setText(model.message,juce::dontSendNotification);}
    }
    Desktop& owner_;std::optional<mrs::Id> source_;std::shared_ptr<TranscriptionSetup> setup_;ModelFetch fixture_fetch_;
    juce::Label title_,source_info_,model_info_,help_,token_label_,parts_label_,status_,message_,footer_,cpu_label_,stem_label_;
    juce::ComboBox cpu_,stem_,backend_,grid_;
    juce::ToggleButton compatibility_,detect_,quantize_,follow_,trim_,original_,import_tempo_;
    juce::TextEditor bpm_,meter_,strength_,gaps_,downbeat_,scale_;
    juce::Label music_label_;
    juce::TextButton runtime_install_,runtime_verify_,runtime_remove_,instruments_;
    std::vector<std::string> instruments_selected_;
    juce::ToggleButton terms_;juce::TextEditor token_;juce::TextButton terms_link_,download_,local_,verify_,remove_,start_,cancel_,discard_,place_;
    juce::ListBox parts_;std::vector<std::string> roles_;std::vector<std::size_t> counts_;std::string signature_;juce::String last_model_message_;
    double progress_value_{};juce::ProgressBar progress_;std::unique_ptr<juce::FileChooser> chooser_;
};
class TranscriptionWindow final:public juce::DocumentWindow {
public:
    TranscriptionWindow(Desktop& owner,std::optional<mrs::Id> source):DocumentWindow("Convert to MIDI",juce::Colour(panel),closeButton),owner_(owner){setUsingNativeTitleBar(true);setLookAndFeel(&owner.theme);setContentOwned(new TranscriptionPanel(owner,std::move(source),owner.transcriptionSetup),true);setResizable(true,false);setResizeLimits(800,930,1200,1200);centreAroundComponent(&owner,800,960);setVisible(true);applyGraphiteWindowCaption(*this);}
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
    Desktop owner(true);owner.view.transcriptionBackend=2;owner.app.new_project(16000);owner.app.import_wavs({path(source)});owner.refresh();
    const auto clip=owner.project()->clips.front().id;const auto before=*owner.project();
    owner.transcriptionSetup=std::make_shared<TranscriptionSetup>(ModelStore(path(root.getChildFile("models")),spec),engine);
    showTranscription(owner,clip);auto* view=dynamic_cast<TranscriptionPanel*>(owner.transcriptionWindow->getContentComponent());need(view!=nullptr,"Transcription panel missing");
    view->cpu_.setSelectedId(8,juce::sendNotificationSync);view->compatibility_.setToggleState(true,juce::sendNotificationSync);view->compatibility_.onClick();
    need(owner.view.transcriptionCpuThreads==7&&owner.view.transcriptionBackend==2&&owner.view.transcriptionCompatibility,"Persist transcription choices before inference");
    const auto preferences=root.getChildFile("transcription-view.json");publishSettings(preferences,owner.view.encode());const auto restored=ViewSettings::decode(preferences.loadFileAsString());
    need(restored.transcriptionCpuThreads==7&&restored.transcriptionBackend==2&&restored.transcriptionCompatibility,"Restore transcription preferences from disk");
    auto malformed=juce::JSON::parse(owner.view.encode());malformed.getDynamicObject()->setProperty("transcriptionCpuThreads",99);malformed.getDynamicObject()->setProperty("transcriptionBackend",99);malformed.getDynamicObject()->setProperty("transcriptionCompatibility","yes");
    const auto defaults=ViewSettings::decode(juce::JSON::toString(malformed));need(defaults.transcriptionCpuThreads==0&&defaults.transcriptionBackend==1&&!defaults.transcriptionCompatibility,"Malformed transcription preferences use safe defaults");
    view->compatibility_.setToggleState(false,juce::dontSendNotification);view->compatibility_.onClick();
    const auto wait=[&](const std::function<bool()>& pending){const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(pending()){need(std::chrono::steady_clock::now()<until,"Transcription GUI timeout");view->tick();juce::Thread::sleep(2);}};
    wait([&]{return owner.transcriptionSetup->status().busy;});view->tick();need(!view->start_.isEnabled()&&!view->download_.isEnabled(),"Uninstalled/unchecked state");
    view->fixture_fetch_=[authorized](const std::string& url,const fs::path& destination,std::uint64_t bytes,std::stop_token stop,const ModelDownloadProgress& progress){need(model_download_target(url).authorize,"Fixture official origin");need(!stop.stop_requested(),"Fixture download cancelled");const auto source_file=path(authorized.getChildFile(text(destination.filename().string())));fs::copy_file(source_file,destination);if(progress)progress(bytes,bytes);};
    view->terms_.setToggleState(true,juce::dontSendNotification);view->token_.setText("fake_test_token",false);view->update();need(view->download_.isEnabled(),"Explicit model download action disabled");view->download_.onClick();need(view->token_.getText().isEmpty(),"Token editor not cleared");
    wait([&]{return owner.transcriptionSetup->status().busy;});view->tick();need(view->start_.isEnabled(),"Verified engine/model should enable conversion");
    view->cpu_.setSelectedId(2,juce::dontSendNotification);int bass=2;for(const auto& role:mrs::ace_midi_instruments()){if(role.role=="electric_bass")break;++bass;}view->stem_.setSelectedId(bass,juce::dontSendNotification);
    view->start_.onClick();wait([&]{return busy(owner.app.transcription_status().state);});view->tick();need(owner.app.transcription_preview()&&!owner.app.transcription_preview()->notes.empty()&&owner.app.transcription_preview()->notes.front().instrument=="electric_bass"&&owner.app.transcription_status().cpu_threads==1,"CPU/stem selection not passed to native");need(owner.app.transcription_status().state==JobState::ready&&view->roles_.size()==1&&view->place_.isEnabled(),"Recognition/parts selection missing");
    view->parts_.deselectAllRows();view->update();need(!view->place_.isEnabled(),"Empty selection must not place");view->parts_.selectRow(0);view->update();
    for(const auto scale:{1.,1.25,1.5,2.}){
        auto image=view->createComponentSnapshot(view->getLocalBounds(),true,static_cast<float>(scale));need(image.isValid(),"Transcription software preview");
        juce::PNGImageFormat format;const auto file=juce::File::getCurrentWorkingDirectory().getChildFile("transcription-"+juce::String(static_cast<int>(scale*100))+".png");auto out=file.createOutputStream();need(out&&format.writeImageToStream(image,*out),"Write transcription preview");
    }
    for(const auto size:{std::pair{800,930},std::pair{850,960},std::pair{1100,1100}}){view->setSize(size.first,size.second);for(int i=0;i<view->getNumChildComponents();++i){auto* c=view->getChildComponent(i);need(view->getLocalBounds().contains(c->getBounds()),"Transcription control outside panel");}}
    view->place_.onClick();need(owner.app.transcription_status().state==JobState::placed&&owner.project()->clips.size()==before.clips.size()+1,"MIDI placement handler failed");const auto after=*owner.project();need(owner.app.undo()&&*owner.project()==before&&owner.app.redo()&&*owner.project()==after,"GUI MIDI Undo/Redo");
    view->start_.onClick();wait([&]{return busy(owner.app.transcription_status().state);});owner.app.add_audio_track("Changed");view->tick();need(owner.app.transcription_status().state==JobState::stale&&!view->place_.isEnabled(),"Stale result should not place");
    wav(source,1);owner.app.new_project(16000);owner.app.import_wavs({path(source)});const auto new_clip=owner.project()->clips.front().id;view->setSource(new_clip);view->tick();view->start_.onClick();view->cancel_.onClick();wait([&]{return busy(owner.app.transcription_status().state);});need(owner.project()->clips.size()==1,"Cancel mutated project");
    owner.transcriptionWindow.reset();showTranscription(owner,new_clip);need(owner.transcriptionWindow!=nullptr,"Transcription reopen failed");closeTranscription(owner);
    need(fs::is_empty(path(jobs)),"Transcription GUI job cleanup");
}
}
