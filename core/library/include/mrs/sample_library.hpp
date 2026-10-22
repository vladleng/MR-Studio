#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <mrs/audio_features.hpp>

namespace mrs::library {

// Non-RT bounded JPG/PNG header validation; caps decoded dimensions.
bool artwork_image_supported(const std::filesystem::path&);

enum class Phase { starting, idle, scanning, cancelled, error };

struct Source {
    std::int64_t id{};
    std::filesystem::path root;
    bool enabled{true};
    std::uint64_t available{}, missing{}, invalid{};
};

struct AssetTag {
    std::string group,label,origin,status; // proposed / confirmed / rejected
    double confidence{};
};
struct AudioAsset {
    std::int64_t id{},source_id{};
    std::string relative_path,status;
    std::vector<AssetTag> tags;
    int detected_bpm{},manual_bpm{}; // 0 = unknown; -1 manual clears auto value
    std::string detected_key,manual_key; // "-" manual clears auto value
    std::string artwork_path; // relative path, source-owned
    std::int64_t sample_rate{}, frames{}, modified{};
    bool favorite{};
    AudioFeatures analysis;
    std::string bpm_source,key_source;
};
struct LibraryFolder {
    std::int64_t source_id{};
    std::string relative_path; // empty = source root, '/' path separators
    std::string artwork_path; // relative to same source root; empty = no cover
};
struct LibraryCollection {
    std::int64_t id{};
    std::string name;
    std::uint64_t count{};
};
struct LibraryFacet {
    std::string group,label;
    std::uint64_t count{};
};
struct LibraryTagFilter {
    bool operator==(const LibraryTagFilter&) const = default;
    std::string group,label;
};
struct LibraryCover {std::string kind;std::int64_t owner{};std::string key,image;};
struct LibraryQuery {
    bool operator==(const LibraryQuery&) const = default;
    std::int64_t source_id{}; // 0 = all sources
    std::int64_t collection_id{}; // 0 = all
    std::string folder; // exact folder + descendants within source
    std::string search; // name or tag, Unicode-aware
    std::string sort{"name"}; // name, relevant, bpm, duration, updated, added, random
    std::string key; // empty = all, '?' = unknown
    int bpm_min{},bpm_max{}; // 0 = no bound
    std::vector<LibraryTagFilter> tags; // AND across groups, OR within group
    std::vector<LibraryTagFilter> excluded_tags;
    bool favorites_only{};
    std::uint64_t shuffle_seed{1};
    std::uint64_t offset{};
    std::uint64_t limit{120}; // bounded page
    std::int64_t similar_asset_id{};
};
struct SavedSearch {std::int64_t id{};std::string name;LibraryQuery query;};
struct AnalysisProgress {std::uint64_t completed{},total{};bool running{},paused{};};
struct Snapshot {
    Phase phase{Phase::starting};
    std::string message;
    std::uint64_t processed{}, changed{}, revision{};
    std::vector<Source> sources;
    std::vector<AudioAsset> assets; // bounded page, generated off realtime thread
    std::uint64_t visible_assets{};
    LibraryQuery query;
    std::vector<LibraryFolder> folders;
    std::vector<LibraryCollection> collections;
    std::filesystem::path artwork_root;
    std::vector<LibraryCover> covers;
    std::vector<LibraryFacet> facets;
    std::vector<LibraryFacet> all_facets;
    AnalysisProgress analysis;
    std::vector<SavedSearch> saved_searches;
    AudioFeatures similar_reference;
};

// Local catalogue of existing WAV files. All I/O, scanning and SQLite writes
// are performed off realtime. A catalogue worker owns SQLite; a dedicated bounded
// analysis worker reads audio without blocking browse commands. Originals are untouched.
// A snapshot is immutable, safe to copy on the JUCE message thread.
// MIDI, artwork and sample playback are separate future stages. Tags are #253.
class SampleLibrary final {
public:
    explicit SampleLibrary(std::filesystem::path database);
    ~SampleLibrary();
    SampleLibrary(const SampleLibrary&) = delete;
    SampleLibrary& operator=(const SampleLibrary&) = delete;

    void add_source(std::filesystem::path);
    void relink_source(std::int64_t source_id, std::filesystem::path);
    void remove_source(std::int64_t source_id); // Removes database references only
    void enable_source(std::int64_t source_id, bool enabled);
    void set_view(std::int64_t source_id, std::string name_filter = {}); // compatibility with #253 editor
    void browse(LibraryQuery query);
    void create_collection(std::string name);
    void delete_collection(std::int64_t id);
    void set_collection_items(std::int64_t collection_id, std::vector<std::int64_t> asset_ids, bool add);
    void set_favorites(std::vector<std::int64_t> asset_ids, bool enabled);
    void set_artwork(std::int64_t source_id,std::string folder,std::filesystem::path image); // source-local override; empty image resets
    void set_cover(std::string kind,std::int64_t owner,std::string key,std::filesystem::path image);
    void add_tag(const std::vector<std::int64_t>& assets,std::string group,std::string label);
    void set_tag_status(const std::vector<std::int64_t>& assets,std::string group,std::string label,std::string status);
    void set_bpm(const std::vector<std::int64_t>& assets,int bpm); // 0 clears detection, -1 removes override
    void set_key(const std::vector<std::int64_t>& assets,std::string key); // '-' clears, empty removes override
    void add_rule(std::string group,std::string phrase,std::string label,bool folder);
    void undo_tag_edit(); // library-only undo; project Undo not touched
    void rescan(); // Incremental, all enabled sources
    void cancel_scan();
    void analyse_audio(bool force=false);
    void pause_analysis(bool paused);
    void cancel_analysis();
    void save_search(std::string name,LibraryQuery);
    void delete_search(std::int64_t id);
    std::shared_ptr<const Snapshot> snapshot() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mrs::library
