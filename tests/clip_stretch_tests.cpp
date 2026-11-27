#include <mrs/desktop.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <thread>

namespace {
using namespace mrs;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),#__VA_ARGS__)
template<class F>void rejects(F f){bool bad{};try{f();}catch(const std::exception&){bad=true;}CHECK(bad);}
struct Directory{
    fs::path path=fs::temp_directory_path()/("mrs-stretch-test-"+new_id().value);
    Directory(){fs::create_directory(path);}
    ~Directory(){std::error_code ec;fs::remove_all(path,ec);}
};
Project fixture(){
    auto p=desktop::foundation_demo().project;
    p.clips.front().start=100;p.clips.front().length=48001;p.clips.front().source_offset=13;
    p.clips.front().fade_in=100;p.clips.front().fade_out=200;p.validate();return p;
}
void model(){
    auto p=fixture();const auto id=p.clips.front().id;const auto original=p;
    ProjectStore store(p);stretch::Parameters params;params.duration_ratio=1.23456789;params.semitones=7;params.cents=-13;
    params.formants=stretch::Formants::shift;params.formant_semitones=-2;params.formant_base_hz=180;
    store.execute(SetAudioStretch{id,params});p=*store.state().project;
    const auto stretched=p;CHECK(p.clips[0].length==59260&&p.clips[0].source_offset==13);
    CHECK(deserialize(serialize(p))==p);CHECK(store.undo()&&*store.state().project==original);CHECK(store.redo()&&*store.state().project==stretched);
    // A shared boundary is rounded once, including odd lengths and nested splits.
    SplitAudioClip{id,12345,Id{"right"}}.apply(p);p.validate();
    SplitAudioClip{Id{"right"},35001,Id{"last"}}.apply(p);p.validate();
    for(std::size_t i=1;i<p.clips.size();++i){CHECK(p.clips[i-1].start+p.clips[i-1].length==p.clips[i].start);CHECK(audio_output_offset(p.clips[i-1])+p.clips[i-1].length==audio_output_offset(p.clips[i]));}
    const auto split=p;
    {
        ProjectStore anchored(split);
        auto shorter=params;shorter.duration_ratio=.75;
        const auto& originalRight=split.clips[1];
        anchored.execute(SetAudioStretch{originalRight.id,shorter,false,StretchAnchor::end});
        const auto changed=anchored.state().project;const auto& right=changed->clips[1];
        CHECK(right.start>originalRight.start&&right.start+right.length==originalRight.start+originalRight.length);
        CHECK(right.source_offset==originalRight.source_offset&&right.stretch->window_begin==originalRight.stretch->window_begin&&right.stretch->window_end==originalRight.stretch->window_end);
        CHECK(right.stretch->parameters==shorter&&deserialize(serialize(*changed))==*changed);
        CHECK(anchored.undo()&&*anchored.state().project==split);
        auto tooLong=params;tooLong.duration_ratio=8;
        rejects([&]{anchored.execute(SetAudioStretch{id,tooLong,false,StretchAnchor::end});});
        CHECK(*anchored.state().project==split);
    }
    for(int i=0;i<100;++i){
        for(const auto& c:split.clips){auto bypass=params;bypass.enabled=false;SetAudioStretch{c.id,bypass}.apply(p);}
        p.validate();
        for(const auto& c:split.clips)SetAudioStretch{c.id,params}.apply(p);
        p.validate();for(std::size_t c=0;c<p.clips.size();++c){CHECK(p.clips[c].length==split.clips[c].length);CHECK(p.clips[c].stretch==split.clips[c].stretch);}
    }
    TrimAudioClip{id,111,12000,60000}.apply(p);p.validate();CHECK(p.clips[0].length==11889);
    const auto trimmed=p.clips[0];
    for(int i=0;i<100;++i){auto other=params;other.duration_ratio=.333333333333;SetAudioStretch{id,other}.apply(p);SetAudioStretch{id,params}.apply(p);CHECK(p.clips[0].length==trimmed.length&&p.clips[0].stretch==trimmed.stretch);}
    CHECK(deserialize(serialize(p))==p);
    SetAudioStretch{id,params,true}.apply(p);p.validate();CHECK(!p.clips[0].stretch&&p.clips[0].source_offset>13);
    auto legacy=serialize(original);legacy.replace(legacy.find("MRS_CORE_SNAPSHOT 32"),20,"MRS_CORE_SNAPSHOT 30");
    for(std::size_t at;(at=legacy.find("STRETCH_MODE "))!=std::string::npos;)legacy.erase(at,legacy.find('\n',at)-at+1);
    CHECK(deserialize(legacy)==original);
    auto corrupt=stretched;corrupt.clips[0].stretch->window_end+=10;rejects([&]{corrupt.validate();});
    corrupt=stretched;corrupt.clips[0].stretch->window_begin=std::numeric_limits<double>::infinity();rejects([&]{corrupt.validate();});
    auto invalid=params;invalid.duration_ratio=0;const auto before=*store.state().project;
    rejects([&]{store.execute(SetAudioStretch{id,invalid});});CHECK(*store.state().project==before);
    auto text=serialize(stretched);const auto section=text.find("STRETCH_CLIPS 1");CHECK(section!=std::string::npos);
    const auto row=text.find('\n',section),end=text.find('\n',row+1);
    text.replace(section,15,"STRETCH_CLIPS 2");text.insert(end+1,text.substr(row+1,end-row));rejects([&]{(void)deserialize(text);});
}
std::shared_ptr<audio::AudioData> signal(){
    auto source=std::make_shared<audio::AudioData>();source->channels=2;
    source->samples.resize(48014*2);
    for(std::size_t i=0;i<48014;++i){const auto value=static_cast<float>(.4*std::sin(2*std::numbers::pi*220*static_cast<double>(i)/48000));source->samples[i*2]=value;source->samples[i*2+1]=-value;}
    return source;
}
void cache(){
    constexpr std::uintmax_t MiB=1024ull*1024;
    // Repro: a valid imported project's active PCM exceeds the former
    // 512 MiB aggregate cap, but is disk-backed and fits available space.
    audio::validate_stretch_disk_space(753*MiB,1024*MiB);
    rejects([&]{audio::validate_stretch_disk_space(753*MiB,800*MiB);});
    audio::validate_stretch_disk_space(0,0);
    audio::validate_stretch_disk_space(960*MiB,1024*MiB);
    auto source=signal();auto p=fixture();auto params=stretch::Parameters{};params.duration_ratio=1.25;params.semitones=7;
    const auto id=p.clips[0].id;SetAudioStretch{id,params}.apply(p);
    auto prepared=audio::prepare_stretch_cache(source,p.clips[0]);
    CHECK(prepared->data->frames()==60001&&prepared->data->file&&prepared->data->samples.empty());
    CHECK(prepared->waveform.frames()==60001&&prepared->source_current());
    const auto file=prepared->data->file->path;const auto pcm=audio::load_wav(file);
    double energy{},error{},maximum{};for(std::size_t i=0;i<pcm.samples.size();i+=2){CHECK(std::isfinite(pcm.samples[i]));const double e=pcm.samples[i]+pcm.samples[i+1];error+=e*e;maximum=std::max(maximum,std::abs(e));energy+=pcm.samples[i]*pcm.samples[i];}CHECK(energy>100);
    std::cout<<"Stereo relative RMS "<<std::sqrt(error/energy)<<" max "<<maximum<<'\n';CHECK(std::sqrt(error/energy)<.001);
    SplitAudioClip{id,30000,Id{"right"}}.apply(p);CHECK(audio::stretch_cache_key(*source,p.clips[0])==audio::stretch_cache_key(*source,p.clips[1]));
    auto again=audio::prepare_stretch_cache(source,p.clips[1]);CHECK(audio::load_wav(again->data->file->path).samples==pcm.samples);
    auto different=p.clips[0];different.stretch->parameters.cents=1;CHECK(audio::stretch_cache_key(*source,different)!=audio::stretch_cache_key(*source,p.clips[0]));
    different=p.clips[0];different.source_offset=12;CHECK(audio::stretch_cache_key(*source,different)!=audio::stretch_cache_key(*source,p.clips[0]));
    // Cancellation is tested during output as well as before any file exists.
    std::stop_source stop;stop.request_stop();rejects([&]{(void)audio::prepare_stretch_cache(source,p.clips[0],stop.get_token());});
    std::stop_source mid;rejects([&]{(void)audio::prepare_stretch_cache(source,p.clips[0],mid.get_token(),[&](float){mid.request_stop();});});
    auto held=prepared->data;prepared.reset();CHECK(fs::exists(file));held.reset();const auto retiredBy=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(fs::exists(file.parent_path())&&std::chrono::steady_clock::now()<retiredBy)std::this_thread::sleep_for(std::chrono::milliseconds(1));CHECK(!fs::exists(file.parent_path()));
    Directory dir;const auto original=dir.path/"source.wav";fs::copy_file(again->data->file->path,original);
    auto streamed=std::make_shared<audio::AudioData>();streamed->channels=2;streamed->file=std::make_shared<audio::WavFile>(audio::inspect_wav(original));
    auto fromFile=audio::prepare_stretch_cache(streamed,p.clips[0]);CHECK(fromFile->source_current());
    fs::last_write_time(original,fs::last_write_time(original)+std::chrono::seconds(2));CHECK(!fromFile->source_current());
    // RED on old session-only cache: reopening the same saved source must
    // reuse the same disk file, not invoke the render progress callback.
    const auto project=dir.path/"Persistent";fs::create_directory(project);
    const auto first=audio::prepare_stretch_cache(source,p.clips[0],{},{},stretch::Backend::signalsmith,project);
    const auto persistent=first->data->file->path;
    CHECK(persistent.parent_path()==project/"Cache"/"TimePitch");
    const auto written=fs::last_write_time(persistent);
    auto reopened=std::make_shared<audio::AudioData>(*source);
    bool rendered=false;
    const auto hit=audio::prepare_stretch_cache(reopened,p.clips[1],{},[&](float value){if(value<1)rendered=true;},stretch::Backend::signalsmith,project);
    CHECK(!rendered&&hit->data->file->path==persistent&&fs::last_write_time(persistent)==written);
    CHECK(audio::load_wav(persistent).samples==pcm.samples);
    auto edited=p.clips[0];edited.stretch->parameters.cents+=1;
    const auto changed=audio::prepare_stretch_cache(reopened,edited,{},{},stretch::Backend::signalsmith,project);
    CHECK(changed->data->file->path!=persistent);
    reopened->samples[0]+=0.1f;
    const auto newSource=audio::prepare_stretch_cache(reopened,p.clips[0],{},{},stretch::Backend::signalsmith,project);
    CHECK(newSource->data->file->path!=persistent);
    fs::resize_file(persistent,12);rendered=false;
    const auto repaired=audio::prepare_stretch_cache(source,p.clips[0],{},[&](float value){if(value<1)rendered=true;},stretch::Backend::signalsmith,project);
    CHECK(rendered&&repaired->data->file->path==persistent&&audio::load_wav(persistent).samples==pcm.samples);
    CHECK(fs::is_regular_file(persistent));
}
class Device final:public audio::IAudioDevice{
    audio::DevicePhase phase{audio::DevicePhase::closed};
public:
    std::vector<audio::DeviceInfo> enumerate()override{return {{0,"Manual stretch test",{}, {"L","R"},32,2048,128,-1}};}
    void control_panel(int)override{}
    void open(const audio::DeviceConfig&,std::shared_ptr<audio::AudioEngine>)override{phase=audio::DevicePhase::open;}
    void start()override{phase=audio::DevicePhase::running;}void stop()override{phase=audio::DevicePhase::stopped;}
    void close()noexcept override{phase=audio::DevicePhase::closed;}
    audio::DeviceStatus status()override{return {phase,48000,0,0,0,{}};}
};
void finish(desktop::Application& app){
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    do{
        try{app.poll();if(!app.stretch_status().busy)return;}
        catch(const std::runtime_error& e){if(std::string_view(e.what())!="audio state busy; poll again")throw;}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }while(std::chrono::steady_clock::now()<end);
    throw std::runtime_error("Cache preparation timed out");
}
void desktop_test(){
    CHECK(desktop::Application::stretch_worker_limit(0,16)==4);
    CHECK(desktop::Application::stretch_worker_limit(0,4)==2);
    CHECK(desktop::Application::stretch_worker_limit(0,0)==1);
    CHECK(desktop::Application::stretch_worker_limit(16,8)==8);
    CHECK(desktop::Application::stretch_worker_limit(1,16)==1);
    Directory dir;auto source=signal();auto p=fixture();stretch::Parameters params;params.duration_ratio=1.25;
    SetAudioStretch{p.clips[0].id,params}.apply(p);auto seed=audio::prepare_stretch_cache(source,p.clips[0]);
    const auto wav=dir.path/"source.wav";fs::copy_file(seed->data->file->path,wav);
    desktop::Application app;app.import_wav(wav);app.connect(std::make_unique<Device>(),{0,48000,128,{}, {0,1}});
    const auto id=app.services().projects->state().project->clips[0].id;
    app.set_clip_stretch(id,params);CHECK(app.stretch_status().busy);rejects([&]{app.play();});finish(app);
    auto clip=app.services().projects->state().project->clips[0];CHECK(clip.length==75001&&app.clip_waveform(clip));
    CHECK(app.clip_waveform(clip)->frames()==clip.length);
    app.play();std::array<float,256> out{};double energy{};
    for(int block=0;block<50;++block){app.engine()->process(nullptr,out.data(),128);for(float value:out){CHECK(std::isfinite(value));energy+=value*value;}std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    CHECK(energy>1);rejects([&]{app.set_clip_stretch(id,params);});app.pause();app.engine()->process(nullptr,out.data(),128);app.poll();
    const auto seek=[&](Sample position){app.seek(position);for(int i=0;i<100&&app.engine()->state().sample!=position;++i){app.engine()->process(nullptr,out.data(),128);std::this_thread::sleep_for(std::chrono::milliseconds(1));}CHECK(app.engine()->state().sample==position);};
    seek(20000);app.services().transport->set_loop(LoopRange{20000,20256});app.engine()->process(nullptr,out.data(),128);app.play();
    app.engine()->process(nullptr,out.data(),128);const auto firstLoop=out;
    app.engine()->process(nullptr,out.data(),128);app.engine()->process(nullptr,out.data(),128);
    for(std::size_t i=0;i<out.size();++i)CHECK(std::abs(out[i]-firstLoop[i])<.000001f);
    app.pause();app.engine()->process(nullptr,out.data(),128);app.services().transport->set_loop({});app.engine()->process(nullptr,out.data(),128);
    seek(clip.length-128);app.play();app.engine()->process(nullptr,out.data(),128);CHECK(app.engine()->state().sample==clip.length);
    app.engine()->process(nullptr,out.data(),128);for(float value:out)CHECK(value==0);app.pause();app.engine()->process(nullptr,out.data(),128);app.poll();
    auto bypass=params;bypass.enabled=false;app.set_clip_stretch(id,bypass);CHECK(app.services().projects->state().project->clips[0].length==60001);
    CHECK(app.undo());finish(app);CHECK(app.services().projects->state().project->clips[0].length==75001);
    CHECK(app.redo());finish(app);CHECK(app.services().projects->state().project->clips[0].length==60001);
    auto cold=params;cold.duration_ratio=1.37;app.set_clip_stretch(id,cold);app.cancel_stretch();CHECK(!app.stretch_status().busy);rejects([&]{app.play();});app.retry_stretch();finish(app);
    // A stale worker must not install its PCM in a replacement document.
    params.semitones=7;app.set_clip_stretch(id,params);app.new_project();finish(app);CHECK(app.services().projects->state().project->clips.empty());
    app.import_wav(wav);const auto savedId=app.services().projects->state().project->clips[0].id;
    app.save_project(dir.path/"Saved"/"project.mrsproject");
    app.set_clip_stretch(savedId,params);finish(app);app.save_project(app.path());
    const auto saved=app.snapshot().project;const auto path=app.path();
    const auto cacheDir=path.parent_path()/"Cache"/"TimePitch";
    std::map<fs::path,fs::file_time_type> entries;
    for(const auto& entry:fs::directory_iterator(cacheDir))if(entry.path().extension()==".wav")entries.emplace(entry.path(),fs::last_write_time(entry.path()));
    CHECK(entries.size()==1);
    app.open_project(path);finish(app);
    CHECK(app.snapshot().project==saved);CHECK(app.clip_waveform(app.services().projects->state().project->clips[0]));
    for(const auto& [file,time]:entries)CHECK(fs::is_regular_file(file)&&fs::last_write_time(file)==time);
    app.set_clip_stretch(savedId,params,true);CHECK(!app.services().projects->state().project->clips[0].stretch);

    // Repeat the real failure shape with 58 stretched / 19 neutral clips.
    // Shared source/parameters intentionally reuse one DSP cache; the graph
    // still needs one audible voice and reader per timeline clip.
    auto many=app.snapshot();many.project.clips.clear();
    const auto baseClip=saved.clips.front();
    for(int i=0;i<77;++i){
        auto c=baseClip;c.id=new_id();c.start=static_cast<Sample>(i)*baseClip.length;
        if(i<19){c.stretch.reset();c.length=1024;c.source_offset=0;}
        many.project.clips.push_back(std::move(c));
    }
    const auto manyPath=path.parent_path()/"many.mrsproject";
    persistence::save_project(manyPath,many);app.open_project(manyPath);
    app.connect(std::make_unique<Device>(),{0,48000,128,{}, {0,1}});
    finish(app);
    CHECK(app.stretch_status().message=="Time / pitch ready");
    CHECK(app.engine()->stopped_graph().voices.size()==77);
    for(const int i:{0,19,40,76}){
        const auto pos=many.project.clips[static_cast<std::size_t>(i)].start+256;
        app.seek(pos);app.engine()->process(nullptr,out.data(),128);
        app.play();double audible{};
        for(int block=0;block<4;++block){app.engine()->process(nullptr,out.data(),128);for(float x:out)audible+=x*x;}
        CHECK(audible>0.001);
        app.pause();app.engine()->process(nullptr,out.data(),128);app.poll();
    }
    // Reproduce publication failure without breaking a source/cache: 129
    // processed clips exceed the existing overall voice bound. Cache readiness
    // must not allow Play to silently run the partial graph.
    auto excessive=many;excessive.project.clips.clear();
    for(std::size_t i=0;i<=audio::max_voices;++i){
        auto c=baseClip;c.id=new_id();c.start=static_cast<Sample>(i)*baseClip.length;
        excessive.project.clips.push_back(std::move(c));
    }
    const auto excessivePath=path.parent_path()/"excessive.mrsproject";
    persistence::save_project(excessivePath,excessive);app.open_project(excessivePath);
    app.connect(std::make_unique<Device>(),{0,48000,128,{}, {0,1}});
    finish(app);
    CHECK(app.stretch_status().message.find("Time / pitch failed:") == 0);
    CHECK(app.clip_waveform(app.services().projects->state().project->clips.front()));
    rejects([&]{app.play();});
    app.retry_stretch();CHECK(app.stretch_status().message.find("Time / pitch failed:") == 0);
    rejects([&]{app.play();});

    // Multiple distinct source windows/parameters (no shared cache keys).
    // Render once per policy in separate roots and compare PCM bit-for-bit.
    auto batch=many;batch.project.clips.clear();
    for(int i=0;i<4;++i){
        auto c=baseClip;c.id=new_id();c.start=static_cast<Sample>(i)*baseClip.length;
        c.stretch->parameters.semitones+=i;
        batch.project.clips.push_back(std::move(c));
    }
    const auto runBatch=[&](unsigned workers){
        auto root=dir.path/("workers-"+std::to_string(workers));fs::create_directory(root);
        fs::copy(path.parent_path()/"Media",root/"Media",fs::copy_options::recursive);
        const auto file=root/"batch.mrsproject";
        persistence::save_project(file,batch);
        app.set_stretch_workers(workers);app.open_project(file);
        app.connect(std::make_unique<Device>(),{0,48000,128,{}, {0,1}});
        finish(app);CHECK(app.stretch_status().message=="Time / pitch ready");
        const auto graph=app.engine()->stopped_graph();CHECK(graph.voices.size()==4);
        std::vector<std::vector<float>> pcm;
        for(const auto& voice:graph.voices)pcm.push_back(audio::load_wav(voice.asset->file->path).samples);
        return pcm;
    };
    CHECK(runBatch(1)==runBatch(4));
    rejects([&]{app.set_stretch_workers(17);});
}
}
int main(int argc,char** argv){try{CHECK(argc==2);const std::string suite=argv[1];if(suite=="model")model();else if(suite=="cache")cache();else if(suite=="desktop")desktop_test();else throw std::runtime_error("Unknown suite");std::cout<<"PASS "<<suite<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
