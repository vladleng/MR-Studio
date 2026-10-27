#pragma once
#include <mrs/waveform.hpp>
#include <algorithm>
#include <cmath>
#include <span>

namespace ui::live {
// Display envelopes only. No PCM mixing, voices or audio-thread state.
struct PlaybackWaveformSource {
    const mrs::audio::Waveform *wave{};
    double rate{}, start{}, offset{}, length{};
    mrs::audio::Peak display(double begin, double end) const {
        if (!wave || rate <= 0 || length <= 0 || end <= begin || end <= start || begin >= start + length)
            return {};
        const bool silence = begin < start || end > start + length;
        begin = std::max(begin, start);
        end = std::min(end, start + length);
        mrs::audio::Peak result;
        for (std::uint32_t c = 0; c < wave->channels(); ++c) {
            const auto peak = wave->display((offset + begin - start) * rate, (offset + end - start) * rate, c);
            result.minimum += peak.minimum / wave->channels();
            result.maximum += peak.maximum / wave->channels();
        }
        if (silence) {
            result.minimum = std::min(0.f, result.minimum);
            result.maximum = std::max(0.f, result.maximum);
        }
        return result;
    }
};
inline mrs::audio::Peak averagePlaybackEnvelope(std::span<const PlaybackWaveformSource> sources,
                                               double begin, double end) {
    mrs::audio::Peak result;
    if (sources.empty()) return result;
    const float divisor = static_cast<float>(sources.size());
    for (const auto &source : sources) {
        const auto peak = source.display(begin, end);
        result.minimum += peak.minimum / divisor;
        result.maximum += peak.maximum / divisor;
    }
    return result;
}
} // namespace ui::live
