#pragma once
#include <mrs/audio.hpp>
namespace mrs::audio {
struct Peak { float minimum{}, maximum{}; };
class Waveform {
public:
    // Worker/control thread only. Channel extrema retain phase and transients.
    explicit Waveform(const AudioData&, std::shared_ptr<std::atomic<bool>> cancel = {});
    Peak range(Sample begin, Sample end, std::uint32_t channel) const;
    Sample frames() const { return frames_; }
    std::uint32_t channels() const { return channels_; }
private:
    Sample frames_{};
    std::uint32_t channels_{};
    Sample bin_frames_{256};
    std::vector<std::vector<Peak>> levels_; // 256-frame base, powers of two
};
} // namespace mrs::audio
