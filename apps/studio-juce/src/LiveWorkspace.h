#pragma once
#include "Desktop.h"

namespace ui {
// Initial native entry slice: v5 panel geometry and shared host state.
// No browser simulator or independent transport/device is instantiated here.
class LiveWorkspace final : public juce::Component, private juce::Timer {
public:
    explicit LiveWorkspace(Desktop& desktop):owner(desktop) {
        setComponentID("mr-live-workspace");setName("MR Live");
        for(auto* button:{&home,&save,&add,&play,&pause,&stop,&studio,&previous,&next,&loop})addAndMakeVisible(button);
        home.onClick=[this]{owner.action(6);};save.onClick=[this]{owner.action(3);};
        add.onClick=[this]{owner.confirmDiscard([this]{owner.choose(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](const auto& file){owner.addLiveProject(file);},"*.mrsproject");});};
        play.onClick=[this]{owner.action(30);};pause.onClick=[this]{owner.action(31);};stop.onClick=[this]{owner.action(32);};
        previous.onClick=[this]{owner.action(34);};next.onClick=[this]{owner.action(35);};loop.onClick=[this]{owner.action(36);};
        studio.onClick=[this]{owner.setWorkspace(mrs::desktop::Workspace::arrange);};
        startTimerHz(15);
    }
    void resized() override {
        const float sx=getWidth()/1920.f,sy=getHeight()/1080.f;
        auto place=[&](juce::Component& c,int x,int y,int w,int h){c.setBounds(juce::roundToInt(x*sx),juce::roundToInt(y*sy),juce::roundToInt(w*sx),juce::jmax(24,juce::roundToInt(h*sy)));};
        place(home,12,12,80,40);place(save,100,12,80,40);place(studio,188,12,170,40);
        place(play,900,18,90,40);place(pause,1000,18,90,40);place(stop,1100,18,90,40);place(add,12,110,350,40);
        place(previous,550,100,140,32);place(next,700,100,140,32);place(loop,850,100,140,32);
        int channelY=168;for(auto& row:channels){place(row->mute,390,channelY,55,28);place(row->solo,452,channelY,55,28);channelY+=80;}
        int y=164;for(auto& button:songs){place(*button,12,y,350,44);y+=48;}
    }
    void paint(juce::Graphics& g) override {
        const float sx=getWidth()/1920.f,sy=getHeight()/1080.f;g.fillAll(juce::Colour(surface));
        auto panel=[&](int x,int y,int w,int h,const juce::String& title){auto r=juce::Rectangle<float>(x*sx,y*sy,w*sx,h*sy);g.setColour(juce::Colour(raised));g.fillRect(r.reduced(1));g.setColour(juce::Colours::lightgrey);g.setFont(owner.theme.font(13));g.drawText(title,r.reduced(10).withHeight(24),juce::Justification::topLeft);};
        panel(0,80,380,624,"Setlist");panel(0,704,380,212,"Playback / stems");panel(0,916,380,164,owner.footerStatus());
        panel(380,0,1540,80,owner.projectCaption());panel(380,82,150,998,"Channels");panel(530,82,1390,998,"Timeline");
        g.setColour(juce::Colours::lightgrey);g.setFont(owner.theme.font(13));
        const auto p=owner.project();const auto head=owner.displayState();
        g.drawText(juce::String(static_cast<double>(head.sample)/p->sample_rate,2)+" s",juce::roundToInt(1250*sx),12,140,40,juce::Justification::left);
        std::size_t channel=0;int y=140;for(const auto& track:p->tracks){g.drawText(juce::String::fromUTF8(track.name.c_str()),juce::roundToInt(390*sx),juce::roundToInt(y*sy),juce::roundToInt(130*sx),26,juce::Justification::left);
            if(channel<owner.peaks.size()){
                const auto peak=owner.peaks[channel];const float level=juce::jlimit(0.f,1.f,juce::jmax(peak.left,peak.right));
                g.setColour(juce::Colour(separatorColour));g.fillRect(juce::Rectangle<float>(390*sx,(y+60)*sy,120*sx,3));
                g.setColour(juce::Colours::lightgreen);g.fillRect(juce::Rectangle<float>(390*sx,(y+60)*sy,120*sx*level,3));
            }
            g.setColour(juce::Colours::lightgrey);++channel;y+=80;
        }
        for(std::size_t i=0;i<owner.contextLabels.size();++i)g.drawText(owner.contextLabels[i].getText(),juce::roundToInt(550*sx),juce::roundToInt((200+32*static_cast<int>(i))*sy),juce::jmax(0,getWidth()-juce::roundToInt(560*sx)),28,juce::Justification::left);
        g.drawText(p->tracks.empty()?"Add a Studio project to the setlist":"Open Studio to edit this shared project",juce::roundToInt(550*sx),juce::roundToInt(140*sy),juce::jmax(0,getWidth()-juce::roundToInt(560*sx)),30,juce::Justification::left);
    }
private:
    void timerCallback() override {
        if(!isVisible())return;
        juce::String signature;if(owner.liveDocument)for(const auto& file:owner.liveDocument->projects)signature+=file.getFullPathName()+"\n";
        if(signature!=songSignature){songSignature=signature;songs.clear();if(owner.liveDocument)for(std::size_t i=0;i<owner.liveDocument->projects.size();++i){auto button=std::make_unique<juce::TextButton>(owner.liveDocument->projects[i].getFileNameWithoutExtension());button->onClick=[this,i]{owner.confirmDiscard([this,i]{owner.run([&]{owner.selectLiveProject(static_cast<int>(i));});});};addAndMakeVisible(*button);songs.push_back(std::move(button));}resized();}
        const auto project=owner.project();juce::String tracks;
        for(const auto& track:project->tracks)tracks+=juce::String::fromUTF8(track.id.value.c_str())+"\n";
        if(tracks!=trackSignature){trackSignature=tracks;channels.clear();for(const auto& track:project->tracks){
            auto row=std::make_unique<Channel>();const auto id=track.id;
            row->mute.onClick=[this,id]{toggle(id,false);};row->solo.onClick=[this,id]{toggle(id,true);};
            addAndMakeVisible(row->mute);addAndMakeVisible(row->solo);channels.push_back(std::move(row));
        }resized();}
        for(std::size_t i=0;i<channels.size();++i){channels[i]->mute.setToggleState(project->tracks[i].mix.mute,juce::dontSendNotification);channels[i]->solo.setToggleState(project->tracks[i].mix.solo,juce::dontSendNotification);}
        repaint();
    }
    void toggle(mrs::Id id,bool solo){owner.run([&]{const auto p=owner.project();for(const auto& track:p->tracks)if(track.id==id){auto mix=track.mix;if(solo)mix.solo=!mix.solo;else mix.mute=!mix.mute;owner.app.set_track_mix(id,mix);break;}});}
    struct Channel {juce::TextButton mute{"M"},solo{"S"};};
    std::vector<std::unique_ptr<Channel>> channels;juce::String trackSignature;
    Desktop& owner;juce::String songSignature;std::vector<std::unique_ptr<juce::TextButton>> songs;
    juce::TextButton home{"Home"},save{"Save"},add{"Add Studio project..."},play{"Play"},pause{"Pause"},stop{"Stop"},studio{"Open Studio"},previous{"< Section"},next{"Section >"},loop{"Loop section"};
};
}
