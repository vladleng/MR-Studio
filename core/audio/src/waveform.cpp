#include <mrs/waveform.hpp>
#include <algorithm>
#include <stdexcept>
#include <limits>
namespace mrs::audio {
Waveform::Waveform(const AudioData& data, std::shared_ptr<std::atomic<bool>> cancel) : frames_(data.frames()), channels_(data.channels) {
    data.validate();
    // Cap peak storage to <= 65536 base bins per channel, independent of duration.
    while ((frames_+bin_frames_-1)/bin_frames_ > 65536/channels_) bin_frames_ *= 2;
    const auto count = static_cast<std::size_t>((frames_+bin_frames_-1)/bin_frames_);
    levels_.emplace_back(count*channels_,Peak{std::numeric_limits<float>::max(),std::numeric_limits<float>::lowest()});
    std::vector<float> block;
    if (data.file) block.resize(static_cast<std::size_t>(8192)*channels_);
    for (Sample first = 0; first < frames_; first += 8192) {
        if (cancel && cancel->load()) throw std::runtime_error("waveform cancelled");
        const auto frames = std::min(Sample{8192},frames_-first);
        if (data.file) data.file->read(first,std::span<float>(block).first(static_cast<std::size_t>(frames)*channels_));
        for (Sample f = 0; f < frames; ++f) for (std::uint32_t c = 0; c < channels_; ++c) {
            const auto v = data.file ? block[static_cast<std::size_t>(f)*channels_+c] :
                data.samples[static_cast<std::size_t>(first+f)*channels_+c];
            auto& p = levels_.front()[static_cast<std::size_t>((first+f)/bin_frames_)*channels_+c];
            p.minimum = std::min(p.minimum,v); p.maximum = std::max(p.maximum,v);
        }
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
    std::size_t a = static_cast<std::size_t>(begin/bin_frames_), b = static_cast<std::size_t>((end+bin_frames_-1)/bin_frames_);
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
