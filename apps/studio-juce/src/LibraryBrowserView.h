#pragma once
#include "Controls.h"
#include <mrs/sample_library.hpp>
#include <mrs/sample_preview.hpp>
#include <future>
#include <optional>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace ui {

// #254 WAV-only local catalogue UI. No transport/preview/Arrange changes (#255).
class LibraryResultsList final:public juce::ListBox {
public:
    using juce::ListBox::ListBox;
    std::function<void()> startLibraryDrag;
    void mouseDown(const juce::MouseEvent& e) override {dragStarted=false;juce::ListBox::mouseDown(e);}
    void mouseDrag(const juce::MouseEvent& e) override {
        juce::ListBox::mouseDrag(e);
        if(!dragStarted&&e.mods.isLeftButtonDown()&&e.getDistanceFromDragStart()>7&&startLibraryDrag){
            dragStarted=true;startLibraryDrag();
        }
    }
private:
    bool dragStarted{};
};
class LibraryBrowserView final : public juce::Component,
                                 private juce::ListBoxModel,
                                 private juce::Timer {
public:
    explicit LibraryBrowserView(mrs::library::SampleLibrary&);
    ~LibraryBrowserView() override;
    void setSnapshot(std::shared_ptr<const mrs::library::Snapshot>);
    juce::StringArray selectedWavFiles() const;
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
    void selectedRowsChanged(int) override;
    void timerCallback() override;
    void prepareSelected();
    void startLibraryDrag();

    mrs::library::SampleLibrary& library;
    std::shared_ptr<const mrs::library::Snapshot> view;
    mrs::library::LibraryQuery query;
    Section section{Section::folders};
    bool gridView{};
    int gridSelected{-1};
    std::uint64_t revision{};
    std::vector<std::int64_t> sourceIds,collectionIds;
    std::vector<std::string> folderNames,facetGroups,facetLabels;


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
    LibraryResultsList results{"MR Library results",this};
    juce::Label previewName,previewMeta;
    std::optional<mrs::library::WavPreview> waveform;
    juce::File wantedPreview;
    juce::File pendingPreview;
    std::shared_ptr<std::atomic<bool>> waveformCancel;
    std::future<mrs::library::WavPreview> waveformWorker;
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
