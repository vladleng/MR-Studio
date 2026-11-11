#pragma once
#include <mrs/audio_to_midi.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace mrs {
// NON-RT, pinned ace-midi flat JSONL schema. Limits are aggregate, not just
// per read; a caller must reject any exception and discard the entire job.
class AceMidiProtocol {
public:
    // Frame count is after pinned 16 kHz resampling (which truncates). The
    // full-source duration remains independent for clipping/placement.
    explicit AceMidiProtocol(double source_seconds, Sample frames_16k = -1, bool allow_zero_duration = false);
    void feed(std::string_view bytes);
    void finish(); // EOF, requires final newline, paired notes/progress/done
    int completed() const { return completed_; }
    int total() const { return total_; }
    std::size_t midi_bytes() const { return midi_bytes_; }
    std::size_t raw_count() const { return notes_.size(); }
    // Clipped to full source duration; empty is a valid recognition result.
    std::vector<TranscribedNote> notes() const;
    const std::vector<TranscribedNote>& raw_notes() const { return notes_; }
private:
    void line(std::string_view);
    double seconds_{};
    std::size_t bytes_{}, lines_{}, ended_{}, midi_bytes_{};
    int completed_{-1}, total_{};
    bool done_{}, finished_{}, allow_zero_duration_{};
    std::string pending_;
    std::vector<TranscribedNote> notes_;
};
}
