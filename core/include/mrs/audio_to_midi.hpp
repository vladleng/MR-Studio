#pragma once
#include <mrs/core.hpp>

namespace mrs {
inline constexpr std::string_view ace_midi_source_commit = "ee7286d14232b48ace3b6eed0fcf3048081ef0d1";
// Native JSONL note_start/note_end pairs, in seconds relative to the FULL
// source file. A worker transcribing an extracted range must rebase first.
// Velocity is not predicted by MuScriptor.
struct TranscribedNote {
    int pitch{};
    double start{}, end{};
    std::string instrument;
};
struct TranscriptionInstrument {
    std::string_view role;
    int program; // zero-based GM; 128 denotes drums, never a MIDI channel
};
std::span<const TranscriptionInstrument> ace_midi_instruments();
// Argument vector, never a shell command. CPU baseline; launcher must also
// override inherited GGML_BACKEND (upstream gives the environment precedence).
std::vector<std::string> ace_midi_arguments(std::string model, std::string wav, std::string output);

// Non-RT preparation only. Creates independent instrument tracks and existing
// MidiClips, with channel 1 per melodic track and channel 10 for drums. Tracks
// have no external port assigned, so 34 categories do not share a MIDI port.
// Refuses a changed project after async inference; caller must prepare from the
// captured source snapshot, rather than silently accepting a newer revision.
class PlaceTranscription final : public ICommand {
public:
    PlaceTranscription(std::shared_ptr<const Project> source, Id audio_clip,
                       std::span<const TranscribedNote> notes);
    std::string_view name() const override { return "Convert audio to MIDI"; }
    void apply(Project&) const override;
private:
    std::shared_ptr<const Project> source_;
    std::vector<Track> tracks_;
    std::vector<Clip> clips_;
};
}
