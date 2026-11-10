#include <mrs/audio_to_midi_worker.hpp>
#include <mrs/sha256.hpp>
#include <fstream>
#include <iostream>
#include <thread>
using namespace mrs;
using namespace mrs::transcription;
namespace fs=std::filesystem;
void check(bool b){if(!b)throw std::runtime_error("Python worker contract assertion");}
void text(const fs::path& p,const std::string& value){std::ofstream out(p,std::ios::binary);out<<value;check(out.good());}
void wav(const fs::path& p,int mode){std::string b="RIFF";const auto le=[&](unsigned n,int bytes){while(bytes--){b+=static_cast<char>(n&255);n>>=8;}};le(32036,4);b+="WAVEfmt ";le(16,4);le(1,2);le(1,2);le(16000,4);le(32000,4);le(2,2);le(16,2);b+="data";le(32000,4);b.resize(32044,0);b[44]=static_cast<char>(mode);text(p,b);}
int main(int argc,char** argv){try{
    check(argc==3);const auto root=fs::temp_directory_path()/("mrs-python-midi-"+new_id().value);fs::create_directory(root);
    struct Cleanup{fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);}} cleanup{root};
    Request r;r.backend=Backend::python_medium;r.executable=fs::absolute(argv[1]);r.executable_sha256=sha256_file(r.executable);r.worker_script=fs::absolute(argv[2]);r.worker_sha256=sha256_file(r.worker_script);r.model_directory=root/"model";fs::create_directory(r.model_directory);r.temporary_root=root/"jobs";fs::create_directory(r.temporary_root);r.source=root/"source.wav";
    text(r.model_directory/"config.json","{}");text(r.model_directory/"model.safetensors","fixture");r.config_sha256=sha256_file(r.model_directory/"config.json");r.weights_sha256=sha256_file(r.model_directory/"model.safetensors");r.model_id="fixture";r.model_revision="self-generated";r.model_license="self-generated";r.cpu_threads=1;r.timeout=std::chrono::seconds(5);
    wav(r.source,0);auto result=run(r);if(result.state!=State::ready)throw std::runtime_error(result.error+": "+result.diagnostics);check(result.notes.size()==1&&result.original_notes.size()==1&&result.notes[0].start==.3&&result.original_notes[0].start==.25&&result.cpu_threads==1&&result.cpu_capability=="DEFAULT"&&result.beats.size()==2&&fs::is_empty(r.temporary_root));
    r.instruments={"electric_bass","acoustic_bass"};result=run(r);check(result.state==State::ready&&result.notes[0].instrument=="electric_bass");
    wav(r.source,5);result=run(r);check(result.state==State::ready&&result.raw_notes==2&&result.original_notes.size()==1);wav(r.source,0);
    r.compatibility_cpu=true;check(run(r).state==State::ready);
    for(const auto mode:{2,3,4}){wav(r.source,mode);result=run(r);check(result.state==State::failed&&result.notes.empty()&&result.original_notes.empty()&&fs::is_empty(r.temporary_root));}
    wav(r.source,1);r.timeout=std::chrono::milliseconds(150);check(run(r).state==State::timeout&&fs::is_empty(r.temporary_root));r.timeout=std::chrono::seconds(5);
    std::stop_source stop;std::jthread cancel([&]{std::this_thread::sleep_for(std::chrono::milliseconds(150));stop.request_stop();});check(run(r,stop.get_token()).state==State::cancelled&&fs::is_empty(r.temporary_root));cancel.join();
    wav(r.source,0);auto bad=r;bad.instruments={"electric_bass","electric_bass"};check(run(bad).state==State::failed);bad=r;bad.worker_sha256=std::string(64,'0');check(run(bad).state==State::failed);bad=r;bad.musical.strength=2;check(run(bad).state==State::failed);
    std::cout<<"PASS isolated Python contract / original vs processed / multi-instrument / cleanup\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
