#pragma once
#include <mrs/midi_input.hpp>
namespace mrs::desktop {
using MidiOutputPort=MidiInputPort;
struct MidiOutputRoute {std::string port;std::size_t track{};int channel{-1};bool operator==(const MidiOutputRoute&)const=default;};
// Backend calls occur only on the output worker. Injectable fixture never opens hardware.
class IMidiOutputBackend {
public:
    virtual ~IMidiOutputBackend()=default;
    virtual std::vector<MidiOutputPort> ports()=0;
    virtual bool open(const MidiOutputPort&)=0;
    virtual bool send(const std::string&,std::uint32_t)=0;
    virtual void panic(const std::string&) noexcept=0;
    virtual void close() noexcept=0;
};
std::uint32_t encode_midi_short(processing::MidiEvent,int channel=-1) noexcept;
class MidiOutputs {
public:
    explicit MidiOutputs(std::shared_ptr<audio::ExternalMidiQueue>,std::unique_ptr<IMidiOutputBackend> = {});
    ~MidiOutputs();
    static std::vector<MidiOutputPort> ports(); // inventory only, no handles
    void routes(std::vector<MidiOutputRoute>,std::uint64_t generation=0);
    void reconnect();
    std::string status(const std::string&) const;
private: struct Impl;std::unique_ptr<Impl> impl_;
};
}
