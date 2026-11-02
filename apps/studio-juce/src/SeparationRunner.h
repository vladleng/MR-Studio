#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <thread>

// NON-RT only. Owner/control thread starts/drains; a private thread owns all
// filesystem, Python and IPC work. No audio engine/model/GUI pointers captured.
namespace mr::separation {
enum class State { idle, running, ready, failed, cancelled };
enum class Device { cpu, cuda, automatic };
struct Request {
    juce::File source, modelDirectory;
    juce::String sessionId, sourceRevision;
    juce::int64 sourceStart{}, sourceFrames{}, timelineStart{};
    int projectRate{48000};
    Device device{Device::cpu};
    std::chrono::milliseconds timeout{std::chrono::minutes(30)};
    juce::uint64 memoryBytes{8ULL * 1024 * 1024 * 1024};
};
struct Configuration {
    juce::File python, workerScript, temporaryRoot;
};
struct Status {
    State state{State::idle};
    juce::String jobId, phase, error, effectiveDevice;
};
struct Result {
    juce::String jobId, sessionId, sourceRevision;
    juce::File directory; // caller owns successful cache; not a durable project asset
    juce::var manifest;
};
class Runner {
public:
    explicit Runner(Configuration);
    ~Runner(); // cancel/kill/join; never call from audio callback
    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;
    bool start(Request); // one outstanding result/job; no Python at construction
    void cancel() noexcept;
    Status status() const; // bounded coalesced state, no backend callbacks
    std::optional<Result> take();
private:
    void run(Request, juce::String jobId) noexcept;
    Configuration config_;
    mutable std::mutex mutex_;
    Status status_;
    std::optional<Result> result_;
    std::atomic<bool> cancelled_{};
    std::thread thread_;
};
} // namespace mr::separation
