#pragma once
#include <mrs/core.hpp>
#include <vector>

namespace mrs::audio {
// Application preferences, not project content. Published to DSP through controls.
struct ClickSettings {
    bool playback{}, recording{}, accent{true};
    int level{20}, count_bars{};
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
