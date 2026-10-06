#include <mrs/audio.hpp>
#include <mrs/channel_workers.hpp>
#include <mrs/processing.hpp>
#include <mrs/recording.hpp>
#include <mrs/no_denormals.hpp>
#include "latency_fixture.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>
namespace {
std::atomic<bool> allocations_enabled{};
std::atomic<unsigned> allocations{};
void check(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
using namespace mrs;
using namespace mrs::audio;
using namespace mrs::processing;
std::shared_ptr<PreparedGraph> delayed(std::uint32_t latency) {
    auto state=demo_graph();state.nodes.front().parameters.clear();
    return std::make_shared<PreparedGraph>(GraphSnapshot{std::make_shared<const GraphState>(state),0,false,false},
        ProcessConfig{48000,2,128},[latency](const auto&){return std::make_unique<LatencyFixture>(latency);});
}
RenderGraph fixture() {
    RenderGraph g;g.mixer.resize(5);g.buses={false,false,true,true,false};
    g.outputs={2,2,3,no_mixer_track,no_mixer_track};g.hardware_outputs={{},{},{},{},{1}};
    g.sends={{{3,.2f,true}},{{3,.3f,false}}, {}, {},{{2,.1f,true}}};
    g.inserts={delayed(3),delayed(11),delayed(7),delayed(5),delayed(1)};g.master_inserts=delayed(128);
    g.monitor={{0,0,.1f,0},{1,1,.2f,0},{0,0,.3f,1},{1,1,.4f,1},{1,0,.2f,4}};
    g.input_monitoring={true,true,false,false,true};g.master_gain=.7f;
    auto asset=std::make_shared<AudioData>(sine_fixture(48000,2,1024,300));
    g.voices={{asset,0,0,1024,{{0,0,.1f},{1,1,.1f}},{},0},{asset,0,0,1024,{{0,0,.2f},{1,1,.2f}},{},1}};
    g.tempos={{0,100,0},{75,140,1}};return g;
}
void equivalence() {
    for(auto workers:{2U,4U,8U}) {
        AudioEngine serial,parallel;auto a=fixture(),b=fixture();
        serial.prepare({48000,2,2,128,128,1},std::move(a),{PlaybackState::paused,0,LoopRange{0,97}});
        RenderConfig cfg{48000,2,2,128,128,workers};cfg.worker_wait_ms=500;cfg.adaptive_parallel=false;
        parallel.prepare(cfg,std::move(b),{PlaybackState::paused,0,LoopRange{0,97}});
        check(serial.enqueue({ControlKind::play})&&parallel.enqueue({ControlKind::play}),"play queues");
        std::array<float,256> input{},expected{},actual{};
        const auto before=allocations.load();
        for(unsigned n=0;n<200;++n) {
            const auto frames=1U+(n*31)%128;
            for(std::size_t i=0;i<frames*2;++i)input[i]=static_cast<float>(.2*std::sin((i+n)*.1));
            if(n==20||n==60) {
                MixerUpdate m;m.count=5;m.master_gain=n==20?.5f:.7f;
                for(std::size_t t=0;t<5;++t)m.tracks[t]={.8f,static_cast<float>(t)*.1f,n==20&&t==1,n==20&&t==3};
                m.send_gains[0][0]=.4f;m.send_gains[1][0]=.2f;m.send_gains[4][0]=.3f;
                check(serial.enqueue_mix(m)&&parallel.enqueue_mix(m),"mix updates");
            }
            if(n==100||n==110||n==130) {
                Control c{n==100?ControlKind::stop:n==110?ControlKind::seek:ControlKind::play,25};
                check(serial.enqueue(c)&&parallel.enqueue(c),"transport queues");
            }
            allocations_enabled=true;
            serial.process(input.data(),expected.data(),frames);parallel.process(input.data(),actual.data(),frames);
            allocations_enabled=false;
            check(!parallel.metrics().processing_fault,"unexpected worker timeout");
            for(std::size_t i=0;i<frames*2;++i)check(expected[i]==actual[i],"serial/parallel samples differ");
            check(serial.state().sample==parallel.state().sample,"transport differs");
            const auto s=serial.take_meters(),p=parallel.take_meters();
            for(std::size_t t=0;t<5;++t)check(s.tracks[t].left==p.tracks[t].left&&s.tracks[t].right==p.tracks[t].right,"meters differ");
        }
        check(allocations.load()==before,"allocation in callback/worker");
        check(parallel.metrics().parallel_batches>0,"parallel batches missing");
        for(unsigned n=0;n<20;++n){parallel.prepare(cfg,fixture());parallel.process(input.data(),actual.data(),128);parallel.quiesce();}
        parallel.prepare({48000,0,2,128},{});check(parallel.metrics().processing_workers==1,"empty graph fallback");
    }
    AudioEngine engine;auto alias=fixture();alias.inserts[1]=alias.inserts[0];bool rejected=false;
    try{engine.prepare({48000,2,2,128,128,4},alias);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"shared mutable plugin graph accepted");
    auto cycle=fixture();cycle.outputs[3]=2;rejected=false;
    try{engine.prepare({48000,2,2,128,128,4},cycle);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"routing cycle accepted");
}
struct TimeoutContext {
    std::atomic<bool> started{};std::atomic<unsigned> calls{},active{};
    static void job(void* data,std::uint32_t index) noexcept {
        auto& c=*static_cast<TimeoutContext*>(data);++c.calls;
        if(index==1){++c.active;c.started=true;std::this_thread::sleep_for(std::chrono::milliseconds(80));--c.active;}
        else {const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
            while(!c.started.load()&&std::chrono::steady_clock::now()<until)std::this_thread::yield();}
    }
};
void profiling() {
    AudioEngine reference,profiled;RenderConfig c{48000,2,2,128,128,4};c.adaptive_parallel=false;c.worker_wait_ms=500;
    reference.prepare({48000,2,2,128,128,1},fixture());profiled.prepare(c,fixture());
    std::array<float,256> in{},expected{},actual{};in.fill(.2f);
    profiled.process(in.data(),actual.data(),128);reference.process(in.data(),expected.data(),128);
    check(profiled.profile().device_master.calls==0,"disabled profiling collected samples");
    profiled.set_profiling(true);const auto before=allocations.load();
    std::atomic<bool> stop{};std::atomic<bool> invalid{};
    std::jthread reader([&](std::stop_token cancelled){while(!cancelled.stop_requested()&&!stop.load()){const auto p=profiled.profile();if(p.channels!=5)invalid=true;}});
    for(unsigned n=0;n<70;++n){allocations_enabled=true;reference.process(in.data(),expected.data(),128);profiled.process(in.data(),actual.data(),128);allocations_enabled=false;
        check(expected==actual,"profiling altered exact PCM");}
    stop=true;reader.join();check(!invalid,"concurrent profile snapshot corrupt");check(allocations.load()==before,"profiling allocated on audio/helper threads");
    const auto p=profiled.profile();check(p.device_master.calls==70&&p.device_path.calls==70,"profile master/path sample counts");
    for(std::size_t t=0;t<5;++t)check(p.device[t].calls==70&&p.device[t].frames==70*128&&p.device[t].max_ns>0,"channel timing missing");
    std::uint64_t jobs{};for(const auto& worker:p.device_workers)jobs+=worker.calls;check(jobs==350,"worker jobs lost/doubled");
    const auto graphs=profiled.profile_graphs();check(graphs.front()->profile()[0].calls==70,"per-insert profiling missing");
    profiled.set_profiling(false);profiled.process(in.data(),actual.data(),128);check(profiled.profile().device_master.calls==70,"profiling disable kept collecting");
    profiled.prepare(c,fixture());check(!profiled.profile().enabled&&profiled.profile().device_master.calls==0,"prepare did not reset diagnostics");
}
void ownership() {
    TimeoutContext c;
    { ChannelWorkers workers(1);check(!workers.run(&c,TimeoutContext::job,2,1),"slow worker did not time out");
      check(!workers.run(&c,TimeoutContext::job,2,1)&&c.calls==2,"timed-out jobs retried");
      workers.quiesce();check(c.active==0,"state access before worker quiescence"); }
    check(c.calls==2&&c.active==0,"teardown ownership");
    struct FP {
        std::atomic<unsigned> calls{},bad{};
        static void job(void* context,std::uint32_t) noexcept {
            auto& fp=*static_cast<FP*>(context);
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
            if((_mm_getcsr()&0x8040U)!=0x8040U)++fp.bad;
#endif
            ++fp.calls;
        }
    } fp;
    ChannelWorkers workers(3);const ScopedNoDenormals mode;
    for(unsigned n=0;n<100;++n)check(workers.run(&fp,FP::job,128,500),"FP batch failed");
    check(fp.calls==12800&&fp.bad==0,"job ownership or worker FP mode");
    struct Slow final:IProcessor {
        TimeoutContext& c;unsigned index;
        Slow(TimeoutContext& context,unsigned i):c(context),index(i){}
        std::vector<ParameterInfo> parameters()const override{return {};}
        void prepare(ProcessConfig)override{}
        void restore(const PluginState&)override{}
        PluginState capture()const override{check(c.active==0,"capture raced worker");return {};}
        bool set_parameter(std::uint32_t,float)noexcept override{return false;}
        std::optional<float> parameter_value(std::uint32_t)const noexcept override{return {};}
        std::uint32_t latency()const noexcept override{return 0;}
        bool live_safe()const noexcept override{return true;}
        void warm()override{}
        void reset()noexcept override{}
        void process(ProcessBlock b)noexcept override{TimeoutContext::job(&c,index);std::fill(b.audio.begin(),b.audio.end(),.25f);}
    };
    TimeoutContext slow;
    const auto slow_chain=[&](unsigned index) {
        auto state=demo_graph();state.nodes.front().parameters.clear();
        return std::make_shared<PreparedGraph>(GraphSnapshot{std::make_shared<const GraphState>(state),0,false,false},ProcessConfig{48000,2,128},
            [&,index](const auto&){return std::make_unique<Slow>(slow,index);});
    };
    RenderGraph g;g.mixer.resize(2);g.inserts={slow_chain(0),slow_chain(1)};
    AudioEngine engine;RenderConfig cfg{48000,0,2,128,128,2};cfg.worker_wait_ms=1;cfg.adaptive_parallel=false;engine.prepare(cfg,g);
    std::array<float,256> output{};engine.process(nullptr,output.data(),128);
    check(engine.metrics().processing_fault&&engine.metrics().worker_timeouts==1,"engine fault not latched");
    check(std::all_of(output.begin(),output.end(),[](float x){return x==0;}),"timeout output not silent");
    check(engine.enqueue({ControlKind::stop}),"queued stop");engine.process(nullptr,output.data(),128);
    check(slow.calls==2,"engine retried incomplete plugin batch");
    engine.quiesce();check(slow.active==0,"engine state access before worker quiescence");
    engine.prepare({48000,0,2,128},{});check(!engine.metrics().processing_fault,"reprepare did not clear fault");
    // Raw recording tap remains before parallel inserts and PDC.
    const auto path=std::filesystem::temp_directory_path()/std::filesystem::path("mrs-parallel-raw-"+new_id().value+".wav");
    auto recorder=std::make_shared<Recorder>(path,48000,0);auto capture=fixture();capture.recording=recorder;
    cfg={48000,2,2,128,128,4};cfg.worker_wait_ms=500;engine.prepare(cfg,std::move(capture));check(engine.enqueue({ControlKind::play}),"raw play");
    std::array<float,256> raw{};raw[0]=.125f;raw[1]=.25f;engine.process(raw.data(),output.data(),128);
    engine.prepare({48000,0,2,128},{});check(recorder->finish().frames==128,"parallel raw capture frame count");
    const auto saved=load_wav(path);check(saved.samples.front()==.125f,"parallel raw capture acquired PDC");
    std::filesystem::remove(path);
}
}
#ifdef _MSC_VER
#define MRS_NOINLINE __declspec(noinline)
#else
#define MRS_NOINLINE __attribute__((noinline))
#endif
MRS_NOINLINE void* operator new(std::size_t bytes) {
    if(allocations_enabled.load(std::memory_order_relaxed))++allocations;
    if(auto p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();
}
MRS_NOINLINE void* operator new[](std::size_t bytes){return ::operator new(bytes);}
MRS_NOINLINE void operator delete(void* p) noexcept{std::free(p);}
MRS_NOINLINE void operator delete[](void* p) noexcept{std::free(p);}
MRS_NOINLINE void operator delete(void* p,std::size_t) noexcept{std::free(p);}
MRS_NOINLINE void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
int main(int argc,char** argv) {
    try {
        check(argc==2,"expected suite");const std::string suite=argv[1];
        if(suite=="equivalence")equivalence();else if(suite=="ownership")ownership();else if(suite=="profiling")profiling();else throw std::invalid_argument("unknown suite");
        std::cout<<"PASS parallel "<<suite<<'\n';return 0;
    }catch(const std::exception& e){allocations_enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
