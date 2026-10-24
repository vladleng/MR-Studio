#include <mrs/stretch.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace mrs::stretch;
constexpr double pi=3.14159265358979323846;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
// Original procedural transient/harmonic fixture, no third-party audio assets.
float fixture(std::int64_t frame,std::uint32_t rate,unsigned channel) {
    const double t=static_cast<double>(frame)/rate;
    const double beat=std::fmod(t,0.5);
    const double drum=std::exp(-beat*70)*std::sin(2*pi*(65*beat+18*std::exp(-beat*25)));
    const double harmonic=0.13*std::sin(2*pi*220*t)+0.06*std::sin(2*pi*440*t)+0.03*std::sin(2*pi*660*t);
    return static_cast<float>((harmonic+0.2*drum)*(channel==0?1:0.75));
}
void u32(std::ostream& out,std::uint32_t n) {
    for(unsigned i=0;i<4;++i) out.put(static_cast<char>((n>>(8*i))&255));
}
void u16(std::ostream& out,std::uint16_t n) {
    out.put(static_cast<char>(n&255));out.put(static_cast<char>((n>>8)&255));
}
void audition(const std::filesystem::path& destination,Config config,Parameters p,double seconds) {
    require(!std::filesystem::exists(destination),"Output already exists; choose a new path");
    // Isolated staging directory owned by this invocation; never overwrite files.
    auto staging=destination;staging += ".stretch-staging";
    require(std::filesystem::create_directory(staging),"Output staging already exists");
    try {
        std::int64_t frames{};
        const auto source=static_cast<std::int64_t>(seconds*config.sample_rate);
        require(output_frames(source,p.duration_ratio,frames)==Status::ok,"Invalid render length");
        require(frames <= (UINT32_MAX-36)/8,"WAV exceeds RIFF limit");
        const auto temp=staging/"render.wav";
        std::ofstream out(temp,std::ios::binary);
        out.write("RIFF",4);u32(out,36+static_cast<std::uint32_t>(frames)*8);out.write("WAVEfmt ",8);
        u32(out,16);u16(out,3);u16(out,2);u32(out,config.sample_rate);u32(out,config.sample_rate*8);
        u16(out,8);u16(out,32);out.write("data",4);u32(out,static_cast<std::uint32_t>(frames)*8);
        auto engine=make_signalsmith_engine();
        const auto result=render(*engine,config,p,source,
            [&](std::int64_t first,std::span<float> block) {
                for(std::size_t i=0;i<block.size();++i) block[i]=fixture(first+static_cast<std::int64_t>(i/2),config.sample_rate,static_cast<unsigned>(i%2));
                return true;
            },[&](std::int64_t,std::span<const float> block) {
                for(float sample:block) u32(out,std::bit_cast<std::uint32_t>(sample));
                return static_cast<bool>(out);
            });
        require(result.status==Status::ok && result.frames==frames,"Audition render failed");
        out.close();require(static_cast<bool>(out),"Audition write failed");
        // copy_file without overwrite is also safe if destination appeared meanwhile.
        std::filesystem::copy_file(temp,destination);
        std::filesystem::remove_all(staging);
    } catch(...) { std::filesystem::remove_all(staging);throw; }
}
// Diagnostic, not an acceptance test: preserves measured failures in the report.
void impulses() {
    Config config;config.channels=1;
    std::cout<<"ratio,source_frame,expected_frame,peak_frame,error_frames,peak_amplitude,within_one_frame,peak_db,energy_90_width_frames,early_energy_fraction\n";
    // Sweep window positions: a single conveniently aligned impulse can hide
    // severe attack attenuation at another phase of the analysis hop.
    for(double ratio:{0.5,0.75,0.9,1.,1.1,1.25,1.5,2.})
        for(std::int64_t at:{0,12001,12317,12733,13211,13709,14303,14927,15619,24000,47999}) {
        Parameters p;p.duration_ratio=ratio;p.cents=ratio==1?0.01:0;
        std::int64_t length{};require(output_frames(48000,ratio,length)==Status::ok,"Impulse length failed");
        std::vector<float> output(static_cast<std::size_t>(length));
        auto engine=make_signalsmith_engine();
        const auto result=render(*engine,config,p,48000,[&](std::int64_t first,std::span<float> block){
            std::fill(block.begin(),block.end(),0.f);
            if(at>=first && at-first<static_cast<std::int64_t>(block.size())) block[static_cast<std::size_t>(at-first)]=1;
            return true;
        },[&](std::int64_t first,std::span<const float> block){std::copy(block.begin(),block.end(),output.begin()+first);return true;});
        require(result.status==Status::ok,"Impulse render failed");
        const auto peak=std::max_element(output.begin(),output.end(),[](float a,float b){return std::abs(a)<std::abs(b);});
        std::int64_t expected{};require(output_frames(at,ratio,expected)==Status::ok,"Impulse mapping failed");
        const auto position=peak-output.begin();const auto error=position-expected;
        double energy{},early{};
        const auto early_boundary=expected-static_cast<std::int64_t>(config.sample_rate/1000);
        for(std::size_t i=0;i<output.size();++i) {
            const double e=static_cast<double>(output[i])*output[i];energy+=e;
            if(static_cast<std::int64_t>(i)<early_boundary) early+=e;
        }
        require(std::isfinite(energy) && energy>0,"Impulse output lost or invalid");
        double cumulative{};std::int64_t q05=-1,q95=-1;
        for(std::size_t i=0;i<output.size();++i) {
            cumulative+=static_cast<double>(output[i])*output[i];
            if(q05<0 && cumulative>=energy*0.05) q05=static_cast<std::int64_t>(i);
            if(cumulative>=energy*0.95) {q95=static_cast<std::int64_t>(i);break;}
        }
        const double peak_db=20*std::log10(std::max(1e-30,static_cast<double>(std::abs(*peak))));
        std::cout<<ratio<<','<<at<<','<<expected<<','<<position<<','<<error<<','<<*peak<<','<<(std::abs(error)<=1)
            <<','<<peak_db<<','<<q95-q05+1<<','<<early/energy<<'\n';
    }
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string_view(argv[1])=="--impulse-report") {impulses();return 0;}
        double seconds=2,warmup=0.25;
        unsigned voices=1,block=128,repeats=1;
        Config config;Parameters p;p.duration_ratio=1.25;
        std::filesystem::path output;
        for(int i=1;i<argc;++i) {
            const std::string key=argv[i];
            if(key=="--split") {config.split_computation=true;continue;}
            if(key=="--cheap") {p.quality=Quality::economical;continue;}
            require(i+1<argc,"Missing argument");const std::string value=argv[++i];
            if(key=="--seconds") seconds=std::stod(value);
            else if(key=="--warmup") warmup=std::stod(value);
            else if(key=="--voices") voices=static_cast<unsigned>(std::stoul(value));
            else if(key=="--block") block=static_cast<unsigned>(std::stoul(value));
            else if(key=="--rate") config.sample_rate=static_cast<unsigned>(std::stoul(value));
            else if(key=="--repeats") repeats=static_cast<unsigned>(std::stoul(value));
            else if(key=="--ratio") p.duration_ratio=std::stod(value);
            else if(key=="--pitch") p.semitones=std::stod(value);
            else if(key=="--output") output=value;
            else throw std::invalid_argument("Unknown argument");
        }
        require(std::isfinite(seconds)&&seconds>0&&seconds<=3600&&std::isfinite(warmup)&&warmup>=0&&warmup<=60&&
                voices>0&&voices<=32&&block>0&&block<=4096&&repeats>0&&repeats<=10&&validate(p)==Status::ok,"Invalid benchmark configuration");
        std::cout<<"# MR Studio 0.7.1; synthetic non-device benchmark; physical ASIO/listening NOT TESTED\n# "<<backend_revision()<<'\n';
        std::cout<<"rate,block,voices,ratio,pitch,quality,split,repeat,seconds,warmup,p50_us,p95_us,p99_us,p999_us,max_us,mean_us,over_budget\n";
        for(unsigned repeat=0;repeat<repeats;++repeat) {
            std::vector<std::unique_ptr<ITimeStretchEngine>> engines;
            for(unsigned i=0;i<voices;++i) {
                auto engine=make_signalsmith_engine();require(engine->prepare(config,p)==Status::ok,"Prepare failed");engines.push_back(std::move(engine));
            }
            std::array<std::vector<float>,2> in{std::vector<float>(32768),std::vector<float>(32768)};
            std::array<std::vector<float>,2> out{std::vector<float>(block),std::vector<float>(block)};
            std::array<const float*,2> inputs{in[0].data(),in[1].data()};std::array<float*,2> outputs{out[0].data(),out[1].data()};
            const auto count=static_cast<unsigned>(std::ceil(seconds*config.sample_rate/block));
            const auto warm=static_cast<unsigned>(std::ceil(warmup*config.sample_rate/block));
            std::vector<double> times;times.reserve(count);
            std::int64_t source{};
            for(unsigned i=0;i<count+warm;++i) {
                const auto required=engines.front()->input_required(block);
                for(unsigned j=0;j<required;++j) for(unsigned c=0;c<2;++c) in[c][j]=fixture(source+j,config.sample_rate,c);
                source+=required;
                const auto start=std::chrono::steady_clock::now();
                for(auto& engine:engines) require(engine->process(inputs,required,outputs,block).status==Status::ok,"Process failed");
                const auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
                if(i>=warm) times.push_back(elapsed);
            }
            std::sort(times.begin(),times.end());
            const auto percentile=[&](double q){return times[static_cast<std::size_t>(q*static_cast<double>(times.size()-1))];};
            double total{};std::size_t late{};const double budget=1e6*block/config.sample_rate;
            for(auto t:times) {total+=t;if(t>budget) ++late;}
            std::cout<<config.sample_rate<<','<<block<<','<<voices<<','<<p.duration_ratio<<','<<p.semitones<<','
                <<(p.quality==Quality::balanced?"balanced":"economical")<<','<<config.split_computation<<','<<repeat+1<<','<<seconds<<','<<warmup<<','
                <<percentile(.5)<<','<<percentile(.95)<<','<<percentile(.99)<<','<<percentile(.999)<<','<<times.back()<<','
                <<total/static_cast<double>(times.size())<<','<<late<<'\n';
        }
        if(!output.empty()) audition(output,config,p,std::min(seconds,30.));
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
