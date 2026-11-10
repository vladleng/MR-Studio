#include <mrs/audio_to_midi.hpp>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace mrs;
void check(bool value){if(!value)throw std::runtime_error("audio-to-MIDI assertion failed");}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::invalid_argument&){caught=true;}check(caught);}
Project fixture(){
    Project p;p.id=new_id();p.title="Transcription fixture";
    p.time.tempos={{0,120},{4*ppq,60}};
    Track t;t.id=new_id();t.name="Source";p.tracks.push_back(t);
    Clip c;c.id=new_id();c.track=t.id;c.name="Trimmed WAV";c.source="isolated-fixture.wav";
    c.start=48000;c.length=144000;c.source_offset=96000;p.clips.push_back(c);
    p.validate();return p;
}
void mapping(){
    const auto p=fixture();ProjectStore store(p);
    // File seconds [2,5) map to project seconds [1,4). Tempo changes at 2s.
    const std::vector<TranscribedNote> notes{
        {60,1.5,3.5,"acoustic_piano"},{64,3.5,8,"acoustic_piano"},
        {36,3,3.01,"drums"},{70,0,1,"voice"},{71,5,6,"voice"}};
    PlaceTranscription command(store.state().project,p.clips[0].id,notes);
    store.execute(command);const auto result=*store.state().project;
    check(result.tracks.size()==3&&result.clips.size()==3&&result.clips[0]==p.clips[0]);
    const auto& midi=*result.clips[1].midi;
    check(midi.start==2*ppq&&midi.length==4*ppq&&midi.source_offset==0);
    check(midi.notes.size()==2&&midi.notes[0].start==0&&midi.notes[0].length==5*ppq/2);
    check(midi.notes[1].start==5*ppq/2&&midi.notes[1].length==3*ppq/2);
    check(midi.notes[0].velocity==100&&midi.notes[0].channel==0);
    check(midi.events.size()==1&&midi.events[0].kind==3&&midi.events[0].data1==0);
    const auto& drums=*result.clips[2].midi;
    check(drums.notes[0].channel==9&&drums.notes[0].start==2*ppq&&drums.events.empty());
    check(deserialize(serialize(result))==result);
    check(store.undo()&&*store.state().project==p);
    check(store.redo()&&*store.state().project==result);
    // Re-running appends independent parts and never removes earlier results.
    PlaceTranscription repeat(store.state().project,p.clips[0].id,notes);
    store.execute(repeat);check(store.state().project->clips.size()==5);
    check(store.undo()&&*store.state().project==result);
    check(store.undo()&&*store.state().project==p);
    store.execute(command);check(*store.state().project==result);
}
void musical_reference(){
    const auto p=fixture();ProjectStore store(p);const auto snapshot=store.state().project;
    const std::vector<TranscribedNote> processed{{40,2.25,3.0,"electric_bass"}},raw{{40,2.237,2.93,"electric_bass"}};
    TimeMap time;time.tempos={{0,90},{4*ppq,110}};time.meters={{1,3,4}};
    store.execute(PlaceTranscription(snapshot,p.clips[0].id,processed,time,raw,"fixture: source hash / model revision"));const auto result=*store.state().project;
    check(result.time==time&&result.tracks.size()==3&&result.clips.size()==3&&result.markers.empty()&&result.tracks.back().mix.mute);
    check(result.transcription_results.size()==1&&result.transcription_results.front().source_clip==p.clips[0].id&&result.transcription_results.front().midi_clips.size()==2);
    auto duplicate=result;duplicate.transcription_results.push_back(duplicate.transcription_results.front());rejects([&]{duplicate.validate();});
    auto malformed=result;malformed.transcription_results.front().midi_clips.push_back(malformed.transcription_results.front().midi_clips.front());rejects([&]{malformed.validate();});
    malformed=result;malformed.transcription_results.front().provenance.clear();rejects([&]{malformed.validate();});
    auto wrong_version=serialize(result);wrong_version.replace(wrong_version.find("35"),2,"34");rejects([&]{(void)deserialize(wrong_version);});
    check(serialize(result).starts_with("MRS_CORE_SNAPSHOT 35\n")&&serialize(p).starts_with("MRS_CORE_SNAPSHOT 32\n"));
    check(result.clips[0]==p.clips[0]&&result.tracks.back().name.find("original timing")!=std::string::npos);
    const Timeline timeline(time,p.sample_rate);const auto& original=*result.clips.back().midi;
    check(std::abs(timeline.to_samples(original.start+original.notes[0].start)-(48000+static_cast<Sample>(std::llround(.237*48000))))<=32);
    check(deserialize(serialize(result))==result);check(store.undo()&&*store.state().project==p);check(store.redo()&&*store.state().project==result);
}
void categories(){
    auto p=fixture();ProjectStore store(p);std::vector<TranscribedNote> notes;
    for(const auto& role:ace_midi_instruments())notes.push_back({60,2.137,2.463,std::string(role.role)});
    check(notes.size()==35);
    store.execute(PlaceTranscription(store.state().project,p.clips[0].id,notes));
    const auto& result=*store.state().project;check(result.tracks.size()==36);
    const Timeline timeline(p.time,p.sample_rate);
    for(std::size_t i=1;i<result.clips.size();++i){
        const auto& m=*result.clips[i].midi;const auto& n=m.notes[0];
        check(n.channel==(i==35?9:0)&&result.tracks[i].midi_output.empty());
        const auto expected=48000+std::llround(.137*48000);
        check(std::abs(timeline.to_samples(m.start+n.start)-expected)<=25);
        if(i<35)check(m.events[0].data1==ace_midi_instruments()[i-1].program);
    }
    check(deserialize(serialize(result))==result);
}
void invalid(){
    const auto p=fixture();ProjectStore store(p);
    const auto snapshot=store.state().project;
    const auto attempt=[&](TranscribedNote n){rejects([&]{PlaceTranscription command(snapshot,p.clips[0].id,std::span(&n,1));});};
    attempt({128,2,3,"voice"});attempt({-1,2,3,"voice"});
    attempt({60,3,2,"voice"});attempt({60,2,2,"voice"});attempt({60,-1,3,"voice"});
    attempt({60,2,std::numeric_limits<double>::infinity(),"voice"});
    attempt({60,std::numeric_limits<double>::quiet_NaN(),3,"voice"});
    attempt({60,2,3,"unknown"});attempt({60,0,1,"voice"});
    const TranscribedNote note{60,2,3,"voice"};
    rejects([&]{PlaceTranscription command(snapshot,Id{"missing"},std::span(&note,1));});
    rejects([&]{PlaceTranscription command(nullptr,p.clips[0].id,std::span(&note,1));});
    PlaceTranscription command(snapshot,p.clips[0].id,std::span(&note,1));
    auto changed=p;changed.clips[0].source_offset+=1;ProjectStore stale(changed);
    const auto revision=stale.state().revision;
    rejects([&]{stale.execute(command);});check(stale.state().revision==revision&&!stale.state().can_undo);
    changed=p;changed.time.tempos[0].bpm=100;ProjectStore tempo(changed);
    rejects([&]{tempo.execute(command);});
    changed=p;changed.clips[0].stretch=ClipStretch{};
    changed.clips[0].stretch->source_frames=changed.clips[0].length;
    changed.clips[0].stretch->window_end=static_cast<double>(changed.clips[0].length);
    changed.validate();
    // Unsupported processing must never silently use full-file time mapping.
    rejects([&]{PlaceTranscription processed(std::make_shared<const Project>(changed),p.clips[0].id,std::span(&note,1));});
    check(*store.state().project==p);
}
void arguments(){
    const auto args=ace_midi_arguments("C:/model dir","C:/source dir/a.wav","C:/output dir/a.mid");
    check(args==std::vector<std::string>{"--model","C:/model dir","--transcribe","C:/source dir/a.wav",
        "--out","C:/output dir/a.mid","--jsonl","--device","cpu"});
    rejects([]{ace_midi_arguments("","a.wav","a.mid");});
    rejects([]{ace_midi_arguments("model",std::string("a\0b",3),"a.mid");});
}
}
int main(){try{mapping();musical_reference();categories();invalid();arguments();std::cout<<"PASS audio-to-MIDI mapping\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
