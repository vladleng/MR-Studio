#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
namespace mrs {
struct TimingSample {std::uint64_t calls{},frames{},total_ns{},max_ns{};};
// One DSP owner writes each cell. UI reads independent cumulative atomics:
// an in-flight snapshot is approximate, never a coherent audio-state transaction.
struct TimingCounter {
    std::atomic<std::uint64_t> calls{},frames{},total_ns{},max_ns{};
    void record(std::uint64_t ns,std::uint32_t count) noexcept {
        total_ns.fetch_add(ns,std::memory_order_relaxed);frames.fetch_add(count,std::memory_order_relaxed);
        if(ns>max_ns.load(std::memory_order_relaxed))max_ns.store(ns,std::memory_order_relaxed);
        calls.fetch_add(1,std::memory_order_relaxed);
    }
    TimingSample read() const noexcept {return {calls.load(std::memory_order_relaxed),frames.load(std::memory_order_relaxed),total_ns.load(std::memory_order_relaxed),max_ns.load(std::memory_order_relaxed)};}
};
inline std::uint64_t elapsed_ns(std::chrono::steady_clock::time_point start) noexcept {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count());
}
}
