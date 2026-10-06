#include <mrs/ahead_renderer.hpp>
#include <mrs/processing.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <thread>
#include <fstream>
#include <bit>
#include <mrs/recording.hpp>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace {
using namespace mrs;using namespace mrs::audio;using namespace mrs::processing;
std::shared_ptr<const AudioData> disk_asset,memory_asset;
class Pacer {
#ifdef _WIN32
    HANDLE timer_=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
#endif
public:
    Pacer(){
#ifdef _WIN32
        if(!timer_)throw std::runtime_error("high-resolution benchmark timer unavailable");
#endif
    }
    ~Pacer(){
#ifdef _WIN32
        CloseHandle(timer_);
#endif
    }
    void until(std::chrono::steady_clock::time_point next,bool coarse){
        if(coarse){std::this_thread::sleep_until(next);return;}
#ifdef _WIN32
        const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(next-std::chrono::steady_clock::now()).count();if(ns<=0)return;
        LARGE_INTEGER due;due.QuadPart=-std::max<std::int64_t>(1,(ns+99)/100);if(!SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE))throw std::runtime_error("benchmark timer set failed");WaitForSingleObject(timer_,INFINITE);
#else
        std::this_thread::sleep_until(next);
#endif
    }
};
class Work final:public IProcessor {unsigned iterations_;double state_{.01};
public:explicit Work(unsigned n):iterations_(n){}
    std::vector<ParameterInfo> parameters()const override{return {};}
    void prepare(ProcessConfig)override{}void restore(const PluginState&)override{}PluginState capture()const override{return {};}
    bool set_parameter(std::uint32_t,float)noexcept override{return false;}std::optional<float> parameter_value(std::uint32_t)const noexcept override{return {};}
    std::uint32_t latency()const noexcept override{return 0;}bool live_safe()const noexcept override{return true;}bool anticipation_safe()const noexcept override{return true;}
    void warm()override{}void reset()noexcept override{state_=.01;}
    void process(ProcessBlock b)noexcept override{for(auto& sample:b.audio){for(unsigned n=0;n<iterations_;++n)state_=state_*.99991+sample*.00009;sample=static_cast<float>(sample*.99+state_*.01);}}
};
std::shared_ptr<PreparedGraph> chain(unsigned work,unsigned block){auto s=demo_graph();s.nodes.front().parameters.clear();return std::make_shared<PreparedGraph>(GraphSnapshot{std::make_shared<const GraphState>(s),0,false,false},ProcessConfig{48000,2,block},[work](const auto&){return std::make_unique<Work>(work);});}
RenderGraph fixture(const std::string& topology,unsigned frames,bool reference){const auto tracks=topology=="playback2"?2U:8U;RenderGraph g;g.mixer.resize(tracks);g.inserts.resize(tracks);g.input_monitoring.assign(tracks,true);
    std::shared_ptr<const AudioData> asset=(topology=="disk8"||topology=="mixed_disk8")?(reference?memory_asset:disk_asset):memory_asset;
    for(unsigned t=0;t<tracks;++t){g.inserts[t]=chain(topology=="master8"?0:128,frames);g.voices.push_back({asset,0,0,asset->frames(),{{0,0,.04f},{1,1,.04f}},{},t});}
    if(topology=="master8"||topology=="mixed_master8")g.master_inserts=chain(1024,frames);
    if(topology.starts_with("mixed")){g.monitor={{0,0,.1f,0},{0,1,.1f,0}};g.mixer.emplace_back();g.buses.assign(tracks,false);g.buses.push_back(true);g.inserts.push_back(chain(32,frames));g.input_monitoring.push_back(false);g.sends.resize(tracks+1);g.sends[0]={{tracks,.2f,false}};}
    return g;
}
std::shared_ptr<AudioEngine> engine(const std::string& topology,unsigned frames,unsigned workers,bool reference=false){auto e=std::make_shared<AudioEngine>();RenderConfig c{48000,1,2,frames,frames,workers};c.worker_wait_ms=500;e->prepare(c,fixture(topology,frames,reference));e->enqueue({ControlKind::play});return e;}
}
int main(int argc,char** argv){try{using Clock=std::chrono::steady_clock;constexpr unsigned warm=40;unsigned measured=120,seconds{},selected_frames{},selected_workers=4,selected_process=UINT32_MAX;bool coarse=false,profiling=false,record=false,all_workers=false;std::string selected_topology;
    for(int i=1;i<argc;++i){const std::string arg=argv[i];const auto value=[&]{if(++i>=argc)throw std::invalid_argument("missing benchmark option value");return std::string(argv[i]);};
        if(arg=="--coarse-clock")coarse=true;else if(arg=="--profile")profiling=true;else if(arg=="--record")record=true;else if(arg=="--all-workers")all_workers=true;
        else if(arg=="--seconds")seconds=static_cast<unsigned>(std::stoul(value()));else if(arg=="--topology")selected_topology=value();else if(arg=="--frames")selected_frames=static_cast<unsigned>(std::stoul(value()));else if(arg=="--workers")selected_workers=static_cast<unsigned>(std::stoul(value()));else if(arg=="--process")selected_process=static_cast<unsigned>(std::stoul(value()));else throw std::invalid_argument("unknown benchmark option");}
    if(seconds>300||selected_workers<1||selected_workers>8||(selected_frames&&selected_frames!=64&&selected_frames!=128&&selected_frames!=256&&selected_frames!=512)|| (selected_process!=UINT32_MAX&&selected_process!=0&&selected_process!=256&&selected_process!=1024&&selected_process!=4096))throw std::invalid_argument("benchmark option outside supported range");
    if((seconds||record)&&(!selected_frames||selected_topology.empty()||all_workers||selected_process==UINT32_MAX))throw std::invalid_argument("sustained/record needs one topology, frames, workers and process");
    Pacer pacer;
    struct Temp {std::filesystem::path path=std::filesystem::temp_directory_path()/("mrs-ahead-bench-"+new_id().value+".wav");~Temp(){disk_asset.reset();memory_asset.reset();std::error_code ec;std::filesystem::remove(path,ec);}} file;
    const unsigned source_frames=std::max(1200000U,(seconds+2)*48000U);std::ofstream wav(file.path,std::ios::binary);const auto put=[&](std::uint32_t value,unsigned bytes){for(unsigned i=0;i<bytes;++i)wav.put(static_cast<char>(value>>(8*i)));};
    wav.write("RIFF",4);put(36+source_frames*8,4);wav.write("WAVEfmt ",8);put(16,4);put(3,2);put(2,2);put(48000,4);put(48000*8,4);put(8,2);put(32,2);wav.write("data",4);put(source_frames*8,4);
    for(unsigned f=0;f<source_frames;++f){const auto v=static_cast<float>(f%127)/256.f;put(std::bit_cast<std::uint32_t>(v),4);put(std::bit_cast<std::uint32_t>(v*.5f),4);}wav.close();disk_asset=std::make_shared<const AudioData>(open_wav(file.path));memory_asset=std::make_shared<const AudioData>(load_wav(file.path));
    if(!disk_asset->file||!disk_asset->samples.empty())throw std::runtime_error("disk fixture preloaded");
    std::cout<<"topology,device_frames,process_frames,active,workers,warmup,measured,p50_us,p95_us,p99_us,max_us,late,underruns,invalidations,producer_max_us,worker_timeouts,disk_underruns,queue_bytes,pcm_checked,checksum,coarse_clock,max_callback_gap_us,burst_callbacks,profiling,elapsed_s,recorded_frames,record_fault\n";
    bool ran=false;
    for(const auto& topology:{"playback2","playback8","master8","mixed8","disk8","mixed_master8","mixed_disk8"})for(auto frames:{64U,128U,256U,512U}){
        if((!selected_topology.empty()&&selected_topology!=topology)||(selected_frames&&selected_frames!=frames))continue;
        measured=seconds?seconds*48000/frames:120;const auto total=warm+measured;
        auto ref=engine(topology,frames,1,true);std::vector<float> input(frames,.2f),expected(static_cast<std::size_t>(total)*frames*2);
        for(unsigned n=0;n<total;++n)ref->process(input.data(),expected.data()+static_cast<std::size_t>(n)*frames*2,frames);
        for(auto workers:all_workers?std::vector<unsigned>{1,4}:std::vector<unsigned>{selected_workers})for(auto process:{0U,256U,1024U,4096U}){if((process&&process<frames)||(selected_process!=UINT32_MAX&&selected_process!=process))continue;ran=true;
            auto e=std::make_shared<AudioEngine>();RenderConfig config{48000,1,2,frames,frames,workers};config.worker_wait_ms=500;auto graph=fixture(topology,frames,false);
            std::shared_ptr<Recorder> recorder;const auto recording_path=file.path.parent_path()/("mrs-p4-capture-"+new_id().value+".wav");
            if(record){recorder=std::make_shared<Recorder>(recording_path,48000,0);graph.recordings.push_back(recorder);}
            e->prepare(config,std::move(graph));e->set_profiling(profiling);e->enqueue({ControlKind::play});AheadRenderer cache(e,frames,process);cache.start();std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::vector<float> output(frames*2);std::vector<double> times;times.reserve(measured);double checksum{};unsigned late{},checked{};
            const auto interval=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(static_cast<double>(frames)/48000));const auto session_start=Clock::now();auto next=session_start,previous=next;double max_gap{};unsigned bursts{};
            for(unsigned n=0;n<total;++n){const auto before=e->metrics().ahead_underruns;const auto start=Clock::now();if(n){const auto gap=std::chrono::duration<double,std::micro>(start-previous).count();max_gap=std::max(max_gap,gap);if(gap<frames*1e6/96000)++bursts;}previous=start;
                cache.process(input.data(),output.data(),frames);const auto ns=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count());e->observe(ns,frames,0);
                if(n>=warm){const auto us=ns/1000.;times.push_back(us);if(us>frames*1e6/48000)++late;for(float sample:output)checksum+=sample;}
                if(e->metrics().ahead_underruns==before&&e->metrics().ahead_underruns==0){const auto heard=e->state().sample;const auto block=static_cast<std::size_t>(heard/frames-1);
                    if(block<total){if(!std::equal(output.begin(),output.end(),expected.begin()+block*frames*2))throw std::runtime_error("benchmark PCM differs from serial reference");++checked;}}
                next+=interval;pacer.until(next,coarse);
            }
            const auto m=e->metrics();if(m.processing_fault)throw std::runtime_error("benchmark processing fault");std::sort(times.begin(),times.end());const auto q=[&](double p){return times[static_cast<std::size_t>(std::ceil(p*times.size()))-1];};
            std::cout<<topology<<','<<frames<<','<<process<<','<<cache.active()<<','<<m.processing_workers<<','<<warm<<','<<measured<<','<<std::fixed<<std::setprecision(3)<<q(.5)<<','<<q(.95)<<','<<q(.99)<<','<<times.back()<<','<<late<<','<<m.ahead_underruns<<','<<m.ahead_invalidations<<','<<m.ahead_max_process_ns/1000.<<','<<m.worker_timeouts<<','<<m.disk_underruns<<','<<m.ahead_memory_bytes<<','<<checked<<','<<std::setprecision(9)<<checksum<<','<<coarse<<','<<max_gap<<','<<bursts<<','<<profiling<<','<<std::chrono::duration<double>(Clock::now()-session_start).count()<<',';
            cache.stop();RecordStatus record_status{};Sample recorded{};if(recorder){const auto captured=recorder->finish();recorded=captured.frames;record_status=captured.status;const auto raw=load_wav(recording_path,128*1024*1024);
                if(recorded!=static_cast<Sample>(total)*frames||raw.frames()!=recorded||record_status.fault!=RecordFault::none||!std::all_of(raw.samples.begin(),raw.samples.end(),[](float sample){return sample==.2f;}))throw std::runtime_error("sustained raw capture differs");std::filesystem::remove(recording_path);}
            std::cout<<recorded<<','<<static_cast<int>(record_status.fault)<<'\n';
        }
    }if(!ran)throw std::invalid_argument("no matching benchmark cases");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
