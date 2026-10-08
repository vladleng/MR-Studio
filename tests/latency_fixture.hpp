#pragma once
#include <mrs/processing.hpp>
#include <algorithm>
#include <utility>
// Synthetic real DSP delay, independent of the host compensation implementation.
class LatencyFixture final : public mrs::processing::IProcessor {
    std::uint32_t latency_;std::vector<float> ring_;std::size_t cursor_{};
public:
    explicit LatencyFixture(std::uint32_t latency):latency_(latency){}
    std::vector<mrs::processing::ParameterInfo> parameters() const override{return {};}
    void prepare(mrs::processing::ProcessConfig c) override{ring_.assign(static_cast<std::size_t>(latency_)*c.channels,0.f);cursor_=0;}
    void restore(const mrs::processing::PluginState&) override{}
    mrs::processing::PluginState capture() const override{return {};}
    bool set_parameter(std::uint32_t,float) noexcept override{return false;}
    std::optional<float> parameter_value(std::uint32_t) const noexcept override{return {};}
    std::uint32_t latency() const noexcept override{return latency_;}
    bool live_safe() const noexcept override{return true;}
    bool anticipation_safe() const noexcept override{return true;}
    void warm() override{}
    void reset() noexcept override{std::fill(ring_.begin(),ring_.end(),0.f);cursor_=0;}
    void process(mrs::processing::ProcessBlock block) noexcept override{if(ring_.empty())return;for(auto& sample:block.audio){sample=std::exchange(ring_[cursor_],sample);if(++cursor_==ring_.size())cursor_=0;}}
};
