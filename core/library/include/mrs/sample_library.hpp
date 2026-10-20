#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace mrs::library {

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
};
struct Snapshot {
    Phase phase{Phase::starting};
    std::string message;
    std::uint64_t processed{}, changed{}, revision{};
    std::vector<Source> sources;
    std::vector<AudioAsset> assets; // bounded worker-side page for the tag editor
    std::uint64_t visible_assets{};
};

// Local catalogue of existing WAV files. All I/O, scanning and SQLite writes
// are performed by a private non-realtime worker. No original files are touched.
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
    void set_view(std::int64_t source_id, std::string name_filter = {}); // 0 = all; at most 300 entries
    void add_tag(const std::vector<std::int64_t>& assets,std::string group,std::string label);
    void set_tag_status(const std::vector<std::int64_t>& assets,std::string group,std::string label,std::string status);
    void set_bpm(const std::vector<std::int64_t>& assets,int bpm); // 0 clears detection, -1 removes override
    void set_key(const std::vector<std::int64_t>& assets,std::string key); // '-' clears, empty removes override
    void add_rule(std::string group,std::string phrase,std::string label,bool folder);
    void undo_tag_edit(); // library-only undo; project Undo not touched
    void rescan(); // Incremental, all enabled sources
    void cancel_scan();
    std::shared_ptr<const Snapshot> snapshot() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mrs::library
