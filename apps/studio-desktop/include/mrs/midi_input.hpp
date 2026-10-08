#pragma once
#include <mrs/audio.hpp>
#include <memory>
#include <string>
#include <vector>
namespace mrs::desktop {
struct MidiInputPort {std::string id,name;unsigned index{};bool operator==(const MidiInputPort&)const=default;};
struct MidiInputRoute {std::string port;std::size_t track{};int channel{-1};bool operator==(const MidiInputRoute&)const=default;};
// Windows driver callbacks -> bounded port queues -> one non-RT bridge producer
// -> engine SPSC -> device-owned instrument processor. No graph pointers cross here.
class MidiInputs {
public:
    explicit MidiInputs(std::shared_ptr<audio::AudioEngine>);
    ~MidiInputs();
    static std::vector<MidiInputPort> ports();
    void routes(std::vector<MidiInputRoute>,std::uint64_t generation);
    std::string status(const std::string&) const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
bool decode_midi_short(std::uint32_t,processing::MidiEvent&) noexcept;
}
