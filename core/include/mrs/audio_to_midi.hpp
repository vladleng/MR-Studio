#pragma once
#include <mrs/core.hpp>

namespace mrs {
// Worker JSONL note_start/note_end pairs, in seconds relative to the FULL
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
// Legacy flat-argv transport used by synthetic process contract fixtures.
// Product transcription uses the isolated Python adapter argument vector.
std::vector<std::string> ace_midi_arguments(std::string model, std::string wav, std::string output);

// Non-RT preparation only. Creates independent instrument tracks and existing
// MidiClips, with channel 1 per melodic track and channel 10 for drums. Tracks
// have no external port assigned, so 34 categories do not share a MIDI port.
// Refuses a changed project after async inference; caller must prepare from the
// captured source snapshot, rather than silently accepting a newer revision.
class PlaceTranscription final : public ICommand {
public:
    PlaceTranscription(std::shared_ptr<const Project> source, Id audio_clip,
                       std::span<const TranscribedNote> notes, std::optional<TimeMap> time = {},
                       std::span<const TranscribedNote> original = {}, std::string provenance = {});
    std::string_view name() const override { return "Convert audio to MIDI"; }
    void apply(Project&) const override;
private:
    std::shared_ptr<const Project> source_;
    std::optional<TimeMap> time_;
    std::optional<TranscriptionRecord> provenance_;
    std::vector<Track> tracks_;
    std::vector<Clip> clips_;
};
}
