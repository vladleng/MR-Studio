#include <mrs/channel_workers.hpp>
#include <mrs/no_denormals.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <semaphore>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <avrt.h>
#endif
namespace mrs::audio {
namespace {
thread_local std::uint32_t participant_index{};
class Signal {
#ifdef _WIN32
    HANDLE handle_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
public:
    Signal() { if (!handle_) throw std::runtime_error("audio worker event creation failed"); }
    ~Signal() { CloseHandle(handle_); }
    void post() noexcept { SetEvent(handle_); }
    void wait() noexcept { WaitForSingleObject(handle_, INFINITE); }
    bool wait_for(std::uint32_t ms) noexcept { return WaitForSingleObject(handle_, ms)==WAIT_OBJECT_0; }
#else
    std::binary_semaphore signal_{0};
public:
    void post() noexcept { signal_.release(); }
    void wait() noexcept { signal_.acquire(); }
    bool wait_for(std::uint32_t ms) noexcept { return signal_.try_acquire_for(std::chrono::milliseconds(ms)); }
#endif
};
}
struct ChannelWorkers::Impl {
    std::array<Signal,7> wake;
    Signal done, started;
    std::array<std::thread,7> threads;
    std::uint32_t helpers{};
    std::atomic<bool> stop{};
    std::atomic<std::uint32_t> next{}, remaining{}, scheduled{};
    std::uint32_t jobs{};
    void* context{};
    Job job{};
    bool quarantined{};
    void consume() noexcept {
        for (auto index=next.fetch_add(1,std::memory_order_acq_rel); index<jobs;
             index=next.fetch_add(1,std::memory_order_acq_rel)) job(context,index);
    }
    void shutdown() noexcept {
        stop.store(true,std::memory_order_release);
        for(std::uint32_t i=0;i<helpers;++i) wake[i].post();
        for(std::uint32_t i=0;i<helpers;++i) if(threads[i].joinable()) threads[i].join();
        helpers=0;
    }
    ~Impl() { shutdown(); }
};
ChannelWorkers::ChannelWorkers(std::uint32_t helpers,bool mmcss):impl_(std::make_unique<Impl>()) {
    if(helpers>7) throw std::invalid_argument("audio worker limit is 1..8 including callback");
    for(std::uint32_t i=0;i<helpers;++i) {
        impl_->threads[i]=std::thread([this,i,mmcss] {
            participant_index=i+1;
#ifdef _WIN32
            DWORD task{};
            HANDLE audio=mmcss?AvSetMmThreadCharacteristicsW(L"Pro Audio",&task):nullptr;
            if(audio) impl_->scheduled.fetch_add(1,std::memory_order_relaxed);
#else
            (void)mmcss;
#endif
            impl_->started.post();
            while(true) {
                impl_->wake[i].wait();
                if(impl_->stop.load(std::memory_order_acquire)) break;
                { const ScopedNoDenormals fp; impl_->consume(); }
                if(impl_->remaining.fetch_sub(1,std::memory_order_acq_rel)==1) impl_->done.post();
            }
#ifdef _WIN32
            if(audio) AvRevertMmThreadCharacteristics(audio);
#endif
        });
        ++impl_->helpers;
        impl_->started.wait(); // startup/scheduling is outside RT
    }
}
ChannelWorkers::~ChannelWorkers()=default;
void ChannelWorkers::quiesce() noexcept {
    if(impl_->quarantined){impl_->done.wait();(void)impl_->remaining.load(std::memory_order_acquire);impl_->quarantined=false;}
}
std::uint32_t ChannelWorkers::count() const noexcept {return impl_->helpers;}
std::uint32_t ChannelWorkers::audio_scheduled() const noexcept {return impl_->scheduled.load(std::memory_order_relaxed);}
std::uint32_t ChannelWorkers::participant() noexcept {return participant_index;}
bool ChannelWorkers::run(void* context,Job job,std::uint32_t jobs,std::uint32_t timeout_ms) noexcept {
    if(impl_->quarantined) return false;
    if(jobs<2 || !impl_->helpers) {for(std::uint32_t i=0;i<jobs;++i)job(context,i);return true;}
    impl_->context=context;impl_->job=job;impl_->jobs=jobs;
    impl_->next.store(1,std::memory_order_release);
    const auto active=std::min(impl_->helpers,jobs-1);
    impl_->remaining.store(active,std::memory_order_relaxed);
    for(std::uint32_t i=0;i<active;++i)impl_->wake[i].post();
    job(context,0);impl_->consume(); // reserve first job for callback, other indices claimed once
    if(impl_->done.wait_for(timeout_ms)) { (void)impl_->remaining.load(std::memory_order_acquire);return true; }
    impl_->quarantined=true; // never retry or reuse mutable plugin/buffers
    return false;
}
}
