#include <mrs/reverb.hpp>
#include <mrs/reverb_file.hpp>
#include <mrs/audio.hpp>
#include <mrs/recording.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <bit>
#include <fstream>

namespace { thread_local bool probe{}; thread_local std::size_t allocations{},deallocations{}; }
void* operator new(std::size_t n){if(probe)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc{};}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {if(probe&&p)++deallocations;std::free(p);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
namespace {
using namespace mrs::processing;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F f){bool caught{};try{f();}catch(const std::exception&){caught=true;}check(caught,"invalid IR accepted");}
ReverbImpulse impulse(std::uint32_t rate,std::size_t length,ReverbRouting mode=ReverbRouting::mono){
    ReverbImpulse ir;ir.sample_rate=rate;ir.routing=mode;
    const auto paths=mode==ReverbRouting::mono?1:mode==ReverbRouting::stereo?2:4;
    for(int p=0;p<paths;++p){ir.kernels[p].resize(length);ir.kernels[p][0]=.1f*(p+1);}
    return ir;
}
void run(ReverbProcessor& dsp,std::span<float> buffer,std::uint32_t channels,std::uint32_t block=127){
    MidiBuffer output;
    for(std::size_t offset=0;offset<buffer.size();){const auto frames=static_cast<std::uint32_t>(std::min<std::size_t>(block,(buffer.size()-offset)/channels));
        dsp.process({buffer.subspan(offset,frames*channels),frames,channels,{},{},output});offset+=frames*channels;}
}
void setup(ReverbProcessor& dsp,const ReverbImpulse& ir,std::uint32_t rate=48000,std::uint32_t channels=2,float mix=1,float predelay=0,float tone=0){
    dsp.prepare({rate,channels,1024});check(dsp.set_parameter(2,mix),"Mix");check(dsp.set_parameter(0,predelay),"Pre-delay");check(dsp.set_parameter(1,tone),"Tone");
    check(dsp.stage_impulse(ir),"initial stage");dsp.warm();check(dsp.latency()==0,"latency");
}
void rig_runtime(){
    for(const auto rate:{44100u,48000u,96000u})for(const auto channels:{1u,2u}){
        ReverbProcessor rig(true,true),cab(true);auto ir=impulse(rate,rate/5,channels==1?ReverbRouting::mono:ReverbRouting::stereo);
        for(auto* dsp:{&rig,&cab}){dsp->prepare({rate,channels,128});check(dsp->stage_impulse(ir),"Rig stage");dsp->warm();}
        std::vector<float> a(4096*channels,.25f),b=a;allocations=deallocations=0;probe=true;run(rig,a,channels,128);probe=false;check(!allocations&&!deallocations,"Rig callback has no heap allocate/free");run(cab,b,channels,128);check(a==b,"Neutral Rig WAV output equals accepted Cab core exactly");
        check(rig.capture().class_id=="mrs.rig","Rig processor identity");rig.set_parameter(5,2);rig.set_parameter(6,.5f);rig.warm();std::fill(a.begin(),a.end(),.25f);std::fill(b.begin(),b.end(),.25f);run(rig,a,channels);cab.warm();run(cab,b,channels);for(std::size_t i=0;i<a.size();++i)check(std::abs(a[i]-b[i])<1e-6,"Input before linear IR and output after mix");
        rig.set_parameter(0,0);rig.set_parameter(1,0);rig.warm();std::fill(a.begin(),a.end(),.25f);allocations=deallocations=0;probe=true;run(rig,a,channels,64);probe=false;check(!allocations&&!deallocations&&std::abs(a.back()-.25f)<1e-6,"Rig dry path uses input/output utility, bypasses wet gain");const auto levels=rig.levels();check(std::abs(levels[0]-.5f)<1e-6&&std::abs(levels[1]-.25f)<1e-6,"Rig peak telemetry contains actual DSP samples");
        rig.set_parameter(0,1);rig.set_parameter(1,1);rig.warm();auto second=ir;second.kernels[0][0]=.3f;check(rig.stage_impulse(second),"Rig swap");a.assign(static_cast<std::size_t>(rate)*channels/5,.25f);allocations=deallocations=0;probe=true;run(rig,a,channels,113);probe=false;check(!allocations&&!deallocations,"Rig crossfade has no callback heap");check(!rig.switching(),"Rig transition finishes at every tested rate");rig.collect_retired();check(!rig.set_parameter(5,5)&&!rig.set_parameter(7,1),"Rig gain/id bounds");
    }
}
void rig_pedals(){
    for(const auto rate:{44100u,48000u,96000u})for(const auto channels:{1u,2u}){
        MidiBuffer midi;
        auto make=[&](const char* type){NodeState node;node.processor_id=type;auto p=native_factory(node);p->prepare({rate,channels,128});return p;};
        auto process=[&](IProcessor& p,std::vector<float>& data){allocations=deallocations=0;probe=true;for(std::size_t start=0;start<data.size();){const auto n=static_cast<std::uint32_t>(std::min<std::size_t>(128,(data.size()-start)/channels));p.process({std::span(data).subspan(start,n*channels),n,channels,{},{},midi});start+=n*channels;}probe=false;check(!allocations&&!deallocations,"Pedals no callback alloc/free");};
        auto compressor=make("mrs.compressor");
        check(compressor->set_parameter(0,-20)&&compressor->set_parameter(1,4),"Compressor physical units");compressor->warm();
        std::vector<float> data(rate*channels,1);process(*compressor,data);
        for(const auto frame:{rate/1000,rate/100,rate/10}){const auto reduction=15.*(1.-std::exp(-(frame+1.)/(.01*rate)));check(std::abs(data[frame*channels]-std::pow(10.,-reduction/20.))<2e-6,"Compressor independent hard-knee attack oracle");}
        check(std::abs(data.back()-std::pow(10.,-15./20.))<2e-6,"Compressor ratio/threshold steady gain");
        std::fill(data.begin(),data.end(),.01f);process(*compressor,data);const auto frame=rate/10;const auto release=15.*std::exp(-(frame+1.)/(.1*rate));check(std::abs(data[frame*channels]-.01*std::pow(10.,-release/20.))<2e-6,"Compressor independent release oracle");
        compressor->set_parameter(1,1);compressor->set_parameter(4,6);compressor->reset();compressor->warm();std::fill(data.begin(),data.end(),.1f);process(*compressor,data);check(std::abs(data.back()-.1*std::pow(10.,6./20.))<1e-6,"Ratio1 unity with explicit output trim");
        compressor->set_parameter(1,4);compressor->set_parameter(4,0);compressor->warm();compressor->reset();
        if(channels==2){for(std::size_t i=0;i<data.size();i+=2){data[i]=.5f;data[i+1]=.25f;}process(*compressor,data);check(std::abs(data[data.size()-2]-2*data.back())<1e-7,"Stereo-linked compressor preserves image");}
        auto drive=make("mrs.drive");drive->set_parameter(0,18);drive->set_parameter(1,200);drive->set_parameter(2,-6);drive->warm();
        std::fill(data.begin(),data.end(),.2f);process(*drive,data);
        const double inputGain=std::pow(10.,18./20.),expected=std::tanh(inputGain*.2)/std::tanh(inputGain)*std::pow(10.,-6./20.);
        check(std::abs(data.back()-expected)<2e-6,"Drive gain/soft clip/level independent DC oracle");
        auto toneEnergy=[&](float hz){drive->set_parameter(1,hz);drive->warm();drive->reset();for(std::size_t i=0;i<data.size();++i)data[i]=static_cast<float>(.02*std::sin(2.*3.141592653589793*4000*(i/channels)/rate));process(*drive,data);double sum{};for(std::size_t i=data.size()/2;i<data.size();++i)sum+=data[i]*data[i];return sum;};
        const auto dark=toneEnergy(200),bright=toneEnergy(12000);check(bright>dark*20,"Drive tone is an audible low-pass");
        auto automated=make("mrs.drive"),reference=make("mrs.drive");std::array<float,256> timed{},plain{};timed.fill(.1f);plain.fill(.1f);const std::array<ParameterChange,1> event{{{64,0,24}}};
        probe=true;automated->process({std::span(timed).first(128*channels),128,channels,{},event,midi});reference->process({std::span(plain).first(128*channels),128,channels,{},{},midi});probe=false;
        for(std::size_t i=0;i<64*channels;++i)check(timed[i]==plain[i],"Pedal automation respects sample offset");check(std::abs(timed[128*channels-1]-plain[128*channels-1])>.001f,"Pedal automation changes real DSP after event");
        if(channels==2){drive->reset();for(std::size_t i=0;i<data.size();i+=2){data[i]=.2f;data[i+1]=0;}process(*drive,data);check(data.back()==0,"Drive independent stereo channels");}
        for(auto* p:{compressor.get(),drive.get()}){check(p->latency()==0,"Pedals zero lookahead latency");check(!p->set_parameter(0,std::numeric_limits<float>::infinity())&&!p->set_parameter(99,1),"Invalid pedal parameter rejected");std::fill(data.begin(),data.end(),std::numeric_limits<float>::quiet_NaN());process(*p,data);for(auto x:data)check(std::isfinite(x),"Nonfinite pedal input sanitized");std::fill(data.begin(),data.end(),0.f);process(*p,data);check(std::abs(data.back())<1e-20,"Silence drains pedal history");}
        // Same child processors routed through the real shared Rig, exact per-sample
        // oracle including PRE compression, one core, and POST nonlinear drive.
        mrs::NativeInsert fx;fx.id=mrs::new_id();fx.kind=mrs::InsertKind::rig;fx.rig.pre_fx={mrs::make_rig_module("mrs.compressor")};fx.rig.post_fx={mrs::make_rig_module("mrs.drive")};
        ReverbProcessor rig(true,true),core(true);rig.prepare({rate,channels,128});core.prepare({rate,channels,128});const auto ir=impulse(rate,32);check(rig.stage_impulse(ir)&&core.stage_impulse(ir),"Pedal oracle core");
        const auto graph=insert_graph(std::array{fx});for(const auto& v:graph.nodes.front().parameters)check(rig.set_parameter(v.id,v.value),"Pedal complete rack snapshot");rig.warm();core.warm();compressor=make("mrs.compressor");drive=make("mrs.drive");
        for(std::size_t i=0;i<data.size();++i)data[i]=.4f*std::sin((i/channels)*.017f);auto oracle=data;process(*compressor,oracle);process(core,oracle);process(*drive,oracle);process(rig,data);for(std::size_t i=0;i<data.size();++i)check(std::abs(data[i]-oracle[i])<3e-6,"Pedal routing matches independent chain");
        fx.rig.pre_fx[0].bypass=true;fx.rig.post_fx[0].bypass=true;const auto dry=insert_graph(std::array{fx});for(const auto& v:dry.nodes.front().parameters)rig.set_parameter(v.id,v.value);rig.warm();core.reset();std::fill(data.begin(),data.end(),.1f);oracle=data;process(core,oracle);process(rig,data);check(std::abs(data.back()-oracle.back())<1e-7,"Pedal bypass exact core parity");
        float previous=data.back(),worst{};for(int edit=0;edit<64;++edit){fx.rig.pre_fx[0].bypass=edit%8==0;fx.rig.post_fx[0].bypass=edit%9==0;fx.rig.pre_fx[0].parameters[0].value=edit%2?-12.f:-36.f;fx.rig.post_fx[0].parameters[0].value=edit%2?24.f:0.f;const auto state=insert_graph(std::array{fx});for(const auto& v:state.nodes.front().parameters)rig.set_parameter(v.id,v.value);std::fill_n(data.begin(),128*channels,.1f);allocations=deallocations=0;probe=true;run(rig,std::span(data).first(128*channels),channels,128);probe=false;check(!allocations&&!deallocations,"Pedal edits/bypass no callback heap");for(std::size_t i=0;i<128*channels;i+=channels){worst=std::max(worst,std::abs(data[i]-previous));previous=data[i];}}check(worst<.005f,"Pedal parameter and bypass changes bounded discontinuity");

    }
}
void rig_chain_runtime(){
    for(const auto rate:{44100u,48000u,96000u})for(const auto channels:{1u,2u}){
        mrs::NativeInsert fx;fx.id=mrs::new_id();fx.kind=mrs::InsertKind::rig;fx.ir.mix=.5f;
        fx.rig.pre_fx={mrs::make_rig_module("mrs.gain")};fx.rig.pre_fx[0].parameters[0].value=.5f;
        fx.rig.post_fx={mrs::make_rig_module("mrs.gain")};fx.rig.post_fx[0].parameters[0].value=.25f;
        ReverbProcessor dsp(true,true);dsp.prepare({rate,channels,128});auto ir=impulse(rate,32,ReverbRouting::mono);check(dsp.stage_impulse(ir),"Chain core stage");
        auto apply=[&]{const auto graph=insert_graph(std::array{fx});for(const auto& p:graph.nodes.front().parameters)check(dsp.set_parameter(p.id,p.value),"Rack snapshot accepted");};apply();dsp.warm();
        std::vector<float> block(4096*channels,.2f);allocations=deallocations=0;probe=true;run(dsp,block,channels,113);probe=false;check(!allocations&&!deallocations,"Rack process alloc/free");for(auto value:block)check(std::abs(value-.01375f)<1e-6,"Input PRE -> one core and Mix -> POST -> output order");
        fx.rig.pre_fx[0].bypass=true;apply();std::fill(block.begin(),block.end(),.2f);probe=true;run(dsp,block,channels,64);probe=false;check(std::abs(block.back()-.0275f)<1e-6,"Module bypass reaches exact dry utility output");
        float worst{};for(int edit=0;edit<60;++edit){fx.rig.pre_fx[0].bypass=edit%2!=0;apply();std::fill_n(block.data(),128*channels,.2f);probe=true;run(dsp,std::span(block).first(128*channels),channels,128);probe=false;for(std::size_t i=channels;i<128*channels;++i)worst=std::max(worst,std::abs(block[i]-block[i-channels]));}check(worst<.002f,"Repeated chain changes use bounded discontinuity fade");check(!allocations&&!deallocations,"Repeated PRE/POST changes have no callback heap");
        // Independent existing native processors are the numerical oracle. Their
        // output must match the same utilities inside one Rig, including filters.
        fx.rig.pre_fx={mrs::make_rig_module("mrs.highpass"),mrs::make_rig_module("mrs.eq")};fx.rig.post_fx={mrs::make_rig_module("mrs.lowpass")};fx.rig.pre_fx[0].parameters[0].value=80;fx.rig.pre_fx[1].parameters[0].value=800;fx.rig.pre_fx[1].parameters[2].value=3;fx.rig.post_fx[0].parameters[0].value=8000;apply();dsp.warm();
        std::array<std::unique_ptr<IProcessor>,3> utilities;for(std::size_t i=0;i<3;++i){const auto& m=i<2?fx.rig.pre_fx[i]:fx.rig.post_fx[0];NodeState n;n.processor_id=m.type_id;utilities[i]=native_factory(n);utilities[i]->prepare({rate,channels,128});for(const auto& p:m.parameters)check(utilities[i]->set_parameter(p.id,p.value),"Oracle parameter");}
        ReverbProcessor core(true);core.prepare({rate,channels,128});core.set_parameter(1,.5f);check(core.stage_impulse(ir),"Oracle core");core.warm();
        std::vector<float> expected(block.size());for(std::size_t i=0;i<block.size();++i)block[i]=expected[i]=.1f*std::sin(static_cast<float>(i/channels)*.13f);MidiBuffer midi;
        for(std::size_t start=0;start<block.size();){const auto frames=static_cast<std::uint32_t>(std::min<std::size_t>(128,(block.size()-start)/channels));auto span=std::span(expected).subspan(start,frames*channels);for(std::size_t i=0;i<2;++i)utilities[i]->process({span,frames,channels,{},{},midi});core.process({span,frames,channels,{},{},midi});utilities[2]->process({span,frames,channels,{},{},midi});start+=frames*channels;}
        probe=true;run(dsp,block,channels,128);probe=false;for(std::size_t i=0;i<block.size();++i)check(std::abs(block[i]-expected[i])<2e-6,"Native utility reuse matches independent oracle");check(!allocations&&!deallocations,"Native filtered chain alloc/free");check(dsp.latency()==0,"Only zero-latency supported utility modules");
    }
    for(const auto frames:{128u,256u,512u,1024u})for(const auto rigs:{1u,2u,4u}){
        std::vector<std::unique_ptr<ReverbProcessor>> processors;std::vector<float> data(frames*2,.001f);MidiBuffer midi;
        for(unsigned n=0;n<rigs;++n){auto p=std::make_unique<ReverbProcessor>(true,true);p->prepare({48000,2,frames});check(p->stage_impulse(impulse(48000,9600)),"Bench Rig core");mrs::NativeInsert fx;fx.id=mrs::new_id();fx.kind=mrs::InsertKind::rig;for(unsigned i=0;i<8;++i){fx.rig.pre_fx.push_back(mrs::make_rig_module(i%2?"mrs.drive":"mrs.compressor"));fx.rig.post_fx.push_back(mrs::make_rig_module(i%2?"mrs.compressor":"mrs.drive"));}const auto graph=insert_graph(std::array{fx});for(const auto& v:graph.nodes.front().parameters)p->set_parameter(v.id,v.value);p->warm();processors.push_back(std::move(p));}
        double maximum{},total{};allocations=deallocations=0;for(unsigned n=0;n<320;++n){std::fill(data.begin(),data.end(),.001f);const auto start=std::chrono::steady_clock::now();probe=true;for(auto& p:processors)p->process({data,frames,2,{},{},midi});probe=false;const auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();if(n>=64){maximum=std::max(maximum,us);total+=us;}}
        check(!allocations&&!deallocations,"16-module multi-Rig callback heap");std::cout<<"Rig rack 16 modules each rigs="<<rigs<<" block="<<frames<<" mean_us="<<total/256<<" max_us="<<maximum<<" deadline_us="<<1e6*frames/48000<<" heap=0/0 ASIO=NOT_TESTED\n";
    }
}
void rig_chain_concurrent(bool pedals=false){
    mrs::NativeInsert fx;fx.id=mrs::new_id();fx.kind=mrs::InsertKind::rig;
    for(int i=0;i<8;++i){fx.rig.pre_fx.push_back(mrs::make_rig_module(pedals?"mrs.compressor":"mrs.gain"));fx.rig.post_fx.push_back(mrs::make_rig_module(pedals?"mrs.drive":"mrs.gain"));}
    auto state=insert_graph(std::array{fx});PreparedGraph graph({std::make_shared<const GraphState>(state),0,false,false},{48000,2,128});auto ir=impulse(48000,64);ir.kernels[0][0]=1;check(graph.stage_reverb(fx.id,ir),"Shared graph core stage");
    std::atomic<bool> done{},valid{true};std::atomic<std::size_t> heap{},frees{};std::atomic<double> maximum{};
    std::thread audio([&]{std::array<float,256> data;allocations=deallocations=0;probe=true;unsigned after{};while(!done.load(std::memory_order_acquire)||after++<256){std::fill(data.begin(),data.end(),.1f);const auto start=std::chrono::steady_clock::now();graph.process(data.data(),128,0,true);maximum.store(std::max(maximum.load(),std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()));for(const auto value:data)if(!std::isfinite(value)||std::abs(value)>2)valid=false;}probe=false;heap=allocations;frees=deallocations;});
    for(unsigned n=0;n<100;++n){for(auto* lane:{&fx.rig.pre_fx,&fx.rig.post_fx})for(auto& m:*lane){m.parameters[0].value=pedals?(m.type_id=="mrs.compressor"?(n%2?-20.f:-30.f):(n%2?12.f:6.f)):(n%2?.9f:.8f);if(pedals)m.bypass=n%3==0;}state=insert_graph(std::array{fx});while(!graph.enqueue_parameters(state))std::this_thread::yield();}
    if(pedals){for(auto* lane:{&fx.rig.pre_fx,&fx.rig.post_fx})for(auto& m:*lane)m.bypass=true;state=insert_graph(std::array{fx});while(!graph.enqueue_parameters(state))std::this_thread::yield();}
    done.store(true,std::memory_order_release);audio.join();check(valid&&!heap&&!frees,"Concurrent structural snapshots have finite audio and no callback heap");std::array<float,256> data;for(int i=0;i<8;++i){std::fill(data.begin(),data.end(),.1f);graph.process(data.data(),128,0,true);}check(std::abs(data.back()-(pedals?.1f:.1f*std::pow(.9f,16)))<1e-5,"Last whole snapshot wins coherently on one shared graph");
    std::cout<<(pedals?"Pedal":"Utility")<<" Rig 100 concurrent snapshot edits: max_callback_us="<<maximum.load()<<" deadline_us=2666.67 heap=0/0; ASIO=NOT_TESTED\n";
}
void cab_runtime(){
    for(const auto rate:{44100u,48000u,96000u})for(const auto channels:{1u,2u}){
        ReverbProcessor cab(true);cab.prepare({rate,channels,128});auto ir=impulse(rate,rate/5,channels==1?ReverbRouting::mono:ReverbRouting::stereo);
        check(cab.stage_impulse(ir),"Cab stage");cab.warm();std::vector<float> buffer(600*channels);buffer[0]=1;if(channels==2)buffer[1]=1;
        probe=true;run(cab,buffer,channels,127);probe=false;check(allocations==0&&deallocations==0,"Cab RT heap");check(std::abs(buffer[0]-.1f)<1e-6,"Cab neutral");if(channels==2)check(std::abs(buffer[1]-.2f)<1e-6,"Cab diagonal stereo");
        cab.set_parameter(4,1);cab.warm();std::fill(buffer.begin(),buffer.end(),0.f);buffer[0]=1;run(cab,buffer,channels);check(std::abs(buffer[0]+.1f)<1e-6,"Cab polarity");
        cab.set_parameter(1,0);cab.warm();std::fill(buffer.begin(),buffer.end(),0.f);buffer[0]=1;run(cab,buffer,channels);check(buffer[0]==1,"Cab dry mix");
        cab.set_parameter(1,1);cab.set_parameter(4,0);cab.warm();std::vector<float> constant(rate*channels,1.f);auto second=impulse(rate,rate/2,channels==1?ReverbRouting::mono:ReverbRouting::stereo);second.kernels[0][0]=.3f;
        check(cab.stage_impulse(second),"Cab replacement");probe=true;run(cab,constant,channels,113);probe=false;check(allocations==0&&deallocations==0,"Cab switching heap");for(std::size_t j=channels;j<constant.size();++j)check(std::abs(constant[j]-constant[j-channels])<.02f,"Cab crossfade discontinuity");check(!cab.switching(),"Cab acknowledgement");check(std::abs(constant.back()-(channels==1?.3f:.2f))<1e-5,"Cab switch final gain");cab.collect_retired();
    }
}
void numerical(){
    // Dense direct-convolution oracle crossing every partition boundary, arbitrary blocks.
    for(auto routing:{ReverbRouting::mono,ReverbRouting::stereo,ReverbRouting::true_stereo}){
        auto ir=impulse(48000,17431,routing);const auto paths=routing==ReverbRouting::mono?1:routing==ReverbRouting::stereo?2:4;
        for(int p=0;p<paths;++p)for(std::size_t i=0;i<ir.kernels[p].size();++i)ir.kernels[p][i]=static_cast<float>(std::sin(i*.137+p)*.002);
        ir.kernels[0][0]=.7f;std::vector<float> input(18500*2),expected(input.size());
        for(std::size_t i=0;i<101;++i){input[i*2]=static_cast<float>(std::sin(i*.31));input[i*2+1]=static_cast<float>(std::cos(i*.47)*.6);}
        for(std::size_t i=0;i<101;++i)for(std::size_t tap=0;tap<ir.kernels[0].size();++tap){
            const auto at=(i+tap)*2;expected[at]+=input[i*2]*ir.kernels[0][tap];
            expected[at+1]+=input[i*2+1]*ir.kernels[routing==ReverbRouting::mono?0:routing==ReverbRouting::stereo?1:3][tap];
            if(routing==ReverbRouting::true_stereo){expected[at]+=input[i*2+1]*ir.kernels[2][tap];expected[at+1]+=input[i*2]*ir.kernels[1][tap];}}
        ReverbProcessor dsp;setup(dsp,ir);auto result=input;allocations=deallocations=0;probe=true;run(dsp,result,2,73);probe=false;
        check(allocations==0&&deallocations==0,"FFT callback allocation/destruction");
        float error{};for(std::size_t i=0;i<result.size();++i)error=std::max(error,std::abs(result[i]-expected[i]));
        check(error<2e-5f,"direct convolution / True Stereo mismatch");
        dsp.reset();std::fill(result.begin(),result.end(),0.f);run(dsp,result,2,511);for(auto v:result)check(v==0,"reset retained a tail");
    }
    // Long tail, original leading delay and unnormalised amplitude at all supported rates.
    for(auto rate:{44100u,48000u,96000u}){
        auto ir=impulse(rate,rate*20);std::fill(ir.kernels[0].begin(),ir.kernels[0].end(),0.f);
        for(auto at:{std::size_t{17},std::size_t{127},std::size_t{128},std::size_t{2047},std::size_t{2048},std::size_t{16383},std::size_t{16384},ir.kernels[0].size()-1})ir.kernels[0][at]=.375f;
        ReverbProcessor dsp;setup(dsp,ir,rate,1);std::vector<float> result(ir.kernels[0].size()+256);result[0]=1;run(dsp,result,1,509);
        float error{};for(std::size_t i=0;i<result.size();++i)error=std::max(error,std::abs(result[i]-(i<ir.kernels[0].size()?ir.kernels[0][i]:0)));
        check(error<2e-5f,"20 second tail truncated or delayed");
        std::cout<<"20s "<<rate<<" Hz: error="<<error<<", prepared="<<dsp.prepared_bytes()<<" bytes\n";
    }
}
void long_matrix_tail(){
    for(auto rate:{48000u,96000u}){
        auto ir=impulse(rate,rate*20,ReverbRouting::true_stereo);
        for(std::size_t ch=0;ch<4;++ch){
            ir.kernels[ch][128]=-.02f*(ch+1);ir.kernels[ch][2048]=.01f*(ch+1);
            ir.kernels[ch][16384]=-.03f*(ch+1);ir.kernels[ch].back()=.05f*(ch+1);
        }
        ReverbProcessor dsp;setup(dsp,ir,rate);const auto length=ir.kernels[0].size();std::vector<float> data((length+4096)*2);data[0]=.5f;data[29*2+1]=-.25f;
        allocations=deallocations=0;probe=true;run(dsp,data,2,256);probe=false;
        check(!allocations&&!deallocations,"20s matrix callback allocation/destruction");
        float error{};for(std::size_t f=0;f<data.size()/2;++f)for(std::size_t ch=0;ch<2;++ch){
            const auto expected=(f<length?.5f*ir.kernels[ch][f]:0)+(f>=29&&f-29<length?-.25f*ir.kernels[ch+2][f-29]:0);
            error=std::max(error,std::abs(data[f*2+ch]-expected));
        }
        check(error<2e-6f,"long shared FFT/mirrored spectrum job missed output deadline");
        std::cout<<"20s True Stereo "<<rate<<" Hz: error="<<error<<", alloc/free=0/0\n";
    }
}
void silence_and_resume(){
    // Arbitrary silent gaps must not alter convolution phase or lose the last
    // tap. Includes tiny signals (no amplitude gate), matrix paths, full wet
    // pre-delay and Tone draining after the finite convolution becomes idle.
    for(auto routing:{ReverbRouting::mono,ReverbRouting::stereo,ReverbRouting::true_stereo})for(auto channels:{1u,2u}){
        auto ir=impulse(48000,18001,routing);const auto paths=routing==ReverbRouting::mono?1:routing==ReverbRouting::stereo?2:4;
        for(int ch=0;ch<paths;++ch)for(std::size_t t=0;t<ir.kernels[ch].size();++t)ir.kernels[ch][t]=static_cast<float>(.002*std::cos(t*.131+ch));
        constexpr std::size_t total=74000;std::vector<float> data(total*channels),expected(data.size());
        for(auto onset:{177u,25031u,51013u}){
            const auto scale=onset==51013?1e-18f:1.f;
            for(std::size_t ch=0;ch<channels;++ch)data[onset*channels+ch]=scale*(ch==0?.5f:-.3f);
            for(std::size_t t=0;t<ir.kernels[0].size();++t)for(std::size_t ch=0;ch<channels;++ch){
                const auto path=channels==1?0:routing==ReverbRouting::mono?0:routing==ReverbRouting::stereo?ch:ch==0?0:3;
                expected[(onset+t)*channels+ch]+=data[onset*channels+ch]*ir.kernels[path][t];
                if(channels==2&&routing==ReverbRouting::true_stereo)expected[(onset+t)*channels+ch]+=data[onset*channels+(1-ch)]*ir.kernels[ch==0?2:1][t];
            }
        }
        ReverbProcessor dsp;setup(dsp,ir,48000,channels);allocations=deallocations=0;probe=true;run(dsp,data,channels,251);probe=false;
        check(!allocations&&!deallocations,"silence/resume callback allocation/destruction");
        for(std::size_t f=0;f<total;++f)for(std::size_t ch=0;ch<channels;++ch){
            const auto at=f*channels+ch;const auto tolerance=f>=51013?2e-23f:2e-6f;
            check(std::abs(data[at]-expected[at])<tolerance,"silent gap changed IR phase/tail or gated tiny input");
        }
        setup(dsp,ir,48000,channels,1,200,6);data.assign(total*channels,0.f);data[177*channels]=1;run(dsp,data,channels,257);
        // Compare a delayed onset against the same processor starting at frame 0.
        ReverbProcessor origin;setup(origin,ir,48000,channels,1,200,6);std::vector<float> reference(data.size());reference[0]=1;run(origin,reference,channels,127);
        for(std::size_t f=177;f<total;++f)for(std::size_t ch=0;ch<channels;++ch)
            check(std::abs(data[f*channels+ch]-reference[(f-177)*channels+ch])<2e-6f,"idle convolution cut delayed/Tone tail");
        for(std::size_t f=177+dsp.tail_frames()+1;f<total;++f)for(std::size_t ch=0;ch<channels;++ch)check(data[f*channels+ch]==0,"idle Tone did not settle");
    }
    ReverbProcessor dsp;auto ir=impulse(48000,2049,ReverbRouting::true_stereo);setup(dsp,ir);
    // Pending publication and retirement must complete even with no audio input.
    check(dsp.stage_impulse(ir),"silent stage");std::vector<float> zeros(8000*2);run(dsp,zeros,2,256);
    check(!dsp.switching(),"silent optimization stalled crossfade acknowledgement");dsp.collect_retired();
    for(auto v:zeros)check(v==0,"silent kernel switch generated output");
    std::cout<<"Finite silence, arbitrary resume, tiny input, wet delay/Tone and silent handoff PASS\n";
}
void input_gate(){
    for(auto rate:{44100u,48000u,96000u})for(auto block:{128u,256u,512u}){
        auto ir=impulse(rate,rate/5,ReverbRouting::true_stereo);
        for(auto& kernel:ir.kernels){kernel.back()=.25f;}
        ReverbProcessor dsp;setup(dsp,ir,rate);
        check(dsp.set_parameter(3,1)&&dsp.set_parameter(4,-60),"gate controls");dsp.reset();
        std::vector<float> data((rate/3)*2,1e-5f);
        allocations=deallocations=0;probe=true;run(dsp,data,2,block);probe=false;
        check(!allocations&&!deallocations,"gate RT allocation/destruction");
        for(auto v:data)check(v==0,"gate leaked quiet continuous wet input");
        // Above-threshold left opens both channels, retaining the quiet right
        // input and the entire delayed True Stereo tail after gate closure.
        dsp.reset();std::fill(data.begin(),data.end(),0.f);data[0]=1;data[1]=1e-5f;
        run(dsp,data,2,block);
        check(std::abs(data[0]-(.1f+3e-6f))<1e-6f&&std::abs(data[1]-(.2f+4e-6f))<1e-6f,"linked gate clipped onset/stereo input");
        check(std::abs(data[(rate/5-1)*2]-.2500025f)<1e-6f,"closing gate truncated IR tail");
        for(std::size_t n=rate/5*2;n<data.size();++n)check(std::abs(data[n])<1e-6f,"closed gate retained output after finite tail");
        setup(dsp,ir,rate,2,0,200,6);dsp.set_parameter(3,1);dsp.set_parameter(4,-60);dsp.reset();
        std::fill(data.begin(),data.end(),1e-5f);run(dsp,data,2,block);
        for(auto v:data)check(v==1e-5f,"wet gate altered dry output");
        setup(dsp,ir,rate,2,1,200,6);dsp.set_parameter(3,1);dsp.reset();
        ReverbProcessor reference;setup(reference,ir,rate,2,1,200,6);
        data.assign((rate/2+128)*2,0);data[0]=.5f;auto expected=data;
        run(dsp,data,2,block);run(reference,expected,2,173);
        for(std::size_t n=0;n<data.size();++n)check(std::abs(data[n]-expected[n])<1e-6f,"gate cut wet pre-delay/Tone tail");
    }
    auto ir=impulse(48000,1);ir.kernels[0][0]=1;
    ReverbProcessor dsp;setup(dsp,ir,48000,1);dsp.set_parameter(3,1);dsp.set_parameter(4,-60);dsp.reset();
    std::vector<float> data(3600,.0007f);data[0]=.002f;run(dsp,data,1);
    check(data.back()==.0007f,"6dB hysteresis did not retain gate");
    std::fill(data.begin(),data.end(),.0001f);run(dsp,data,1);
    check(data[2000]==.0001f&&data[2600]>0&&data[2600]<.0001f&&data[3200]==0,"gate hold/release did not settle to zero");
    std::fill(data.begin(),data.end(),.002f);run(dsp,data,1);check(data[0]==.002f,"gate wake lost transient");
    check(!dsp.set_parameter(3,.5f)&&!dsp.set_parameter(3,std::numeric_limits<float>::quiet_NaN())&&!dsp.set_parameter(4,-59.9f)&&!dsp.set_parameter(4,-120.1f),"invalid gate control accepted");
    dsp.reset();data.assign(128,.0001f);MidiBuffer midi;std::array<ParameterChange,1> event{{{17,3,0}}};
    allocations=deallocations=0;probe=true;dsp.process({data,128,1,{},event,midi});probe=false;
    check(!allocations&&!deallocations&&data[16]==0&&data[17]==.0001f,"gate event offset/dry input restore");
    // Turning the optional gate back on during processing closes smoothly.
    dsp.set_parameter(3,1);data.assign(4000,.0001f);run(dsp,data,1);
    check(data[0]>0&&data[1000]==0,"live gate enable did not fade closed");
    data.assign(128,.0001f);std::array<ParameterChange,2> changes{{{17,4,-120},{33,4,-60}}};
    allocations=deallocations=0;probe=true;dsp.process({data,128,1,{},changes,midi});probe=false;
    check(!allocations&&!deallocations&&data[16]==0&&data[17]==.0001f&&dsp.parameter_value(4)==-60.f,"live threshold events/RT allocation");
    std::cout<<"Optional wet gate: rates/blocks, linked stereo, full tail, dry, hold/hysteresis, events, alloc/free0 PASS\n";
}
void controls(){
    auto ir=impulse(48000,1);ir.kernels[0][0]=.5f;
    for(float mix:{0.f,.25f,1.f}){ReverbProcessor dsp;setup(dsp,ir,48000,2,mix);std::vector<float> data(200,1);run(dsp,data,2);for(auto v:data)check(std::abs(v-(1-mix*.5f))<1e-6f,"linear Mix");}
    ReverbProcessor dsp;setup(dsp,ir,48000,1,1,10);std::vector<float> data(700);data[0]=1;run(dsp,data,1);
    for(std::size_t i=0;i<data.size();++i)check(std::abs(data[i]-(i==480?.5f:0.f))<1e-6f,"wet pre-delay");
    setup(dsp,ir,48000,1,0,200,6);std::fill(data.begin(),data.end(),.3f);run(dsp,data,1);for(auto v:data)check(std::abs(v-.3f)<1e-6f,"Tone changed dry signal");
    for(float tone:{-6.f,6.f}){setup(dsp,ir,48000,1,1,0,tone);data.resize(48000);for(std::size_t i=0;i<data.size();++i)data[i]=(i&1)?-1.f:1.f;run(dsp,data,1);const float amplitude=std::abs(data.back());check(tone<0?amplitude<.35f:amplitude>.8f,"Tone high shelf direction");}
    setup(dsp,ir,48000,1,0);check(dsp.set_parameter(2,1),"Mix target");data.assign(48000,1);run(dsp,data,1);
    check(data[0]>.99f&&std::abs(data.back()-.5f)<2e-5f,"20ms smoothing");
    check(!dsp.set_parameter(0,201)&&!dsp.set_parameter(1,-7)&&!dsp.set_parameter(2,std::numeric_limits<float>::quiet_NaN()),"invalid parameters");
    setup(dsp,ir,48000,1,1);data.assign(200,1);MidiBuffer midi;std::array<ParameterChange,1> events{{{17,2,0}}};
    dsp.process({data,200,1,{},events,midi});for(std::size_t f=0;f<17;++f)check(data[f]==.5f,"automation applied before its offset");
    check(data[17]>.5f&&data[17]<.502f&&data.back()>data[17],"offset Mix event did not smooth");
    setup(dsp,ir,48000,1,1,200,6);check(dsp.tail_frames()==10080,"Tone/delay tail bound");
    data.assign(10200,0);data[0]=1;run(dsp,data,1);for(std::size_t f=10081;f<data.size();++f)check(data[f]==0,"Tone exceeded reported settling bound");
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
    const auto previous=_mm_getcsr();_mm_setcsr(previous&~0x8040U);const auto caller=_mm_getcsr();
    data.assign(128,std::numeric_limits<float>::denorm_min());run(dsp,data,1);check(_mm_getcsr()==caller,"processor changed caller denormal mode");_mm_setcsr(previous);
#endif
}
void resampling_and_graph(){
    for(auto rates:{std::array{24000u,48000u},std::array{96000u,48000u},std::array{44100u,96000u}}){
        auto ir=impulse(rates[0],rates[0]/10);std::fill(ir.kernels[0].begin(),ir.kernels[0].end(),0.f);ir.kernels[0][rates[0]/20]=.75f;
        ReverbProcessor dsp;setup(dsp,ir,rates[1],1);std::vector<float> data(rates[1]/10+100);data[0]=1;run(dsp,data,1,97);
        const auto peak=std::max_element(data.begin(),data.end())-data.begin();double sum{};for(auto v:data)sum+=v;
        check(std::abs(peak-static_cast<std::ptrdiff_t>(rates[1]/20))<=1&&std::abs(sum-.75)<.003,"resampling timing or gain");
    }
    GraphState state;state.id={"reverb-graph"};state.patch_name="DSP fixture";
    state.nodes={{{"reverb"},"mr_reverb",ProcessorFormat::native,{},{{2,1}},false}};
    state.edges={{std::nullopt,{"reverb"},1}};state.outputs={{{"reverb"}}};state.validate();
    PreparedGraph graph{{std::make_shared<const GraphState>(state),0,false,false},{48000,2,128}};
    auto ir=impulse(48000,20000,ReverbRouting::true_stereo);ir.kernels[0][17000]=.25f;
    check(graph.stage_reverb({"reverb"},ir)&&graph.latency().output==0,"shared graph / PDC");
    std::array<float,256> block{};for(int n=0;n<31;++n)graph.process(block.data(),128);
    graph.panic();block.fill(0);block[0]=1;graph.process(block.data(),128);
    check(std::abs(block[0]-.1f)<1e-6f&&std::abs(block[1]-.2f)<1e-6f,"graph True Stereo input L");
    graph.panic();block.fill(0);graph.process(block.data(),128);for(auto v:block)check(v==0,"graph panic retained tail");
    state.nodes[0].bypass=true;check(graph.enqueue_parameters(state),"bypass queue");block.fill(.3f);graph.process(block.data(),128);for(auto v:block)check(v==.3f,"shared bypass");
    check(graph.stage_reverb({"reverb"},ir),"stage while bypassed");allocations=deallocations=0;probe=true;graph.process(block.data(),128);probe=false;
    check(allocations==0&&deallocations==0,"bypassed IR handoff allocated/destroyed on DSP");
    check(graph.stage_reverb({"reverb"},ir),"bypass left previous handoff stuck");graph.process(block.data(),128);
    graph.collect_reverb_retired();
}
void validation_and_handoff(){
    std::array<float,8> quad{1,2,3,4,0,0,0,0};rejects([&]{reverb_impulse(48000,4,quad);});
    auto mapped=reverb_impulse(48000,4,quad,ReverbQuadMap::ll_lr_rl_rr);check(mapped.kernels[2][0]==3,"quad map");
    auto left=impulse(48000,3,ReverbRouting::stereo),right=left;auto pair=reverb_pair(left,right);check(pair.routing==ReverbRouting::true_stereo,"pair mode");right.sample_rate=44100;rejects([&]{reverb_pair(left,right);});
    auto invalid=left;invalid.kernels[0][0]=std::numeric_limits<float>::infinity();rejects([&]{invalid.validate();});
    ReverbProcessor dsp;auto a=impulse(48000,1);a.kernels[0][0]=.2f;auto b=a;b.kernels[0][0]=.8f;setup(dsp,a,48000,1);
    const auto initial=dsp.prepared_bytes();check(dsp.stage_impulse(b),"swap stage");check(!dsp.stage_impulse(a),"unbounded swap queue");
    std::vector<float> data(5000,1);allocations=deallocations=0;probe=true;run(dsp,data,1,64);dsp.reset();probe=false;
    check(allocations==0&&deallocations==0,"RT allocation/destruction");check(!dsp.switching(),"crossfade completion");
    check(std::abs(data.front()-.2f)<1e-5f&&std::abs(data.back()-.8f)<1e-5f,"crossfade endpoints");
    for(std::size_t i=1;i<3840;++i)check(data[i]>=data[i-1]&&data[i]-data[i-1]<.001f,"crossfade discontinuity");
    dsp.collect_retired();check(dsp.prepared_bytes()==initial,"old kernel retained");
    for(int i=0;i<8;++i){check(dsp.stage_impulse(a),"quiescent swap");dsp.warm();check(dsp.prepared_bytes()==initial,"warm leaked old owner");}
    rejects([&]{dsp.stage_impulse(invalid);});check(!dsp.switching(),"failed load left busy");
    ReverbProcessor budget;budget.prepare({192000,2,128});auto too_large=impulse(48000,48000*20,ReverbRouting::true_stereo);
    rejects([&]{budget.stage_impulse(too_large);});check(!budget.switching(),"budget rejection left busy");
    std::cout<<"callback allocations="<<allocations<<", deallocations="<<deallocations<<"\n";
    check(dsp.capture().class_id=="mr_reverb","generic native/VST capture; external identity belongs to model");
}
void concurrent_handoff(){
    ReverbProcessor dsp;auto ir=impulse(48000,2049);setup(dsp,ir,48000,1);
    std::atomic<bool> done{};std::atomic<std::size_t> rt_allocations{},rt_deallocations{};
    std::thread consumer([&]{std::array<float,128> block{};MidiBuffer midi;probe=true;allocations=deallocations=0;
        while(!done.load()||dsp.switching()){block.fill(.2f);dsp.process({block,128,1,{},{},midi});}
        probe=false;rt_allocations=allocations;rt_deallocations=deallocations;});
    for(int n=0;n<100;++n){ir.kernels[0][0]=n&1?.2f:.4f;while(!dsp.stage_impulse(ir))std::this_thread::yield();}
    done=true;consumer.join();dsp.collect_retired();
    check(rt_allocations==0&&rt_deallocations==0,"concurrent IR handoff allocated/freed on DSP thread");
    std::cout<<"100 concurrent swaps: callback allocations="<<rt_allocations<<", deallocations="<<rt_deallocations<<"\n";
}
void wav_loading(){
    // Synthetic temporary RIFF fixtures; no user IR library or bundled assets.
    const auto temp=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());
    const auto root=temp/("mrs-reverb-"+mrs::new_id().value);
    check(root.parent_path()==temp&&!std::filesystem::exists(root),"temporary fixture path");std::filesystem::create_directories(root);
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}}cleanup{root};
    const auto write=[&](const char* name,std::uint32_t rate,std::uint16_t channels,std::uint16_t bits,bool floating,std::uint32_t frames){
        const auto path=root/name;std::ofstream file(path,std::ios::binary);
        const auto word=[&](std::uint32_t value,int bytes){for(int n=0;n<bytes;++n)file.put(static_cast<char>((value>>(n*8))&255));};
        const auto size=frames*channels*(bits/8);file.write("RIFF",4);word(36+size+(size&1),4);file.write("WAVEfmt ",8);word(16,4);word(floating?3:1,2);word(channels,2);word(rate,4);word(rate*channels*(bits/8),4);word(channels*(bits/8),2);word(bits,2);file.write("data",4);word(size,4);
        for(std::uint32_t f=0;f<frames;++f)for(std::uint16_t ch=0;ch<channels;++ch){const auto value=f==17?.125f*(ch+1):0.f;
            word(floating?std::bit_cast<std::uint32_t>(value):static_cast<std::uint32_t>(value*std::ldexp(1.,bits-1)),bits/8);}
        if(size&1)file.put(0);file.close();return path;
    };
    for(auto bits:{16u,24u,32u}){auto path=write("pcm.wav",44100,1,static_cast<std::uint16_t>(bits),false,88201);const auto ir=mrs::audio::load_reverb_ir(path);check(ir.kernels[0].size()==88201&&ir.kernels[0][17]==.125f,"long PCM IR decoder");}
    const auto quad=write("quad.wav",96000,4,32,true,101);rejects([&]{mrs::audio::load_reverb_ir(quad);});
    const auto ir=mrs::audio::load_reverb_ir(quad,ReverbQuadMap::ll_lr_rl_rr);check(ir.kernels[3][17]==.5f,"float quad decoder");
    const auto left=write("left.wav",48000,2,32,true,101),right=write("right.wav",48000,2,32,true,101);
    check(mrs::audio::load_reverb_pair(left,right).routing==ReverbRouting::true_stereo,"dual stereo WAV");
    const auto mismatch=write("mismatch.wav",44100,2,32,true,101);rejects([&]{mrs::audio::load_reverb_pair(left,mismatch);});
    {std::ofstream truncated(root/"broken.wav",std::ios::binary);truncated<<"RIFF";}rejects([&]{mrs::audio::load_reverb_ir(root/"broken.wav");});
}
void user_library(const std::filesystem::path& quad_root,const std::filesystem::path& pair_root){
    // Explicit opt-in local validation; never a CTest dependency or bundled IR asset.
    std::vector<std::filesystem::path> files;
    for(const auto& entry:std::filesystem::directory_iterator(quad_root))if(entry.is_regular_file()&&entry.path().extension()==".wav")files.push_back(entry.path());
    std::sort(files.begin(),files.end());check(!files.empty(),"empty quad test library");
    std::size_t longest{},largest{},count{};float worst{};double maximum_load_ms{};
    for(const auto& file:files){
        const auto load_start=std::chrono::steady_clock::now();
        const auto raw=mrs::audio::load_wav(file,64*1024*1024);
        const auto left=pair_root/(file.stem().string()+" L.wav"),right=pair_root/(file.stem().string()+" R.wav");
        const auto paired=mrs::audio::load_reverb_pair(left,right);
        check(raw.channels==4&&raw.sample_rate==48000&&paired.kernels[0].size()*4==raw.samples.size(),"library headers or pair lengths");
        for(std::size_t f=0;f<paired.kernels[0].size();++f)for(std::size_t ch=0;ch<4;++ch)
            check(raw.samples[f*4+ch]==paired.kernels[ch][f],"quad map differs from explicit L/R reference");
        const auto ir=mrs::audio::load_reverb_ir(file,ReverbQuadMap::ll_lr_rl_rr);ReverbProcessor dsp;setup(dsp,ir);
        maximum_load_ms=std::max(maximum_load_ms,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-load_start).count());
        largest=std::max(largest,dsp.prepared_bytes());const auto frames=ir.kernels[0].size();longest=std::max(longest,frames);
        std::vector<float> data((frames+1100)*2);data[0]=.5f;data[29*2+1]=-.25f;
        allocations=deallocations=0;probe=true;run(dsp,data,2,127);probe=false;
        check(allocations==0&&deallocations==0,"user IR callback allocation/destruction");
        float error{};
        for(std::size_t f=0;f<data.size()/2;++f)for(std::size_t ch=0;ch<2;++ch){
            const float expected=(f<frames?.5f*ir.kernels[ch][f]:0)+(f>=29&&f-29<frames?-.25f*ir.kernels[2+ch][f-29]:0);
            error=std::max(error,std::abs(data[f*2+ch]-expected));}
        check(error<2e-5f,"user IR matrix/tail mismatch");worst=std::max(worst,error);++count;
        if(count%10==0||count==files.size())std::cout<<"library "<<count<<"/"<<files.size()<<" PASS; max_error="<<worst<<std::endl;
    }
    std::cout<<"USER LIBRARY PASS files="<<count<<" quad_pair_channel_error=0 sample_rate=48000 longest_frames="<<longest
        <<" maximum_prepared_bytes="<<largest<<" maximum_combined_decode_pair_prepare_ms="<<maximum_load_ms<<" DSP_max_error="<<worst<<" callback allocations/deallocations=0/0\n";
}
std::shared_ptr<PreparedGraph> engine_reverb(const ReverbImpulse& ir,float predelay=0){
    GraphState state;state.id={"engine-reverb"};state.patch_name="MR Reverb engine fixture";
    state.nodes={{{"reverb"},"mr_reverb",ProcessorFormat::native,{},{{0,predelay},{2,1}},false}};
    state.edges={{std::nullopt,{"reverb"},1}};state.outputs={{{"reverb"}}};
    auto graph=std::make_shared<PreparedGraph>(GraphSnapshot{std::make_shared<const GraphState>(state),0,false,false},ProcessConfig{48000,2,128});
    check(graph->stage_reverb({"reverb"},ir),"engine stage");graph->warm_reverb({"reverb"});return graph;
}
mrs::audio::RenderGraph routed_engine_graph(const std::shared_ptr<PreparedGraph>& reverb,int placement){
    using namespace mrs::audio;
    RenderGraph graph;graph.mixer.resize(2);graph.buses={false,true};graph.outputs={1,no_mixer_track};graph.inserts.resize(2);
    if(placement<2)graph.inserts[placement]=reverb;else graph.master_inserts=reverb;
    const auto asset=std::make_shared<const AudioData>(AudioData{48000,2,{.4f,.2f}});
    graph.voices={{asset,0,0,1,{{0,0,1},{1,1,1}},{},0}};return graph;
}
void engine_transport_and_export(){
    using namespace mrs::audio;
    auto ir=impulse(48000,20001,ReverbRouting::true_stereo);
    for(std::size_t ch=0;ch<4;++ch){ir.kernels[ch][128]=.03f*(ch+1);ir.kernels[ch][2048]=-.01f*(ch+1);ir.kernels[ch].back()=.02f*(ch+1);}
    const auto temp=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());const auto root=temp/("mrs-reverb-render-"+mrs::new_id().value);
    check(root.parent_path()==temp&&!std::filesystem::exists(root),"render fixture path");std::filesystem::create_directories(root);
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}}cleanup{root};
    for(int placement=0;placement<3;++placement){
        auto chain=engine_reverb(ir,10);check(chain->latency().output==0&&chain->reverb_tail_frames({"reverb"})==20480,"tail range includes wet delay, excludes PDC");
        auto graph=routed_engine_graph(chain,placement);AudioEngine engine;engine.prepare({48000,0,2,128,128},graph);
        check(engine.enqueue({ControlKind::play}),"engine play");const std::size_t frames=1+chain->reverb_tail_frames({"reverb"});std::vector<float> rendered(frames*2);
        allocations=deallocations=0;probe=true;
        for(std::size_t first=0;first<frames;first+=127)engine.process(nullptr,rendered.data()+first*2,static_cast<std::uint32_t>(std::min<std::size_t>(127,frames-first)));
        probe=false;check(allocations==0&&deallocations==0,"track/bus/master callback allocation/destruction");
        for(std::size_t f=0;f<frames;++f)for(std::size_t ch=0;ch<2;++ch){const auto tap=f>=480?f-480:ir.kernels[0].size();
            const auto expected=tap<ir.kernels[0].size()?.4f*ir.kernels[ch][tap]+.2f*ir.kernels[2+ch][tap]:0.f;
            check(std::abs(rendered[f*2+ch]-expected)<2e-6f,"engine routing or long tail mismatch");}
        engine.quiesce();
        if(placement==0){
            // Explicit offline range includes the complete wet tail. File I/O stays off DSP.
            Recorder writer(root/"render-with-tail.wav",48000,0,{0,1});
            for(std::size_t first=0;first<frames;first+=128)writer.capture(rendered.data()+first*2,2,static_cast<std::uint32_t>(std::min<std::size_t>(128,frames-first)),static_cast<mrs::Sample>(first));
            const auto saved=writer.finish();check(saved.frames==static_cast<mrs::Sample>(frames)&&saved.status.fault==RecordFault::none,"offline WAV writer");
            const auto decoded=load_wav(saved.path);check(decoded.samples==rendered,"exported wet tail differs from engine output");
        }
        chain->warm_reverb({"reverb"});engine.prepare({48000,0,2,128,128},graph);std::array<float,256> block{};
        check(engine.enqueue({ControlKind::play}),"replay");engine.process(nullptr,block.data(),128);
        check(engine.enqueue({ControlKind::stop}),"stop");engine.process(nullptr,block.data(),128);for(auto v:block)check(v==0,"Stop retained pending tail");
        check(engine.enqueue({ControlKind::play}),"play after stop");engine.process(nullptr,block.data(),128);
        check(engine.enqueue({ControlKind::seek,5000}),"seek");
        for(int n=0;n<165;++n){engine.process(nullptr,block.data(),128);for(auto v:block)check(v==0,"Seek leaked history into new timeline");}
        engine.quiesce();
    }
    auto loop_ir=impulse(48000,129,ReverbRouting::mono);loop_ir.kernels[0][128]=.3f;auto chain=engine_reverb(loop_ir);
    auto graph=routed_engine_graph(chain,0);AudioEngine engine;engine.prepare({48000,0,2,128,128},graph);
    check(engine.enqueue({ControlKind::loop,0,97})&&engine.enqueue({ControlKind::play}),"loop");std::vector<float> output(2000);
    for(std::size_t f=0;f<1000;f+=127)engine.process(nullptr,output.data()+f*2,static_cast<std::uint32_t>(std::min<std::size_t>(127,1000-f)));
    for(std::size_t f=0;f<1000;++f){const auto gain=(f%97==0?.1f:0.f)+(f>=128&&(f-128)%97==0?.3f:0.f);
        check(std::abs(output[f*2]-.4f*gain)<2e-6f&&std::abs(output[f*2+1]-.2f*gain)<2e-6f,"loop wrap lost continuous reverb history");}
    engine.quiesce();std::cout<<"AudioEngine track/bus/Master, Stop/Seek/loop and offline WAV tail PASS\n";
}
void engine_benchmark(){
    using namespace mrs::audio;
    auto ir=impulse(48000,48000*8,ReverbRouting::true_stereo);
    for(std::size_t ch=0;ch<4;++ch)for(std::size_t f=1;f<ir.kernels[ch].size();++f)
        ir.kernels[ch][f]=static_cast<float>(.0001*std::sin(f*.173+ch)*std::exp(-static_cast<double>(f)/192000));
    std::vector<std::shared_ptr<PreparedGraph>> chains;
    for(int n=0;n<16;++n)chains.push_back(engine_reverb(ir));
    std::array<float,256> block{};
    for(int n=0;n<3200;++n)for(auto& graph:chains){block.fill(.001f);graph->process(block.data(),128);}
    auto asset=std::make_shared<AudioData>();asset->sample_rate=48000;asset->channels=2;asset->samples.assign(48000*20*2,.001f);
    std::array<double,2> expected{};for(std::size_t f=0;f<ir.kernels[0].size();++f)for(std::size_t ch=0;ch<2;++ch)expected[ch]+=.001*(ir.kernels[ch][f]+ir.kernels[ch+2][f]);
    for(auto count:{1u,2u,3u,8u,16u})for(auto workers:{1u,4u}){
        RenderGraph graph;graph.mixer.resize(count);graph.inserts.assign(chains.begin(),chains.begin()+count);
        for(std::size_t i=0;i<count;++i)graph.voices.push_back({asset,0,0,asset->frames(),{{0,0,1},{1,1,1}},{},i});
        RenderConfig config{48000,0,2,512,128,workers};config.adaptive_parallel=false;
        AudioEngine engine;engine.prepare(config,graph);check(engine.enqueue({ControlKind::play}),"benchmark play");
        std::array<float,1024> output{};for(int n=0;n<32;++n)engine.process(nullptr,output.data(),128);
        for(auto frames:{128u,256u,512u}){
            std::vector<double> times;times.reserve(256);
            for(int n=0;n<256;++n){const auto start=std::chrono::steady_clock::now();engine.process(nullptr,output.data(),frames);
                times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
                check(std::abs(output[0]-count*expected[0])<1e-5&&std::abs(output[1]-count*expected[1])<1e-5,"worker matrix output mismatch");}
            double sum{};for(auto time:times)sum+=time;std::sort(times.begin(),times.end());const auto metrics=engine.metrics();
            check(!metrics.processing_fault&&metrics.worker_timeouts==0,"worker fault during Reverb benchmark");
            std::cout<<"AudioEngine dense8s True Stereo48k instances="<<count<<" workers="<<workers<<" block="<<frames<<" mean_us="<<sum/times.size()
                <<" p95_us="<<times[243]<<" max_us="<<times.back()<<" deadline_us="<<1e6*frames/48000<<" parallel_batches="<<metrics.parallel_batches<<std::endl;
        }
        engine.quiesce();
    }
    std::cout<<"Offline AudioEngine worker profile complete; ASIO/VSTi acceptance NOT RUN\n";
}
void cab_profile_ir(const std::filesystem::path& file){
    const auto ir=mrs::audio::load_reverb_ir(file);check(ir.routing==ReverbRouting::stereo,"Combo stereo source");
    for(const auto rate:{44100u,48000u,96000u})for(const auto frames:{128u,256u,512u}){
        ReverbProcessor dsp(true);dsp.prepare({rate,2,frames});const auto start=std::chrono::steady_clock::now();check(dsp.stage_impulse(ir),"Combo prepare");dsp.warm();const auto prepare_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        std::vector<float> block(frames*2,.001f);MidiBuffer midi;for(int n=0;n<1000;++n){std::fill(block.begin(),block.end(),.001f);dsp.process({block,frames,2,{},{},midi});}
        std::vector<double> times(1024);allocations=deallocations=0;
        for(auto& t:times){std::fill(block.begin(),block.end(),.001f);probe=true;const auto before=std::chrono::steady_clock::now();dsp.process({block,frames,2,{},{},midi});const auto after=std::chrono::steady_clock::now();probe=false;t=std::chrono::duration<double,std::micro>(after-before).count();}
        check(!allocations&&!deallocations,"Real Combo callback heap");check(dsp.latency()==0,"Combo algorithmic latency");double total{};for(auto t:times)total+=t;std::sort(times.begin(),times.end());
        std::cout<<"Cab Combo rate="<<rate<<" block="<<frames<<" source_frames="<<ir.kernels[0].size()<<" bytes="<<dsp.prepared_bytes()<<" prepare_ms="<<prepare_ms<<" mean_us="<<total/times.size()<<" p95_us="<<times[972]<<" max_us="<<times.back()<<" deadline_us="<<1e6*frames/rate<<" alloc/free=0/0 latency=0\n";
    }
    ReverbProcessor exact(true);exact.prepare({ir.sample_rate,2,128});check(exact.stage_impulse(ir),"Combo oracle prepare");exact.warm();std::vector<float> data((ir.kernels[0].size()+2048)*2);data[0]=.5f;data[1]=-.25f;run(exact,data,2,127);float error{};
    for(std::size_t f=0;f<data.size()/2;++f){error=std::max(error,std::abs(data[f*2]-(f<ir.kernels[0].size()?.5f*ir.kernels[0][f]:0.f)));error=std::max(error,std::abs(data[f*2+1]-(f<ir.kernels[1].size()?-.25f*ir.kernels[1][f]:0.f)));}check(error<2e-5f,"Combo full-tail one-WAV oracle");std::cout<<"Combo full-tail max_error="<<error<<"; offline timings, ASIO/listening not tested\n";
}
void profile_ir(const std::filesystem::path& file){
    const auto ir=mrs::audio::load_reverb_ir(file,ReverbQuadMap::ll_lr_rl_rr);
    std::cout<<"IR frames="<<ir.kernels[0].size()<<" rate="<<ir.sample_rate<<"; offline DSP timing, not ASIO CPU\n";
    for(auto frames:{128u,256u,512u}){
        ReverbProcessor dsp;setup(dsp,ir,48000,2,.25f);
        std::vector<float> block(frames*2);MidiBuffer midi;
        auto process=[&]{dsp.process({block,frames,2,{},{},midi});};
        auto measure=[&](const char* label,bool signal,float scale=1.f){
            std::vector<double> times(1024);allocations=deallocations=0;
            for(std::size_t n=0;n<times.size();++n){
                for(std::size_t f=0;f<frames;++f){block[f*2]=signal?static_cast<float>(.1*std::sin((n*frames+f)*.137)):0.f;block[f*2+1]=signal?static_cast<float>(.07*std::cos((n*frames+f)*.071)):0.f;}
                for(auto& value:block)value*=scale;
                probe=true;const auto start=std::chrono::steady_clock::now();process();const auto end=std::chrono::steady_clock::now();probe=false;
                times[n]=std::chrono::duration<double,std::micro>(end-start).count();
            }
            check(!allocations&&!deallocations,"profile callback allocation/destruction");
            double total{};for(auto t:times)total+=t;std::sort(times.begin(),times.end());
            std::cout<<label<<" block="<<frames<<" mean_us="<<total/times.size()<<" p95_us="<<times[972]<<" max_us="<<times.back()<<" deadline_pct="<<(total/times.size())*48000/(frames*10000.)<<" alloc/free=0/0"<<std::endl;
        };
        measure("fresh_silence",false);
        for(std::size_t n=0;n<(ir.kernels[0].size()+frames-1)/frames+32;++n){std::fill(block.begin(),block.end(),.001f);process();}
        measure("steady_signal",true);
        measure("tiny_signal_minus_140dB",true,1e-6f);
        measure("noise_minus_80dB_gate_OFF",true,.001f);
        for(std::size_t n=0;n<(ir.kernels[0].size()+9600+frames-1)/frames+32;++n){std::fill(block.begin(),block.end(),0.f);process();}
        measure("drained_silence",false);
        check(dsp.set_parameter(3,1)&&dsp.set_parameter(4,-60),"profile gate");
        for(std::size_t n=0;n<(ir.kernels[0].size()+9600+frames-1)/frames+32;++n){std::fill(block.begin(),block.end(),.0001f);process();}
        measure("gated_noise_minus_80dB",true,.001f);
        check(dsp.set_parameter(3,0),"profile gate off");
        check(dsp.set_parameter(0,100)&&dsp.set_parameter(1,6),"profile controls");
        for(int n=0;n<64;++n){std::fill(block.begin(),block.end(),.001f);process();}
        measure("signal_controls",true);
    }
    ReverbProcessor dsp;setup(dsp,ir);const auto length=ir.kernels[0].size();std::vector<float> data((length+4096)*2);data[0]=.5f;data[58+1]=-.25f;
    run(dsp,data,2,256);float error{};
    for(std::size_t f=0;f<data.size()/2;++f)for(std::size_t ch=0;ch<2;++ch){
        const auto expected=(f<length?.5f*ir.kernels[ch][f]:0)+(f>=29&&f-29<length?-.25f*ir.kernels[ch+2][f-29]:0);
        error=std::max(error,std::abs(data[f*2+ch]-expected));
    }
    check(error<2e-5f,"profile IR full-tail oracle");std::cout<<"full-tail max_error="<<error<<std::endl;
}
void benchmark(){
    // Offline throughput plus callback bursts, not hardware ASIO or total system CPU.
    for(auto rate:{48000u,96000u}){auto ir=impulse(rate,rate*20,ReverbRouting::true_stereo);ReverbProcessor dsp;setup(dsp,ir,rate);
        std::vector<float> block(1024,.001f);const auto warm_blocks=(ir.kernels[0].size()+511)/512;
        for(std::size_t n=0;n<warm_blocks;++n){std::fill(block.begin(),block.end(),.001f);run(dsp,block,2,512);}
        for(auto frames:{128u,256u,512u}){std::vector<double> times;times.reserve(1024);
            for(int n=0;n<1024;++n){std::fill(block.begin(),block.end(),.001f);const auto start=std::chrono::steady_clock::now();run(dsp,std::span(block).first(frames*2),2,frames);times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());}
            double total{};for(auto t:times)total+=t;std::sort(times.begin(),times.end());
            std::cout<<"20s True Stereo "<<rate<<"Hz block="<<frames<<" RAM="<<dsp.prepared_bytes()<<" mean_us="<<total/times.size()<<" p95_us="<<times[972]<<" max_us="<<times.back()<<" deadline_us="<<1e6*frames/rate<<"\n";}
    }
    auto ir=impulse(48000,48000*8,ReverbRouting::true_stereo);
    std::vector<std::unique_ptr<ReverbProcessor>> instances;
    for(int n=0;n<16;++n){auto dsp=std::make_unique<ReverbProcessor>();setup(*dsp,ir);instances.push_back(std::move(dsp));}
    std::array<float,1024> block{};
    for(int n=0;n<750;++n)for(auto& dsp:instances){block.fill(.001f);run(*dsp,block,2,512);}
    for(auto count:{1u,2u,3u,8u,16u})for(auto frames:{128u,256u,512u}){
        double total{},maximum{};
        for(int n=0;n<256;++n){const auto start=std::chrono::steady_clock::now();
            for(std::size_t i=0;i<count;++i){block.fill(.001f);run(*instances[i],std::span(block).first(frames*2),2,frames);}
            const auto time=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();total+=time;maximum=std::max(maximum,time);}
        std::cout<<"8s True Stereo 48000Hz instances="<<count<<" block="<<frames<<" RAM="<<instances[0]->prepared_bytes()*count<<" mean_us="<<total/256<<" max_us="<<maximum<<" deadline_us="<<1e6*frames/48000<<"\n";
    }
}
}
int main(int argc,char** argv){try{if(argc==2&&std::string_view(argv[1])=="--rig"){rig_runtime();rig_pedals();rig_chain_runtime();rig_chain_concurrent();rig_chain_concurrent(true);std::cout<<"Rig WAV parity/IO/meters/RT PASS\n";}else if(argc==3&&std::string_view(argv[1])=="--cab-profile-ir")cab_profile_ir(argv[2]);else if(argc==3&&std::string_view(argv[1])=="--profile-ir")profile_ir(argv[2]);else if(argc==4&&std::string_view(argv[1])=="--library")user_library(argv[2],argv[3]);else if(argc==2&&std::string_view(argv[1])=="--engine-benchmark")engine_benchmark();else if(argc>1&&std::string_view(argv[1])=="--benchmark"){benchmark();std::cout<<"Offline profile complete; ASIO acceptance NOT RUN\n";}else{cab_runtime();numerical();long_matrix_tail();silence_and_resume();input_gate();controls();resampling_and_graph();validation_and_handoff();concurrent_handoff();wav_loading();engine_transport_and_export();std::cout<<"MR Reverb DSP PASS\n";}return 0;}catch(const std::exception& e){probe=false;std::cerr<<e.what()<<"\n";return 1;}}
