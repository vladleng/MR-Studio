#include <mrs/desktop.hpp>
#include <mrs/wav_resample.hpp>
#include <bit>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stop_token>
#include <thread>

namespace {
using namespace mrs;
namespace fs=std::filesystem;
fs::path sourcePath(const std::string& value){return fs::path(std::u8string(value.begin(),value.end()));}
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),#__VA_ARGS__)
template<class F> void rejects(F&& f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}CHECK(bad);}
struct Directory {
    fs::path path=fs::temp_directory_path()/("mrs-import-test-"+new_id().value);
    Directory(){fs::create_directory(path);}
    ~Directory(){std::error_code ec;fs::remove_all(path,ec);}
};
void wav(const fs::path& path,std::uint32_t rate,std::uint32_t frames,double hz=1000.,bool pcm16=false){
    std::ofstream out(path,std::ios::binary);const std::uint32_t width=pcm16?2:4;
    const auto put=[&](std::uint32_t v,int bytes){for(int i=0;i<bytes;++i)out.put(static_cast<char>((v>>(8*i))&255));};
    out.write("RIFF",4);put(36+frames*2*width,4);out.write("WAVEfmt ",8);put(16,4);put(pcm16?1:3,2);put(2,2);
    put(rate,4);put(rate*2*width,4);put(2*width,2);put(width*8,2);out.write("data",4);put(frames*2*width,4);
    for(std::uint32_t i=0;i<frames;++i){
        const float sample=hz==0?.25f:static_cast<float>(.5*std::sin(2.*std::numbers::pi*hz*i/rate));
        for(const float value:{sample,-sample}){
            if(pcm16)put(static_cast<std::uint16_t>(static_cast<std::int16_t>(value*32768)),2);
            else put(std::bit_cast<std::uint32_t>(value),4);
        }
    }
    CHECK(static_cast<bool>(out));
}
std::string bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void finish(desktop::Application& app){
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while(app.wav_import_status().busy&&std::chrono::steady_clock::now()<until){app.poll();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    CHECK(!app.wav_import_status().busy);
}
void no_staging(const fs::path& root){for(const auto& f:fs::directory_iterator(root))CHECK(!f.path().filename().string().starts_with(".mrs-import-"));}
void quality(){
    Directory dir;const auto input=dir.path/"16-bit.wav",output=dir.path/"48.wav";
    wav(input,44100,4410,1000,true);const auto original=bytes(input);float progress{};
    audio::resample_wav_file(input,output,48000,{},[&](float p){CHECK(p>=progress&&p<=1);progress=p;});
    const auto data=audio::load_wav(output);CHECK(data.frames()==4800&&data.sample_rate==48000&&data.channels==2&&progress==1);
    double error{};
    for(std::size_t i=100;i<4700;++i){const auto value=data.samples[i*2];const auto expected=.5*std::sin(2.*std::numbers::pi*1000.*static_cast<double>(i)/48000.);
        error+=(value-expected)*(value-expected);CHECK(std::abs(value+data.samples[i*2+1])<1e-7f);}
    CHECK(std::sqrt(error/4600)<.0001);CHECK(bytes(input)==original);
    rejects([&]{audio::resample_wav_file(input,input,48000);});
    rejects([&]{audio::resample_wav_file(input,output,48000);});CHECK(bytes(input)==original);
    const auto dc=dir.path/"dc.wav",converted=dir.path/"dc-44.wav";wav(dc,48000,4800,0);
    audio::resample_wav_file(dc,converted,44100);const auto constant=audio::load_wav(converted);
    CHECK(constant.frames()==4410);for(std::size_t i=0;i<constant.samples.size();i+=2){CHECK(std::abs(constant.samples[i]-.25f)<.000002f);CHECK(std::abs(constant.samples[i+1]+.25f)<.000002f);}
    const auto high=dir.path/"96.wav",low=dir.path/"alias.wav";wav(high,96000,9600,30000);
    audio::resample_wav_file(high,low,48000);const auto filtered=audio::load_wav(low);double energy{};
    for(std::size_t i=200;i<4600;++i)energy+=filtered.samples[i*2]*filtered.samples[i*2];
    CHECK(std::sqrt(energy/4400)<.0001);
    std::stop_source stop;const auto cancelled=dir.path/"cancel.wav";
    rejects([&]{audio::resample_wav_file(input,cancelled,48000,stop.get_token(),[&](float){stop.request_stop();});});
    CHECK(!fs::exists(cancelled));CHECK(bytes(input)==original);
    // Endpoint handling for a source shorter than the sinc halo and a fractional duration.
    const auto shortfile=dir.path/"short.wav",shortout=dir.path/"short-out.wav";wav(shortfile,44100,1,0);
    audio::resample_wav_file(shortfile,shortout,48000);const auto tiny=audio::load_wav(shortout);
    CHECK(tiny.frames()==2);CHECK(std::abs(tiny.samples[0]-.25f)<.000002f);
}
void project_path_alias(){
    Directory dir;const auto actual=dir.path/"actual",alias=dir.path/"alias";
    fs::create_directories(actual/"project");std::error_code ec;fs::create_directory_symlink(actual,alias,ec);
    // Windows workers may lack symlink privileges; their ordinary temp-root
    // 8.3 alias is also exercised by the full Save/Open regression below.
    if(ec)return;
    const auto root=alias/"project";desktop::MediaCopy copies(root);
    const auto staging=root/(".mrs-import-"+new_id().value);fs::create_directory(staging);
    const auto input=staging/"item.wav";wav(input,48000,100,0);
    const auto published=copies.adopt_import(input,"imported.wav");
    CHECK(published.lexically_relative(root)==fs::path("Media")/"imported.wav");
    CHECK(fs::is_regular_file(published)&&!fs::exists(input));copies.commit();fs::remove(staging);
}
void imports(){
    Directory dir;const auto input=dir.path/"original.wav",matching=dir.path/"matching.wav";
    wav(input,44100,4410,1000,true);wav(matching,48000,4800);const auto original=bytes(input);
    desktop::Application app;app.new_project(48000);const auto track=app.add_audio_track("Target");
    const auto revision=app.services().projects->state().revision;
    app.queue_wav_import({input,matching,input},track,123);CHECK(app.wav_import_status().busy);
    CHECK(app.snapshot().project.clips.empty());rejects([&]{app.queue_wav_import({input});});finish(app);
    const auto project=app.snapshot().project;CHECK(project.clips.size()==3);CHECK(app.services().projects->state().revision==revision+1);
    CHECK(project.clips[0].start==123&&project.clips[1].start==4923&&project.clips[2].start==9723);
    CHECK(project.clips[0].length==4800&&project.clips[0].source==project.clips[2].source);
    const auto temporary=sourcePath(project.clips[0].source);CHECK(fs::is_regular_file(temporary));
    CHECK(app.undo()&&app.snapshot().project.clips.empty());CHECK(fs::exists(temporary));
    CHECK(app.redo()&&app.snapshot().project==project);
    const auto saved=dir.path/"saved"/"project.mrsproject";app.save_project(saved);
    const auto persisted=app.snapshot().project;CHECK(persisted.clips.size()==3);
    app.open_project(app.path());CHECK(app.snapshot().project==persisted);CHECK(!fs::exists(temporary));
    CHECK(bytes(input)==original);
    const auto before=app.snapshot().project;
    const auto mediaCount=[&]{return std::distance(fs::directory_iterator(app.path().parent_path()/"Media"),fs::directory_iterator{});};
    const auto countBefore=mediaCount();rejects([&]{app.import_wavs({input},{},max_sample);});
    CHECK(app.snapshot().project==before&&mediaCount()==countBefore);no_staging(app.path().parent_path());
    app.queue_wav_import({input,dir.path/"missing.wav"});finish(app);CHECK(app.snapshot().project==before);
    CHECK(app.wav_import_status().message.find("failed")!=std::string::npos);no_staging(app.path().parent_path());
    app.queue_wav_import({input});app.add_audio_track("Changed while preparing");const auto edited=app.snapshot().project;
    finish(app);CHECK(app.snapshot().project==edited);no_staging(app.path().parent_path());
    app.queue_wav_import({input});app.cancel_wav_import();CHECK(!app.wav_import_status().busy);
    for(int i=0;i<100;++i){app.poll();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    CHECK(app.snapshot().project==edited);no_staging(app.path().parent_path());
    app.queue_wav_import({input,input});finish(app);CHECK(app.snapshot().project.clips.size()==before.clips.size()+2);
    const auto imported=app.snapshot().project.clips.back();const auto media=app.path().parent_path()/sourcePath(imported.source);
    CHECK(fs::is_regular_file(media)&&audio::inspect_wav(media).sample_rate==48000);no_staging(app.path().parent_path());
    CHECK(app.undo()&&app.redo());app.save_project(app.path());app.open_project(app.path());
    if(app.snapshot().project.clips.back()!=imported)std::cerr<<"Imported source: "<<imported.source<<"; reopened source: "<<app.snapshot().project.clips.back().source<<'\n';
    CHECK(app.snapshot().project.clips.back()==imported);
    const auto saveAs=dir.path/"save-as"/"copy.mrsproject";app.save_project(saveAs);app.open_project(app.path());
    const auto copied=app.path().parent_path()/sourcePath(app.snapshot().project.clips.back().source);
    CHECK(fs::is_regular_file(copied)&&audio::inspect_wav(copied).sample_rate==48000);
    CHECK(bytes(input)==original);
}
class ManualDevice final:public audio::IAudioDevice {
    audio::DevicePhase phase{audio::DevicePhase::closed};
public:
    std::vector<audio::DeviceInfo> enumerate() override{return {{0,"Import fixture",{}, {"L","R"},32,8192,128,-1}};}
    void control_panel(int) override{}
    void open(const audio::DeviceConfig&,std::shared_ptr<audio::AudioEngine>) override{phase=audio::DevicePhase::open;}
    void start() override{phase=audio::DevicePhase::running;}
    void stop() override{phase=audio::DevicePhase::stopped;}
    void close() noexcept override{phase=audio::DevicePhase::closed;}
    audio::DeviceStatus status() override{return {phase,48000,0,0,0,{}};}
};
void streamed_lifetime(){
    Directory dir;const auto input=dir.path/"large.wav";wav(input,44100,1100000,0);
    fs::path temporary;std::shared_ptr<audio::AudioEngine> engine;
    {
        desktop::Application app;app.new_project(48000);app.queue_wav_import({input});finish(app);
        CHECK(app.snapshot().project.clips.size()==1);temporary=sourcePath(app.snapshot().project.clips.front().source);
        app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});
        CHECK(fs::file_size(temporary)>8*1024*1024);engine=app.engine();CHECK(fs::exists(temporary));
    }
    // Application shutdown cannot delete a streamed file still owned by its retained renderer.
    CHECK(fs::exists(temporary));engine.reset();CHECK(!fs::exists(temporary));
    {
        desktop::Application app;app.queue_wav_import({input});app.new_project(48000);finish(app);CHECK(app.snapshot().project.clips.empty());
    }
}
}
int main(){try{quality();project_path_alias();imports();streamed_lifetime();std::cout<<"WAV SRC, background import, persistence, cancellation and streaming lifetime PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
