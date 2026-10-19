#include <mrs/desktop.hpp>
#include <mrs/midi_import.hpp>
#include <mrs/persistence.hpp>
#include <mrs/midi_playback.hpp>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <stdexcept>
using namespace mrs;
namespace {
void check(bool ok){if(!ok)throw std::runtime_error("MIDI import regression");}
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed);}
std::vector<std::byte> smf(){
    // Format 0: two channels, velocity-zero off and sustain CC.
    const unsigned char raw[]={0x4d,0x54,0x68,0x64,0,0,0,6,0,0,0,1,1,0xe0,
        0x4d,0x54,0x72,0x6b,0,0,0,33,
        0,0xff,3,4,'K','e','y','s',0,0x90,60,100,0,0x91,64,90,
        0x83,0x60,0x90,60,0,0,0x81,64,0,0,0xb0,64,127,0,0xff,0x2f,0};
    std::vector<std::byte> b;for(auto v:raw)b.push_back(static_cast<std::byte>(v));return b;
}
struct Manual final:audio::IAudioDevice {
    audio::DevicePhase phase{audio::DevicePhase::closed};bool fail{};
    std::vector<audio::DeviceInfo> enumerate()override{return {{0,"MIDI import fixture",{}, {"L","R"},16,8192,128,1}};}
    void control_panel(int)override{}void open(const audio::DeviceConfig&,std::shared_ptr<audio::AudioEngine>)override{phase=audio::DevicePhase::open;}
    void start()override{if(fail){fail=false;throw std::runtime_error("Start failure");}phase=audio::DevicePhase::running;}void stop()override{phase=audio::DevicePhase::stopped;}void close()noexcept override{phase=audio::DevicePhase::closed;}audio::DeviceStatus status()override{return {phase,48000};}
};
}
int main(int argc,char** argv){try{
    auto bytes=smf();check(bytes.size()==55);auto parts=parse_midi_file(bytes);check(parts.size()==2&&parts[0].midi.notes[0].length==ppq&&parts[1].midi.notes[0].channel==1);check(parts[0].midi.events.size()==1&&parts[0].midi.events[0].data1==64&&parts[0].name=="Keys / Ch 1");
    auto bad=bytes;bad.pop_back();rejects([&]{parse_midi_file(bad);});bad=bytes;bad[12]=std::byte{0xe7};rejects([&]{parse_midi_file(bad);});bad=bytes;bad[9]=std::byte{2};rejects([&]{parse_midi_file(bad);});
    // Unterminated notes close at EOT; sub-tick notes remain at least one tick.
    bad=bytes;bad[41]=std::byte{61};auto dangling=parse_midi_file(bad);check(dangling[0].midi.notes[0].length>=ppq);
    // Format1 with a conductor-only track must not create an empty instrument.
    auto multi=bytes;multi[9]=std::byte{1};multi[11]=std::byte{2};const unsigned char conductor[]={0x4d,0x54,0x72,0x6b,0,0,0,11,0,0xff,0x51,3,7,0xa1,0x20,0,0xff,0x2f,0};for(const auto b:conductor)multi.push_back(static_cast<std::byte>(b));check(parse_midi_file(multi).size()==2);
    // Two overlapping notes using running status; program/pressure/poly/bend/SysEx.
    const unsigned char events[]={0,0x90,60,100,0,60,90,1,60,0,1,60,0,0,0xc0,12,0,0xd0,80,0,0xa0,60,70,0,0xe0,0,64,0,0xf0,2,1,0xf7,0,0xff,0x2f,0};
    auto running=std::vector<std::byte>(bytes.begin(),bytes.begin()+22);running[21]=static_cast<std::byte>(sizeof(events));for(const auto b:events)running.push_back(static_cast<std::byte>(b));const auto eventParts=parse_midi_file(running);check(eventParts.size()==1&&eventParts[0].midi.notes.size()==2&&eventParts[0].midi.events.size()==4&&eventParts[0].midi.notes[0].length==2&&eventParts[0].midi.notes[1].length==4);check(eventParts[0].midi.events[0].kind==3&&eventParts[0].midi.events[3].kind==5);
    const auto root=std::filesystem::temp_directory_path()/std::filesystem::path("mrs-midi-import-"+new_id().value);std::filesystem::create_directories(root);
    struct Clean{std::filesystem::path p;~Clean(){std::error_code e;std::filesystem::remove_all(p,e);}}clean{root};const auto file=root/"test.mid";{std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));}
    const auto makeLong=[&](std::size_t count){auto data=std::vector<std::byte>(bytes.begin(),bytes.begin()+22);const unsigned char pair[]={0,0x90,60,100,1,0x80,60,0};for(std::size_t i=0;i<count;++i)for(const auto b:pair)data.push_back(static_cast<std::byte>(b));for(const auto b:{0,255,47,0})data.push_back(static_cast<std::byte>(b));const auto length=data.size()-22;for(int i=0;i<4;++i)data[18+static_cast<std::size_t>(i)]=static_cast<std::byte>((length>>(24-8*i))&255);return data;};
    const auto longData=makeLong(7605);const auto longFile=root/"long.mid";{std::ofstream out(longFile,std::ios::binary);out.write(reinterpret_cast<const char*>(longData.data()),static_cast<std::streamsize>(longData.size()));}
    desktop::Application longApp;longApp.new_project();longApp.import_midi({longFile});const auto longProject=*longApp.services().projects->state().project;check(longProject.clips.size()==1&&longProject.clips[0].midi->notes.size()==7605);check(deserialize(serialize(longProject))==longProject);check(longApp.undo()&&longApp.services().projects->state().project->clips.empty());check(longApp.redo());
    auto maximum=makeLong(max_project_midi_notes);check(parse_midi_file(maximum)[0].midi.notes.size()==max_project_midi_notes);rejects([&]{parse_midi_file(makeLong(max_project_midi_notes+1));});
    std::vector<audio::PlaybackMidiNote> many;for(std::size_t i=0;i<max_project_midi_notes;++i)many.push_back({static_cast<Sample>(i*128),static_cast<Sample>(i*128+64),60,100,0});audio::MidiPlayback playback;playback.prepare(many);processing::MidiBuffer buffer;check(playback.render(static_cast<Sample>((max_project_midi_notes-1)*128+32),16,true,buffer)&&buffer.size==1);many.push_back(many.back());rejects([&]{playback.prepare(many);});
    desktop::Application app;app.new_project();const auto before=*app.services().projects->state().project;const auto revision=app.services().projects->state().revision;
    const auto clips=app.import_midi({file},{},2*ppq);const auto imported=*app.services().projects->state().project;check(clips.size()==2&&imported.tracks.size()==before.tracks.size()+2&&imported.clips[0].midi->start==2*ppq&&app.services().projects->state().revision==revision+1&&imported.time==before.time);
    check(app.undo()&&*app.services().projects->state().project==before);check(app.redo()&&*app.services().projects->state().project==imported);
    const auto saved=root/"saved.mrsproject";app.save_project(saved);const auto expected=*app.services().projects->state().project;app.open_project(saved);check(*app.services().projects->state().project==expected);
    const auto id=app.services().projects->state().project->tracks.back().id;const auto count=app.services().projects->state().project->tracks.size();app.import_midi({file},id,0);check(app.services().projects->state().project->tracks.size()==count+1);check(app.undo());
    const auto state=app.services().projects->state();rejects([&]{app.import_midi({file,root/"missing.mid"});});check(app.services().projects->state().revision==state.revision&&*app.services().projects->state().project==*state.project&&app.services().projects->state().can_redo);
    const auto audio=app.add_audio_track("Audio");rejects([&]{app.import_midi({file},audio);});
    auto manual=std::make_unique<Manual>();auto* device=manual.get();app.connect(std::move(manual),{0,48000,128,{}, {0,1}});const auto generation=app.insert_generation();app.import_midi({file});check(app.insert_generation()==generation);check(app.undo());const auto rollback=app.services().projects->state();device->fail=true;rejects([&]{app.import_midi({file});});check(app.services().projects->state().revision==rollback.revision&&*app.services().projects->state().project==*rollback.project&&app.services().projects->state().can_redo&&app.audio_running());
    if(argc==2){const auto userParts=load_midi_file(std::filesystem::path(argv[1]));std::size_t notes{},userEvents{};for(const auto& part:userParts){notes+=part.midi.notes.size();userEvents+=part.midi.events.size();}desktop::Application real;real.new_project();real.import_midi({std::filesystem::path(argv[1])});real.connect(std::make_unique<Manual>(),{0,48000,128,{}, {0,1}});const auto dest=root/"user-midi-test.mrsproject";real.save_project(dest);const auto userExpected=*real.services().projects->state().project;real.open_project(dest);check(*real.services().projects->state().project==userExpected);real.connect(std::make_unique<Manual>(),{0,48000,128,{}, {0,1}});real.play();std::array<float,256> userOutput{};real.engine()->process(nullptr,userOutput.data(),128);check(!real.engine()->metrics().processing_fault);real.stop();real.engine()->process(nullptr,userOutput.data(),128);std::cout<<"USER MIDI PASS parts="<<userParts.size()<<" notes="<<notes<<" events="<<userEvents<<'\n';}
    std::cout<<"PASS MIDI import\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
