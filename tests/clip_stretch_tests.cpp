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
    // Either edge keeps the opposite edge and source selection unchanged.
    auto edgeProject=stretched;const auto base=edgeProject.clips.front();
    auto right=stretch_audio_edge(base,base.start+base.length*2,false);
    CHECK(right.start==base.start&&right.length==base.length*2);
    CHECK(right.source_offset==base.source_offset&&right.stretch->window_begin==base.stretch->window_begin&&right.stretch->window_end==base.stretch->window_end);
    CHECK(right.stretch->parameters.semitones==7&&right.stretch->parameters.cents==-13);
    const auto left=stretch_audio_edge(right,right.start+right.length/2,true);
    CHECK(left.start+left.length==right.start+right.length&&left.length==right.length/2);
    const auto clamped=stretch_audio_edge(base,max_sample,false);CHECK(clamped.stretch->parameters.duration_ratio==8);
    auto leftAtZero=base;leftAtZero.start=0;const auto zero=stretch_audio_edge(leftAtZero,-10000,true);CHECK(zero.start==0&&zero.length==base.length);
    const auto tiny=stretch_audio_edge(base,base.start-100,false);CHECK(tiny.length>=1&&tiny.stretch->parameters.duration_ratio>=.125);
    ProjectStore timingStore(edgeProject);
    timingStore.execute(SetAudioClipTiming{base,left.stretch->parameters,105.,left.start});
    const auto timing=*timingStore.state().project;CHECK(timing.clips.front().start+timing.clips.front().length==right.start+right.length);
    CHECK(timing.clips.front().file_tempo==105.);CHECK(deserialize(serialize(timing))==timing);
    rejects([&]{timingStore.execute(SetAudioClipTiming{base,params,105.,base.start});});CHECK(*timingStore.state().project==timing);
    CHECK(timingStore.undo()&&*timingStore.state().project==edgeProject);CHECK(timingStore.redo()&&*timingStore.state().project==timing);
    auto badTempo=timing;badTempo.clips.front().file_tempo=std::numeric_limits<double>::quiet_NaN();rejects([&]{badTempo.validate();});
    badTempo.clips.front().file_tempo=0;rejects([&]{badTempo.validate();});
    edgeProject.time.tempos={{0,120},{ppq*4,90}};
    CHECK(project_tempo_at(edgeProject,0)==120);CHECK(project_tempo_at(edgeProject,Timeline(edgeProject.time,edgeProject.sample_rate).to_samples(ppq*4))==90);
    auto fit=params;fit.duration_ratio=105./120.;SetAudioClipTiming{base,fit,105.,base.start}.apply(edgeProject);
    CHECK(edgeProject.clips.front().stretch->parameters.duration_ratio==.875&&edgeProject.clips.front().stretch->parameters.semitones==7);
    // Trimming preserves a fractional source window through edge stretching.
    auto view=split.clips[1];p=split;TrimAudioClip{view.id,view.start+23,view.start+view.length-31,60000}.apply(p);view=p.clips[1];
    const auto viewRight=stretch_audio_edge(view,view.start+view.length*2,false);
    CHECK(viewRight.stretch->window_begin==view.stretch->window_begin&&viewRight.stretch->window_end==view.stretch->window_end);
    CHECK(viewRight.source_offset==view.source_offset&&viewRight.start==view.start);
    auto duplicated=serialize(timing);const auto tempoSection=duplicated.find("CLIP_TEMPOS 1");CHECK(tempoSection!=std::string::npos);
    const auto tempoRow=duplicated.find('\n',tempoSection),tempoEnd=duplicated.find('\n',tempoRow+1);
    duplicated.replace(tempoSection,13,"CLIP_TEMPOS 2");duplicated.insert(tempoEnd+1,duplicated.substr(tempoRow+1,tempoEnd-tempoRow));rejects([&]{(void)deserialize(duplicated);});
    auto legacy=serialize(original);legacy.replace(legacy.find("MRS_CORE_SNAPSHOT 32"),20,"MRS_CORE_SNAPSHOT 30");
    CHECK(deserialize(legacy)==original);
    auto v31=serialize(stretched);v31.replace(v31.find("MRS_CORE_SNAPSHOT 32"),20,"MRS_CORE_SNAPSHOT 31");CHECK(deserialize(v31)==stretched);
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
    auto held=prepared->data;prepared.reset();CHECK(fs::exists(file));held.reset();CHECK(!fs::exists(file.parent_path()));
    Directory dir;const auto original=dir.path/"source.wav";fs::copy_file(again->data->file->path,original);
    auto streamed=std::make_shared<audio::AudioData>();streamed->channels=2;streamed->file=std::make_shared<audio::WavFile>(audio::inspect_wav(original));
    auto fromFile=audio::prepare_stretch_cache(streamed,p.clips[0]);CHECK(fromFile->source_current());
    fs::last_write_time(original,fs::last_write_time(original)+std::chrono::seconds(2));CHECK(!fromFile->source_current());
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
    app.set_clip_stretch(id,params);app.cancel_stretch();CHECK(!app.stretch_status().busy);rejects([&]{app.play();});app.retry_stretch();finish(app);
    clip=app.services().projects->state().project->clips[0];
    const auto edgePreview=stretch_audio_edge(clip,clip.start+clip.length/2,false);
    app.set_clip_timing(clip,edgePreview.stretch->parameters,105.,edgePreview.start);finish(app);
    CHECK(app.services().projects->state().project->clips[0].length==edgePreview.length);
    CHECK(app.services().projects->state().project->clips[0].file_tempo==105.);
    const auto timed=app.snapshot().project;
    rejects([&]{app.set_clip_timing(clip,params,105.,clip.start);});CHECK(app.snapshot().project==timed);
    CHECK(app.undo());finish(app);CHECK(app.services().projects->state().project->clips[0]==clip);
    CHECK(app.redo());finish(app);CHECK(app.snapshot().project==timed);
    // A stale worker must not install its PCM in a replacement document.
    params.semitones=7;app.set_clip_stretch(id,params);app.new_project();finish(app);CHECK(app.services().projects->state().project->clips.empty());
    app.import_wav(wav);const auto savedId=app.services().projects->state().project->clips[0].id;
    const auto toSave=app.services().projects->state().project->clips[0];app.set_clip_timing(toSave,params,105.,toSave.start);finish(app);app.save_project(dir.path/"Saved"/"project.mrsproject");
    const auto saved=app.snapshot().project;const auto path=app.path();app.open_project(path);finish(app);
    CHECK(app.snapshot().project==saved);CHECK(app.clip_waveform(app.services().projects->state().project->clips[0]));
    for(const auto& entry:fs::recursive_directory_iterator(path.parent_path()))CHECK(entry.path().filename()!="audio.wav");
    app.set_clip_stretch(savedId,params,true);CHECK(!app.services().projects->state().project->clips[0].stretch);
}
}
int main(int argc,char** argv){try{CHECK(argc==2);const std::string suite=argv[1];if(suite=="model")model();else if(suite=="cache")cache();else if(suite=="desktop")desktop_test();else throw std::runtime_error("Unknown suite");std::cout<<"PASS "<<suite<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
