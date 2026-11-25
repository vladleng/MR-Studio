#pragma once
#include "Controls.h"
#include <mrs/core.hpp>
#include <MRConceptAssets.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <memory>
#include <utility>
#include <vector>
namespace ui {
// Note-value grid shared by the piano roll and the drum grid. Values follow the
// quarter-note PPQ ladder: straight, triplet (T), quintuplet (5:4), septuplet
// (7:8) and swing variants. A swing line is displaced toward the triplet point,
// so 0% is the straight grid and 100% gives a 2:1 (triplet) pair.
struct NoteGridSpec {
    enum class Shape:int{straight,triplet,quintuplet,septuplet,swing};
    int divisor{16};int swing{};
    Shape shape{Shape::straight};
    bool operator==(const NoteGridSpec&) const=default;
    bool operator!=(const NoteGridSpec&) const=default;

    // Base note length of this note value.
    double unit() const{return 4.*static_cast<double>(mrs::ppq)/static_cast<double>(std::max(1,divisor));}
    // Nominal line spacing; swing only displaces every second line.
    double step() const{
        switch(shape){
            case Shape::triplet:return unit()*2./3.;
            case Shape::quintuplet:return unit()*4./5.;
            case Shape::septuplet:return unit()*8./7.;
            case Shape::swing:return unit()+swingOffset()*.5;
            default:return unit();
        }
    }
    // Displacement of the weak line from the straight position (0..unit/3).
    double swingOffset() const{
        return shape==Shape::swing?static_cast<double>(std::clamp(swing,0,100))/100.*unit()/3.:0.;
    }
    juce::String value() const{return "1/"+juce::String(std::max(1,divisor));}
    juce::String modifier() const{
        switch(shape){
            case Shape::triplet:return "T";
            case Shape::quintuplet:return "5:4";
            case Shape::septuplet:return "7:8";
            case Shape::swing:return juce::String(std::clamp(swing,0,100))+"% sw";
            default:return {};
        }
    }
    juce::String label() const{const auto tail=modifier();return tail.isEmpty()?value():value()+" "+tail;}
    // Grid line position in ticks. Negative indexes continue the same repeating
    // pattern, so snapped drag deltas stay symmetric around the gesture anchor.
    mrs::Tick position(long long index) const{
        double tick=static_cast<double>(index)*(shape==Shape::swing?unit():step());
        if(shape==Shape::swing&&(index%2)!=0)tick+=swingOffset();
        const auto rounded=std::llround(tick);
        return static_cast<mrs::Tick>(std::clamp<long long>(rounded,-mrs::max_tick,mrs::max_tick));
    }
    long long indexAt(mrs::Tick tick) const{
        return static_cast<long long>(std::llround(static_cast<double>(tick)/std::max(1.,step())));
    }
    // Nearest grid line; septuplets round to the shared PPQ tick.
    mrs::Tick snap(mrs::Tick tick) const{
        tick=std::min(tick,mrs::max_tick);
        const auto index=indexAt(tick);
        auto best=position(index);
        auto distance=std::llabs(static_cast<long long>(tick)-static_cast<long long>(best));
        for(auto candidate=index-2;candidate<=index+2;++candidate){
            const auto line=position(candidate);
            const auto next=std::llabs(static_cast<long long>(tick)-static_cast<long long>(line));
            if(next<distance){distance=next;best=line;}
        }
        return best;
    }
};
// Reference ladder in menu order: straight values, triplets, 5:4, 7:8, then
// swing variants of eighths and sixteenths.
inline const std::vector<NoteGridSpec>& noteGridChoices(){
    static const std::vector<NoteGridSpec> choices=[]{
        std::vector<NoteGridSpec> values;
        for(const int divisor:{1,2,4,8,16,32,64})values.push_back({divisor,0,NoteGridSpec::Shape::straight});
        for(const int divisor:{4,8,16,32})values.push_back({divisor,0,NoteGridSpec::Shape::triplet});
        for(const int divisor:{8,16,32})values.push_back({divisor,0,NoteGridSpec::Shape::quintuplet});
        for(const int divisor:{8,16,32})values.push_back({divisor,0,NoteGridSpec::Shape::septuplet});
        for(const int divisor:{8,16})for(const int swing:{20,40,60,80,100})values.push_back({divisor,swing,NoteGridSpec::Shape::swing});
        return values;
    }();
    return choices;
}
// Settings may hold any stored value; anything outside the ladder becomes 1/16.
inline NoteGridSpec canonicalNoteGrid(const NoteGridSpec& value){
    for(const auto& choice:noteGridChoices())if(choice==value)return choice;
    return NoteGridSpec{};
}
// Toolbar control: snap icon, current grid and a menu chevron in one button.
class NoteGridButton final:public juce::Button {
public:
    NoteGridButton():juce::Button("Note grid"){
        setComponentID("piano-note-grid");setTitle("Snap grid");
        setWantsKeyboardFocus(true);setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    void setState(const NoteGridSpec& spec,bool snapEnabled){
        spec_=spec;snap_=snapEnabled;repaint();
        setTooltip((snap_?"Snap grid: ":"Snap off, grid ")+spec_.label()+" (click to choose)");
    }
    const NoteGridSpec& spec() const{return spec_;}
    bool snapEnabled() const{return snap_;}
    void paintButton(juce::Graphics& g,bool hover,bool down) override{
        const auto bounds=getLocalBounds().toFloat().reduced(.5f);
        g.setColour(juce::Colour(raised).brighter(down?.14f:hover?.07f:0.f));
        g.fillRoundedRectangle(bounds,cornerRadius);
        g.setColour(hasKeyboardFocus(true)?juce::Colour(accent):juce::Colour(outline));
        g.drawRoundedRectangle(bounds,cornerRadius,borderWidth);
        const auto iconArea=bounds.withTrimmedLeft(8.f).withWidth(15.f).withSizeKeepingCentre(15.f,15.f);
        if(auto* icon=gridIcon()){
            const auto ink=snap_?juce::Colour(0xffdae5f4):juce::Colour(0xff7d8896);
            icon->replaceColour(iconColour_,ink);iconColour_=ink;
            icon->drawWithin(g,iconArea,juce::RectanglePlacement::centred,1.f);
        }
        g.setColour(snap_?juce::Colour(primaryText):juce::Colour(secondaryText));
        g.setFont(Theme::channelFont());
        g.drawFittedText(spec_.label(),bounds.withTrimmedLeft(28.f).withTrimmedRight(15.f).toNearestInt(),
                         juce::Justification::centredLeft,1);
        juce::Path chevron;const auto x=bounds.getRight()-9.f,y=bounds.getCentreY();
        chevron.startNewSubPath(x-3.f,y-2.f);chevron.lineTo(x,y+1.f);chevron.lineTo(x+3.f,y-2.f);
        g.setColour(juce::Colour(secondaryText));g.strokePath(chevron,juce::PathStrokeType(1.f));
    }
private:
    juce::Drawable* gridIcon(){
        if(!icon_){
            int size{};
            if(const auto* bytes=MRConceptAssets::getNamedResource("grid_svg",size))
                icon_=juce::Drawable::createFromImageData(bytes,static_cast<std::size_t>(size));
        }
        return icon_.get();
    }
    NoteGridSpec spec_;bool snap_{true};
    juce::Colour iconColour_{0xffa9bfdc};
    std::unique_ptr<juce::Drawable> icon_;
};
// Seven-column note-grid palette; swing cells keep the note value and the
// percentage on separate lines. "Snap off" sits in its own footer row.
class NoteGridMenu final:public juce::Component {
public:
    NoteGridMenu(const NoteGridSpec& current,bool snapEnabled):current_(current),snapEnabled_(snapEnabled){
        setComponentID("note-grid-menu");setName("Note grid menu");setWantsKeyboardFocus(true);
        setSize(columns*cellWidth+padding*2,padding*2+rowCount()*cellHeight+footerGap+footerHeight);
    }
    std::function<void(const NoteGridSpec&,bool)> choose;
    void paint(juce::Graphics& g) override{
        drawPopupSurface(g,getWidth(),getHeight(),.97f,juce::Colour(surface),juce::Colour(menuBorder));
        const auto& choices=noteGridChoices();
        for(std::size_t i=0;i<choices.size();++i){
            const auto selected=snapEnabled_&&choices[i]==current_;
            paintCell(g,cellRect(i),choices[i],selected,static_cast<int>(i)==hovered_);
        }
        // Thin table dividers keep the value grid readable without per-cell chips.
        const auto line=juce::Colour(separatorColour).withAlpha(.65f);
        const int gridBottom=padding+rowCount()*cellHeight;
        for(int column=1;column<columns;++column){
            g.setColour(line);
            g.drawVerticalLine(padding+column*cellWidth,static_cast<float>(padding),static_cast<float>(gridBottom));
        }
        for(int row=1;row<rowCount();++row){
            g.setColour(line);
            g.drawHorizontalLine(padding+row*cellHeight,static_cast<float>(padding),static_cast<float>(padding+columns*cellWidth));
        }
        g.setColour(juce::Colour(separatorColour));
        g.fillRect(padding,footerTop()-footerGap/2,columns*cellWidth,1);
        paintFooter(g);
    }
    void mouseMove(const juce::MouseEvent& e) override{
        const auto next=hit(e.position);
        if(next!=hovered_){hovered_=next;repaint();}
        setMouseCursor(next>=0?juce::MouseCursor::PointingHandCursor:juce::MouseCursor::NormalCursor);
    }
    void mouseExit(const juce::MouseEvent&) override{if(hovered_>=0){hovered_=-1;repaint();}}
    void mouseDown(const juce::MouseEvent& e) override{
        if(!e.mods.isLeftButtonDown())return;
        activate(hit(e.position));
    }
    bool keyPressed(const juce::KeyPress& key) override{
        // Escape falls through to the call-out box, which closes the popup.
        if(key==juce::KeyPress::escapeKey)return false;
        if(key==juce::KeyPress::returnKey){activate(hovered_);return true;}
        return false;
    }
    void focusFirst(){
        const auto& choices=noteGridChoices();
        hovered_=0;
        if(snapEnabled_)for(std::size_t i=0;i<choices.size();++i)if(choices[i]==current_){hovered_=static_cast<int>(i);break;}
        grabKeyboardFocus();repaint();
    }
private:
    static constexpr int columns=7,cellWidth=56,cellHeight=44,padding=6,footerGap=6,footerHeight=30;
    int rowCount() const{return (static_cast<int>(noteGridChoices().size())+columns-1)/columns;}
    juce::Rectangle<int> cellRect(std::size_t index) const{
        const int row=static_cast<int>(index)/columns,column=static_cast<int>(index)%columns;
        return {padding+column*cellWidth,padding+row*cellHeight,cellWidth,cellHeight};
    }
    int footerTop() const{return padding+rowCount()*cellHeight+footerGap;}
    juce::Rectangle<int> footerRect() const{return {padding,footerTop(),columns*cellWidth,footerHeight};}
    int hit(juce::Point<float> point) const{
        if(footerRect().toFloat().contains(point))return static_cast<int>(noteGridChoices().size());
        const auto& choices=noteGridChoices();
        for(std::size_t i=0;i<choices.size();++i)if(cellRect(i).toFloat().contains(point))return static_cast<int>(i);
        return -1;
    }
    void activate(int index){
        if(index<0)return;
        const auto& choices=noteGridChoices();
        const auto offCell=index>=static_cast<int>(choices.size());
        if(choose)choose(offCell?current_:choices[static_cast<std::size_t>(index)],!offCell);
    }
    void paintCell(juce::Graphics& g,juce::Rectangle<int> bounds,const NoteGridSpec& spec,bool selected,bool hovered) const{
        const auto area=bounds.toFloat().reduced(2.f,1.5f);
        if(selected||hovered){
            g.setColour(juce::Colour(raised).withAlpha(selected ? .75f : hovered ? .45f : 0.f));
            g.fillRoundedRectangle(area,2.5f);
        }
        if(selected){
            g.setColour(juce::Colour(accent));
            g.drawRoundedRectangle(area.reduced(.75f),2.5f,1.5f);
        }
        const auto ink=selected?juce::Colour(primaryText):juce::Colour(0xffcdd6e0);
        const auto tail=spec.modifier();
        if(tail.isEmpty()){
            g.setColour(ink);g.setFont(Theme::channelFont().withHeight(12.f));
            g.drawText(spec.value(),bounds,juce::Justification::centred,false);
            return;
        }
        auto textArea=bounds.reduced(2,3);
        g.setColour(ink);g.setFont(Theme::channelFont().withHeight(12.f));
        g.drawText(spec.value(),textArea.removeFromTop(textArea.getHeight()*3/5),juce::Justification::centred,false);
        g.setColour(selected?juce::Colour(primaryText):juce::Colour(secondaryText));
        g.setFont(Theme::channelFont().withHeight(10.f));
        g.drawText(tail,textArea,juce::Justification::centred,false);
    }
    void paintFooter(juce::Graphics& g) const{
        const auto bounds=footerRect();
        const auto area=bounds.toFloat().reduced(1.f);
        const auto selected=!snapEnabled_;
        const auto hovered=hovered_>=static_cast<int>(noteGridChoices().size());
        if(selected||hovered){
            g.setColour(selected?juce::Colour(raised).brighter(.22f):juce::Colour(raised).brighter(.1f));
            g.fillRoundedRectangle(area,3.f);
        }
        if(selected){g.setColour(juce::Colour(accent));g.drawRoundedRectangle(area.reduced(.75f),3.f,1.5f);}
        g.setColour(selected?juce::Colour(primaryText):juce::Colour(secondaryText));
        g.setFont(Theme::channelFont().withHeight(12.f));
        g.drawText("Snap off",bounds,juce::Justification::centred,false);
    }
    NoteGridSpec current_;bool snapEnabled_{true};int hovered_{-1};
};
inline void showNoteGridMenu(juce::Component& anchor,const NoteGridSpec& current,bool snapEnabled,
                             std::function<void(const NoteGridSpec&,bool)> apply){
    auto* parent=anchor.getTopLevelComponent();
    if(!parent)return;
    auto content=std::make_unique<NoteGridMenu>(current,snapEnabled);
    auto* menu=content.get();
    auto& box=juce::CallOutBox::launchAsynchronously(std::move(content),
                                                     parent->getLocalArea(&anchor,anchor.getLocalBounds()),parent);
    box.setArrowSize(0.f);
    box.setDismissalMouseClicksAreAlwaysConsumed(true);
    juce::Component::SafePointer<juce::CallOutBox> popup(&box);
    menu->choose=[popup,apply=std::move(apply)](const NoteGridSpec& spec,bool snap){
        if(popup)popup->dismiss();
        if(!apply)return;
        // Apply on the message queue so the popup is already gone.
        juce::MessageManager::callAsync([apply,spec,snap]{apply(spec,snap);});
    };
    juce::Component::SafePointer<NoteGridMenu> safeMenu(menu);
    juce::MessageManager::callAsync([safeMenu]{if(safeMenu&&safeMenu->isShowing())safeMenu->focusFirst();});
}
}
