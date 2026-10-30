#pragma once
#include "LiveContext.h"
#include "LivePlaybackState.h"
namespace ui {
class LiveClipEditor final:public juce::Component {
 public:
    LiveClipEditor(Desktop& desktop,juce::String item,juce::String stem,juce::String clip,juce::var rows,double sourceBars)
        :owner(&desktop),itemId(std::move(item)),stemId(std::move(stem)),clipId(std::move(clip)),original(rows.clone()),sourceLength(sourceBars){
        setComponentID("live-clip-editor");
        sourceFile=live::playbackStem(owner->liveDocument->interfaceState,itemId,stemId)["file"].toString();
        for(const auto& row:*original.getArray())if(row["id"].toString()==clipId){
            fields[0].setText(row["start"].toString(),false);fields[1].setText(row["length"].toString(),false);fields[2].setText(row["offset"].toString(),false);
        }
        const juce::StringArray names{"Start (bars)","Length (bars)","Source offset (bars)"};
        for(std::size_t i=0;i<fields.size();++i){addAndMakeVisible(fields[i]);addAndMakeVisible(labels[i]);labels[i].setText(names[static_cast<int>(i)],juce::dontSendNotification);fields[i].onReturnKey=[this]{commit();};}
        addAndMakeVisible(error);addAndMakeVisible(apply);apply.onClick=[this]{commit();};setSize(340,230);
    }
    void resized()override{auto area=getLocalBounds().reduced(8);for(std::size_t i=0;i<fields.size();++i){labels[i].setBounds(area.removeFromTop(20));fields[i].setBounds(area.removeFromTop(30));}error.setBounds(area.removeFromTop(30));apply.setBounds(area);}
    std::function<void()> close;
 private:
    static double numeric(const juce::String& text){std::size_t used{};const auto bytes=text.trim().toStdString();const auto value=std::stod(bytes,&used);if(used!=bytes.size()||!std::isfinite(value))throw std::runtime_error("Enter a finite bar value");return value;}
    void commit(){if(!owner||!owner->liveDocument)return;try{
        const double start=numeric(fields[0].getText()),length=numeric(fields[1].getText()),offset=numeric(fields[2].getText());
        if(offset<0||offset+length>sourceLength+1.e-7)throw std::runtime_error("Region exceeds source WAV");
        owner->editLiveInterface("Playback region properties",[&](auto& state){
            const auto row=live::playbackStem(state,itemId,stemId);
            if(row["file"].toString()!=sourceFile)throw std::runtime_error("WAV source changed; reopen properties");
            auto current=live::playbackRegions(state,itemId,row);
            if(current.isVoid())current=live::defaultPlaybackRegions(row,sourceLength,live::playbackSecondsPerBar(state,itemId));
            if(live::canonical(current)!=live::canonical(original))throw std::runtime_error("Regions changed; reopen properties");
            auto next=current.clone();for(auto& region:*next.getArray())if(region["id"].toString()==clipId){live::put(region,"start",start);live::put(region,"length",length);live::put(region,"offset",offset);}
            live::setPlaybackRegions(state,itemId,stemId,next);
        });owner->syncLivePlayback();if(close)close();
    }catch(const std::exception& e){error.setText(juce::String::fromUTF8(e.what()),juce::dontSendNotification);}}
    juce::Component::SafePointer<Desktop> owner;
    juce::String itemId,stemId,clipId,sourceFile;
    juce::var original;double sourceLength{};
    std::array<juce::TextEditor,3> fields;std::array<juce::Label,3> labels;
    juce::Label error;juce::TextButton apply{"Apply"};
};
inline void openLiveClipEditor(Desktop& owner,const juce::String& item,const juce::String& stem,const juce::String& clip,
                               const juce::var& rows,double sourceBars,juce::Point<int> screen){
    auto content=std::make_unique<LiveClipEditor>(owner,item,stem,clip,rows,sourceBars);auto* raw=content.get();
    auto& callout=showLiveContext(std::move(content),owner,liveContextTarget(screen));
    raw->close=[safe=juce::Component::SafePointer<juce::CallOutBox>(&callout)]{if(safe)safe->dismiss();};
}
}
