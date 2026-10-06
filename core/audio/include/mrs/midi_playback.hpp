#pragma once
#include <mrs/processing.hpp>
namespace mrs::audio {
struct PlaybackMidiNote {
    Sample start{},end{};
    std::uint8_t pitch{},velocity{100},channel{};
};
struct PlaybackMidiEvent {Sample sample{};processing::MidiEvent event;};
std::vector<std::vector<PlaybackMidiEvent>> compile_midi_events(const Project&,std::span<const Id>);
std::vector<std::vector<PlaybackMidiNote>> compile_midi_clips(const Project&,std::span<const Id> tracks);
// Prepared off RT, mutated only by its channel's device scheduler.
// Equal channel/pitch overlaps are merged into one interval during preparation.
class MidiPlayback {
public:
    void prepare(std::vector<PlaybackMidiNote>,std::vector<PlaybackMidiEvent> = {});
    void invalidate() noexcept {expected_=-1;}
    void forget() noexcept {active_count_=0;expected_=-1;was_playing_=false;}
    bool render(Sample,std::uint32_t,bool,processing::MidiBuffer&) noexcept;
private:
    struct Event {Sample sample{};processing::MidiEvent midi;};
    std::vector<PlaybackMidiNote> notes_;
    std::vector<Event> events_;
    std::vector<PlaybackMidiEvent> controls_;
    std::vector<std::uint16_t> control_keys_;
    std::vector<std::vector<PlaybackMidiEvent>> control_lanes_;
    std::size_t control_cursor_{};
    std::vector<Sample> chase_ends_; // prepared interval tree, skips expired/future subtrees
    std::size_t chase_leaves_{1};
    std::array<std::uint16_t,128> active_{};
    std::size_t active_count_{},cursor_{};
    Sample expected_{-1};
    bool was_playing_{};
    bool emit(processing::MidiEvent,processing::MidiBuffer&) noexcept;
};
}
