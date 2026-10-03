#pragma once
#include <mrs/processing.hpp>
#include <filesystem>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace mrs::persistence {
inline constexpr std::uint32_t archive_version = 1;
inline constexpr std::size_t max_archive_bytes = 64 * 1024 * 1024;
struct Extension {
    std::string tag; // exactly four ASCII characters; unrecognized chunks survive save
    std::string bytes;
    bool operator==(const Extension&) const = default;
};
struct MixerChannel {
    Id track;
    float gain{1}, pan{};
    bool mute{}, solo{};
    bool operator==(const MixerChannel&) const = default;
};
struct MidiBinding {
    Id port; // stable logical port; physical device may be unavailable
    std::string device_key, name;
    bool input{}, output{};
    bool operator==(const MidiBinding&) const = default;
};
struct LivePatch {
    Id id;
    processing::GraphState state; // a preset for the SAME shared graph
    bool operator==(const LivePatch&) const = default;
};
struct SectionPatch {
    Id section, patch;
    bool operator==(const SectionPatch&) const = default;
};
struct LiveAction {
    Id id, marker, port;
    processing::MidiEvent event; // authored MIDI action; never executed on load
    bool operator==(const LiveAction&) const = default;
};
struct ProjectDocument {
    std::uint64_t generation{};
    Project project;
    processing::GraphState graph;
    std::vector<MixerChannel> mixer;
    std::vector<MidiBinding> midi;
    std::vector<LivePatch> patches;
    std::vector<SectionPatch> section_patches;
    std::vector<LiveAction> actions;
    std::string live_notes;
    std::vector<Extension> extensions;
    void validate() const;
    bool operator==(const ProjectDocument&) const = default;
};
struct ShowEntry {
    Id id, project;
    std::string path, notes; // references only, never embeds another project
    std::optional<Id> patch;
    bool operator==(const ShowEntry&) const = default;
};
struct ShowDocument {
    std::uint64_t generation{};
    Id id;
    std::string title;
    std::vector<ShowEntry> entries;
    std::optional<Id> selected;
    std::vector<Extension> extensions;
    void validate() const;
    bool operator==(const ShowDocument&) const = default;
};
std::string encode(const ProjectDocument&);
std::string encode(const ShowDocument&);
ProjectDocument decode_project(std::string_view); // also migrates core snapshot v1/v2
ShowDocument decode_show(std::string_view);
ProjectDocument demo_document();

// Control/quiescent thread only. These operations never run in an audio callback.
// One writer per path. Temporary files live beside the destination.
enum class SavePoint { temporary_synced, backup_synced, before_replace };
using SaveHook = std::function<void(SavePoint)>; // fault injection/diagnostics
void save_project(const std::filesystem::path&, const ProjectDocument&, SaveHook = {});
void save_show(const std::filesystem::path&, const ShowDocument&, SaveHook = {});
ProjectDocument load_project(const std::filesystem::path&);
ShowDocument load_show(const std::filesystem::path&);
void save_autosave(const std::filesystem::path& project_path, const ProjectDocument&);
struct Recovery {
    ProjectDocument document;
    std::filesystem::path source;
    std::vector<std::string> warnings;
};
Recovery recover_project(const std::filesystem::path&); // read-only, highest valid generation

// Application thread captures immutable state, worker coalesces pending saves.
// No timer here: shell schedules submit. Destruction drains and joins off RT.
class AutosaveWorker {
public:
    explicit AutosaveWorker(std::filesystem::path project_path);
    ~AutosaveWorker();
    AutosaveWorker(const AutosaveWorker&) = delete;
    AutosaveWorker& operator=(const AutosaveWorker&) = delete;
    std::uint64_t submit(std::shared_ptr<const ProjectDocument>);
    std::string wait(std::uint64_t ticket); // empty = last completed write succeeded
private:
    void run();
    std::filesystem::path path_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::shared_ptr<const ProjectDocument> pending_;
    std::uint64_t submitted_{}, completed_{}, last_generation_{};
    bool stopping_{};
    std::string error_;
    std::thread thread_;
};

// Inject the same Services and GraphStore into every workspace.
// Loading never opens devices/plugins, restores playback, or restores Undo history.
class SharedSession {
public:
    explicit SharedSession(ProjectDocument);
    const Services& services() const { return services_; }
    std::shared_ptr<processing::GraphStore> graphs() const { return graphs_; }
    ProjectDocument capture() const;
private:
    ProjectDocument base_;
    Services services_;
    std::shared_ptr<processing::GraphStore> graphs_;
};
} // namespace mrs::persistence
