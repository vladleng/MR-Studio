#pragma once
#include "LiveContext.h"
#include "LiveActionMenu.h"
#include "LiveTimelineEditState.h"

namespace ui {
// One persistent RMB surface: fields, swatches and the former context actions.
class LiveMusicalEditor final : public juce::Component, private juce::Timer {
  public:
    LiveMusicalEditor(Desktop &desktop, juce::String item, live::MusicalLane kind, juce::String event,
                      const juce::PopupMenu &menu = {})
        : owner(&desktop), itemId(std::move(item)), eventId(std::move(event)), lane(kind),
          generation(desktop.liveDocumentGeneration), actions(menu) {
        setComponentID("live-musical-editor");
        const auto row = live::spanById(live::musicalRows(owner->liveDocument->interfaceState, itemId, lane), eventId);
        original = row.clone();
        name.setText(row[lane == live::MusicalLane::sections ? "name" : "symbol"].toString(), false);
        start.setText(row["start"].toString(), false);
        end.setText(juce::String(static_cast<double>(row["start"]) + static_cast<double>(row["length"])), false);
        const juce::StringArray captions{lane == live::MusicalLane::sections ? "Section" : "Chord symbol",
                                         "Start (bars, from 0)", "End (bars)", "Color"};
        const std::array<juce::TextEditor *,3> fields{&name,&start,&end};
        for (std::size_t i=0;i<fields.size();++i) {
            addAndMakeVisible(*fields[i]); addAndMakeVisible(labels[i]);
            labels[i].setText(captions[static_cast<int>(i)],juce::dontSendNotification);
            fields[i]->setTitle(captions[static_cast<int>(i)]);
        }
        name.setInputRestrictions(120);
        name.setComponentID("live-musical-name");
        start.setComponentID("live-musical-start");
        end.setComponentID("live-musical-end");
        addAndMakeVisible(labels[3]); labels[3].setText(captions[3],juce::dontSendNotification);
        // Same 28x26 swatches / 40 px spacing as Studio MenuColourPalette.
        // Keep Live's existing seven persisted tokens and accepted block colors.
        for(std::size_t i=0;i<swatches.size();++i) {
            auto &button=swatches[i]; const auto color=colorNames[static_cast<int>(i)];
            addAndMakeVisible(button); button.setName(color); button.setTooltip(color);
            button.setComponentID("live-musical-color-"+color);
            button.setColour(juce::TextButton::buttonColourId,liveBlockColour(color));
            button.setColour(juce::TextButton::buttonOnColourId,liveBlockColour(color));
            button.onClick=[this,color]{ change([&]{owner->setLiveMusicalColor(
                itemId,eventId,lane==live::MusicalLane::sections,color);}); };
        }
        refreshSwatches();
        addAndMakeVisible(error);
        addAndMakeVisible(actions);
        actions.select=[this](int id){if(action)action(id);};
        name.onTextChange=[this]{commitName();};
        name.onReturnKey=[this]{commitName();};
        name.onFocusLost=name.onReturnKey;
        for(auto *field:{&start,&end}) {
            // Commit valid completed numbers automatically; debounce partial
            // numeric input, and flush on Return/focus loss. No Apply button.
            field->onTextChange=[this]{startTimer(200);};
            field->onReturnKey=[this]{commitBounds();};
            field->onFocusLost=field->onReturnKey;
        }
        setSize(340,240+actions.getHeight());
    }
    ~LiveMusicalEditor() override {
        stopTimer();
        for(auto *field:{&name,&start,&end}) {
            field->onTextChange={}; field->onFocusLost={}; field->onReturnKey={};
        }
    }
    void resized() override {
        auto area=getLocalBounds().reduced(8);
        const std::array<juce::Component *,3> fields{&name,&start,&end};
        for (std::size_t i=0;i<fields.size();++i) {
            labels[i].setBounds(area.removeFromTop(18)); fields[i]->setBounds(area.removeFromTop(28)); area.removeFromTop(2);
        }
        labels[3].setBounds(area.removeFromTop(18));
        const auto palette=area.removeFromTop(38);
        for(std::size_t i=0;i<swatches.size();++i)
            swatches[i].setBounds(palette.getX()+4+40*static_cast<int>(i),palette.getY()+4,28,26);
        error.setBounds(area.removeFromTop(24));
        actions.setBounds(area);
    }
    void paintOverChildren(juce::Graphics &g) override {
        g.setColour(liveReferenceTokens().colour(ColourRole::text));
        for(const auto &button:swatches) if(button.getToggleState())
            g.drawRoundedRectangle(button.getBounds().toFloat().reduced(1.f),2.f,1.f);
    }
    std::function<void(int)> action;
    std::function<void()> close;
  private:
    static double numeric(const juce::String &text) {
        const auto bytes=text.trim().toStdString(); std::size_t used{};
        const double value=std::stod(bytes,&used);
        if (used!=bytes.size() || !std::isfinite(value)) throw std::runtime_error("Enter a finite bar position");
        return value;
    }
    void refreshSwatches() {
        const auto color=original["color"].isVoid()?juce::String("blue"):original["color"].toString();
        for(std::size_t i=0;i<swatches.size();++i)
            swatches[i].setToggleState(colorNames[static_cast<int>(i)]==color,juce::dontSendNotification);
        repaint();
    }
    void change(const std::function<void()> &edit) {
        if (!owner || !owner->liveDocument) return;
        try {
            if(generation!=owner->liveDocumentGeneration)
                throw std::runtime_error("Show changed; reopen its properties");
            const auto current=live::spanById(
                live::musicalRows(owner->liveDocument->interfaceState,itemId,lane),eventId);
            if(!current.isObject()||!live::sameShowValue(current,original))
                throw std::runtime_error("Event changed; reopen its properties");
            edit();
            original=live::spanById(live::musicalRows(owner->liveDocument->interfaceState,itemId,lane),eventId).clone();
            error.setText({},juce::dontSendNotification);
            refreshSwatches();
        }catch(const std::exception &e){error.setText(juce::String::fromUTF8(e.what()),juce::dontSendNotification);}
    }
    void commitName() {
        change([&]{owner->setLiveMusicalName(itemId,eventId,lane==live::MusicalLane::sections,name.getText());});
    }
    void timerCallback() override { commitBounds(); }
    void commitBounds() {
        stopTimer();
        change([&]{
            const double a=numeric(start.getText()),b=numeric(end.getText());
            if(a==static_cast<double>(original["start"])&&b-a==static_cast<double>(original["length"]))return;
            owner->editLiveInterface("Musical event bounds",[&](auto &state){
                auto rows=live::musicalRows(state,itemId,lane).clone(); bool found=false;
                if(!rows.isArray())throw std::runtime_error("Musical event no longer exists");
                for(auto &row:*rows.getArray())if(row["id"].toString()==eventId){
                    if(!live::sameShowValue(row,original))throw std::runtime_error("Event changed; reopen its properties");
                    live::put(row,"start",a);live::put(row,"length",b-a);found=true;
                }
                if(!found)throw std::runtime_error("Musical event no longer exists");
                live::setMusicalRows(state,itemId,lane,rows);
            });
        });
    }
    juce::Component::SafePointer<Desktop> owner;
    juce::String itemId,eventId;
    juce::var original;
    live::MusicalLane lane;
    std::uint64_t generation{};
    const juce::StringArray colorNames{"blue","violet","teal","green","amber","red","graphite"};
    std::array<juce::Label,4> labels;
    juce::TextEditor name,start,end;
    std::array<juce::TextButton,7> swatches;
    juce::Label error;
    LiveActionMenu actions;
};
inline void openLiveMusicalEditor(Desktop &owner,const juce::String &item,live::MusicalLane lane,
                                 const juce::String &event,juce::Point<int> screen,
                                 const juce::PopupMenu &menu={},std::function<void(int)> action={}) {
    auto content=std::make_unique<LiveMusicalEditor>(owner,item,lane,event,menu); auto *editor=content.get();
    auto &box=showLiveContext(std::move(content),owner,liveContextTarget(screen));
    juce::Component::SafePointer<juce::CallOutBox> safe(&box);
    editor->close=[safe]{if(safe)safe->dismiss();};
    editor->action=[safe,action=std::move(action)](int id){
        if(safe)safe->dismiss();
        if(id&&action)juce::MessageManager::callAsync([action,id]{action(id);});
    };
}
} // namespace ui
