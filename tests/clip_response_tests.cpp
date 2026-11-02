#include <mrs/desktop.hpp>
#include <mrs/arrangement.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>
#include <cmath>
#ifdef _MSC_VER
#include <malloc.h>
#endif
namespace {
thread_local bool probing{};
std::atomic<std::size_t> allocations{},deallocations{};
}
void* operator new(std::size_t n) {
    if(probing) ++allocations;
    if(auto* p=std::malloc(n?n:1)) return p;
    throw std::bad_alloc{};
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { if(probing && p) ++deallocations; std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p,std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }
void* operator new(std::size_t n,std::align_val_t a) {
    if(probing) ++allocations;
    void* p{};
#ifdef _MSC_VER
    p=_aligned_malloc(n?n:1,static_cast<std::size_t>(a));
#else
    if(posix_memalign(&p,static_cast<std::size_t>(a),n?n:1)!=0) p=nullptr;
#endif
    if(!p) throw std::bad_alloc{};
    return p;
}
void* operator new[](std::size_t n,std::align_val_t a) { return ::operator new(n,a); }
void operator delete(void* p,std::align_val_t) noexcept {
    if(probing && p) ++deallocations;
#ifdef _MSC_VER
    _aligned_free(p);
#else
    std::free(p);
#endif
}
void operator delete[](void* p,std::align_val_t a) noexcept { ::operator delete(p,a); }
void operator delete(void* p,std::size_t,std::align_val_t a) noexcept { ::operator delete(p,a); }
void operator delete[](void* p,std::size_t,std::align_val_t a) noexcept { ::operator delete(p,a); }
namespace {
using namespace mrs;using namespace mrs::desktop;
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct Counts{unsigned stops{},starts{};bool fail{};};
class Device final:public audio::IAudioDevice{
    Counts& n;audio::DevicePhase phase{audio::DevicePhase::closed};
public:explicit Device(Counts& counts):n(counts){}
    std::vector<audio::DeviceInfo> enumerate()override{return {{0,"Clip response fixture",{}, {"L","R"},32,2048,128,-1}};}
    void control_panel(int)override{}
    void open(const audio::DeviceConfig&,std::shared_ptr<audio::AudioEngine>)override{phase=audio::DevicePhase::open;}
    void start()override{if(n.fail){n.fail=false;throw std::runtime_error("Injected restart failure");}++n.starts;phase=audio::DevicePhase::running;}
    void stop()override{++n.stops;phase=audio::DevicePhase::stopped;}
    void close()noexcept override{phase=audio::DevicePhase::closed;}
    audio::DeviceStatus status()override{return {phase,48000,0,0,0,{}};}
};
void callback(audio::AudioEngine& engine,std::span<float> out){
    allocations=0;deallocations=0;probing=true;engine.process(nullptr,out.data(),128);probing=false;
    check(allocations.load()==0&&deallocations.load()==0,"callback allocated/freed while adopting clip voices");
}
void shared_state(){
    NativeInsert fx;fx.id=new_id();fx.kind=InsertKind::vst3;fx.plugin_path="synthetic-not-a-plugin";fx.class_id=std::string(32,'0');fx.component_state.assign(8*1024*1024,std::byte{42});
    auto copied=fx;check(copied.component_state.data()==fx.component_state.data(),"opaque state copy must share immutable storage");
    copied.component_state.assign(17,std::byte{7});check(fx.component_state.size()==8*1024*1024&&fx.component_state[0]==std::byte{42},"state mutation changed original snapshot");
    auto project=demo_project();project.tracks.front().inserts={fx};ProjectStore store(project);
    auto mix=project.tracks.front().mix;mix.gain=.5f;store.execute(SetTrackMix{project.tracks.front().id,mix});
    check(store.state().project->tracks.front().inserts.front().component_state.data()==fx.component_state.data(),"scalar command deep-copied plugin state");
    check(store.undo()&&store.state().project->tracks.front().mix.gain==1,"scalar undo corrupted snapshot");
    copied.component_state.assign(32,std::byte{11});project.tracks.front().inserts={copied};
    check(deserialize(serialize(project))==project,"shared opaque state serialization changed format");
}
void voice_publication(){
    audio::AudioEngine engine;audio::RenderGraph graph;graph.mixer.resize(1);graph.outputs={audio::no_mixer_track};
    auto source=std::make_shared<const audio::AudioData>(audio::AudioData{48000,1,std::vector<float>(512,.25f)});
    std::weak_ptr<const audio::AudioData> old=source;
    audio::Voice voice{source,0,0,512,{{0,0,1}}};voice.mixer_track=0;graph.voices={voice};voice.asset.reset();source.reset();
    engine.prepare({48000,0,1,128},std::move(graph));
    auto nextSource=std::make_shared<const audio::AudioData>(audio::AudioData{48000,1,std::vector<float>(512,.5f)});
    audio::Voice next{nextSource,0,0,512,{{0,0,1}}};next.mixer_track=0;
    auto plan=engine.prepare_voice_transition({next});check(engine.stage_transition(plan),"stage clip voices");
    check(engine.enqueue({audio::ControlKind::play}),"queue Play after clip edit");
    std::array<float,128> out{};callback(engine,out);
    check(engine.transition_phase(plan)==audio::AudioEngine::TransitionPhase::adopted,"callback did not adopt voices");
    check(std::abs(out[64]-.5f)<1e-6,"queued Play heard stale PCM");check(!old.expired(),"outgoing storage freed before control acknowledgement");
    check(engine.acknowledge_transition(plan),"ack clip publication");plan.reset();check(old.expired(),"outgoing storage not retired on control");
    check(engine.enqueue({audio::ControlKind::pause}),"pause");callback(engine,out);
    for(int i=0;i<100;++i){auto cancelled=engine.prepare_voice_transition({next});check(engine.stage_transition(cancelled),"stage cancelled plan");check(engine.cancel_transition(cancelled),"cancel pending voices");cancelled.reset();callback(engine,out);}
}
void publication_race(){
    audio::AudioEngine engine;audio::RenderGraph graph;graph.mixer.resize(1);graph.outputs={audio::no_mixer_track};engine.prepare({48000,0,1,128},std::move(graph));
    auto source=std::make_shared<const audio::AudioData>(audio::AudioData{48000,1,std::vector<float>(512,.25f)});
    audio::Voice voice{source,0,0,512,{{0,0,1}}};voice.mixer_track=0;
    std::atomic<bool> quit{},fault{};
    std::jthread consumer([&](std::stop_token stop){std::array<float,128> out{};while(!quit.load()&&!stop.stop_requested()){allocations=0;deallocations=0;probing=true;engine.process(nullptr,out.data(),128);probing=false;if(allocations.load()||deallocations.load())fault=true;}});
    bool success=true;
    for(int i=0;i<1000&&success;++i){auto plan=engine.prepare_voice_transition(i%2?std::vector<audio::Voice>{voice}:std::vector<audio::Voice>{});
        if(!engine.stage_transition(plan)){success=false;break;}
        if(engine.cancel_transition(plan))continue;
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        for(;;){const auto phase=engine.transition_phase(plan);if(phase==audio::AudioEngine::TransitionPhase::adopted){success=engine.acknowledge_transition(plan);break;}
            if(phase==audio::AudioEngine::TransitionPhase::cancelled)break;
            if(std::chrono::steady_clock::now()>until){success=false;break;}std::this_thread::yield();}
        if(!success){quit=true;consumer.join();engine.prepare({48000,0,1,128},{});break;}
    }
    quit=true;if(consumer.joinable())consumer.join();check(success&&!fault.load(),"concurrent publication/cancel ownership or RT allocation failure");
}
void wav(const std::filesystem::path& file){std::ofstream f(file,std::ios::binary);auto put=[&](unsigned n,int bytes){for(int i=0;i<bytes;++i)f.put(static_cast<char>((n>>(8*i))&255));};
    f.write("RIFF",4);put(36+48000*4,4);f.write("WAVEfmt ",8);put(16,4);put(1,2);put(2,2);put(48000,4);put(192000,4);put(4,2);put(16,2);f.write("data",4);put(48000*4,4);
    for(unsigned i=0;i<48000;++i){put(8192,2);put(8192,2);}
}
void application_edits(const std::filesystem::path& file,bool buffered){
    Counts counts;Application app;app.new_project();app.import_wav(file);
    const auto id=app.services().projects->state().project->clips.front().id,track=app.services().projects->state().project->clips.front().track;
    NativeInsert gain;gain.id=new_id();gain.kind=InsertKind::gain;gain.gain=.75f;app.set_inserts(track,{gain});
    audio::DeviceConfig config{0,48000,128,{}, {0,1}};if(buffered){config.processing_workers=2;config.process_buffer_frames=2048;}
    app.connect(std::make_unique<Device>(counts),config);std::array<float,256> out{};
    const auto original=app.engine()->stopped_graph().inserts.front();const auto generation=app.insert_generation();const auto stops=counts.stops;
    app.remove_clips({id});callback(*app.engine(),out);app.poll();check(app.engine()->stopped_graph().voices.empty(),"Delete left stale audio voice");
    check(app.history_preserves_plugins(false),"clip history closes unrelated plugins");check(app.undo(),"Undo Delete");callback(*app.engine(),out);app.poll();
    check(app.engine()->stopped_graph().voices.size()==1,"Undo lost audio voice");
    app.move_clip(id,track,128);callback(*app.engine(),out);app.poll();check(app.undo(),"Undo move");callback(*app.engine(),out);app.poll();
    app.trim_clip(id,0,24000);callback(*app.engine(),out);app.poll();check(app.undo(),"Undo trim");callback(*app.engine(),out);app.poll();
    (void)app.split_clip(id,24000);callback(*app.engine(),out);app.poll();check(app.engine()->stopped_graph().voices.size()==2,"Split voice mapping");check(app.undo(),"Undo split");callback(*app.engine(),out);app.poll();
    app.remove_clip(id);check(app.undo(),"rapid undo before callback");callback(*app.engine(),out);app.poll();check(app.engine()->stopped_graph().voices.size()==1,"rapid edit published stale voices");
    check(app.insert_generation()==generation&&app.engine()->stopped_graph().inserts.front()==original,"clip edit replaced/reset plugin instance");
    if(!buffered)check(counts.stops==stops,"ordinary clip edit stopped ASIO/device");
    else{
        const auto before=*app.services().projects->state().project;counts.fail=true;bool rejected=false;try{app.remove_clip(id);}catch(const std::exception&){rejected=true;}
        check(rejected&&*app.services().projects->state().project==before,"failed restart did not roll back project/history");
        check(app.engine()->stopped_graph().voices.size()==1&&app.engine()->stopped_graph().inserts.front()==original,"failed restart did not restore PCM/DSP");
    }
    app.play();callback(*app.engine(),out);check(std::abs(out[128])>.01,"edited playback silent");app.pause();callback(*app.engine(),out);app.poll();
    (void)app.add_audio_track("Unrelated empty channel");check(app.engine()->stopped_graph().inserts.front()==original,"adding empty channel recreated unrelated plugin");check(app.undo(),"Undo add channel");check(app.engine()->stopped_graph().inserts.front()==original,"channel history recreated unrelated plugin");
}
}
int main(){try{shared_state();voice_publication();publication_race();auto directory=std::filesystem::temp_directory_path()/("mrs-clip-response-"+mrs::new_id().value);std::filesystem::create_directory(directory);
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code e;std::filesystem::remove_all(path,e);}}cleanup{directory};
    auto file=directory/"fixture.wav";wav(file);application_edits(file,false);application_edits(file,true);
    std::cout<<"PASS immutable opaque snapshots, value serialization, callback no alloc/free, PCM handoff/cancel, clip edits/Undo, plugin retention, queued Play, buffered restart rollback\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
