#pragma once
#include <tuple>
#include "Controls.h"
#include <mrs/sample_library.hpp>
#include <functional>

namespace ui {
class LibraryBrowserView;
// MR Library #253: source management and a bounded manual WAV tag editor.
// Full collection/folder/artwork UI belongs to #254.
class LibraryFoundationPanel final : public juce::Component, private juce::ListBoxModel, private juce::Timer {
public:
    LibraryFoundationPanel(std::function<bool(const std::filesystem::path&,double)> play,
        std::function<void()> stop,
        std::function<std::tuple<bool,float,bool>()> status);
    ~LibraryFoundationPanel() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    void refresh();
    juce::StringArray selectedLibrarySamples() const;
private:
    int getNumRows() override;
    void paintListBoxItem(int row,juce::Graphics&,int width,int height,bool selected) override;
    void listBoxItemClicked(int row,const juce::MouseEvent&) override;
    void timerCallback() override;
    void chooseFolder(std::function<void(const juce::File&)>);
    struct AssetRows;
    std::vector<std::int64_t> selectedAssetIds() const;
    void fillSuggestedTags();
    void applyTag(const std::string& status);
    void updateMode();
    mrs::library::SampleLibrary library;
    std::shared_ptr<const mrs::library::Snapshot> view;
    std::uint64_t lastRevision{};
    juce::TextButton addFolder{"Add Folder"},rescan{"Rescan"},cancel{"Cancel"};
    juce::TextButton relink{"Relink"},toggle{"Enable / Disable"},remove{"Remove"};
    juce::ListBox sources{"MR Library sources",this};
    juce::Label title,status,tagInfo;
    std::unique_ptr<AssetRows> assetRows;
    juce::ListBox assets{"Indexed WAV tags",nullptr};
    juce::TextButton modeButton{"Edit Tags"},sourcesMode{"Sources"};
    std::unique_ptr<LibraryBrowserView> browserView;
    bool browsing{true};
    juce::TextButton addTag{"Add Tag"},confirmTag{"Confirm"},rejectTag{"Reject"},undoTag{"Undo Edit"};
    juce::TextButton setBpm{"Set BPM"},setKey{"Set Key"},addRule{"Add Rule"};
    juce::TextEditor assetSearch,tagLabel,bpmValue,keyValue,rulePhrase;
    juce::ComboBox tagGroup,suggestedTags;
    juce::ToggleButton ruleFolder{"Folder"};
    bool editingTags{};

    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace ui
