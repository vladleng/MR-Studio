#pragma once
#include "Controls.h"
#include <mrs/sample_library.hpp>
#include <functional>

namespace ui {
// #252: source-management shell only. Tag/search/artwork UI belongs to #253/#254.
class LibraryFoundationPanel final : public juce::Component, private juce::ListBoxModel, private juce::Timer {
public:
    LibraryFoundationPanel();
    ~LibraryFoundationPanel() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void refresh();
private:
    int getNumRows() override;
    void paintListBoxItem(int row,juce::Graphics&,int width,int height,bool selected) override;
    void listBoxItemClicked(int row,const juce::MouseEvent&) override;
    void timerCallback() override;
    void chooseFolder(std::function<void(const juce::File&)>);
    mrs::library::SampleLibrary library;
    std::shared_ptr<const mrs::library::Snapshot> view;
    std::uint64_t lastRevision{};
    juce::TextButton addFolder{"Add Folder"},rescan{"Rescan"},cancel{"Cancel"};
    juce::TextButton relink{"Relink"},toggle{"Enable / Disable"},remove{"Remove"};
    juce::ListBox sources{"MR Library sources",this};
    juce::Label title,status;
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace ui
