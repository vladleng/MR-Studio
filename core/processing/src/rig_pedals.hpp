#pragma once
#include <mrs/processing.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace mrs::processing {
namespace {
// Native MVP pedals. No lookahead, auto makeup, oversampling, dry summing or
// runtime ownership. Targets use physical units; 20ms smoothing runs per sample.
class RigPedal final : public IProcessor {
    bool compressor_;
    std::array<float,5> target_{}, current_{};
    double smoothing_{}, attack_{}, release_{}, input_gain_{1}, output_gain_{1}, tone_{1}, clip_scale_{1};
    double reduction_{};
    std::array<double,64> low_{};
    std::uint32_t rate_{48000}, tick_{};
    std::span<const RigParameterDescriptor> descriptor() const noexcept {
        return compressor_ ? std::span<const RigParameterDescriptor>(rig_compressor_parameters)
                           : std::span<const RigParameterDescriptor>(rig_drive_parameters);
    }
    static double clean(double v) noexcept { return std::isfinite(v)&&std::abs(v)>=1e-30?v:0.; }
    void coefficients() noexcept {
        if(compressor_){
            attack_=std::exp(-1./(.001*current_[2]*rate_));
            release_=std::exp(-1./(.001*current_[3]*rate_));
            output_gain_=std::pow(10.,current_[4]/20.);
        }else{
            input_gain_=std::pow(10.,current_[0]/20.);
            output_gain_=std::pow(10.,current_[2]/20.);
            clip_scale_=1./std::tanh(input_gain_);
            tone_=1.-std::exp(-2.*std::numbers::pi*std::min<double>(current_[1],rate_*.45)/rate_);
        }
    }
public:
    explicit RigPedal(bool compressor):compressor_(compressor){for(const auto& p:descriptor())target_[p.id]=p.initial;current_=target_;}
    std::vector<ParameterInfo> parameters() const override {
        std::vector<ParameterInfo> result;
        for(const auto& p:descriptor())result.push_back({p.id,p.minimum,p.maximum,p.initial,true,std::string(p.name)});
        return result;
    }
    void prepare(ProcessConfig config) override {
        if(!config.sample_rate||!config.channels||config.channels>64)throw std::invalid_argument("Invalid pedal device config");
        rate_=config.sample_rate;smoothing_=1.-std::exp(-1./(.02*rate_));reset();warm();
    }
    void restore(const PluginState& state) override {
        if((!state.class_id.empty()&&state.class_id!=(compressor_?"mrs.compressor":"mrs.drive"))||!state.component.empty()||!state.controller.empty())
            throw std::invalid_argument("Rig pedal state uses typed parameters");
    }
    PluginState capture() const override {PluginState state;state.class_id=compressor_?"mrs.compressor":"mrs.drive";return state;}
    bool set_parameter(std::uint32_t id,float value) noexcept override {
        const auto params=descriptor();if(id>=params.size()||!std::isfinite(value)||value<params[id].minimum||value>params[id].maximum)return false;
        target_[id]=value;return true;
    }
    std::optional<float> parameter_value(std::uint32_t id) const noexcept override {return id<descriptor().size()?std::optional<float>(target_[id]):std::nullopt;}
    std::uint32_t latency() const noexcept override{return 0;}
    bool anticipation_safe() const noexcept override{return true;}
    bool live_safe() const noexcept override{return true;}
    void warm() override {current_=target_;tick_=0;coefficients();}
    void reset() noexcept override {reduction_=0;low_.fill(0);tick_=0;}
    void reset_anticipation() noexcept override {reset();warm();}
    void process(ProcessBlock block) noexcept override {
        std::size_t event{};
        const auto count=compressor_?5u:3u;
        for(std::uint32_t frame=0;frame<block.frames;++frame){
            while(event<block.parameters.size()&&block.parameters[event].offset<=frame){const auto& p=block.parameters[event++];(void)set_parameter(p.id,p.value);}
            for(std::size_t p=0;p<count;++p){current_[p]+=static_cast<float>(smoothing_*(target_[p]-current_[p]));if(std::abs(current_[p]-target_[p])<1e-5f)current_[p]=target_[p];}
            // Expensive coefficient functions are bounded to one update per 16 samples.
            if((tick_++&15)==0)coefficients();
            const auto offset=static_cast<std::size_t>(frame)*block.channels;
            double gain=output_gain_;
            if(compressor_){
                double peak{};for(std::uint32_t ch=0;ch<block.channels;++ch)peak=std::max(peak,std::abs(clean(block.audio[offset+ch])));
                const double db=20.*std::log10(std::max(peak,1e-15));
                const double desired=std::max(0.,db-current_[0])*(1.-1./current_[1]);
                const auto coefficient=desired>reduction_?attack_:release_;
                reduction_=clean(coefficient*reduction_+(1.-coefficient)*desired);
                gain*=std::pow(10.,-reduction_/20.);
            }
            for(std::uint32_t ch=0;ch<block.channels;++ch){
                const double input=clean(block.audio[offset+ch]);double value;
                if(compressor_)value=input*gain;
                else{
                    // Symmetric soft clip, then one-pole low-pass tone. Unity peak
                    // at full scale before level; this intentionally adds harmonics.
                    const double clipped=std::tanh(input_gain_*input)*clip_scale_;
                    low_[ch]=clean(low_[ch]+tone_*(clipped-low_[ch]));value=low_[ch]*gain;
                }
                const auto output=static_cast<float>(value);block.audio[offset+ch]=std::isfinite(output)?output:0.f;
            }
        }
        for(auto e:block.midi)(void)block.midi_output.push(e);
    }
};
}
}
