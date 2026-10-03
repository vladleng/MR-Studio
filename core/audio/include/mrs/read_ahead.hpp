#pragma once
#include <mrs/audio.hpp>
#include <thread>
namespace mrs::audio {
// Eight 8192-frame pages per voice. Worker writes only unpinned slots.
// Callback pins once per block; no waiting, allocation, I/O or ownership changes.
class ReadAhead {
public:
    static constexpr Sample page_frames = 8192;
    static constexpr std::size_t pages = 8;
    explicit ReadAhead(std::shared_ptr<const WavFile>);
    ~ReadAhead();
    ReadAhead(const ReadAhead&) = delete;
    ReadAhead& operator=(const ReadAhead&) = delete;
    void prime(Sample); // control-only bounded wait, throws on media error/timeout
    void loop(Sample first) noexcept;
    void begin(Sample first) noexcept;
    bool read(Sample frame, std::uint32_t channel, float& value) noexcept;
    bool end() noexcept; // true if this callback missed any source frames
    std::uint64_t errors() const noexcept { return errors_.load(); }
    std::size_t bytes() const { return pages*static_cast<std::size_t>(page_frames)*file_->channels*sizeof(float); }
private:
    struct Slot {
        std::atomic<int> owner{}; // 0 free, -1 worker, +1 callback
        Sample page{-1};
        std::vector<float> samples;
    };
    std::shared_ptr<const WavFile> file_;
    std::array<Slot,pages> slots_;
    std::array<bool,pages> pinned_{};
    std::atomic<Sample> desired_{}, warm_{-1}, loop_{-1};
    std::atomic<bool> quit_{};
    std::atomic<std::uint64_t> errors_{};
    std::thread worker_;
    bool missed_{};
    void run() noexcept;
};
}
