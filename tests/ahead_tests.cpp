#include <mrs/ahead_renderer.hpp>
#include <mrs/device.hpp>
#include <mrs/offline_device.hpp>
#include <mrs/processing.hpp>
#include "latency_fixture.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <bit>
#include <fstream>
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
template<class F> void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}check(rejected,"unsafe configuration accepted");}
void settle(){std::this_thread::sleep_for(std::chrono::milliseconds(4));}
std::shared_ptr<PreparedGraph> delay(unsigned frames){auto state=demo_graph();state.nodes.front().parameters.clear();
    return std::make_shared<PreparedGraph>(GraphSnapshot{std::make_shared<const GraphState>(state),0,false,false},ProcessConfig{48000,2,128},[frames](const auto&){return std::make_unique<LatencyFixture>(frames);});}
std::shared_ptr<PreparedGraph> gain(){const std::array<NativeInsert,1> fx{{{new_id(),InsertKind::gain,.75f}}};
    return std::make_shared<PreparedGraph>(GraphSnapshot{std::make_shared<const GraphState>(insert_graph(fx)),0,false,false},ProcessConfig{48000,2,128});}
RenderGraph graph(bool pdc=true){RenderGraph g;g.mixer.resize(4);g.buses={false,false,true,false};g.outputs={2,2,no_mixer_track,no_mixer_track};g.hardware_outputs={{},{},{},{1}};
    g.click.playback=true;g.click.level=13;
    g.sends={{{2,.2f,true}}, {}, {},{{2,.1f,false}}};g.master_gain=.7f;
    auto asset=std::make_shared<AudioData>(sine_fixture(48000,2,16384,300));
    g.voices={{asset,0,0,16384,{{0,0,.1f},{1,1,.1f}},{},0},{asset,0,0,16384,{{0,0,.2f},{1,1,.2f}},{},1},{asset,0,0,16384,{{0,0,.1f},{1,1,.1f}},{},3}};
    if(pdc){g.inserts={delay(3),delay(11),delay(7),delay(1)};g.master_inserts=delay(17);}g.tempos={{0,100,0},{75,140,1}};return g;}
std::shared_ptr<AudioEngine> engine(RenderGraph g,RealtimeState initial={},unsigned workers=1){auto e=std::make_shared<AudioEngine>();RenderConfig c{48000,0,2,128,128,workers};c.worker_wait_ms=500;c.adaptive_parallel=false;e->prepare(c,std::move(g),initial);return e;}
void equivalence(){for(auto workers:{1U,2U,4U})for(auto process:{256U,1024U,4096U}){
    const RealtimeState initial{PlaybackState::paused,0,LoopRange{0,97}};auto direct=engine(graph(),initial),ahead=engine(graph(),initial,workers);
    check(direct->enqueue({ControlKind::play})&&ahead->enqueue({ControlKind::play}),"play");AheadRenderer cache(ahead,128,process);cache.start();check(cache.active(),"eligible playback rejected");settle();
    check(ahead->state().sample==0,"speculative playhead published before hearing");std::array<float,256> expected{},actual{};const auto allocs=probe::allocations.load();probe::enabled=true;
    for(unsigned n=0;n<80;++n){direct->process(nullptr,expected.data(),128);cache.process(nullptr,actual.data(),17);cache.process(nullptr,actual.data()+34,111);
        for(unsigned i=0;i<256;++i)check(expected[i]==actual[i],"cached PCM differs from serial PDC/loop reference");check(direct->state().sample==ahead->state().sample,"audible playhead differs");settle();}
    probe::enabled=false;check(probe::allocations==allocs,"allocation on producer/device/helper");check(ahead->metrics().ahead_underruns==0,"unexpected paced underrun");check(ahead->metrics().callbacks==160,"producer counted as device callbacks");
    rejects([&]{ahead->prepare({48000,0,2,128},{});});const auto heard=ahead->state().sample;cache.stop();check(ahead->state().sample==heard,"stop leaked speculative position");
    cache.start();settle();direct->enqueue({ControlKind::seek,heard});direct->process(nullptr,expected.data(),128);cache.process(nullptr,actual.data(),128);
    check(std::equal(expected.begin(),expected.end(),actual.begin()),"restart did not rebase PDC/history to heard head");cache.stop();
}}
// Generation edits are compared with a fresh serial owner at the last audible
// sample. Resetting effect history on invalidation is the explicit P2 policy.
void edits(){auto g=graph(false);g.inserts={gain(),gain(),gain(),gain()};auto e=engine(g);e->enqueue({ControlKind::play});AheadRenderer cache(e,128,1024);cache.start();settle();std::array<float,256> out{};cache.process(nullptr,out.data(),128);settle();
    e->enqueue({ControlKind::seek,1024});e->enqueue({ControlKind::pause});e->enqueue({ControlKind::play});
    // First callback rejects old generation and wakes a full producer queue.
    cache.process(nullptr,out.data(),128);settle();cache.process(nullptr,out.data(),128);
    check(e->state().sample==1152&&e->state().play_start==1024,"seek/pause/play journal lost or stale audio emitted");
    auto reference=engine(graph(false),{PlaybackState::paused,1024});reference->enqueue({ControlKind::play});
    MixerUpdate mix;mix.count=4;mix.master_gain=.2f;for(auto& t:mix.tracks)t={.5f,0,false,false};mix.send_gains[0][0]=.2f;mix.send_gains[3][0]=.1f;
    // Repeated edits must retire acknowledged commands instead of exhausting
    // the bounded replay journal after a fixed lifetime number of UI edits.
    for(unsigned n=0;n<180;++n){e->enqueue({ControlKind::pause});e->enqueue({ControlKind::play});check(e->enqueue_mix(mix),"mixer queue full");cache.process(nullptr,out.data(),128);settle();cache.process(nullptr,out.data(),128);settle();check(!e->metrics().processing_fault,"acknowledged command journal never retired");}
    const auto node=g.inserts[0]->snapshot().graph->nodes.front().id;
    const auto info=g.inserts[0]->parameter_infos(node).front();check(g.inserts[0]->enqueue_parameter(node,{0,info.id,info.minimum}),"parameter enqueue");cache.process(nullptr,out.data(),128);settle();cache.process(nullptr,out.data(),128);
    check(e->metrics().ahead_invalidations>0&&!e->metrics().processing_fault,"parameter invalidation");
    const auto anchor=e->state().play_start;e->enqueue({ControlKind::stop});cache.process(nullptr,out.data(),128);settle();cache.process(nullptr,out.data(),128);check(e->state().playback==PlaybackState::stopped&&e->state().sample==anchor,"stop anchor used speculative cursor");cache.stop();
    // PCM and ramp comparison for a mixer edit at a partial audible block.
    auto a=engine(graph(false)),b=engine(graph(false));a->enqueue({ControlKind::play});b->enqueue({ControlKind::play});AheadRenderer ramps(b,128,1024);ramps.start();settle();ramps.process(nullptr,out.data(),17);
    a->enqueue({ControlKind::seek,17});a->enqueue_mix(mix);b->enqueue_mix(mix);ramps.process(nullptr,out.data(),128);settle();std::array<float,256> expected{};a->process(nullptr,expected.data(),128);ramps.process(nullptr,out.data(),128);
    check(std::equal(expected.begin(),expected.end(),out.begin()),"partial-head mixer ramp differs from serial reference");ramps.stop();
    // EQ smoothing belongs to speculative DSP state too: after an unheard edit
    // and stop/restart it must reset to retained targets, not future coefficients.
    NativeInsert eq;eq.id=new_id();eq.kind=InsertKind::channel_eq;
    const auto eq_chain=[](const NativeInsert& fx){const std::array<NativeInsert,1> inserts{fx};return std::make_shared<PreparedGraph>(GraphSnapshot{std::make_shared<const GraphState>(insert_graph(inserts)),0,false,false},ProcessConfig{48000,2,128});};
    auto eq_graph=graph(false);auto processor=eq_chain(eq);eq_graph.master_inserts=processor;auto eq_engine=engine(eq_graph);eq_engine->enqueue({ControlKind::play});AheadRenderer eq_cache(eq_engine,128,1024);eq_cache.start();settle();eq_cache.process(nullptr,out.data(),128);
    processor->enqueue_parameter(eq.id,{0,6,6.f});eq_cache.process(nullptr,out.data(),128);settle();eq_cache.stop();const auto position=eq_engine->state().sample;
    eq.bands[1].gain=6.f;auto fresh_graph=graph(false);fresh_graph.master_inserts=eq_chain(eq);auto fresh=engine(fresh_graph,{PlaybackState::paused,position});fresh->enqueue({ControlKind::play});fresh->process(nullptr,expected.data(),128);
    eq_cache.start();settle();eq_cache.process(nullptr,out.data(),128);check(std::equal(expected.begin(),expected.end(),out.begin()),"EQ retained speculative smoothing after restart");eq_cache.stop();
}
struct Gate {std::atomic<bool> hold{},entered{};std::atomic<unsigned> calls{},active{},maximum{};};
class Gated final:public IProcessor {Gate& gate_;bool eligible_;
public:Gated(Gate& g,bool safe):gate_(g),eligible_(safe){}
    std::vector<ParameterInfo> parameters()const override{return {};}
    void prepare(ProcessConfig)override{}void restore(const PluginState&)override{}PluginState capture()const override{return {};}
    bool set_parameter(std::uint32_t,float)noexcept override{return false;}std::optional<float> parameter_value(std::uint32_t)const noexcept override{return {};}
    std::uint32_t latency()const noexcept override{return 0;}bool live_safe()const noexcept override{return true;}bool anticipation_safe()const noexcept override{return eligible_;}
    void warm()override{}void reset()noexcept override{}
    void process(ProcessBlock)noexcept override{const auto active=++gate_.active;gate_.maximum=std::max(gate_.maximum.load(),active);++gate_.calls;gate_.entered=true;while(gate_.hold)std::this_thread::sleep_for(std::chrono::milliseconds(1));--gate_.active;}
};
std::shared_ptr<PreparedGraph> gated(Gate& gate,bool safe){auto s=demo_graph();s.nodes.front().parameters.clear();return std::make_shared<PreparedGraph>(GraphSnapshot{std::make_shared<const GraphState>(s),0,false,false},ProcessConfig{48000,2,128},[&gate,safe](const auto&){return std::make_unique<Gated>(gate,safe);});}
void recovery(){Gate gate;auto g=graph(false);g.master_inserts=gated(gate,true);auto e=engine(g);e->enqueue({ControlKind::play});AheadRenderer cache(e,128,256);cache.start();settle();gate.hold=true;std::array<float,256> out{};
    for(unsigned n=0;n<5;++n)cache.process(nullptr,out.data(),128);const auto heard=e->state().sample;cache.process(nullptr,out.data(),128);
    check(std::all_of(out.begin(),out.end(),[](float x){return x==0;}),"underrun did not silence");check(e->state().sample==heard&&e->metrics().ahead_underruns>0,"underrun advanced unheard position");gate.hold=false;settle();cache.process(nullptr,out.data(),128);
    if(e->state().sample!=heard+128||gate.maximum!=1)std::cerr<<"recovery heard="<<heard<<" after="<<e->state().sample<<" maximum="<<gate.maximum.load()<<'\n';
    check(e->state().sample==heard+128&&gate.maximum==1,"recovery skipped/duplicated processing ownership");cache.stop();check(gate.active==0,"producer alive after stop");
    // Actual external MIDI/offset automation must not become speculative.
    auto p=gain();auto scheduled=graph(false);scheduled.master_inserts=p;auto x=engine(scheduled);x->enqueue({ControlKind::play});AheadRenderer unsafe(x,128,256);unsafe.start();
    const auto id=p->snapshot().graph->nodes.front().id;const auto info=p->parameter_infos(id).front();p->enqueue_parameter(id,{1,info.id,info.minimum});unsafe.process(nullptr,out.data(),128);settle();unsafe.process(nullptr,out.data(),128);
    check(x->metrics().processing_fault,"unsafe schedule silently continued anticipative DSP");unsafe.stop();
}
void eligibility(){auto e=engine(graph(false));AheadRenderer off(e,128,0);off.start();check(!off.active(),"off enabled");off.stop();
    rejects([&]{AheadRenderer invalid(e,128,64);});rejects([&]{AheadRenderer invalid(e,128,16384);});rejects([&]{AheadRenderer invalid(e,1,8192);});
    auto g=graph(false);g.monitor={{0,0,1,0}};g.input_monitoring={true,false,false,false};auto live=std::make_shared<AudioEngine>();live->prepare({48000,1,2,128},g);AheadRenderer mixed(live,128,1024);mixed.start();check(!mixed.active(),"live closure anticipated");std::array<float,128> in{};std::array<float,256> out{};in.fill(.25f);mixed.process(in.data(),out.data(),128);check(out[0]!=0,"live fallback disabled input");mixed.stop();
    Gate gate;g=graph(false);g.master_inserts=gated(gate,false);auto unsupported=engine(g);AheadRenderer fallback(unsupported,128,1024);fallback.start();check(!fallback.active(),"unsupported processor anticipated");fallback.process(nullptr,out.data(),128);check(gate.calls>0,"unsupported fallback lost DSP");fallback.stop();
    auto device=make_offline_device();DeviceConfig c{0,48000,128,{}, {0,1},1,1024};auto clock=engine(graph(false));device->open(c,clock);device->start();settle();check(clock->metrics().anticipation_active,"backend did not wire Process Buffer");device->stop();check(!clock->metrics().anticipation_active,"backend stop left producer active");device->close();
}
void streaming(){struct Temp{std::filesystem::path path=std::filesystem::temp_directory_path()/("mrs-ahead-"+new_id().value+".wav");~Temp(){std::error_code ec;std::filesystem::remove(path,ec);}} file;
    constexpr unsigned total=1200000;std::ofstream wav(file.path,std::ios::binary);const auto put=[&](std::uint32_t value,unsigned bytes){for(unsigned i=0;i<bytes;++i)wav.put(static_cast<char>(value>>(8*i)));};
    wav.write("RIFF",4);put(36+total*8,4);wav.write("WAVEfmt ",8);put(16,4);put(3,2);put(2,2);put(48000,4);put(48000*8,4);put(8,2);put(32,2);wav.write("data",4);put(total*8,4);
    for(unsigned f=0;f<total;++f){const auto v=static_cast<float>(f%127)/256.f;put(std::bit_cast<std::uint32_t>(v),4);put(std::bit_cast<std::uint32_t>(v*.5f),4);}wav.close();
    auto disk=std::make_shared<const AudioData>(open_wav(file.path));check(disk->file&&disk->samples.empty(),"long WAV preloaded instead of streamed");auto memory=std::make_shared<const AudioData>(load_wav(file.path));
    const auto source=[](auto asset){RenderGraph g;g.voices={{asset,0,0,asset->frames(),{{0,0,1},{1,1,1}}}};return g;};
    const RealtimeState initial{PlaybackState::paused,1000000,LoopRange{1000000,1000097}};auto a=engine(source(memory),initial),b=engine(source(disk),initial);a->enqueue({ControlKind::play});b->enqueue({ControlKind::play});AheadRenderer cache(b,128,4096);cache.start();settle();std::array<float,256> expected{},actual{};
    for(unsigned n=0;n<160;++n){a->process(nullptr,expected.data(),128);cache.process(nullptr,actual.data(),128);check(std::equal(expected.begin(),expected.end(),actual.begin()),"streamed short-loop PCM differs");settle();}
    b->prime_loop({});b->prime_streams(200000);b->enqueue({ControlKind::loop});b->enqueue({ControlKind::prepared_seek,200000});cache.process(nullptr,actual.data(),128);settle();cache.process(nullptr,actual.data(),128);
    check(b->state().sample==200128,"prepared stream seek lost across generation");check(actual[0]==static_cast<float>(200000%127)/256.f,"stale disk packet after seek");check(!b->metrics().disk_underruns&&!b->metrics().disk_errors,"process buffer overran streaming cache");cache.stop();
}
}
int main(int argc,char** argv){try{check(argc==2,"suite required");const std::string suite=argv[1];if(suite=="equivalence")equivalence();else if(suite=="edits")edits();else if(suite=="recovery")recovery();else if(suite=="eligibility")eligibility();else if(suite=="streaming")streaming();else throw std::invalid_argument("unknown suite");std::cout<<"PASS ahead "<<suite<<'\n';return 0;}catch(const std::exception& e){probe::enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
