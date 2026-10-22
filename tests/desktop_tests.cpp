#include <mrs/desktop.hpp>
#include <mrs/offline_device.hpp>
#include <chrono>
#include <cmath>
#include <thread>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
using namespace mrs; using namespace mrs::desktop;
void check(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)),#__VA_ARGS__)
template<class F> void rejects(F f) { bool bad{}; try { f(); } catch (const std::exception&) { bad = true; } CHECK(bad); }
struct Directory {
    std::filesystem::path path = std::filesystem::temp_directory_path()/("mrs-shell-"+new_id().value);
    Directory() { std::filesystem::create_directory(path); }
    ~Directory() { std::error_code error; std::filesystem::remove_all(path,error); }
};
template<class F> void eventually(F f) {
    auto end = std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while (std::chrono::steady_clock::now()<end) {
        try { if (f()) return; } catch (const std::runtime_error&) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    throw std::runtime_error("timed out");
}
void track_folders(){
    Application app;app.disconnect();app.new_project();const auto a=app.add_audio_track("A"),b=app.add_audio_track("B");
    const auto id=app.create_track_folder("Drums",{a,b});app.update_track_folder(id,"Rhythm",true);
    CHECK(app.services().projects->state().project->folders.back().collapsed);CHECK(app.history_preserves_plugins(false));CHECK(app.undo()&&app.redo());
    app.link_folder_bus(id,{},true);const auto linked=*app.services().projects->state().project;const auto bus=*linked.folders.back().bus;
    CHECK(linked.tracks.size()==3&&linked.tracks[0].output==bus&&linked.tracks[1].output==bus);
    CHECK(app.undo()&&app.services().projects->state().project->tracks.size()==2);CHECK(app.redo());
    Directory dir;app.save_project(dir.path/"folders.mrsproject");app.open_project(app.path());CHECK(*app.services().projects->state().project==linked);
    app.play();eventually([&]{return app.engine()->state().playback==PlaybackState::playing;});const auto generation=app.insert_generation();
    app.update_track_folder(id,"Playing folder",false);CHECK(app.engine()->state().playback==PlaybackState::playing&&app.insert_generation()==generation);
    CHECK(app.undo()&&app.redo()&&app.engine()->state().playback==PlaybackState::playing);
    app.remove_track_folder(id);CHECK(app.services().projects->state().project->folders.empty()&&app.services().projects->state().project->tracks[0].output==bus);
    CHECK(app.undo());app.pause();eventually([&]{return app.engine()->state().playback!=PlaybackState::playing;});
    app.remove_track(bus);CHECK(!app.services().projects->state().project->folders.back().bus);CHECK(app.undo());
}
void workspaces() {
    Application app; auto services = app.services(); auto graph = app.graphs(); auto engine = app.engine();
    for (auto w : {Workspace::arrange,Workspace::mix,Workspace::edit,Workspace::live}) {
        app.workspace(w); CHECK(app.workspace() == w && app.services().projects == services.projects &&
            app.services().transport == services.transport && app.graphs() == graph && app.engine() == engine);
    }
    app.rename_track(services.projects->state().project->tracks.front().id,"From Arrange");
    CHECK(app.musical().state().project->tracks.front().name == "From Arrange");
    CHECK(services.projects->undo() && app.musical().state().project->tracks.front().name == "Demo tone");
    CHECK(services.projects->redo()); rejects([&] { app.workspace(static_cast<Workspace>(10)); });
}
void transport() {
    Application app; app.play();
    // Do not pump the application/UI: audio callback must continue independently.
    eventually([&] { return app.engine()->state().sample >= 1024; });
    app.poll(); CHECK(app.musical().state().transport.playback == PlaybackState::playing);
    app.pause(); eventually([&] { return app.engine()->state().playback == PlaybackState::paused; });
    const auto sample = app.engine()->state().sample; std::this_thread::sleep_for(std::chrono::milliseconds(10)); CHECK(app.engine()->state().sample == sample);
    app.musical().seek_section({"section-verse"}); eventually([&] { return app.engine()->state().sample > sample; });
    app.poll(); CHECK(app.musical().state().current_section->name == "Verse");
    app.musical().loop_section({"section-verse"}); eventually([&] { return app.engine()->state().loop.has_value(); });
    app.stop(); eventually([&] { return app.engine()->state().sample == 0 && app.engine()->state().playback == PlaybackState::stopped; });
    app.poll(); CHECK(app.musical().state().transport.sample == 0);
    rejects([&] { app.seek(-1); });
}
void files() {
    Directory dir; Application app; auto path = dir.path / std::filesystem::path(std::u8string(u8"Песня.mrsproject"));
    auto id = app.services().projects->state().project->id; app.save_project(path); CHECK(!app.dirty());
    app.rename_track(app.services().projects->state().project->tracks.front().id,"Saved"); CHECK(app.dirty()); app.save_project(path);
    auto graph = app.graphs()->state().graph; app.workspace(Workspace::live); app.open_project(path);
    CHECK(app.services().projects->state().project->id == id && app.services().projects->state().project->tracks.front().name == "Saved");
    CHECK(app.graphs()->state().graph->id == graph->id && app.workspace() == Workspace::live && !app.dirty());
    CHECK(app.services().transport->state().playback == PlaybackState::stopped);
    auto snapshot = app.snapshot(); snapshot.extensions = {{"FUTR","opaque"}};
    auto second = dir.path / "future.mrsproject"; persistence::save_project(second,snapshot);
    app.open_project(second); app.rename_track(app.services().projects->state().project->tracks.front().id,"Future"); app.save_project(second);
    CHECK(persistence::load_project(second).extensions == snapshot.extensions);
    auto before = app.services().projects; rejects([&] { app.open_project(dir.path / "missing"); }); CHECK(app.services().projects == before);
    { std::ofstream out(dir.path / "broken"); out << "bad"; }
    rejects([&] { app.open_project(dir.path / "broken"); }); CHECK(app.services().projects == before);
    app.demo(); CHECK(app.dirty() && app.path().empty());
    auto opaque=app.snapshot();AraBinding binding;binding.source=opaque.project.clips.front().source;binding.persistent_id="saved-ara-id";binding.module=(dir.path/"missing-ara.vst3").string();binding.class_id="missing-class";binding.factory_id="factory";binding.version="1";binding.archive_id="opaque";binding.archive={std::byte{0},std::byte{255},std::byte{42}};binding.component_state={std::byte{7}};binding.controller_state={std::byte{8}};binding.checksum=17;binding.source_checksum=99;opaque.project.ara_bindings={binding};
    const auto araFile=dir.path/"opaque-ara.mrsproject";persistence::save_project(araFile,opaque);app.open_project(araFile);app.save_project(araFile);CHECK(persistence::load_project(araFile).project.ara_bindings==opaque.project.ara_bindings); // also runs with ARA OFF/no SDK
}
void wav(const std::filesystem::path& path, std::uint16_t channels = 2,std::uint32_t rate=44100) {
    // Exact PCM16 stereo RIFF fixture at a non-default sample rate.
    std::ofstream f(path,std::ios::binary);
    auto u16 = [&](std::uint16_t value) { for (int i = 0; i < 2; ++i) f.put(static_cast<char>((value >> (8*i)) & 255)); };
    auto u32 = [&](std::uint32_t value) { for (int i = 0; i < 4; ++i) f.put(static_cast<char>((value >> (8*i)) & 255)); };
    f.write("RIFF",4); u32(36+4000); f.write("WAVEfmt ",8); u32(16); u16(1); u16(channels); u32(rate); u32(rate*channels*2); u16(static_cast<std::uint16_t>(channels*2)); u16(16);
    f.write("data",4); u32(4000); for (int i = 0; i < 2000; ++i) u16(4000);
}
class ManualDevice final : public audio::IAudioDevice {
public:
    std::vector<audio::DeviceInfo> enumerate() override { if (phase_ != audio::DevicePhase::closed) throw std::runtime_error("driver cannot enumerate an open stream"); return {{0,"Manual render test",{"Mic","Line","DI","Aux"},{"L","R","Cue 1","Cue 2"},32,2048,128,-1}}; }
    void control_panel(int) override {}
    audio::DeviceConfig last_config{};
    unsigned opens{},stops{};
    void open(const audio::DeviceConfig& c,std::shared_ptr<audio::AudioEngine>) override { last_config=c; ++opens; phase_ = audio::DevicePhase::open; }
    void start() override { phase_ = audio::DevicePhase::running; }
    void stop() override { if (fail_stop_) throw std::runtime_error("test driver stop failure"); ++stops;phase_ = audio::DevicePhase::stopped; }
    void fail_driver() { phase_ = audio::DevicePhase::error; fail_stop_ = true; }
    void close() noexcept override { phase_ = audio::DevicePhase::closed; }
    audio::DeviceStatus status() override { return {phase_,44100,0,0,0,{}}; }
private:
    audio::DevicePhase phase_{audio::DevicePhase::closed};
    bool fail_stop_{};
};
void tempo(){
    Directory dir;Application app;app.new_project(48000);const auto id=app.add_instrument_track("Tempo keys");app.create_midi_clip(id,0,8*ppq);app.add_section("Verse",0,4*ppq);app.add_marker("Cue",ppq,MarkerKind::cue);const auto audioFile=dir.path/"Tempo.wav";wav(audioFile,2,48000);app.import_wavs({audioFile});
    struct Device final:audio::IAudioDevice{
        bool fail{},fail_always{};int starts{},stops{};audio::DevicePhase phase{audio::DevicePhase::closed};
        std::vector<audio::DeviceInfo> enumerate()override{return {{0,"Timing fixture",{"Mic"},{"L","R"},16,8192,128,1}};}
        void control_panel(int)override{}void open(const audio::DeviceConfig&,std::shared_ptr<audio::AudioEngine>)override{phase=audio::DevicePhase::open;}
        void start()override{++starts;if(fail||fail_always){fail=false;throw std::runtime_error("timing start failure");}phase=audio::DevicePhase::running;}
        void stop()override{++stops;phase=audio::DevicePhase::stopped;}void close()noexcept override{phase=audio::DevicePhase::closed;}
        audio::DeviceStatus status()override{return {phase,48000,{}};}
    };
    auto device=std::make_unique<Device>();auto* driver=device.get();app.connect(std::move(device),{0,48000,128,{}, {0,1}});
    std::array<float,256> out{};app.seek(34567);app.engine()->process(nullptr,out.data(),128);app.poll();app.pause();app.engine()->process(nullptr,out.data(),128);app.poll();
    const auto original=*app.services().projects->state().project;const auto head=app.engine()->state();const auto generation=app.insert_generation(),midi_generation=app.engine()->midi_generation();
    auto map=original.time;map.tempos={{0,120},{4*ppq,90},{8*ppq,140}};unsigned events{};auto connection=app.services().projects->subscribe([&](const auto&){++events;});
    app.set_tempos(original.time,map.tempos);CHECK(app.services().projects->state().project->time==map&&events==1);
    CHECK(app.engine()->state().sample==head.sample&&app.engine()->state().playback==head.playback&&app.insert_generation()==generation&&app.engine()->midi_generation()==midi_generation);
    auto preserved=*app.services().projects->state().project;preserved.time=original.time;CHECK(preserved==original&&app.history_preserves_plugins(false));CHECK(app.undo()&&app.redo());
    auto before=app.services().projects->state();const auto event_count=events;auto changed=map;changed.tempos.front().bpm=150;driver->fail=true;rejects([&]{app.set_time_map(changed);});
    CHECK(app.services().projects->state().project==before.project&&app.services().projects->state().revision==before.revision&&events==event_count&&driver->phase==audio::DevicePhase::running&&app.engine()->state().sample==head.sample);
    struct FailingListener{bool* fail;unsigned* count;FailingListener(bool& f,unsigned& c):fail(&f),count(&c){}FailingListener(const FailingListener& other):fail(other.fail),count(other.count){if(*fail)throw std::runtime_error("publish preparation failure");}void operator()(const ProjectState&)const{++*count;}};
    bool failPublish=false;unsigned notified{};auto failing=app.services().projects->subscribe(FailingListener{failPublish,notified});failPublish=true;before=app.services().projects->state();rejects([&]{app.set_time_map(changed);});failPublish=false;
    CHECK(app.services().projects->state().revision==before.revision&&app.services().projects->state().project==before.project&&notified==0&&driver->phase==audio::DevicePhase::running);
    CHECK(app.undo());before=app.services().projects->state();driver->fail=true;rejects([&]{app.redo();});CHECK(app.services().projects->state().revision==before.revision&&app.services().projects->state().can_redo==before.can_redo);CHECK(app.redo());
    before=app.services().projects->state();app.set_time_map(map);CHECK(app.services().projects->state().revision==before.revision);
    rejects([&]{app.set_tempos(original.time,map.tempos);});auto invalid=map;invalid.tempos.erase(invalid.tempos.begin());rejects([&]{app.set_time_map(invalid);});invalid=map;invalid.tempos.push_back(invalid.tempos.back());rejects([&]{app.set_time_map(invalid);});invalid=map;invalid.tempos[0].bpm=std::numeric_limits<double>::quiet_NaN();rejects([&]{app.set_time_map(invalid);});invalid=map;invalid.tempos[0].bpm=1001;rejects([&]{app.set_time_map(invalid);});
    const auto stops=driver->stops;invalid=map;invalid.tempos.front().bpm=0;rejects([&]{app.set_time_map(invalid);});CHECK(driver->stops==stops&&app.services().projects->state().revision==before.revision);
    {Application high;high.new_project(768000);const auto originalState=high.services().projects->state();auto distant=originalState.project->time;distant.tempos={{0,1},{max_tick,120}};rejects([&]{high.set_time_map(distant);});CHECK(high.services().projects->state().revision==originalState.revision&&high.services().projects->state().project==originalState.project);}
    {Application distant;distant.new_project(48000);const auto initial=distant.services().projects->state();const auto sample=Timeline(initial.project->time,48000).to_samples(max_tick/2);distant.seek(sample);distant.engine()->process(nullptr,out.data(),128);distant.poll();auto fast=initial.project->time;fast.tempos={{0,1000}};rejects([&]{distant.set_time_map(fast);});CHECK(distant.services().projects->state().project==initial.project&&distant.services().projects->state().revision==initial.revision&&distant.engine()->state().sample==sample);}
    const auto loopSection=app.services().projects->state().project->sections.front().id;app.musical().loop_section(loopSection);app.engine()->process(nullptr,out.data(),128);rejects([&]{app.set_time_map(changed);});rejects([&]{app.undo();});app.musical().clear_loop();app.engine()->process(nullptr,out.data(),128);
    app.play();app.engine()->process(nullptr,out.data(),128);rejects([&]{app.set_time_map(changed);});app.pause();app.engine()->process(nullptr,out.data(),128);app.poll();
    auto clicks=app.click_settings();clicks.playback=true;app.set_click_settings(clicks);
    for(Sample start:{Sample{95940},Sample{223940}}){app.seek(start);app.engine()->process(nullptr,out.data(),128);app.poll();audio::Metronome reference;reference.prepare(map,48000);app.play();for(int block=0;block<3;++block){app.engine()->process(nullptr,out.data(),128);for(int f=0;f<128;++f){const auto expected=reference.sample(start+block*128+f,true,clicks.accent,clicks.level/100.f);CHECK(out[2*f]==expected&&out[2*f+1]==expected);}}app.pause();app.engine()->process(nullptr,out.data(),128);app.poll();}
    app.save_project(dir.path/"Tempo.mrsproject");const auto saved=app.snapshot().project;app.open_project(dir.path/"Tempo.mrsproject");CHECK(app.snapshot().project==saved);
    ProjectStore store{original};const UpdateTempoMap edit{original.time,map.tempos};auto transaction=store.prepare_transaction(&edit);const auto state=store.state();unsigned notifications{};auto observer=store.subscribe([&](const auto&){++notifications;});transaction->commit();CHECK(notifications==0&&store.state().project->time==map);transaction->rollback();CHECK(store.state().project==state.project&&store.state().revision==state.revision&&notifications==0);
    transaction=store.prepare_transaction(&edit);transaction->commit();transaction->finish();CHECK(notifications==1&&store.undo()&&*store.state().project==original);
}
void meter(){
    Directory dir;Application app;app.new_project(48000);const auto id=app.add_instrument_track("Meter keys");app.create_midi_clip(id,0,8*ppq);app.add_section("Verse",0,4*ppq);app.add_marker("Cue",ppq,MarkerKind::cue);const auto audioFile=dir.path/"Meter.wav";wav(audioFile,2,48000);app.import_wavs({audioFile});
    struct Device final:audio::IAudioDevice{
        bool fail{},fail_always{};int starts{},stops{};audio::DevicePhase phase{audio::DevicePhase::closed};
        std::vector<audio::DeviceInfo> enumerate()override{return {{0,"Meter fixture",{"Mic"},{"L","R"},16,8192,128,1}};}
        void control_panel(int)override{}void open(const audio::DeviceConfig&,std::shared_ptr<audio::AudioEngine>)override{phase=audio::DevicePhase::open;}
        void start()override{++starts;if(fail||fail_always){fail=false;throw std::runtime_error("meter start failure");}phase=audio::DevicePhase::running;}
        void stop()override{++stops;phase=audio::DevicePhase::stopped;}void close()noexcept override{phase=audio::DevicePhase::closed;}
        audio::DeviceStatus status()override{return {phase,48000,{}};}
    };
    auto device=std::make_unique<Device>();auto* driver=device.get();app.connect(std::move(device),{0,48000,128,{}, {0,1}});
    std::array<float,256> out{};app.seek(34567);app.engine()->process(nullptr,out.data(),128);app.poll();app.pause();app.engine()->process(nullptr,out.data(),128);app.poll();
    const auto original=*app.services().projects->state().project;const auto head=app.engine()->state();const auto generation=app.insert_generation(),midi_generation=app.engine()->midi_generation();
    auto map=original.time;map.meters={{1,4,4},{3,6,8},{5,3,4}};unsigned events{};auto connection=app.services().projects->subscribe([&](const auto&){++events;});
    app.set_meters(original.time,map.meters);CHECK(app.services().projects->state().project->time==map&&events==1);
    CHECK(app.engine()->state().sample==head.sample&&app.engine()->state().playback==head.playback&&app.insert_generation()==generation&&app.engine()->midi_generation()==midi_generation);
    auto preserved=*app.services().projects->state().project;preserved.time=original.time;CHECK(preserved==original&&app.history_preserves_plugins(false));CHECK(app.undo()&&app.redo());
    auto before=app.services().projects->state();const auto event_count=events;auto changed=map;changed.meters.front().numerator=2;driver->fail=true;rejects([&]{app.set_time_map(changed);});
    CHECK(app.services().projects->state().project==before.project&&app.services().projects->state().revision==before.revision&&events==event_count&&driver->phase==audio::DevicePhase::running&&app.engine()->state().sample==head.sample);
    struct FailingListener{bool* fail;unsigned* count;FailingListener(bool& f,unsigned& c):fail(&f),count(&c){}FailingListener(const FailingListener& other):fail(other.fail),count(other.count){if(*fail)throw std::runtime_error("publish preparation failure");}void operator()(const ProjectState&)const{++*count;}};
    bool failPublish=false;unsigned notified{};auto failing=app.services().projects->subscribe(FailingListener{failPublish,notified});failPublish=true;before=app.services().projects->state();rejects([&]{app.set_time_map(changed);});failPublish=false;
    CHECK(app.services().projects->state().revision==before.revision&&app.services().projects->state().project==before.project&&notified==0&&driver->phase==audio::DevicePhase::running);
    CHECK(app.undo());before=app.services().projects->state();driver->fail=true;rejects([&]{app.redo();});CHECK(app.services().projects->state().revision==before.revision&&app.services().projects->state().can_redo==before.can_redo);CHECK(app.redo());
    before=app.services().projects->state();app.set_time_map(map);CHECK(app.services().projects->state().revision==before.revision);
    CHECK(Timeline(map,48000).to_ticks({5,1,0})==14*ppq);app.set_meters(map,changed.meters);CHECK(Timeline(app.services().projects->state().project->time,48000).to_ticks({5,1,0})==10*ppq);auto kept=*app.services().projects->state().project;kept.time=map;CHECK(kept==*before.project);CHECK(app.undo());
    before=app.services().projects->state();rejects([&]{app.set_meters(original.time,map.meters);});
    for(auto invalid:std::vector<TimeMap>{[&]{auto t=map;t.meters.erase(t.meters.begin());return t;}(),[&]{auto t=map;t.meters.push_back(t.meters.back());return t;}(),[&]{auto t=map;t.meters[1].denominator=3;return t;}(),[&]{auto t=map;t.meters[1].numerator=65;return t;}(),[&]{auto t=map;t.meters[1].numerator=0;return t;}(),[&]{auto t=map;t.meters[1].bar=0;return t;}(),[&]{auto t=map;t.meters.push_back({max_tick,64,1});return t;}()}){const auto stops=driver->stops;rejects([&]{app.set_time_map(invalid);});CHECK(driver->stops==stops&&app.services().projects->state().revision==before.revision);}
    {Application high;high.new_project(768000);high.disconnect();auto slow=high.services().projects->state().project->time;slow.tempos={{0,1}};high.set_time_map(slow);auto time=slow;time.meters={{1,4,4},{max_tick/4000,4,4}};const auto prior=high.services().projects->state();rejects([&]{high.set_meters(prior.project->time,time.meters);});CHECK(high.services().projects->state().revision==prior.revision);}
    const auto loopSection=app.services().projects->state().project->sections.front().id;app.musical().loop_section(loopSection);app.engine()->process(nullptr,out.data(),128);rejects([&]{app.set_time_map(changed);});rejects([&]{app.undo();});app.musical().clear_loop();app.engine()->process(nullptr,out.data(),128);
    app.play();app.engine()->process(nullptr,out.data(),128);rejects([&]{app.set_time_map(changed);});app.pause();app.engine()->process(nullptr,out.data(),128);app.poll();
    auto clicks=app.click_settings();clicks.playback=true;app.set_click_settings(clicks);
    for(Sample start:{Sample{191940},Sample{335940}}){app.seek(start);app.engine()->process(nullptr,out.data(),128);app.poll();audio::Metronome reference;reference.prepare(map,48000);app.play();for(int block=0;block<3;++block){app.engine()->process(nullptr,out.data(),128);for(int f=0;f<128;++f){const auto expected=reference.sample(start+block*128+f,true,clicks.accent,clicks.level/100.f);CHECK(out[2*f]==expected&&out[2*f+1]==expected);}}app.pause();app.engine()->process(nullptr,out.data(),128);app.poll();}
    const auto audioTrack=app.services().projects->state().project->tracks.back().id;app.set_track_input(audioTrack,0);app.set_track_armed(audioTrack,true);clicks.count_bars=1;app.set_click_settings(clicks);app.seek(192000);app.engine()->process(nullptr,out.data(),128);app.poll();app.start_recording(dir.path/"Meter-count.wav");
    std::array<float,128> input{};app.engine()->process(input.data(),out.data(),128);CHECK(app.engine()->state().count_remaining==72000-128&&app.engine()->state().sample==192000);const auto recordingRevision=app.services().projects->state().revision;rejects([&]{app.set_meters(map,changed.meters);});rejects([&]{app.undo();});CHECK(app.services().projects->state().revision==recordingRevision);
    for(int i=0;i<600&&app.engine()->state().count_remaining;++i)app.engine()->process(input.data(),out.data(),128);CHECK(app.engine()->state().count_remaining==0&&app.engine()->state().sample>=192000);app.engine()->process(input.data(),out.data(),128);app.stop_recording();CHECK(!app.recording());
    app.save_project(dir.path/"Meter.mrsproject");const auto saved=app.snapshot().project;app.open_project(dir.path/"Meter.mrsproject");CHECK(app.snapshot().project==saved);
    ProjectStore store{original};const UpdateMeterMap edit{original.time,map.meters};auto transaction=store.prepare_transaction(&edit);const auto state=store.state();unsigned notifications{};auto observer=store.subscribe([&](const auto&){++notifications;});transaction->commit();CHECK(notifications==0&&store.state().project->time==map);transaction->rollback();CHECK(store.state().project==state.project&&store.state().revision==state.revision&&notifications==0);
    transaction=store.prepare_transaction(&edit);transaction->commit();transaction->finish();CHECK(notifications==1&&store.undo()&&*store.state().project==original);
}
void sections(){
    Directory dir;Application app;app.new_project(44100);const auto track=app.add_instrument_track("Keys");app.create_midi_clip(track,0,4*ppq);auto device=std::make_unique<ManualDevice>();auto* driver=device.get();app.connect(std::move(device),{0,44100,128,{}, {0,1}});const auto original=*app.services().projects->state().project;
    const auto id=app.add_section("Intro",0,4*ppq);app.add_section("Припев",8*ppq,12*ppq,0xD5B63E);const auto before=*app.services().projects->state().project;auto current=before.sections.front();auto changed=current;changed.color=0xFFFFFF;
    std::array<float,256> out{};app.play();app.engine()->process(nullptr,out.data(),128);const auto stops=driver->stops;const auto generation=app.insert_generation();const auto position=app.engine()->state().sample;app.update_section(current,changed);CHECK(app.undo());CHECK(app.redo());CHECK(driver->stops==stops&&app.insert_generation()==generation&&app.engine()->state().sample==position&&app.engine()->state().playback==PlaybackState::playing);
    auto moved=changed;moved.end=3*ppq;const auto revision=app.services().projects->state().revision;rejects([&]{app.update_section(changed,moved);});CHECK(app.services().projects->state().revision==revision);app.pause();app.engine()->process(nullptr,out.data(),128);app.update_section(changed,moved);CHECK(driver->stops==stops&&app.insert_generation()==generation);CHECK(app.undo());CHECK(app.redo());
    const auto state=app.services().projects->state();auto bad=moved;bad.end=9*ppq;rejects([&]{app.update_section(moved,bad);});bad=moved;bad.end=bad.start;rejects([&]{app.update_section(moved,bad);});bad=moved;bad.color=0x1000000;rejects([&]{app.update_section(moved,bad);});rejects([&]{app.update_section(current,changed);});CHECK(app.services().projects->state().revision==state.revision);
    app.musical().loop_section(id);app.engine()->process(nullptr,out.data(),128);bad=moved;bad.start=ppq;rejects([&]{app.update_section(moved,bad);});rejects([&]{app.undo();});changed=moved;changed.color=0x287B89;app.update_section(moved,changed);CHECK(app.undo()&&app.redo());CHECK(app.engine()->state().loop.has_value());app.musical().clear_loop();app.engine()->process(nullptr,out.data(),128);app.update_section(changed,bad);
    const auto bodyBefore=*app.services().projects->state().project;auto body=bodyBefore.sections.front();auto bodyMoved=body;bodyMoved.start+=ppq;bodyMoved.end+=ppq;app.update_section(body,bodyMoved);CHECK(app.undo()&&app.redo());CHECK(app.undo()&&*app.services().projects->state().project==bodyBefore);
    auto unaffected=*app.services().projects->state().project;unaffected.sections.clear();CHECK(unaffected==original);app.save_project(dir.path/"Sections.mrsproject");const auto saved=*app.services().projects->state().project;app.open_project(dir.path/"Sections.mrsproject");CHECK(*app.services().projects->state().project==saved&&saved.sections.front().id==id);
    const auto renameBefore=app.services().projects->state().project->sections.front();auto renamed=renameBefore;renamed.name="Вступление";app.update_section(renameBefore,renamed);
    CHECK(app.services().projects->state().project->sections.front()==renamed);CHECK(app.undo()&&app.redo());
    auto removeRevision=app.services().projects->state().revision;rejects([&]{app.remove_section(renameBefore);});CHECK(app.services().projects->state().revision==removeRevision);
    app.remove_section(renamed);CHECK(app.services().projects->state().project->sections.size()==1);CHECK(app.undo()&&app.services().projects->state().project->sections.front()==renamed);CHECK(app.redo()&&app.undo());
    app.save_project(dir.path/"Renamed.mrsproject");app.open_project(dir.path/"Renamed.mrsproject");CHECK(app.services().projects->state().project->sections.front()==renamed);
    auto linked=app.snapshot();const auto patch=new_id();linked.patches.push_back({patch,linked.graph});linked.section_patches.push_back({id,patch});linked.extensions.push_back({"FUTR","section appearance"});persistence::save_project(dir.path/"Linked.mrsproject",linked);app.open_project(dir.path/"Linked.mrsproject");auto linkedSection=app.services().projects->state().project->sections.front();auto linkedValue=linkedSection;linkedValue.end-=ppq/4;linkedValue.color=0x7050A0;app.update_section(linkedSection,linkedValue);CHECK(app.history_preserves_plugins(false));CHECK(app.undo()&&app.redo());CHECK(app.snapshot().section_patches==linked.section_patches&&app.snapshot().extensions==linked.extensions);
    removeRevision=app.services().projects->state().revision;rejects([&]{app.remove_section(linkedValue);});CHECK(app.services().projects->state().revision==removeRevision&&app.snapshot().section_patches==linked.section_patches);
    app.new_project(44100);const auto mic=app.add_audio_track("Mic");app.set_track_armed(mic,true);app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{0},{0,1}});auto settings=app.click_settings();settings.count_bars=1;app.set_click_settings(settings);app.start_recording(dir.path/"SectionGuard.wav");rejects([&]{app.add_section("Blocked",0,ppq);});rejects([&]{app.undo();});app.stop();app.engine()->process(nullptr,out.data(),128);app.poll();
    // Copy is a single metadata transaction, never copies clip content or Live bindings.
    app.new_project(44100);const auto keys=app.add_instrument_track("Keys");app.create_midi_clip(keys,0,ppq);
    const auto sectionId=app.add_section("Verse",0,ppq,0x7050A0);const auto source=app.services().projects->state().project->sections.front();
    auto copyDoc=app.snapshot();const auto copyPatch=new_id();copyDoc.patches.push_back({copyPatch,copyDoc.graph});copyDoc.section_patches.push_back({sectionId,copyPatch});
    persistence::save_project(dir.path/"CopyBinding.mrsproject",copyDoc);app.open_project(dir.path/"CopyBinding.mrsproject");
    auto copyDevice=std::make_unique<ManualDevice>();auto* copyDriver=copyDevice.get();app.connect(std::move(copyDevice),{0,44100,128,{}, {0,1}});
    const auto copyBefore=app.services().projects->state();const auto inserts=app.insert_generation();const auto head=app.engine()->state().sample;
    const auto copiedId=app.copy_section(source,ppq);const auto copiedProject=*app.services().projects->state().project;const auto copy=copiedProject.sections.back();
    CHECK(copy.id==copiedId&&copy.id!=source.id&&copy.name==source.name&&copy.color==source.color&&copy.start==ppq&&copy.end==2*ppq);
    auto contents=copiedProject;contents.sections=copyBefore.project->sections;CHECK(contents==*copyBefore.project);
    CHECK(copyDriver->stops==0&&app.insert_generation()==inserts&&app.engine()->state().sample==head&&app.history_preserves_plugins(false));
    CHECK(app.snapshot().section_patches==copyDoc.section_patches);CHECK(app.undo()&&*app.services().projects->state().project==*copyBefore.project&&app.redo());
    const auto copiedRevision=app.services().projects->state().revision;auto stale=source;stale.name="Stale";
    rejects([&]{app.copy_section(stale,4*ppq);});rejects([&]{app.copy_section(source,0);});rejects([&]{app.copy_section(source,-1);});rejects([&]{app.copy_section(source,max_tick);});
    CHECK(app.services().projects->state().revision==copiedRevision);
    app.play();app.engine()->process(nullptr,out.data(),128);rejects([&]{app.copy_section(source,4*ppq);});app.pause();app.engine()->process(nullptr,out.data(),128);
    app.musical().loop_section(sectionId);app.engine()->process(nullptr,out.data(),128);rejects([&]{app.copy_section(source,4*ppq);});app.musical().clear_loop();app.engine()->process(nullptr,out.data(),128);
    CHECK(app.services().projects->state().revision==copiedRevision);
    app.save_project(dir.path/"Copied.mrsproject");app.open_project(dir.path/"Copied.mrsproject");CHECK(*app.services().projects->state().project==copiedProject&&app.snapshot().section_patches==copyDoc.section_patches);
    const auto groupBefore=app.services().projects->state();const std::vector<ArrangerSection> group{groupBefore.project->sections.front(),groupBefore.project->sections.back()};
    const auto groupIds=app.transform_sections(group,4*ppq,true);CHECK(groupIds.size()==2&&groupIds[0]!=group[0].id&&app.services().projects->state().project->sections.size()==4&&app.snapshot().section_patches==copyDoc.section_patches);
    CHECK(app.undo()&&*app.services().projects->state().project==*groupBefore.project&&app.redo()&&app.undo());
    auto staleGroup=group;staleGroup.back().name="stale";const auto groupRevision=app.services().projects->state().revision;rejects([&]{app.transform_sections(staleGroup,4*ppq,true);});rejects([&]{app.transform_sections(group,ppq,true);});rejects([&]{app.remove_sections(group);});CHECK(app.services().projects->state().revision==groupRevision);
    app.transform_sections(group,4*ppq,false);CHECK(app.services().projects->state().project->sections.front().id==source.id&&app.snapshot().section_patches==copyDoc.section_patches);CHECK(app.undo()&&*app.services().projects->state().project==*groupBefore.project);
    ProjectStore copyCommands{*copyBefore.project};rejects([&]{copyCommands.execute(CopySection{source,source.id,ppq});});CHECK(copyCommands.state().revision==0);
    ProjectStore commands{original};commands.execute(AddSection{{new_id(),"Section",0,ppq,0x505B70}});CHECK(commands.undo()&&*commands.state().project==original&&commands.redo());
}
void lane_group_regressions(){
    Directory dir;Application app;app.new_project();app.add_chord("Am",0,ppq);app.add_chord("C",3*ppq,4*ppq);app.add_marker("A",0,MarkerKind::cue);app.add_marker("B",3*ppq,MarkerKind::warning);
    auto color=app.services().projects->state().project->chords.front();auto colored=color;colored.color=0x287B89;app.update_chord(color,colored);const auto before=*app.services().projects->state().project;const auto generation=app.insert_generation();
    const auto copies=app.transform_chords(before.chords,4*ppq,true);CHECK(copies.size()==2&&copies.front()!=before.chords.front().id&&app.services().projects->state().project->chords[2].color==colored.color);
    app.save_project(dir.path/"Colors.mrsproject");CHECK(persistence::load_project(dir.path/"Colors.mrsproject").project==*app.services().projects->state().project);CHECK(app.undo()&&*app.services().projects->state().project==before&&app.redo()&&app.undo());
    const auto rev=app.services().projects->state().revision;rejects([&]{app.transform_chords(before.chords,0,true);});auto stale=before.chords;stale.back().symbol="Stale";rejects([&]{app.transform_chords(stale,4*ppq,true);});rejects([&]{app.transform_chords(before.chords,-1,true);});CHECK(app.services().projects->state().revision==rev);
    app.transform_markers(before.markers,0,true);const auto marked=*app.services().projects->state().project;CHECK(marked.markers.size()==4&&marked.markers[0]==before.markers[0]&&marked.markers[1]==before.markers[1]&&marked.markers[2].id!=before.markers[0].id&&marked.markers[3].kind==MarkerKind::warning);
    app.save_project(dir.path/"Markers.mrsproject");CHECK(persistence::load_project(dir.path/"Markers.mrsproject").project==marked);CHECK(app.undo()&&*app.services().projects->state().project==before);
    app.transform_markers(before.markers,ppq,false);CHECK(app.services().projects->state().project->markers.front().id==before.markers.front().id&&app.services().projects->state().project->markers.front().tick==ppq);CHECK(app.undo());CHECK(app.insert_generation()==generation);
    app.remove_chords(before.chords);CHECK(app.services().projects->state().project->chords.empty()&&app.undo());app.remove_markers(before.markers);CHECK(app.services().projects->state().project->markers.empty()&&app.undo());
    auto bad=colored;bad.color=0x1000000;rejects([&]{app.update_chord(colored,bad);});
}
void chords() {
    lane_group_regressions();
    Directory dir;const auto media=dir.path/"tone.wav";wav(media);
    Application app;app.import_wav(media);const auto before=*app.services().projects->state().project;
    auto device=std::make_unique<ManualDevice>();auto* driver=device.get();
    app.connect(std::move(device),{0,44100,128,{}, {0,1}});
    const auto id=app.add_chord("C/G",8*ppq,12*ppq),first=app.add_chord("Am",0,4*ppq);
    const auto state=app.services().projects->state();
    CHECK(state.project->chords.front().id==first&&state.project->chords.back().id==id);
    auto other=*state.project;other.chords.clear();CHECK(other==before);
    const auto c=state.project->chords.front();auto value=c;value.symbol="Авторский аккорд";value.end=2*ppq;
    app.services().transport->set_loop(LoopRange{0,1000});
    app.play();std::array<float,256> out{};app.engine()->process(nullptr,out.data(),128);app.poll();
    const auto transport=app.engine()->state();const auto midi_generation=app.engine()->midi_generation();
    const auto insert_generation=app.insert_generation();
    app.update_chord(c,value);
    CHECK(app.engine()->state().sample==transport.sample&&app.engine()->state().loop==transport.loop);
    CHECK(app.engine()->state().playback==PlaybackState::playing&&driver->opens==1);
    CHECK(app.engine()->midi_generation()==midi_generation&&app.insert_generation()==insert_generation);
    CHECK(app.musical().state().current_chord->symbol==value.symbol);
    CHECK(app.undo()&&app.services().projects->state().project->chords.front()==c);
    CHECK(app.redo()&&app.services().projects->state().project->chords.front()==value);
    const auto revision=app.services().projects->state().revision;
    app.update_chord(value,value);CHECK(app.services().projects->state().revision==revision);
    rejects([&]{app.update_chord(c,c);});
    auto bad=value;bad.end=0;rejects([&]{app.update_chord(value,bad);});
    bad=value;bad.end=9*ppq;rejects([&]{app.update_chord(value,bad);});
    bad=value;bad.symbol.clear();rejects([&]{app.update_chord(value,bad);});
    bad=value;bad.symbol=std::string(257,'a');rejects([&]{app.update_chord(value,bad);});
    bad=value;bad.id=id;rejects([&]{app.update_chord(value,bad);});
    rejects([&]{app.add_chord("Overlap",1,ppq);});rejects([&]{app.add_chord("Negative",-1,1);});
    CHECK(app.services().projects->state().revision==revision);
    CHECK(app.engine()->midi_generation()==midi_generation&&app.insert_generation()==insert_generation);
    const auto copied=app.copy_chord(value,value.end);
    const auto copiedState=app.services().projects->state().project;
    const auto copy=std::find_if(copiedState->chords.begin(),copiedState->chords.end(),[&](const auto& chord){return chord.id==copied;});
    CHECK(copy!=copiedState->chords.end()&&copy->id!=value.id&&copy->symbol==value.symbol);
    CHECK(copy->start==value.end&&copy->end==2*value.end-value.start);
    CHECK(app.engine()->state().sample==transport.sample&&driver->opens==1);
    CHECK(app.engine()->midi_generation()==midi_generation&&app.insert_generation()==insert_generation);
    CHECK(app.undo()&&app.services().projects->state().project->chords.size()==2);
    CHECK(app.redo()&&app.services().projects->state().project->chords==copiedState->chords);
    CHECK(app.undo());
    const auto copyRevision=app.services().projects->state().revision;
    rejects([&]{app.copy_chord(value,0);}); // source overlap: no partial copy/history
    rejects([&]{app.copy_chord(value,8*ppq);});
    rejects([&]{app.copy_chord(c,4*ppq);}); // stale source
    rejects([&]{app.copy_chord(value,max_tick);});
    CHECK(app.services().projects->state().revision==copyRevision);
    app.engine()->process(nullptr,out.data(),128);app.poll();CHECK(app.engine()->state().sample==256&&out.front()!=0);
    app.remove_chord(value);CHECK(app.undo());CHECK(app.redo());CHECK(app.undo());
    app.pause();app.engine()->process(nullptr,out.data(),128);app.poll();
    app.services().transport->set_loop({});
    app.seek(Timeline(before.time,before.sample_rate).to_samples(2*ppq));
    app.engine()->process(nullptr,out.data(),128);app.poll();
    CHECK(!app.musical().state().current_chord&&app.musical().state().next_chord->id==id);
    app.stop();app.engine()->process(nullptr,out.data(),128);app.poll();
    auto document=app.snapshot();document.extensions={{"FUTR","keep chord metadata edits"}};
    const auto path=dir.path/"Chords.mrsproject";persistence::save_project(path,document);
    app.open_project(path);CHECK(app.services().projects->state().project->chords==document.project.chords);
    app.update_chord(value,value);CHECK(!app.dirty());
    app.remove_chord(value);app.save_project(path);
    const auto loaded=persistence::load_project(path);
    CHECK(loaded.extensions==document.extensions&&loaded.project.chords.back().id==id);
    auto saved_clips=loaded.project.clips,original_clips=before.clips;
    // Save migrates absolute external media refs to managed relative paths.
    // Chord edits must preserve every clip field except that intended path rewrite.
    for(auto& clip:saved_clips)clip.source.clear();
    for(auto& clip:original_clips)clip.source.clear();
    CHECK(saved_clips==original_clips&&loaded.project.time==before.time);
    rejects([&]{app.remove_chord(value);});
    app.new_project(44100);const auto mic=app.add_audio_track("Mic");app.set_track_armed(mic,true);
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{0},{0,1}});
    auto settings=app.click_settings();settings.count_bars=1;app.set_click_settings(settings);
    app.start_recording(dir.path/"Precount.wav");CHECK(app.recording());
    rejects([&]{app.add_chord("Blocked",0,ppq);});rejects([&]{app.undo();});
    CHECK(app.services().projects->state().project->chords.empty());
    app.stop();app.engine()->process(nullptr,out.data(),128);app.poll();
}
void markers() {
    Directory dir;Application app;app.new_project();const auto track=app.add_audio_track("Marker fixture");
    const auto before=*app.services().projects->state().project;
    std::vector<Id> ids;for(int kind=0;kind<6;++kind)ids.push_back(app.add_marker("Marker "+std::to_string(kind),ppq,static_cast<MarkerKind>(kind)));
    auto state=app.services().projects->state().project;CHECK(state->markers.size()==6);
    for(std::size_t i=0;i<ids.size();++i)CHECK(state->markers[i].id==ids[i]&&state->markers[i].kind==static_cast<MarkerKind>(i));
    auto other=*state;other.markers.clear();CHECK(other==before);
    auto device=std::make_unique<ManualDevice>();auto* driver=device.get();app.connect(std::move(device),{0,48000,128,{}, {0,1}});
    app.services().transport->set_loop(LoopRange{0,48000});app.play();std::array<float,256> out{};app.engine()->process(nullptr,out.data(),128);app.poll();
    const auto head=app.engine()->state();const auto source=state->markers[2];auto value=source;value.name="Авторский маркер";value.tick=3*ppq;value.kind=MarkerKind::lyric;
    value.color=0xAD5677;
    const auto midi=app.engine()->midi_generation(),inserts=app.insert_generation();app.update_marker(source,value);
    CHECK(app.services().projects->state().project->markers[2]==value&&app.dirty());
    CHECK(driver->opens==1&&app.engine()->state().sample==head.sample&&app.engine()->state().loop==head.loop&&app.engine()->state().playback==head.playback);
    CHECK(app.engine()->midi_generation()==midi&&app.insert_generation()==inserts);
    CHECK(app.undo()&&app.services().projects->state().project->markers[2]==source);CHECK(app.redo());
    const auto revision=app.services().projects->state().revision;app.update_marker(value,value);CHECK(app.services().projects->state().revision==revision);
    rejects([&]{app.update_marker(source,source);});auto bad=value;bad.id=ids[1];rejects([&]{app.update_marker(value,bad);});
    bad=value;bad.tick=-1;rejects([&]{app.update_marker(value,bad);});bad=value;bad.tick=max_tick+1;rejects([&]{app.update_marker(value,bad);});
    bad=value;bad.kind=static_cast<MarkerKind>(6);rejects([&]{app.update_marker(value,bad);});
    rejects([&]{app.add_marker(" ",0);});rejects([&]{app.add_marker(std::string(257,'a'),0);});
    CHECK(app.services().projects->state().revision==revision);
    app.remove_marker(value);CHECK(app.services().projects->state().project->markers.size()==5);CHECK(app.undo()&&app.services().projects->state().project->markers[2]==value);
    app.seek(Timeline(before.time,before.sample_rate).to_samples(ppq));app.engine()->process(nullptr,out.data(),128);app.poll();
    CHECK(app.musical().state().current_marker->id==ids.back()&&app.musical().state().next_marker->id==value.id);
    app.stop();app.engine()->process(nullptr,out.data(),128);app.poll();
    auto document=app.snapshot();document.extensions={{"FUTR","preserve marker metadata"}};
    document.midi.push_back({{"marker-port"},"missing-fixture","Fixture",false,true});
    document.actions.push_back({{"marker-action"},value.id,{"marker-port"},{0,processing::MidiKind::cc,0,1,64}});
    const auto file=dir.path/"Markers.mrsproject";persistence::save_project(file,document);app.open_project(file);
    CHECK(app.services().projects->state().project->markers==document.project.markers&&!app.dirty());
    const auto linkedRevision=app.services().projects->state().revision;rejects([&]{app.remove_marker(value);});
    rejects([&]{app.remove_markers(document.project.markers);});
    CHECK(app.services().projects->state().revision==linkedRevision&&!app.dirty());
    app.transform_markers({value},ppq,true);CHECK(app.snapshot().actions==document.actions&&app.services().projects->state().project->markers.size()==document.project.markers.size()+1);CHECK(app.undo()&&app.services().projects->state().project->markers==document.project.markers);
    auto renamed=value;renamed.name="Linked rename";app.update_marker(value,renamed);CHECK(app.undo()&&app.redo());
    app.save_project(file);const auto saved=persistence::load_project(file);
    CHECK(saved.actions==document.actions&&saved.midi==document.midi&&saved.extensions==document.extensions&&saved.project.markers[2]==renamed);
    app.open_project(file);CHECK(app.services().projects->state().project->markers==saved.project.markers);
    app.set_track_armed(track,true);auto settings=app.click_settings();settings.count_bars=1;app.set_click_settings(settings);
    app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{0}, {0,1}});
    app.start_recording(dir.path/"Precount.wav");CHECK(app.recording());
    rejects([&]{app.add_marker("Blocked",0);});rejects([&]{app.update_marker(renamed,renamed);});rejects([&]{app.remove_marker(source);});rejects([&]{app.undo();});
    app.stop();app.engine()->process(nullptr,out.data(),128);app.poll();
}
void assets() {
    Directory dir; auto file = dir.path / "music.wav"; wav(file); Application app; app.import_wav(file);
    const auto p = app.services().projects->state().project;
    CHECK(p->sample_rate == 44100 && p->clips.front().length == 1000 && p->chords.empty() && p->sections.empty());
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}});
    app.play(); std::array<float,256> output{};
    app.engine()->process(nullptr,output.data(),128); // manual device has no concurrent consumer
    CHECK(output[0] == 0.06103515625f && output[1] == 0.06103515625f); // PCM16 4000/32768 through shared native gain 0.5
    app.poll(); CHECK(app.musical().state().transport.sample == 128);
    app.disconnect(); CHECK(app.engine()->state().sample == 0);
    auto services = app.services(); rejects([&] { app.import_wav(dir.path / "absent.wav"); }); CHECK(app.services().projects == services.projects);
}
void mono_route() {
    Directory dir; const auto file = dir.path / "mono.wav"; wav(file,1);
    Application app; app.import_wav(file);
    CHECK(audio::inspect_wav(file).channels == 1);
    for (const auto& outputs : {std::vector<int>{0},std::vector<int>{0,1},std::vector<int>{0,1,2,3}}) {
        app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{},outputs});
        app.play(); std::array<float,512> buffer{};
        app.engine()->process(nullptr,buffer.data(),128);
        for (std::size_t frame = 0; frame < 128; ++frame) for (std::size_t channel = 0; channel < outputs.size(); ++channel)
            CHECK(buffer[frame*outputs.size()+channel] == (channel < 2 ? 0.06103515625f : 0.0f));
    }
}
void hardware() {
    Directory dir; const auto file = dir.path/"hardware.wav"; wav(file);
    Application app; app.import_wav(file); const auto id = app.services().projects->state().project->tracks.front().id;
    const auto bus=app.add_bus("Cue"); app.set_track_output(id,bus); app.set_hardware_output(bus,{2,3}); app.set_hardware_output(std::nullopt,{0,1});
    // Offline clock remains running while unavailable hardware routes are edited/saved.
    CHECK(app.audio_running()); app.rename_track(bus,"Monitor"); CHECK(app.audio_running());
    app.save_project(dir.path/"routes.mrsproject"); app.open_project(dir.path/"routes.mrsproject"); CHECK(app.audio_running());
    auto device=std::make_unique<ManualDevice>(); const auto driver=device.get(); app.connect(std::move(device),{0,44100,128,{}, {3,1,0,2}});
    CHECK(app.input_names().size() == 4 && app.output_names().size() == 4);
    app.play(); std::array<float,512> out{}; app.engine()->process(nullptr,out.data(),128);
    CHECK(out[0] == 0.1220703125f && out[1] == 0 && out[2] == 0 && out[3] == 0.1220703125f); // selected physical order, bypass master FX
    app.pause(); app.engine()->process(nullptr,out.data(),128); const auto position=app.engine()->state().sample;
    app.set_hardware_output(bus,{3}); CHECK(app.engine()->state().sample == position && driver->opens == 1);
    CHECK(app.undo() && app.redo() && app.engine()->state().sample == position);
    const auto before=app.services().projects->state().revision; rejects([&] { app.set_hardware_output(id,{4}); }); CHECK(app.services().projects->state().revision == before && app.audio_running());
    rejects([&] { app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}}); }); CHECK(app.audio_running() && app.active_outputs() == std::vector<int>{3,1,0,2});
    rejects([&] { app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1,2,3}}); }); CHECK(app.audio_running());
    app.remove_track(bus); CHECK(app.services().projects->state().project->tracks.front().hardware_outputs == std::vector<int>{3}); CHECK(app.undo());
    app.set_hardware_output(bus,{}); app.set_track_output(id,std::nullopt); app.set_hardware_output(std::nullopt,{2,3});
    app.seek(0); app.play(); app.engine()->process(nullptr,out.data(),128); CHECK(out[0] == 0.06103515625f && out[3] == 0.06103515625f && out[1] == 0 && out[2] == 0);
    rejects([&] { app.set_hardware_output(id,{0}); });
}
void profiles() {
    ManualDevice driver; auto info=driver.enumerate().front(); auto profile=capture_profile("Stage",info,{0,44100,128,{1},{2,3,0,1}});
    info.index=7; CHECK(resolve_profile(profile,info).device == 7 && resolve_profile(profile,info).inputs == std::vector<int>{1});
    Preferences prefs; prefs.profiles={profile}; CHECK(decode_preferences(encode_preferences(prefs)) == prefs);
    CHECK(decode_preferences("MRS_DESKTOP_CONFIG 3\n0 48000 128 -1 \"\" 2 0 1 0\n0\n").profiles.empty());
    auto bad=info; bad.name="Another interface"; rejects([&] { (void)resolve_profile(profile,bad); });
    bad=info; bad.outputs.resize(2); rejects([&] { (void)resolve_profile(profile,bad); });
    bad=info; std::swap(bad.outputs[2],bad.outputs[3]); rejects([&] { (void)resolve_profile(profile,bad); });
    bad=info; bad.inputs[1]="Renamed"; rejects([&] { (void)resolve_profile(profile,bad); });
    bad=info; bad.max_buffer=64; rejects([&] { (void)resolve_profile(profile,bad); });
    prefs.profiles.push_back(profile); rejects([&] { (void)encode_preferences(prefs); });
    rejects([&] { (void)capture_profile("",info,{7,44100,128,{}, {0,1}}); });
    rejects([&] { (void)decode_preferences("MRS_DESKTOP_CONFIG 4\n0 48000 128 -1 \"\" 2 0 1 0\n0\n17\n"); });
}
void sends() {
    Directory dir; Application app; app.new_project(44100);
    const auto id = app.add_audio_track("Audio"), bus=app.add_bus("Return"), second=app.add_bus("Return 2");
    auto device=std::make_unique<ManualDevice>(); const auto driver=device.get();
    app.connect(std::move(device),{0,44100,128,{0},{0,1}});
    app.set_track_input(id,1); CHECK(driver->last_config.inputs == std::vector<int>{0});
    app.arm_track(id); CHECK(driver->last_config.inputs == std::vector<int>{1} && driver->opens == 2);
    app.set_track_sends(id,{{bus,0.5f,true},{second,0.25f,false}}); CHECK(driver->opens == 2);
    const auto return_count=app.services().projects->state().project->tracks.size();
    (void)app.add_return_send(id,"Atomic return");
    CHECK(app.services().projects->state().project->tracks.size() == return_count+1);
    CHECK(app.undo() && app.services().projects->state().project->tracks.size() == return_count && app.services().projects->state().project->tracks.front().sends.size() == 2);
    const auto position=app.engine()->state().sample; app.set_track_input(id,-1);
    CHECK(!app.has_input() && driver->last_config.inputs.empty() && app.engine()->state().sample == position);
    CHECK(app.undo() && app.has_input() && driver->last_config.inputs == std::vector<int>{1});
    const auto revision=app.services().projects->state().revision;
    rejects([&] { app.set_track_input(id,7); }); CHECK(app.services().projects->state().revision == revision);
    app.play(); std::array<float,256> output{}; app.engine()->process(nullptr,output.data(),128);
    const auto before_preview=*app.services().projects->state().project;
    const auto preview_revision=app.services().projects->state().revision;
    CHECK(app.preview_send_gain(id,second,0.f));app.engine()->process(nullptr,output.data(),128);
    CHECK(app.preview_send_gain(id,bus,16.f));app.engine()->process(nullptr,output.data(),128);
    CHECK(*app.services().projects->state().project==before_preview&&app.services().projects->state().revision==preview_revision);
    CHECK(!app.preview_send_gain(id,new_id(),1.f)&&!app.preview_send_gain(new_id(),bus,1.f));
    rejects([&]{app.preview_send_gain(id,bus,-1.f);});rejects([&]{app.preview_send_gain(id,bus,std::numeric_limits<float>::infinity());});
    bool full=false;for(int i=0;i<1000;++i)if(!app.preview_send_gain(id,bus,.125f)){full=true;break;}CHECK(full);
    app.cancel_mix_preview();app.engine()->process(nullptr,output.data(),128);app.poll();app.engine()->process(nullptr,output.data(),128);
    CHECK(*app.services().projects->state().project==before_preview&&app.services().projects->state().revision==preview_revision);
    CHECK(app.preview_send_gain(id,bus,.75f));app.set_send_gain(id,0,0.75f);
    CHECK(app.services().projects->state().revision==preview_revision+1);
    CHECK(app.services().projects->state().project->tracks.front().sends[1].gain==.25f);
    CHECK(app.undo()&&*app.services().projects->state().project==before_preview&&app.redo());
    const auto stops=driver->stops;const auto generation=app.insert_generation();
    app.set_send_bypassed(id,0,true);app.engine()->process(nullptr,output.data(),128);
    CHECK(app.services().projects->state().project->tracks.front().sends[0].bypass&&app.services().projects->state().project->tracks.front().sends[0].gain==.75f);
    CHECK(app.undo());app.engine()->process(nullptr,output.data(),128);CHECK(!app.services().projects->state().project->tracks.front().sends[0].bypass);
    CHECK(app.redo());app.engine()->process(nullptr,output.data(),128);
    CHECK(driver->stops==stops&&app.insert_generation()==generation&&app.engine()->state().playback==PlaybackState::playing);
    rejects([&] { app.set_track_sends(id,{}); }); rejects([&] { app.set_track_input(id,0); });
    app.pause(); app.engine()->process(nullptr,output.data(),128);
    const auto file=dir.path/"sends.mrsproject"; app.save_project(file);
    const auto saved=*app.services().projects->state().project; app.open_project(file); CHECK(*app.services().projects->state().project == saved);
    CHECK(saved.tracks.front().input == 1 && saved.tracks.front().sends.front().gain == 0.75f);
    CHECK(saved.tracks.front().sends.front().bypass);
    app.remove_track(bus); CHECK(app.services().projects->state().project->tracks.front().sends.size() == 1);
    CHECK(app.undo() && app.services().projects->state().project->tracks.front().sends.size() == 2);
    Preferences p; for (int n=0; n<12; ++n) remember_project(p,dir.path/(std::to_string(n)+".mrsproject"));
    CHECK(p.recent_projects.size() == 10); const auto last=p.recent_projects.front();
    remember_project(p,dir.path/"11.mrsproject"); CHECK(p.recent_projects.front() == last && p.recent_projects.size() == 10);
    CHECK(decode_preferences(encode_preferences(p)) == p);
    CHECK(decode_preferences("MRS_DESKTOP_CONFIG 2\n0 48000 128 -1 \"\" 2 0 1 0\n").recent_projects.empty());
    rejects([&] { (void)decode_preferences("MRS_DESKTOP_CONFIG 3\n0 48000 128 -1 \"\" 2 0 1 0\n11\n"); });
}
void buses() {
    Directory dir; const auto file = dir.path / "bus.wav"; wav(file,1);
    Application app; app.import_wav(file);
    const auto track = app.services().projects->state().project->tracks.front().id;
    const auto bus = app.add_bus("Drums"), parent = app.add_bus("Band");
    app.set_track_output(track,bus); app.set_track_output(bus,parent);
    app.set_track_mix(bus,{0.5f,0,false,false}); app.set_track_mix(parent,{0.5f,0,false,false});
    auto before = app.services().projects->state();
    rejects([&] { app.set_track_output(parent,bus); });
    rejects([&] { app.arm_track(bus); });
    CHECK(app.services().projects->state().revision == before.revision);
    app.save_project(dir.path / "bus.mrsproject");
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}});
    const auto engine = app.engine(); app.play(); std::array<float,1024> output{}; engine->process(nullptr,output.data(),512);
    CHECK(output[0] == 0.0152587890625f);
    rejects([&] { app.set_track_output(track,std::nullopt); }); rejects([&] { (void)app.add_bus("Blocked"); });
    app.set_track_mix(bus,{0.5f,0,true,false}); engine->process(nullptr,output.data(),512); CHECK(output[1022] == 0);
    CHECK(app.undo()); engine->process(nullptr,output.data(),512); CHECK(output[1022] == 0.0152587890625f);
    app.pause(); engine->process(nullptr,output.data(),128); const auto position = engine->state().sample;
    app.remove_track(bus); CHECK(app.engine() == engine && engine->state().sample == position);
    CHECK(app.services().projects->state().project->tracks.front().output == parent);
    CHECK(app.undo()); CHECK(app.services().projects->state().project->tracks.front().output == bus);
    app.save_project(dir.path / "bus.mrsproject"); const auto snapshot = app.snapshot();
    app.open_project(dir.path / "bus.mrsproject"); CHECK(app.snapshot().project == snapshot.project);
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}}); app.play(); engine->process(nullptr,output.data(),512);
    CHECK(output[0] == 0.0152587890625f);
    app.stop(); engine->process(nullptr,output.data(),128); app.new_project(44100);
    const auto vocal = app.add_audio_track("Raw"), monitor_bus = app.add_bus("Monitor");
    app.set_track_output(vocal,monitor_bus); app.arm_track(vocal);
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{0},{0,1}});
    const auto take = dir.path / "raw-bus.wav"; app.start_recording(take);
    std::array<float,512> input{}; input.fill(0.25f); engine->process(input.data(),output.data(),512);
    CHECK(output[0] == 0.125f); // existing demo master processor has gain 0.5
    app.set_track_mix(monitor_bus,{0,1,true,true}); engine->process(input.data(),output.data(),512);
    CHECK(output[1022] == 0 && output[1023] == 0);
    CHECK(app.stop_recording()); const auto raw = audio::load_wav(take);
    CHECK(raw.frames() == 1024); for (const auto value : raw.samples) CHECK(value == 0.25f);
}
void mixer() {
    Directory dir; const auto file = dir.path / "mix.wav"; wav(file,1);
    Application app; app.import_wav(file);
    const auto track = app.services().projects->state().project->tracks.front().id;
    app.save_project(dir.path / "mix.mrsproject");
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}});
    const auto engine = app.engine(); app.play(); std::array<float,1024> output{};
    engine->process(nullptr,output.data(),128);
    const auto callbacks = engine->metrics().callbacks;
    app.set_track_mix(track,{0.5f,1,false,false}); app.set_master_gain(0.5f);
    CHECK(app.dirty() && app.engine() == engine && engine->metrics().callbacks == callbacks);
    engine->process(nullptr,output.data(),512);
    CHECK(output[1022] == 0 && output[1023] == 0.0152587890625f);
    CHECK(app.undo()); engine->process(nullptr,output.data(),128);
    CHECK(app.services().projects->state().project->master_gain == 1);
    CHECK(app.redo()); engine->process(nullptr,output.data(),128);
    app.stop(); engine->process(nullptr,output.data(),128); app.save_project(dir.path / "mix.mrsproject");
    app.open_project(dir.path / "mix.mrsproject");
    CHECK(!app.dirty()); const auto p = app.services().projects->state().project;
    CHECK(p->tracks.front().mix.gain == 0.5f && p->tracks.front().mix.pan == 1 && p->master_gain == 0.5f);
    const auto revision = app.services().projects->state().revision;
    rejects([&] { app.set_track_mix(track,{1,2,false,false}); });
    rejects([&] { app.set_master_gain(-1); });
    CHECK(app.services().projects->state().revision == revision);
    app.remove_track(track); CHECK(app.undo() && app.services().projects->state().project->tracks.front().mix.gain == 0.5f);
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}});
    // Saturated previews must restore even when commit is a model no-op.
    for (int n=0; n<20; ++n) (void)app.preview_mix(track,{0,0,false,false},0);
    app.cancel_mix_preview(); app.engine()->process(nullptr,output.data(),512); app.poll();
    app.play(); app.engine()->process(nullptr,output.data(),512);
    CHECK(output[1022] == 0 && output[1023] == 0.0152587890625f);
    app.stop(); app.engine()->process(nullptr,output.data(),128);
    app.new_project(44100); const auto vocal = app.add_audio_track("Raw take"); app.arm_track(vocal);
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{0},{0,1}});
    const auto take = dir.path / "raw-mix.wav"; app.start_recording(take);
    std::array<float,512> input{}; input.fill(0.25f);
    app.engine()->process(input.data(),output.data(),512);
    app.set_track_mix(vocal,{0,1,true,true}); app.set_master_gain(0);
    app.engine()->process(input.data(),output.data(),512); CHECK(output[1022] == 0 && output[1023] == 0);
    CHECK(app.stop_recording()); const auto raw = audio::load_wav(take);
    CHECK(raw.frames() == 1024); for (const auto value : raw.samples) CHECK(value == 0.25f);
}

void audio_settings() {
    Application app;
    app.connect(audio::make_offline_device(),{0,48000,128,{}, {0,1}}); CHECK(app.audio_running());
    app.connect(audio::make_offline_device(),{0,48000,64,{0}, {0,1}}); CHECK(app.audio_running() && app.engine()->config().input_channels == 1);
    rejects([&] { app.connect(audio::make_offline_device(),{0,48000,63,{}, {0,1}}); }); CHECK(app.audio_running()); // validate before closing
    rejects([&] { app.connect(audio::make_offline_device(),{0,44100,128,{}, {0,1}}); });
    CHECK(app.audio_running() && app.engine()->config().sample_rate == 48000 && app.engine()->config().input_channels == 1);
    app.connect(audio::make_offline_device(),{0,48000,128,{}, {0,1}}); CHECK(app.audio_running());
    app.disconnect(); CHECK(!app.audio_running() && app.engine()->state().sample == 0);
}
void arrangement() {
    {
        Application channels;channels.new_project(48000);const auto a=channels.add_audio_track("Audio"),b=channels.add_bus("Return");channels.set_track_output(a,b);channels.set_track_sends(a,{{b,1,false}});
        channels.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});const auto original=channels.snapshot().project;const auto generation=channels.insert_generation();
        channels.set_track_color(a,0x36b9b7);CHECK(channels.insert_generation()==generation&&channels.history_preserves_plugins(false));
        const auto colored=channels.snapshot().project;CHECK(colored.clips==original.clips&&colored.tracks.front().color==0x36b9b7u);CHECK(deserialize(serialize(colored))==colored);
        CHECK(channels.undo()&&channels.snapshot().project==original);CHECK(channels.redo()&&channels.snapshot().project==colored);
        const auto revision=channels.services().projects->state().revision;channels.set_track_color(a,0x36b9b7);channels.reorder_track(a,0);CHECK(channels.services().projects->state().revision==revision);
        channels.reorder_track(a,1);auto reordered=colored;std::swap(reordered.tracks[0],reordered.tracks[1]);CHECK(channels.snapshot().project==reordered&&channels.insert_generation()==generation&&channels.history_preserves_plugins(false));
        CHECK(channels.undo()&&channels.snapshot().project==colored&&channels.insert_generation()==generation);CHECK(channels.redo()&&channels.snapshot().project==reordered&&channels.insert_generation()==generation);
        rejects([&]{channels.reorder_track(a,99);});CHECK(channels.snapshot().project==reordered);rejects([&]{channels.set_track_color(a,0x1000000);});
    }
    Directory dir; auto a = dir.path / "one.wav", b = dir.path / "two.wav"; wav(a); wav(b);
    Application app; app.new_project(44100);
    const auto service = app.services().projects; const auto graph = app.graphs(); const auto id = service->state().project->id;
    app.import_wavs({a,b});
    CHECK(service == app.services().projects && graph == app.graphs());
    CHECK(service->state().project->id == id && service->state().project->tracks.size() == 2);
    CHECK(service->state().project->clips.size() == 2);
    CHECK(app.undo() && service->state().project->tracks.empty());
    CHECK(app.redo() && service->state().project->tracks.size() == 2);
    auto before = *service->state().project;
    rejects([&] { app.import_wavs({a,dir.path / "missing.wav"}); });
    CHECK(*service->state().project == before);
    app.reorder_track(before.tracks.front().id,1);
    CHECK(service->state().project->tracks.back().id == before.tracks.front().id);
    app.remove_track(before.tracks.front().id);
    CHECK(service->state().project->tracks.size() == 1 && service->state().project->clips.size() == 1);
    auto saved = dir.path / "arrange.mrsproject"; app.save_project(saved);
    CHECK(persistence::load_project(saved).project.tracks.size() == 1);
    CHECK(app.undo() && service->state().project->tracks.size() == 2);
    CHECK(app.snapshot().mixer.size() == 2);
    auto third = app.add_audio_track("Empty"); CHECK(service->state().project->tracks.back().id == third);
    app.prepare_waveforms();
    const auto source = service->state().project->clips.front().source;
    eventually([&] { app.poll(); return app.waveform(source) != nullptr; });
    CHECK(app.waveform(source)->frames() == 1000);
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}});
    app.remove_track(third); // same opened backend, rebuilt renderer
    CHECK(app.audio_running());
    app.play(); std::array<float,256> output{}; app.engine()->process(nullptr,output.data(),128);
    CHECK(output[0] == 0.1220703125f); // BOTH imported tracks, shared gain 0.5
    rejects([&] { app.add_audio_track("While playing"); });
    rejects([&] { app.undo(); });
    app.stop(); app.engine()->process(nullptr,output.data(),128); app.poll();
    app.save_project(saved); auto snapshot = app.snapshot(); app.open_project(saved);
    CHECK(app.snapshot().project == snapshot.project);
    // Reopen a session whose monitored input/output selectors exceed the offline clock.
    Application session;session.new_project(44100);const auto mic=session.add_audio_track("Stereo input");
    session.connect(std::make_unique<ManualDevice>(),{0,44100,256,{}, {0,1,2,3}});
    session.set_track_input(mic,2,true);session.set_track_monitoring(mic,true);session.set_hardware_output({}, {2,3});
    const auto sessionFile=dir.path/"hardware-session.mrsproject";session.save_project(sessionFile);session.open_project(sessionFile);
    session.connect(audio::make_offline_device(),{0,44100,128,{}, {0,1}});
    CHECK(session.audio_running() && session.audio_name()=="Offline clock (no sound)");
    CHECK(session.services().projects->state().project->tracks.front().input==2);
    CHECK(session.services().projects->state().project->master_outputs==std::vector<int>({2,3}));
    auto reconnected=std::make_unique<ManualDevice>();auto* driver=reconnected.get();
    session.connect(std::move(reconnected),{0,44100,256,{0}, {0,1,2,3}});
    CHECK(session.audio_running() && driver->last_config.inputs==std::vector<int>({2,3}));
    CHECK(driver->last_config.buffer_frames==256 && driver->last_config.outputs==std::vector<int>({0,1,2,3}));
    Application other; other.new_project(48000);other.import_wavs({a});
    CHECK(other.services().projects->state().project->clips.front().length == 1089); // ceil(1000*48000/44100)
}
void nonplaying_edits() {
    Directory dir; auto file = dir.path / "edit.wav"; wav(file);
    Application app; app.new_project(44100);
    app.import_wavs({file});
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}});
    std::array<float,256> output{};
    // Seeking away from zero while stopped must not prevent edits or reset position.
    app.seek(400); app.engine()->process(nullptr,output.data(),128);
    const auto empty = app.add_audio_track("Remove while stopped");
    CHECK(app.engine()->state().sample == 400 && app.engine()->state().playback == PlaybackState::stopped);
    app.remove_track(empty);
    CHECK(app.engine()->state().sample == 400);
    CHECK(app.undo() && app.engine()->state().sample == 400);
    CHECK(app.redo() && app.engine()->state().sample == 400);
    // Pause is an idle transport at a useful edit position, not a playing session.
    app.play(); app.engine()->process(nullptr,output.data(),128); app.pause();
    app.engine()->process(nullptr,output.data(),128);
    const auto paused = app.engine()->state(); CHECK(paused.playback == PlaybackState::paused && paused.sample == 528);
    auto track = app.services().projects->state().project->tracks.front().id;
    app.remove_track(track); CHECK(app.services().projects->state().project->clips.empty());
    CHECK(app.audio_running() && app.engine()->state().sample == paused.sample && app.engine()->state().playback == PlaybackState::paused);
    CHECK(app.undo() && app.services().projects->state().project->clips.size() == 1);
    CHECK(app.engine()->state().sample == paused.sample && app.engine()->state().playback == PlaybackState::paused);
    app.import_wavs({file}); CHECK(app.engine()->state().sample == paused.sample && app.engine()->state().playback == PlaybackState::paused);
    app.engine()->process(nullptr,output.data(),128);
    CHECK(output[0] == 0 && app.engine()->state().sample == paused.sample); // no accidental restart
    app.play(); app.engine()->process(nullptr,output.data(),128);
    CHECK(output[0] == 0.1220703125f && app.engine()->state().sample == paused.sample+128);
    rejects([&] { app.remove_track(track); }); // actual playback remains protected
    app.disconnect();
    // Invalid quiescent state is rejected atomically; prepare never restores Play.
    audio::AudioEngine engine;
    engine.prepare({}, {}, {PlaybackState::paused,123,LoopRange{100,400}});
    CHECK(engine.state().sample == 123 && engine.state().loop == std::optional<LoopRange>{{100,400}});
    rejects([&] { engine.prepare({}, {}, {PlaybackState::playing,100,{}}); });
    rejects([&] { engine.prepare({}, {}, {PlaybackState::stopped,-1,{}}); });
    rejects([&] { engine.prepare({}, {}, {PlaybackState::stopped,0,LoopRange{400,100}}); });
    CHECK(engine.state().sample == 123 && engine.state().playback == PlaybackState::paused);
}
void backlog150(){
    Directory dir;auto file=dir.path/"Group.wav";wav(file);Application app;app.new_project(44100);app.import_wavs({file});const auto audio=app.services().projects->state().project->clips.front().id;
    const auto track=app.add_instrument_track("Group MIDI");const auto midi=app.create_midi_clip(track,0,4*ppq);app.set_midi_notes(midi,{{new_id(),0,4*ppq,60,90,0}});
    auto state=[&]{return *app.services().projects->state().project;};auto before=state();
    app.set_clip_colors({audio,midi},0xF15E62);auto colored=state();CHECK(colored.clips[0].color==0xF15E62u&&colored.clips[1].color==0xF15E62u);CHECK(deserialize(serialize(colored))==colored);CHECK(app.history_preserves_plugins(false));CHECK(app.undo()&&state()==before);CHECK(app.redo()&&state()==colored);rejects([&]{app.set_clip_colors({audio},0x1000000);});CHECK(state()==colored);
    auto legacy=serialize(colored);for(auto at=legacy.find("TRACKCOLOR ");at!=std::string::npos;at=legacy.find("TRACKCOLOR "))legacy.erase(at,legacy.find('\n',at)-at+1);for(auto at=legacy.find("AUDIOFADE ");at!=std::string::npos;at=legacy.find("AUDIOFADE "))legacy.erase(at,legacy.find('\n',at)-at+1);for(auto at=legacy.find("CLIPCOLOR ");at!=std::string::npos;at=legacy.find("CLIPCOLOR "))legacy.erase(at,legacy.find('\n',at)-at+1);legacy.replace(0,std::string("MRS_CORE_SNAPSHOT 22").size(),"MRS_CORE_SNAPSHOT 21");const auto old=deserialize(legacy);CHECK(!old.clips[0].color&&!old.clips[1].color);
    app.trim_clips(colored.clips,-100,false);auto resized=state();CHECK(resized.clips[0].length==colored.clips[0].length-100&&resized.clips[1].midi->length<colored.clips[1].midi->length);CHECK(app.undo()&&state()==colored);CHECK(app.redo()&&state()==resized);
    rejects([&]{app.trim_clips(colored.clips,-20,false);});CHECK(state()==resized);rejects([&]{app.trim_clips(resized.clips,-100000000,false);});CHECK(state()==resized);
    const auto rights=app.split_clips({audio,midi},300);auto split=state();CHECK(rights.size()==2&&split.clips.size()==4);CHECK(split.clips[2].color==0xF15E62u&&split.clips[3].color==0xF15E62u);CHECK(app.undo()&&state()==resized);CHECK(app.redo()&&state()==split);rejects([&]{app.split_clips({audio},0);});CHECK(state()==split);
    app.save_project(dir.path/"Group.mrsproject");const auto saved=app.snapshot().project;app.open_project(dir.path/"Group.mrsproject");CHECK(state()==saved);
    const auto section=app.add_section("Verse",0,4*ppq,0x845CFF);const auto chord=app.add_chord("Em",0,4*ppq);auto lanes=state();app.split_sections({lanes.sections.front()},2*ppq);auto sectionSplit=state();CHECK(sectionSplit.sections.size()==2&&sectionSplit.sections[0].id==section&&sectionSplit.sections[0].end==2*ppq&&sectionSplit.sections[1].start==2*ppq&&sectionSplit.sections[1].color==0x845CFFu);CHECK(app.undo()&&state()==lanes);CHECK(app.redo()&&state()==sectionSplit);
    app.split_chords({lanes.chords.front()},ppq);auto chordSplit=state();CHECK(chordSplit.chords.size()==2&&chordSplit.chords[0].id==chord&&chordSplit.chords[1].symbol=="Em");CHECK(app.undo()&&state()==sectionSplit);CHECK(app.redo()&&state()==chordSplit);rejects([&]{app.split_chords({lanes.chords.front()},0);});CHECK(state()==chordSplit);
}
void clip_edits() {
    {Directory fadeDir;const auto sourceFile=fadeDir.path/"Fades.wav";wav(sourceFile);Application faded;faded.new_project(44100);faded.import_wavs({sourceFile});const auto first=faded.services().projects->state().project->clips.front();const auto second=faded.duplicate_clip(first.id);
        faded.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}});const auto generation=faded.insert_generation();
        std::array<float,256> baselineFade{},scratchFade{},renderedFade{};faded.play();faded.engine()->process(nullptr,baselineFade.data(),128);faded.stop();faded.engine()->process(nullptr,scratchFade.data(),128);
        faded.set_clip_fades(first,32,64);CHECK(faded.services().projects->state().project->clips.front().fade_out==64&&faded.insert_generation()==generation);faded.play();faded.engine()->process(nullptr,renderedFade.data(),128);CHECK(baselineFade[0]!=0&&renderedFade[0]==0&&std::abs(renderedFade[62]-baselineFade[62])<1e-6f);faded.stop();faded.engine()->process(nullptr,renderedFade.data(),128);CHECK(faded.history_preserves_plugins(false)&&faded.undo());CHECK(faded.redo());
        rejects([&]{faded.set_clip_fades(first,2,3);});const auto current=faded.services().projects->state().project->clips.front();const auto revision=faded.services().projects->state().revision;faded.set_clip_fades(current,32,64);CHECK(faded.services().projects->state().revision==revision);
        faded.rename_clips({second,first.id},"Verse");CHECK(faded.services().projects->state().project->clips.front().name=="Verse 01"&&faded.services().projects->state().project->clips.back().name=="Verse 02");CHECK(faded.history_preserves_plugins(false)&&faded.undo()&&faded.insert_generation()==generation);CHECK(faded.redo());
        faded.save_project(fadeDir.path/"Fades.mrsproject");const auto saved=faded.snapshot().project;faded.open_project(fadeDir.path/"Fades.mrsproject");CHECK(*faded.services().projects->state().project==saved);const auto raw=audio::load_wav(sourceFile);CHECK(raw.frames()==first.length);
    }

    backlog150();
    Directory dir; auto file = dir.path / "source.wav"; wav(file);
    Application app; app.new_project(44100); app.import_wavs({file});
    const auto projects = app.services().projects;
    const auto original = projects->state().project->clips.front();
    const auto destination = app.add_audio_track("Destination");
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}});
    app.seek(700); std::array<float,256> output{}; app.engine()->process(nullptr,output.data(),128);
    app.move_clip(original.id,destination,200);
    auto clip = projects->state().project->clips.front();
    CHECK(clip.start == 200 && clip.track == destination && clip.source == original.source && clip.source_offset == 0);
    CHECK(app.engine()->state().sample == 700);
    const Timeline copyTime(projects->state().project->time,44100);
    const auto copied=app.duplicate_clip(original.id);
    const auto copiedAudio=projects->state().project->clips.back();
    CHECK(copiedAudio.id==copied&&copiedAudio.track==destination&&copiedAudio.start==copyTime.to_samples(4*ppq)+200);
    CHECK(copiedAudio.source==original.source&&copiedAudio.length==original.length);
    app.seek(copiedAudio.start);app.play();app.engine()->process(nullptr,output.data(),128);
    CHECK(output[0]==0.06103515625f);app.pause();app.engine()->process(nullptr,output.data(),128);
    CHECK(app.undo()&&projects->state().project->clips.size()==1);CHECK(app.redo()&&projects->state().project->clips.back()==copiedAudio);CHECK(app.undo());
    const auto midiTrack=app.add_instrument_track("Copy keys");const auto midi=app.create_midi_clip(midiTrack,ppq/3,ppq);
    app.set_midi_notes(midi,{{new_id(),0,ppq/2,67,90,3}});
    const auto copyBefore=*projects->state().project;
    const auto group=app.copy_clips({original.id,midi},copyTime.to_samples(4*ppq));
    const auto copyAfter=*projects->state().project;CHECK(group.size()==2&&copyAfter.clips.size()==4);
    CHECK(copyAfter.clips[2].start==200+copyTime.to_samples(4*ppq));
    CHECK(copyAfter.clips[3].midi->start==4*ppq+ppq/3&&copyAfter.clips[3].midi->notes.front().id!=copyBefore.clips[1].midi->notes.front().id);
    CHECK(deserialize(serialize(copyAfter))==copyAfter);CHECK(app.undo()&&*projects->state().project==copyBefore);CHECK(app.redo()&&*projects->state().project==copyAfter);CHECK(app.undo());
    rejects([&]{app.copy_clips({original.id,midi},-1000);});rejects([&]{app.copy_clips({original.id,midi},100,1);});rejects([&]{app.copy_clips({original.id,original.id},100);});CHECK(*projects->state().project==copyBefore);
    CHECK(app.undo());CHECK(app.undo());CHECK(app.undo()); // notes, MIDI clip, track
    app.trim_clip(original.id,300,1000);
    clip = projects->state().project->clips.front();
    CHECK(clip.start == 300 && clip.length == 700 && clip.source_offset == 100);
    auto right = app.split_clip(original.id,600);
    const auto split = projects->state().project;
    CHECK(split->clips.size() == 2 && split->clips[0].length == 300);
    CHECK(split->clips[1].id == right && split->clips[1].start == 600 && split->clips[1].length == 400 && split->clips[1].source_offset == 400);
    CHECK(split->clips[1].source == original.source && split->clips[1].track == destination);
    // Boundary playback: no gap/double amplitude at a split, silence outside trim.
    app.seek(599); app.play(); app.engine()->process(nullptr,output.data(),128);
    for (auto value : output) CHECK(value == 0.06103515625f);
    app.pause(); app.engine()->process(nullptr,output.data(),128);
    const auto position = app.engine()->state().sample;
    app.remove_clip(right); CHECK(projects->state().project->clips.size() == 1);
    CHECK(app.engine()->state().playback == PlaybackState::paused && app.engine()->state().sample == position);
    CHECK(app.undo() && projects->state().project->clips.size() == 2);
    CHECK(app.undo() && projects->state().project->clips.size() == 1 && projects->state().project->clips.front().length == 700);
    CHECK(app.redo() && projects->state().project->clips.back().id == right);
    auto before = *projects->state().project;
    for (auto sample : {Sample{300},Sample{600},Sample{-1}}) rejects([&] { app.split_clip(original.id,sample); });
    rejects([&] { app.trim_clip(original.id,0,1000); });
    rejects([&] { app.trim_clip(original.id,300,5000); });
    rejects([&] { app.move_clip(original.id,{"missing"},0); });
    CHECK(*projects->state().project == before);
    app.trim_clip(right,600,1200); // recover hidden source at right end
    CHECK(projects->state().project->clips.back().length == 600 && projects->state().project->clips.back().source_offset == 400);
    app.seek(1199); app.play(); app.engine()->process(nullptr,output.data(),128);
    CHECK(output[0] == 0.06103515625f && output[2] == 0);
    app.pause(); app.engine()->process(nullptr,output.data(),128);
    app.save_project(dir.path / "clips.mrsproject"); auto saved = app.snapshot();
    app.open_project(dir.path / "clips.mrsproject"); CHECK(app.snapshot().project == saved.project);
    CHECK(audio::load_wav(file).frames() == 1000);
    Project p = saved.project; p.time = TimeMap{}; // explicit 120 BPM, not demo's 72 BPM
    CHECK(snap_to_grid(p,5513) == 5513);
    CHECK(snap_to_grid(p,5000) == 5513);
    p.time.tempos = {{0,60},{ppq,120}};
    CHECK(snap_to_grid(p,10000) == 11025);
    CHECK(snap_to_grid(p,49000) == 49613);
}
void waveform() {
    audio::AudioData data{48000,2,std::vector<float>(2050,0)};
    data.samples[600] = 0.75f; data.samples[601] = -0.9f;
    data.samples[2048] = -0.4f; data.samples[2049] = 0.3f;
    audio::Waveform peaks(data);
    CHECK(peaks.channels() == 2 && peaks.frames() == 1025);
    auto l = peaks.range(0,1025,0), r = peaks.range(0,1025,1);
    CHECK(l.maximum == 0.75f && l.minimum == -0.4f);
    CHECK(r.minimum == -0.9f && r.maximum == 0.3f);
    CHECK(peaks.range(1024,1025,0).minimum == -0.4f);
    CHECK(peaks.range(0,0,0).maximum == 0);
    CHECK(peaks.range(-10,99999,1).minimum == -0.9f);
    rejects([&] { (void)peaks.range(0,1,2); });
    rejects([&] { (void)peaks.display(0,1,2); });
    CHECK(peaks.display(0,0,0).maximum==0);
    CHECK(peaks.display(9999,10000,0).maximum==0);
    audio::AudioData ramp{48000,1,std::vector<float>(32)};
    std::fill(ramp.samples.begin()+16,ramp.samples.end(),1.f);
    audio::Waveform smooth(ramp);
    float previous=-1;bool fractional=false;
    for(int x=0;x<64;++x){const auto p=smooth.display(x*.5,x*.5+.5,0);CHECK(p.minimum>=0&&p.maximum<=1&&p.minimum==p.maximum&&p.maximum>=previous);previous=p.maximum;fractional|=p.maximum>0&&p.maximum<1;}
    CHECK(fractional);CHECK(smooth.display(0,32,0).maximum==1&&smooth.display(0,32,0).minimum==0);
    CHECK(smooth.range(16,17,0).maximum==1); // smoothing never changes conservative extrema
}
void streaming() {
    Directory dir; const auto file = dir.path / "long.wav";
    constexpr std::uint32_t frames = 40'000'003;
    {
        std::ofstream out(file,std::ios::binary);
        const auto u16 = [&](std::uint16_t n) { for (int i=0; i<2; ++i) out.put(static_cast<char>((n>>(8*i))&255)); };
        const auto u32 = [&](std::uint32_t n) { for (int i=0; i<4; ++i) out.put(static_cast<char>((n>>(8*i))&255)); };
        out.write("RIFF",4); u32(36+frames*4); out.write("WAVEfmt ",8); u32(16);
        u16(1); u16(2); u32(44100); u32(176400); u16(4); u16(16);
        out.write("data",4); u32(frames*4);
        out.seekp(static_cast<std::streamoff>(44)+frames*4-1); out.put(0);
    }
    const auto source = audio::open_wav(file);
    CHECK(source.file && source.samples.empty() && source.frames() == frames);
    rejects([&] { (void)audio::load_wav(file); }); // exceeds old decoded preload cap
    Application app; app.new_project(44100);
    const auto revision = app.services().projects->state().revision;
    rejects([&] { app.import_wavs(std::vector<std::filesystem::path>(33,file)); });
    CHECK(app.services().projects->state().revision == revision && app.services().projects->state().project->clips.empty());
    app.import_wavs({file});
    const auto p = app.services().projects->state().project;
    CHECK(p->clips.size() == 1 && p->clips.front().length == frames);
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}});
    app.seek(frames-1000);
    std::array<float,256> out{};
    app.engine()->process(nullptr,out.data(),128);
    app.play(); app.engine()->process(nullptr,out.data(),128);
    app.pause(); app.engine()->process(nullptr,out.data(),128);
    const auto position = app.engine()->state().sample;
    CHECK(position == frames-872);
    const auto clip = p->clips.front().id;
    app.trim_clip(clip,100,frames-100);
    CHECK(app.engine()->state().sample == position && app.engine()->state().playback == PlaybackState::paused);
    CHECK(app.undo() && app.source_frames(clip) == frames);
    const auto saved = dir.path / "long.mrsproject"; app.save_project(saved); app.open_project(saved);
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}});
    CHECK(app.services().projects->state().project->clips.front().length == frames);
    CHECK(app.engine()->metrics().disk_underruns == 0);
    app.new_project(); // cancels the long waveform scan before clearing retained media
    CHECK(app.services().projects->state().project->clips.empty());
}


void recording() {
    { // First automatic save while playing also manages imported media.
        Directory imported;const auto source=imported.path/"External.wav";wav(source);
        Application a;a.new_project(44100);a.import_wavs({source});a.arm_track(a.services().projects->state().project->tracks.front().id);
        a.connect(std::make_unique<ManualDevice>(),{0,44100,128,{0},{0,1}});
        std::array<float,128> in;in.fill(.125f);std::array<float,256> out{};
        a.play();a.engine()->process(in.data(),out.data(),128);const auto generation=a.insert_generation();
        a.start_project_recording(imported.path/"Projects");CHECK(a.recording()&&a.insert_generation()==generation);
        CHECK(persistence::load_project(a.path()).project.clips.front().source.starts_with("Media/"));
        a.engine()->process(in.data(),out.data(),128);CHECK(a.stop_recording()&&a.insert_generation()==generation);a.disconnect();
    }
    { // Managed destinations: no chooser, never overwrite, raw input, portable references.
        Directory managed;Application a;a.new_project(44100);
        const auto one=a.add_audio_track("One"),two=a.add_audio_track("Two");
        a.set_track_armed(one,true);a.set_track_armed(two,true);
        rejects([&]{a.start_project_recording(managed.path/"Projects");});
        CHECK(a.path().empty()&&!std::filesystem::exists(managed.path/"Projects"));
        a.connect(std::make_unique<ManualDevice>(),{0,44100,128,{0},{0,1}});
        std::array<float,128> in;in.fill(.25f);std::array<float,256> out{};
        a.play();a.engine()->process(in.data(),out.data(),128);const auto start=a.engine()->state().sample;
        a.start_project_recording(managed.path/"Projects");CHECK(a.recording()&&!a.path().empty());
        const auto file=a.path();CHECK(std::filesystem::exists(file));
        a.engine()->process(in.data(),out.data(),128);CHECK(a.stop_recording());
        CHECK(a.services().projects->state().project->clips.size()==2);
        const auto takes=a.last_takes();CHECK(takes.size()==2&&takes[0]!=takes[1]);
        for(const auto& take:takes){CHECK(take.parent_path()==file.parent_path()/"Media");const auto raw=audio::open_wav(take);CHECK(raw.frames()==128&&raw.samples.front()==.25f);}
        CHECK(a.services().projects->state().project->clips.front().start==start);
        CHECK(a.undo()&&a.services().projects->state().project->clips.empty());
        CHECK(std::filesystem::exists(takes[0])&&a.redo());
        a.start_project_recording(managed.path/"unused");a.engine()->process(in.data(),out.data(),128);CHECK(a.stop_recording());
        CHECK(a.path()==file&&a.last_takes()[0]!=takes[0]&&!std::filesystem::exists(managed.path/"unused"));
        CHECK(audio::open_wav(takes[0]).samples.front()==.25f);
        a.save_project(file);const auto saved=persistence::load_project(file);
        for(const auto& c:saved.project.clips)CHECK(c.source.starts_with("Media/")&&!std::filesystem::path(c.source).is_absolute());
        a.open_project(file);CHECK(a.services().projects->state().project->clips.size()==4);a.disconnect();
    }
    Directory dir; Application app; app.new_project(44100);
    const auto backing = dir.path/"Backing.wav"; wav(backing);
    app.import_wavs({backing});
    const auto armed = app.add_audio_track("Vocal");
    const auto project_file = dir.path/std::filesystem::path(std::u8string(u8"Запись.mrsproject"));
    app.save_project(project_file);
    rejects([&] { app.start_recording(dir.path/"No-arm.wav"); });
    app.arm_track(armed);
    rejects([&] { app.start_recording(dir.path/"Offline.wav"); });
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{0},{0,1,2,3}});
    std::array<float,128> input{}; input.fill(0.25f);
    std::array<float,512> output{};
    app.seek(500); app.engine()->process(input.data(),output.data(),128);
    CHECK(output[0] == 0.125f && output[1] == 0.125f && output[2] == 0 && output[3] == 0);
    const auto project = app.services().projects;
    const auto revision = project->state().revision;
    const auto take = dir.path/"Take.wav";
    app.start_recording(take); CHECK(app.recording());
    app.engine()->process(input.data(),output.data(),128);
    CHECK(output[0] == 0.18603515625f && output[1] == 0.18603515625f); // backing + monitor through gain
    app.monitoring(false); app.engine()->process(input.data(),output.data(),128);
    CHECK(output[0] == 0.06103515625f); // backing stays audible, input still captured
    rejects([&] { app.new_project(); }); rejects([&] { app.save_project(project_file); });
    rejects([&] { app.disconnect(); }); rejects([&] { app.seek(0); });
    rejects([&] { app.undo(); }); rejects([&] { app.arm_track({}); });
    rejects([&] { app.remove_track(armed); }); rejects([&] { app.rename_track(armed,"During rec"); });
    CHECK(project->state().revision == revision && app.recording());
    CHECK(app.stop_recording() && !app.recording() && app.audio_running());
    CHECK(project->state().revision == revision+1 && app.dirty());
    CHECK(project->state().project->tracks.size() == 2 && project->state().project->clips.size() == 2);
    const auto clip = project->state().project->clips.back();
    CHECK(clip.track == armed && clip.start == 500 && clip.length == 256 && clip.source_offset == 0);
    CHECK(app.engine()->state().sample == 756 && app.engine()->state().playback == PlaybackState::paused);
    const auto raw = audio::load_wav(take); CHECK(raw.channels == 1 && raw.frames() == 256);
    for (auto value : raw.samples) CHECK(value == 0.25f);
    CHECK(app.recording_status().frames == 256 && app.recording_error().empty());
    CHECK(app.undo() && project->state().project->clips.size() == 1 && std::filesystem::exists(take));
    CHECK(app.redo() && project->state().project->clips.back() == clip);
    // Integrated import/edit/record/processor state/unknown-chunk save/load.
    const auto imported = project->state().project->clips.front().id;
    app.trim_clip(imported,100,900);
    auto snapshot = app.snapshot(); snapshot.extensions = {{"FUTR","preserve with take"}};
    persistence::save_project(project_file,snapshot);
    app.open_project(project_file);
    CHECK(app.snapshot().project == snapshot.project && app.snapshot().extensions == snapshot.extensions);
    CHECK(app.snapshot().graph == snapshot.graph);
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{0},{0,1,2,3}});
    app.seek(500); app.engine()->process(input.data(),output.data(),128);
    app.play(); app.engine()->process(input.data(),output.data(),128);
    CHECK(output[0] == 0.18603515625f && output[1] == output[0]); // recorded mono centered with backing
    CHECK(output[2] == 0 && output[3] == 0);
    app.pause(); app.engine()->process(input.data(),output.data(),128);
    app.save_project(project_file); CHECK(!app.dirty());
    app.arm_track(armed); app.start_recording(dir.path/"Dropout.wav");
    app.engine()->process(input.data(),output.data(),128);
    app.engine()->process(nullptr,output.data(),128);
    app.poll();
    CHECK(!app.recording() && !app.recording_error().empty() && app.audio_running());
    CHECK(app.recording_status().missing_blocks == 1);
    // project pointer was replaced by Open, so inspect current store.
    CHECK(app.services().projects->state().project->clips.back().length == 128);
    CHECK(audio::load_wav(dir.path/"Dropout.wav").frames() == 128);
    const auto count = app.services().projects->state().project->clips.size();
    app.start_recording(dir.path/"Empty.wav"); CHECK(!app.stop_recording());
    CHECK(app.services().projects->state().project->clips.size() == count && !std::filesystem::exists(dir.path/"Empty.wav"));
    app.services().transport->set_loop(LoopRange{0,2000});
    app.engine()->process(input.data(),output.data(),128);
    rejects([&] { app.start_recording(dir.path/"Loop.wav"); });
    app.services().transport->set_loop({}); app.engine()->process(input.data(),output.data(),128);
    const auto stop_start=app.engine()->state().sample;
    app.start_recording(dir.path/"Stop.wav"); app.engine()->process(input.data(),output.data(),128);
    app.stop(); CHECK(!app.recording() && app.services().projects->state().project->clips.size() == count+1);
    app.engine()->process(input.data(),output.data(),128); CHECK(app.engine()->state().sample == stop_start);
    app.remove_track(armed); CHECK(!app.armed_track()); CHECK(app.undo());
    app.arm_track(armed); app.redo(); CHECK(!app.armed_track());
    CHECK(app.undo()); app.arm_track(armed);
    auto broken = std::make_unique<ManualDevice>(); auto* driver = broken.get();
    app.connect(std::move(broken),{0,44100,128,{0},{0,1}});
    app.start_recording(dir.path/"Driver-failure.wav");
    app.engine()->process(input.data(),output.data(),128);
    driver->fail_driver(); app.poll();
    CHECK(!app.recording() && !app.audio_running() && app.audio_name() == "Disconnected");
    CHECK(!app.recording_error().empty() && audio::load_wav(dir.path/"Driver-failure.wav").frames() == 128);
    CHECK(app.services().projects->state().project->clips.back().length == 128);
    {
        // Lost streamed backing must not prevent finalization of the captured take.
        const auto missing = dir.path/"Streamed-backing.wav";
        constexpr std::uint32_t frames = 3'000'000;
        {
            std::ofstream out(missing,std::ios::binary);
            const auto u16 = [&](std::uint16_t n) { for (int i=0; i<2; ++i) out.put(static_cast<char>((n>>(8*i))&255)); };
            const auto u32 = [&](std::uint32_t n) { for (int i=0; i<4; ++i) out.put(static_cast<char>((n>>(8*i))&255)); };
            out.write("RIFF",4); u32(36+frames*4); out.write("WAVEfmt ",8); u32(16);
            u16(3); u16(1); u32(44100); u32(176400); u16(4); u16(32);
            out.write("data",4); u32(frames*4);
            out.seekp(static_cast<std::streamoff>(44)+frames*4-1); out.put(0);
        }
        Application failure; failure.new_project(44100); failure.import_wavs({missing});
        const auto source = failure.services().projects->state().project->clips.front().source;
        eventually([&] { failure.poll(); return failure.waveform(source) != nullptr; });
        const auto target = failure.add_audio_track("Captured");
        failure.arm_track(target);
        failure.connect(std::make_unique<ManualDevice>(),{0,44100,128,{0},{0,1}});
        const auto final = dir.path/"Preserved-take.wav";
        failure.start_recording(final); failure.engine()->process(input.data(),output.data(),128);
        CHECK(std::filesystem::remove(missing));
        rejects([&] { (void)failure.stop_recording(); });
        CHECK(!failure.recording() && !failure.audio_running() && !failure.recording_error().empty());
        CHECK(failure.last_take() == std::filesystem::absolute(final) && audio::load_wav(final).frames() == 128);
        CHECK(failure.services().projects->state().project->clips.back().track == target);
    }
}


std::size_t file_count(const std::filesystem::path& folder) {
    std::size_t count{}; for (const auto& entry : std::filesystem::recursive_directory_iterator(folder)) if (entry.is_regular_file()) ++count;
    return count;
}
void project_folders() {
    Directory dir, originals; StudioFolders studio{dir.path/"MR Studio"}; studio.ensure();
    CHECK(std::filesystem::is_directory(studio.projects()) && std::filesystem::is_directory(studio.lives()));
    const auto name = std::filesystem::path(std::u8string(u8"Песня"));
    const auto file = project_folder_file(studio.projects()/(name.native()+std::filesystem::path(".mrsproject").native()));
    CHECK(file.parent_path() == studio.projects()/name && file.filename().stem() == name);
    CHECK(project_folder_file(file) == file);
    Application app; app.demo(); app.new_project(44100,"Song");
    CHECK(app.services().projects->state().project->time == TimeMap{});
    const Timeline blankTime(app.services().projects->state().project->time,44100);
    CHECK(blankTime.to_samples(4*ppq)==88200 && blankTime.to_samples(8*ppq)==176400);
    app.save_project(file);
    CHECK(persistence::load_project(file).project.time == TimeMap{});
    CHECK(std::filesystem::exists(file) && std::filesystem::is_directory(file.parent_path()/"Media") && std::filesystem::is_directory(file.parent_path()/"Mixdown"));
    std::filesystem::create_directory(originals.path/"A"); std::filesystem::create_directory(originals.path/"B");
    const auto a = originals.path/"A"/"same.wav", b = originals.path/"B"/"same.wav"; wav(a,1); wav(b);
    app.import_wavs({a,b,a});
    const auto clips = app.services().projects->state().project->clips;
    CHECK(clips.size() == 3 && clips[0].source == clips[2].source && clips[0].source != clips[1].source);
    CHECK(file_count(file.parent_path()/"Media") == 2 && std::filesystem::exists(a) && std::filesystem::exists(b));
    const auto count = file_count(file.parent_path()/"Media");
    rejects([&] { app.import_wavs({a,originals.path/"missing.wav"}); });
    CHECK(file_count(file.parent_path()/"Media") == count && app.services().projects->state().project->clips.size() == 3);
    app.save_project(file);
    const auto saved = persistence::load_project(file);
    for (const auto& clip : saved.project.clips) {
        const auto source = std::filesystem::path(std::u8string(clip.source.begin(),clip.source.end()));
        CHECK(source.is_relative() && *source.begin() == "Media" && std::filesystem::exists(file.parent_path()/source));
    }
    CHECK(std::filesystem::remove(a) && std::filesystem::remove(b)); // imported media is independent
    std::filesystem::copy_file(file.parent_path()/std::filesystem::path(std::u8string(clips[0].source.begin(),clips[0].source.end())),file.parent_path()/"Media"/"Unused.wav");
    { std::ofstream export_file(file.parent_path()/"Mixdown"/"render.txt"); export_file << "future export content"; }
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}});
    std::array<float,256> output{};
    app.seek(600); app.engine()->process(nullptr,output.data(),128);
    app.play(); app.engine()->process(nullptr,output.data(),128);
    app.save_project(file); CHECK(app.engine()->state().playback == PlaybackState::playing); // ordinary save remains possible while playing
    app.pause(); app.engine()->process(nullptr,output.data(),128);
    const auto position = app.engine()->state().sample;
    const auto copy = project_folder_file(studio.projects()/"Copy.mrsproject");
    app.save_project(copy);
    CHECK(app.path() == copy && app.audio_running() && app.engine()->state().sample == position && app.engine()->state().playback == PlaybackState::paused);
    CHECK(file_count(copy.parent_path()/"Media") == 3 && std::filesystem::exists(copy.parent_path()/"Mixdown"/"render.txt"));
    CHECK(app.undo() && app.services().projects->state().project->clips.empty());
    CHECK(app.redo() && app.services().projects->state().project->clips.size() == 3);
    app.save_project(copy); const auto snapshot = app.snapshot();
    app.new_project(44100); // close device/workers before moving folders
    const auto moved = dir.path/"Portable";
    std::filesystem::rename(copy.parent_path(),moved);
    app.open_project(moved/copy.filename()); CHECK(app.snapshot().project == snapshot.project);
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}}); app.play();
    app.engine()->process(nullptr,output.data(),128);
    CHECK(output[0] == 0.18310546875f && output[1] == output[0]);
    app.pause(); app.engine()->process(nullptr,output.data(),128);
    const auto foreign = project_folder_file(studio.projects()/"Foreign.mrsproject");
    Application other; other.save_project(foreign);
    const auto identity = persistence::load_project(foreign).project.id;
    rejects([&] { app.save_project(foreign); });
    CHECK(app.path() == moved/copy.filename() && persistence::load_project(foreign).project.id == identity);

    // Upgrade an external long source, then Save As again without the original.
    const auto long_file = originals.path/"Long.wav";
    constexpr std::uint32_t frames = 3'000'000;
    {
        std::ofstream out(long_file,std::ios::binary);
        const auto u16 = [&](std::uint16_t n) { for (int i=0; i<2; ++i) out.put(static_cast<char>((n>>(8*i))&255)); };
        const auto u32 = [&](std::uint32_t n) { for (int i=0; i<4; ++i) out.put(static_cast<char>((n>>(8*i))&255)); };
        out.write("RIFF",4); u32(36+frames*4); out.write("WAVEfmt ",8); u32(16);
        u16(3); u16(1); u32(44100); u32(176400); u16(4); u16(32);
        out.write("data",4); u32(frames*4); u32(0x3e800000); // first frame 0.25f
        out.seekp(static_cast<std::streamoff>(44)+frames*4-1); out.put(0);
    }
    Application legacy; legacy.import_wav(long_file);
    const auto legacy_source = legacy.services().projects->state().project->clips.front().source;
    const auto long_project = project_folder_file(studio.projects()/"Long.mrsproject");
    legacy.save_project(long_project);
    CHECK(persistence::load_project(long_project).project.clips.front().source.starts_with("Media/"));
    CHECK(std::filesystem::remove(long_file));
    legacy.connect(std::make_unique<ManualDevice>(),{0,44100,128,{}, {0,1}});
    legacy.play(); legacy.engine()->process(nullptr,output.data(),128); CHECK(output[0] == 0.125f);
    legacy.pause(); legacy.engine()->process(nullptr,output.data(),128);
    const auto long_copy = project_folder_file(studio.projects()/"Long-copy.mrsproject");
    legacy.save_project(long_copy);
    CHECK(legacy.services().projects->state().project->clips.front().source == legacy_source); // stable source aliases/Undo
    legacy.seek(0); legacy.play(); legacy.engine()->process(nullptr,output.data(),128); CHECK(output[0] == 0.125f);
    legacy.pause(); legacy.engine()->process(nullptr,output.data(),128);
    legacy.open_project(long_copy); CHECK(legacy.snapshot().project == persistence::load_project(long_copy).project);
}

void config() {
    Preferences p; p.workspace = Workspace::live; p.device_name = "Komplete Audio ASIO Driver"; p.reconnect_audio = true;
    p.click={true,true,false,35,2};
    p.process_buffer_frames=1024;
    CHECK(decode_preferences(encode_preferences(p)) == p);
    auto disabled = p; disabled.reconnect_audio = false;
    CHECK(decode_preferences(encode_preferences(disabled)) == disabled);
    const auto legacy = decode_preferences("MRS_DESKTOP_CONFIG 1\n0 48000 128 -1 \"Komplete Audio ASIO Driver\" 2 0 1\n");
    CHECK(legacy.reconnect_audio && legacy.device_name == p.device_name);
    CHECK(legacy.process_buffer_frames==0);
    CHECK(legacy.click==audio::ClickSettings{});auto bad_click=p;bad_click.click.count_bars=5;rejects([&]{encode_preferences(bad_click);});bad_click=p;bad_click.click.level=-1;rejects([&]{encode_preferences(bad_click);});
    auto invalid_process=p;invalid_process.process_buffer_frames=64;rejects([&]{(void)encode_preferences(invalid_process);});
    CHECK(!decode_preferences("MRS_DESKTOP_CONFIG 1\n0 48000 128 -1 \"\" 2 0 1\n").reconnect_audio);
    rejects([&] { (void)decode_preferences("MRS_DESKTOP_CONFIG 2\n0 48000 128 -1 \"\" 2 0 1 2\n"); });
    CHECK(parse_outputs("1, 2,6") == std::vector<int>({0,1,5}));
    for (auto text : {"","1,","0","1,1","65","x","1,,2","-1"}) rejects([&] { (void)parse_outputs(text); });
    rejects([&] { (void)decode_preferences("MRS_DESKTOP_CONFIG 2"); });
    rejects([&] { (void)decode_preferences(encode_preferences(p)+"extra"); });
    auto bad = p; bad.outputs = {0,0}; rejects([&] { (void)encode_preferences(bad); });
    Directory dir; Logger log(dir.path / "app.log"); log.write("control-thread log");
}


void inserts() {
    {Application app;app.new_project(48000);const auto a=app.add_audio_track("A"),b=app.add_bus("B");
        NativeInsert first{new_id(),InsertKind::gain,.5f},second{new_id(),InsertKind::gain,.25f};second.bypass=true;
        app.set_inserts(a,{first,second});app.connect(std::make_unique<ManualDevice>(),{0,48000,128,{}, {0,1}});
        const auto generation=app.insert_generation();const auto revision=app.services().projects->state().revision;
        app.transfer_insert(a,first.id,a,1,false);CHECK(app.services().projects->state().revision==revision);
        app.transfer_insert(a,first.id,a,2,false);CHECK(app.insert_generation()==generation&&app.services().projects->state().project->tracks.front().inserts.front().id==second.id);
        CHECK(app.history_preserves_plugins(false)&&app.undo()&&app.insert_generation()==generation);CHECK(app.redo()&&app.insert_generation()==generation);
        app.transfer_insert(a,first.id,b,0,false);const auto copy=app.services().projects->state().project->tracks.back().inserts.front();CHECK(copy.id!=first.id&&copy.gain==first.gain);
        CHECK(app.undo()&&app.services().projects->state().project->tracks.back().inserts.empty());CHECK(app.redo()&&app.services().projects->state().project->tracks.back().inserts.front().id==copy.id);
        app.transfer_insert(a,second.id,b,1,true);CHECK(app.services().projects->state().project->tracks.front().inserts.size()==1&&app.services().projects->state().project->tracks.back().inserts.back()==second);
        CHECK(app.undo()&&app.services().projects->state().project->tracks.front().inserts.size()==2);CHECK(app.redo());
        const auto before=*app.services().projects->state().project;rejects([&]{app.transfer_insert(a,first.id,b,99,false);});CHECK(*app.services().projects->state().project==before);
        Directory dir;app.save_project(dir.path/"Transfer.mrsproject");app.open_project(dir.path/"Transfer.mrsproject");CHECK(*app.services().projects->state().project==before);
    }

    {Application live;const auto id=live.services().projects->state().project->tracks.front().id;NativeInsert eq;eq.id=new_id();eq.kind=InsertKind::channel_eq;live.set_inserts(id,{eq});auto device=std::make_unique<ManualDevice>();auto* driver=device.get();live.connect(std::move(device),{0,48000,128,{}, {0,1}});live.play();std::array<float,256> out{};live.engine()->process(nullptr,out.data(),128);const auto at=live.engine()->state().sample;
        eq.bands[2].gain=6;live.preview_inserts(id,{eq});CHECK(live.services().projects->state().project->tracks.front().inserts.front().bands[2].gain==0);live.set_inserts(id,{eq});live.engine()->process(nullptr,out.data(),128);CHECK(live.engine()->state().sample==at+128&&live.engine()->state().playback==PlaybackState::playing&&driver->opens==1);CHECK(live.undo());live.engine()->process(nullptr,out.data(),128);CHECK(live.services().projects->state().project->tracks.front().inserts.front().bands[2].gain==0);CHECK(live.redo());live.engine()->process(nullptr,out.data(),128);CHECK(driver->opens==1);rejects([&]{live.set_inserts(id,{});});}

    Directory dir; Application app; app.new_project(44100);
    const auto track=app.add_audio_track("Mic"), bus=app.add_bus("Bus"); app.set_track_output(track,bus);
    NativeInsert a{new_id(),InsertKind::gain,.5f}, b{new_id(),InsertKind::gain,.25f}, c{new_id(),InsertKind::gain,.5f};
    app.set_inserts(track,{a,b}); CHECK(app.undo() && app.services().projects->state().project->tracks.front().inserts.empty()); CHECK(app.redo());
    app.set_inserts(track,{b,a}); CHECK(app.services().projects->state().project->tracks.front().inserts.front().id==b.id);
    app.set_inserts(bus,{c}); app.set_inserts(std::nullopt,{NativeInsert{new_id(),InsertKind::gain,.5f}});
    app.arm_track(track); app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{0},{0,1}});
    std::array<float,128> in{}; in.fill(.8f); std::array<float,256> out{};
    app.engine()->process(in.data(),out.data(),128); CHECK(std::abs(out[0]-.0125f)<1e-6f && out[0]==out[1]);
    app.seek(500); app.engine()->process(in.data(),out.data(),128);
    a.bypass=true; app.set_inserts(track,{b,a}); CHECK(app.engine()->state().sample==500);
    app.engine()->process(in.data(),out.data(),128); CHECK(std::abs(out[0]-.025f)<1e-6f);
    app.start_recording(dir.path/"Raw.wav"); app.engine()->process(in.data(),out.data(),128);
    rejects([&] { app.set_inserts(bus,{}); }); app.stop(); app.engine()->process(in.data(),out.data(),128);
    CHECK(app.engine()->state().sample==500 && audio::load_wav(dir.path/"Raw.wav").samples.front()==.8f);
    app.play(); app.engine()->process(in.data(),out.data(),128); rejects([&] { app.set_inserts(track,{}); });
    app.pause(); app.engine()->process(in.data(),out.data(),128);
    app.save_project(dir.path/"FX.mrsproject"); const auto saved=app.snapshot(); app.open_project(dir.path/"FX.mrsproject");
    CHECK(app.snapshot().project==saved.project && app.snapshot().graph==saved.graph);
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{0},{0,1}}); app.engine()->process(in.data(),out.data(),128); CHECK(std::abs(out[0]-.025f)<1e-6f);
}

void multi_input() {
    {
        Directory dir;const auto source=dir.path/"Playback.wav";wav(source);Application mixed;mixed.import_wav(source);
        const auto playback=mixed.services().projects->state().project->tracks.front().id;
        const auto live=mixed.add_audio_track("Live input");mixed.set_track_monitoring(live,true);
        auto device=std::make_unique<ManualDevice>();auto* driver=device.get();mixed.connect(std::move(device),{0,44100,128,{0},{0,1},2,1024});
        const auto monitorGeneration=mixed.insert_generation();
        const auto owner=[&]{return mixed.engine()->processing_domains().channels.front().owner;};
        CHECK(owner()==audio::ProcessingDomains::Owner::ahead);
        CHECK(mixed.engine()->processing_domains().channels[1].owner==audio::ProcessingDomains::Owner::device);
        std::array<float,256> output{};mixed.play();mixed.engine()->process(nullptr,output.data(),128);
        rejects([&]{mixed.set_track_monitoring(playback,true);});
        mixed.pause();mixed.engine()->process(nullptr,output.data(),128);mixed.set_track_monitoring(playback,true);
        CHECK(owner()==audio::ProcessingDomains::Owner::device&&driver->opens==1);
        mixed.set_track_monitoring(playback,false);CHECK(owner()==audio::ProcessingDomains::Owner::ahead);
        CHECK(mixed.undo()&&owner()==audio::ProcessingDomains::Owner::device);
        CHECK(mixed.redo()&&owner()==audio::ProcessingDomains::Owner::ahead);
        mixed.set_track_armed(playback,true);CHECK(owner()==audio::ProcessingDomains::Owner::device);
        mixed.play();mixed.engine()->process(nullptr,output.data(),128);mixed.set_track_monitoring(playback,true);
        mixed.engine()->process(nullptr,output.data(),128);CHECK(owner()==audio::ProcessingDomains::Owner::device&&driver->opens==1);
        mixed.pause();mixed.engine()->process(nullptr,output.data(),128);mixed.set_track_monitoring(playback,false);mixed.set_track_armed(playback,false);
        CHECK(owner()==audio::ProcessingDomains::Owner::ahead);
        CHECK(mixed.insert_generation()==monitorGeneration); // ownership transitions retain plugins
    }
    Directory dir; Application app; app.new_project(44100);
    const auto mono=app.add_audio_track("Mic"), stereo=app.add_audio_track("Keys");
    auto device=std::make_unique<ManualDevice>(); auto* manual=device.get();
    app.connect(std::move(device),{0,44100,128,{2},{0,1}});
    app.set_track_input(mono,3); app.set_track_input(stereo,0,true);
    app.set_track_monitoring(stereo,true); CHECK(app.stereo_track(stereo) && !app.stereo_track(mono));
    app.set_track_armed(mono,true); app.set_track_armed(stereo,true);
    CHECK(app.track_armed(mono) && app.track_armed(stereo));
    CHECK(app.services().projects->state().project->tracks[0].input_monitor);app.set_track_monitoring(mono,false); // R enables I; explicit off preserves raw recording.
    CHECK(manual->last_config.inputs == std::vector<int>({3,0,1}));
    std::array<float,384> input{}; for (std::size_t n=0; n<128; ++n) { input[n*3]=.8f; input[n*3+1]=.1f; input[n*3+2]=.4f; }
    std::array<float,256> out{}; app.seek(500); app.engine()->process(input.data(),out.data(),128);
    CHECK(out[0] == .05f && out[1] == .2f);
    rejects([&] { app.set_track_input(stereo,3,true); }); CHECK(app.stereo_track(stereo));
    app.start_recording(dir.path/"Take.wav"); app.engine()->process(input.data(),out.data(),128);
    app.set_track_monitoring(stereo,false); app.engine()->process(input.data(),out.data(),128);
    CHECK(out[0] == 0 && out[1] == 0); // monitoring off, both raw captures keep running
    app.stop(); app.engine()->process(input.data(),out.data(),128);
    CHECK(!app.recording() && app.engine()->state().sample == 500);
    const auto p=app.services().projects->state().project;
    CHECK(p->clips.size() == 2 && p->clips[0].start == 500 && p->clips[1].start == 500);
    const auto a=audio::load_wav(dir.path/"Take-1.wav"), b=audio::load_wav(dir.path/"Take-2.wav");
    CHECK(a.channels == 1 && b.channels == 2 && a.frames() == 256 && b.frames() == 256);
    CHECK(a.samples.front() == .8f && b.samples[0] == .1f && b.samples[1] == .4f);
    CHECK(app.last_takes().size() == 2);
    CHECK(app.undo() && app.services().projects->state().project->clips.empty());
    CHECK(std::filesystem::exists(dir.path/"Take-1.wav") && std::filesystem::exists(dir.path/"Take-2.wav"));
    CHECK(app.redo() && app.services().projects->state().project->clips.size() == 2);
    app.save_project(dir.path/"Session.mrsproject"); app.open_project(dir.path/"Session.mrsproject");
    CHECK(app.stereo_track(stereo) && app.armed_tracks().empty());
    app.set_track_monitoring(stereo,true); CHECK(app.undo() && !app.services().projects->state().project->tracks[1].input_monitor);
    app.connect(std::make_unique<ManualDevice>(),{0,44100,128,{0},{0,1}});
    app.set_track_input(mono,-1); app.set_track_armed(mono,true);
    rejects([&] { app.start_recording(dir.path/"Off.wav"); }); CHECK(!std::filesystem::exists(dir.path/"Off.wav"));
}

#include "reverb_desktop_cases.hpp"
#include "cab_desktop_cases.hpp"

void cab_ir(){
    Directory dir;const auto path=dir.path/"Test.wav";wav(path,2);auto ir=audio::load_cab_ir(path);CHECK(ir.channels==2&&!ir.samples.empty());
    Application app;app.new_project(44100);const auto track=app.add_audio_track("IR"),bus=app.add_bus("Cab bus");app.set_track_output(track,bus);NativeInsert fx;fx.id=new_id();fx.kind=InsertKind::cab_ir;fx.ir=ir;app.set_inserts(track,{fx});auto busfx=fx;busfx.id=new_id();app.set_inserts(bus,{busfx});auto masterfx=fx;masterfx.id=new_id();app.set_inserts(std::nullopt,{masterfx});
    auto device=std::make_unique<ManualDevice>();auto* driver=device.get();app.connect(std::move(device),{0,44100,128,{}, {0,1}});app.play();std::array<float,256> out{};app.engine()->process(nullptr,out.data(),128);const auto at=app.engine()->state().sample;fx.ir.mix=.25f;fx.ir.low_cut=80;fx.ir.high_cut=5000;fx.ir.invert=true;app.set_inserts(track,{fx});app.engine()->process(nullptr,out.data(),128);CHECK(driver->opens==1&&app.engine()->state().sample==at+128&&app.engine()->state().playback==PlaybackState::playing);CHECK(app.undo());app.engine()->process(nullptr,out.data(),128);CHECK(app.redo());app.engine()->process(nullptr,out.data(),128);CHECK(driver->opens==1);rejects([&]{auto changed=fx;changed.ir.samples[0]+=.1f;app.set_inserts(track,{changed});});app.stop();app.engine()->process(nullptr,out.data(),128);app.save_project(dir.path/"Cab.mrsproject");std::filesystem::remove(path);app.open_project(dir.path/"Cab.mrsproject");CHECK(app.services().projects->state().project->tracks.front().inserts.front().ir==fx.ir);
    rejects([&]{audio::load_cab_ir(path);});
    Application record;record.new_project(44100);const auto mic=record.add_audio_track("Mic");record.set_track_input(mic,0);record.set_track_monitoring(mic,true);NativeInsert half;half.id=new_id();half.kind=InsertKind::cab_ir;half.ir.name="Half";half.ir.sample_rate=44100;half.ir.samples={.5f};record.set_inserts(mic,{half});record.connect(std::make_unique<ManualDevice>(),{0,44100,128,{0},{0,1}});record.set_track_armed(mic,true);std::array<float,128> input{};input.fill(.8f);record.start_recording(dir.path/"RawCab.wav");record.engine()->process(input.data(),out.data(),128);record.stop();record.engine()->process(input.data(),out.data(),128);const auto raw=audio::load_wav(dir.path/"RawCab.wav");CHECK(raw.samples.front()==.8f&&raw.samples.back()==.8f);

}

}
int main(int argc, char** argv) {
    try {
        if(argc==3&&std::string_view(argv[1])=="cab_combo_real"){cab_combo_real(std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(argv[2]))));return 0;}
        if (argc != 2) throw std::runtime_error("expected suite");
        std::string name = argv[1];
        if(name=="track_folders"){track_folders();std::cout<<"PASS desktop track_folders\n";return 0;}
        if(name=="meter"){meter();std::cout<<"PASS desktop meter\n";return 0;}
        if(name=="tempo"){tempo();std::cout<<"PASS desktop tempo\n";return 0;}
        if(name=="sections"){sections();std::cout<<"PASS desktop sections\n";return 0;}
        if(name=="chords"){chords();std::cout<<"PASS desktop chords\n";return 0;}
        if(name=="markers"){markers();std::cout<<"PASS desktop markers\n";return 0;}
        if(name=="cab_combo")cab_combo();else if(name=="cab_collection")cab_collection();else if(name=="reverb")reverb();else if(name=="cab_ir")cab_ir();else if (name == "inserts") inserts(); else if (name == "multi_input") multi_input(); else if (name == "hardware") hardware(); else if (name == "profiles") profiles(); else if (name == "sends") sends(); else if (name == "buses") buses(); else if (name == "mixer") mixer(); else if (name == "workspaces") workspaces(); else if (name == "transport") transport();
        else if (name == "files") files(); else if (name == "assets") assets();
        else if (name == "mono_route") mono_route(); else if (name == "audio") audio_settings(); else if (name == "config") config(); else if (name == "arrangement") arrangement(); else if (name == "waveform") waveform(); else if (name == "nonplaying_edits") nonplaying_edits(); else if (name == "streaming") streaming(); else if (name == "clip_edits") clip_edits(); else if (name == "recording") recording(); else if (name == "project_folders") project_folders(); else throw std::runtime_error("unknown suite");
        std::cout << "PASS desktop " << name << '\n'; return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
