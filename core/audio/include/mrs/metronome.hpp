#pragma once
#include <mrs/core.hpp>
#include <vector>

namespace mrs::audio {
// Host preferences / Live show settings, not Core project content. Published through controls.
struct ClickSettings {
    bool playback{}, recording{}, accent{true};
    int level{20}, count_bars{};
    // Application: physical outputs. DSP: mapped stream indices. -1/-1 follows Master.
    int output_left{-1}, output_right{-1};
    std::int64_t control_flags() const noexcept {
        return (playback ? 1 : 0) | (recording ? 2 : 0) | (accent ? 4 : 0) |
               (static_cast<std::int64_t>(output_left + 1) << 8) |
               (static_cast<std::int64_t>(output_right + 1) << 16);
    }
    bool operator==(const ClickSettings&) const = default;
    void validate() const;
};
class Metronome {
public:
    void prepare(const TimeMap&,std::uint32_t);
    float sample(Sample position,bool enabled,bool accent,float gain) noexcept;
    float count_sample(Sample elapsed,double beat_frames,int beats,bool accent,float gain) noexcept;
    void reset() noexcept {last_=-2;cursor_=normal_.size();}
private:
    struct Tempo {Tick tick;double frame,per_tick;};
    struct Meter {Tick tick;int beats;Tick step;};
    std::vector<Tempo> tempos_;
    std::vector<Meter> meters_;
    std::vector<float> normal_,accent_;
    Sample last_{-2},next_{};
    Tick next_tick_{};
    std::size_t cursor_{};
    bool strong_{},next_strong_{};
    double tick_at(double) const noexcept;
    Sample frame_at(Tick) const noexcept;
    void schedule(Sample) noexcept;
    float tone(bool,bool,float) noexcept;
};
}
