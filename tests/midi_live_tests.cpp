#include "legacy_midi_snapshot.hpp"
#include <mrs/desktop.hpp>
#include <mrs/vst3.hpp>
#include <mrs/ahead_renderer.hpp>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>
namespace {
thread_local bool probe{};std::atomic<unsigned> allocations{};
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),#__VA_ARGS__)
using namespace mrs;using namespace mrs::processing;using namespace mrs::audio;using namespace mrs::desktop;
struct Directory {std::filesystem::path path=std::filesystem::temp_directory_path()/("mrs-midi-"+new_id().value);Directory(){std::filesystem::create_directory(path);}~Directory(){std::error_code ec;std::filesystem::remove_all(path,ec);}};
std::string utf8(const std::filesystem::path& p){auto s=p.u8string();return {s.begin(),s.end()};}
struct ManualDevice final:IAudioDevice {
    DevicePhase phase{DevicePhase::closed};
    bool fail_start{};
    std::vector<DeviceInfo> enumerate()override{return {{0,"Manual MIDI fixture",{}, {"L","R"},16,8192,128,1}};}
    void control_panel(int)override{}
    void open(const DeviceConfig&,std::shared_ptr<AudioEngine>)override{phase=DevicePhase::open;}
    void start()override{if(fail_start){fail_start=false;throw std::runtime_error("fixture start failure");}phase=DevicePhase::running;}void stop()override{phase=DevicePhase::stopped;}void close()noexcept override{phase=DevicePhase::closed;}
    DeviceStatus status()override{return {phase,48000};}
};
void decode(){MidiEvent e;CHECK(decode_midi_short(0x643C92,e)&&e.kind==MidiKind::note_on&&e.channel==2&&e.data1==60&&e.data2==100);CHECK(decode_midi_short(0x003C92,e)&&e.kind==MidiKind::note_off);CHECK(decode_midi_short(0x0040B0,e)&&e.kind==MidiKind::cc);CHECK(decode_midi_short(0x407FE0,e)&&e.kind==MidiKind::pitch_bend);CHECK(!decode_midi_short(0xF8,e));}
void model(){auto p=demo_project();Track t{new_id(),"Keys",TrackKind::instrument,{}};p.tracks.push_back(t);ProjectStore store{p};store.execute(SetMidiInput{t.id,"missing-port",3,true});CHECK(store.state().project->tracks.back().midi_channel==3);CHECK(store.undo()&&store.state().project->tracks.back().midi_input.empty());CHECK(store.redo());CHECK(deserialize(serialize(*store.state().project))==*store.state().project);
    auto old=without_midi_outputs(serialize(demo_project()));const std::string clipMidi="MIDI 0\nMIDIEVENTS 0\n";for(auto at=old.find(clipMidi);at!=std::string::npos;at=old.find(clipMidi))old.erase(at,clipMidi.size());const std::string line="\"\" -1 1\n";for(auto at=old.find(line);at!=std::string::npos;at=old.find(line))old.erase(at,line.size());old.replace(0,std::string("MRS_CORE_SNAPSHOT 14").size(),"MRS_CORE_SNAPSHOT 10");CHECK(deserialize(old)==demo_project());}
void engine(const std::filesystem::path& fixture){
    Directory dir;const auto plugin=dir.path/"instrument.vst3";std::filesystem::copy_file(fixture,plugin);
    auto info=probe_vst3(utf8(plugin)).front();NativeInsert fx;fx.id=new_id();fx.kind=InsertKind::vst3;fx.plugin_path=info.path;fx.class_id=info.class_id;fx.plugin_name=info.name;
    auto state=std::make_shared<const GraphState>(insert_graph(std::array{fx}));
    auto synth=std::make_shared<PreparedGraph>(GraphSnapshot{state,0,false,false},ProcessConfig{48000,2,128,128,true},hosted_factory);
    CHECK(!synth->anticipation_safe());RenderGraph graph;graph.mixer.resize(1);graph.live_midi={true};graph.inserts={synth};AudioEngine engine;engine.prepare({48000,0,2,128,128,1},graph);
    CHECK(!engine.anticipation_safe());CHECK(engine.processing_domains().channels[0].owner==ProcessingDomains::Owner::device);
    std::array<float,256> output{};const auto generation=engine.midi_generation();
    CHECK(engine.enqueue_live_midi({generation,0,{0,MidiKind::note_on,0,60,127}}));allocations=0;probe=true;engine.process(nullptr,output.data(),128);probe=false;CHECK(allocations==0&&output.back()>0.49f); // fixed PDC 7 frames, also plays while stopped
    CHECK(engine.enqueue_live_midi({generation,0,{0,MidiKind::cc,0,64,127}}));engine.process(nullptr,output.data(),128);
    CHECK(engine.enqueue_live_midi({generation,0,{0,MidiKind::note_off,0,60,0}}));engine.process(nullptr,output.data(),128);CHECK(output.back()>0.49f);
    CHECK(engine.enqueue_live_midi({generation,0,{0,MidiKind::pitch_bend,0,127,127}}));engine.process(nullptr,output.data(),128);CHECK(output.back()>0.74f);
    for(int i=0;i<64;++i)CHECK(engine.enqueue_live_midi({generation,0,{0,MidiKind::cc,0,64,127}}));
    CHECK(engine.enqueue_live_midi({generation,0,{0,MidiKind::cc,0,64,0}}));engine.process(nullptr,output.data(),128);CHECK(output.back()==0); // controller burst retains final sustain release
    engine.midi_panic();engine.process(nullptr,output.data(),128);CHECK(output.back()==0);
    CHECK(engine.enqueue_live_midi({generation,0,{0,MidiKind::note_on,2,64,0}}));engine.process(nullptr,output.data(),128);CHECK(output.back()==0);
    CHECK(engine.enqueue_live_midi({generation,0,{0,MidiKind::note_on,0,60,127}}));engine.process(nullptr,output.data(),128);CHECK(output.back()>0);
    MixerUpdate monitor;monitor.count=1;monitor.tracks[0]=graph.mixer[0];monitor.midi_monitor[0]=false;
    CHECK(engine.enqueue_mix(monitor));probe=true;engine.process(nullptr,output.data(),128);engine.process(nullptr,output.data(),128);probe=false;
    CHECK(allocations==0&&output.back()==0&&engine.midi_generation()==generation); // held note released; no reprepare
    CHECK(engine.enqueue_live_midi({generation,0,{0,MidiKind::note_on,0,60,127}}));engine.process(nullptr,output.data(),128);CHECK(output.back()==0);
    monitor.midi_monitor[0]=true;CHECK(engine.enqueue_mix(monitor));engine.process(nullptr,output.data(),128);
    CHECK(engine.enqueue_live_midi({generation,0,{0,MidiKind::note_on,0,60,127}}));engine.process(nullptr,output.data(),128);CHECK(output.back()>0);
    CHECK(engine.enqueue({ControlKind::stop}));engine.process(nullptr,output.data(),128);CHECK(output.back()==0);
    for(int i=0;i<1100;++i)(void)engine.enqueue_live_midi({generation,0,{0,MidiKind::note_on,0,60,127}});
    probe=true;engine.process(nullptr,output.data(),128);probe=false;CHECK(allocations==0&&engine.midi_dropped()>0&&output.back()==0);
    engine.prepare({48000,0,2,128,128,1},graph);CHECK(engine.enqueue_live_midi({generation,0,{0,MidiKind::note_on,0,60,127}}));engine.process(nullptr,output.data(),128);CHECK(output.back()==0); // stale generation rejected
    for(auto frames:{17U,64U,128U}){CHECK(engine.enqueue_live_midi({engine.midi_generation(),0,{0,MidiKind::note_on,0,60,100}}));probe=true;engine.process(nullptr,output.data(),frames);probe=false;CHECK(allocations==0&&output[frames*2-1]>0);engine.midi_panic();engine.process(nullptr,output.data(),frames);}
    engine.prepare({48000,0,2,128},{});
    RenderGraph routed;routed.mixer.resize(2);routed.mixer[0].gain=.5f;routed.mixer[1].gain=.5f;routed.outputs={1,no_mixer_track};routed.buses={false,true};routed.live_midi={true,false};routed.inserts={synth,{}};
    engine.prepare({48000,0,2,128,128,1},routed);engine.midi_panic();engine.process(nullptr,output.data(),128);engine.enqueue_live_midi({engine.midi_generation(),0,{0,MidiKind::note_on,0,60,127}});engine.process(nullptr,output.data(),128);CHECK(std::abs(output.back()-.125f)<.0001f);engine.midi_panic();engine.process(nullptr,output.data(),128);CHECK(output.back()==0);engine.prepare({48000,0,2,128},{});routed.inserts.clear();
    for(auto workers:{2U,4U})for(auto window:{1024U,4096U}){
        auto shared=std::make_shared<AudioEngine>();auto mixed=graph;mixed.mixer.resize(2);mixed.live_midi={true,false};mixed.inserts={synth,{}};
        auto asset=std::make_shared<AudioData>(sine_fixture(48000,2,1024,440));std::fill(asset->samples.begin(),asset->samples.end(),0.f);
        mixed.voices={{asset,0,0,1024,{{0,0,1},{1,1,1}},{},1}};
        shared->prepare({48000,0,2,128,128,workers},mixed);AheadRenderer renderer(shared,128,window);renderer.start();CHECK(shared->metrics().mixed_anticipation&&!shared->metrics().processing_fault);
        shared->midi_panic();renderer.process(nullptr,output.data(),128);CHECK(shared->enqueue_live_midi({shared->midi_generation(),0,{0,MidiKind::note_on,0,60,127}}));
        probe=true;renderer.process(nullptr,output.data(),128);probe=false;CHECK(allocations==0&&output.back()>.49f);
        CHECK(shared->enqueue({ControlKind::play}));renderer.process(nullptr,output.data(),128);CHECK(output.back()>.49f);
        shared->midi_panic();renderer.process(nullptr,output.data(),128);CHECK(output.back()==0&&!shared->metrics().processing_fault);renderer.stop();
    }
    Application app;app.new_project();auto track=app.add_instrument_track("Keys");app.set_inserts(track,{fx});app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1},1});
    app.set_midi_input(track,"missing-port",2,true);CHECK(app.services().projects->state().project->tracks.back().midi_channel==2);CHECK(app.undo());CHECK(app.redo());
    const auto insertGeneration=app.insert_generation(),midiGeneration=app.engine()->midi_generation();
    app.set_midi_input(track,"missing-port",2,false);app.set_track_armed(track,true);
    CHECK(app.services().projects->state().project->tracks.back().midi_monitor&&app.track_armed(track));
    app.set_track_armed(track,false);CHECK(app.services().projects->state().project->tracks.back().midi_monitor);
    app.set_midi_input(track,"missing-port",2,false);CHECK(app.undo()&&app.services().projects->state().project->tracks.back().midi_monitor);
    CHECK(app.redo()&&!app.services().projects->state().project->tracks.back().midi_monitor);
    CHECK(app.insert_generation()==insertGeneration&&app.engine()->midi_generation()==midiGeneration);
    app.set_midi_input(track,"missing-port",2,true);
    {
        const auto audio=app.add_audio_track("Arm transition");app.set_track_input(audio,-1);
        const auto beforeOrder=*app.services().projects->state().project;const auto orderGeneration=app.insert_generation();
        app.set_track_armed(track,true);const auto armed=app.track_armed(track);app.reorder_track(track,1);
        CHECK(app.insert_generation()==orderGeneration&&app.track_armed(track)==armed&&app.history_preserves_plugins(false));
        CHECK(app.services().projects->state().project->tracks.back()==beforeOrder.tracks.front());
        CHECK(app.engine()->enqueue_live_midi({app.engine()->midi_generation(),1,{0,MidiKind::note_on,0,60,127}}));app.engine()->process(nullptr,output.data(),128);CHECK(output.back()>0);
        CHECK(app.undo()&&app.insert_generation()==orderGeneration&&app.track_armed(track)==armed);app.engine()->process(nullptr,output.data(),128);CHECK(output.back()==0);
        CHECK(app.redo()&&app.insert_generation()==orderGeneration);CHECK(app.undo());app.set_track_armed(track,false);
        CHECK(app.engine()->enqueue_live_midi({app.engine()->midi_generation(),0,{0,MidiKind::note_on,0,60,127}}));
        app.engine()->process(nullptr,output.data(),128);CHECK(output.back()>0);
        const auto retainedGeneration=app.insert_generation();app.set_track_armed(audio,true);
        app.engine()->process(nullptr,output.data(),128);CHECK(output.back()==0&&app.insert_generation()==retainedGeneration);
        app.set_track_armed(audio,false);app.remove_track(audio);
    }
    const auto project=dir.path/"keys.mrsproject";app.save_project(project);app.open_project(project);const auto saved=app.services().projects->state().project->tracks.back();CHECK(saved.kind==TrackKind::instrument&&saved.midi_input=="missing-port"&&saved.inserts.front().class_id==fx.class_id&&!saved.inserts.front().component_state.empty());
    app.poll();for(int attempt=0;attempt<200&&!app.midi_status(track).starts_with("MIDI: missing");++attempt)std::this_thread::sleep_for(std::chrono::milliseconds(10));if(!app.midi_status(track).starts_with("MIDI: missing"))throw std::runtime_error(app.midi_status(track));app.disconnect();
    engine.prepare({48000,0,2,128},{});graph.inserts.clear();synth.reset();std::filesystem::remove(plugin);app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1},1});CHECK(app.midi_status(track).find("Instrument unavailable:")==0);CHECK(app.services().projects->state().project->tracks.back().inserts.front().class_id==fx.class_id);
}
void create_instrument(const std::filesystem::path& fixture){
    Directory dir;const auto module=dir.path/"instrument.vst3";std::filesystem::copy_file(fixture,module);
    const auto midiEffect=dir.path/"midi-effect-instrument.vst3",noMidi=dir.path/"instrument-no-midi.vst3";std::filesystem::copy_file(fixture,midiEffect);std::filesystem::copy_file(fixture,noMidi);
    const auto info=probe_vst3(utf8(module)).front();NativeInsert fx;fx.id=new_id();fx.kind=InsertKind::vst3;fx.plugin_path=info.path;fx.class_id=info.class_id;fx.plugin_name=info.name;
    for(bool connected:{false,true}){
        Application app;app.new_project();const auto existing=app.add_audio_track("Existing");
        ManualDevice* device{};if(connected){auto backend=std::make_unique<ManualDevice>();device=backend.get();app.connect(std::move(backend),{0,48000,128,{}, {0,1},2});}
        const auto before=*app.services().projects->state().project;const auto revision=app.services().projects->state().revision;
        const auto id=app.add_instrument_track("Dropped keys",fx);const auto after=*app.services().projects->state().project;
        CHECK(after.tracks.size()==before.tracks.size()+1&&after.tracks.back().id==id&&after.tracks.back().kind==TrackKind::instrument&&after.tracks.back().inserts==std::vector<NativeInsert>{fx});
        CHECK(app.services().projects->state().revision==revision+1&&app.undo()&&*app.services().projects->state().project==before);
        // Failed create preserves the existing Redo and the running graph.
        for(int failure:{0,1,2,3,4}){auto bad=fx;if(failure==0)bad.plugin_path=utf8(dir.path/"missing.vst3");else if(failure==1)bad.class_id=std::string(32,'0');else if(failure==2)bad.plugin_path=utf8(fixture);else bad.plugin_path=utf8(failure==3?midiEffect:noMidi);
            const auto state=app.services().projects->state();bool rejected=false;try{app.add_instrument_track("Invalid",bad);}catch(const std::exception&){rejected=true;}
            CHECK(rejected&&*app.services().projects->state().project==before&&app.services().projects->state().revision==state.revision&&app.services().projects->state().can_redo);
            CHECK(!connected||app.audio_running());
        }
        if(device){device->fail_start=true;const auto state=app.services().projects->state();bool rejected=false;try{app.add_instrument_track("Restart failure",fx);}catch(const std::exception&){rejected=true;}CHECK(rejected&&*app.services().projects->state().project==before&&app.services().projects->state().revision==state.revision&&app.services().projects->state().can_redo&&app.audio_running());}
        CHECK(app.redo()&&*app.services().projects->state().project==after);
        if(connected){std::array<float,256> out{};app.engine()->process(nullptr,out.data(),128);CHECK(app.audition_note(id,60,127));for(int n=0;n<4;++n)app.engine()->process(nullptr,out.data(),128);CHECK(std::abs(out.back()-.25f)<.00001f); // fixture .5 through the existing foundation graph gain .5
            app.midi_panic();app.engine()->process(nullptr,out.data(),128);app.play();app.engine()->process(nullptr,out.data(),128);const auto state=app.services().projects->state();bool rejected=false;try{app.add_instrument_track("During playback",fx);}catch(const std::exception&){rejected=true;}CHECK(rejected&&app.services().projects->state().revision==state.revision);app.stop();app.engine()->process(nullptr,out.data(),128);}
        const auto file=dir.path/(connected?"connected.mrsproject":"disconnected.mrsproject");app.save_project(file);app.open_project(file);
        const auto restored=app.services().projects->state().project->tracks.back();CHECK(restored.id==id&&restored.inserts.front().class_id==fx.class_id&&restored.kind==TrackKind::instrument);
        CHECK(app.services().projects->state().project->tracks.front().id==existing);app.disconnect();
    }
}
}
void* operator new(std::size_t n){if(probe)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc{};}void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
int main(int argc,char** argv){try{CHECK(argc==2);decode();model();create_instrument(std::filesystem::path(argv[1]));engine(std::filesystem::path(argv[1]));std::cout<<"PASS MIDI live\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
