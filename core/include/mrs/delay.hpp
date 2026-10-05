#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
namespace mrs {
// Prepared on the control thread; sample(), reset() never allocate or clear buffers.
class CompensationDelay {
    std::vector<float> audio_;
    std::vector<std::uint64_t> stamps_;
    std::uint64_t epoch_{1};
    std::size_t cursor_{};
    std::uint32_t channels_{};
public:
    void prepare(std::uint64_t frames,std::uint32_t channels,std::size_t& budget) {
        if(frames>262144)throw std::invalid_argument("PDC path exceeds 262144 samples");
        const auto bytes=static_cast<std::size_t>(frames)*(channels*sizeof(float)+sizeof(std::uint64_t));
        if(bytes>budget)throw std::invalid_argument("PDC memory budget exceeded");
        budget-=bytes;channels_=channels;cursor_=0;epoch_=1;
        audio_.assign(static_cast<std::size_t>(frames)*channels,0.f);stamps_.assign(static_cast<std::size_t>(frames),0);
    }
    float sample(float input,std::uint32_t channel) noexcept {
        if(stamps_.empty())return input;
        const auto index=cursor_*channels_+channel;
        const float output=stamps_[cursor_]==epoch_?audio_[index]:0.f;audio_[index]=input;
        if(channel+1==channels_){stamps_[cursor_]=epoch_;if(++cursor_==stamps_.size())cursor_=0;}
        return output;
    }
    void reset() noexcept {++epoch_;cursor_=0;}
};
}
