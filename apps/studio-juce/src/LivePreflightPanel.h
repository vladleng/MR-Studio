#pragma once
#include "Desktop.h"
#include "LiveContext.h"
#include "LiveClipProjection.h"
#include <mrs/playback_preflight.hpp>
#include <future>
#include <stop_token>

namespace ui {
// Desktop retains cancelled jobs until their bounded worker exits. Closing a
// context never joins a file worker; final shutdown retires it outside audio.
struct LivePreflightJob {
    std::stop_source stop;
    std::future<std::vector<mrs::audio::PlaybackCheckResult>> pending;
    std::function<void(std::vector<mrs::audio::PlaybackCheckResult>)> completion;
    ~LivePreflightJob(){stop.request_stop();}
};
inline juce::String livePreflightFingerprint(Desktop& owner){
    if(!owner.liveDocument)return {};
    auto state=owner.liveDocument->interfaceState.clone();live::put(state,"currentItem",{});
    auto key=live::canonical(state)+":"+juce::String(owner.project()->sample_rate)+juce::String(owner.app.audio_name())+
        ":"+juce::String(static_cast<juce::int64>(owner.liveSourceCheckEpoch))+owner.liveFile.getFullPathName();
    for(const auto output:owner.app.active_outputs())key+=":"+juce::String(output);return key;
}
inline std::shared_ptr<LivePreflightJob> launchLiveInspection(Desktop& owner,std::vector<mrs::audio::PlaybackSourceCheck> requests){
    auto& retired=owner.livePreflightJobs;
    std::erase_if(retired,[](const auto& old){return old.use_count()==1&&!old->completion&&(!old->pending.valid()||old->pending.wait_for(std::chrono::seconds(0))==std::future_status::ready);});
    if(retired.size()>=4)throw std::runtime_error("Previous source checks are still cancelling; retry shortly");
    auto next=std::make_shared<LivePreflightJob>();const auto token=next->stop.get_token();
    const auto rate=owner.project()->sample_rate;auto outputs=owner.app.active_outputs();
    next->pending=std::async(std::launch::async,[requests=std::move(requests),outputs=std::move(outputs),rate,token]{return mrs::audio::check_playback_sources(requests,rate,outputs,token);});
    retired.push_back(next);return next;
}
inline void Desktop::pollLivePreflightJobs(){
    auto pending=livePreflightJobs;
    for(const auto& task:pending)if(task->completion&&task->pending.valid()&&task->pending.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        auto complete=std::move(task->completion);run([&]{complete(task->pending.get());});
    }
    pending.clear();
    std::erase_if(livePreflightJobs,[](const auto& task){return task.use_count()==1&&!task->completion&&(!task->pending.valid()||task->pending.wait_for(std::chrono::seconds(0))==std::future_status::ready);});
}
inline void queueLiveRelink(Desktop& owner,const juce::String& item,const juce::String& stem,const juce::File& file,std::function<void()> finished={}){
    owner.requireLiveRepair(item);const auto key=livePreflightFingerprint(owner);
    mrs::audio::PlaybackSourceCheck request;request.key="replacement";request.file=std::filesystem::path(file.getFullPathName().toWideCharPointer());
    const auto source=live::playbackStem(owner.liveDocument->interfaceState,item,stem);
    if(!source.isObject())throw std::runtime_error("Playback source was removed");
    const auto layout=live::playbackStemLayout(owner.liveDocument->interfaceState,item,source,owner.project()->sample_rate);
    request.begin=layout.begin;request.end=layout.end;
    if(layout.regions){request.regions.emplace();for(const auto& region:*layout.regions)request.regions->push_back({region.offset,region.length});}
    auto task=launchLiveInspection(owner,{request});juce::Component::SafePointer<Desktop> safe(&owner);
    task->completion=[safe,key,item,stem,file,finished=std::move(finished)](auto results){
        if(!safe||!safe->liveDocument)return;
        if(livePreflightFingerprint(*safe)!=key)throw std::runtime_error("Show or device changed. Choose the replacement again.");
        safe->requireLiveRepair(item);
        if(results.size()!=1||results.front().code!=mrs::audio::PlaybackCheckCode::ok)
            throw std::runtime_error(results.empty()?"Source check cancelled":results.front().detail);
        safe->commitLiveRelink(item,stem,file);if(finished)finished();
    };
}
class LivePreflightPanel final : public juce::Component,private juce::Timer {
    using Request=mrs::audio::PlaybackSourceCheck;
    using Result=mrs::audio::PlaybackCheckResult;
    using Code=mrs::audio::PlaybackCheckCode;
    struct Target {juce::String item,stem,name,file;bool click{};};
    struct Row final : juce::Component {
        juce::Label title,detail;juce::TextButton choose{"Choose WAV..."},master{"Use Master"},reload{"Reload source"};
        Row(){for(auto* child:std::array<juce::Component*,5>{&title,&detail,&choose,&master,&reload})addAndMakeVisible(child);}
        void resized() override{auto area=getLocalBounds().reduced(4);title.setBounds(area.removeFromTop(22));detail.setBounds(area.removeFromTop(28));
            choose.setBounds(area.removeFromLeft(136).withHeight(26));master.setBounds(area.removeFromLeft(116).withHeight(26));reload.setBounds(area.removeFromLeft(124).withHeight(26));}
        void paint(juce::Graphics& g)override{g.setColour(juce::Colour(0xff485463));g.drawHorizontalLine(getHeight()-1,0.f,static_cast<float>(getWidth()));}
    };
  public:
    explicit LivePreflightPanel(Desktop& desktop):owner(&desktop){
        setComponentID("live-preflight");setSize(660,460);
        addAndMakeVisible(title);addAndMakeVisible(status);addAndMakeVisible(check);addAndMakeVisible(viewport);
        title.setText("Show / source check",juce::dontSendNotification);
        check.setButtonText("Check again");check.onClick=[this]{startCheck();};
        viewport.setViewedComponent(&body,false);startTimerHz(10);startCheck();
    }
    ~LivePreflightPanel() override{stopTimer();if(job)job->stop.request_stop();viewport.setViewedComponent(nullptr,false);}
    bool busy()const{return static_cast<bool>(job);}
    void pollInspection(){timerCallback();}
    std::size_t sourceCount()const{return results.size();}
    void resized()override{
        auto area=getLocalBounds().reduced(12);auto top=area.removeFromTop(30);
        check.setBounds(top.removeFromRight(120));title.setBounds(top);status.setBounds(area.removeFromTop(56));viewport.setBounds(area);
        layoutRows();
    }
  private:
    juce::Component::SafePointer<Desktop> owner;
    juce::Label title,status;juce::TextButton check;juce::Viewport viewport;juce::Component body;
    std::shared_ptr<LivePreflightJob> job;
    std::vector<Target> targets;std::vector<Result> results;std::vector<std::unique_ptr<Row>> rows;
    juce::String checkedKey;
    juce::String fingerprint()const{return owner?livePreflightFingerprint(*owner):juce::String();}
    void launch(std::vector<Request> requests){
        if(job){job->stop.request_stop();job.reset();}
        checkedKey=fingerprint();job=launchLiveInspection(*owner,std::move(requests));
        check.setEnabled(false);status.setText("Checking sources... Playback continues.",juce::dontSendNotification);
    }
    void startCheck(){
        if(!owner||!owner->liveDocument)return;
        owner->run([&]{
            targets.clear();results.clear();rows.clear();
            std::vector<Request> requests;const auto& state=owner->liveDocument->interfaceState;
            for(const auto& item:*state["items"].getArray()){
                const auto id=item["id"].toString();const auto sources=live::playbackRows(state,id);
                std::vector<mrs::audio::PlaybackFileStamp> stamps;
                const auto cacheKey=live::playbackMediaKey(state,id)+":"+juce::String(owner->project()->sample_rate);
                for(const auto& cached:owner->livePreparedItems)if(cached.id==id&&cached.key==cacheKey)stamps=owner->app.playback_stamps(cached.media);
                for(std::size_t i=0;i<sources.size();++i){const auto& source=sources[i];const auto file=source["file"].toString();
                    Target target{id,i?source["id"].toString():juce::String(),item["name"].toString(),file,false};
                    Request request;request.key=std::to_string(targets.size());request.file=std::filesystem::path(file.toWideCharPointer());
                    request.begin=static_cast<double>(source["trimStart"]);request.end=source["trimEnd"].isVoid()?-1.:static_cast<double>(source["trimEnd"]);
                    request.outputs=live::playbackOutputs(source);if(i<stamps.size())request.expected=stamps[i];
                    const auto layout=live::playbackStemLayout(state,id,source,owner->project()->sample_rate);
                    if(layout.regions){request.regions.emplace();for(const auto& region:*layout.regions)request.regions->push_back({region.offset,region.length});}
                    targets.push_back(std::move(target));requests.push_back(std::move(request));
                }
            }
            launch(std::move(requests));layoutRows();
        });
    }
    void chooseSource(Target target){
        if(!owner)return;
        juce::Component::SafePointer<LivePreflightPanel> safe(this);juce::Component::SafePointer<Desktop> host(owner);
        owner->run([&]{owner->requireLiveRepair(target.item);
            owner->choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,host,target](const auto& file){
                if(!host||!host->liveDocument||!file.existsAsFile())return;
                host->run([&]{
                    if(safe&&safe->fingerprint()!=safe->checkedKey)throw std::runtime_error("Show or device changed. Check again before relinking.");
                    queueLiveRelink(*host,target.item,target.stem,file,[safe]{if(safe)safe->startCheck();});
                });
            },"*.wav");
        });
    }
    void useMaster(const Target& target){
        owner->run([&]{
            owner->requireLiveRepair(target.item,target.click);
            if(fingerprint()!=checkedKey)throw std::runtime_error("Show or device changed. Check again before repairing outputs.");
            if(target.click){auto click=live::liveClick(owner->liveDocument->interfaceState);click.output_left=click.output_right=-1;
                owner->setLiveAudio(live::playbackGain(owner->liveDocument->interfaceState),click);
            }else{
                owner->repairLivePlaybackOutputs(target.item,target.stem);

            }
            startCheck();
        });
    }
    void display(){
        rows.clear();std::size_t failures{};
        for(std::size_t i=0;i<results.size();++i){const auto result=results[i];const auto target=targets[i];auto row=std::make_unique<Row>();
            row->title.setText(target.name+" / "+(target.click?juce::String("Click"):juce::File(target.file).getFileName()),juce::dontSendNotification);
            row->title.setTooltip(target.file);row->detail.setText(juce::String::fromUTF8(result.detail.c_str()),juce::dontSendNotification);
            row->detail.setColour(juce::Label::textColourId,result.code==Code::ok?juce::Colour(0xffa9bacb):juce::Colour(0xffe0ba79));
            failures+=result.code!=Code::ok;
            const bool editable=owner&&!owner->app.recording()&&(target.item!=owner->liveDocument->interfaceState["currentItem"].toString()||owner->displayState().playback!=mrs::PlaybackState::playing);
            row->choose.setEnabled(editable&&!target.click);row->choose.setTooltip("Relink/reload WAV; trim, regions, channel and mix are retained.");
            row->master.setEnabled(editable&&result.code==Code::outputs);row->reload.setEnabled(editable&&!target.click);
            juce::Component::SafePointer<LivePreflightPanel> safe(this);
            row->reload.onClick=[safe,target]{if(safe&&safe->owner)safe->owner->run([&]{
                if(safe->fingerprint()!=safe->checkedKey)throw std::runtime_error("Show or device changed. Check again before reloading.");
                queueLiveRelink(*safe->owner,target.item,target.stem,juce::File(target.file),[safe]{if(safe)safe->startCheck();});
            });};
            row->choose.onClick=[this,target]{chooseSource(target);};row->master.onClick=[this,target]{useMaster(target);};
            body.addAndMakeVisible(*row);rows.push_back(std::move(row));
        }
        const auto device=owner->app.audio_name()=="Offline clock (no sound)"?juce::String("Offline clock — no physical sound"):juce::String::fromUTF8(owner->app.audio_name().c_str());
        status.setText((owner->app.audio_running()?juce::String():juce::String("Audio is not running. "))+juce::String(static_cast<int>(failures))+" issue(s), "+juce::String(static_cast<int>(results.size()))+" source/output checks. "+device+
            "\nSource checks do not certify prepared DSP or physical ASIO/MIDI.",juce::dontSendNotification);layoutRows();
    }
    void layoutRows(){const int width=juce::jmax(240,viewport.getWidth()-18);body.setSize(width,juce::jmax(viewport.getHeight(),static_cast<int>(rows.size())*90));
        for(std::size_t i=0;i<rows.size();++i)rows[i]->setBounds(0,static_cast<int>(i)*90,width,90);}
    void timerCallback()override{
        if(!job||job->pending.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        check.setEnabled(true);
        try{
            auto checked=job->pending.get();job.reset();
            if(!owner||!owner->liveDocument)return;
            if(fingerprint()!=checkedKey){status.setText("Show or device changed. Check again.",juce::dontSendNotification);return;}
            results=std::move(checked);
            const auto click=live::liveClick(owner->liveDocument->interfaceState);const auto outputs=owner->app.active_outputs();
            if(click.playback||click.output_left>=0||click.output_right>=0){Result result;result.detail="Click outputs checked; device/runtime verification is separate.";
                for(const auto output:{click.output_left,click.output_right})if(output>=0&&std::find(outputs.begin(),outputs.end(),output)==outputs.end()){
                    result.code=Code::outputs;result.detail="Click output is unavailable. Choose Master or enable it in Audio settings.";}
                targets.push_back({{}, {}, "Show", {}, true});results.push_back(std::move(result));}
            display();
        }catch(const std::exception& error){job.reset();status.setText(juce::String::fromUTF8(error.what()),juce::dontSendNotification);}
    }
};
inline void openLivePreflight(Desktop& owner){if(owner.liveDocument)showLiveContext(std::make_unique<LivePreflightPanel>(owner),owner,liveContextTarget(juce::Desktop::getMousePosition()));}
}
