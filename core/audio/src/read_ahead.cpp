#include <mrs/read_ahead.hpp>
#include <algorithm>
#include <chrono>
#include <stdexcept>
namespace mrs::audio {
ReadAhead::ReadAhead(std::shared_ptr<const WavFile> file) : file_(std::move(file)) {
    for (auto& slot : slots_) slot.samples.resize(static_cast<std::size_t>(page_frames)*file_->channels);
    worker_ = std::thread([this] { run(); });
}
ReadAhead::~ReadAhead() { quit_ = true; if (worker_.joinable()) worker_.join(); }
void ReadAhead::loop(Sample first) noexcept { loop_.store(first < 0 ? -1 : first/page_frames,std::memory_order_release); }
void ReadAhead::begin(Sample first) noexcept {
    desired_.store(std::clamp(first,Sample{0},file_->frame_count-1)/page_frames,std::memory_order_release);
    auto warm = desired_.load(std::memory_order_relaxed);
    (void)warm_.compare_exchange_strong(warm,-1,std::memory_order_acq_rel);
    missed_ = false;
    for (std::size_t i = 0; i < pages; ++i) {
        int expected = 0;
        pinned_[i] = slots_[i].owner.compare_exchange_strong(expected,1,std::memory_order_acquire);
    }
}
bool ReadAhead::read(Sample frame, std::uint32_t channel, float& value) noexcept {
    const auto page = frame/page_frames;
    for (std::size_t i = 0; i < pages; ++i) if (pinned_[i] && slots_[i].page == page) {
        value = slots_[i].samples[static_cast<std::size_t>(frame%page_frames)*file_->channels+channel];
        return true;
    }
    desired_.store(page,std::memory_order_release); missed_ = true; value = 0; return false;
}
bool ReadAhead::end() noexcept {
    for (std::size_t i = 0; i < pages; ++i) if (pinned_[i]) {
        slots_[i].owner.store(0,std::memory_order_release); pinned_[i] = false;
    }
    return missed_;
}
void ReadAhead::prime(Sample first) {
    const auto target = std::clamp(first,Sample{0},file_->frame_count-1)/page_frames;
    warm_.store(target,std::memory_order_release);
    const auto until = std::chrono::steady_clock::now()+std::chrono::seconds(5);
    for (;;) {
        bool ready = true;
        for (Sample p = target; p < target+4 && p*page_frames < file_->frame_count; ++p) {
            bool found = false;
            // Control never accesses samples/page without owning the slot.
            for (auto& slot : slots_) {
                int expected = 0;
                if (slot.owner.compare_exchange_strong(expected,1,std::memory_order_acquire)) {
                    found = found || slot.page == p; slot.owner.store(0,std::memory_order_release);
                }
            }
            ready = ready && found;
        }
        if (ready) { desired_ = target; return; }
        if (errors_.load() || std::chrono::steady_clock::now() >= until) {
            warm_ = -1; throw std::runtime_error("disk read-ahead failed: media unavailable, invalid samples or timeout");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
void ReadAhead::run() noexcept {
    while (!quit_.load()) {
        std::array<Sample,pages> wanted{}; wanted.fill(-1);
        std::size_t count{};
        const auto add = [&](Sample page) {
            if (page < 0 || page*page_frames >= file_->frame_count ||
                std::find(wanted.begin(),wanted.begin()+static_cast<std::ptrdiff_t>(count),page) != wanted.begin()+static_cast<std::ptrdiff_t>(count)) return;
            if (count < pages) wanted[count++] = page;
        };
        const auto warm = warm_.load(std::memory_order_acquire);
        const auto current = desired_.load(std::memory_order_acquire);
        const auto loop = loop_.load(std::memory_order_acquire);
        if (warm >= 0) for (Sample n = 0; n < 4; ++n) add(warm+n);
        for (Sample n = 0; n < (warm >= 0 ? 2 : 4); ++n) add(current+n);
        if (loop >= 0) { add(loop); add(loop+1); }
        for (std::size_t p = 0; p < count && !quit_.load(); ++p) {
            bool exists = false;
            // Only worker writes page tags; pinned tags also remain immutable.
            for (auto& slot : slots_) if (slot.page == wanted[p]) exists = true;
            if (exists) continue;
            for (auto& slot : slots_) {
                if (std::find(wanted.begin(),wanted.begin()+static_cast<std::ptrdiff_t>(count),slot.page) != wanted.begin()+static_cast<std::ptrdiff_t>(count)) continue;
                int expected = 0;
                if (!slot.owner.compare_exchange_strong(expected,-1,std::memory_order_acquire)) continue;
                try {
                    const auto first = wanted[p]*page_frames;
                    const auto frames = std::min(page_frames,file_->frame_count-first);
                    file_->read(first,std::span<float>(slot.samples).first(static_cast<std::size_t>(frames)*file_->channels));
                    slot.page = wanted[p];
                } catch (...) { slot.page = -1; errors_.fetch_add(1); quit_ = true; }
                slot.owner.store(0,std::memory_order_release); break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
}
