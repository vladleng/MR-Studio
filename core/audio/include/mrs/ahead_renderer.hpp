#pragma once
#include <mrs/audio.hpp>
#include <thread>
namespace mrs::audio {
// Optional single-owner playback producer. Graph eligibility is conservative;
// live/unsupported graphs retain the ordinary callback path.
class AheadRenderer {
public:
    AheadRenderer(std::shared_ptr<AudioEngine>,std::uint32_t device_frames,std::uint32_t process_frames);
    ~AheadRenderer();
    AheadRenderer(const AheadRenderer&)=delete;
    void start(); // control thread, before device callback starts
    void stop() noexcept; // device callback MUST already be stopped
    void process(const float*,float*,std::uint32_t,std::uint32_t flags=0) noexcept;
    bool active() const noexcept {return active_;}
private:
    friend class AudioEngine;
    struct Signals;
    struct Delivered {RealtimeState head;AudioEngine::MixHead mix;std::uint64_t control_serial{},mix_serial{};};
    struct Mailbox {std::atomic<int> owner{};Delivered value;};
    struct Packet {
        std::vector<float> audio;
        RealtimeState first,last;
        AudioEngine::MixHead mix,last_mix;
        MixerMeters meters;
        std::uint64_t revision{};
        std::uint64_t control_serial{},mix_serial{};
    };
    std::shared_ptr<AudioEngine> engine_;
    std::unique_ptr<Signals> signals_;
    std::vector<Packet> packets_;
    std::array<Mailbox,3> delivered_;
    std::atomic<std::uint32_t> latest_{};
    Delivered consumer_head_;
    std::atomic<std::uint32_t> read_{},write_{};
    std::atomic<bool> quit_{};
    std::thread producer_;
    std::uint32_t device_frames_{},process_frames_{},partial_{};
    bool active_{};
    std::array<Control,128> controls_{};
    std::vector<MixerUpdate> mixes_; // 16 prepared entries, no RT resize
    std::size_t control_count_{},mix_count_{};
    std::uint64_t control_serial_{},mix_serial_{};
    void record_control(const Control&) noexcept;
    void record_mix(const MixerUpdate&) noexcept;
    void replay(const Delivered&) noexcept;
    void run() noexcept;
    bool read_head(Delivered&) noexcept;
    void publish_head() noexcept;
    void advance(Delivered&,std::uint32_t) noexcept;
};
}
