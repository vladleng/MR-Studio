#pragma once
#include "Controls.h"
#include <mrs/core.hpp>
#include <set>
namespace ui {
class Desktop;class Arrangement;
class SectionTrack final:public juce::Component {
public:
    SectionTrack(Desktop&,Arrangement&);
    void paint(juce::Graphics&) override;void resized() override;void sync(bool replaced=false);
    void mouseDown(const juce::MouseEvent&) override;void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;void mouseMove(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;bool keyPressed(const juce::KeyPress&) override;
    void focusLost(FocusChangeType) override {cancel();}
    void edit(std::optional<mrs::ArrangerSection>,mrs::Tick start=0,std::optional<juce::Point<int>> screen={});
    void clearSelection(){chosen.clear();selected.reset();cancel();}
    void removeSelected();
    void duplicateSelected();
    void splitSelected(mrs::Tick);
    bool hasSelection()const{return !chosen.empty();}
    std::size_t selectionCount()const{return chosen.size();}
    juce::Component* popupEditor()const{return editor.getComponent();}
    void rename(const mrs::ArrangerSection&);
    InlineNameEditor* renameEditor()const{return nameEditor.get();}
    void cancel();bool dragging()const{return original.has_value()||selecting;}
    juce::Rectangle<float> sectionRect(const mrs::ArrangerSection&)const;
    std::optional<mrs::ArrangerSection> sectionAt(juce::Point<float>)const;
private:
    int edgeAt(const mrs::ArrangerSection&,float)const;
    std::unique_ptr<InlineNameEditor> nameEditor;
    Desktop& owner;Arrangement& arrangement;juce::TextButton add{"+"};
    std::optional<mrs::Id> selected;std::optional<mrs::ArrangerSection> original,preview;
    std::shared_ptr<mrs::IProjectStore> store;mrs::TimeMap time;double zoom{},scroll{};
    std::set<std::string> chosen,selectionBase;
    std::vector<mrs::ArrangerSection> originals,previews;
    bool copyDrag{},previewValid{true},selecting{};
    juce::Point<float> selectionOrigin;juce::Rectangle<float> selectionBox;
    std::vector<mrs::ArrangerSection> selectedSections()const;
    void selectOne(mrs::Id);
    float dragX{};mrs::Tick anchor{};int edge{};
    juce::Component::SafePointer<juce::Component> editor;
};
void section81Smoke();
}
