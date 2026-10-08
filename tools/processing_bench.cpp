#include <mrs/vst3.hpp>
#include <mrs/persistence.hpp>
#include <mrs/no_denormals.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#define NOMINMAX
#include <windows.h>

// Offline CPU experiment only: no audio device, no editor, no user project writes.
int wmain(int argc,wchar_t** argv){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    try {
        if(argc<3)throw std::runtime_error("chunk(64/128) plugin-path... expected");
        const auto chunk=static_cast<std::uint32_t>(std::stoul(argv[1]));
        if(chunk<1||chunk>128)throw std::runtime_error("chunk must be 1..128 for this 128-frame benchmark");
        std::vector<mrs::NativeInsert> effects;
        if(argc==4&&std::wstring(argv[2])==L"--project"){
            const auto document=mrs::persistence::load_project(std::filesystem::path(argv[3]));
            auto append=[&](const auto& chain){for(const auto& effect:chain)if(effect.kind==mrs::InsertKind::vst3){effects.push_back(effect);std::cout<<"Saved plugin: "<<effect.plugin_name<<"\n";}};
            for(const auto& track:document.project.tracks)append(track.inserts);append(document.project.master_inserts);
            if(effects.empty()||effects.size()>8)throw std::runtime_error("project benchmark needs 1..8 VST3 effects; tracks flattened for this CPU experiment only");
        }else for(int i=2;i<argc;++i){const auto path=std::filesystem::path(argv[i]).u8string();const auto plugins=mrs::processing::probe_vst3(std::string(path.begin(),path.end()));
            if(plugins.empty())throw std::runtime_error("no VST3 effect class");const auto& p=plugins.front();
            mrs::NativeInsert effect;effect.id=mrs::new_id();effect.kind=mrs::InsertKind::vst3;effect.plugin_path=p.path;effect.class_id=p.class_id;effect.plugin_name=p.name;effects.push_back(effect);
            std::cout<<"Plugin: "<<p.name<<"\n";
        }
        const auto saved=std::make_shared<const mrs::processing::GraphState>(mrs::processing::insert_graph(effects));
        mrs::processing::PreparedGraph graph{{saved,0,false,false},{48000,2,chunk,128},mrs::processing::hosted_factory};
        constexpr int frames=128,iterations=3000,warmup=400;
        const mrs::ScopedNoDenormals no_denormals;
        std::array<float,frames*2> audio{};std::vector<double> times;times.reserve(iterations);
        for(int n=-warmup;n<iterations;++n){for(int f=0;f<frames;++f)audio[f*2]=audio[f*2+1]=.02f*std::sin(static_cast<float>((n+warmup)*frames+f)*.031f);
            const auto begin=std::chrono::steady_clock::now();
            for(std::uint32_t base=0;base<frames;base+=chunk){const auto count=std::min(chunk,frames-base);graph.process(audio.data()+base*2,count,static_cast<mrs::Sample>((n+warmup)*frames+base),true);}
            const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();if(n>=0)times.push_back(us);
            if(graph.failed()||!std::all_of(audio.begin(),audio.end(),[](float value){return std::isfinite(value);}))throw std::runtime_error("plugin processing failed");
        }
        const double average=std::accumulate(times.begin(),times.end(),0.)/times.size();std::sort(times.begin(),times.end());
        const auto percentile=[&](double p){return times[static_cast<std::size_t>((times.size()-1)*p)];};
        std::cout<<std::fixed<<std::setprecision(3)<<"rate=48000 callback_frames=128 chunk="<<chunk<<" plugin_calls_per_callback="<<effects.size()*((frames+chunk-1)/chunk)
            <<" latency_samples="<<graph.latency().output<<"\naverage_us="<<average<<" p50_us="<<percentile(.5)<<" p95_us="<<percentile(.95)<<" p99_us="<<percentile(.99)<<" max_us="<<times.back()
            <<" deadline_us="<<frames/48000.*1e6<<" deadline_misses="<<std::count_if(times.begin(),times.end(),[](double us){return us>frames/48000.*1e6;})<<"/"<<iterations<<"\n";
        CoUninitialize();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";CoUninitialize();return 1;}
}
