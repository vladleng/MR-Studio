#include <mrs/audio.hpp>
#include <mrs/processing.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
namespace {
using namespace mrs;
using namespace mrs::audio;
using namespace mrs::processing;
class Work final:public IProcessor {
    unsigned iterations_;double state_{.01};
public:
    explicit Work(unsigned iterations):iterations_(iterations){}
    std::vector<ParameterInfo> parameters()const override{return {};}
    void prepare(ProcessConfig)override{}
    void restore(const PluginState&)override{}
    PluginState capture()const override{return {};}
    bool set_parameter(std::uint32_t,float)noexcept override{return false;}
    std::optional<float> parameter_value(std::uint32_t)const noexcept override{return {};}
    std::uint32_t latency()const noexcept override{return 0;}
    bool live_safe()const noexcept override{return true;}
    void warm()override{}
    void reset()noexcept override{state_=.01;}
    void process(ProcessBlock b)noexcept override {
        for(auto& sample:b.audio){for(unsigned n=0;n<iterations_;++n)state_=state_*.99991+sample*.00009;sample=static_cast<float>(sample*.99+state_*.01);}
    }
};
std::shared_ptr<PreparedGraph> chain(unsigned iterations,std::uint32_t block) {
    auto saved=demo_graph();saved.nodes.front().parameters.clear();
    return std::make_shared<PreparedGraph>(GraphSnapshot{std::make_shared<const GraphState>(saved),0,false,false},
        ProcessConfig{48000,2,block},[iterations](const auto&){return std::make_unique<Work>(iterations);});
}
RenderGraph fixture(const std::string& topology,std::uint32_t block) {
    const unsigned tracks=topology=="independent2"||topology=="tiny2"?2:8;
    RenderGraph g;g.mixer.resize(tracks);g.inserts.resize(tracks);g.outputs.assign(tracks,no_mixer_track);g.buses.assign(tracks,false);
    const auto heavy=topology=="tiny2"||topology=="master8"?0U:256U;
    for(unsigned t=0;t<tracks;++t){g.inserts[t]=chain(heavy,block);g.monitor.push_back({0,0,.04f,t});g.monitor.push_back({0,1,.04f,t});}
    if(topology=="serial8") {
        for(unsigned t=0;t<tracks-1;++t)g.outputs[t]=t+1;
        for(unsigned t=1;t<tracks;++t)g.buses[t]=true;
        g.monitor.resize(2);
    } else if(topology=="fanin8") {
        g.mixer.resize(tracks+1);g.buses.push_back(true);g.outputs.assign(tracks,tracks);g.outputs.push_back(no_mixer_track);g.inserts.push_back(chain(64,block));
    }
    if(topology=="fanout8"){g.monitor.resize(2);g.sends.resize(tracks);for(unsigned t=1;t<tracks;++t){g.buses[t]=true;g.sends[0].push_back({t,.1f,false});}}
    if(topology=="master8")g.master_inserts=chain(2048,block);
    g.input_monitoring.assign(g.mixer.size(),true);return g;
}
}
int main(int argc,char** argv) {
    try {
        bool mmcss=true,profiling=false;for(int i=1;i<argc;++i){const std::string arg=argv[i];if(arg=="--no-mmcss")mmcss=false;else if(arg=="--profile")profiling=true;else throw std::invalid_argument("expected --profile or --no-mmcss");}
        constexpr unsigned warmup=100,measured=400;
        std::cout<<"topology,frames,requested_workers,active_workers,mmcss_helpers,warmup,measured,p50_us,p95_us,p99_us,max_us,late,worker_timeouts,checksum,parallel_batches,scheduler_overhead_ns,profiling,critical_max_ns,job_total_ns\n";
        for(const auto& topology:{"independent2","independent8","serial8","fanin8","fanout8","master8","tiny2"})for(auto frames:{64U,128U,256U,512U}) {
            double reference{};
            for(auto workers:{1U,2U,4U,8U}) {
                AudioEngine engine;RenderConfig config{48000,1,2,frames,frames,workers,mmcss};config.worker_wait_ms=500;
                engine.prepare(config,fixture(topology,frames));engine.set_profiling(profiling);std::vector<float> input(frames),output(frames*2);
                for(unsigned f=0;f<frames;++f)input[f]=static_cast<float>(std::sin(f*.07)*.2);
                std::vector<double> times;times.reserve(measured);double checksum{};unsigned late{};
                for(unsigned n=0;n<warmup+measured;++n) {
                    const auto start=std::chrono::steady_clock::now();engine.process(input.data(),output.data(),frames);
                    const auto elapsed=std::chrono::steady_clock::now()-start;
                    if(n>=warmup){const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();engine.observe(static_cast<std::uint64_t>(ns),frames,0);
                        const auto us=static_cast<double>(ns)/1000;times.push_back(us);if(us>frames*1e6/48000)++late;
                        for(float sample:output)checksum+=sample;}
                }
                if(workers==1)reference=checksum;else if(checksum!=reference)throw std::runtime_error("benchmark sample checksum differs");
                const auto m=engine.metrics();if(m.processing_fault)throw std::runtime_error("benchmark worker timeout");
                std::sort(times.begin(),times.end());const auto percentile=[&](double q){return times[static_cast<std::size_t>(std::ceil(q*times.size()))-1];};
                std::cout<<topology<<','<<frames<<','<<workers<<','<<m.processing_workers<<','<<m.audio_scheduled_workers<<','<<warmup<<','<<measured<<','
                    <<std::fixed<<std::setprecision(3)<<percentile(.5)<<','<<percentile(.95)<<','<<percentile(.99)<<','<<times.back()<<','<<late<<','<<m.worker_timeouts<<','<<std::setprecision(9)<<checksum<<','<<m.parallel_batches<<','<<m.scheduler_overhead_ns<<','<<profiling<<','<<engine.profile().device_path.max_ns<<','<<[&]{std::uint64_t total{};for(const auto& t:engine.profile().device_workers)total+=t.total_ns;return total;}()<<'\n';
            }
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
