#pragma once
#include <cstdint>
#include <memory>
namespace mrs::audio {
// Prepared on the control thread; a single callback submits one bounded batch.
// Jobs own disjoint channel scratch. Timeout quarantines the batch until teardown.
class ChannelWorkers {
public:
    using Job = void (*)(void*, std::uint32_t) noexcept;
    explicit ChannelWorkers(std::uint32_t helpers, bool mmcss = true);
    ~ChannelWorkers();
    ChannelWorkers(const ChannelWorkers&) = delete;
    ChannelWorkers& operator=(const ChannelWorkers&) = delete;
    bool run(void*, Job, std::uint32_t jobs, std::uint32_t timeout_ms) noexcept;
    void quiesce() noexcept; // control thread, after device callback stops
    std::uint32_t count() const noexcept;
    std::uint32_t audio_scheduled() const noexcept;
    static std::uint32_t participant() noexcept; // 0 caller, 1..7 helpers, scalar TLS
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
