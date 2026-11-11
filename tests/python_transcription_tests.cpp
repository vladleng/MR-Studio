#include <mrs/audio_to_midi_worker.hpp>
#include <mrs/sha256.hpp>
#include <mrs/transcription_controller.hpp>
#include <mrs/persistence.hpp>
#include <mrs/desktop.hpp>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <thread>
using namespace mrs;
using namespace mrs::transcription;
namespace fs=std::filesystem;
void check(bool b){if(!b)throw std::runtime_error("Python worker contract assertion");}
void text(const fs::path& p,const std::string& value){std::ofstream out(p,std::ios::binary);out<<value;check(out.good());}
void wav(const fs::path& p,int mode){std::string b="RIFF";const auto le=[&](unsigned n,int bytes){while(bytes--){b+=static_cast<char>(n&255);n>>=8;}};le(32036,4);b+="WAVEfmt ";le(16,4);le(1,2);le(1,2);le(16000,4);le(32000,4);le(2,2);le(16,2);b+="data";le(32000,4);b.resize(32044,0);b[44]=static_cast<char>(mode);text(p,b);}
int main(int argc,char** argv){try{
    check(Request{}.backend==Backend::python_medium);
    check(argc==3);const auto root=fs::temp_directory_path()/("mrs-python-midi-"+new_id().value);fs::create_directory(root);
    struct Cleanup{fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);}} cleanup{root};
    Request r;r.backend=Backend::python_medium;r.executable=fs::absolute(argv[1]);r.executable_sha256=sha256_file(r.executable);r.worker_script=fs::absolute(argv[2]);r.worker_sha256=sha256_file(r.worker_script);r.model_directory=root/"model";fs::create_directory(r.model_directory);r.temporary_root=root/"jobs";fs::create_directory(r.temporary_root);r.source=root/"source.wav";
    text(r.model_directory/"config.json","{}");text(r.model_directory/"model.safetensors","fixture");r.config_sha256=sha256_file(r.model_directory/"config.json");r.weights_sha256=sha256_file(r.model_directory/"model.safetensors");r.model_id="fixture";r.model_revision="self-generated";r.model_license="self-generated";r.cpu_threads=1;r.timeout=std::chrono::seconds(5);
    wav(r.source,0);std::vector<Phase> phases;auto result=run(r,{}, {},[&](Phase phase){phases.push_back(phase);});if(result.state!=State::ready)throw std::runtime_error(result.error+": "+result.diagnostics);check(result.notes.size()==1&&result.original_notes.size()==1&&result.notes[0].start==.3&&result.original_notes[0].start==.25&&result.cpu_threads==1&&result.cpu_capability=="DEFAULT"&&result.beats.size()==2&&fs::is_empty(r.temporary_root));
    check(std::find(phases.begin(),phases.end(),Phase::loading_model)!=phases.end());
    check(std::find(phases.begin(),phases.end(),Phase::recognizing)!=phases.end());
    check(result.phase==Phase::complete&&phases.back()==Phase::complete);
    r.instruments={"electric_bass","acoustic_bass"};result=run(r);check(result.state==State::ready&&result.notes[0].instrument=="electric_bass");
    wav(r.source,5);result=run(r);check(result.state==State::ready&&result.raw_notes==2&&result.original_notes.size()==1);wav(r.source,0);
    r.compatibility_cpu=true;check(run(r).state==State::ready);
    for(const auto mode:{2,3,4}){wav(r.source,mode);result=run(r);check(result.state==State::failed&&result.notes.empty()&&result.original_notes.empty()&&fs::is_empty(r.temporary_root));}
    wav(r.source,1);r.timeout=std::chrono::milliseconds(150);check(run(r).state==State::timeout&&fs::is_empty(r.temporary_root));r.timeout=std::chrono::seconds(5);
    std::stop_source stop;std::jthread cancel([&]{std::this_thread::sleep_for(std::chrono::milliseconds(150));stop.request_stop();});check(run(r,stop.get_token()).state==State::cancelled&&fs::is_empty(r.temporary_root));cancel.join();
    wav(r.source,0);auto bad=r;bad.instruments={"electric_bass","electric_bass"};check(run(bad).state==State::failed);bad=r;bad.worker_sha256=std::string(64,'0');check(run(bad).state==State::failed);bad=r;bad.musical.strength=2;check(run(bad).state==State::failed);
    bad=r;bad.verify_runtime=[](std::stop_token){throw std::runtime_error("corrupt managed runtime");};const auto broken_runtime=run(bad);check(broken_runtime.state==State::failed&&broken_runtime.error=="runtime_verification_failed"&&broken_runtime.notes.empty()&&fs::is_empty(r.temporary_root));
    // The Python result enters the existing project command, including an
    // explicit tempo import and durable muted originals/provenance.
    text(r.model_directory/"README.md","self-generated fixture");ModelSpec spec{"fixture",std::string(40,'a'),"self-generated",{}};
    for(const auto* name:{"config.json","model.safetensors","README.md"})spec.files.push_back({name,sha256_file(r.model_directory/name),fs::file_size(r.model_directory/name)});
    ModelStore models(root/"installed",spec);models.install(r.model_directory,true);
    Project project;project.id=new_id();project.sample_rate=16000;project.time.tempos={{0,90}};Track track;track.id=new_id();project.tracks.push_back(track);Clip clip;clip.id=new_id();clip.track=track.id;clip.source="fixture.wav";clip.length=16000;project.clips.push_back(clip);project.validate();
    auto store=std::make_shared<ProjectStore>(project);Controller controller;r.musical.import_tempo=true;r.musical.quantize=true;
    controller.start(store,root/"fixture.mrs",clip.id,r.source,models,r);
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(controller.status().state==JobState::running){check(std::chrono::steady_clock::now()<until);controller.poll(store,root/"fixture.mrs");std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    check(controller.status().state==JobState::ready);store->execute(controller.prepare(store,root/"fixture.mrs"));const auto placed=*store->state().project;
    check(placed.tracks.size()==3&&placed.tracks.back().mix.mute&&placed.time.tempos.front().bpm==120&&placed.time.tempos.back().bpm==90&&placed.markers.empty()&&placed.transcription_results.size()==1&&placed.clips.front()==clip);
    persistence::ProjectDocument document;document.project=placed;document.graph.id=new_id();
    const auto saved=root/"Python provenance.mrsproject",saved_as=root/"Python provenance Save As.mrsproject";persistence::save_project(saved,document);check(persistence::load_project(saved)==document);persistence::save_project(saved_as,persistence::load_project(saved));check(persistence::load_project(saved_as)==document);
    check(deserialize(serialize(placed))==placed);check(store->undo()&&*store->state().project==project);check(store->redo()&&*store->state().project==placed);controller.placed();
    wav(r.source,6);controller.start(store,root/"fixture.mrs",clip.id,r.source,models,r);
    while(controller.status().state==JobState::running){check(std::chrono::steady_clock::now()<until);controller.poll(store,root/"fixture.mrs");std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    check(controller.status().state==JobState::failed&&controller.status().error=="missing_eos"&&!controller.status().diagnostics.empty()&&*store->state().project==placed);
    // Product conversion is raw timing only; stale musical options and runtime
    // probes must not run automatically at the application boundary.
    wav(r.source,7);desktop::Application app;app.new_project(16000);app.import_wavs({r.source},{},16000);
    const auto app_before=*app.services().projects->state().project;
    unsigned runtime_probes{};r.verify_runtime=[&](std::stop_token){++runtime_probes;throw std::runtime_error("unexpected automatic runtime probe");};
    r.musical.import_tempo=true;r.musical.tempo_map=true;r.musical.detect_tempo=true;r.musical.quantize=true;r.musical.bpm=90;r.musical.fill_gaps=1;r.musical.trim_leading_bars=true;
    app.queue_transcription(app_before.clips.front().id,models,r);
    const auto app_until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(app.transcription_status().state==JobState::running){check(std::chrono::steady_clock::now()<app_until);app.poll();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    check(app.transcription_status().state==JobState::ready&&runtime_probes==0);
    const auto* app_result=app.transcription_preview();check(app_result&&!app_result->musical.quantize&&!app_result->musical.import_tempo&&!app_result->musical.tempo_map&&!app_result->musical.detect_tempo&&app_result->musical.bpm==0&&app_result->notes.size()==1&&app_result->original_notes.size()==1&&app_result->notes.front().start==app_result->original_notes.front().start&&app_result->notes.front().end==app_result->original_notes.front().end);
    app.place_transcription();const auto app_after=*app.services().projects->state().project;check(app_after.time==app_before.time&&app_after.clips.size()==app_before.clips.size()+1);
    check(app.undo()&&*app.services().projects->state().project==app_before&&app.redo()&&*app.services().projects->state().project==app_after);
    std::cout<<"PASS isolated Python contract / original vs processed / multi-instrument / cleanup\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
