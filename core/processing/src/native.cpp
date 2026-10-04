#include <mrs/processing.hpp>
#include <cmath>
#include <algorithm>
#include <numbers>
#include <cstring>
#include <stdexcept>

namespace mrs::processing {
namespace {
class Gain final : public IProcessor {
    float gain_{1};
public:
    std::vector<ParameterInfo> parameters() const override { return {{0,0,4,1,true}}; }
    void prepare(ProcessConfig) override {}
    void restore(const PluginState& state) override {
        if ((!state.class_id.empty() && state.class_id != "mrs.gain") || !state.controller.empty() ||
            (!state.component.empty() && state.component.size() != sizeof(float)))
            throw std::invalid_argument("invalid native gain state");
        if (!state.component.empty()) {
            float value{};
            std::memcpy(&value,state.component.data(),sizeof(value));
            if (!set_parameter(0,value)) throw std::invalid_argument("invalid saved gain");
        }
    }
    PluginState capture() const override {
        PluginState state; state.class_id = "mrs.gain"; state.component.resize(sizeof(gain_));
        std::memcpy(state.component.data(),&gain_,sizeof(gain_)); return state;
    }
    bool set_parameter(std::uint32_t id, float value) noexcept override {
        if (id != 0 || !std::isfinite(value) || value < 0 || value > 4) return false;
        gain_ = value; return true;
    }
    std::optional<float> parameter_value(std::uint32_t id) const noexcept override {
        return id == 0 ? std::optional<float>{gain_} : std::nullopt;
    }
    std::uint32_t latency() const noexcept override { return 0; }
    bool live_safe() const noexcept override { return true; }
    void warm() override {}
    void reset() noexcept override {}
    void process(ProcessBlock block) noexcept override {
        std::size_t next = 0;
        for (std::uint32_t frame = 0; frame < block.frames; ++frame) {
            while (next < block.parameters.size() && block.parameters[next].offset == frame) {
                const auto& change = block.parameters[next++];
                (void)set_parameter(change.id,change.value);
            }
            const auto offset = static_cast<std::size_t>(frame) * block.channels;
            for (std::uint32_t ch = 0; ch < block.channels; ++ch) block.audio[offset+ch] *= gain_;
        }
        for (auto event : block.midi) (void)block.midi_output.push(event);
    }
};
}
namespace {
class Biquad final : public IProcessor {
    std::string name_;
    bool highpass_{}, eq_{};
    float frequency_{1000}, q_{0.70710678f}, gain_{};
    std::uint32_t rate_{48000};
    double b0_{1},b1_{},b2_{},a1_{},a2_{};
    std::array<double,64> z1_{},z2_{};
    void coefficients() noexcept {
        // RBJ / W3C Audio EQ Cookbook, normalized transposed direct form II.
        const double w=2*std::numbers::pi*std::min<double>(frequency_,rate_*0.45)/rate_;
        const double cosine=std::cos(w), alpha=std::sin(w)/(2*q_), A=std::pow(10.0,gain_/40.0);
        const double a0=eq_ ? 1+alpha/A : 1+alpha;
        b0_=eq_ ? 1+alpha*A : (1+(highpass_ ? cosine : -cosine))/2;
        b1_=eq_ ? -2*cosine : highpass_ ? -(1+cosine) : 1-cosine;
        b2_=eq_ ? 1-alpha*A : b0_;
        a1_=-2*cosine/a0; a2_=(eq_ ? 1-alpha/A : 1-alpha)/a0;
        b0_/=a0; b1_/=a0; b2_/=a0;
    }
public:
    explicit Biquad(std::string name) : name_(std::move(name)), highpass_(name_=="mrs.highpass"), eq_(name_=="mrs.eq") {}
    std::vector<ParameterInfo> parameters() const override {
        std::vector<ParameterInfo> result{{0,20,20000,1000,true},{1,0.1f,10,0.70710678f,true}};
        if (eq_) result.push_back({2,-24,24,0,true}); return result;
    }
    void prepare(ProcessConfig c) override { rate_=c.sample_rate; coefficients(); reset(); }
    void restore(const PluginState& state) override {
        if ((!state.class_id.empty() && state.class_id!=name_) || !state.controller.empty() || (!state.component.empty() && state.component.size()!=3*sizeof(float))) throw std::invalid_argument("invalid native filter state");
        if (!state.component.empty()) {
            std::array<float,3> values{}; std::memcpy(values.data(),state.component.data(),state.component.size());
            if (!set_parameter(0,values[0]) || !set_parameter(1,values[1]) || (eq_ && !set_parameter(2,values[2]))) throw std::invalid_argument("invalid saved filter parameters");
        }
    }
    PluginState capture() const override {
        PluginState state; state.class_id=name_; const std::array<float,3> values{frequency_,q_,gain_};
        state.component.resize(sizeof(values)); std::memcpy(state.component.data(),values.data(),sizeof(values)); return state;
    }
    bool set_parameter(std::uint32_t id,float value) noexcept override {
        if (!std::isfinite(value)) return false;
        if (id==0 && value>=20 && value<=20000) frequency_=value;
        else if (id==1 && value>=0.1f && value<=10) q_=value;
        else if (eq_ && id==2 && value>=-24 && value<=24) gain_=value;
        else return false;
        coefficients(); return true;
    }
    std::optional<float> parameter_value(std::uint32_t id) const noexcept override {
        if (id==0) return frequency_; if (id==1) return q_; if (id==2 && eq_) return gain_; return {};
    }
    std::uint32_t latency() const noexcept override { return 0; }
    bool live_safe() const noexcept override { return true; }
    void warm() override {}
    void reset() noexcept override { z1_.fill(0); z2_.fill(0); }
    void process(ProcessBlock block) noexcept override {
        std::size_t next{};
        for (std::uint32_t f=0;f<block.frames;++f) {
            while (next<block.parameters.size() && block.parameters[next].offset==f) { const auto& p=block.parameters[next++]; (void)set_parameter(p.id,p.value); }
            for (std::uint32_t c=0;c<block.channels;++c) {
                auto& sample=block.audio[static_cast<std::size_t>(f)*block.channels+c];
                const double input=std::isfinite(sample) ? sample : 0, output=b0_*input+z1_[c];
                z1_[c]=b1_*input-a1_*output+z2_[c]; z2_[c]=b2_*input-a2_*output;
                if (!std::isfinite(output) || !std::isfinite(z1_[c]) || !std::isfinite(z2_[c])) { sample=0; z1_[c]=z2_[c]=0; }
                else { sample=static_cast<float>(output); if (std::abs(z1_[c])<1e-30) z1_[c]=0; if (std::abs(z2_[c])<1e-30) z2_[c]=0; }
            }
        }
        for (auto event : block.midi) (void)block.midi_output.push(event);
    }
};
}
std::unique_ptr<IProcessor> native_factory(const NodeState& state) {
    if (state.format == ProcessorFormat::native && (state.processor_id=="mrs.highpass" || state.processor_id=="mrs.lowpass" || state.processor_id=="mrs.eq")) return std::make_unique<Biquad>(state.processor_id);
    if (state.format != ProcessorFormat::native || state.processor_id != "mrs.gain")
        throw std::invalid_argument("processor unavailable; VST3 hosting belongs to MRS Stage 3");
    return std::make_unique<Gain>();
}
} // namespace mrs::processing
