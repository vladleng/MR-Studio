#pragma once
#include "Controls.h"
#include "MusicalSelection.h"
#include <mrs/core.hpp>

namespace ui {
class Desktop;
class Arrangement;
// Message-thread view/controller; only the optional gesture preview is writable
// here. Durable chords and history belong to the shared ProjectStore.
class ChordTrack final : public juce::Component {
public:
    ChordTrack(Desktop&, Arrangement&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void sync(bool projectReplaced = false);
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusLost(FocusChangeType) override;
    juce::Rectangle<float> chordRect(const mrs::Chord&) const;
    mrs::Tick tickAt(float, bool snapped = true) const;
    std::optional<mrs::Chord> chordAt(juce::Point<float>) const;
    std::optional<mrs::Id> currentChord() const;
    void edit(std::optional<mrs::Chord>, mrs::Tick);
    void cancel();
    bool hasSelection() const { return !group.ids.empty(); }
    void clearSelection() { cancel();selected.reset();group.ids.clear(); }
    void duplicateSelected();
    void splitSelected(mrs::Tick);
    void removeSelected();
    bool dragging() const { return original.has_value()||group.selecting; }
    std::size_t selectionCount()const{return group.ids.size();}
private:
    MusicalSelection<mrs::Chord> group;
    juce::Component::SafePointer<juce::Component> paletteEditor;
public:
    juce::Component* colorMenu()const{return paletteEditor.getComponent();}
private:
    void selectGroup(const std::vector<mrs::Id>& ids);

    friend void chordTrackSmoke();
    void menu(juce::Point<float>);
    void create(mrs::Tick);
    int edgeAt(const mrs::Chord&, float) const;
    Desktop& owner;
    Arrangement& arrangement;
    juce::TextButton add{"+"};
    std::optional<mrs::Id> selected;
    std::optional<mrs::Chord> original, preview;
    std::shared_ptr<mrs::IProjectStore> gestureStore;
    mrs::TimeMap gestureTime;
    double gestureZoom{},gestureScroll{};
    mrs::Tick anchor{};
    float dragX{};
    int edge{};
    bool copyDrag{};
};
void chordTrackSmoke();
} // namespace ui
