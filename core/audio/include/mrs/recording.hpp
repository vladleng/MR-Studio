#pragma once
#include <mrs/audio.hpp>
#include <fstream>
#include <thread>
namespace mrs::audio {
enum class RecordFault : int { none, overflow, missing_input, discontinuity, disk, size_limit };
struct RecordStatus {
    std::uint64_t frames{}, dropped_blocks{}, missing_blocks{}, discontinuities{}, nonfinite_samples{};
    RecordFault fault{};
};
struct RecordedFile { std::filesystem::path path; Sample frames{}; RecordStatus status; };
class Recorder {
public:
    static constexpr std::size_t capacity = 262144; // 1 MiB mono float ring
    Recorder(std::filesystem::path destination, std::uint32_t rate, Sample start);
    ~Recorder();
    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;
    std::uint32_t rate() const { return rate_; }
    Sample start() const { return start_; }
    // One audio producer only. Raw selected mono input, before monitor/processors.
    void capture(const float*, std::uint32_t input_channels, std::uint32_t frames, Sample position) noexcept;
    void input_dropout() noexcept;
    void discontinuity() noexcept;
    RecordStatus status() const noexcept;
    RecordedFile finish(); // ONLY after callback stops: drain, repair header, publish
private:
    std::filesystem::path destination_, temporary_;
    std::uint32_t rate_{};
    Sample start_{};
    std::vector<float> ring_ = std::vector<float>(capacity);
    std::atomic<std::uint64_t> write_{}, read_{}, dropped_{}, missing_{}, jumps_{}, nonfinite_{};
    std::atomic<int> fault_{};
    std::atomic<bool> quit_{};
    std::ofstream output_;
    std::thread worker_;
    std::uint64_t written_{};
    bool finished_{};
    void fail(RecordFault) noexcept;
    void header(std::uint32_t samples);
    void run() noexcept;
};
}
