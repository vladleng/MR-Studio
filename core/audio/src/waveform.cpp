#include <mrs/waveform.hpp>
#include <algorithm>
#include <stdexcept>
#include <limits>
namespace mrs::audio {
Waveform::Waveform(const AudioData& data) : frames_(data.frames()), channels_(data.channels) {
    data.validate();
    const auto count = static_cast<std::size_t>((frames_+255)/256);
    levels_.emplace_back(count*channels_);
    for (std::size_t b = 0; b < count; ++b) for (std::uint32_t c = 0; c < channels_; ++c) {
        Peak p{std::numeric_limits<float>::max(),std::numeric_limits<float>::lowest()};
        const auto end = std::min(frames_,static_cast<Sample>((b+1)*256));
        for (Sample f = static_cast<Sample>(b*256); f < end; ++f) {
            const auto v = data.samples[static_cast<std::size_t>(f)*channels_+c];
            p.minimum = std::min(p.minimum,v); p.maximum = std::max(p.maximum,v);
        }
        levels_.front()[b*channels_+c] = p;
    }
    std::size_t bins = count;
    while (bins > 1) {
        const auto next = (bins+1)/2;
        std::vector<Peak> level(next*channels_);
        for (std::size_t b = 0; b < next; ++b) for (std::uint32_t c = 0; c < channels_; ++c) {
            auto p = levels_.back()[b*2*channels_+c];
            if (b*2+1 < bins) {
                const auto q = levels_.back()[(b*2+1)*channels_+c];
                p.minimum = std::min(p.minimum,q.minimum); p.maximum = std::max(p.maximum,q.maximum);
            }
            level[b*channels_+c] = p;
        }
        levels_.push_back(std::move(level)); bins = next;
    }
}
Peak Waveform::range(Sample begin, Sample end, std::uint32_t channel) const {
    if (channel >= channels_) throw std::invalid_argument("invalid waveform channel");
    begin = std::clamp(begin,Sample{0},frames_); end = std::clamp(end,Sample{0},frames_);
    if (end <= begin) return {};
    std::size_t a = static_cast<std::size_t>(begin/256), b = static_cast<std::size_t>((end+255)/256);
    Peak p{std::numeric_limits<float>::max(),std::numeric_limits<float>::lowest()};
    // Exact union of base bins, O(log bins); no allocation or PCM scan in paint.
    while (a < b) {
        std::size_t level = 0, block = 1;
        while (level+1 < levels_.size() && a%(block*2) == 0 && block*2 <= b-a) { ++level; block *= 2; }
        const auto q = levels_[level][(a/block)*channels_+channel];
        p.minimum = std::min(p.minimum,q.minimum); p.maximum = std::max(p.maximum,q.maximum); a += block;
    }
    return p;
}
} // namespace mrs::audio
