// Developer/CI CPU baseline only. No DAW integration and no Python inference.
#include <audiocpp.h>
#include <mrs/sha256.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif
namespace {
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point at){return std::chrono::duration<double,std::milli>(Clock::now()-at).count();}
void need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void check(audiocpp_status value){if(value!=AUDIOCPP_OK)throw std::runtime_error(audiocpp_last_error());}
std::string text(const fs::path& p){const auto s=p.u8string();return {s.begin(),s.end()};}
std::uint32_t read32(std::istream& in){std::array<unsigned char,4> b{};in.read(reinterpret_cast<char*>(b.data()),4);need(bool(in),"Truncated WAV");return b[0]|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);}
std::uint16_t read16(std::istream& in){std::array<unsigned char,2> b{};in.read(reinterpret_cast<char*>(b.data()),2);need(bool(in),"Truncated WAV");return static_cast<std::uint16_t>(b[0]|(unsigned(b[1])<<8));}
std::string fourcc(std::istream& in){std::array<char,4> b{};in.read(b.data(),4);need(bool(in),"Truncated WAV");return {b.begin(),b.end()};}
struct Audio{std::vector<float> samples;int rate{},channels{};std::size_t frames()const{return samples.size()/static_cast<std::size_t>(channels);}};
Audio readAudio(const fs::path& file){
    static_assert(std::endian::native==std::endian::little);
    const auto size=fs::file_size(file);need(size>=44&&size<=128*1024*1024,"Baseline input size limit");
    std::ifstream in(file,std::ios::binary);need(fourcc(in)=="RIFF","Expected RIFF WAV");
    const auto riff=read32(in);need(std::uint64_t(riff)+8==size&&fourcc(in)=="WAVE","Invalid RIFF extent");
    Audio audio;std::uint64_t offset{};std::uint32_t dataBytes{};bool format{};
    while(static_cast<std::uint64_t>(in.tellg())+8<=size){
        const auto id=fourcc(in);const auto bytes=read32(in);const auto begin=static_cast<std::uint64_t>(in.tellg());
        need(bytes<=size-begin,"Invalid WAV chunk extent");
        if(id=="fmt "){
            need(!format&&bytes>=16&&read16(in)==3,"Baseline requires IEEE FLOAT32 WAV");
            audio.channels=read16(in);audio.rate=static_cast<int>(read32(in));(void)read32(in);
            const auto block=read16(in);const auto bits=read16(in);
            need((audio.channels==1||audio.channels==2)&&audio.rate==44100&&block==audio.channels*4&&bits==32,"Expected mono/stereo 44.1kHz FLOAT32 snapshot");format=true;
        }else if(id=="data"){need(offset==0,"Multiple data chunks");offset=begin;dataBytes=bytes;}
        const auto next=begin+bytes+(bytes&1u);need(next<=size,"Invalid WAV padding");in.seekg(static_cast<std::streamoff>(next));
    }
    need(format&&offset&&dataBytes&&dataBytes%(static_cast<unsigned>(audio.channels)*4)==0,"Missing WAV audio");
    need(dataBytes/(static_cast<unsigned>(audio.channels)*4)<=44100u*300u,"Baseline supports at most five minutes");
    audio.samples.resize(dataBytes/4);in.seekg(static_cast<std::streamoff>(offset));in.read(reinterpret_cast<char*>(audio.samples.data()),dataBytes);
    need(bool(in)&&std::all_of(audio.samples.begin(),audio.samples.end(),[](float v){return std::isfinite(v);}),"Invalid PCM");return audio;
}
void writeAudio(const fs::path& file,const float* pcm,std::size_t frames,int rate,int channels){
    need(frames<=44100u*300u&&(channels==1||channels==2),"Output bounds");
    std::ofstream out(file,std::ios::binary);const auto put=[&](std::uint32_t v,int count){for(int i=0;i<count;++i)out.put(static_cast<char>((v>>(8*i))&255));};
    const auto bytes=static_cast<std::uint32_t>(frames*static_cast<std::size_t>(channels)*4);
    out.write("RIFF",4);put(bytes+36,4);out.write("WAVEfmt ",8);put(16,4);put(3,2);put(static_cast<std::uint32_t>(channels),2);
    put(static_cast<std::uint32_t>(rate),4);put(static_cast<std::uint32_t>(rate*channels*4),4);put(static_cast<std::uint32_t>(channels*4),2);put(32,2);out.write("data",4);put(bytes,4);
    out.write(reinterpret_cast<const char*>(pcm),bytes);out.close();need(bool(out),"Output write failed");
}
std::uint64_t peakRss(){
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};counters.cb=static_cast<DWORD>(sizeof(counters));
    return GetProcessMemoryInfo(GetCurrentProcess(),&counters,static_cast<DWORD>(sizeof(counters)))?static_cast<std::uint64_t>(counters.PeakWorkingSetSize):0;
#else
    rusage usage{};return getrusage(RUSAGE_SELF,&usage)==0?static_cast<std::uint64_t>(usage.ru_maxrss)*1024:0;
#endif
}
int run(const std::vector<fs::path>& args){
    need(args.size()==6,"Usage: probe MODEL INPUT OUTPUT THREADS REPEATS");
    const auto modelFile=fs::absolute(args[1]),input=fs::absolute(args[2]),output=fs::absolute(args[3]);
    const int threads=std::stoi(text(args[4])),repeats=std::stoi(text(args[5]));need(threads>=1&&threads<=16&&repeats>=1&&repeats<=3,"Invalid baseline limits");
    need(!fs::exists(output),"Evidence directory already exists");fs::create_directories(output);
    const auto started=Clock::now();const auto sourceHash=mrs::sha256_file(input);const auto audio=readAudio(input);
    const auto abi=audiocpp_abi_version();need((abi>>16)==AUDIOCPP_ABI_VERSION_MAJOR&&((abi>>8)&255)>=AUDIOCPP_ABI_VERSION_MINOR,"Incompatible C API ABI");
    audiocpp_registry* rawRegistry{};check(audiocpp_registry_create(nullptr,&rawRegistry));
    std::unique_ptr<audiocpp_registry,decltype(&audiocpp_registry_free)> registry(rawRegistry,audiocpp_registry_free);
    auto at=Clock::now();audiocpp_model* rawModel{};const auto modelPath=text(modelFile);
    check(audiocpp_model_load(registry.get(),modelPath.c_str(),nullptr,nullptr,&rawModel));
    std::unique_ptr<audiocpp_model,decltype(&audiocpp_model_free)> model(rawModel,audiocpp_model_free);const auto loadMs=elapsed(at);
    need(audiocpp_model_supports(model.get(),"sep","offline")!=0,"Native model lacks sep/offline capability");
    const audiocpp_backend_config backend{"cpu",0,threads};audiocpp_session* rawSession{};at=Clock::now();
    check(audiocpp_session_create(model.get(),"sep","offline",&backend,nullptr,&rawSession));
    std::unique_ptr<audiocpp_session,decltype(&audiocpp_session_free)> session(rawSession,audiocpp_session_free);const auto sessionMs=elapsed(at);
    std::unique_ptr<audiocpp_request,decltype(&audiocpp_request_free)> request(audiocpp_request_create(),audiocpp_request_free);need(bool(request),"Request allocation failed");
    check(audiocpp_request_set_audio(request.get(),audio.samples.data(),audio.frames(),audio.rate,audio.channels));
    std::ofstream report(output/"report.json");report<<std::setprecision(12)<<"{\"upstream\":\"41c1f8ffa0d13a1b7e366e30ea5fe5ab8d169d48\",\"abi\":"<<abi<<",\"threads\":"<<threads<<",\"input_sha256\":"<<std::quoted(sourceHash)<<",\"model_sha256\":"<<std::quoted(mrs::sha256_file(modelFile))<<",\"frames\":"<<audio.frames()<<",\"load_ms\":"<<loadMs<<",\"session_ms\":"<<sessionMs<<",\"runs\":[";
    const std::set<std::string> expected{"drums","bass","other","vocals","guitar","piano"};
    for(int iteration=0;iteration<repeats;++iteration){
        at=Clock::now();audiocpp_result* rawResult{};check(audiocpp_session_run(session.get(),request.get(),&rawResult));
        std::unique_ptr<audiocpp_result,decltype(&audiocpp_result_free)> result(rawResult,audiocpp_result_free);const auto inferenceMs=elapsed(at);
        need(audiocpp_result_named_audio_count(result.get())==6,"HTDemucs6 must return six native outputs");
        const auto folder=output/("run"+std::to_string(iteration));fs::create_directory(folder);
        if(iteration){report<<',';}
        report<<"{\"inference_ms\":"<<inferenceMs<<",\"outputs\":[";std::set<std::string> seen;
        for(std::size_t i=0;i<6;++i){
            const char* name{};const float* samples{};std::size_t frames{};int rate{},channels{};
            check(audiocpp_result_named_audio(result.get(),i,&name,&samples,&frames,&rate,&channels));
            need(name&&expected.contains(name)&&seen.insert(name).second&&samples,"Invalid native stem role");
            need(frames==audio.frames()&&rate==audio.rate&&channels==audio.channels,"Native result geometry changed");
            float peak{};for(std::size_t j=0;j<frames*static_cast<std::size_t>(channels);++j){need(std::isfinite(samples[j]),"Non-finite native output");peak=std::max(peak,std::abs(samples[j]));}
            const auto file=folder/(std::string(name)+".wav");writeAudio(file,samples,frames,rate,channels);
            if(i){report<<',';}
            report<<"{\"stem\":"<<std::quoted(name)<<",\"frames\":"<<frames<<",\"rate\":"<<rate<<",\"channels\":"<<channels<<",\"peak\":"<<peak<<",\"sha256\":"<<std::quoted(mrs::sha256_file(file))<<'}';
        }
        report<<"]}";report.flush();std::cout<<"PASS native HTDemucs6 run "<<iteration<<" inference_ms="<<inferenceMs<<"\n"<<std::flush;
    }
    need(mrs::sha256_file(input)==sourceHash,"Input modified during native baseline");
    report<<"],\"total_ms\":"<<elapsed(started)<<",\"peak_rss_bytes\":"<<peakRss()<<",\"input_unchanged\":true,\"gain_policy\":\"unclipped_float32\"}\n";report.close();need(bool(report),"Report write failed");return 0;
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
#else
int main(int argc,char** argv){
#endif
    try{std::vector<fs::path> args;for(int i=0;i<argc;++i)args.emplace_back(argv[i]);return run(args);}
    catch(const std::exception& e){std::cerr<<"Native baseline failed: "<<e.what()<<'\n';return 1;}
}
