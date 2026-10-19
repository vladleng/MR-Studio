#pragma once
#include "Controls.h"
#include "MusicalSelection.h"
#include <mrs/core.hpp>
namespace ui {
class Desktop;
class Arrangement;
class MarkerTrack final : public juce::Component {
public:
    MarkerTrack(Desktop&,Arrangement&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void sync(bool replaced=false);
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusLost(FocusChangeType) override;
    void edit(std::optional<mrs::Marker>,mrs::Tick);
    void rename(const mrs::Marker&);
    InlineNameEditor* renameEditor()const{return nameEditor.get();}
    void cancel();
    bool dragging() const { return original.has_value()||group.selecting; }
    bool hasSelection() const { return !group.ids.empty(); }
    void clearSelection() { cancel();selected.reset();group.ids.clear(); }
    void removeSelected();
    void duplicateSelected();
    bool copyDrag{};
    float xAt(mrs::Tick) const;
    std::vector<mrs::Marker> markersAt(float) const;
    std::size_t selectionCount()const{return group.ids.size();}
private:
    MusicalSelection<mrs::Marker> group;
    void selectGroup(const std::vector<mrs::Id>& ids);

    friend void markerTrackSmoke();
    void select(mrs::Id);
    void menu(float,std::optional<juce::Point<int>> screen={});
    juce::Component::SafePointer<juce::Component> paletteEditor;
    std::unique_ptr<InlineNameEditor> nameEditor;
    Desktop& owner;Arrangement& arrangement;
    juce::ComboBox list;juce::TextButton add{"+"};
    std::vector<mrs::Marker> listed;
    std::optional<mrs::Id> selected;
    std::optional<mrs::Marker> original,preview;
    std::shared_ptr<mrs::IProjectStore> gestureStore;
    mrs::TimeMap gestureTime;
    double gestureZoom{},gestureScroll{};
    mrs::Tick anchor{};float dragX{};
};
void markerTrackSmoke();
}
