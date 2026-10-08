#pragma once
#include <mrs/audio.hpp>
#include <thread>
namespace mrs::audio {
// Disjoint channel owners. Master and live closure remain on the device.
// Only prepared PCM/PDC contributions cross this SPSC boundary.
class MixedRenderer {
public:
    MixedRenderer(std::shared_ptr<AudioEngine>,std::uint32_t,std::uint32_t);
    ~MixedRenderer();
    bool start(); // control, device stopped
    void stop() noexcept; // device MUST be stopped first
    bool active() const noexcept { return active_; }
    void begin(std::uint32_t) noexcept; // callback, after controls/seek/mix
    void end() noexcept; // publish heard context after live processing
private:
    struct Signals;
    struct Context {
        RealtimeState head;
        AudioEngine::MixHead mix;
        std::uint64_t revision{},sequence{};
    };
    struct Mailbox { std::atomic<int> owner{};Context context; };
    struct Packet {
        std::vector<float> pcm;
        MixerMeters meters;
        std::uint64_t revision{},sequence{};
    };
    std::shared_ptr<AudioEngine> device_;
    std::unique_ptr<AudioEngine> ahead_;
    std::unique_ptr<ChannelWorkers> saved_workers_;
    std::unique_ptr<Signals> signals_;
    std::array<Mailbox,3> contexts_;
    std::atomic<unsigned> latest_{};
    std::vector<Packet> packets_;
    std::vector<float> input_,output_;
    std::array<bool,max_mixer_tracks> ahead_channels_{};
    std::atomic<unsigned> read_{},write_{};
    std::atomic<bool> quit_{};
    std::thread producer_;
    std::uint32_t frames_{},window_{};
    std::size_t edges_{};
    std::uint64_t sequence_{},revision_{};
    bool active_{};
    bool read_context(Context&) noexcept;
    void publish_context(const Context&) noexcept;
    void run() noexcept;
};
}
