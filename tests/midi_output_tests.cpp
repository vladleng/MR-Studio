#include <mrs/desktop.hpp>
#include <algorithm>
#include <mrs/midi_recording.hpp>
#include <mrs/mixed_renderer.hpp>
#include "legacy_midi_snapshot.hpp"
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <new>
#include <thread>
using namespace mrs;using namespace mrs::audio;using namespace mrs::desktop;using namespace mrs::processing;
namespace {
thread_local bool probe{};std::atomic<unsigned> allocations{};
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),#__VA_ARGS__)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
template<class F>void until(F f){for(int i=0;i<1500;++i){if(f())return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}CHECK(f());}
struct State {std::mutex mutex;std::vector<std::uint32_t> messages;std::atomic<bool> present{true},busy{},sendFail{};std::atomic<unsigned> resets{},closes{},opens{};std::thread::id sender;};
struct Backend final:IMidiOutputBackend {
    std::shared_ptr<State> state;explicit Backend(std::shared_ptr<State> s):state(std::move(s)){}
    std::vector<MidiOutputPort> ports()override{return state->present?std::vector<MidiOutputPort>{{"fixture","Fixture output",0}}:std::vector<MidiOutputPort>{};}
    bool open(const MidiOutputPort&)override{++state->opens;return !state->busy;}
    bool send(const std::string&,std::uint32_t raw)override{if(state->sendFail)return false;std::lock_guard lock(state->mutex);state->sender=std::this_thread::get_id();state->messages.push_back(raw);return true;}
    void panic(const std::string&)noexcept override{++state->resets;}
    void close()noexcept override{++state->closes;}
};
struct Manual final:IAudioDevice {
    std::vector<DeviceInfo> enumerate()override{return {{0,"External MIDI fixture",{}, {"L","R"},16,8192,128,1}};}
    void control_panel(int)override{}void open(const DeviceConfig&,std::shared_ptr<AudioEngine>)override{}
    void start()override{}void stop()override{}void close()noexcept override{}DeviceStatus status()override{return {DevicePhase::running,48000};}
};
std::size_t messages(const std::shared_ptr<State>& s){std::lock_guard lock(s->mutex);return s->messages.size();}
bool received(const std::shared_ptr<State>& s,std::uint32_t raw){std::lock_guard lock(s->mutex);return std::find(s->messages.begin(),s->messages.end(),raw)!=s->messages.end();}
void model(){Project p;p.id=new_id();Track t;t.id=new_id();t.kind=TrackKind::instrument;t.name="External";p.tracks={t};ProjectStore store(p);
    store.execute(SetMidiOutput{t.id,"fixture",3});const auto after=*store.state().project;CHECK(after.tracks[0].midi_output=="fixture");CHECK(deserialize(serialize(after))==after);CHECK(store.undo()&&*store.state().project==p);CHECK(store.redo()&&*store.state().project==after);
    const auto revision=store.state().revision;rejects([&]{store.execute(SetMidiInput{t.id,"fixture",-1,true});});rejects([&]{store.execute(SetMidiOutput{t.id,"fixture",16});});CHECK(store.state().revision==revision);
    auto legacy=without_midi_outputs(serialize(p));legacy.replace(0,std::string("MRS_CORE_SNAPSHOT 14").size(),"MRS_CORE_SNAPSHOT 13");CHECK(deserialize(legacy)==p);
    for(int kind=0;kind<=6;++kind){MidiEvent e{0,static_cast<MidiKind>(kind),7,60,100},decoded;CHECK(decode_midi_short(encode_midi_short(e),decoded));CHECK(decoded.kind==e.kind&&decoded.channel==7&&decoded.data1==60);CHECK(decode_midi_short(encode_midi_short(e,2),decoded)&&decoded.channel==2);}
    CHECK(!encode_midi_short({0,MidiKind::note_on,16,60,100}));
}
void engine(){for(auto rate:{44100U,48000U,96000U})for(auto workers:{1U,4U}){
    auto q=std::make_shared<ExternalMidiQueue>();AudioEngine e;RenderGraph g;g.mixer.resize(1);g.live_midi={true};g.external_midi=q;g.external_midi_tracks={true};g.external_midi_channels={3};g.midi_notes={{{17,70,60,100,0},{30,90,60,100,1}}};g.midi_events={{{20,{0,MidiKind::cc,0,1,99}}}};
    e.prepare({rate,0,2,128,128,workers},g);CHECK(!e.anticipation_safe());e.enqueue({ControlKind::play});std::array<float,256> out{};allocations=0;probe=true;e.process(nullptr,out.data(),128);probe=false;CHECK(allocations==0);
    ExternalMidiQueue::Packet p;std::vector<ExternalMidiQueue::Packet> packets;while(q->pop(p))packets.push_back(p);
    CHECK(std::count_if(packets.begin(),packets.end(),[](const auto& p){return p.event.kind==MidiKind::note_on;})==1);CHECK(std::count_if(packets.begin(),packets.end(),[](const auto& p){return p.event.kind==MidiKind::note_off;})==1);
    CHECK(std::all_of(packets.begin(),packets.end(),[&](const auto& p){return p.event.channel==3&&p.generation==e.midi_generation();}));
    e.enqueue({ControlKind::pause});e.process(nullptr,out.data(),128);while(q->pop(p)){}
    CHECK(e.enqueue_live_midi({e.midi_generation(),0,{0,MidiKind::note_on,2,65,90}}));e.process(nullptr,out.data(),128);bool thru=false;while(q->pop(p))thru|=p.event.kind==MidiKind::note_on&&p.event.data1==65&&p.event.channel==3;CHECK(thru);
    const auto epoch=q->epoch();e.midi_panic();e.process(nullptr,out.data(),128);CHECK(q->epoch()>epoch);
    e.enqueue({ControlKind::seek,32});e.enqueue({ControlKind::play});e.process(nullptr,out.data(),128);bool chased=false;while(q->pop(p))chased|=p.event.kind==MidiKind::note_on&&p.event.data1==60;CHECK(chased);
    g.midi_monitor={false};e.prepare({rate,0,2,128,128,workers},g);CHECK(e.enqueue_live_midi({e.midi_generation(),0,{0,MidiKind::note_on,2,65,90}}));e.process(nullptr,out.data(),128);while(q->pop(p))CHECK(p.event.kind!=MidiKind::note_on);
    q->recover();for(int i=0;i<8191;++i)CHECK(q->push(0,{0,MidiKind::note_on,0,60,100},0));CHECK(!q->push(0,{0,MidiKind::note_off,0,60,0},0));CHECK(q->fault()&&q->dropped()==1);while(q->pop(p)){}q->recover();CHECK(!q->fault());
}}
void transitions(){
    auto q=std::make_shared<ExternalMidiQueue>();AudioEngine e;RenderGraph g;g.mixer.resize(1);g.live_midi={true};g.external_midi=q;g.external_midi_tracks={true};g.midi_notes={{{0,10000,60,100,0}}};
    e.prepare({48000,0,2,512,128,2},g);e.enqueue({ControlKind::loop,0,96});e.enqueue({ControlKind::play});std::array<float,1024> out{};e.process(nullptr,out.data(),128);
    ExternalMidiQueue::Packet packet;int on=0;while(q->pop(packet))on+=packet.event.kind==MidiKind::note_on;CHECK(on==2);
    MixerUpdate mix;mix.count=1;mix.tracks[0].mute=true;CHECK(e.enqueue_mix(mix));e.process(nullptr,out.data(),128);while(q->pop(packet))if(packet.epoch==q->epoch())CHECK(packet.event.kind!=MidiKind::note_on);
    mix.tracks[0].mute=false;CHECK(e.enqueue_mix(mix));e.process(nullptr,out.data(),64);on=0;while(q->pop(packet))on+=packet.event.kind==MidiKind::note_on;CHECK(on>0);
    e.enqueue({ControlKind::stop});const auto before=q->epoch();e.process(nullptr,out.data(),64);CHECK(q->epoch()>before);while(q->pop(packet)){}
    CHECK(e.enqueue_audition_midi({e.midi_generation(),0,{0,MidiKind::note_on,0,65,90}}));e.process(nullptr,out.data(),64);while(q->pop(packet)){}
    CHECK(e.enqueue_audition_midi({e.midi_generation(),0,{0,MidiKind::note_off,0,65,0},0,70}));bool off=false;
    for(int block=0;block<60;++block){e.process(nullptr,out.data(),64);while(q->pop(packet))off|=packet.event.kind==MidiKind::note_off&&packet.event.data1==65;}CHECK(off);
    auto recording=std::make_shared<MidiRecorder>(0);g.midi_notes={{}};g.midi_monitor={false};g.midi_recordings={{0,recording}};e.prepare({48000,0,2,128,128,2},g);e.enqueue({ControlKind::play});
    CHECK(e.enqueue_live_midi({e.midi_generation(),0,{0,MidiKind::note_on,0,67,100}}));e.process(nullptr,out.data(),128);while(q->pop(packet))CHECK(packet.event.kind!=MidiKind::note_on);CHECK(recording->finish(Timeline(TimeMap{},48000)).notes.size()==1);
}
void worker(){auto state=std::make_shared<State>();auto q=std::make_shared<ExternalMidiQueue>();{
    MidiOutputs outputs(q,std::make_unique<Backend>(state));outputs.routes({{"fixture",0,4}},11);until([&]{return outputs.status("fixture").find("connected")!=std::string::npos;});
    CHECK(q->push(0,{0,MidiKind::note_on,0,60,100},midi_clock_ns(),11));until([&]{return received(state,0x643C94);});CHECK(state->sender!=std::this_thread::get_id());
    const auto before=messages(state);CHECK(q->push(0,{0,MidiKind::note_on,0,61,100},midi_clock_ns(),10));std::this_thread::sleep_for(std::chrono::milliseconds(5));CHECK(messages(state)==before);
    CHECK(q->push(0,{0,MidiKind::note_on,0,62,100},midi_clock_ns()+100000000ULL,11));q->panic();std::this_thread::sleep_for(std::chrono::milliseconds(110));CHECK(messages(state)==before);
    state->present=false;until([&]{return outputs.status("fixture").find("missing")!=std::string::npos;});state->present=true;state->busy=true;until([&]{return outputs.status("fixture").find("unavailable")!=std::string::npos;});state->busy=false;outputs.reconnect();until([&]{return outputs.status("fixture").find("connected")!=std::string::npos;});
    state->sendFail=true;CHECK(q->push(0,{0,MidiKind::note_on,0,63,100},midi_clock_ns(),11));until([&]{return q->fault();});q->recover();state->sendFail=false;outputs.reconnect();until([&]{return outputs.status("fixture").find("connected")!=std::string::npos;});
}CHECK(state->resets>0&&state->closes>0);}
void application(){auto state=std::make_shared<State>();Application app(std::make_unique<Backend>(state));app.new_project();const auto track=app.add_instrument_track("External keys");app.connect(std::make_unique<Manual>(),{0,48000,128,{}, {0,1}});app.set_midi_output(track,"fixture",4);
    until([&]{return app.midi_output_status(track).find("connected")!=std::string::npos;});const auto snapshot=*app.services().projects->state().project;CHECK(app.undo());CHECK(app.redo());CHECK(*app.services().projects->state().project==snapshot);
    const auto clip=app.create_midi_clip(track,0,ppq);app.set_midi_notes(clip,{{new_id(),0,ppq/2,60,100,2}});app.poll();until([&]{return app.midi_output_status(track).find("connected")!=std::string::npos;});std::this_thread::sleep_for(std::chrono::milliseconds(5));
    app.play();std::array<float,256> out{};app.engine()->process(nullptr,out.data(),128);until([&]{return received(state,0x643C94);});app.pause();app.engine()->process(nullptr,out.data(),128);app.poll();
    const auto folder=std::filesystem::temp_directory_path()/("mrs-external-midi-"+new_id().value);std::filesystem::create_directory(folder);const auto file=folder/"External.mrsproject";app.save_project(file);app.open_project(file);CHECK(app.services().projects->state().project->tracks[0].midi_output=="fixture"&&app.services().projects->state().project->tracks[0].midi_output_channel==4);app.disconnect();std::filesystem::remove_all(folder);
}
}
void* operator new(std::size_t n){if(probe)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc{};}void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
int main(){try{model();engine();transitions();worker();application();std::cout<<"External MIDI PASS: model/schema/Undo, remap/playback/thru, stale/overflow/transport, worker/reconnect, Save/Open, zero callback allocations\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
