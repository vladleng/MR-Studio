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

struct Snapshot {
    Phase phase{Phase::starting};
    std::string message;
    std::uint64_t processed{}, changed{}, revision{};
    std::vector<Source> sources;
};

// Local catalogue of existing WAV files. All I/O, scanning and SQLite writes
// are performed by a private non-realtime worker. No original files are touched.
// A snapshot is immutable, safe to copy on the JUCE message thread.
// MIDI, tagging, artwork and sample playback are separate future stages.
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
    void rescan(); // Incremental, all enabled sources
    void cancel_scan();
    std::shared_ptr<const Snapshot> snapshot() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mrs::library
