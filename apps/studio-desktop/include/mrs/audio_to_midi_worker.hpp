#pragma once
#include <mrs/audio_to_midi.hpp>
#include <chrono>
#include <filesystem>
#include <functional>
#include <stop_token>
#include <memory>

namespace mrs::transcription {
inline constexpr std::string_view python_source_commit="8a1ef88672cc23c26619adaff37ec48d82c0b906";
// protocol_fixture is a synthetic process transport used by contract tests only.
enum class Backend { python_medium, protocol_fixture };
// NON-RT job phases; no project persistence or audio callback state.
enum class Phase { checking_files, checking_runtime, preparing_audio, starting_worker,
                   importing_runtime, loading_model, reading_audio, recognizing,
                   processing_midi, exporting_midi, validating_result, complete };
inline constexpr std::string_view phase_label(Phase phase){
    switch(phase){
    case Phase::checking_files:return "Verifying model and input files...";
    case Phase::checking_runtime:return "Verifying Python runtime...";
    case Phase::preparing_audio:return "Preparing selected audio range...";
    case Phase::starting_worker:return "Starting recognition worker...";
    case Phase::importing_runtime:return "Loading Python libraries...";
    case Phase::loading_model:return "Loading MuScriptor Medium model...";
    case Phase::reading_audio:return "Inspecting input WAV...";
    case Phase::recognizing:return "Recognizing audio...";
    case Phase::processing_midi:return "Processing MIDI and musical timing...";
    case Phase::exporting_midi:return "Exporting MIDI result...";
    case Phase::validating_result:return "Validating MIDI result...";
    case Phase::complete:return "Recognition complete.";
    }
    return "Preparing transcription...";
}
struct MusicalOptions {
    bool import_tempo{};
    bool detect_tempo{}, quantize{}, tempo_map{true}, trim_leading_bars{};
    double bpm{}, strength{1.}, first_downbeat{-1.}, beat_scale{1.}, fill_gaps{};
    unsigned meter{}, subdivision{4};
};
enum class State { ready, empty, cancelled, timeout, failed };
// NON-RT CPU selection. Auto reserves one logical CPU and caps inference at 16.
unsigned available_cpu_threads();
unsigned choose_cpu_threads(unsigned requested = 0);
bool avx2_usable(std::uint32_t leaf1_ecx,std::uint32_t leaf7_ebx,std::uint64_t xcr0);
bool optimized_cpu_available();
struct SourceRange {Sample offset{},frames{};std::uint32_t rate{};}; // project sample domain
struct Request {
    TranscriptionPlacement placement;
    Backend backend{Backend::python_medium};
    std::filesystem::path worker_script;
    std::string worker_sha256;
    std::vector<std::string> instruments; // Python official vocabulary; empty = Auto
    MusicalOptions musical;
    bool compatibility_cpu{}; // restrict Python dispatch / select native generic
    std::shared_ptr<const void> runtime_lifetime;
    std::function<void(std::stop_token)> verify_runtime; // NON-RT inventory/probe
    std::filesystem::path executable, model_directory, source, temporary_root;
    // Expected digests are mandatory, supplied by the installer/caller's trust
    // boundary. This API does not establish publisher identity or model rights.
    std::string executable_sha256, weights_sha256, config_sha256;
    std::string model_id, model_revision, model_license;
    Id project_id, clip_id;
    std::uint64_t project_revision{};
    std::chrono::milliseconds timeout{std::chrono::minutes(30)};
    unsigned cpu_threads{2}; // 0 = bounded Auto; explicit values capped to available CPUs
    std::optional<SourceRange> range;
    std::string stem_instrument; // empty = Auto; canonical native decoder constraint
    std::uint64_t memory_bytes{4ULL*1024*1024*1024};
};
struct Result {
    Backend backend{Backend::python_medium};
    MusicalOptions musical;
    std::string cpu_capability;
    std::uint64_t peak_memory_bytes{}; // process peak working set (Windows) / RSS (Linux)
    double model_load_seconds{}, inference_seconds{}, detected_bpm{}, first_downbeat{};
    unsigned detected_meter{};
    std::vector<double> beats;
    std::vector<TranscribedNote> original_notes; // immutable unquantized reference
    State state{State::failed};
    std::string error, diagnostics;
    Phase phase{Phase::checking_files};
    Id project_id, clip_id;
    std::uint64_t project_revision{};
    // source_sha256: full WAV for legacy whole-source requests, selected PCM for range requests.
    std::string source_sha256, executable_sha256, weights_sha256, config_sha256;
    std::string model_id, model_revision, model_license, midi_sha256, worker_sha256;
    std::size_t raw_notes{};
    double analysis_seconds{}, source_offset_seconds{}, elapsed_seconds{};
    unsigned cpu_threads{};
    std::uint32_t source_rate{};
    Sample source_first_frame{}, source_frame_count{};
    std::string input_sha256; // exact private WAV, distinct from selected PCM hash
    std::vector<TranscribedNote> notes; // full-source seconds, source-bounded
};
// Blocking NON-RT service call, to be owned by a job/service thread. The progress
// callback runs on that thread, never the UI/audio thread. No Model mutation.
// Every exit destroys the process tree before deleting the private job folder.
Result run(const Request&, std::stop_token = {},
           const std::function<void(int completed,int total)>& progress = {},
           const std::function<void(Phase)>& phase_changed = {});
}
