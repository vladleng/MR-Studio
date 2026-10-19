#include <mrs/reverb.hpp>
#include <mrs/no_denormals.hpp>
#include "ir_resample.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>

namespace mrs::processing {
namespace {
constexpr std::size_t source_budget=64*1024*1024, runtime_budget=384*1024*1024, head_size=128;
using Complex=std::complex<float>;
// Two independent complex butterflies per SSE2 vector. Plans are immutable;
// data/roots may be unaligned. Scalar remainder covers the length-2 stage.
void butterflies(Complex* first,Complex* second,const Complex* roots,std::size_t step,std::size_t count,bool inverse) noexcept {
    std::size_t i{};
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
    const auto real_signs=_mm_set_ps(0.f,-0.f,0.f,-0.f),imag_signs=_mm_set_ps(-0.f,0.f,-0.f,0.f);
    for(;i+2<=count;i+=2){
        auto root=step==1?_mm_loadu_ps(reinterpret_cast<const float*>(roots+i)):
            _mm_set_ps(roots[(i+1)*step].imag(),roots[(i+1)*step].real(),roots[i*step].imag(),roots[i*step].real());
        if(inverse)root=_mm_xor_ps(root,imag_signs);
        const auto x=_mm_loadu_ps(reinterpret_cast<const float*>(second+i)),a=_mm_loadu_ps(reinterpret_cast<const float*>(first+i));
        const auto real=_mm_shuffle_ps(x,x,_MM_SHUFFLE(2,2,0,0)),imag=_mm_shuffle_ps(x,x,_MM_SHUFFLE(3,3,1,1));
        const auto swapped=_mm_shuffle_ps(root,root,_MM_SHUFFLE(2,3,0,1));
        const auto b=_mm_add_ps(_mm_mul_ps(real,root),_mm_xor_ps(_mm_mul_ps(imag,swapped),real_signs));
        _mm_storeu_ps(reinterpret_cast<float*>(first+i),_mm_add_ps(a,b));
        _mm_storeu_ps(reinterpret_cast<float*>(second+i),_mm_sub_ps(a,b));
    }
#endif
    for(;i<count;++i){const auto root=roots[i*step];const auto a=first[i],b=second[i]*(inverse?std::conj(root):root);first[i]=a+b;second[i]=a-b;}
}
// SSE2 is baseline on x64. Other platforms retain the scalar reference path.
void spectral_mac(Complex* output,const Complex* input,const Complex* kernel,std::size_t count) noexcept {
    std::size_t i{};
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
    static_assert(sizeof(Complex)==2*sizeof(float));
    const auto signs=_mm_set_ps(0.f,-0.f,0.f,-0.f);
    for(;i+2<=count;i+=2){
        const auto x=_mm_loadu_ps(reinterpret_cast<const float*>(input+i)),h=_mm_loadu_ps(reinterpret_cast<const float*>(kernel+i));
        const auto real=_mm_shuffle_ps(x,x,_MM_SHUFFLE(2,2,0,0)),imag=_mm_shuffle_ps(x,x,_MM_SHUFFLE(3,3,1,1));
        const auto swapped=_mm_shuffle_ps(h,h,_MM_SHUFFLE(2,3,0,1));
        const auto value=_mm_add_ps(_mm_mul_ps(real,h),_mm_xor_ps(_mm_mul_ps(imag,swapped),signs));
        _mm_storeu_ps(reinterpret_cast<float*>(output+i),_mm_add_ps(_mm_loadu_ps(reinterpret_cast<const float*>(output+i)),value));
    }
#endif
    for(;i<count;++i)output[i]+=input[i]*kernel[i];
}
double direct_head(const float* kernel,const float* history) noexcept {
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
    auto a=_mm_setzero_pd(),b=a,c=a,d=a;
    for(std::size_t i=0;i<head_size;i+=8){
        const auto x=_mm_mul_ps(_mm_loadu_ps(kernel+i),_mm_loadu_ps(history+i));
        const auto y=_mm_mul_ps(_mm_loadu_ps(kernel+i+4),_mm_loadu_ps(history+i+4));
        a=_mm_add_pd(a,_mm_cvtps_pd(x));b=_mm_add_pd(b,_mm_cvtps_pd(_mm_movehl_ps(x,x)));
        c=_mm_add_pd(c,_mm_cvtps_pd(y));d=_mm_add_pd(d,_mm_cvtps_pd(_mm_movehl_ps(y,y)));
    }
    // Four independent double accumulators shorten the dependency chains.
    std::array<double,2> result{};_mm_storeu_pd(result.data(),_mm_add_pd(_mm_add_pd(a,b),_mm_add_pd(c,d)));
    return result[0]+result[1];
#else
    double result{};for(std::size_t i=0;i<head_size;++i)result+=kernel[i]*history[i];return result;
#endif
}
void require(bool ok,const char* text){if(!ok)throw std::invalid_argument(text);}
float clean(float v) noexcept {return std::isfinite(v)&&std::abs(v)>=1e-25f?v:0.f;}
struct Fft {
    std::vector<Complex> roots;
    std::vector<std::size_t> reverse;
    explicit Fft(std::size_t size):roots(size),reverse(size){
        std::size_t bits{};for(auto n=size;n>1;n>>=1)++bits;
        for(std::size_t i=0;i<size;++i){std::size_t r{},v=i;for(std::size_t b=0;b<bits;++b){r=r*2+(v&1);v>>=1;}reverse[i]=r;
            const auto angle=-2*std::numbers::pi*static_cast<double>(i)/size;roots[i]={static_cast<float>(std::cos(angle)),static_cast<float>(std::sin(angle))};}
    }
    void transform(std::vector<Complex>& data,bool inverse) const noexcept {
        const auto size=data.size();
        for(std::size_t i=0;i<size;++i)if(reverse[i]>i)std::swap(data[i],data[reverse[i]]);
        for(std::size_t length=2;length<=size;length*=2)for(std::size_t start=0;start<size;start+=length)
            butterflies(data.data()+start,data.data()+start+length/2,roots.data(),size/length,length/2,inverse);
        if(inverse)for(auto& v:data)v/=static_cast<float>(size);
    }
};
// Three non-uniform tiers. The immediate 128 tier starts at P taps; incremental
// tiers start at least 2P taps in (P to collect input, P to finish the job).
// Their output is therefore ready with zero algorithm latency.
// Larger-tier FFT/MAC work is distributed over that slack; Live needs its own gate.
struct Tier {
    Tier* input_owner{}; // same DSP owner, stable prepared lifetime; no cross-thread access
    std::size_t size,offset,count,cursor{},position{},filled{};
    std::uint64_t sample{},generation{1};
    Fft fft;
    std::vector<Complex> kernels,spectra,work,sum;
    std::vector<float> input,overlap,output;
    std::vector<std::uint64_t> tags;
    bool overlap_valid{};
    // Larger tiers have P samples of slack. Spread FFT and spectral products
    // across that interval, rather than doing the whole long tail in one callback.
    enum class Job { idle, await_input, forward, store, multiply, mirror, inverse, publish };
    Job job{Job::idle};
    std::size_t index{},length{2},start{},column{},product{},job_cursor{},job_filled{},quota{};
    std::uint64_t publish_at{},job_block{},ready_block{};
    Tier(std::span<const float> ir,std::size_t p,std::size_t begin,std::size_t end,Tier* owner=nullptr,std::stop_token stop={})
        :input_owner(owner),size(p),offset(begin),count((end-begin+p-1)/p),fft(2*p),kernels(count*2*p),spectra(owner?0:kernels.size()),
        work(2*p),sum(2*p),input(owner?0:p),overlap(p),output(std::bit_ceil(begin+2*p)),tags(output.size()){
        std::size_t log{};for(auto n=2*p;n>1;n>>=1)++log;
        // Butterfly/reversal steps cost more than contiguous spectral MACs.
        // Weight them to avoid spending the whole callback quota on FFT branches.
        const auto operations=count*(p+1)+8*p*log+28*p;
        quota=(operations+p-1)/p+8;
        for(std::size_t k=0;k<count;++k){if(stop.stop_requested())throw std::runtime_error("IR preparation cancelled");std::fill(work.begin(),work.end(),Complex{});
            for(std::size_t i=0;i<p&&begin+k*p+i<end;++i)work[i]=ir[begin+k*p+i];
            fft.transform(work,false);std::copy(work.begin(),work.end(),kernels.begin()+static_cast<std::ptrdiff_t>(k*2*p));}
        if(owner)std::vector<Complex>{}.swap(work);
    }
    std::size_t bytes() const noexcept {return (kernels.size()+spectra.size()+work.size()+sum.size()+fft.roots.size())*sizeof(Complex)+
        fft.reverse.size()*sizeof(std::size_t)+(input.size()+overlap.size()+output.size())*sizeof(float)+tags.size()*sizeof(std::uint64_t);}
    void reset() noexcept {cursor=position=filled=0;sample=job_block=ready_block=0;++generation;overlap_valid=false;job=Job::idle;}
    const Complex* input_spectra() const noexcept {return input_owner?input_owner->spectra.data():spectra.data();}
    void begin_fft() noexcept {index=0;length=2;start=column=0;}
    std::pair<std::size_t,bool> fft_steps(std::vector<Complex>& data,bool inverse,std::size_t capacity) noexcept {
        const auto n=2*size;
        std::size_t used{};
        while(used<capacity){
            if(index<n){const auto batch=std::min(capacity-used,n-index);for(std::size_t i=index;i<index+batch;++i)if(fft.reverse[i]>i)std::swap(data[i],data[fft.reverse[i]]);index+=batch;used+=batch;}
            else if(length<=n){
                const auto batch=std::min(capacity-used,length/2-column),step=n/length;
                butterflies(data.data()+start+column,data.data()+start+column+length/2,fft.roots.data()+column*step,step,batch,inverse);
                column+=batch;used+=batch;if(column==length/2){column=0;start+=length;if(start==n){start=0;length*=2;}}
            }else if(inverse&&index<2*n){const auto batch=std::min(capacity-used,2*n-index);
                for(std::size_t i=index;i<index+batch;++i)data[i-n]/=static_cast<float>(n);index+=batch;used+=batch;
            }else break;
        }
        return {used,length>n&&(!inverse||index==2*n)};
    }
    void advance_job(std::size_t allowance) noexcept {
        for(std::size_t budget=0;budget<allowance&&job!=Job::idle;++budget){
            if(job==Job::forward||job==Job::inverse){
                const bool inverse=job==Job::inverse;
                const auto [used,finished]=fft_steps(inverse?sum:work,inverse,std::max<std::size_t>(1,(allowance-budget)/4));
                if(used)budget+=used*4-1;
                if(finished){job=inverse?Job::publish:Job::store;index=0;}
                continue;
            }
            if(job==Job::multiply){
                const auto x=((job_cursor+count-product)%count)*2*size;
                const auto batch=std::min(allowance-budget,size+1-index);
                const auto* signal=input_spectra()+x+index;const auto* kernel=kernels.data()+product*2*size+index;auto* dest=sum.data()+index;
                spectral_mac(dest,signal,kernel,batch);
                index+=batch;budget+=batch-1;
                if(index==size+1){index=0;if(++product==job_filled){job=Job::mirror;index=1;}}
                continue;
            }
            switch(job){
            case Job::await_input:if(input_owner->ready_block!=job_block)return;job=Job::multiply;index=product=0;break;
            case Job::forward:break; // batched above
            case Job::store:spectra[job_cursor*2*size+index]=work[index];sum[index]={};if(++index==2*size){ready_block=job_block;job=Job::multiply;index=product=0;}break;
            case Job::multiply:break; // batched above for contiguous spectral MACs
            // All input and IR channels are real. Negative frequency bins are
            // conjugates, so multiply only the nonredundant half. Mirroring is
            // part of the bounded job, never an 8192-bin callback burst.
            case Job::mirror:sum[2*size-index]=std::conj(sum[index]);if(++index==size){job=Job::inverse;begin_fft();}break;
            case Job::inverse:break; // batched above
            case Job::publish:{const auto dest=static_cast<std::size_t>((publish_at+index)&(output.size()-1));
                output[dest]=sum[index].real()+(overlap_valid?overlap[index]:0.f);tags[dest]=generation;overlap[index]=sum[index+size].real();
                if(++index==size){overlap_valid=true;job=Job::idle;}break;}
            case Job::idle:break;
            }
        }
    }
    float process(float dry) noexcept {
        // Bounded 32-sample slices retain the original total work allowance.
        // Owner and alias run in order; slices finish within the existing P slack.
        if(size>128&&(position&31)==0)advance_job(quota*32);
        const auto slot=static_cast<std::size_t>(sample&(output.size()-1));const auto result=tags[slot]==generation?output[slot]:0.f;tags[slot]=0;
        if(!input_owner)input[position]=dry;++position;
        if(position==size){
            if(!input_owner){std::fill(work.begin(),work.end(),Complex{});for(std::size_t i=0;i<size;++i)work[i]=input[i];}
            if(size>128){
                job_cursor=cursor;filled=std::min(count,filled+1);job_filled=filled;
                ++job_block;publish_at=sample+1-size+offset;job=input_owner?Job::await_input:Job::forward;begin_fft();
                if(input_owner)std::fill(sum.begin(),sum.end(),Complex{});
                cursor=(cursor+1)%count;position=0;++sample;return result;
            }
            if(!input_owner){fft.transform(work,false);std::copy(work.begin(),work.end(),spectra.begin()+static_cast<std::ptrdiff_t>(cursor*2*size));}
            filled=std::min(count,filled+1);
            std::fill(sum.begin(),sum.end(),Complex{});
            for(std::size_t p=0;p<filled;++p){const auto x=((cursor+count-p)%count)*2*size,k=p*2*size;
                spectral_mac(sum.data(),input_spectra()+x,kernels.data()+k,size+1);}
            for(std::size_t i=1;i<size;++i)sum[2*size-i]=std::conj(sum[i]);
            fft.transform(sum,true);
            const auto at=sample+1-size+offset;
            for(std::size_t i=0;i<size;++i){const auto dest=static_cast<std::size_t>((at+i)&(output.size()-1));
                output[dest]=sum[i].real()+(overlap_valid?overlap[i]:0.f);tags[dest]=generation;overlap[i]=sum[i+size].real();}
            overlap_valid=true;cursor=(cursor+1)%count;position=0;
        }
        ++sample;return result;
    }
};
struct Route {
    Route* input_owner{};
    std::array<float,head_size> head{};
    std::array<float,head_size*2> history{};
    std::size_t cursor{};
    std::vector<Tier> tiers;
    explicit Route(std::span<const float> ir,Route* owner=nullptr,std::stop_token stop={}):input_owner(owner){
        for(std::size_t i=0;i<std::min(head_size,ir.size());++i)head[head_size-1-i]=ir[i];tiers.reserve(3);
        for(const auto [p,begin,end]:std::array<std::array<std::size_t,3>,3>{{{128,128,2048},{1024,2048,16384},{8192,16384,ir.size()}}})
            if(ir.size()>begin)tiers.emplace_back(ir,p,begin,std::min(end,ir.size()),owner?&owner->tiers[tiers.size()]:nullptr,stop);
    }
    void reset() noexcept {history.fill(0);cursor=0;for(auto& tier:tiers)tier.reset();}
    float process(float input) noexcept {
        if(!input_owner)history[cursor]=history[cursor+head_size]=input;
        double output=direct_head(head.data(),input_owner?input_owner->history.data()+input_owner->cursor:history.data()+cursor+1);
        for(auto& tier:tiers)output+=tier.process(input);cursor=(cursor+1)%head_size;return clean(static_cast<float>(output));
    }
    std::size_t bytes() const noexcept {std::size_t result=sizeof(*this);for(const auto& t:tiers)result+=t.bytes();return result;}
};
struct Runtime {
    Runtime(const Runtime&)=delete;
    Runtime& operator=(const Runtime&)=delete;
    ReverbRouting routing;
    std::uint32_t channels;
    std::vector<Route> routes;
    std::uint64_t ir_frames{},remaining{};
    bool running{};
    Runtime(const ReverbImpulse& ir,ProcessConfig config,std::stop_token stop={}):routing(ir.routing),channels(config.channels){
        const std::size_t paths=channels==1?1:routing==ReverbRouting::true_stereo?4:2;
        const auto length=ir.kernels[0].size();const auto frames=(static_cast<std::uint64_t>(length)*config.sample_rate+ir.sample_rate-1)/ir.sample_rate;
        ir_frames=frames;
        // Includes FDL spectra, complex kernels, plans and scheduling buffers.
        require(frames*paths*40+paths*1024*1024<=runtime_budget,"MR Reverb prepared memory budget exceeded (384 MiB); reduce IR duration/rate");
        routes.reserve(paths);
        for(std::size_t route=0;route<paths;++route){const auto source=routing==ReverbRouting::mono?0:route;
            if(stop.stop_requested())throw std::runtime_error("IR preparation cancelled");
            const auto resampled=detail::resample_ir(ir.kernels[source],1,ir.sample_rate,config.sample_rate,stop);
            routes.emplace_back(resampled,routing==ReverbRouting::true_stereo&&(route==1||route==3)?&routes[route-1]:nullptr,stop);}
        require(bytes()<=runtime_budget,"MR Reverb prepared memory budget exceeded (384 MiB)");
    }
    std::size_t bytes() const noexcept {std::size_t size=sizeof(*this);for(const auto& r:routes)size+=r.bytes();return size;}
    void reset() noexcept {remaining=0;running=false;for(auto& route:routes)route.reset();}
    std::array<float,2> process(const std::array<float,2>& input) noexcept {
        // Exact finite convolution support, not a level gate: even the smallest
        // sanitised nonzero input keeps the entire IR alive. Predelay/Tone are
        // downstream and continue draining independently. An empty runtime can
        // stop its local clock; reset invalidates scheduled output/FDL history
        // without clearing large buffers or touching the control-thread owner.
        if(input[0]!=0||(channels==2&&input[1]!=0))remaining=ir_frames;
        if(!remaining){if(running)reset();return {};}
        running=true;
        const auto output=process_active(input);
        // Reset only when actually entering silence on the next sample. A
        // one-tap IR with continuous input must not clear its heads every frame.
        --remaining;
        return output;
    }
    std::array<float,2> process_active(const std::array<float,2>& input) noexcept {
        if(channels==1)return {routes[0].process(input[0]),0}; // retain mono, no phantom stereo
        if(routing==ReverbRouting::true_stereo){
            // Owners must run before their aliases, including at block/FFT boundaries.
            const auto ll=routes[0].process(input[0]),lr=routes[1].process(input[0]);
            const auto rl=routes[2].process(input[1]),rr=routes[3].process(input[1]);return {ll+rl,lr+rr};}
        return {routes[0].process(input[0]),routes[1].process(input[1])};
    }
};
}
void ReverbImpulse::validate() const {
    require(sample_rate>=8000&&sample_rate<=192000,"MR Reverb IR rate must be 8–192 kHz");
    require(routing==ReverbRouting::mono||routing==ReverbRouting::stereo||routing==ReverbRouting::true_stereo,"Invalid MR Reverb routing");
    const std::size_t count=routing==ReverbRouting::mono?1:routing==ReverbRouting::stereo?2:4;
    const auto length=kernels[0].size();require(length>0&&length<=source_budget/(count*sizeof(float)),"MR Reverb source memory budget exceeded (64 MiB) or empty IR");
    bool nonzero=false;
    for(std::size_t k=0;k<4;++k){require(kernels[k].size()==(k<count?length:0),"MR Reverb kernel lengths/map mismatch");
        for(float v:kernels[k]){require(std::isfinite(v)&&std::abs(v)<=16,"MR Reverb IR contains invalid samples");nonzero|=v!=0;}}
    require(nonzero,"MR Reverb IR is silent");
}
ReverbImpulse reverb_impulse(std::uint32_t rate,std::uint32_t channels,std::span<const float> samples,ReverbQuadMap map){
    require(channels==1||channels==2||channels==4,"MR Reverb requires mono, stereo or explicitly mapped quad IR");
    require(channels!=4||map==ReverbQuadMap::ll_lr_rl_rr,"Confirm quad channel map LL/LR/RL/RR; arbitrary quad is not True Stereo");
    require(!samples.empty()&&samples.size()%channels==0&&samples.size()<=source_budget/sizeof(float),"MR Reverb source memory budget exceeded or invalid channels");
    ReverbImpulse result;result.sample_rate=rate;result.routing=channels==1?ReverbRouting::mono:channels==2?ReverbRouting::stereo:ReverbRouting::true_stereo;
    for(std::size_t ch=0;ch<channels;++ch){result.kernels[ch].reserve(samples.size()/channels);for(std::size_t i=ch;i<samples.size();i+=channels)result.kernels[ch].push_back(samples[i]);}
    result.validate();return result;
}
ReverbImpulse reverb_pair(const ReverbImpulse& left,const ReverbImpulse& right){
    left.validate();right.validate();require(left.routing==ReverbRouting::stereo&&right.routing==ReverbRouting::stereo&&left.sample_rate==right.sample_rate&&left.kernels[0].size()==right.kernels[0].size(),"True Stereo pair needs two stereo IRs with matching rate and length");
    require(left.kernels[0].size()<=source_budget/(4*sizeof(float)),"True Stereo pair source memory budget exceeded (64 MiB)");
    ReverbImpulse result;result.sample_rate=left.sample_rate;result.routing=ReverbRouting::true_stereo;
    result.kernels={left.kernels[0],left.kernels[1],right.kernels[0],right.kernels[1]};result.validate();return result;
}
// Fixed native-only rack. Both banks and all catalog processors are prepared
// off RT with the owning graph. Edits are coherent bounded host parameter batches,
// not heap-owning graph objects. A 10ms fade blends PRE before ONE core and POST
// after it. Rapid edits coalesce until the current fade completes.
struct RigRack {
    using Values=std::array<float,rig_slot_fields>; // type, bypass, typed parameters
    using State=std::array<Values,rig_lane_capacity*2>;
    struct Slot {std::array<std::unique_ptr<IProcessor>,rig_modules.size()> effects;};
    struct Bank {State state;std::array<Slot,rig_lane_capacity*2> slots;};
    std::array<Bank,2> banks;State desired{};std::uint32_t channels{},fade{},length{};std::size_t active{};bool dirty{};MidiBuffer discarded;
    explicit RigRack(ProcessConfig config):channels(config.channels),length(std::max(1u,config.sample_rate/100)){
        for(auto& bank:banks){bank.state=desired;for(auto& slot:bank.slots)for(std::size_t i=0;i<rig_modules.size();++i){NodeState node;node.processor_id=rig_modules[i].type_id;slot.effects[i]=native_factory(node);slot.effects[i]->prepare(config);}}
    }
    bool set(std::uint32_t id,float value)noexcept{
        if(id<32||id>=32+desired.size()*rig_slot_fields||!std::isfinite(value))return false;
        const auto field=(id-32)%rig_slot_fields;
        if((field==0&&(value<0||value>rig_modules.size()||value!=std::floor(value)))||(field==1&&value!=0&&value!=1)||(field>=2&&(value<-60||value>20000)))return false;
        auto& current=desired[(id-32)/rig_slot_fields][field];if(current!=value){current=value;dirty=true;}return true;
    }
    void parameters(Bank& bank,std::size_t i)noexcept{
        const auto& v=desired[i];if(v[0]==0)return;
        const auto type=static_cast<std::size_t>(v[0])-1;
        auto& fx=*bank.slots[i].effects[type];
        for(const auto& p:rig_modules[type].parameters)(void)fx.set_parameter(p.id,v[2+p.id]);
    }
    void configure(Bank& bank)noexcept{
        bank.state=desired;
        for(std::size_t i=0;i<desired.size();++i){const auto& v=desired[i];if(v[0]==0)continue;
            auto& fx=*bank.slots[i].effects[static_cast<std::size_t>(v[0])-1];parameters(bank,i);fx.reset();fx.warm();}
    }
    void reset()noexcept{fade=0;configure(banks[active]);dirty=false;}
    void begin()noexcept{
        if(!dirty||fade)return;
        dirty=false;if(desired==banks[active].state)return;
        // Pedal-only parameter edits retain envelope/filter histories and use
        // their sample smoothing. Structural/utility edits keep the bank fade.
        auto& bank=banks[active];bool parametersOnly=true;
        for(std::size_t i=0;i<desired.size();++i)if(desired[i]!=bank.state[i]&&
            (desired[i][0]<5||desired[i][0]!=bank.state[i][0]||desired[i][1]!=bank.state[i][1]))parametersOnly=false;
        if(parametersOnly){for(std::size_t i=0;i<desired.size();++i)if(desired[i]!=bank.state[i])parameters(bank,i);bank.state=desired;return;}
        configure(banks[1-active]);active=1-active;fade=length;
    }
    std::array<float,2> run(Bank& bank,std::array<float,2> samples,bool post)noexcept{
        const std::size_t start=post?rig_lane_capacity:0;
        for(std::size_t i=start;i<start+rig_lane_capacity;++i){const auto& v=bank.state[i];if(v[0]==0||v[1]!=0)continue;discarded.clear();bank.slots[i].effects[static_cast<std::size_t>(v[0])-1]->process({std::span(samples).first(channels),1,channels,{},{},discarded});for(auto& value:samples)if(!std::isfinite(value))value=0;}
        return samples;
    }
    std::array<float,2> process(std::array<float,2> input,bool post)noexcept{
        auto result=run(banks[active],input,post);if(fade){const auto old=run(banks[1-active],input,post);const auto amount=1-static_cast<float>(fade)/length;for(std::size_t ch=0;ch<channels;++ch)result[ch]=old[ch]+amount*(result[ch]-old[ch]);}return result;
    }
};
struct ReverbProcessor::Impl {
    std::unique_ptr<RigRack> rack;
    ProcessConfig config;
    std::array<float,3> target{0,0,.25f},current{target};
    std::array<std::atomic<float>,2> peaks{};
    std::array<float,2> rig_target{1,1},rig_current{1,1};
    std::array<float,5> cab_target{1,1,20,20000,0},cab_current{cab_target};
    std::array<float,2> cab_previous{},cab_hp{},cab_lp{};
    float cab_hp_coefficient{},cab_lp_coefficient{};
    bool gate_enabled{},gate_open{};
    float gate_db{-90.f},gate_level{std::pow(10.f,-90.f/20)},gate_gain{1},gate_release_step{};
    std::uint32_t gate_hold{},gate_hold_frames{};
    float smoothing{},shelf_pole{},shelf_gain{1};
    std::array<float,2> low{};
    std::vector<std::array<float,2>> delay;
    std::vector<std::uint64_t> delay_tags;
    std::uint64_t generation{1},sample{};
    std::vector<std::unique_ptr<Runtime>> owned; // only the control producer mutates this
    std::atomic<Runtime*> pending{},retired{};
    std::atomic<bool> busy{};
    Runtime* active{};Runtime* fading{}; // DSP owner only, quiescent warm/reset excepted
    std::uint32_t fade_left{},fade_length{};
    bool bypassed{};
};
struct ReverbKernel::Impl {ProcessConfig config;std::unique_ptr<Runtime> runtime;};
ReverbKernel::ReverbKernel(std::unique_ptr<Impl> value):impl_(std::move(value)){}
ReverbKernel::~ReverbKernel()=default;
ReverbKernel::ReverbKernel(ReverbKernel&&) noexcept=default;
ReverbKernel& ReverbKernel::operator=(ReverbKernel&&) noexcept=default;
ReverbKernel ReverbProcessor::prepare_kernel(const ReverbImpulse& ir,ProcessConfig config,std::stop_token stop){
    require(config.sample_rate>=8000&&config.sample_rate<=192000&&(config.channels==1||config.channels==2)&&config.max_block>0,"Invalid MR Reverb preparation config");
    if(stop.stop_requested())throw std::runtime_error("IR preparation cancelled");ir.validate();
    auto value=std::make_unique<ReverbKernel::Impl>();value->config=config;value->runtime=std::make_unique<Runtime>(ir,config,stop);
    return ReverbKernel(std::move(value));
}
bool ReverbProcessor::stage_kernel(ReverbKernel& kernel){
    auto& p=*impl_;require(!p.delay.empty()&&kernel.impl_&&kernel.impl_->runtime,"Invalid/unprepared MR Reverb kernel");
    require(kernel.impl_->config.sample_rate==p.config.sample_rate&&kernel.impl_->config.channels==p.config.channels,"MR Reverb prepared kernel device config changed");
    bool expected=false;if(!p.busy.compare_exchange_strong(expected,true))return false;
    try{collect_retired();auto* ptr=kernel.impl_->runtime.get();p.owned.push_back(std::move(kernel.impl_->runtime));p.pending.store(ptr,std::memory_order_release);return true;}
    catch(...){p.busy.store(false,std::memory_order_release);throw;}
}
ReverbProcessor::ReverbProcessor(bool cab,bool rig):cab_(cab||rig),rig_(rig),impl_(std::make_unique<Impl>()){}
ReverbProcessor::~ReverbProcessor()=default;
std::vector<ParameterInfo> ReverbProcessor::parameters() const{
    if(rig_){std::vector<ParameterInfo> result{{0,0,4,1,true,"Wet gain"},{1,0,1,1,true,"Mix"},{2,20,20000,20,true,"Low cut"},{3,20,20000,20000,true,"High cut"},{4,0,1,0,true,"Polarity"},{5,0,4,1,true,"Input"},{6,0,4,1,true,"Output"}};
        for(std::size_t slot=0;slot<rig_lane_capacity*2;++slot)for(std::uint32_t f=0;f<rig_slot_fields;++f)result.push_back({static_cast<std::uint32_t>(32+slot*rig_slot_fields+f),f<2?0.f:-60.f,f==0?static_cast<float>(rig_modules.size()):f==1?1.f:20000.f,0,false,"Rig rack state",true});return result;}
    if(cab_)return {{0,0,4,1,true,"Gain"},{1,0,1,1,true,"Mix"},{2,20,20000,20,true,"Low cut"},{3,20,20000,20000,true,"High cut"},{4,0,1,0,true,"Polarity"}};
    return {{0,0,200,0,true,"Pre-delay"},{1,-6,6,0,true,"Tone"},{2,0,1,.25f,true,"Mix"},{3,0,1,0,true,"Input gate"},{4,-120,-60,-90,true,"Gate threshold"}};
}
void ReverbProcessor::prepare(ProcessConfig config){
    require(config.sample_rate>=8000&&config.sample_rate<=192000&&(config.channels==1||config.channels==2)&&config.max_block>0,"MR Reverb runtime requires mono/stereo at 8–192 kHz");
    auto next=std::make_unique<Impl>();next->config=config;next->smoothing=1-std::exp(-1.f/(.02f*config.sample_rate));
    next->gate_hold_frames=static_cast<std::uint32_t>(config.sample_rate*.05);
    next->gate_release_step=1.f/(config.sample_rate*.01f);
    next->shelf_pole=1-std::exp(-2*std::numbers::pi_v<float>*std::min(3000.f,config.sample_rate*.45f)/config.sample_rate);
    next->delay.resize(std::bit_ceil(static_cast<std::size_t>(std::ceil(config.sample_rate*.2))+2));next->delay_tags.resize(next->delay.size());
    if(rig_)next->rack=std::make_unique<RigRack>(config);
    next->owned.reserve(3);next->fade_length=static_cast<std::uint32_t>(config.sample_rate*.08);if(cab_){next->target={0,0,1};next->current=next->target;}impl_=std::move(next);
}
void ReverbProcessor::restore(const PluginState& state){
    require((state.class_id.empty()||state.class_id==(rig_?"mrs.rig":cab_?"mrs.cab-v2":"mr_reverb"))&&state.component.empty()&&state.controller.empty(),"MR Reverb DSP slice accepts only empty external-selection state");
}
PluginState ReverbProcessor::capture() const{
    // External identity is authoritative in NativeInsert; never serialize the
    // kernel or inspect worker-owned storage during generic VST state capture.
    return {rig_?"mrs.rig":cab_?"mrs.cab-v2":"mr_reverb",{}, {}};
}
bool ReverbProcessor::set_parameter(std::uint32_t id,float value) noexcept {
    if(rig_&&id>=32)return impl_->rack&&impl_->rack->set(id,value);
    if(rig_&&id>=5){if(id>6||!std::isfinite(value)||value<0||value>4)return false;impl_->rig_target[id-5]=value;return true;}
    if(cab_){if(id>=5||!std::isfinite(value)||value<(id==2||id==3?20.f:0.f)||value>(id==0?4.f:id==2||id==3?20000.f:1.f))return false;impl_->cab_target[id]=value;return true;}
    if(id==3){if(value!=0&&value!=1)return false;impl_->gate_enabled=value==1;return true;}
    if(id==4){if(!std::isfinite(value)||value<-120||value>-60)return false;
        if(value!=impl_->gate_db){impl_->gate_db=value;impl_->gate_level=std::pow(10.f,value/20);}return true;}
    if(id>=3||!std::isfinite(value)||value<(id==1?-6.f:0.f)||value>(id==0?200.f:id==1?6.f:1.f))return false;
    impl_->target[id]=value;return true;
}
std::optional<float> ReverbProcessor::parameter_value(std::uint32_t id) const noexcept{if(rig_&&id>=32)return impl_->rack&&id<32+rig_lane_capacity*2*rig_slot_fields?std::optional<float>(impl_->rack->desired[(id-32)/rig_slot_fields][(id-32)%rig_slot_fields]):std::nullopt;if(rig_&&id>=5)return id<7?std::optional<float>(impl_->rig_target[id-5]):std::nullopt;if(cab_)return id<5?std::optional<float>(impl_->cab_target[id]):std::nullopt;if(id==3)return impl_->gate_enabled?1.f:0.f;if(id==4)return impl_->gate_db;return id<3?std::optional<float>(impl_->target[id]):std::nullopt;}
bool ReverbProcessor::stage_impulse(const ReverbImpulse& ir){
    auto& p=*impl_;require(!p.delay.empty(),"Prepare MR Reverb before loading IR");
    bool expected=false;if(!p.busy.compare_exchange_strong(expected,true))return false;
    collect_retired(); // busy acquire observes the previous callback's retirement
    try{ir.validate();auto next=std::make_unique<Runtime>(ir,p.config);auto* ptr=next.get();p.owned.push_back(std::move(next));p.pending.store(ptr,std::memory_order_release);return true;}
    catch(...){p.busy.store(false,std::memory_order_release);throw;}
}
void ReverbProcessor::collect_retired(){auto& p=*impl_;if(auto* old=p.retired.exchange(nullptr,std::memory_order_acquire))std::erase_if(p.owned,[&](const auto& v){return v.get()==old;});}
bool ReverbProcessor::switching() const noexcept{return impl_->busy.load(std::memory_order_acquire);}
std::size_t ReverbProcessor::prepared_bytes() const noexcept {std::size_t size=impl_->delay.size()*sizeof(std::array<float,2>)+impl_->delay_tags.size()*sizeof(std::uint64_t);for(const auto& v:impl_->owned)size+=v->bytes();return size;}
std::uint64_t ReverbProcessor::tail_frames() const noexcept {
    const auto& p=*impl_;std::uint64_t frames{};
    for(const auto* runtime:{p.active,p.fading,p.pending.load()})if(runtime)frames=std::max(frames,runtime->ir_frames);
    std::uint64_t rackTail{};
    if(p.rack){const auto bound=[&](const RigRack::State& state){double samples{};for(const auto& v:state)if(v[1]==0){if(v[0]>=2&&v[0]<=4)samples+=64.*v[3]*p.config.sample_rate*std::max(1.,v[0]==4?std::pow(10.,v[4]/40.):1.)/(std::numbers::pi*std::max(20.f,v[2]));else if(v[0]==6)samples+=64.*p.config.sample_rate/(2.*std::numbers::pi*std::max(200.f,v[3]));}return static_cast<std::uint64_t>(std::ceil(samples));};
        rackTail=bound(p.rack->desired);if(p.rack->fade)rackTail=std::max(rackTail,bound(p.rack->banks[1-p.rack->active].state))+p.rack->fade;}
    if(!frames&&!rackTail)return 0;
    const auto predelay=static_cast<std::uint64_t>(std::ceil(static_cast<double>(std::max(p.target[0],p.current[0]))*p.config.sample_rate/1000.));
    // Fixed 3 kHz wet shelf settles far below the sanitisation floor in 10 ms,
    // including finite float headroom. Tone=0 has no additional output tail.
    const auto tone=p.target[1]!=0||p.current[1]!=0?(p.config.sample_rate+99)/100:0;
    if(cab_)return (frames?frames-1:0)+((p.cab_target[2]>20||p.cab_target[3]<20000)?p.config.sample_rate:0)+rackTail;
    return frames-1+predelay+tone;
}
void ReverbProcessor::warm(){
    auto& p=*impl_;collect_retired(); // quiescent control thread only
    if(auto* next=p.pending.exchange(nullptr))p.active=next;
    p.fading=nullptr;p.fade_left=0;p.busy.store(false);
    std::erase_if(p.owned,[&](const auto& value){return value.get()!=p.active;});
    reset();
}
void ReverbProcessor::reset() noexcept {auto& p=*impl_;if(p.active)p.active->reset();if(p.fading)p.fading->reset();++p.generation;p.sample=0;p.low={};p.cab_previous={};p.cab_hp={};p.cab_lp={};p.cab_current=p.cab_target;p.rig_current=p.rig_target;p.current=p.target;if(p.rack)p.rack->reset();p.shelf_gain=std::pow(10.f,p.current[1]/20);p.gate_open=false;p.gate_hold=0;p.gate_gain=p.gate_enabled?0.f:1.f;}
void ReverbProcessor::advance_bypass(std::uint32_t) noexcept {
    if(rig_)for(auto& peak:impl_->peaks)peak.store(0,std::memory_order_relaxed);
    auto& p=*impl_;auto* next=p.pending.exchange(nullptr,std::memory_order_acquire);
    const bool completed=next||p.fade_left;
    auto* old=p.fading;
    if(next){old=p.active;p.active=next;}
    p.fading=nullptr;p.fade_left=0;
    if(!p.bypassed||completed)reset();
    p.bypassed=true;
    if(completed){p.retired.store(old,std::memory_order_release);p.busy.store(false,std::memory_order_release);}
    // Do not clear busy while the producer is still preparing an unpublished IR.
}
std::array<float,2> ReverbProcessor::levels()const noexcept{return {impl_->peaks[0].load(std::memory_order_relaxed),impl_->peaks[1].load(std::memory_order_relaxed)};}
void ReverbProcessor::process(ProcessBlock block) noexcept {
    static_assert(std::atomic<float>::is_always_lock_free);std::array<float,2> peaks{};
    const ScopedNoDenormals no_denormals;
    auto& p=*impl_;if(block.channels!=p.config.channels||block.audio.size()<static_cast<std::size_t>(block.frames)*block.channels||p.delay.empty())return;
    p.bypassed=false;
    if(!p.fade_left)if(auto* next=p.pending.exchange(nullptr,std::memory_order_acquire)){p.fading=p.active;p.active=next;p.fade_left=p.fade_length;}
    std::size_t event{};
    for(std::uint32_t f=0;f<block.frames;++f){
        while(event<block.parameters.size()&&block.parameters[event].offset<=f){const auto& e=block.parameters[event++];(void)set_parameter(e.id,e.value);}
        if(p.rack)p.rack->begin();
        if(rig_)for(std::size_t i=0;i<2;++i)p.rig_current[i]+=p.smoothing*(p.rig_target[i]-p.rig_current[i]);
        if(cab_)for(std::size_t i=0;i<5;++i)p.cab_current[i]+=p.smoothing*(p.cab_target[i]-p.cab_current[i]);
        for(std::size_t i=0;i<3;++i){p.current[i]+=p.smoothing*(p.target[i]-p.current[i]);if(std::abs(p.target[i]-p.current[i])<1e-6f)p.current[i]=p.target[i];}
        if((p.sample&31)==0){p.shelf_gain=std::pow(10.f,p.current[1]/20);if(cab_){p.cab_hp_coefficient=std::exp(-2*std::numbers::pi_v<float>*std::min(p.cab_current[2],p.config.sample_rate*.45f)/p.config.sample_rate);p.cab_lp_coefficient=1-std::exp(-2*std::numbers::pi_v<float>*std::min(p.cab_current[3],p.config.sample_rate*.45f)/p.config.sample_rate);}}
        std::array<float,2> dry{};for(std::size_t ch=0;ch<block.channels;++ch)dry[ch]=clean(block.audio[static_cast<std::size_t>(f)*block.channels+ch]*(rig_?p.rig_current[0]:1.f));
        if(rig_)peaks[0]=std::max({peaks[0],std::abs(dry[0]),std::abs(dry[1])});
        if(p.rack)dry=p.rack->process(dry,false);
        auto input=dry;
        if(p.gate_enabled){
            // Linked peak detector gates only new wet input. Opening is immediate
            // to retain transients; 6 dB hysteresis, 50 ms hold and a 10 ms ramp
            // prevent chatter. Exact zero at closure lets finite IR support idle.
            const auto peak=std::max(std::abs(dry[0]),std::abs(dry[1]));
            if(peak>=p.gate_level||(p.gate_open&&peak>=p.gate_level*.5011872f)){
                p.gate_open=true;p.gate_hold=p.gate_hold_frames;
            }else if(p.gate_hold)--p.gate_hold;else p.gate_open=false;
            p.gate_gain=p.gate_open?1.f:std::max(0.f,p.gate_gain-p.gate_release_step);
            for(auto& v:input)v*=p.gate_gain;
        }else{p.gate_gain=1;p.gate_open=false;p.gate_hold=0;}
        auto wet=p.active?p.active->process(input):dry;
        if(p.fade_left){const auto old=p.fading?p.fading->process(input):dry;const auto fraction=1-static_cast<float>(p.fade_left)/p.fade_length;
            for(std::size_t ch=0;ch<block.channels;++ch)wet[ch]=old[ch]+fraction*(wet[ch]-old[ch]);
            if(--p.fade_left==0){p.retired.store(p.fading,std::memory_order_release);p.fading=nullptr;p.busy.store(false,std::memory_order_release);}}
        const auto cursor=static_cast<std::size_t>(p.sample&(p.delay.size()-1));p.delay[cursor]=wet;p.delay_tags[cursor]=p.generation;
        const double frames=static_cast<double>(p.current[0])*p.config.sample_rate/1000.;const auto integer=static_cast<std::size_t>(frames);const float fraction=static_cast<float>(frames-integer);
        const auto a=(cursor-integer)&(p.delay.size()-1),b=(a-1)&(p.delay.size()-1);
        for(std::size_t ch=0;ch<block.channels;++ch){const auto va=p.delay_tags[a]==p.generation?p.delay[a][ch]:0.f,vb=p.delay_tags[b]==p.generation?p.delay[b][ch]:0.f;
            const auto delayed=va+fraction*(vb-va);p.low[ch]=clean(p.low[ch]+p.shelf_pole*(delayed-p.low[ch]));
            auto toned=delayed+(p.shelf_gain-1)*(delayed-p.low[ch]);
            if(cab_){const auto hp=p.cab_hp_coefficient;
                const auto lp=p.cab_lp_coefficient;
                p.cab_hp[ch]=clean(hp*(p.cab_hp[ch]+toned-p.cab_previous[ch]));p.cab_previous[ch]=toned;
                const auto high=p.cab_current[2]<=20.001f?toned:p.cab_hp[ch];p.cab_lp[ch]=clean(p.cab_lp[ch]+lp*(high-p.cab_lp[ch]));
                toned=(p.cab_current[3]>=19999.9f?high:p.cab_lp[ch])*p.cab_current[0]*(1-2*p.cab_current[4]);}
            block.audio[static_cast<std::size_t>(f)*block.channels+ch]=clean((p.active?dry[ch]+(cab_?p.cab_current[1]:p.current[2])*(toned-dry[ch]):dry[ch]));}
        if(p.rack){std::array<float,2> out{};for(std::size_t ch=0;ch<block.channels;++ch)out[ch]=block.audio[static_cast<std::size_t>(f)*block.channels+ch];out=p.rack->process(out,true);for(std::size_t ch=0;ch<block.channels;++ch){const auto value=clean(out[ch]*p.rig_current[1]);block.audio[static_cast<std::size_t>(f)*block.channels+ch]=value;peaks[1]=std::max(peaks[1],std::abs(value));}if(p.rack->fade)--p.rack->fade;}
        ++p.sample;
    }
    if(rig_)for(std::size_t i=0;i<2;++i)p.peaks[i].store(peaks[i],std::memory_order_relaxed);
    for(auto e:block.midi)(void)block.midi_output.push(e);
}
std::unique_ptr<IProcessor> reverb_factory(){return std::make_unique<ReverbProcessor>();}
}
