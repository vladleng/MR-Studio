#pragma once
#include <mrs/reverb_file.hpp>
#include <memory>
#include <string>
#include <functional>

namespace mrs::audio {
enum class ReverbFileKind { mono, stereo, quad, pair, unsupported };
enum class ReverbLibraryPhase { empty, scanning, ready, missing, error };
struct ReverbLibraryEntry {
    std::string name, folder, key, error; // UTF-8, relative display/key; never a brand heuristic
    std::filesystem::path first, second;
    ReverbFileKind kind{ReverbFileKind::unsupported};
    std::uint32_t sample_rate{},channels{};
    std::uint64_t frames{};
    double duration() const noexcept {return sample_rate?static_cast<double>(frames)/sample_rate:0.;}
};
struct ReverbLibraryIndex {
    std::filesystem::path root;
    std::vector<ReverbLibraryEntry> entries;
    std::string message;
    ReverbLibraryPhase phase{ReverbLibraryPhase::empty};
    std::uint64_t revision{};
};
// Shared app-level catalogue; one worker, immutable snapshots, last request wins.
// All methods/destruction NON-RT. Does not write/copy library files or preferences.
class ReverbLibrary {
public:
    ReverbLibrary();
    ~ReverbLibrary();
    void set_root(std::filesystem::path); // async; cancels obsolete scan
    std::shared_ptr<const ReverbLibraryIndex> snapshot() const;
    bool commit_if_current(std::uint64_t,const std::function<bool()>&); // bounded NON-RT publication only
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
ReverbLibraryIndex index_reverb_library(const std::filesystem::path&,std::stop_token = {});

struct ReverbFileIdentity {
    std::filesystem::path relative, absolute;
    std::string digest; // raw-byte FNV-1a-64 fingerprint, deterministic, not a security hash
    std::uint64_t bytes{};
};
struct ReverbSelection {
    ReverbLibraryEntry entry;
    ReverbFileIdentity first,second;
    processing::ReverbQuadMap map{processing::ReverbQuadMap::unspecified};
};
ReverbState reverb_selection_state(const ReverbSelection&,const ReverbState& controls = {});
enum class ReverbLoadPhase { empty, loading, ready, missing, error };
struct ReverbLoadStatus {
    ReverbLoadPhase phase{ReverbLoadPhase::empty};
    std::string message;
    std::optional<ReverbSelection> active; // previous good selection survives errors/cancellation
    std::string requested_key;
    std::uint64_t revision{};
    std::uint64_t active_revision{}; // Published kernel generation, independent of a newer pending request.
};
// Per-instance worker/control producer, owns a graph lease until it joins.
// Owner must cancel/join this service BEFORE re-prepare/capture/graph replacement.
// ready means complete validated kernel published; DSP acknowledges crossfade separately.
class ReverbLoader {
public:
    ReverbLoader(std::shared_ptr<processing::PreparedGraph>,Id,processing::ProcessConfig,std::shared_ptr<ReverbLibrary> = {});
    ~ReverbLoader();
    void shutdown(); // cancel + join, even when an editor still holds a shared view
    void activate_quiescent(); // callbacks STOPPED; serializes warm with worker reclamation
    void restore(const ReverbState&,const std::filesystem::path& root); // verify relative, then absolute fallback on worker
    void select(const ReverbLibraryEntry&,const std::filesystem::path& root,
                processing::ReverbQuadMap = processing::ReverbQuadMap::unspecified);
    void cancel(); // preserves active identity/kernel; suppresses stale publication
    std::shared_ptr<const ReverbLoadStatus> snapshot() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
} // namespace mrs::audio
