#pragma once
#include "Controls.h"
#include <mrs/sample_library.hpp>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace ui {

// #254 WAV-only local catalogue UI. No transport/preview/Arrange changes (#255).
class LibraryBrowserView final : public juce::Component,
                                 private juce::ListBoxModel,
                                 private juce::Timer {
public:
    explicit LibraryBrowserView(mrs::library::SampleLibrary&);
    ~LibraryBrowserView() override;
    void setSnapshot(std::shared_ptr<const mrs::library::Snapshot>);
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    enum class Section { folders, tags, collections };
    void chooseSection(Section);
    void sendQuery();
    void refreshControls();
    void updateVisibility();
    void updateFacets();
    void chooseArtwork(bool clear);
    std::vector<std::int64_t> selectedAssets() const;
    juce::Image thumbnail(const mrs::library::AudioAsset&);
    int getNumRows() override;
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override;
    void listBoxItemClicked(int,const juce::MouseEvent&) override;
    void timerCallback() override;

    mrs::library::SampleLibrary& library;
    std::shared_ptr<const mrs::library::Snapshot> view;
    mrs::library::LibraryQuery query;
    Section section{Section::folders};
    bool gridView{};
    int gridSelected{-1};
    std::uint64_t revision{};

    juce::TextButton foldersTab{"Folders"},tagsTab{"Tags"},collectionsTab{"Collections"};
    juce::TextButton favorites{"Favorites"},clearFilters{"Clear"},listGrid{"Grid"};
    juce::TextButton addFilter{"Add filter"},removeFilter{"Remove filter"};
    juce::TextButton newCollection{"Create"},deleteCollection{"Delete"};
    juce::TextButton addToCollection{"Add selected"},removeFromCollection{"Remove selected"};
    juce::TextButton favoriteSelected{"Star selected"};
    juce::TextButton chooseCover{"Set cover"},resetCover{"Auto cover"};
    juce::TextButton prevPage{"Prev"},nextPage{"Next"};
    juce::ComboBox sources,folders,tagGroups,tagValues,selectedFilters,collections,sorts;
    juce::TextEditor search,bpmFrom,bpmTo,keyField,collectionName;
    juce::Label counter,filterLabel;
    juce::ListBox results{"MR Library results",this};
    std::unique_ptr<juce::FileChooser> imageChooser;

    // JPEG/PNG I/O and image decoding are deliberately outside GUI/audio threads.
    std::jthread imageWorker;
    std::mutex imageMutex;
    std::condition_variable_any imageWake;
    std::deque<std::pair<std::string,juce::File>> pendingImages;
    std::set<std::string> requestedImages;
    std::map<std::string,juce::Image> imageCache;
    std::atomic<std::uint64_t> readyImages{0};
    std::uint64_t paintedImages{};
};
} // namespace ui
