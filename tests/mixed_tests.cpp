#include <mrs/ahead_renderer.hpp>
#include <mrs/processing.hpp>
#include <mrs/recording.hpp>
#include <mrs/offline_device.hpp>
#include "latency_fixture.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <bit>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
namespace probe {std::atomic<bool> enabled{};std::atomic<unsigned> allocations{};}
#ifdef _MSC_VER
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif
NOINLINE void* operator new(std::size_t n){if(probe::enabled.load())++probe::allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
NOINLINE void* operator new[](std::size_t n){return ::operator new(n);}
NOINLINE void operator delete(void* p)noexcept{std::free(p);}
NOINLINE void operator delete[](void* p)noexcept{std::free(p);}
NOINLINE void operator delete(void* p,std::size_t)noexcept{std::free(p);}
NOINLINE void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
namespace {
using namespace mrs;using namespace mrs::audio;using namespace mrs::processing;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void settle(){std::this_thread::sleep_for(std::chrono::milliseconds(4));}
struct Trace {std::atomic<unsigned> active{},collisions{},calls{};std::atomic<bool> hold{},entered{};};
class Effect final:public IProcessor {
    LatencyFixture delay_;Trace& trace_;bool safe_;
public:
    Effect(unsigned delay,Trace& trace,bool safe):delay_(delay),trace_(trace),safe_(safe){}
    std::vector<ParameterInfo> parameters()const override{return {};}
    void prepare(ProcessConfig c)override{delay_.prepare(c);}
    void restore(const PluginState&)override{}
    PluginState capture()const override{check(trace_.active==0,"state capture raced processing");return {};}
    bool set_parameter(std::uint32_t,float)noexcept override{return false;}
    std::optional<float> parameter_value(std::uint32_t)const noexcept override{return {};}
    std::uint32_t latency()const noexcept override{return delay_.latency();}
    bool live_safe()const noexcept override{return true;}
    bool anticipation_safe()const noexcept override{return safe_;}
    void warm()override{}
    void reset()noexcept override{if(trace_.active)++trace_.collisions;delay_.reset();}
    void process(ProcessBlock b)noexcept override{
        if(trace_.active.fetch_add(1))++trace_.collisions;++trace_.calls;trace_.entered=true;
        while(trace_.hold.load(std::memory_order_acquire))std::this_thread::yield();
        // Stateful delay followed by nonlinear DSP verifies merging BEFORE the
        // shared effect; summing independently processed outputs would differ.
        delay_.process(b);for(auto& s:b.audio)s=s/(1+std::abs(s));--trace_.active;
    }
};
using Traces=std::array<Trace,5>;
std::shared_ptr<PreparedGraph> chain(unsigned delay,Trace& trace,bool safe){auto state=demo_graph();state.nodes.front().parameters.clear();
    return std::make_shared<PreparedGraph>(GraphSnapshot{std::make_shared<const GraphState>(state),0,false,false},ProcessConfig{48000,2,512},
        [&,delay,safe](const auto&){return std::make_unique<Effect>(delay,trace,safe);});}
RenderGraph graph(Traces& traces,bool pdc=true){
    RenderGraph g;g.mixer.resize(4);g.buses={false,false,true,false};g.outputs={2,2,no_mixer_track,no_mixer_track};g.hardware_outputs={{},{},{},{1}};
    g.sends={{{2,.2f,true}}, {}, {},{{2,.1f,false}}};g.master_gain=.7f;
    auto asset=std::make_shared<AudioData>(sine_fixture(48000,2,32768,300));
    for(auto t:{0U,1U,3U})g.voices.push_back({asset,0,0,32768,{{0,0,.1f},{1,1,.1f}},{},t});
    g.monitor={{0,0,.3f,1},{1,1,.4f,1}};g.input_monitoring={false,true,false,false};
    g.inserts={chain(pdc?3:0,traces[0],true),chain(pdc?11:0,traces[1],false),chain(pdc?7:0,traces[2],true),chain(pdc?1:0,traces[3],true)};
    g.master_inserts=chain(pdc?17:0,traces[4],false);g.tempos={{0,100,0},{75,140,1}};return g;
}
std::shared_ptr<AudioEngine> engine(RenderGraph graph,unsigned workers=4,RealtimeState initial={}){
    auto e=std::make_shared<AudioEngine>();RenderConfig c{48000,2,2,512,512,workers};c.worker_wait_ms=500;c.adaptive_parallel=false;
    e->prepare(c,std::move(graph),initial);return e;
}
void equivalence(){
    for(auto device:{64U,128U,256U,512U})for(auto workers:{2U,4U}){
        Traces a,b;const RealtimeState initial{PlaybackState::paused,0,LoopRange{0,97}};
        auto direct=engine(graph(a),1,initial),mixed=engine(graph(b),workers,initial);
        check(direct->enqueue({ControlKind::play})&&mixed->enqueue({ControlKind::play}),"play queues");
        AheadRenderer renderer(mixed,device,2048);renderer.start();check(renderer.active(),"mixed did not start");settle();
        std::array<float,1024> in{},expected{},actual{};const auto before=probe::allocations.load();
        for(unsigned n=0;n<80;++n){const auto frames=n%2?device-17:17;
            for(std::size_t i=0;i<frames*2;++i)in[i]=.2f*static_cast<float>((i+n)%7)/7;
            probe::enabled=true;direct->process(in.data(),expected.data(),frames);renderer.process(in.data(),actual.data(),frames);probe::enabled=false;
            check(!mixed->metrics().processing_fault,"mixed fault");check(mixed->metrics().ahead_underruns==0,"paced mixed starvation");
            for(std::size_t i=0;i<frames*2;++i)check(expected[i]==actual[i],"mixed PCM differs from serial");
            check(direct->state().sample==mixed->state().sample,"mixed heard timeline differs");settle();
        }
        check(probe::allocations==before,"mixed host RT allocation");renderer.stop();
        for(const auto& trace:b)check(trace.collisions==0&&trace.active==0,"mixed instance collision or failed join");
        check(b[4].calls==80,"shared Master moved off device or processed twice");
        renderer.start();settle();renderer.process(in.data(),actual.data(),device);renderer.stop();
    }
}
void recovery(){
    Traces traces,live_traces;auto g=graph(traces,false);g.voices.clear();
    auto asset=std::make_shared<AudioData>(sine_fixture(48000,2,32768,300));g.voices={{asset,0,0,32768,{{0,0,.1f},{1,1,.1f}},{},0}};
    auto live_graph=graph(live_traces,false);live_graph.voices.clear();
    auto mixed=engine(g,2),live=engine(live_graph,1);mixed->enqueue({ControlKind::play});live->enqueue({ControlKind::play});
    AheadRenderer renderer(mixed,128,256);renderer.start();settle();
    traces[0].hold=true;
    struct Release {Trace& t;~Release(){t.hold=false;}} release{traces[0]};
    std::array<float,256> in{},out{},reference{};in.fill(.25f);
    for(unsigned n=0;n<8;++n){live->process(in.data(),reference.data(),128);renderer.process(in.data(),out.data(),128);}
    check(mixed->metrics().ahead_underruns>0,"producer starvation not exercised");
    check(out==reference,"starvation silenced or altered live DSP");check(mixed->state().sample==1024,"starvation held device timeline");
    check(traces[1].calls==8&&traces[2].calls==8&&traces[4].calls==8,"live/shared DSP lost callbacks");
    traces[0].hold=false;settle();renderer.process(in.data(),out.data(),128);settle();renderer.process(in.data(),out.data(),128);
    check(!mixed->metrics().processing_fault,"starvation latched unnecessary processing fault");
    check(mixed->state().sample==1280,"recovery skipped or doubled device time");renderer.stop();
    for(const auto& trace:traces)check(trace.collisions==0,"recovery reset concurrently owned instance");
}
void edits(){
    Traces traces;auto mixed=engine(graph(traces,false));mixed->enqueue({ControlKind::play});AheadRenderer renderer(mixed,128,1024);renderer.start();settle();
    std::array<float,256> in{},out{};in.fill(.2f);renderer.process(in.data(),out.data(),128);
    mixed->enqueue({ControlKind::seek,1000});renderer.process(in.data(),out.data(),128);check(mixed->state().sample==1128,"seek did not advance live timeline");
    settle();renderer.process(in.data(),out.data(),128);check(mixed->state().sample==1256,"seek replayed twice");
    mixed->enqueue({ControlKind::pause});renderer.process(in.data(),out.data(),128);check(mixed->state().sample==1256,"pause advanced timeline");
    check(std::any_of(out.begin(),out.end(),[](float s){return s!=0;}),"pause disabled live monitor");
    mixed->enqueue({ControlKind::monitor,0});renderer.process(in.data(),out.data(),128);check(std::all_of(out.begin(),out.end(),[](float s){return s==0;}),"disabled monitor retained stale playback");
    mixed->enqueue({ControlKind::monitor,1});mixed->enqueue({ControlKind::play});renderer.process(in.data(),out.data(),128);check(mixed->state().sample==1384,"play after pause");
    MixerUpdate m;m.count=4;m.master_gain=.5f;m.input_monitoring[1]=true;for(auto& t:m.tracks)t.gain=.6f;m.send_gains[0][0]=.2f;m.send_gains[3][0]=.1f;
    mixed->enqueue_mix(m);renderer.process(in.data(),out.data(),128);settle();renderer.process(in.data(),out.data(),128);
    mixed->enqueue({ControlKind::stop});renderer.process(in.data(),out.data(),128);check(mixed->state().sample==1256,"stop lost heard play anchor");
    check(mixed->metrics().ahead_invalidations>0,"mixed generation invalidation missing");renderer.stop();
    for(const auto& t:traces)check(t.collisions==0,"edit raced producer DSP");
}
void capture(){
    Traces traces;const auto path=std::filesystem::temp_directory_path()/("mrs-mixed-"+new_id().value+".wav");
    struct Cleanup {std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove(p,e);}} cleanup{path};
    auto recorder=std::make_shared<Recorder>(path,48000,0,std::vector<std::uint32_t>{0,1});auto g=graph(traces);g.recording=recorder;
    auto e=engine(g,2);e->enqueue({ControlKind::play});AheadRenderer renderer(e,128,1024);renderer.start();check(renderer.active(),"recording prevented independent playback anticipation");settle();
    std::array<float,256> in{},out{};in[0]=.125f;in[1]=.25f;renderer.process(in.data(),out.data(),128);renderer.stop();
    e->prepare({48000,0,2,512},{});check(recorder->finish().frames==128,"raw capture count");const auto raw=load_wav(path);
    check(raw.samples[0]==in[0]&&raw.samples[1]==in[1],"raw capture acquired effects/PDC/process delay");
    std::weak_ptr<PreparedGraph> retired=g.inserts[0];g={};auto pure=graph(traces);pure.monitor.clear();pure.inserts.clear();pure.master_inserts.reset();
    e->prepare({48000,0,2,512,128,2},pure);e->enqueue({ControlKind::play});renderer.start();settle();
    check(renderer.active()&&!e->metrics().mixed_anticipation&&retired.expired(),"mixed-to-P2 switch retained old DSP/queue ownership");renderer.process(nullptr,out.data(),128);renderer.stop();
    auto serial=engine(graph(traces),1);AheadRenderer fallback(serial,128,1024);fallback.start();check(!fallback.active(),"one-worker mixed mode violated participant budget");fallback.stop();
    auto wired_graph=graph(traces);wired_graph.monitor.resize(1);
    auto wired=std::make_shared<AudioEngine>();wired->prepare({48000,1,2,512,128,2},wired_graph);
    auto device=make_offline_device();DeviceConfig config{0,48000,128,{0},{0,1},2,1024};device->open(config,wired);wired->enqueue({ControlKind::play});device->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));check(wired->metrics().mixed_anticipation,"offline backend did not wire mixed renderer");device->stop();
    check(!wired->metrics().mixed_anticipation&&wired->metrics().processing_workers<=2,"backend stop failed to restore callback participant pool");
    device->start();std::this_thread::sleep_for(std::chrono::milliseconds(10));device->close();
    for(const auto& t:traces)check(t.active==0&&t.collisions==0,"backend stop/restart failed DSP ownership");
}
void parameters(){
    Traces traces,reference_traces;
    const auto gain=[](float value){const std::array<NativeInsert,1> fx{{{new_id(),InsertKind::gain,value}}};
        return std::make_shared<PreparedGraph>(GraphSnapshot{std::make_shared<const GraphState>(insert_graph(fx)),0,false,false},ProcessConfig{48000,2,512});};
    auto g=graph(traces,false);auto processor=gain(.75f);g.inserts[0]=processor;
    auto e=engine(g,2);e->enqueue({ControlKind::play});AheadRenderer renderer(e,128,1024);renderer.start();settle();
    std::array<float,256> in{},out{},expected{};in.fill(.2f);renderer.process(in.data(),out.data(),17);
    const auto node=processor->snapshot().graph->nodes.front().id;
    const auto info=processor->parameter_infos(node).front().id;
    check(processor->enqueue_parameter(node,{0,info,0.f}),"native parameter enqueue");renderer.process(in.data(),out.data(),128);settle();
    auto reference_graph=graph(reference_traces,false);reference_graph.inserts[0]=gain(0.f);
    auto reference=engine(reference_graph,1,{PlaybackState::paused,e->state().sample});reference->enqueue({ControlKind::play});
    reference->process(in.data(),expected.data(),128);renderer.process(in.data(),out.data(),128);
    check(out==expected,"native edit consumed stale partial playback packet");
    check(processor->enqueue_parameter(node,{1,info,.5f}),"offset automation enqueue");renderer.process(in.data(),out.data(),128);settle();renderer.process(in.data(),out.data(),128);
    check(e->metrics().processing_fault&&e->state().playback==PlaybackState::paused,"unsafe producer capability did not latch paused fault");
    renderer.stop();for(const auto& t:traces)check(t.collisions==0,"capability change ownership");
    RenderGraph large;large.mixer.resize(128);large.monitor={{0,0,1,127}};
    auto source=std::make_shared<AudioData>(sine_fixture(48000,2,16384,100));large.voices={{source,0,0,16384,{{0,0,.1f}},{},0}};
    auto bounded=std::make_shared<AudioEngine>();bounded->prepare({48000,2,2,8192,128,2},large);AheadRenderer memory(bounded,128,8192);bool rejected=false;
    try{memory.start();}catch(const std::invalid_argument&){rejected=true;}
    check(rejected&&!bounded->metrics().anticipation_active,"mixed PCM memory bound not enforced");memory.stop();
    bounded->process(in.data(),out.data(),128);check(!bounded->metrics().processing_fault,"failed setup damaged direct owner");
}
void streaming(){
    struct Temp{std::filesystem::path path=std::filesystem::temp_directory_path()/("mrs-mixed-stream-"+new_id().value+".wav");~Temp(){std::error_code e;std::filesystem::remove(path,e);}}file;
    constexpr unsigned total=1200000;std::ofstream wav(file.path,std::ios::binary);
    const auto put=[&](std::uint32_t v,unsigned bytes){for(unsigned i=0;i<bytes;++i)wav.put(static_cast<char>(v>>(8*i)));};
    wav.write("RIFF",4);put(36+total*8,4);wav.write("WAVEfmt ",8);put(16,4);put(3,2);put(2,2);put(48000,4);put(48000*8,4);put(8,2);put(32,2);wav.write("data",4);put(total*8,4);
    for(unsigned f=0;f<total;++f){const float v=static_cast<float>(f%127)/256;put(std::bit_cast<std::uint32_t>(v),4);put(std::bit_cast<std::uint32_t>(v*.5f),4);}wav.close();
    const auto disk=std::make_shared<const AudioData>(open_wav(file.path)),memory=std::make_shared<const AudioData>(load_wav(file.path));check(disk->file&&disk->samples.empty(),"mixed source not streamed");
    Traces a,b;const auto source=[](RenderGraph g,auto asset){g.voices={{asset,0,0,asset->frames(),{{0,0,.1f},{1,1,.1f}},{},0}};return g;};
    const RealtimeState initial{PlaybackState::paused,1000000,LoopRange{1000000,1000097}};
    auto direct=engine(source(graph(a,false),memory),1,initial),mixed=engine(source(graph(b,false),disk),2,initial);
    direct->enqueue({ControlKind::play});mixed->enqueue({ControlKind::play});AheadRenderer renderer(mixed,128,4096);renderer.start();settle();
    std::array<float,256> in{},expected{},actual{};in.fill(.2f);
    for(unsigned n=0;n<100;++n){direct->process(in.data(),expected.data(),128);renderer.process(in.data(),actual.data(),128);check(actual==expected,"mixed streamed loop PCM differs");settle();}
    mixed->prime_loop({});mixed->prime_streams(200000);mixed->enqueue({ControlKind::loop});mixed->enqueue({ControlKind::prepared_seek,200000});renderer.process(in.data(),actual.data(),128);settle();
    check(mixed->state().sample==200128,"mixed prepared seek held live timeline");
    direct->enqueue({ControlKind::loop});direct->enqueue({ControlKind::seek,200128});direct->process(in.data(),expected.data(),128);renderer.process(in.data(),actual.data(),128);
    check(actual==expected,"mixed seek replayed stale disk contribution");check(!mixed->metrics().disk_underruns&&!mixed->metrics().disk_errors,"mixed stream cache ownership/miss");renderer.stop();
}
void concurrent(){
    Traces traces,live_traces;auto e=engine(graph(traces,false),2);e->enqueue({ControlKind::play});AheadRenderer renderer(e,128,1024);renderer.start();settle();
    auto live_graph=graph(live_traces,false);live_graph.voices.clear();auto live=engine(live_graph,1);
    std::array<float,256> input{},actual{},expected{};input.fill(.2f);
    traces[4].hold=true;std::thread callback([&]{renderer.process(input.data(),actual.data(),128);});
    struct Join {Trace& t;std::thread& thread;~Join(){t.hold=false;if(thread.joinable())thread.join();}} join{traces[4],callback};
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!traces[4].entered.load()&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
    check(traces[4].entered,"callback did not reach shared Master");check(e->enqueue({ControlKind::pause}),"mid-callback pause enqueue");
    traces[4].hold=false;callback.join();settle();
    renderer.process(input.data(),actual.data(),128);settle();renderer.process(input.data(),actual.data(),128);
    live->process(input.data(),expected.data(),128);
    check(actual==expected,"unapplied command generation admitted stale playing PCM");
    check(e->state().playback==PlaybackState::paused&&e->state().sample==128,"mid-callback pause advanced transport");
    renderer.stop();for(const auto& t:traces)check(t.collisions==0,"mid-callback command raced DSP ownership");
}
}
int main(int argc,char** argv){try{check(argc==2,"suite required");const std::string suite=argv[1];
    if(suite=="equivalence")equivalence();else if(suite=="recovery")recovery();else if(suite=="edits")edits();else if(suite=="capture")capture();else if(suite=="parameters")parameters();else if(suite=="streaming")streaming();else if(suite=="concurrent")concurrent();else throw std::invalid_argument("unknown suite");
    std::cout<<"PASS mixed "<<suite<<'\n';return 0;
}catch(const std::exception& e){probe::enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
