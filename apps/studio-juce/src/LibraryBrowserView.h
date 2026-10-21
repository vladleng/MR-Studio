#pragma once
#include <tuple>
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

// WAV-only catalogue with inline audition through the existing audio engine.
class LibraryBrowserView final : public juce::Component,
                                 private juce::ListBoxModel,
                                 private juce::Timer {
public:
    LibraryBrowserView(mrs::library::SampleLibrary&,
        std::function<bool(const std::filesystem::path&,double)> play,
        std::function<void()> stop,
        std::function<std::tuple<bool,float,bool>()> position);
    ~LibraryBrowserView() override;
    void setSnapshot(std::shared_ptr<const mrs::library::Snapshot>);
    void invalidatePreview();
    juce::StringArray selectedWavFiles() const;
    void resized() override;
    void paint(juce::Graphics&) override;
    void visibilityChanged() override;
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
    struct RowWave {
        mrs::library::WavPreview wav;
        std::string error;
    };
    std::optional<RowWave> rowWave(const mrs::library::AudioAsset&);
    std::string waveKey(const mrs::library::AudioAsset&) const;
    juce::var getDragSourceDescription(const juce::SparseSet<int>&) override;
    void audition(std::size_t assetIndex,double position,bool toggle);

    mrs::library::SampleLibrary& library;
    std::function<bool(const std::filesystem::path&,double)> previewPlay;
    std::function<void()> previewStop;
    std::function<std::tuple<bool,float,bool>()> previewState;
    std::int64_t activePreviewId{};
    std::string activePreviewKey;
    std::vector<std::pair<std::int64_t,std::filesystem::path>> dragPayload;
    bool paintedPlaying{};
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
    juce::TextButton prevPage{"Prev"},nextPage{"Next"},stopPreview{"Stop"};
    juce::ComboBox sources,folders,tagGroups,tagValues,selectedFilters,collections,sorts;
    juce::TextEditor search,bpmFrom,bpmTo,keyField,collectionName;
    juce::Label counter,filterLabel;
    juce::ListBox results{"MR Library results",this};
    // Waveforms are generated lazily for painted rows only; bounded cache.
    // Never decode WAVs on the GUI thread or audio callback.
    std::jthread waveWorker;
    std::mutex waveMutex;
    std::condition_variable_any waveWake;
    struct WaveTask {
        std::string key;
        std::shared_ptr<const mrs::library::Snapshot> catalogue;
        std::int64_t assetId{};
        std::shared_ptr<std::atomic<bool>> cancel;
    };
    std::shared_ptr<std::atomic<bool>> waveCancel=std::make_shared<std::atomic<bool>>(false);
    std::deque<WaveTask> waveQueue;
    std::set<std::string> waveRequested;
    std::map<std::string,RowWave> waveCache;
    std::atomic<std::uint64_t> wavesReady{0};
    std::uint64_t wavesPainted{};
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
