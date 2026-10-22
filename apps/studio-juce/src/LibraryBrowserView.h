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
void libraryBrowserSmoke();

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
    void mouseMove(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent& e) override {mouseMove(e);}
    void mouseExit(const juce::MouseEvent&) override;
private:
    friend void libraryBrowserSmoke();
    enum class Section { folders, tags, collections };
    void chooseSection(Section);
    void sendQuery();
    void refreshControls();
    void rebuildCards();
    void updateVisibility();
    juce::PopupMenu popupMenu();
    juce::PopupMenu::Options popupOptions(juce::Component*,std::optional<juce::Point<int>> point={}) const;
    void filterMenu(const std::string&,juce::Component*);
    void musicalMenu(bool bpm,juce::Component*);
    void toggleTag(std::string group,std::string label,bool exclude=false);
    void rowMenu(std::size_t,juce::Point<int>);
    void optionsMenu();
    void prompt(juce::String title,juce::String initial,std::function<void(juce::String)>);
    struct Card {std::string kind,label,key,group,cover;std::int64_t owner{};std::uint64_t count{};std::int64_t artSource{};};
    void openCard(const Card&);
    void coverMenu(Card,juce::Component*,std::optional<juce::Point<int>> point={});
    void chooseArtwork(Card,bool clear);
    std::string coverFor(const Card&) const;
    int columns() const;
    bool largeCards() const;
    int cardColumns() const;
    int cardRowSpan() const;
    int cardIndex(int row,int x) const;
    int cardRows() const;
    int assetIndex(int row,int x) const;
    juce::Image cachedImage(const juce::File&);
    std::vector<Card> cards;
    std::string tagPath;
    bool rootMode{true},pendingQuery{};
    std::map<std::string,std::unique_ptr<juce::Drawable>> instrumentArt;
    std::vector<std::int64_t> selectedAssets() const;
    std::string sampleCover(const mrs::library::AudioAsset&) const;
    juce::Image thumbnail(const mrs::library::AudioAsset&);
    int getNumRows() override;
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override;
    void listBoxItemClicked(int,const juce::MouseEvent&) override;
    void selectedRowsChanged(int) override;
    juce::String getTooltipForRow(int) override;
    void timerCallback() override;
    struct RowWave {
        mrs::library::WavPreview wav;
        std::string error;
        std::uint64_t lastUse{};
    };
    std::optional<RowWave> rowWave(const mrs::library::AudioAsset&);
    std::string waveKey(const mrs::library::AudioAsset&) const;
    juce::var getDragSourceDescription(const juce::SparseSet<int>&) override;
    void audition(std::size_t assetIndex,double position,bool toggle);
    void stepPreview(int delta);
    bool browserKey(const juce::KeyPress&);
    void updateDetails();
    bool keyboardAudition{};
    int hoveredAsset{-1};
    struct RowLayout {juce::Rectangle<int> cover,control,title,tags,instrument,wave;};
    RowLayout rowLayout(int width,int height) const;
    int pageNameWidth{};
    std::string rowGlyph(const mrs::library::AudioAsset&,bool focused,bool hovered) const;
    juce::ScaledImage dragImage(int& x,int& y);

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
    juce::TextButton foldersTab{"Folders"},tagsTab{"Tags"},collectionsTab{"Collections"};
    juce::TextButton favorites{"Likes"},clearFilters{"Clear"},listGrid{"Grid"},back{"Back"},options{"..."};
    juce::TextButton instruments{"Instruments"},genres{"Genres"},packButton{"Pack"},keyButton{"Key"},bpmButton{"BPM"},typeButton{"Type"},activeFilters{"Filters"};
    std::vector<std::unique_ptr<juce::TextButton>> chips;
    juce::TextButton prevPage{"<"},nextPage{">"},stopPreview{"Stop"};
    juce::TextButton previousSound{"< Section"},playPause{"Play"},nextSound{"Section >"};
    juce::ComboBox sorts;
    juce::TextEditor search;
    juce::Label counter,breadcrumb,details;
    struct Results final : juce::ListBox {
        Results(LibraryBrowserView& o):juce::ListBox("MR Library results",&o),owner(o){}
        bool keyPressed(const juce::KeyPress& k) override {return owner.browserKey(k)||juce::ListBox::keyPressed(k);}
        bool keyStateChanged(bool down) override {
            return juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey)||juce::ListBox::keyStateChanged(down);
        }
        juce::ScaledImage createSnapshotOfRows(const juce::SparseSet<int>&,int& x,int& y) override {return owner.dragImage(x,y);}
        LibraryBrowserView& owner;
    };
    Results results{*this};
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
    std::uint64_t waveUse{}; // protected by waveMutex; recency, never key order
    static constexpr std::size_t waveCapacity=384; // entire maximum 300-asset page + queued work
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
