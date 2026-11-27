#pragma once
#include "Desktop.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <atomic>
#include <future>
#include <filesystem>

namespace ui {
// Dedicated Studio modal window. No model writes from the worker. Vendor
// processor/ARA creation and destruction stay with the control owner.
class ExportAudioPanel final : public juce::Component,private juce::Timer {
    friend void exportAudioSmoke();
    using Plan=mrs::desktop::Application::PreparedMasterExport;
    using Result=mrs::audio::WavExportResult;
    using Clock=std::chrono::steady_clock;
    Desktop& owner;
    const bool liveExport;
    juce::Component trackList;
    juce::Viewport trackViewport;
    struct StemRow {
        mrs::Id id;
        std::unique_ptr<juce::ToggleButton> toggle;
        bool eligible{};
    };
    std::vector<StemRow> stemRows;
    juce::Label trackHeading;
    juce::TextButton selectAll{"All"},selectNone{"None"};
    juce::Label heading,subheading,formatLabel,nameLabel,folderLabel,rangeLabel,durationLabel,operation,metrics,detail;
    juce::TextButton wav{"WAV   |   48 kHz / 24 bit"},mp3{"MP3   |   320 kbps CBR"},browse{"Browse..."},start{"Export"},cancel{"Cancel"},reveal{"Open folder"};
    juce::TextEditor name,folder;
    juce::ComboBox range;
    double barValue{};
    juce::ProgressBar bar{barValue};
    std::array<std::optional<mrs::LoopRange>,3> ranges;
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<Plan> plan;
    struct Progress {
        std::atomic<mrs::Sample> processed{},total{},written{};
        std::atomic<float> conversion{-1.f};
        std::stop_source stop;
    };
    std::shared_ptr<Progress> progress;
    std::future<Result> work;
    Clock::time_point started{};
    juce::File output;
    int format{1};
    bool busy{},finished{};
    static juce::String timeText(double seconds){
        if(!std::isfinite(seconds)||seconds<0)return "--:--";
        const auto s=static_cast<juce::int64>(std::min(seconds,359999.));
        return juce::String(s/60).paddedLeft('0',2)+":"+juce::String(s%60).paddedLeft('0',2);
    }
    void labelText(juce::Label& l,const juce::String& text,bool secondary=true){
        l.setText(text,juce::dontSendNotification);
        l.setFont(secondary?Theme::detailFont():Theme::browserFont());
        l.setColour(juce::Label::textColourId,juce::Colour(secondary?secondaryText:primaryText));
        l.setJustificationType(juce::Justification::centredLeft);addAndMakeVisible(l);
    }
    std::optional<mrs::LoopRange> selectedRange()const{
        const auto id=range.getSelectedId();return id>=1&&id<=3?ranges[static_cast<std::size_t>(id-1)]:std::nullopt;
    }
    std::vector<mrs::Id> selectedTracks()const{
        std::vector<mrs::Id> ids;
        for(const auto& row:stemRows)if(row.eligible&&row.toggle->getToggleState())ids.push_back(row.id);
        return ids;
    }
    void setAllTracks(bool selected){
        for(auto& row:stemRows)if(row.eligible)row.toggle->setToggleState(selected,juce::dontSendNotification);
        updateRange();
    }
    void selectFormat(int id){
        format=id;wav.setToggleState(id==1,juce::dontSendNotification);mp3.setToggleState(id==2,juce::dontSendNotification);
        updateRange();
    }
    void updateRange(){
        const auto r=selectedRange();
        const auto seconds=r?static_cast<double>(r->end-r->start)/owner.project()->sample_rate:0.;
        const auto selected=liveExport?selectedTracks().size():0;
        const auto mode=liveExport?juce::String(static_cast<int>(selected))+" stems  /  Offline":juce::String("Master Out  /  Offline");
        durationLabel.setText(r?"Duration  "+timeText(seconds)+"    |    "+mode:"Selected range is unavailable",juce::dontSendNotification);
        start.setEnabled(!busy&&r.has_value()&&(!liveExport||selected>0)&&name.getText().trim().isNotEmpty()&&folder.getText().trim().isNotEmpty());
    }
    void enableSettings(bool enabled){
        for(auto* c:std::array<juce::Component*,6>{&wav,&mp3,&browse,&name,&folder,&range})c->setEnabled(enabled);
        if(liveExport){
            selectAll.setEnabled(enabled);selectNone.setEnabled(enabled);
            for(auto& row:stemRows)row.toggle->setEnabled(enabled&&row.eligible);
        }
        updateRange();
    }
    void releasePlan(){
        if(plan){auto p=std::move(plan);owner.app.finish_master_export(p);}
    }
    void begin(){
        if(busy)return;
        const auto r=selectedRange();if(!r)return;
        const auto ids=selectedTracks();
        if(liveExport&&ids.empty())return;
        try{
            if(owner.app.recording()||owner.app.engine()->state().playback==mrs::PlaybackState::playing)throw std::runtime_error("Stop playback or recording before export");
            const auto filename=name.getText().trim();
            if(filename.isEmpty()||filename!=juce::File::createLegalFileName(filename)||filename=="."||filename=="..")
                throw std::invalid_argument("Enter a valid filename without a directory");
            if(!juce::File::isAbsolutePath(folder.getText()))throw std::invalid_argument("Choose an absolute output directory");
            const juce::File directory(folder.getText());
            output=directory.getChildFile(liveExport?filename:filename+(format==1?".wav":".mp3"));
            if(output.exists())throw std::runtime_error("Output already exists. Choose another name.");
            const auto made=directory.createDirectory();if(made.failed())throw std::runtime_error(made.getErrorMessage().toStdString());
            busy=true;finished=false;enableSettings(false);reveal.setVisible(false);cancel.setButtonText("Cancel Export");
            barValue=0.;operation.setText("Preparing project and plugins...",juce::dontSendNotification);
            detail.setText({},juce::dontSendNotification);metrics.setText({},juce::dontSendNotification);
            if(!liveExport){owner.view.exportFormat=format;owner.view.exportRange=range.getSelectedId();owner.saveSettings();}
            // This phase intentionally stays on the control thread (ARA/VST3).
            // No general message-loop pumping while preparing the graph.
            started=Clock::now();
            plan=liveExport?owner.app.prepare_stem_export(r->start,r->end,ids):owner.app.prepare_master_export(r->start,r->end);
            progress=std::make_shared<Progress>();
            const auto state=progress;const auto job=plan;
            const auto destination=std::filesystem::path(output.getFullPathName().toWideCharPointer());
            const bool isMp3=format==2;
            const bool stems=liveExport;
            work=std::async(std::launch::async,[job,state,destination,isMp3,stems]()->Result{
                mrs::audio::WavExportCallbacks callbacks;
                callbacks.stop=state->stop.get_token();
                callbacks.continue_render=[state](const auto& p){
                    state->processed.store(p.processed);state->total.store(p.total);state->written.store(p.written);
                    return !state->stop.stop_requested();
                };
                callbacks.continue_conversion=[state](float p){state->conversion.store(p);return !state->stop.stop_requested();};
                if(stems){
                    const auto batch=job->export_stem_wavs(destination,callbacks);
                    Result aggregate{};
                    aggregate.completed=batch.completed;
                    for(const auto& stem:batch.stems){
                        aggregate.frames=stem.frames;
                        aggregate.peak=std::max(aggregate.peak,stem.peak);
                        aggregate.clipped_samples+=stem.clipped_samples;
                    }
                    return aggregate;
                }
                const mrs::audio::MasterRenderSource source=[job](const auto& cb){return job->render(cb);};
                return isMp3?mrs::audio::export_mp3(destination,job->rate(),job->end()-job->begin(),source,callbacks):
                             mrs::audio::export_wav24(destination,job->rate(),job->end()-job->begin(),source,callbacks);
            });
            operation.setText(liveExport?"Rendering selected stems...":"Rendering Master Out...",juce::dontSendNotification);
            startTimerHz(10);
        }catch(const std::exception& e){
            juce::String reason=juce::String::fromUTF8(e.what());
            try{releasePlan();}catch(const std::exception& restore){reason+=" | "+juce::String::fromUTF8(restore.what());}
            busy=false;enableSettings(true);cancel.setButtonText("Close");operation.setText("Export failed",juce::dontSendNotification);
            detail.setText(reason,juce::dontSendNotification);updateRange();
        }
    }
    void timerCallback()override{
        if(!busy||!work.valid())return;
        const auto elapsed=std::chrono::duration<double>(Clock::now()-started).count();
        const auto total=progress->total.load(),processed=progress->processed.load();
        const auto converting=progress->conversion.load();
        const bool hasConversion=format==2||plan->rate()!=48000;
        const double rendered=total>0?std::clamp(static_cast<double>(processed)/static_cast<double>(total),0.,1.):0.;
        barValue=converting>=0?.85+.15*static_cast<double>(converting):rendered*(hasConversion?.85:1.);
        operation.setText(progress->stop.stop_requested()?"Cancelling...":converting>=0?"Encoding / converting audio...":liveExport?"Rendering selected stems...":"Rendering Master Out...",juce::dontSendNotification);
        const auto remaining=barValue>.001?elapsed*(1./barValue-1.): -1.;
        const auto speed=elapsed>0?static_cast<double>(processed)/plan->rate()/elapsed:0.;
        metrics.setText("Elapsed  "+timeText(elapsed)+"     Remaining  ~"+timeText(remaining)+"     Speed  "+juce::String(speed,1)+"x",juce::dontSendNotification);
        if(work.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        stopTimer();
        Result result{};juce::String error;
        try{result=work.get();}catch(const std::exception& e){error=juce::String::fromUTF8(e.what());}catch(...){error="Unknown export error";}
        try{releasePlan();}catch(const std::exception& e){error+=" | Audio restart: "+juce::String::fromUTF8(e.what());}
        busy=false;finished=result.completed;
        enableSettings(true);cancel.setButtonText("Close");cancel.setEnabled(true);
        if(error.isNotEmpty()){operation.setText(result.completed?"File saved; audio restart failed":"Export failed",juce::dontSendNotification);reveal.setVisible(result.completed);detail.setText(error,juce::dontSendNotification);}
        else if(result.completed){
            barValue=1.;operation.setText(liveExport?"Stem export complete":"Export complete",juce::dontSendNotification);reveal.setVisible(true);
            detail.setText(result.clipped_samples?"Warning: "+juce::String(static_cast<juce::int64>(result.clipped_samples))+" clipped samples; no normalization applied.": liveExport?"WAV stems saved. Live Item creation is not available in this stage.":"File saved successfully.",juce::dontSendNotification);
        }else{operation.setText("Export cancelled",juce::dontSendNotification);detail.setText(liveExport?"Incomplete stems removed.":"Incomplete file removed.",juce::dontSendNotification);}
        updateRange();repaint();
    }
public:
    explicit ExportAudioPanel(Desktop& d,bool forLive=false):owner(d),liveExport(forLive){
        setComponentID(liveExport?"export-to-live":"export-audio");
        labelText(heading,liveExport?"Export to MR Live":"Stereo mixdown",false);heading.setFont(Theme::browserFont().withHeight(23.f));
        labelText(subheading,liveExport?"Audio stage: render selected backing tracks to WAV.":"Render the Master signal directly to disk.");
        labelText(formatLabel,liveExport?"STEREO WAV  /  48 kHz  /  24 bit":"OUTPUT FORMAT");labelText(nameLabel,liveExport?"ITEM / FOLDER NAME":"FILE NAME");
        labelText(folderLabel,"DESTINATION");labelText(rangeLabel,"EXPORT RANGE");
        labelText(durationLabel,"");labelText(operation,"Ready to export",false);labelText(metrics,"");labelText(detail,"");
        for(auto* c:std::array<juce::Component*,10>{&wav,&mp3,&browse,&name,&folder,&range,&start,&cancel,&reveal,&bar})addAndMakeVisible(*c);
        wav.setRadioGroupId(721);mp3.setRadioGroupId(721);
        wav.onClick=[this]{selectFormat(1);};mp3.onClick=[this]{selectFormat(2);};
        start.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff496e98));
        bar.setColour(juce::ProgressBar::foregroundColourId,juce::Colour(0xff648db8));
        bar.setColour(juce::ProgressBar::backgroundColourId,juce::Colour(background));
        const auto p=owner.project();const mrs::Timeline time(p->time,p->sample_rate);
        if(liveExport){
            wav.setVisible(false);mp3.setVisible(false);
            labelText(trackHeading,"SELECT TRACKS  /  MASTER ROUTES");
            addAndMakeVisible(selectAll);addAndMakeVisible(selectNone);
            addAndMakeVisible(trackViewport);trackViewport.setViewedComponent(&trackList,false);
            trackViewport.setScrollBarsShown(true,false);
            for(const auto& track:p->tracks){
                const bool eligible=track.kind!=mrs::TrackKind::midi&&!track.output&&track.hardware_outputs.empty()&&track.midi_output.empty();
                juce::String kind=track.kind==mrs::TrackKind::audio?"Audio":track.kind==mrs::TrackKind::instrument?"Instrument":track.kind==mrs::TrackKind::bus?"Bus":"MIDI";
                auto title=juce::String::fromUTF8(track.name.c_str())+"  |  "+kind;
                if(!eligible)title+="  (unsupported route)";
                else if(track.kind==mrs::TrackKind::bus)title+="  (includes all inputs)";
                if(track.mix.mute)title+="  [Muted]";
                if(track.mix.solo)title+="  [Solo]";
                auto toggle=std::make_unique<juce::ToggleButton>(title);
                toggle->setEnabled(eligible);
                toggle->onClick=[this]{updateRange();};
                trackList.addAndMakeVisible(*toggle);
                stemRows.push_back({track.id,std::move(toggle),eligible});
            }
            selectAll.onClick=[this]{setAllTracks(true);};selectNone.onClick=[this]{setAllTracks(false);};
            start.setButtonText("Export Stems");
        }
        mrs::Sample end{};for(const auto& c:p->clips)end=std::max(end,c.midi?time.to_samples(c.midi->start+c.midi->length):c.start+c.length);
        if(end>0)ranges[0]=mrs::LoopRange{0,end};
        std::optional<mrs::Sample> first,last;bool ambiguous=false;
        for(const auto& marker:p->markers){
            const auto text=juce::String::fromUTF8(marker.name.c_str()).trim().toLowerCase();
            if(text=="start"){if(first)ambiguous=true;first=time.to_samples(marker.tick);}
            if(text=="end"){if(last)ambiguous=true;last=time.to_samples(marker.tick);}
        }
        if(!ambiguous&&first&&last&&*last>*first)ranges[1]=mrs::LoopRange{*first,*last};
        ranges[2]=owner.app.engine()->state().loop;
        if(!ranges[2]&&owner.arrangement)ranges[2]=owner.arrangement->rememberedLoop();
        if(ranges[2]&&ranges[2]->end<=ranges[2]->start)ranges[2].reset();
        range.addItem("Entire Project",1);range.addItem("Between Start / End Markers",2);range.addItem("Loop Range",3);
        for(int id=1;id<=3;++id)range.setItemEnabled(id,ranges[static_cast<std::size_t>(id-1)].has_value());
        const auto requested=liveExport?(ranges[1]?2:1):juce::jlimit(1,3,owner.view.exportRange);
        range.setSelectedId(ranges[static_cast<std::size_t>(requested-1)]?requested:1,juce::dontSendNotification);
        name.setText(juce::File::createLegalFileName(juce::String::fromUTF8(p->title.c_str())),false);
        const auto root=owner.app.path().empty()?juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MR Studio"):
            juce::File(juce::String(owner.app.path().parent_path().wstring().c_str()));
        folder.setText(root.getChildFile("Exports").getFullPathName(),false);
        folder.setReadOnly(true);
        browse.onClick=[this]{
            chooser=std::make_unique<juce::FileChooser>("Export destination",juce::File(folder.getText()),"*",true);
            juce::Component::SafePointer<ExportAudioPanel> safe(this);
            chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,
                [safe](const juce::FileChooser& c){if(safe&&c.getResult()!=juce::File()){safe->folder.setText(c.getResult().getFullPathName(),false);safe->updateRange();}});
        };
        range.onChange=[this]{updateRange();};name.onTextChange=[this]{updateRange();};
        start.onClick=[this]{begin();};cancel.onClick=[this]{requestClose();};
        reveal.onClick=[this]{if(finished){if(liveExport)output.revealToUser();else output.getParentDirectory().revealToUser();}};reveal.setVisible(false);
        selectFormat(liveExport?1:owner.view.exportFormat);setSize(liveExport?640:580,liveExport?720:540);
    }
    ~ExportAudioPanel()override{
        stopTimer();chooser.reset();if(progress)progress->stop.request_stop();
        if(work.valid()){try{(void)work.get();}catch(...){}}
        try{releasePlan();}catch(...){}
    }
    bool keyPressed(const juce::KeyPress& key)override{if(key.getKeyCode()==juce::KeyPress::escapeKey){requestClose();return true;}return false;}
    void requestClose(){
        if(busy){if(progress)progress->stop.request_stop();cancel.setEnabled(false);return;}
        if(auto* window=findParentComponentOfClass<juce::DocumentWindow>()){window->exitModalState(0);window->setVisible(false);}
    }
    void paint(juce::Graphics& g)override{
        g.fillAll(juce::Colour(0xff242529));g.setColour(juce::Colour(separatorColour));
        g.drawHorizontalLine(86,20.f,static_cast<float>(getWidth()-20));
    }
    void resized()override{
        auto area=getLocalBounds().reduced(20);
        heading.setBounds(area.removeFromTop(32));subheading.setBounds(area.removeFromTop(28));area.removeFromTop(18);
        formatLabel.setBounds(area.removeFromTop(20));
        if(!liveExport){
            auto formats=area.removeFromTop(50);
            wav.setBounds(formats.removeFromLeft(formats.getWidth()/2-4));formats.removeFromLeft(8);mp3.setBounds(formats);
        }
        area.removeFromTop(12);
        nameLabel.setBounds(area.removeFromTop(20));name.setBounds(area.removeFromTop(32));area.removeFromTop(10);
        folderLabel.setBounds(area.removeFromTop(20));auto destination=area.removeFromTop(32);
        browse.setBounds(destination.removeFromRight(92));destination.removeFromRight(8);folder.setBounds(destination);area.removeFromTop(10);
        rangeLabel.setBounds(area.removeFromTop(20));range.setBounds(area.removeFromTop(32));durationLabel.setBounds(area.removeFromTop(27));
        auto footer=area.removeFromBottom(34);start.setBounds(footer.removeFromRight(liveExport?120:100));footer.removeFromRight(8);cancel.setBounds(footer.removeFromRight(124));reveal.setBounds(footer.removeFromLeft(110));
        if(liveExport){
            auto trackHeader=area.removeFromTop(28);
            selectNone.setBounds(trackHeader.removeFromRight(65));trackHeader.removeFromRight(6);
            selectAll.setBounds(trackHeader.removeFromRight(65));trackHeading.setBounds(trackHeader);
            area.removeFromTop(4);
            trackViewport.setBounds(area.removeFromTop(std::max(0,area.getHeight()-124)));
            trackList.setSize(std::max(1,trackViewport.getWidth()-18),std::max(1,static_cast<int>(stemRows.size())*30));
            for(std::size_t i=0;i<stemRows.size();++i)
                stemRows[i].toggle->setBounds(4,static_cast<int>(i)*30,std::max(1,trackList.getWidth()-8),30);
        }
        area.removeFromTop(6);operation.setBounds(area.removeFromTop(25));bar.setBounds(area.removeFromTop(15));metrics.setBounds(area.removeFromTop(25));detail.setBounds(area);
    }
};
}
