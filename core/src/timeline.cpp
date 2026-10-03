#include <mrs/core.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace mrs {
namespace {
void valid_tick(Tick tick) {
    if (tick < 0 || tick > max_tick) throw std::invalid_argument("tick out of range");
}
void valid_sample(Sample sample) {
    if (sample < 0 || sample > max_sample) throw std::invalid_argument("sample out of range");
}
struct MeterSegment { Tick start; MeterPoint meter; };
std::vector<MeterSegment> segments(const TimeMap& map) {
    std::vector<MeterSegment> result;
    Tick start = 0;
    MeterPoint previous = map.meters.front();
    for (const auto& meter : map.meters) {
        start += (meter.bar - previous.bar) * previous.numerator * (4 * ppq / previous.denominator);
        result.push_back({start, meter});
        previous = meter;
    }
    return result;
}
}
Timeline::Timeline(TimeMap map, std::uint32_t sample_rate)
    : map_(std::move(map)), rate_(sample_rate) {
    map_.validate();
    if (rate_ < 8000 || rate_ > 768000) throw std::invalid_argument("invalid sample rate");
}
Sample Timeline::to_samples(Tick tick) const {
    valid_tick(tick);
    double samples = 0;
    for (std::size_t i = 0; i < map_.tempos.size(); ++i) {
        const auto& point = map_.tempos[i];
        if (point.tick >= tick) break;
        const auto end = i + 1 < map_.tempos.size() ? std::min(tick, map_.tempos[i + 1].tick) : tick;
        samples += static_cast<double>(end - point.tick) * (60.0 * rate_ / (point.bpm * ppq));
    }
    if (!std::isfinite(samples) || samples > static_cast<double>(max_sample))
        throw std::out_of_range("sample conversion out of range");
    return static_cast<Sample>(std::llround(samples));
}
Tick Timeline::to_ticks(Sample sample) const {
    valid_sample(sample);
    double remaining = static_cast<double>(sample);
    for (std::size_t i = 0; i < map_.tempos.size(); ++i) {
        const auto& point = map_.tempos[i];
        const double samples_per_tick = 60.0 * rate_ / (point.bpm * ppq);
        if (i + 1 < map_.tempos.size()) {
            const double span = static_cast<double>(map_.tempos[i + 1].tick - point.tick) * samples_per_tick;
            if (remaining >= span) {
                remaining -= span;
                continue;
            }
        }
        const double tick = static_cast<double>(point.tick) + remaining / samples_per_tick;
        if (!std::isfinite(tick) || tick > static_cast<double>(max_tick))
            throw std::out_of_range("tick conversion out of range");
        return static_cast<Tick>(std::llround(tick));
    }
    throw std::logic_error("empty tempo map");
}
MusicalPosition Timeline::musical_position(Tick tick) const {
    valid_tick(tick);
    const auto all = segments(map_);
    auto chosen = all.front();
    for (const auto& segment : all) {
        if (segment.start > tick) break;
        chosen = segment;
    }
    const Tick beat_ticks = 4 * ppq / chosen.meter.denominator;
    const Tick bar_ticks = beat_ticks * chosen.meter.numerator;
    const Tick local = tick - chosen.start;
    return {chosen.meter.bar + local / bar_ticks,
            static_cast<int>((local % bar_ticks) / beat_ticks) + 1, local % beat_ticks};
}
Tick Timeline::to_ticks(MusicalPosition position) const {
    if (position.bar < 1 || position.bar > 1'000'000'000)
        throw std::invalid_argument("musical bar out of range");
    const auto all = segments(map_);
    auto chosen = all.front();
    for (const auto& segment : all) {
        if (segment.meter.bar > position.bar) break;
        chosen = segment;
    }
    const Tick beat_ticks = 4 * ppq / chosen.meter.denominator;
    if (position.beat < 1 || position.beat > chosen.meter.numerator ||
        position.tick < 0 || position.tick >= beat_ticks)
        throw std::invalid_argument("musical beat/tick out of range");
    const Tick tick = chosen.start + (position.bar - chosen.meter.bar) * chosen.meter.numerator * beat_ticks
                    + (position.beat - 1) * beat_ticks + position.tick;
    valid_tick(tick);
    return tick;
}
} // namespace mrs
