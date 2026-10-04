#include <mrs/processing.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstring>
#include <numbers>
#include <stdexcept>

namespace mrs::processing {
namespace {
// The first 128 taps are direct. The tail begins at tap 128, exactly
// when its first FFT block becomes available: no additional block latency.
constexpr std::size_t partition=128, fft_size=partition*2;
using Complex=std::complex<float>;
struct Header {std::uint32_t magic,rate,channels,samples;};
class CabProcessor final : public IProcessor {
    ProcessConfig config_;
    PluginState original_;
    std::array<float,5> target_{1,1,20,20000,0},current_{target_};
    float smoothing_{},hp_{},lp_{};std::uint64_t tick_{};
    std::size_t partitions_{},cursor_{},head_pos_{},block_pos_{};
    std::vector<float> head_,history_,input_,output_,overlap_;
    std::vector<Complex> kernels_,spectra_;
    std::array<Complex,fft_size> work_{},sum_{},roots_{};
    std::array<std::size_t,fft_size> reverse_{};
    std::array<float,64> previous_input_{},hp_state_{},lp_state_{};
    void fft(std::array<Complex,fft_size>& data,bool inverse) noexcept {
        for(std::size_t i=0;i<fft_size;++i)if(reverse_[i]>i)std::swap(data[i],data[reverse_[i]]);
        for(std::size_t length=2;length<=fft_size;length*=2)for(std::size_t start=0;start<fft_size;start+=length)for(std::size_t j=0;j<length/2;++j){const auto root=roots_[j*fft_size/length];const auto b=data[start+j+length/2]*(inverse?std::conj(root):root),a=data[start+j];data[start+j]=a+b;data[start+j+length/2]=a-b;}
        if(inverse)for(auto& v:data)v/=static_cast<float>(fft_size);
    }
    void tail_block() noexcept {
        if(!partitions_)return;
        for(std::size_t ch=0;ch<config_.channels;++ch){
            work_.fill({});for(std::size_t j=0;j<partition;++j)work_[j]=input_[ch*partition+j];fft(work_,false);
            const auto base=ch*partitions_*fft_size;std::copy(work_.begin(),work_.end(),spectra_.begin()+static_cast<std::ptrdiff_t>(base+cursor_*fft_size));sum_.fill({});
            for(std::size_t p=0;p<partitions_;++p){const auto x=base+((cursor_+partitions_-p)%partitions_)*fft_size,k=base+p*fft_size;for(std::size_t bin=0;bin<fft_size;++bin)sum_[bin]+=spectra_[x+bin]*kernels_[k+bin];}
            fft(sum_,true);for(std::size_t j=0;j<partition;++j){output_[ch*partition+j]=sum_[j].real()+overlap_[ch*partition+j];overlap_[ch*partition+j]=sum_[j+partition].real();}
        }
        cursor_=(cursor_+1)%partitions_;
    }
public:
    std::vector<ParameterInfo> parameters() const override{return {{0,0,4,1,true},{1,0,1,1,true},{2,20,20000,20,true},{3,20,20000,20000,true},{4,0,1,0,true}};}
    void prepare(ProcessConfig config) override{
        if(config.sample_rate>192000)throw std::invalid_argument("Cab IR runtime supports rates up to 192 kHz");
        config_=config;smoothing_=1-std::exp(-1.f/(0.02f*config.sample_rate));
        for(std::size_t i=0;i<fft_size;++i){std::size_t r{},v=i;for(int b=0;b<8;++b){r=r*2+(v&1);v>>=1;}reverse_[i]=r;const auto angle=-2*std::numbers::pi*static_cast<double>(i)/fft_size;roots_[i]={static_cast<float>(std::cos(angle)),static_cast<float>(std::sin(angle))};}
    }
    // Decode, resample and allocate only while the graph is quiescent.
    void restore(const PluginState& state) override {
        if(state.class_id!="mrs.cab-ir"||state.component.size()<sizeof(Header)||!state.controller.empty())throw std::invalid_argument("invalid Cab IR state");
        Header h{};std::memcpy(&h,state.component.data(),sizeof(h));
        if(h.magic!=0x31524943||h.rate<8000||h.rate>192000||(h.channels!=1&&h.channels!=2)||!h.samples||h.samples>h.rate*h.channels||h.samples%h.channels||state.component.size()!=sizeof(h)+h.samples*sizeof(float))throw std::invalid_argument("invalid Cab IR kernel");
        std::vector<float> source(h.samples);std::memcpy(source.data(),state.component.data()+sizeof(h),h.samples*sizeof(float));
        for(float v:source)if(!std::isfinite(v)||std::abs(v)>16)throw std::invalid_argument("invalid Cab IR kernel sample");
        original_=state;const auto source_frames=h.samples/h.channels;
        const auto frames=static_cast<std::size_t>((static_cast<std::uint64_t>(source_frames)*config_.sample_rate+h.rate-1)/h.rate);
        std::vector<float> resampled(frames*h.channels);
        const double ratio=static_cast<double>(h.rate)/config_.sample_rate,cutoff=std::min(1.,1./ratio);
        for(std::size_t f=0;f<frames;++f)for(std::size_t ch=0;ch<h.channels;++ch){
            if(h.rate==config_.sample_rate){resampled[f*h.channels+ch]=source[f*h.channels+ch];continue;}
            const double position=f*ratio;const auto center=static_cast<std::int64_t>(position);double value{};
            for(std::int64_t k=center-32;k<=center+32;++k){if(k<0||k>=source_frames)continue;const double d=position-k;if(std::abs(d)>=32)continue;const double x=std::numbers::pi*d*cutoff,sinc=std::abs(x)<1e-12?1.:std::sin(x)/x,window=0.5+0.5*std::cos(std::numbers::pi*d/32);value+=source[static_cast<std::size_t>(k)*h.channels+ch]*sinc*window*cutoff*ratio;}
            resampled[f*h.channels+ch]=static_cast<float>(value);
        }
        partitions_=frames>partition?(frames-partition+partition-1)/partition:0;
        const auto bins=partitions_*fft_size*config_.channels;
        if(bins*sizeof(Complex)*2>64*1024*1024)throw std::invalid_argument("Cab IR scratch budget exceeded; reduce output channels or IR duration");
        head_.assign(config_.channels*partition,0);history_=head_;input_=head_;output_=head_;overlap_=head_;kernels_.assign(bins,{});spectra_.assign(bins,{});
        for(std::size_t ch=0;ch<config_.channels;++ch){const auto irch=h.channels==1?0:ch%2;for(std::size_t f=0;f<std::min(frames,partition);++f)head_[ch*partition+f]=resampled[f*h.channels+irch];
            for(std::size_t p=0;p<partitions_;++p){work_.fill({});for(std::size_t j=0;j<partition;++j){const auto f=partition+p*partition+j;if(f<frames)work_[j]=resampled[f*h.channels+irch];}fft(work_,false);std::copy(work_.begin(),work_.end(),kernels_.begin()+static_cast<std::ptrdiff_t>((ch*partitions_+p)*fft_size));}}
        reset();
    }
    PluginState capture() const override{return original_;}
    bool set_parameter(std::uint32_t id,float value) noexcept override{
        if(id>=5||!std::isfinite(value)||value<(id==2||id==3?20.f:0.f)||value>(id==0?4.f:id==2||id==3?20000.f:1.f))return false;target_[id]=value;return true;
    }
    std::optional<float> parameter_value(std::uint32_t id) const noexcept override{return id<5?std::optional<float>{target_[id]}:std::nullopt;}
    std::uint32_t latency() const noexcept override{return 0;} // direct head hides tail partition delay
    bool live_safe() const noexcept override{return true;}
    void warm() override{reset();}
    void reset() noexcept override{for(auto* v:{&history_,&input_,&output_,&overlap_})std::fill(v->begin(),v->end(),0.f);std::fill(spectra_.begin(),spectra_.end(),Complex{});previous_input_.fill(0);hp_state_.fill(0);lp_state_.fill(0);current_=target_;cursor_=head_pos_=block_pos_=0;tick_=0;}
    void process(ProcessBlock block) noexcept override {
        std::size_t next{};
        for(std::uint32_t f=0;f<block.frames;++f){
            while(next<block.parameters.size()&&block.parameters[next].offset==f){const auto& p=block.parameters[next++];(void)set_parameter(p.id,p.value);}
            for(std::size_t p=0;p<5;++p)current_[p]+=smoothing_*(target_[p]-current_[p]);
            if((tick_++&31)==0){hp_=std::exp(-2*std::numbers::pi_v<float>*std::min(current_[2],config_.sample_rate*0.45f)/config_.sample_rate);lp_=1-std::exp(-2*std::numbers::pi_v<float>*std::min(current_[3],config_.sample_rate*0.45f)/config_.sample_rate);}
            for(std::size_t ch=0;ch<block.channels;++ch){const auto index=static_cast<std::size_t>(f)*block.channels+ch;const float dry=std::isfinite(block.audio[index])?block.audio[index]:0;history_[ch*partition+head_pos_]=dry;input_[ch*partition+block_pos_]=dry;
                double wet=output_[ch*partition+block_pos_];for(std::size_t j=0;j<partition;++j)wet+=head_[ch*partition+j]*history_[ch*partition+(head_pos_+partition-j)%partition];
                const float v=static_cast<float>(wet);hp_state_[ch]=hp_*(hp_state_[ch]+v-previous_input_[ch]);previous_input_[ch]=v;const float high=current_[2]<=20.001f?v:hp_state_[ch];lp_state_[ch]+=lp_*(high-lp_state_[ch]);float filtered=current_[3]>=19999.9f?high:lp_state_[ch];filtered*=current_[0]*(1-2*current_[4]);
                float result=dry+current_[1]*(filtered-dry);if(!std::isfinite(result)||std::abs(result)<1e-25f)result=0;block.audio[index]=result;
                if(std::abs(hp_state_[ch])<1e-25f)hp_state_[ch]=0;if(std::abs(lp_state_[ch])<1e-25f)lp_state_[ch]=0;
            }
            head_pos_=(head_pos_+1)%partition;if(++block_pos_==partition){tail_block();block_pos_=0;}
        }
        for(auto e:block.midi)(void)block.midi_output.push(e);
    }
};
}
PluginState cab_ir_state(const CabIr& ir){ir.validate();PluginState state;state.class_id="mrs.cab-ir";const Header h{0x31524943,ir.sample_rate,ir.channels,static_cast<std::uint32_t>(ir.samples.size())};state.component.resize(sizeof(h)+ir.samples.size()*sizeof(float));std::memcpy(state.component.data(),&h,sizeof(h));std::memcpy(state.component.data()+sizeof(h),ir.samples.data(),ir.samples.size()*sizeof(float));return state;}
std::unique_ptr<IProcessor> cab_ir_factory(){return std::make_unique<CabProcessor>();}
}
