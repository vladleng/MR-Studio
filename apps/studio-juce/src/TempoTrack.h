#pragma once
#include "Controls.h"
#include <mrs/core.hpp>
namespace ui {
class Desktop;class Arrangement;
class TempoTrack final:public juce::Component {
public:
    TempoTrack(Desktop&,Arrangement&);
    void paint(juce::Graphics&)override;void resized()override;void sync(bool replaced=false);
    void mouseDown(const juce::MouseEvent&)override;void mouseDrag(const juce::MouseEvent&)override;void mouseUp(const juce::MouseEvent&)override;
    void mouseDoubleClick(const juce::MouseEvent&)override;bool keyPressed(const juce::KeyPress&)override;
    void focusLost(FocusChangeType)override{cancel();}
    void edit(std::optional<mrs::TempoPoint>,mrs::Tick=0,std::optional<juce::Point<int>> screen={});
    void clearSelection(){selected.reset();cancel();}
    bool hasSelection()const{return selected.has_value();}
    void removeSelected();
    void cancel();bool dragging()const{return original.has_value();}
    float xAt(mrs::Tick)const;juce::Component* popupEditor()const{return editor.getComponent();}
private:
    std::optional<mrs::TempoPoint> pointAt(float)const;
    Desktop& owner;Arrangement& arrangement;juce::ComboBox list;juce::TextButton add{"+"};
    std::optional<mrs::Tick> selected;std::optional<mrs::TempoPoint> original,preview;
    mrs::TimeMap captured,listed;std::shared_ptr<mrs::IProjectStore> store;double zoom{},scroll{};float dragX{};mrs::Tick anchor{};
    juce::Component::SafePointer<juce::Component> editor;
};
void tempo83Smoke();
}
