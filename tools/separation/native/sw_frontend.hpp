#pragma once
// Reference-compatible MDXC overlap/add. No inference, file IO or UI work here.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mrs::separation::native {
inline std::vector<std::size_t> swChunkStarts(std::size_t frames, std::size_t chunk, std::size_t step) {
    if (!chunk || !step || step > chunk) throw std::invalid_argument("Invalid SW chunk geometry");
    std::vector<std::size_t> starts;
    for (std::size_t offset = 0; offset < frames;) {
        if (frames - offset <= chunk) {
            const auto tail = frames > chunk ? frames - chunk : 0;
            if (starts.empty() || starts.back() != tail) starts.push_back(tail);
            break;
        }
        starts.push_back(offset);
        offset += step; // offset + step < frames because step <= chunk.
    }
    return starts;
}

inline std::vector<float> swHammingWindow(std::size_t chunk) {
    if (chunk < 2) throw std::invalid_argument("Invalid SW window length");
    std::vector<float> window(chunk);
    for (std::size_t i = 0; i < chunk; ++i) {
        // scipy.signal.windows.hamming(C), sym=True; evaluate in double then cast.
        window[i] = static_cast<float>(0.54 - 0.46 * std::cos(2.0 * std::numbers::pi *
            static_cast<double>(i) / static_cast<double>(chunk - 1)));
    }
    return window;
}

class SwOverlapAdd {
public:
    SwOverlapAdd(std::size_t frames, int channels, std::size_t chunk)
        : frames_(frames), channels_(channels), window_(swHammingWindow(chunk)) {
        if (!frames || frames > 44100u * 300u || (channels != 1 && channels != 2))
            throw std::invalid_argument("SW accumulator bounds");
        samples_.resize(frames * static_cast<std::size_t>(channels));
        counters_.resize(frames);
    }
    void add(std::size_t start, const float* interleaved, std::size_t inputFrames,
             std::size_t modelFrames) {
        if (!interleaved || start >= frames_ || !inputFrames || inputFrames > frames_ - start ||
            inputFrames > window_.size() || modelFrames > inputFrames || finished_)
            throw std::invalid_argument("SW overlap/add input bounds");
        // torch.istft without length returns floor(inputFrames/hop)*hop.
        // MDXC counts only this valid extent; never count the native padded tail.
        for (std::size_t i = 0; i < modelFrames; ++i) {
            const auto weight = window_[i];
            for (int ch = 0; ch < channels_; ++ch) {
                const auto source = i * static_cast<std::size_t>(channels_) + static_cast<std::size_t>(ch);
                if (!std::isfinite(interleaved[source])) throw std::runtime_error("Non-finite SW output");
                const auto dest = (start + i) * static_cast<std::size_t>(channels_) + static_cast<std::size_t>(ch);
                samples_[dest] += interleaved[source] * weight;
            }
            counters_[start + i] += weight;
        }
    }
    std::vector<float> finish() {
        if (finished_) throw std::logic_error("SW accumulator already finished");
        finished_ = true;
        for (std::size_t i = 0; i < frames_; ++i) {
            const float denominator = std::max(counters_[i], 1.0e-10f);
            for (int ch = 0; ch < channels_; ++ch) {
                auto& sample = samples_[i * static_cast<std::size_t>(channels_) + static_cast<std::size_t>(ch)];
                sample /= denominator;
                if (!std::isfinite(sample)) throw std::runtime_error("Non-finite SW overlap/add result");
            }
        }
        return std::move(samples_);
    }
private:
    std::size_t frames_;
    int channels_;
    std::vector<float> window_, samples_, counters_;
    bool finished_{};
};
}
