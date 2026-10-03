#include <mrs/recording.hpp>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace mrs::audio {
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<int>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
namespace {
void put16(std::ostream& out, std::uint16_t n) { for (int i=0; i<2; ++i) out.put(static_cast<char>((n>>(8*i))&255)); }
void put32(std::ostream& out, std::uint32_t n) { for (int i=0; i<4; ++i) out.put(static_cast<char>((n>>(8*i))&255)); }
constexpr std::uint64_t maximum_frames = (std::numeric_limits<std::uint32_t>::max()-36ULL)/4;
}
Recorder::Recorder(std::filesystem::path destination, std::uint32_t rate, Sample start)
    : destination_(std::filesystem::absolute(destination)), temporary_(destination_), rate_(rate), start_(start) {
    if (rate < 8000 || rate > 768000 || start < 0 || start > max_sample)
        throw std::invalid_argument("invalid recording rate/position");
    temporary_ += "."+new_id().value+".partial";
    if (std::filesystem::exists(destination_) || std::filesystem::exists(temporary_))
        throw std::invalid_argument("recording destination already exists; choose a new take name");
    output_.open(temporary_,std::ios::binary | std::ios::trunc);
    if (!output_) throw std::runtime_error("cannot create recording file");
    header(0); output_.seekp(44);
    if (!output_) throw std::runtime_error("cannot write recording header");
    worker_ = std::thread([this] { run(); });
}
Recorder::~Recorder() { quit_ = true; if (worker_.joinable()) worker_.join(); }
void Recorder::fail(RecordFault value) noexcept {
    int expected = 0; (void)fault_.compare_exchange_strong(expected,static_cast<int>(value),std::memory_order_relaxed);
}
void Recorder::input_dropout() noexcept { missing_.fetch_add(1,std::memory_order_relaxed); fail(RecordFault::missing_input); }
void Recorder::capture(const float* input, std::uint32_t channels, std::uint32_t frames, Sample position) noexcept {
    if (fault_.load(std::memory_order_relaxed) || quit_.load(std::memory_order_relaxed)) return;
    if (!input || channels == 0) { input_dropout(); return; }
    const auto write = write_.load(std::memory_order_relaxed);
    if (position < start_ || static_cast<std::uint64_t>(position-start_) != write) {
        jumps_.fetch_add(1,std::memory_order_relaxed); fail(RecordFault::discontinuity); return;
    }
    if (write > maximum_frames || frames > maximum_frames-write ||
        static_cast<std::uint64_t>(max_sample-start_) < write+frames) { fail(RecordFault::size_limit); return; }
    const auto read = read_.load(std::memory_order_acquire);
    if (frames > capacity || write-read > capacity-frames) {
        dropped_.fetch_add(1,std::memory_order_relaxed); fail(RecordFault::overflow); return;
    }
    for (std::uint32_t f=0; f<frames; ++f) {
        auto value = input[static_cast<std::size_t>(f)*channels];
        if (!std::isfinite(value)) { value = 0; nonfinite_.fetch_add(1,std::memory_order_relaxed); }
        ring_[static_cast<std::size_t>((write+f)%capacity)] = value;
    }
    write_.store(write+frames,std::memory_order_release);
}
RecordStatus Recorder::status() const noexcept {
    return {write_.load(),dropped_.load(),missing_.load(),jumps_.load(),nonfinite_.load(),static_cast<RecordFault>(fault_.load())};
}
void Recorder::header(std::uint32_t samples) {
    output_.seekp(0); output_.write("RIFF",4); put32(output_,36+samples*4); output_.write("WAVEfmt ",8);
    put32(output_,16); put16(output_,3); put16(output_,1); put32(output_,rate_); put32(output_,rate_*4);
    put16(output_,4); put16(output_,32); output_.write("data",4); put32(output_,samples*4);
}
void Recorder::run() noexcept {
    // Worker staging/encoding storage never belongs to the callback.
    std::array<unsigned char,8192*4> bytes{};
    try {
        for (;;) {
            const auto read = read_.load(std::memory_order_relaxed);
            const auto write = write_.load(std::memory_order_acquire);
            const auto count = std::min<std::uint64_t>(8192,write-read);
            if (!count) {
                if (quit_.load()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue;
            }
            for (std::size_t n=0; n<count; ++n) {
                const auto bits = std::bit_cast<std::uint32_t>(ring_[static_cast<std::size_t>((read+n)%capacity)]);
                for (int b=0; b<4; ++b) bytes[n*4+static_cast<std::size_t>(b)] = static_cast<unsigned char>((bits>>(8*b))&255);
            }
            output_.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(count*4));
            if (!output_) { fault_ = static_cast<int>(RecordFault::disk); return; }
            written_ += count; read_.store(read+count,std::memory_order_release);
        }
    } catch (...) { fault_ = static_cast<int>(RecordFault::disk); }
}
RecordedFile Recorder::finish() {
    if (finished_) throw std::logic_error("recording already finalized");
    quit_ = true; if (worker_.joinable()) worker_.join(); finished_ = true;
    auto state = status();
    if (state.fault == RecordFault::disk) { output_.close(); throw std::runtime_error("recording disk write failed; partial file: "+temporary_.string()); }
    header(static_cast<std::uint32_t>(written_)); output_.flush();
    if (!output_) { output_.close(); throw std::runtime_error("cannot finalize recording header; partial file: "+temporary_.string()); }
    output_.close();
    if (!written_) { std::filesystem::remove(temporary_); return {{},0,state}; }
    if (std::filesystem::exists(destination_)) throw std::runtime_error("recording destination appeared; partial file retained");
#ifdef _WIN32
    // Atomic publication without overwriting an existing take.
    if (!MoveFileExW(temporary_.c_str(),destination_.c_str(),MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("cannot publish recording; partial file retained");
#else
    std::filesystem::create_hard_link(temporary_,destination_);
    std::filesystem::remove(temporary_);
#endif
    return {destination_,static_cast<Sample>(written_),state};
}
}
