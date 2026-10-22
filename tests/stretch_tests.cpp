#include <mrs/stretch.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _MSC_VER
#include <malloc.h>
#endif

namespace {
std::atomic<bool> probing{};
std::atomic<std::size_t> allocations{},deallocations{};
}
void* operator new(std::size_t n) {
    if(probing) ++allocations;
    if(auto* p=std::malloc(n?n:1)) return p;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { if(probing && p) ++deallocations; std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p,std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }
void* operator new(std::size_t n,std::align_val_t a) {
    if(probing) ++allocations;
    void* p{};
#ifdef _MSC_VER
    p=_aligned_malloc(n?n:1,static_cast<std::size_t>(a));
#else
    if(posix_memalign(&p,static_cast<std::size_t>(a),n?n:1)!=0) p=nullptr;
#endif
    if(!p) throw std::bad_alloc{};
    return p;
}
void* operator new[](std::size_t n,std::align_val_t a) { return ::operator new(n,a); }
void operator delete(void* p,std::align_val_t) noexcept {
    if(probing && p) ++deallocations;
#ifdef _MSC_VER
    _aligned_free(p);
#else
    std::free(p);
#endif
}
void operator delete[](void* p,std::align_val_t a) noexcept { ::operator delete(p,a); }
void operator delete(void* p,std::size_t,std::align_val_t a) noexcept { ::operator delete(p,a); }
void operator delete[](void* p,std::size_t,std::align_val_t a) noexcept { ::operator delete(p,a); }
namespace {
using namespace mrs::stretch;
constexpr double pi=3.14159265358979323846;
void check(bool ok,const char* what) { if(!ok) throw std::runtime_error(what); }
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__)
std::vector<float> signal(std::size_t n,std::uint32_t channels=2,std::uint32_t rate=48000) {
    std::vector<float> data(n*channels);
    for(std::size_t i=0;i<n;++i) for(std::uint32_t c=0;c<channels;++c)
        data[i*channels+c]=static_cast<float>(0.25*std::sin(2*pi*440*static_cast<double>(i)/rate))*(c==0?1.f:-0.5f);
    return data;
}
std::vector<float> rendered(const std::vector<float>& source,Parameters p,Config config={}) {
    auto engine=make_signalsmith_engine();
    std::vector<float> output;
    std::int64_t length{};CHECK(output_frames(static_cast<std::int64_t>(source.size()/config.channels),p.enabled?p.duration_ratio:1,length)==Status::ok);
    output.resize(static_cast<std::size_t>(length)*config.channels);
    const auto result=render(*engine,config,p,static_cast<std::int64_t>(source.size()/config.channels),
        [&](std::int64_t first,std::span<float> block){
            std::copy_n(source.data()+static_cast<std::size_t>(first)*config.channels,block.size(),block.data());return true;},
        [&](std::int64_t first,std::span<const float> block){
            std::copy(block.begin(),block.end(),output.begin()+static_cast<std::ptrdiff_t>(first*config.channels));return true;});
    CHECK(result.status==Status::ok && result.frames==length);
    for(auto sample:output) CHECK(std::isfinite(sample));
    return output;
}
void adapter() {
    auto engine=make_signalsmith_engine();
    CHECK(engine->reset(1)==Status::not_prepared);
    Config config; Parameters p;
    CHECK(engine->prepare(config,p)==Status::ok);
    const auto latency=engine->latency();CHECK(latency.input_frames>0 && latency.output_frames>0);
    CHECK(latency.seek_context_frames>=latency.input_frames);
    config.split_computation=true;CHECK(engine->prepare(config,p)==Status::ok);
    CHECK(engine->latency().output_frames>latency.output_frames);
    auto invalid=config;invalid.channels=3;CHECK(engine->prepare(invalid,p)==Status::unsupported);
    CHECK(engine->latency().output_frames>latency.output_frames); // failure preserves prepared state
    p.duration_ratio=0;CHECK(engine->set_parameters(p)==Status::invalid_parameter);
    p={};p.quality=Quality::economical;CHECK(engine->set_parameters(p)==Status::unsupported);
    CHECK(engine->reset(123)==Status::ok && engine->generation()==123);
    std::array<float,512> input{},output{};
    std::array<const float*,2> in{input.data(),input.data()+256};
    std::array<float*,2> out{output.data(),output.data()+256};
    CHECK(engine->process(in,255,out,256).status==Status::need_input);
    CHECK(engine->process(in,256,out,4097).status==Status::output_full);
    std::array<float*,2> alias{input.data(),input.data()+256};
    CHECK(engine->process(in,256,alias,256).status==Status::invalid_buffer);
    alias={output.data(),output.data()};CHECK(engine->process(in,256,alias,256).status==Status::invalid_buffer);
    input[0]=std::numeric_limits<float>::quiet_NaN();
    CHECK(engine->process(in,256,out,256).status==Status::invalid_buffer);input[0]=0;
    CHECK(engine->process(in,256,out,256).produced==256);
    std::vector<float> tail(static_cast<std::size_t>(engine->latency().output_frames)*2);
    out={tail.data(),tail.data()+tail.size()/2};
    CHECK(engine->flush(out,0).status==Status::output_full);
    CHECK(engine->flush(out,static_cast<std::uint32_t>(tail.size()/2)).status==Status::drained);
    CHECK(engine->process(in,256,out,256).status==Status::drained);
    CHECK(engine->seek(in,256,124)==Status::ok && engine->generation()==124);
}
void offline() {
    const auto source=signal(48000);
    CHECK(rendered(source,{})==source);
    Parameters bypass;bypass.enabled=false;bypass.duration_ratio=2;bypass.semitones=12;
    CHECK(rendered(source,bypass)==source);
    for(double r:{0.125,0.5,0.75,0.9,1.1,1.25,1.5,2.,8.}) {
        Parameters p;p.duration_ratio=r;
        const auto output=rendered(source,p);
        CHECK(output.size()==static_cast<std::size_t>(std::floor(48000*r+0.5))*2);
        double energy{};
        for(auto x:output) energy+=x*x;
        CHECK(energy/output.size()>0.001);
        double err{},right_energy{};
        for(std::size_t i=0;i<output.size();i+=2) { const double e=output[i]*(-0.5)-output[i+1];err+=e*e;right_energy+=output[i+1]*output[i+1]; }
        CHECK(std::sqrt(err/right_energy)<0.001);
    }
    Parameters p;p.duration_ratio=1.25;p.semitones=7;
    const auto first=rendered(source,p),second=rendered(source,p);
    CHECK(first==second);
    // Frequency estimate away from spectral startup/tail.
    unsigned crossings{};
    for(std::size_t i=10002;i<first.size()-10000;i+=2) if(first[i-2]<=0 && first[i]>0) ++crossings;
    const double hz=crossings*48000.0/(static_cast<double>(first.size()-20000)/2);
    CHECK(std::abs(hz-440*std::pow(2.,7./12))<8);
    for(auto mode:{Formants::preserve,Formants::shift}) {
        p.formants=mode;p.formant_semitones=3;p.formant_base_hz=440;
        CHECK(!rendered(source,p).empty());
    }
    for(auto rate:{44100u,48000u,96000u}) for(auto quality:{Quality::balanced,Quality::economical}) {
        Config config;config.sample_rate=rate;config.channels=1;config.split_computation=true;
        p={};p.semitones=1;p.quality=quality;
        for(std::size_t n:{0u,1u,7u,500u}) CHECK(rendered(signal(n,1,rate),p,config).size()==n);
    }
    // Isolate adapter latency from the backend's non-unity transient timing:
    // a nearly neutral pitch avoids the worker bypass, with unity time ratio.
    Config mono;mono.channels=1;p={};p.cents=0.01;
    for(std::size_t at:{0u,24000u,47999u}) {
        std::vector<float> impulse(48000);impulse[at]=1;
        const auto audio=rendered(impulse,p,mono);
        const auto peak=std::max_element(audio.begin(),audio.end(),[](float a,float b){return std::abs(a)<std::abs(b);});
        CHECK(std::abs(static_cast<std::ptrdiff_t>(at)-(peak-audio.begin()))<=1);
        CHECK(std::abs(*peak)>0.9f);
    }
    for(auto block:{64u,511u,4096u}) {
        Config partition;partition.max_output_frames=block;p={};p.duration_ratio=1.1;
        CHECK(rendered(source,p,partition).size()==105600);
    }
    // Worker cancellation/errors are visible; caller owns atomic publication.
    auto engine=make_signalsmith_engine();std::stop_source stop;stop.request_stop();
    CHECK(render(*engine,{},p,48000,[](auto,auto){return true;},[](auto,auto){return true;},stop.get_token()).status==Status::cancelled);
    CHECK(render(*engine,{},p,48000,[](auto,auto){return false;},[](auto,auto){return true;}).status==Status::io_error);
    CHECK(render(*engine,{},Parameters{},48000,[](auto,auto){return true;},[](auto,auto){return false;}).status==Status::io_error);
}
void realtime() {
    std::array<float,32768> left{},right{};
    std::array<float,4096> lout{},rout{};
    std::uint32_t rng=1234567;
    for(std::size_t i=0;i<left.size();++i) {
        rng=rng*1664525+1013904223;left[i]=static_cast<float>(static_cast<double>(rng)/UINT32_MAX-0.5);
        right[i]=left[i]*0.5f;
    }
    std::array<const float*,2> in{left.data(),right.data()};std::array<float*,2> out{lout.data(),rout.data()};
    for(auto rate:{44100u,48000u,96000u}) for(auto quality:{Quality::balanced,Quality::economical}) for(bool split:{false,true}) {
        auto engine=make_signalsmith_engine();Config config;config.sample_rate=rate;config.split_computation=split;
        Parameters p;p.quality=quality;p.semitones=7;p.formants=Formants::preserve;
        CHECK(engine->prepare(config,p)==Status::ok);
        allocations=0;deallocations=0;
        bool good=true;
        // Probe first process as well as steady state and parameter changes.
        probing=true;
        for(unsigned i=0;i<1200;++i) {
            p.duration_ratio=std::array{0.125,0.5,0.9,1.,1.25,2.,8.}[i%7];
            p.semitones=static_cast<double>(i%25)-12;
            if(engine->set_parameters(p)!=Status::ok) good=false;
            const auto n=std::array<std::uint32_t,6>{1,64,127,128,512,4096}[i%6];
            const auto r=engine->process(in,engine->input_required(n),out,n);
            if(r.status!=Status::ok) good=false;
            for(std::uint32_t j=0;j<r.produced;++j) if(!std::isfinite(lout[j]) || !std::isfinite(rout[j])) good=false;
        }
        probing=false;
        CHECK(good);CHECK(allocations==0);CHECK(deallocations==0);
    }
}
}
int main(int argc,char** argv) {
    try {
        const std::string suite=argc>1?argv[1]:"adapter";
        if(suite=="adapter") adapter(); else if(suite=="render") offline();
        else if(suite=="realtime") realtime(); else throw std::invalid_argument("unknown suite");
        std::cout<<"stretch "<<suite<<" PASS\n";
    } catch(const std::exception& e) { probing=false;std::cerr<<e.what()<<'\n';return 1; }
}
