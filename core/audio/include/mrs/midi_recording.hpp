#pragma once
#include <mrs/processing.hpp>
#include <atomic>
namespace mrs::audio {
std::uint64_t midi_clock_ns() noexcept;
// Maps input arrival time to the current transport anchor, never before take start.
Sample midi_record_sample(Sample head,Sample start,std::uint64_t now,std::uint64_t stamp,std::uint32_t rate) noexcept;
class MidiRecorder {
public:
    static constexpr std::size_t capacity=65536;
    struct Entry {Sample sample{};processing::MidiEvent event;bool cut{};};
    explicit MidiRecorder(Sample start,std::size_t note_limit=4096,std::size_t event_limit=8144,std::vector<std::uint16_t> existing_keys={});
    void capture(Sample,processing::MidiEvent) noexcept;
    void cut(Sample sample) noexcept;
    void advance(Sample end) noexcept {end_.store(end);}
    void fail() noexcept {fault_=true;}
    bool fault() const noexcept {return fault_.load();}
    Sample start() const noexcept {return start_;}
    Sample end() const noexcept {return end_.load();}
    // Callback must be stopped and joined before finish reads entries.
    MidiClip finish(const Timeline&) const;
private:
    Sample start_{};
    std::atomic<Sample> end_{};
    std::vector<Entry> entries_;
    std::size_t count_{},note_count_{},event_count_{},key_count_{};
    std::array<std::uint16_t,64> keys_{};
    std::array<bool,2048> held_{};std::size_t held_count_{},note_limit_{4096},event_limit_{8144};
    std::atomic<bool> fault_{};
};
}
