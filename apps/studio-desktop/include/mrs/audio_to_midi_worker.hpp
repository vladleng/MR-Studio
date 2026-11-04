#pragma once
#include <mrs/audio_to_midi.hpp>
#include <chrono>
#include <filesystem>
#include <functional>
#include <stop_token>

namespace mrs::transcription {
enum class State { ready, empty, cancelled, timeout, failed };
struct Request {
    std::filesystem::path executable, model_directory, source, temporary_root;
    // Expected digests are mandatory, supplied by the installer/caller's trust
    // boundary. This API does not establish publisher identity or model rights.
    std::string executable_sha256, weights_sha256, config_sha256;
    std::string model_id, model_revision, model_license;
    Id project_id, clip_id;
    std::uint64_t project_revision{};
    std::chrono::milliseconds timeout{std::chrono::minutes(30)};
    unsigned cpu_threads{2};
    std::uint64_t memory_bytes{4ULL*1024*1024*1024};
};
struct Result {
    State state{State::failed};
    std::string error, diagnostics;
    Id project_id, clip_id;
    std::uint64_t project_revision{};
    std::string source_sha256, executable_sha256, weights_sha256, config_sha256;
    std::string model_id, model_revision, model_license, midi_sha256;
    std::size_t raw_notes{};
    std::vector<TranscribedNote> notes; // full-source seconds, source-bounded
};
// Blocking NON-RT service call, to be owned by a job/service thread. The progress
// callback runs on that thread, never the UI/audio thread. No Model mutation.
// Every exit destroys the process tree before deleting the private job folder.
Result run(const Request&, std::stop_token = {},
           const std::function<void(int completed,int total)>& progress = {});
}
