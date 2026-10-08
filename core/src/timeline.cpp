#include <mrs/core.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace mrs {
Sample clip_start(const Clip& c,const Timeline& t){return c.midi?t.to_samples(c.midi->start):c.start;}
Sample clip_end(const Clip& c,const Timeline& t){return c.midi?t.to_samples(c.midi->start+c.midi->length):c.start+c.length;}
namespace {
void valid_tick(Tick tick) {
    if (tick < 0 || tick > max_tick) throw std::invalid_argument("tick out of range");
}
}
Timeline::Timeline(TimeMap map, std::uint32_t sample_rate) : rate_(sample_rate) {
    map.validate();
    if (rate_ < 8000 || rate_ > 768000) throw std::invalid_argument("invalid sample rate");
    double sample = 0;
    for (const auto& p : map.tempos) {
        if (!tempos_.empty()) {
            const auto& previous = tempos_.back();
            sample += static_cast<double>(p.tick - previous.tick) * previous.samples_per_tick;
        }
        tempos_.push_back({p.tick, sample, 60.0 * rate_ / (p.bpm * ppq)});
    }
    Tick tick = 0;
    for (const auto& p : map.meters) {
        if (!meters_.empty()) {
            const auto& previous = meters_.back().meter;
            tick += (p.bar - previous.bar) * previous.numerator * (4 * ppq / previous.denominator);
        }
        meters_.push_back({tick, p});
    }
}
Sample Timeline::to_samples(Tick tick) const {
    valid_tick(tick);
    const auto it = std::upper_bound(tempos_.begin(), tempos_.end(), tick,
        [](Tick t, const TempoSegment& p) { return t < p.tick; });
    const auto& p = *std::prev(it);
    const double sample = p.sample + static_cast<double>(tick - p.tick) * p.samples_per_tick;
    if (!std::isfinite(sample) || sample > static_cast<double>(max_sample))
        throw std::out_of_range("sample conversion out of range");
    return static_cast<Sample>(std::llround(sample));
}
Tick Timeline::to_ticks(Sample sample) const {
    if (sample < 0 || sample > max_sample) throw std::invalid_argument("sample out of range");
    const auto it = std::upper_bound(tempos_.begin(), tempos_.end(), static_cast<double>(sample),
        [](double s, const TempoSegment& p) { return s < p.sample; });
    const auto& p = *std::prev(it);
    const double tick = static_cast<double>(p.tick) +
        (static_cast<double>(sample) - p.sample) / p.samples_per_tick;
    if (!std::isfinite(tick) || tick > static_cast<double>(max_tick))
        throw std::out_of_range("tick conversion out of range");
    return static_cast<Tick>(std::llround(tick));
}
MusicalPosition Timeline::musical_position(Tick tick) const {
    valid_tick(tick);
    const auto it = std::upper_bound(meters_.begin(), meters_.end(), tick,
        [](Tick t, const MeterSegment& p) { return t < p.tick; });
    const auto& p = *std::prev(it);
    const Tick beat_ticks = 4 * ppq / p.meter.denominator;
    const Tick bar_ticks = beat_ticks * p.meter.numerator;
    const Tick local = tick - p.tick;
    return {p.meter.bar + local / bar_ticks,
        static_cast<int>((local % bar_ticks) / beat_ticks) + 1, local % beat_ticks};
}
Tick Timeline::to_ticks(MusicalPosition position) const {
    if (position.bar < 1 || position.bar > max_tick + 1)
        throw std::invalid_argument("musical bar out of range");
    const auto it = std::upper_bound(meters_.begin(), meters_.end(), position.bar,
        [](std::int64_t bar, const MeterSegment& p) { return bar < p.meter.bar; });
    const auto& p = *std::prev(it);
    const Tick beat_ticks = 4 * ppq / p.meter.denominator;
    if (position.beat < 1 || position.beat > p.meter.numerator ||
        position.tick < 0 || position.tick >= beat_ticks)
        throw std::invalid_argument("musical beat/tick out of range");
    const Tick tick = p.tick + (position.bar - p.meter.bar) * p.meter.numerator * beat_ticks +
        (position.beat - 1) * beat_ticks + position.tick;
    valid_tick(tick);
    return tick;
}
} // namespace mrs
