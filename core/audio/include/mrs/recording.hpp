#pragma once
#include <mrs/audio.hpp>
#include <mrs/waveform.hpp>
#include <fstream>
#include <thread>
namespace mrs::audio {
enum class RecordFault : int { none, overflow, missing_input, discontinuity, disk, size_limit };
struct RecordStatus {
    std::uint64_t frames{}, dropped_blocks{}, missing_blocks{}, discontinuities{}, nonfinite_samples{};
    RecordFault fault{};
};
struct RecordedFile { std::filesystem::path path; Sample frames{}; RecordStatus status; };
struct RecordPreview {
    Sample frames{}, bin_frames{};
    std::uint32_t channels{};
    std::vector<Peak> peaks; // interleaved channels; disk-worker-generated envelope
};
class Recorder {
public:
    static constexpr std::size_t capacity = 262144; // 1 MiB mono float ring
    Recorder(std::filesystem::path destination, std::uint32_t rate, Sample start, std::vector<std::uint32_t> selectors = {0});
    ~Recorder();
    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;
    const std::vector<std::uint32_t>& selectors() const { return selectors_; }
    std::uint32_t channels() const { return static_cast<std::uint32_t>(selectors_.size()); }
    std::uint32_t rate() const { return rate_; }
    Sample start() const { return start_; }
    const std::filesystem::path& destination() const { return destination_; } // immutable, control thread
    // One audio producer only. Raw selected mono input, before monitor/processors.
    void capture(const float*, std::uint32_t input_channels, std::uint32_t frames, Sample position) noexcept;
    void input_dropout() noexcept;
    void discontinuity() noexcept;
    RecordStatus status() const noexcept;
    // Message/control thread only. Bounded coherent snapshot, no waiting for writer.
    std::optional<RecordPreview> preview() const;
    RecordedFile finish(); // ONLY after callback stops: drain, repair header, publish
private:
    std::filesystem::path destination_, temporary_;
    std::uint32_t rate_{};
    Sample start_{};
    std::vector<std::uint32_t> selectors_;
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
    static constexpr std::size_t preview_bins=2048;
    std::array<std::atomic<std::uint64_t>,preview_bins*2> preview_peaks_{};
    std::atomic<std::uint64_t> preview_version_{},preview_frames_{},preview_bin_frames_{256};
    void update_preview(std::uint64_t first_sample,std::size_t count) noexcept; // disk worker only
};
}
