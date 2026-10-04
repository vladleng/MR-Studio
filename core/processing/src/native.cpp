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
namespace {
class ChannelEq final : public IProcessor {
    std::array<Biquad,5> filters_{{Biquad{"mrs.highpass"},Biquad{"mrs.eq"},Biquad{"mrs.eq"},Biquad{"mrs.eq"},Biquad{"mrs.lowpass"}}};
    std::array<EqBand,5> targets_=NativeInsert{}.bands, current_=targets_;
    std::array<float,5> wet_{};
    float smoothing_{0.001f}; std::uint32_t tick_{};
    void update() noexcept { for(std::size_t i=0;i<5;++i) { (void)filters_[i].set_parameter(0,current_[i].frequency);(void)filters_[i].set_parameter(1,current_[i].q);if(i>0 && i<4)(void)filters_[i].set_parameter(2,current_[i].gain); } }
public:
    std::vector<ParameterInfo> parameters() const override { std::vector<ParameterInfo> p;for(std::uint32_t i=0;i<5;++i){p.push_back({i*4,20,20000,targets_[i].frequency,true});p.push_back({i*4+1,0.1f,10,0.70710678f,true});p.push_back({i*4+2,-24,24,0,true});p.push_back({i*4+3,0,1,targets_[i].enabled?1.f:0.f,true});}return p; }
    void prepare(ProcessConfig c) override {for(auto& f:filters_)f.prepare(c);smoothing_=1-std::exp(-1.f/(0.02f*c.sample_rate));warm();}
    void restore(const PluginState& state) override {if(!state.component.empty() || !state.controller.empty())throw std::invalid_argument("EQ state uses parameters");}
    PluginState capture() const override {PluginState s;s.class_id="mrs.channel-eq";return s;}
    bool set_parameter(std::uint32_t id,float v) noexcept override {if(id>=20 || !std::isfinite(v))return false;auto& b=targets_[id/4];switch(id%4){case 0:if(v<20||v>20000)return false;b.frequency=v;break;case 1:if(v<0.1f||v>10)return false;b.q=v;break;case 2:if(v<-24||v>24)return false;b.gain=v;break;case 3:if(v<0||v>1)return false;b.enabled=v>=0.5f;break;}return true;}
    std::optional<float> parameter_value(std::uint32_t id) const noexcept override {if(id>=20)return {};const auto& b=targets_[id/4];switch(id%4){case 0:return b.frequency;case 1:return b.q;case 2:return b.gain;default:return b.enabled?1.f:0.f;}}
    std::uint32_t latency() const noexcept override{return 0;} bool live_safe() const noexcept override{return true;}
    void warm() override {current_=targets_;for(std::size_t i=0;i<5;++i)wet_[i]=targets_[i].enabled?1.f:0.f;update();}
    void reset() noexcept override {for(auto& f:filters_)f.reset();}
    void process(ProcessBlock block) noexcept override {
        std::size_t next{};MidiBuffer discarded;
        for(std::uint32_t frame=0;frame<block.frames;++frame){
            while(next<block.parameters.size() && block.parameters[next].offset==frame){const auto& p=block.parameters[next++];(void)set_parameter(p.id,p.value);}
            for(std::size_t i=0;i<5;++i){auto& b=current_[i];const auto& t=targets_[i];b.frequency+=smoothing_*(t.frequency-b.frequency);b.q+=smoothing_*(t.q-b.q);b.gain+=smoothing_*(t.gain-b.gain);wet_[i]+=smoothing_*((t.enabled?1.f:0.f)-wet_[i]);}
            if((tick_++ & 15)==0)update();
            auto samples=block.audio.subspan(static_cast<std::size_t>(frame)*block.channels,block.channels);
            for(std::size_t i=0;i<5;++i){std::array<float,64> dry{};std::copy(samples.begin(),samples.end(),dry.begin());filters_[i].process({samples,1,block.channels,{},{},discarded});for(std::uint32_t c=0;c<block.channels;++c)samples[c]=dry[c]+wet_[i]*(samples[c]-dry[c]);}
        }
        for(auto e:block.midi)(void)block.midi_output.push(e);
    }
};
}
double eq_response_db(const NativeInsert& fx,double frequency,std::uint32_t rate) {
    if(fx.bypass)return 0;
    double magnitude=1;const double phase=2*std::numbers::pi*frequency/rate;
    for(std::size_t i=0;i<5;++i){const auto& b=fx.bands[i];if(!b.enabled)continue;
        const double w=2*std::numbers::pi*std::min<double>(b.frequency,rate*0.45)/rate,c=std::cos(w),alpha=std::sin(w)/(2*b.q),A=std::pow(10.,b.gain/40);const bool eq=i>0&&i<4,hp=i==0;
        const double a0=eq?1+alpha/A:1+alpha,b0=(eq?1+alpha*A:(1+(hp?c:-c))/2)/a0,b1=(eq?-2*c:hp?-(1+c):1-c)/a0,b2=(eq?1-alpha*A:(1+(hp?c:-c))/2)/a0,a1=-2*c/a0,a2=(eq?1-alpha/A:1-alpha)/a0;
        const auto norm=[&](double x,double y,double z){const double re=x+y*std::cos(phase)+z*std::cos(2*phase),im=-y*std::sin(phase)-z*std::sin(2*phase);return re*re+im*im;};magnitude*=std::sqrt(norm(b0,b1,b2)/std::max(1e-30,norm(1,a1,a2)));
    }return 20*std::log10(std::max(magnitude,1e-12));
}
std::unique_ptr<IProcessor> native_factory(const NodeState& state) {
    if (state.format==ProcessorFormat::native && state.processor_id=="mrs.channel-eq") return std::make_unique<ChannelEq>();
    if (state.format == ProcessorFormat::native && (state.processor_id=="mrs.highpass" || state.processor_id=="mrs.lowpass" || state.processor_id=="mrs.eq")) return std::make_unique<Biquad>(state.processor_id);
    if (state.format != ProcessorFormat::native || state.processor_id != "mrs.gain")
        throw std::invalid_argument("processor unavailable; VST3 hosting belongs to MRS Stage 3");
    return std::make_unique<Gain>();
}
} // namespace mrs::processing
