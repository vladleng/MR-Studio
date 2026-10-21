#pragma once
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <thread>

namespace mrs::audio {

// Optional, user-triggered sample audition through the existing audio callback.
// Never owns or opens an audio device and never changes the project's transport.
class LibraryPreviewStream final {
public:
    struct Status { bool playing{}; float progress{}; };
    LibraryPreviewStream();
    ~LibraryPreviewStream();
    LibraryPreviewStream(const LibraryPreviewStream&)=delete;
    LibraryPreviewStream& operator=(const LibraryPreviewStream&)=delete;
    void play(std::filesystem::path path,std::uint32_t output_rate,double fraction=0);
    void stop() noexcept;
    Status status() const noexcept;
    void mix(float* interleaved,std::uint32_t frames,std::uint32_t channels,bool allowed) noexcept;
private:
    struct Frame {
        std::uint64_t generation{};
        float left{},right{};
    };
    static constexpr std::size_t ringSize=1u<<15;
    void worker(std::stop_token);
    std::array<Frame,ringSize> ring_{};
    std::atomic<std::uint64_t> read_{0},write_{0},generation_{0},finished_{0};
    std::atomic<std::uint64_t> position_{0},length_{0};
    std::atomic<bool> playing_{false};
    struct Request {
        std::uint64_t generation{};
        std::filesystem::path path;
        std::uint32_t output_rate{};
        double fraction{};
    };
    std::mutex mutex_;
    std::condition_variable_any wake_;
    Request request_;
    std::jthread thread_;
};
} // namespace mrs::audio
