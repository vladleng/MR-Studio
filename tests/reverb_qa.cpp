#include <mrs/reverb.hpp>
#include <mrs/reverb_file.hpp>
#include <mrs/recording.hpp>
#include <mrs/audio.hpp>
#include <mrs/vst3.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>
#include <string_view>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <psapi.h>
#endif
namespace {std::atomic<bool> probe{};std::atomic<std::size_t> allocations{},deallocations{};}
void* operator new(std::size_t n){if(probe.load())++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc{};}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {if(probe.load()&&p)++deallocations;std::free(p);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
namespace {
using namespace mrs;using namespace mrs::processing;using namespace mrs::audio;
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
std::string utf8(const std::filesystem::path& path){const auto value=path.u8string();return {value.begin(),value.end()};}
void run(ReverbProcessor& dsp,std::span<float> data,std::uint32_t channels){
    constexpr std::array sizes{1u,63u,128u,257u,511u};MidiBuffer midi;std::size_t offset{},n{};
    while(offset<data.size()){const auto frames=static_cast<std::uint32_t>(std::min<std::size_t>(sizes[n++%sizes.size()],(data.size()-offset)/channels));
        dsp.process({data.subspan(offset,frames*channels),frames,channels,{},{},midi});offset+=frames*channels;}
}
ReverbImpulse synthetic(std::uint32_t rate,std::size_t frames,ReverbRouting routing){
    ReverbImpulse ir;ir.sample_rate=rate;ir.routing=routing;const auto paths=routing==ReverbRouting::mono?1:routing==ReverbRouting::stereo?2:4;
    for(int ch=0;ch<paths;++ch){ir.kernels[ch].resize(frames);for(std::size_t f=0;f<frames;++f)
        ir.kernels[ch][f]=static_cast<float>(.0001*std::sin(f*.173+ch)*std::exp(-static_cast<double>(f)/(rate*4.)));
        ir.kernels[ch][0]=.1f*(ch+1);ir.kernels[ch].back()=.002f*(ch+1);}
    return ir;
}
void setup(ReverbProcessor& dsp,const ReverbImpulse& ir,std::uint32_t rate){dsp.prepare({rate,2,1024});check(dsp.set_parameter(2,1)&&dsp.stage_impulse(ir),"setup");dsp.warm();}
void reference(){
    for(auto rate:{44100u,48000u,96000u})for(auto routing:{ReverbRouting::mono,ReverbRouting::stereo,ReverbRouting::true_stereo}){
        const auto ir=synthetic(rate,4099,routing);constexpr std::size_t input_frames=768,total=input_frames+4098;
        std::vector<float> input(total*2);std::vector<double> oracle(input.size());
        const auto duration=static_cast<double>(input_frames)/rate,k=std::log(12000./40.)/duration;
        for(std::size_t f=0;f<input_frames;++f){const auto t=static_cast<double>(f)/rate,phase=2*std::numbers::pi*40*(std::exp(k*t)-1)/k;
            input[f*2]=static_cast<float>(.7*std::sin(phase));input[f*2+1]=static_cast<float>(.3*std::cos(phase*.73));}
        for(std::size_t f=0;f<input_frames;++f)for(std::size_t tap=0;tap<ir.kernels[0].size();++tap){
            oracle[(f+tap)*2]+=static_cast<double>(input[f*2])*ir.kernels[0][tap];
            oracle[(f+tap)*2+1]+=static_cast<double>(input[f*2+1])*ir.kernels[routing==ReverbRouting::mono?0:routing==ReverbRouting::stereo?1:3][tap];
            if(routing==ReverbRouting::true_stereo){oracle[(f+tap)*2]+=static_cast<double>(input[f*2+1])*ir.kernels[2][tap];oracle[(f+tap)*2+1]+=static_cast<double>(input[f*2])*ir.kernels[1][tap];}}
        ReverbProcessor dsp;setup(dsp,ir,rate);allocations=deallocations=0;probe=true;run(dsp,input,2);probe=false;
        check(!allocations&&!deallocations,"sweep callback allocated/freed");double error{},fold_error{};
        for(std::size_t f=0;f<total;++f){for(std::size_t ch=0;ch<2;++ch)error=std::max(error,std::abs(input[f*2+ch]-oracle[f*2+ch]));
            fold_error=std::max(fold_error,std::abs(.5*(input[f*2]+input[f*2+1])-.5*(oracle[f*2]+oracle[f*2+1])));}
        check(error<2e-5&&fold_error<2e-5,"swept sine/matrix/mono fold differs from double direct oracle");
        std::cout<<"sweep rate="<<rate<<" routing="<<static_cast<int>(routing)<<" error="<<error<<" fold_error="<<fold_error<<" alloc/free=0/0\n";
    }
    // Native DSP is linear and does not silently hard-clip floating-point audio.
    auto ir=synthetic(48000,1,ReverbRouting::true_stereo);for(auto& k:ir.kernels)k[0]=2;
    ReverbProcessor dsp;setup(dsp,ir,48000);std::array<float,4> data{1.25f,1.5f,-1.25f,-1.5f};run(dsp,data,2);
    for(std::size_t i=0;i<data.size();++i)check(std::abs(data[i]-(i<2?5.5f:-5.5f))<1e-6f,"DSP clipped finite float headroom");
    dsp.reset();data={NAN,INFINITY,-INFINITY,std::numeric_limits<float>::denorm_min()};run(dsp,data,2);for(auto x:data)check(x==0,"invalid/subnormal input leaked output");
    // Reprepare/resample the same immutable external IR at each project rate.
    const auto source=synthetic(48000,17001,ReverbRouting::true_stereo);
    for(auto rate:{44100u,96000u,48000u}){setup(dsp,source,rate);check(dsp.tail_frames()==(17001ull*rate+47999)/48000-1&&dsp.latency()==0,"rate transition tail/PDC");
        data.fill(0);run(dsp,data,2);for(auto x:data)check(x==0,"rate transition retained history");}
    std::cout<<"float headroom/sanitation and rate reprepare PASS\n";
}
struct Delay final:IProcessor {
    std::array<std::array<float,2>,7> ring{};std::size_t at{};
    std::vector<ParameterInfo> parameters() const override{return {};}
    void prepare(ProcessConfig) override{reset();}void restore(const PluginState&) override{}
    PluginState capture() const override{return {};}
    bool set_parameter(std::uint32_t,float) noexcept override{return false;}
    std::optional<float> parameter_value(std::uint32_t) const noexcept override{return {};}
    std::uint32_t latency() const noexcept override{return 7;}bool live_safe() const noexcept override{return true;}
    void warm() override{reset();}void reset() noexcept override{ring={};at=0;}
    void process(ProcessBlock b) noexcept override{for(std::uint32_t f=0;f<b.frames;++f){for(std::size_t ch=0;ch<2;++ch)std::swap(b.audio[f*2+ch],ring[at][ch]);at=(at+1)%ring.size();}}
};
void pdc(){
    GraphState state;state.id={"pdc-reverb"};state.nodes={{{"reverb"},"mr_reverb",ProcessorFormat::native,{},{{2,1}},false},{{"delay"},"qa.delay",ProcessorFormat::native,{},{},false}};
    state.edges={{std::nullopt,{"reverb"},1},{std::nullopt,{"delay"},1}};state.outputs={{{"reverb"}},{{"delay"}}};
    PreparedGraph graph({std::make_shared<const GraphState>(state),0,false,false},{48000,2,1024},[](const NodeState& n)->std::unique_ptr<IProcessor>{if(n.processor_id=="qa.delay")return std::make_unique<Delay>();return native_factory(n);});
    auto ir=synthetic(48000,20001,ReverbRouting::true_stereo);for(auto& k:ir.kernels)std::fill(k.begin(),k.end(),0.f);ir.kernels[0][0]=.5f;ir.kernels[1][0]=.25f;ir.kernels[2].back()=-.125f;ir.kernels[3].back()=.375f;
    check(graph.stage_reverb({"reverb"},ir),"PDC stage");graph.warm_reverb({"reverb"});check(graph.latency().output==7&&graph.reverb_tail_frames({"reverb"})==20000,"PDC/tail accounting");
    std::vector<float> data((20001+7+512)*2);data[0]=1;data[1]=.5f;
    allocations=deallocations=0;probe=true;for(std::size_t f=0;f<data.size()/2;f+=251)graph.process(data.data()+f*2,static_cast<std::uint32_t>(std::min<std::size_t>(251,data.size()/2-f)));probe=false;
    check(!allocations&&!deallocations,"PDC callback allocation/destruction");
    for(std::size_t f=0;f<data.size()/2;++f){const float l=f==7?1.5f:f==20007?-.0625f:0,r=f==7?.75f:f==20007?.1875f:0;
        check(std::abs(data[f*2]-l)<2e-6f&&std::abs(data[f*2+1]-r)<2e-6f,"PDC shortened/delayed tail twice");}
    std::cout<<"7-frame parallel PDC + full Reverb tail PASS\n";
}
struct Chain {std::shared_ptr<PreparedGraph> graph;ReverbProcessor* dsp{};};
Chain chain(const ReverbImpulse& ir){
    GraphState state;state.id={"matrix"};state.nodes={{{"reverb"},"mr_reverb",ProcessorFormat::native,{},{{2,1}},false}};state.edges={{std::nullopt,{"reverb"},1}};state.outputs={{{"reverb"}}};
    Chain out;out.graph=std::make_shared<PreparedGraph>(GraphSnapshot{std::make_shared<const GraphState>(state),0,false,false},ProcessConfig{ir.sample_rate,2,1024},[&](const NodeState&)->std::unique_ptr<IProcessor>{auto dsp=std::make_unique<ReverbProcessor>();out.dsp=dsp.get();return dsp;});
    check(out.graph->stage_reverb({"reverb"},ir),"matrix stage");out.graph->warm_reverb({"reverb"});return out;
}
std::array<std::uint64_t,2> memory(){
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};counters.cb=sizeof(counters);check(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters))!=0,"memory counters");return {counters.WorkingSetSize,counters.PrivateUsage};
#else
    return {};
#endif
}
void matrix(const std::filesystem::path& fixture,const std::filesystem::path& csv){
#ifdef MRS_HAS_VST3
    const auto temp=std::filesystem::temp_directory_path()/("mrs-reverb-qa-"+new_id().value);std::filesystem::create_directory(temp);
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}} cleanup{temp};
    const auto module=temp/"instrument.vst3";std::filesystem::copy_file(fixture,module);const auto info=probe_vst3(utf8(module)).front();
    NativeInsert vst;vst.id={"synth"};vst.kind=InsertKind::vst3;vst.plugin_path=info.path;vst.class_id=info.class_id;
    std::ofstream file(csv);check(static_cast<bool>(file),"CSV output");
    file<<"rate,ir_seconds,instances,workers,frames,mean_us,p95_us,p99_us,max_us,deadline_us,late,worker_timeouts,processing_fault,prepared_bytes,working_set,private_bytes,pdc_frames,vsti,allocations,deallocations\n";
    for(auto rate:{44100u,48000u,96000u})for(auto seconds:{8u,20u}){
        const auto ir=synthetic(rate,rate*seconds,ReverbRouting::true_stereo);std::vector<Chain> chains;
        std::array<float,2048> buffer{};
        for(auto count:{2u,8u,16u}){
            // Only the measured instance count is retained for honest RAM data.
            // Each newly added kernel fills its complete FDL, not a startup slice.
            while(chains.size()<count){auto c=chain(ir);for(std::size_t f=0;f<ir.kernels[0].size()+2048;f+=1024){buffer.fill(.001f);c.graph->process(buffer.data(),1024);}chains.push_back(std::move(c));}
            for(auto workers:{1u,8u}){
            auto synth=std::make_shared<PreparedGraph>(GraphSnapshot{std::make_shared<const GraphState>(insert_graph(std::array{vst})),0,false,false},ProcessConfig{rate,2,1024,256,true},hosted_factory);
            RenderGraph graph;graph.mixer.resize(count+1);graph.inserts.resize(count+1);graph.live_midi.assign(count+1,false);graph.midi_notes.resize(count+1);graph.midi_notes[count]={{0,static_cast<Sample>(rate)*120,60,1,0}};
            graph.live_midi[count]=true;graph.inserts[count]=synth;
            auto asset=std::make_shared<AudioData>();asset->sample_rate=rate;asset->channels=2;asset->samples.assign(static_cast<std::size_t>(rate)*20*2,.001f);
            std::size_t prepared{};for(std::size_t t=0;t<count;++t){graph.inserts[t]=chains[t].graph;prepared+=chains[t].dsp->prepared_bytes();graph.voices.push_back({asset,0,0,asset->frames(),{{0,0,1},{1,1,1}},{},t});}
            AudioEngine engine;RenderConfig config{rate,0,2,1024,256,workers};config.adaptive_parallel=false;engine.prepare(config,std::move(graph));check(engine.enqueue({ControlKind::play}),"matrix Play");
            for(int n=0;n<64;++n)engine.process(nullptr,buffer.data(),256);
            const auto ram=memory();
            for(auto frames:{128u,256u,512u}){
                std::array<double,256> times{};std::size_t late{};allocations=deallocations=0;
                for(std::size_t n=0;n<times.size();++n){const auto start=std::chrono::steady_clock::now();probe=true;engine.process(nullptr,buffer.data(),frames);probe=false;
                    times[n]=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();if(times[n]>1e6*frames/rate)++late;}
                const auto metrics=engine.metrics();check(!allocations&&!deallocations,"matrix callback allocation/destruction");
                double mean{};for(auto t:times)mean+=t;mean/=times.size();std::sort(times.begin(),times.end());
                file<<rate<<','<<seconds<<','<<count<<','<<workers<<','<<frames<<','<<mean<<','<<times[243]<<','<<times[253]<<','<<times.back()<<','<<1e6*frames/rate<<','<<late<<','<<metrics.worker_timeouts<<','<<metrics.processing_fault<<','<<prepared<<','<<ram[0]<<','<<ram[1]<<','<<engine.compensation().output<<",synthetic_vst3,0,0\n";file.flush();
                std::cout<<"matrix "<<rate<<"Hz "<<seconds<<"s n="<<count<<" workers="<<workers<<" block="<<frames<<" mean="<<mean<<" late="<<late<<"/256 fault="<<metrics.processing_fault<<std::endl;
            }
            engine.quiesce();
            }
        }
    }
    std::cout<<"Offline matrix complete; actual ASIO/commercial VSTi/listening NOT RUN\n";
#else
    (void)fixture;(void)csv;throw std::runtime_error("VST3 host unavailable");
#endif
}
void ab_files(const std::filesystem::path& quad,const std::filesystem::path& folder){
    const auto ir=load_reverb_ir(quad,ReverbQuadMap::ll_lr_rl_rr);check(ir.sample_rate==48000,"A/B fixture needs original 48k IR");
    check(!std::filesystem::exists(folder),"A/B destination exists; preserve it");std::filesystem::create_directories(folder);
    const auto frames=static_cast<std::size_t>(48000*3+ir.kernels[0].size()-1);std::vector<float> dry(frames);dry[0]=.1f;
    for(std::size_t f=24000;f<72000;++f){const double t=static_cast<double>(f-24000)/48000,k=std::log(12000./40.);dry[f]=static_cast<float>(.08*std::sin(2*std::numbers::pi*40*(std::exp(k*t)-1)/k)*std::pow(std::sin(std::numbers::pi*t),2));}
    for(std::size_t f=96000;f<144000;++f){const double t=static_cast<double>(f-96000)/48000;dry[f]=static_cast<float>(.025*(std::sin(2*std::numbers::pi*220*t)+std::sin(2*std::numbers::pi*660*t)+std::sin(2*std::numbers::pi*1730*t))*std::pow(std::sin(std::numbers::pi*t),2));}
    auto write=[&](const char* name,const std::vector<float>& data,std::uint32_t channels){std::vector<std::uint32_t> selectors;for(std::uint32_t ch=0;ch<channels;++ch)selectors.push_back(ch);Recorder writer(folder/name,48000,0,std::move(selectors));for(std::size_t f=0;f<data.size()/channels;f+=512)writer.capture(data.data()+f*channels,channels,static_cast<std::uint32_t>(std::min<std::size_t>(512,data.size()/channels-f)),static_cast<Sample>(f));const auto saved=writer.finish();check(saved.status.fault==RecordFault::none,"A/B write");};
    write("dry-mono-probe.wav",dry,1);
    for(std::size_t excitation=0;excitation<2;++excitation){ReverbProcessor dsp;setup(dsp,ir,48000);std::vector<float> wet(frames*2);for(std::size_t f=0;f<frames;++f)wet[f*2+excitation]=dry[f];run(dsp,wet,2);write(excitation==0?"MR-wet-left-excitation.wav":"MR-wet-right-excitation.wav",wet,2);}
    std::cout<<"A/B dry and MR wet files written; no private IR copied, no Altiverb comparison performed\n";
}
}
int main(int argc,char** argv){try{
    if(argc==4&&std::string_view(argv[1])=="--matrix")matrix(argv[2],argv[3]);
    else if(argc==4&&std::string_view(argv[1])=="--ab-files")ab_files(argv[2],argv[3]);
    else {reference();pdc();std::cout<<"MR Reverb QA reference PASS\n";}
    return 0;
}catch(const std::exception& e){probe=false;std::cerr<<e.what()<<'\n';return 1;}}
