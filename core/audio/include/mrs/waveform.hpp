#pragma once
#include <mrs/audio.hpp>
namespace mrs::audio {
struct Peak { float minimum{}, maximum{}; };
class Waveform {
public:
    // Worker/control thread only. Channel extrema retain phase and transients.
    explicit Waveform(const AudioData&, std::shared_ptr<std::atomic<bool>> cancel = {});
    Peak range(Sample begin, Sample end, std::uint32_t channel) const;
    // Display envelope only: bounded interpolation of cached extrema at high zoom.
    // Overview ranges still retain every transient. Never reads PCM/files in paint.
    Peak display(double begin, double end, std::uint32_t channel) const;
    Sample frames() const { return frames_; }
    std::uint32_t channels() const { return channels_; }
private:
    Sample frames_{};
    std::uint32_t channels_{};
    Sample bin_frames_{16};
    std::vector<std::vector<Peak>> levels_; // bounded base, powers of two
};
} // namespace mrs::audio
