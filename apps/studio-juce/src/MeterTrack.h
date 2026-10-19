#pragma once
#include "Controls.h"
#include <mrs/core.hpp>
namespace ui {
class Desktop;class Arrangement;
class MeterTrack final:public juce::Component {
public:
    MeterTrack(Desktop&,Arrangement&);
    void paint(juce::Graphics&)override;void resized()override;void sync(bool replaced=false);
    void mouseDown(const juce::MouseEvent&)override;void mouseDrag(const juce::MouseEvent&)override;void mouseUp(const juce::MouseEvent&)override;
    void mouseDoubleClick(const juce::MouseEvent&)override;bool keyPressed(const juce::KeyPress&)override;
    void focusLost(FocusChangeType)override{cancel();}
    void edit(std::optional<mrs::MeterPoint>,std::int64_t bar=1,std::optional<juce::Point<int>> screen={});
    void clearSelection(){selected.reset();cancel();}bool hasSelection()const{return selected.has_value();}
    void removeSelected();void cancel();bool dragging()const{return original.has_value();}
    float xAt(std::int64_t)const;std::int64_t barAt(float)const;
    juce::Component* popupEditor()const{return editor.getComponent();}
private:
    void selectOnly();std::optional<mrs::MeterPoint> pointAt(float)const;
    Desktop& owner;Arrangement& arrangement;juce::ComboBox list;juce::TextButton add{"+"};
    std::optional<std::int64_t> selected;std::optional<mrs::MeterPoint> original,preview;
    mrs::TimeMap captured,listed;std::shared_ptr<mrs::IProjectStore> store;double zoom{},scroll{};float dragX{};std::int64_t anchor{};
    juce::Component::SafePointer<juce::Component> editor;
};
void meter84Smoke();
}
