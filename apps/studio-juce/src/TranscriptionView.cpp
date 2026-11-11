#include "TranscriptionView.h"
#include "Desktop.h"
#include "GraphiteWindow.h"
#include "TranscriptionRuntime.h"
#include <mrs/sha256.hpp>
#include <transcription_profile.h>
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
    if(e=="process_exit"||e=="process_start")return "The recognition worker could not finish. Check Diagnostics, the runtime and available memory, or try a shorter clip.";
    if(e=="invalid_output"||e=="invalid_midi")return "The engine returned an invalid result. No MIDI was placed.";
    if(e=="runtime_verification_failed"||e=="muscriptor_runtime_probe_failed")return "Python runtime verification failed. Verify or reinstall the Python runtime.";
    if(e=="muscriptor_runtime_install_failed")return "Python runtime installation failed. Check connection, free disk space and runtime-last-operation.log.";
    if(e=="runtime_requirements_changed")return "Runtime setup files do not match this MR Studio build. Reinstall the application.";
    if(e=="invalid_music_options")return "Check BPM, meter, strength (0..1), fill gaps and beat scale values.";
    if(e=="missing_eos")return "The model reached the token limit before EOS. No partial MIDI was placed. Try a shorter clip or Auto.";
    if(e=="timeout")return "Conversion timed out. Try a shorter source WAV.";
    return text(e);
}
}

// Desktop-owned, persistent across utility-window close/reopen. Background work
// captures values/shared progress, never a Component or Application pointer.
class TranscriptionSetup {
public:
    struct Status {bool failed{},busy{},model{},present{},runtime{},runtime_present{};double progress{};juce::String message;};
    TranscriptionSetup(ModelStore models,Request engine,std::shared_ptr<TranscriptionRuntime> runtime={}):models_(std::move(models)),engine_(std::move(engine)),runtime_(std::move(runtime)){refresh();}
    ~TranscriptionSetup(){shutdown();}
    ModelStore models()const{return models_;}
    Request pythonEngine()const{
        need(runtime_!=nullptr,"Python runtime unavailable");auto r=engine_;r.backend=Backend::python_medium;
        r.executable=path(runtime_->python());r.executable_sha256=mrs::sha256_file(r.executable);r.memory_bytes=8ULL*1024*1024*1024;
        r.runtime_lifetime=runtime_->acquire();return r;
    }
    void runtimeOperation(int operation){
        need(runtime_!=nullptr,"Python runtime unavailable");auto runtime=runtime_;
        begin(operation==0?"Installing isolated Python Medium runtime (this can take several minutes)...":operation==1?"Verifying Python Medium runtime...":"Removing Python Medium runtime...",[runtime,operation](ModelStore,std::stop_token stop,const ModelDownloadProgress&){if(operation==0)runtime->install(stop);else if(operation==1)runtime->verify(stop);else runtime->remove();});
    }
    Status status()const{auto s=status_;if(job_){const auto total=job_->total.load();s.progress=total?static_cast<double>(job_->completed.load())/static_cast<double>(total):-1.;}return s;}
    void poll(){
        if(!job_||job_->future.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        auto job=std::move(job_);try{status_=job->future.get();}catch(...){status_={};status_.failed=true;status_.message="Model operation failed.";}
    }
    void cancel(){if(job_){job_->stop.request_stop();status_.message="Cancelling model operation...";}}
    void shutdown(){if(job_){job_->stop.request_stop();job_->future.wait();job_.reset();}}
    void refresh(){begin("Reading installed files...",[](ModelStore,std::stop_token,const ModelDownloadProgress&){});}
    void check(){begin("Verifying model files...",[](ModelStore store,std::stop_token stop,const ModelDownloadProgress&){store.verify(stop);});}
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
            catch(const std::exception& e){out.failed=true;out.message=stop.stop_requested()?"Model operation cancelled.":failure(e.what());}
            if(stop.stop_requested()){out.message="Model operation cancelled.";return out;}
            try{out.present=fs::exists(models.directory());if(out.present){const auto lease=models.acquire();(void)lease;out.model=true;}}
            catch(const std::exception& e){out.failed=true;out.message=failure(e.what());}
            if(out.message.isEmpty())out.message=out.model?"Model installed and available offline.":"Download Medium or choose an authorized local folder.";
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
    Request settings;settings.cpu_threads=0;
    settings.worker_script=path(app.getChildFile("mr_transcription_worker.py"));settings.worker_sha256=mrs::transcription_worker_sha256;
    settings.temporary_root=path(root.getChildFile("jobs"));
    // Worker requires a pre-existing trusted temp root; no model download here.
    need(root.getChildFile("jobs").createDirectory().wasOk(),"Cannot create transcription job folder");
    return std::make_shared<TranscriptionSetup>(ModelStore(path(root.getChildFile("models"))),std::move(settings),std::make_shared<TranscriptionRuntime>(root,app.getChildFile("mr_transcription_requirements.txt")));
}
}
namespace {
juce::String instrumentName(std::string_view role){auto name=text(std::string(role)).replaceCharacter('_',' ');return name.substring(0,1).toUpperCase()+name.substring(1);}
juce::String elapsed(double seconds){const auto s=static_cast<int>(seconds);return (s>=3600?juce::String(s/3600)+":":juce::String{})+juce::String((s/60)%60).paddedLeft('0',2)+":"+juce::String(s%60).paddedLeft('0',2);}
void label(juce::Label& l,juce::Component& parent,const juce::String& value){l.setText(value,juce::dontSendNotification);l.setMinimumHorizontalScale(1.f);l.setJustificationType(juce::Justification::topLeft);parent.addAndMakeVisible(l);}
// One worker-driven progress view for stem and full mix. Chunk completion is
// deliberately not advertised as an end-to-end percentage.
class TranscriptionProgress final:public juce::Component {
public:
    explicit TranscriptionProgress(Desktop& owner):owner_(owner),bar_(value_){
        label(model_,*this,"MuScriptor Medium");model_.setFont(owner.theme.font(20));label(context_,*this,{});label(chunks_,*this,{});label(operation_,*this,{});label(stages_,*this,{});
        addAndMakeVisible(bar_);bar_.setTitle("Прогресс обработки");bar_.setPercentageDisplay(false);
    }
    void update(const JobStatus& job,bool stem,const juce::String& source){
        value_=busy(job.state)?-1.:job.state==JobState::ready||job.state==JobState::placed?1.:0.;
        context_.setText((stem?juce::String("Stem → MIDI"):juce::String("Full Mix → MIDI"))+" · "+source,juce::dontSendNotification);
        chunks_.setText("CHUNKS   "+(job.total?juce::String(job.completed)+" / "+juce::String(job.total):juce::String("—")),juce::dontSendNotification);
        juce::String operation;
        switch(job.phase){
        case Phase::checking_files:case Phase::checking_runtime:operation="Проверка файлов";break;
        case Phase::preparing_audio:case Phase::reading_audio:operation="Подготовка аудио";break;
        case Phase::starting_worker:case Phase::importing_runtime:case Phase::loading_model:operation="Загрузка модели";break;
        case Phase::recognizing:operation="Декодирование нот";if(job.total)operation+=" · Chunk "+juce::String((std::min)(job.completed+1,job.total));break;
        case Phase::processing_midi:case Phase::exporting_midi:case Phase::validating_result:operation="Формирование MIDI";break;
        case Phase::complete:operation="Распознавание завершено";break;
        }
        if(job.state==JobState::cancelling)operation="Отмена обработки…";
        operation_.setText(elapsed(job.elapsed_seconds)+"   ·   "+operation,juce::dontSendNotification);
        const auto mark=[&](Phase phase){return job.phase>phase?juce::String("✓  "):job.phase==phase?juce::String("●  "):juce::String("○  ");};
        stages_.setText(mark(Phase::loading_model)+"Загрузка модели        "+mark(Phase::recognizing)+"Декодирование нот\n"+mark(Phase::preparing_audio)+"Подготовка аудио       "+mark(Phase::processing_midi)+"Формирование MIDI",juce::dontSendNotification);
    }
    void resized()override{const int w=getWidth();model_.setBounds(0,0,w,32);context_.setBounds(0,38,w,24);bar_.setBounds(0,92,w,12);chunks_.setBounds(12,136,140,52);operation_.setBounds(160,136,w-172,52);stages_.setBounds(0,220,w,52);}
    void paint(juce::Graphics& g)override{g.setColour(owner_.theme.role(ColourRole::panel,surface));g.fillRoundedRectangle(0,128,static_cast<float>(getWidth()),64,6);}
private:Desktop& owner_;double value_{-1.};juce::ProgressBar bar_;juce::Label model_,context_,chunks_,operation_,stages_;
};

class ModelManagerPanel final:public juce::Component,private juce::Timer {
public:
    ModelManagerPanel(Desktop& owner):owner_(owner),setup_(owner.transcriptionSetup),bar_(progress_){
        label(title_,*this,"MuScriptor Medium");title_.setFont(owner.theme.font(20));
        label(info_,*this,"CPU · Medium 307M · CC BY-NC 4.0\nВеса устанавливаются отдельно, с официального источника.");
        label(status_,*this,{});terms_.setButtonText("У меня есть доступ; принимаю условия модели");addAndMakeVisible(terms_);
        token_.setPasswordCharacter('*');token_.setTitle("HF read token — не сохраняется");token_.setTextToShowWhenEmpty("HF read token",owner.theme.role(ColourRole::muted,secondaryText));addAndMakeVisible(token_);
        int id=0;for(auto* b:{&terms_link_,&download_,&local_,&verify_,&remove_,&runtime_install_,&runtime_verify_,&runtime_remove_}){addAndMakeVisible(b);b->setExplicitFocusOrder(++id);}
        terms_link_.setButtonText("Условия модели ↗");download_.setButtonText("Установить Medium");local_.setButtonText("Из папки…");verify_.setButtonText("Проверить");remove_.setButtonText("Удалить модель");
        runtime_install_.setButtonText("Установить Python runtime");runtime_verify_.setButtonText("Проверить runtime");runtime_remove_.setButtonText("Удалить runtime");
        terms_link_.onClick=[]{juce::URL("https://huggingface.co/MuScriptor/muscriptor-medium").launchInDefaultBrowser();};
        download_.onClick=[this]{invoke([&]{need(terms_.getToggleState(),"Accept the official model terms first");auto token=token_.getText().trim().toStdString();token_.clear();setup_->download(std::move(token),fixture_fetch_);});};
        local_.onClick=[this]{need(terms_.getToggleState(),"Accept the official model terms first");chooser_=std::make_unique<juce::FileChooser>("Choose authorized Medium folder");juce::Component::SafePointer<ModelManagerPanel> safe(this);chooser_->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe](const juce::FileChooser& f){if(safe&&f.getResult().isDirectory())safe->invoke([&]{safe->setup_->local(path(f.getResult()));});});};
        verify_.onClick=[this]{invoke([&]{setup_->check();});};remove_.onClick=[this]{invoke([&]{setup_->remove();});};
        runtime_install_.onClick=[this]{invoke([&]{setup_->runtimeOperation(0);});};runtime_verify_.onClick=[this]{invoke([&]{setup_->runtimeOperation(1);});};runtime_remove_.onClick=[this]{invoke([&]{setup_->runtimeOperation(2);});};
        addAndMakeVisible(bar_);setSize(640,420);startTimerHz(8);timerCallback();
    }
    ~ModelManagerPanel()override{stopTimer();setup_->cancel();setLookAndFeel(nullptr);}
    void paint(juce::Graphics& g)override{g.fillAll(owner_.theme.role(ColourRole::window,panel));}
    void resized()override{const int w=getWidth();title_.setBounds(24,16,w-48,32);info_.setBounds(24,60,w-48,52);terms_.setBounds(24,124,w-48,32);token_.setBounds(24,164,w-220,32);terms_link_.setBounds(w-184,164,160,32);download_.setBounds(24,208,176,32);local_.setBounds(208,208,112,32);verify_.setBounds(328,208,112,32);remove_.setBounds(448,208,w-472,32);runtime_install_.setBounds(24,256,224,32);runtime_verify_.setBounds(256,256,168,32);runtime_remove_.setBounds(432,256,w-456,32);bar_.setBounds(24,312,w-48,12);status_.setBounds(24,340,w-48,64);}
private:
    friend void ::ui::transcriptionSmoke(const juce::File&);
    template<class F>void invoke(F f){try{f();error_.clear();}catch(const std::exception& e){error_=failure(e.what());}timerCallback();}
    void timerCallback()override{setup_->poll();const auto s=setup_->status();const auto job=owner_.app.transcription_status();const bool available=!s.busy&&!busy(job.state)&&!preview(job.state);progress_=s.busy?s.progress:0.;status_.setText(error_.isNotEmpty()?error_:s.message+(s.runtime?"\nPython runtime установлен":"\nPython runtime не установлен"),juce::dontSendNotification);terms_.setEnabled(available);token_.setEnabled(available);download_.setEnabled(available&&!s.present&&terms_.getToggleState()&&token_.getText().trim().isNotEmpty());local_.setEnabled(available&&!s.present&&terms_.getToggleState());verify_.setEnabled(available&&s.present);remove_.setEnabled(available&&s.present);runtime_install_.setEnabled(available&&!s.runtime_present);runtime_verify_.setEnabled(available&&s.runtime_present);runtime_remove_.setEnabled(available&&s.runtime_present);}
    Desktop& owner_;std::shared_ptr<TranscriptionSetup> setup_;juce::Label title_,info_,status_;juce::ToggleButton terms_;juce::TextEditor token_;juce::TextButton terms_link_,download_,local_,verify_,remove_,runtime_install_,runtime_verify_,runtime_remove_;double progress_{};juce::ProgressBar bar_;juce::String error_;std::unique_ptr<juce::FileChooser> chooser_;ModelFetch fixture_fetch_;
};

class TranscriptionPreferences final:public juce::Component {
public:
    explicit TranscriptionPreferences(Desktop& owner):owner_(owner){
        label(nav_,*this,"Audio\n\nMIDI\n\nAudio-to-MIDI\n\nPlugins\n\nModels & Data");
        label(title_,*this,"Audio-to-MIDI");title_.setFont(owner.theme.font(22));
        label(performance_,*this,"ПРОИЗВОДИТЕЛЬНОСТЬ");label(midi_,*this,"ПОВЕДЕНИЕ MIDI");
        label(cpu_label_,*this,"CPU Threads");label(device_label_,*this,"Устройство вычислений");label(quantize_label_,*this,"Квантизация после импорта");label(grid_label_,*this,"Сетка / сила квантизации");label(velocity_label_,*this,"Velocity по умолчанию");label(hint_,*this,"Исходное время нот сохраняется при Quantization Off.\nVelocity — fallback, модель не распознаёт динамику.");
        cpu_.addItem("Auto",1);for(unsigned n=1;n<=(std::min)(32U,available_cpu_threads());++n)cpu_.addItem(juce::String(n),static_cast<int>(n+1));cpu_.setSelectedId((std::min)(owner.view.transcriptionCpuThreads,static_cast<int>((std::min)(32U,available_cpu_threads())))+1);
        device_.addItem("CPU",1);device_.setSelectedId(1);device_.setTooltip("Установленный runtime поддерживает CPU");
        quantize_.addItem("Off",1);quantize_.addItem("On",2);quantize_.setSelectedId(owner.view.transcriptionQuantize?2:1);
        const juce::StringArray names{"1/4","1/8","1/16","1/32","1/8 T","1/16 T"};for(int i=0;i<names.size();++i)grid_.addItem(names[i],i+1);grid_.setSelectedId(owner.view.transcriptionGrid);
        strength_.setRange(0,100,1);strength_.setValue(owner.view.transcriptionStrength);strength_.setTextValueSuffix("%");strength_.setSliderStyle(juce::Slider::IncDecButtons);strength_.setTextBoxStyle(juce::Slider::TextBoxLeft,false,64,28);
        velocity_.setRange(1,127,1);velocity_.setValue(owner.view.transcriptionVelocity);velocity_.setSliderStyle(juce::Slider::IncDecButtons);velocity_.setTextBoxStyle(juce::Slider::TextBoxLeft,false,64,28);
        for(auto* c:{&cpu_,&device_,&quantize_,&grid_})addAndMakeVisible(c);addAndMakeVisible(strength_);addAndMakeVisible(velocity_);
        cpu_.setTitle("CPU Threads");device_.setTitle("Compute Device");quantize_.setTitle("Quantization");grid_.setTitle("Quantization Grid");strength_.setTitle("Quantization Strength");velocity_.setTitle("Default Velocity");
        compatibility_.setButtonText("Compatibility CPU (disable AVX2)");compatibility_.setToggleState(owner.view.transcriptionCompatibility,juce::dontSendNotification);addAndMakeVisible(compatibility_);
        const auto save=[this]{owner_.view.transcriptionCpuThreads=cpu_.getSelectedId()-1;owner_.view.transcriptionCompatibility=compatibility_.getToggleState();owner_.view.transcriptionQuantize=quantize_.getSelectedId()==2;owner_.view.transcriptionGrid=grid_.getSelectedId();owner_.view.transcriptionStrength=static_cast<int>(strength_.getValue());owner_.view.transcriptionVelocity=static_cast<int>(velocity_.getValue());grid_.setEnabled(owner_.view.transcriptionQuantize);strength_.setEnabled(owner_.view.transcriptionQuantize);owner_.saveSettings();};
        cpu_.onChange=save;quantize_.onChange=save;grid_.onChange=save;strength_.onValueChange=save;velocity_.onValueChange=save;compatibility_.onClick=save;grid_.setEnabled(owner.view.transcriptionQuantize);strength_.setEnabled(owner.view.transcriptionQuantize);
        models_.setButtonText("Управление моделями   →");models_.onClick=[this]{showTranscriptionModels(owner_);};addAndMakeVisible(models_);setSize(800,580);
    }
    void resized()override{const int x=208,w=getWidth()-x-28,r=getWidth()-220;nav_.setBounds(24,72,164,400);title_.setBounds(x,24,w,36);performance_.setBounds(x,84,w,24);cpu_label_.setBounds(x,124,w-200,32);cpu_.setBounds(r,124,192,32);device_label_.setBounds(x,172,w-200,32);device_.setBounds(r,172,192,32);compatibility_.setBounds(x,216,w,32);midi_.setBounds(x,272,w,24);quantize_label_.setBounds(x,312,w-200,32);quantize_.setBounds(r,312,192,32);grid_label_.setBounds(x,360,w-200,32);grid_.setBounds(r,360,88,32);strength_.setBounds(r+96,360,96,32);velocity_label_.setBounds(x,408,w-200,32);velocity_.setBounds(r,408,192,32);hint_.setBounds(x,452,w,48);models_.setBounds(x,520,w,36);}
    void paint(juce::Graphics& g)override{g.fillAll(owner_.theme.role(ColourRole::window,panel));g.setColour(owner_.theme.role(ColourRole::panel,surface));g.fillRoundedRectangle(12,12,180,static_cast<float>(getHeight()-24),6);}
private:Desktop& owner_;juce::Label nav_,title_,performance_,midi_,cpu_label_,device_label_,quantize_label_,grid_label_,velocity_label_,hint_;juce::ComboBox cpu_,device_,quantize_,grid_;juce::Slider strength_,velocity_;juce::ToggleButton compatibility_;juce::TextButton models_;
};

class TranscriptionPartsList final:public juce::ListBox {
public:
    std::function<void(int)> toggle;
    bool keyPressed(const juce::KeyPress& key)override{if(key.getKeyCode()==juce::KeyPress::spaceKey){if(toggle)toggle(getSelectedRow());return true;}return juce::ListBox::keyPressed(key);}
};
class TranscriptionPanel final:public juce::Component,private juce::Timer,private juce::ListBoxModel {
public:
    TranscriptionPanel(Desktop& owner,std::optional<mrs::Id> source,mrs::TranscriptionPlacement placement)
        :owner_(owner),source_(std::move(source)),placement_(std::move(placement)),progress_(owner){
        capture_=owner_.app.services().projects->state();store_=owner_.app.services().projects;
        label(title_,*this,"Convert to MIDI");title_.setFont(owner.theme.font(22));label(source_label_,*this,"Источник");label(source_info_,*this,{});label(model_label_,*this,"Модель распознавания");label(stem_label_,*this,"Инструмент партии · один вариант");label(hint_,*this,placement_.target_track?"Инструмент ограничивает MIDI-декодирование":"Распознанные партии появятся после обработки");label(message_,*this,{});label(selection_count_,*this,{});
        model_.addItem("MuScriptor Medium",1);model_.setSelectedId(1);model_.setTitle("Модель распознавания");addAndMakeVisible(model_);
        int id=1;for(const auto& role:mrs::ace_midi_instruments())stem_.addItem(instrumentName(role.role),id++);stem_.setSelectedId(1);stem_.setTitle("Инструмент партии");addAndMakeVisible(stem_);
        // Only exact canonical separation metadata is a safe preselection.
        if(source_)for(const auto& c:owner.project()->clips)if(c.id==*source_)
            for(const auto& record:owner.project()->separation_results)for(const auto& file:record.stems)if(file.source==c.source){int n=1;for(const auto& role:mrs::ace_midi_instruments()){if(role.role==file.role)stem_.setSelectedId(n);++n;}}
        for(auto* b:{&start_,&cancel_,&all_,&none_,&place_,&preferences_,&diagnostics_})addAndMakeVisible(b);
        start_.setButtonText(placement_.target_track?"Конвертировать":"Распознать партии");cancel_.setButtonText("Отмена");all_.setButtonText("Выбрать все");none_.setButtonText("Снять все");preferences_.setButtonText("Preferences → Audio-to-MIDI");diagnostics_.setButtonText("Diagnostics");
        start_.onClick=[this]{invoke([&]{need(source_.has_value(),"Select an audio clip");need(store_==owner_.app.services().projects&&capture_.revision==store_->state().revision,"Project changed; reopen Convert to MIDI");auto request=owner_.transcriptionSetup->pythonEngine();request.cpu_threads=static_cast<unsigned>(owner_.view.transcriptionCpuThreads);request.compatibility_cpu=owner_.view.transcriptionCompatibility;request.placement=placement_;request.placement.velocity=owner_.view.transcriptionVelocity;static constexpr mrs::Tick grids[]{mrs::ppq,mrs::ppq/2,mrs::ppq/4,mrs::ppq/8,mrs::ppq/3,mrs::ppq/6};request.placement.quantize_grid=owner_.view.transcriptionQuantize?grids[owner_.view.transcriptionGrid-1]:0;request.placement.quantize_strength=owner_.view.transcriptionStrength/100.;
            if(placement_.target_track){need(stem_.getSelectedId()>0,"Choose one instrument");request.instruments={std::string(mrs::ace_midi_instruments()[static_cast<std::size_t>(stem_.getSelectedId()-1)].role)};}
            owner_.app.queue_transcription(*source_,owner_.transcriptionSetup->models(),std::move(request));started_=true;});};
        cancel_.onClick=[this]{if(!busy(owner_.app.transcription_status().state)){owner_.transcriptionWindow.reset();return;}owner_.app.cancel_transcription();started_=false;update();};
        all_.onClick=[this]{selected_.assign(roles_.size(),true);update();parts_.repaint();};none_.onClick=[this]{selected_.assign(roles_.size(),false);update();parts_.repaint();};
        place_.onClick=[this]{invoke([&]{std::vector<std::string> roles;for(std::size_t i=0;i<roles_.size();++i)if(selected_[i])roles.push_back(roles_[i]);owner_.app.place_transcription(roles);capture_=owner_.app.services().projects->state();owner_.refresh();});};
        preferences_.onClick=[this]{showTranscriptionPreferences(owner_);};
        diagnostics_.onClick=[this]{auto editor=std::make_unique<juce::TextEditor>();editor->setMultiLine(true);editor->setReadOnly(true);editor->setSize(700,420);editor->setText(text(owner_.app.transcription_status().diagnostics));juce::DialogWindow::LaunchOptions o;o.content.setOwned(editor.release());o.dialogTitle="Audio-to-MIDI diagnostics";o.dialogBackgroundColour=owner_.theme.role(ColourRole::window,panel);o.componentToCentreAround=this;o.useNativeTitleBar=true;o.resizable=true;o.launchAsync();};
        parts_.toggle=[this](int row){toggle(row);};parts_.setModel(this);parts_.setRowHeight(48);parts_.setTitle("Извлечённые MIDI-партии — пробел переключает выбор");addAndMakeVisible(parts_);addAndMakeVisible(progress_);setSize(640,520);startTimerHz(8);update();
    }
    ~TranscriptionPanel()override{stopTimer();owner_.app.cancel_transcription();parts_.setModel(nullptr);}
    void resized()override{
        const int w=getWidth(),h=getHeight();title_.setBounds(24,16,w-48,40);source_label_.setBounds(24,76,w-48,24);source_info_.setBounds(24,104,w-48,40);model_label_.setBounds(24,160,w-48,24);model_.setBounds(24,192,w-48,36);stem_label_.setBounds(24,244,w-48,24);stem_.setBounds(24,276,w-48,36);hint_.setBounds(24,placement_.target_track?324:244,w-48,32);
        progress_.setBounds(24,84,w-48,280);selection_count_.setBounds(24,72,w-48,24);parts_.setBounds(24,104,w-48,h-280);all_.setBounds(24,h-160,144,32);none_.setBounds(176,h-160,144,32);place_.setBounds(w-280,h-76,256,36);start_.setBounds(w-248,h-76,224,36);cancel_.setBounds(w-384,h-76,128,36);preferences_.setBounds(24,h-36,w-48,28);message_.setBounds(24,h-124,w-48,36);diagnostics_.setBounds(24,h-76,128,36);
    }
    void paint(juce::Graphics& g)override{g.fillAll(owner_.theme.role(ColourRole::window,panel));g.setColour(owner_.theme.role(ColourRole::border,outline));g.drawHorizontalLine(64,24.f,static_cast<float>(getWidth()-24));}
private:
    friend void ::ui::transcriptionSmoke(const juce::File&);
    template<class F>void invoke(F f){try{message_.setText({},juce::dontSendNotification);f();}catch(const std::exception& e){message_.setText(failure(e.what()),juce::dontSendNotification);}update();}
    void timerCallback()override{owner_.transcriptionSetup->poll();owner_.app.poll();update();}
    void update(){
        const auto job=owner_.app.transcription_status();const auto model=owner_.transcriptionSetup->status();const bool running=busy(job.state),results=started_&&job.state==JobState::ready&&!placement_.target_track;
        const auto* result=owner_.app.transcription_preview();
        if(started_&&job.state==JobState::ready&&placement_.target_track&&!placing_){placing_=true;try{owner_.app.place_transcription();capture_=owner_.app.services().projects->state();owner_.refresh();message_.setText("MIDI-клип создан на выбранной дорожке",juce::dontSendNotification);}catch(const std::exception& e){message_.setText(failure(e.what()),juce::dontSendNotification);owner_.app.cancel_transcription();}placing_=false;update();return;}
        if(results&&result&&signature_!=result->midi_sha256+result->clip_id.value){signature_=result->midi_sha256+result->clip_id.value;roles_.clear();for(const auto& note:result->notes)if(std::find(roles_.begin(),roles_.end(),note.instrument)==roles_.end())roles_.push_back(note.instrument);selected_.assign(roles_.size(),true);parts_.updateContent();}
        title_.setText(running?"Audio-to-MIDI · Обработка":results?"Извлечённые MIDI-партии":"Convert to MIDI",juce::dontSendNotification);
        bool hasSource=false;for(const auto& c:owner_.project()->clips)if(source_&&c.id==*source_&&!c.midi){hasSource=true;const auto rate=owner_.project()->sample_rate;source_info_.setText(text(c.name)+" · "+elapsed(static_cast<double>(placement_.range_offset)/rate)+"–"+elapsed(static_cast<double>(placement_.range_offset+placement_.range_length.value_or(c.length-placement_.range_offset))/rate),juce::dontSendNotification);}
        for(auto* c:std::initializer_list<juce::Component*>{&source_label_,&source_info_,&model_label_,&model_,&hint_})c->setVisible(!running&&!results);
        stem_label_.setVisible(!running&&!results&&placement_.target_track.has_value());stem_.setVisible(stem_label_.isVisible());
        progress_.setVisible(running);progress_.update(job,placement_.target_track.has_value(),source_info_.getText());selection_count_.setVisible(results);parts_.setVisible(results);all_.setVisible(results);none_.setVisible(results);place_.setVisible(results);start_.setVisible(!running&&!results);cancel_.setVisible(true);preferences_.setVisible(!running&&!results&&(!model.model||!model.runtime));diagnostics_.setVisible(!job.diagnostics.empty()&&(job.state==JobState::failed||job.state==JobState::timeout));
        const auto count=static_cast<int>(std::count(selected_.begin(),selected_.end(),true));selection_count_.setText("Отметьте партии для создания дорожек   ·   "+juce::String(count)+" из "+juce::String(static_cast<int>(roles_.size())),juce::dontSendNotification);place_.setButtonText("Создать MIDI-дорожки: "+juce::String(count));place_.setEnabled(count>0&&!owner_.app.recording()&&owner_.displayState().playback!=mrs::PlaybackState::playing);
        if(!hasSource)source_info_.setText("Выберите аудиоклип в Arrange",juce::dontSendNotification);
        start_.setEnabled(hasSource&&!model.busy&&model.model&&model.runtime&&!preview(job.state)&&!running&&!owner_.app.recording());
        if(model.model&&model.runtime&&message_.getText()=="Требуется установка → Preferences → Управление моделями")message_.setText({},juce::dontSendNotification);
        if(!running&&!results&&message_.getText().isEmpty()&&(!model.model||!model.runtime))message_.setText("Требуется установка → Preferences → Управление моделями",juce::dontSendNotification);
        if(job.state==JobState::empty)message_.setText("Нот не обнаружено",juce::dontSendNotification);
        if(job.state==JobState::failed||job.state==JobState::timeout)message_.setText(failure(job.error),juce::dontSendNotification);
        if(job.state==JobState::stale)message_.setText("Проект или источник изменён. Запустите конвертацию снова.",juce::dontSendNotification);
        if(started_&&job.state==JobState::placed)message_.setText("MIDI создан · Undo / Redo доступны",juce::dontSendNotification);
        const int height=running?480:results?560:placement_.target_track?520:400;
        if(getHeight()!=height){if(auto* window=findParentComponentOfClass<juce::DocumentWindow>()){window->setContentComponentSize(640,height);if(auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(window->getBounds()))window->setBounds(window->getBounds().constrainedWithin(display->userArea));}else setSize(640,height);}

    }
    int getNumRows()override{return static_cast<int>(roles_.size());}
    void paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool)override{if(row<0||row>=getNumRows())return;const bool checked=selected_[static_cast<std::size_t>(row)];g.setColour(owner_.theme.role(ColourRole::panel,surface));g.fillRoundedRectangle(0,2,static_cast<float>(w),static_cast<float>(h-4),4);g.setColour(owner_.theme.role(checked?ColourRole::focus:ColourRole::muted,checked?accent:secondaryText));g.drawRoundedRectangle(14,14,20,20,3,1.5f);if(checked){juce::Path p;p.startNewSubPath(18,24);p.lineTo(23,29);p.lineTo(31,19);g.strokePath(p,juce::PathStrokeType(2));}g.setColour(owner_.theme.role(ColourRole::text,primaryText));g.setFont(owner_.theme.font(16));g.drawText(instrumentName(roles_[static_cast<std::size_t>(row)]),48,0,w-64,h,juce::Justification::centredLeft);}
    void listBoxItemClicked(int row,const juce::MouseEvent&)override{toggle(row);}
    void listBoxItemDoubleClicked(int,const juce::MouseEvent&)override{}
    void returnKeyPressed(int row)override{toggle(row);}
    void toggle(int row){if(row>=0&&row<getNumRows()){selected_[static_cast<std::size_t>(row)]=!selected_[static_cast<std::size_t>(row)];parts_.repaint();update();}}
    Desktop& owner_;std::optional<mrs::Id> source_;mrs::TranscriptionPlacement placement_;mrs::ProjectState capture_;std::shared_ptr<mrs::IProjectStore> store_;bool started_{},placing_{};
    juce::Label title_,source_label_,source_info_,model_label_,stem_label_,hint_,message_,selection_count_;juce::ComboBox model_,stem_;juce::TextButton start_,cancel_,all_,none_,place_,preferences_,diagnostics_;TranscriptionProgress progress_;TranscriptionPartsList parts_;std::vector<std::string> roles_;std::vector<bool> selected_;std::string signature_;
};
class TranscriptionWindow final:public juce::DocumentWindow {
public:
    TranscriptionWindow(Desktop& owner,juce::Component* content,const juce::String& title,std::unique_ptr<juce::DocumentWindow>& slot,bool atCursor=false):DocumentWindow(title,owner.theme.role(ColourRole::window,panel),closeButton),slot_(slot){setUsingNativeTitleBar(true);setLookAndFeel(&owner.theme);setContentOwned(content,true);setResizable(false,false);centreAroundComponent(&owner,getWidth(),getHeight());if(atCursor){const auto p=juce::Desktop::getMousePosition();if(auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForPoint(p))setBounds(getBounds().withPosition(p).constrainedWithin(display->userArea));}setVisible(true);applyGraphiteWindowCaption(*this);}
    ~TranscriptionWindow()override{clearContentComponent();setLookAndFeel(nullptr);}
    void closeButtonPressed()override{slot_.reset();}
private:std::unique_ptr<juce::DocumentWindow>& slot_;
};
}
void showTranscription(Desktop& owner,std::optional<mrs::Id> source,mrs::TranscriptionPlacement placement){
    if(owner.standaloneLive)return;
    if(owner.transcriptionWindow){const auto state=owner.app.transcription_status().state;if(busy(state)||preview(state)){owner.transcriptionWindow->toFront(true);return;}owner.transcriptionWindow.reset();}
    if(!source)for(const auto& c:owner.project()->clips)if(!c.midi&&owner.selectedClips.contains(c.id.value)){source=c.id;break;}
    if(!owner.transcriptionSetup)owner.transcriptionSetup=setup();
    owner.transcriptionWindow=std::make_unique<TranscriptionWindow>(owner,new TranscriptionPanel(owner,std::move(source),placement),"Audio-to-MIDI",owner.transcriptionWindow,placement.target_track.has_value());
}
void showTranscriptionPreferences(Desktop& owner){if(owner.standaloneLive)return;if(!owner.transcriptionSetup)owner.transcriptionSetup=setup();if(owner.transcriptionPreferencesWindow){owner.transcriptionPreferencesWindow->toFront(true);return;}owner.transcriptionPreferencesWindow=std::make_unique<TranscriptionWindow>(owner,new TranscriptionPreferences(owner),"Preferences → Audio-to-MIDI",owner.transcriptionPreferencesWindow);}
void showTranscriptionModels(Desktop& owner){if(owner.standaloneLive)return;if(!owner.transcriptionSetup)owner.transcriptionSetup=setup();if(owner.transcriptionModelsWindow){owner.transcriptionModelsWindow->toFront(true);return;}owner.transcriptionModelsWindow=std::make_unique<TranscriptionWindow>(owner,new ModelManagerPanel(owner),"Audio-to-MIDI · Управление моделями",owner.transcriptionModelsWindow);}
void closeTranscription(Desktop& owner){owner.transcriptionWindow.reset();owner.transcriptionPreferencesWindow.reset();owner.transcriptionModelsWindow.reset();if(owner.transcriptionSetup)owner.transcriptionSetup->shutdown();owner.transcriptionSetup.reset();}

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
    const auto fixture_script=root.getChildFile("worker-fixture.py");need(fixture_script.replaceWithText("# synthetic process fixture"),"Create worker fixture");engine.worker_script=path(fixture_script);engine.worker_sha256=mrs::sha256_file(engine.worker_script);
    // A metadata-only runtime fixture cannot pass an inventory/probe. Opening
    // the converter and preparing a request must nevertheless avoid either.
    auto runtime=std::make_shared<TranscriptionRuntime>(root.getChildFile("runtime-fixture"),root.getChildFile("unused-requirements.txt"));
    need(runtime->directory().createDirectory().wasOk()&&fake.copyFileTo(runtime->python())&&runtime->directory().getChildFile("managed.json").replaceWithText("fixture"),"Create runtime metadata fixture");
    auto quick=std::make_shared<TranscriptionSetup>(ModelStore(path(root.getChildFile("startup-models")),spec),engine,runtime);
    const auto quick_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(quick->status().busy){need(std::chrono::steady_clock::now()<quick_deadline,"Metadata refresh timeout");quick->poll();juce::Thread::sleep(2);}
    need(!quick->status().failed&&quick->status().runtime&&!quick->pythonEngine().verify_runtime,"Opening converter/request must not verify runtime automatically");quick.reset();
    Desktop owner(true);owner.view.transcriptionBackend=2;owner.app.new_project(16000);owner.app.import_wavs({path(source)});owner.refresh();
    const auto clip=owner.project()->clips.front().id;const auto before=*owner.project();
    owner.transcriptionSetup=std::make_shared<TranscriptionSetup>(ModelStore(path(root.getChildFile("models")),spec),engine,runtime);
    ModelManagerPanel manager(owner);manager.setLookAndFeel(&owner.theme);
    const auto wait=[&](const std::function<bool()>& pending){const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(pending()){need(std::chrono::steady_clock::now()<until,"Transcription GUI timeout");manager.timerCallback();owner.app.poll();juce::Thread::sleep(2);}};
    wait([&]{return owner.transcriptionSetup->status().busy;});manager.timerCallback();need(!manager.download_.isEnabled(),"Uninstalled/unchecked state");
    manager.fixture_fetch_=[authorized](const std::string& url,const fs::path& destination,std::uint64_t bytes,std::stop_token stop,const ModelDownloadProgress& progress){need(model_download_target(url).authorize,"Fixture official origin");need(!stop.stop_requested(),"Fixture download cancelled");fs::copy_file(path(authorized.getChildFile(text(destination.filename().string()))),destination);if(progress)progress(bytes,bytes);};
    manager.terms_.setToggleState(true,juce::dontSendNotification);manager.token_.setText("fake_test_token",false);manager.timerCallback();need(manager.download_.isEnabled(),"Explicit download action disabled");manager.download_.onClick();need(manager.token_.getText().isEmpty(),"Token editor not cleared");wait([&]{return owner.transcriptionSetup->status().busy;});
    const auto installed=juce::File(juce::String(owner.transcriptionSetup->models().directory().wstring().c_str()));need(installed.getChildFile("model.safetensors").replaceWithText("invalid"),"Corrupt model fixture");owner.transcriptionSetup->refresh();wait([&]{return owner.transcriptionSetup->status().busy;});need(owner.transcriptionSetup->status().model,"Refresh must remain metadata-only");owner.transcriptionSetup->check();wait([&]{return owner.transcriptionSetup->status().busy;});need(owner.transcriptionSetup->status().failed,"Explicit Verify detects corrupt weights");need(installed.getChildFile("model.safetensors").replaceWithText("fixture"),"Repair fixture");owner.transcriptionSetup->check();wait([&]{return owner.transcriptionSetup->status().busy;});need(!owner.transcriptionSetup->status().failed,"Verify repaired model");
    owner.view.transcriptionCpuThreads=1;owner.view.transcriptionQuantize=true;owner.view.transcriptionGrid=5;owner.view.transcriptionStrength=63;owner.view.transcriptionVelocity=85;
    const auto preferences=root.getChildFile("transcription-view.json");publishSettings(preferences,owner.view.encode());const auto restored=ViewSettings::decode(preferences.loadFileAsString());need(restored.transcriptionCpuThreads==1&&restored.transcriptionQuantize&&restored.transcriptionGrid==5&&restored.transcriptionStrength==63&&restored.transcriptionVelocity==85,"Persist Preferences snapshot");
    auto malformed=juce::JSON::parse(owner.view.encode());malformed.getDynamicObject()->setProperty("transcriptionCpuThreads",99);malformed.getDynamicObject()->setProperty("transcriptionVelocity",0);malformed.getDynamicObject()->setProperty("transcriptionGrid",99);malformed.getDynamicObject()->setProperty("transcriptionStrength",101);malformed.getDynamicObject()->setProperty("transcriptionCompatibility","yes");const auto defaults=ViewSettings::decode(juce::JSON::toString(malformed));need(defaults.transcriptionCpuThreads==0&&defaults.transcriptionVelocity==100&&defaults.transcriptionGrid==3&&defaults.transcriptionStrength==100&&!defaults.transcriptionCompatibility,"Malformed settings use defaults");
    auto legacy=juce::JSON::parse(owner.view.encode());legacy.getDynamicObject()->setProperty("transcriptionBackend",2);need(ViewSettings::decode(juce::JSON::toString(legacy)).transcriptionBackend==1,"Legacy backend migrates to Python");owner.view.transcriptionQuantize=false;owner.view.transcriptionVelocity=100;
    const auto snapshot=[&](juce::Component& c,const juce::String& name){for(float scale:{1.f,1.25f,1.5f,2.f}){auto image=c.createComponentSnapshot(c.getLocalBounds(),true,scale);need(image.isValid(),"GUI snapshot");auto out=juce::File::getCurrentWorkingDirectory().getChildFile("transcription-"+name+"-"+juce::String(static_cast<int>(scale*100))+".png").createOutputStream();need(out&&juce::PNGImageFormat().writeImageToStream(image,*out),"Write preview");}};
    // Arrange gestures: click never converts; Ctrl-drag captures an internal
    // sample range, then dropping on an instrument previews MIDI only.
    owner.app.add_instrument_track("Gesture target");owner.refresh();auto& arrangement=*owner.arrangement;arrangement.setSize(1100,700);arrangement.pixelsPerSecond=360;arrangement.horizontal=0;arrangement.vertical=0;arrangement.resized();owner.snap=false;
    juce::Point<float> gestureOrigin;const auto mouse=[&](juce::Point<float> point,int mods){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(mods),1,0,0,0,0,&arrangement,&arrangement,juce::Time::getCurrentTime(),gestureOrigin,juce::Time::getCurrentTime(),1,true);};
    auto rect=arrangement.clipRect(owner.project()->clips.front());auto begin=rect.getRelativePoint(.25f,.7f),end=rect.getRelativePoint(.75f,.7f);gestureOrigin=begin;arrangement.mouseDown(mouse(begin,juce::ModifierKeys::leftButtonModifier));arrangement.mouseUp(mouse(begin,0));need(!owner.transcriptionWindow,"Click started conversion");
    const auto gestureBefore=*owner.project();arrangement.mouseDown(mouse(begin,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::ctrlModifier));arrangement.mouseDrag(mouse(end,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::ctrlModifier));arrangement.mouseUp(mouse(end,0));const auto range=arrangement.transcriptionRange(clip);need(range.range_length&&*range.range_length==8000&&range.range_offset==4000&&*owner.project()==gestureBefore,"Ctrl-drag range must not split or mutate audio");
    const auto targetY=static_cast<float>(arrangement.headerHeight()+arrangement.trackY(1)+arrangement.trackHeight/2);const juce::Point<float> dropPoint{arrangement.left+180.f,targetY};arrangement.mouseDown(mouse(begin,juce::ModifierKeys::leftButtonModifier));arrangement.mouseDrag(mouse(dropPoint,juce::ModifierKeys::leftButtonModifier));need(*owner.project()==gestureBefore&&!owner.transcriptionWindow,"MIDI drop preview mutated project");snapshot(arrangement,"timeline-drop");
    arrangement.mouseUp(mouse(dropPoint,0));need(owner.transcriptionWindow&&*owner.project()==gestureBefore,"Instrument drop should open converter without audio placement");owner.transcriptionWindow.reset();arrangement.selectedAudioRange.reset();need(owner.app.undo(),"Remove gesture fixture target");owner.refresh();need(*owner.project()==before,"Gesture fixture restores baseline");
    showTranscription(owner,clip);auto* view=dynamic_cast<TranscriptionPanel*>(owner.transcriptionWindow->getContentComponent());need(view&&view->model_.getNumItems()==1&&view->start_.isEnabled()&&!view->stem_.isVisible(),"Compact full-mix start");
    snapshot(*view,"mix-start");TranscriptionPreferences prefs(owner);prefs.setLookAndFeel(&owner.theme);snapshot(prefs,"preferences");prefs.setLookAndFeel(nullptr);snapshot(manager,"models");
    view->start_.onClick();owner.view.transcriptionVelocity=77;wait([&]{return busy(owner.app.transcription_status().state);});view->timerCallback();need(owner.app.transcription_status().state==JobState::ready&&view->roles_.size()==1&&view->place_.isEnabled(),"Real result list");view->none_.onClick();need(!view->place_.isEnabled(),"Zero selected disables create");view->all_.onClick();need(view->place_.isEnabled(),"Select all");snapshot(*view,"parts");
    view->place_.onClick();need(owner.app.transcription_status().state==JobState::placed&&owner.project()->clips.size()==before.clips.size()+1&&owner.project()->clips.back().midi->notes.front().velocity==100,"Atomic mix placement and captured Preferences");const auto after=*owner.project();need(owner.app.undo()&&*owner.project()==before&&owner.app.redo()&&*owner.project()==after,"Mix Undo/Redo");
    view->start_.onClick();wait([&]{return busy(owner.app.transcription_status().state);});owner.app.add_audio_track("Changed");view->timerCallback();need(owner.app.transcription_status().state==JobState::stale&&!view->place_.isEnabled(),"Stale result blocked");owner.transcriptionWindow.reset();
    const auto target=owner.app.add_instrument_track("Existing VSTi");owner.refresh();const auto stem_before=*owner.project();mrs::TranscriptionPlacement drop;drop.target_track=target;drop.position=8000;drop.range_offset=4000;drop.range_length=8000;
    showTranscription(owner,clip,drop);view=dynamic_cast<TranscriptionPanel*>(owner.transcriptionWindow->getContentComponent());need(view&&view->stem_.isVisible(),"Stem has one instrument selector");int bass=1;for(const auto& role:mrs::ace_midi_instruments()){if(role.role=="electric_bass")break;++bass;}view->stem_.setSelectedId(bass,juce::dontSendNotification);snapshot(*view,"stem-start");view->start_.onClick();wait([&]{return busy(owner.app.transcription_status().state);});view->timerCallback();need(owner.app.transcription_status().state==JobState::placed&&owner.project()->tracks==stem_before.tracks&&owner.project()->clips.size()==stem_before.clips.size()+1&&owner.project()->clips.back().track==target,"Stem auto placement without result picker");const auto stem_after=*owner.project();need(owner.app.undo()&&*owner.project()==stem_before&&owner.app.redo()&&*owner.project()==stem_after,"Stem Undo/Redo");owner.transcriptionWindow.reset();
    wav(source,1);owner.app.new_project(16000);owner.app.import_wavs({path(source)});const auto new_clip=owner.project()->clips.front().id;showTranscription(owner,new_clip);view=dynamic_cast<TranscriptionPanel*>(owner.transcriptionWindow->getContentComponent());view->start_.onClick();view->timerCallback();snapshot(*view,"progress");view->cancel_.onClick();wait([&]{return busy(owner.app.transcription_status().state);});need(owner.project()->clips.size()==1,"Cancel mutated project");owner.transcriptionWindow.reset();showTranscription(owner,new_clip);need(owner.transcriptionWindow!=nullptr,"Reopen converter");closeTranscription(owner);need(fs::is_empty(path(jobs)),"GUI worker cleanup");
}
}
