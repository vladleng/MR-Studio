#pragma once
#include "LiveContext.h"
#include "LiveTimelineEditState.h"

namespace ui {
class LiveMusicalEditor final : public juce::Component {
  public:
    LiveMusicalEditor(Desktop &desktop, juce::String item, live::MusicalLane kind, juce::String event)
        : owner(&desktop), itemId(std::move(item)), eventId(std::move(event)), lane(kind) {
        setComponentID("live-musical-editor");
        const auto row = live::spanById(live::musicalRows(owner->liveDocument->interfaceState, itemId, lane), eventId);
        original = row.clone();
        name.setText(row[lane == live::MusicalLane::sections ? "name" : "symbol"].toString(), false);
        start.setText(row["start"].toString(), false);
        end.setText(juce::String(static_cast<double>(row["start"]) + static_cast<double>(row["length"])), false);
        colors.addItemList({"graphite","blue","teal","green","amber","red","violet"}, 1);
        colors.setText(row["color"].isVoid() ? "blue" : row["color"].toString(), juce::dontSendNotification);
        const juce::StringArray captions{lane == live::MusicalLane::sections ? "Section" : "Chord symbol",
                                         "Start (bars, from 0)","End (bars)","Color"};
        const std::array<juce::Component *,4> fields{&name,&start,&end,&colors};
        for (std::size_t i=0;i<fields.size();++i) {
            addAndMakeVisible(*fields[i]); addAndMakeVisible(labels[i]);
            labels[i].setText(captions[static_cast<int>(i)],juce::dontSendNotification);
            fields[i]->setTitle(captions[static_cast<int>(i)]);
        }
        name.setInputRestrictions(120);
        addAndMakeVisible(error); addAndMakeVisible(apply);
        apply.onClick = [this] { commit(); };
        for (auto *field : {&name,&start,&end}) field->onReturnKey = [this] { commit(); };
        setSize(340,260);
    }
    void resized() override {
        auto area=getLocalBounds().reduced(8);
        const std::array<juce::Component *,4> fields{&name,&start,&end,&colors};
        for (std::size_t i=0;i<fields.size();++i) {
            labels[i].setBounds(area.removeFromTop(18)); fields[i]->setBounds(area.removeFromTop(28)); area.removeFromTop(2);
        }
        error.setBounds(area.removeFromTop(24)); apply.setBounds(area);
    }
    std::function<void()> close;
  private:
    static double numeric(const juce::String &text) {
        const auto bytes=text.trim().toStdString(); std::size_t used{};
        const double value=std::stod(bytes,&used);
        if (used!=bytes.size() || !std::isfinite(value)) throw std::runtime_error("Enter a finite bar position");
        return value;
    }
    void commit() {
        if (!owner || !owner->liveDocument) return;
        try {
            const double a=numeric(start.getText()), b=numeric(end.getText());
            owner->editLiveInterface("Musical event properties",[&](auto &state) {
                auto rows=live::musicalRows(state,itemId,lane).clone(); bool found=false;
                if (!rows.isArray()) throw std::runtime_error("Musical event no longer exists");
                for (auto &row:*rows.getArray()) if(row["id"].toString()==eventId) {
                    if (live::canonical(row) != live::canonical(original))
                        throw std::runtime_error("Event changed; reopen its properties");
                    live::put(row,lane==live::MusicalLane::sections?"name":"symbol",name.getText().trim());
                    live::put(row,"start",a); live::put(row,"length",b-a); live::put(row,"color",colors.getText()); found=true;
                }
                if(!found) throw std::runtime_error("Musical event no longer exists");
                live::setMusicalRows(state,itemId,lane,rows);
            });
            if(close)close();
        }catch(const std::exception &e){error.setText(juce::String::fromUTF8(e.what()),juce::dontSendNotification);}
    }
    juce::Component::SafePointer<Desktop> owner;
    juce::String itemId,eventId;
    juce::var original;
    live::MusicalLane lane;
    std::array<juce::Label,4> labels;
    juce::TextEditor name,start,end;
    juce::ComboBox colors;
    juce::Label error;
    juce::TextButton apply{"Apply"};
};
inline void openLiveMusicalEditor(Desktop &owner,const juce::String &item,live::MusicalLane lane,
                                    const juce::String &event,juce::Point<int> screen) {
    auto content=std::make_unique<LiveMusicalEditor>(owner,item,lane,event); auto *editor=content.get();
    auto &box=showLiveContext(std::move(content),owner,liveContextTarget(screen));
    juce::Component::SafePointer<juce::CallOutBox> safe(&box);
    editor->close=[safe]{if(safe)safe->dismiss();};
}
} // namespace ui
