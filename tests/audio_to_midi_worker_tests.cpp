#include <mrs/audio_to_midi_worker.hpp>
#include <mrs/sha256.hpp>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
namespace fs=std::filesystem;
using namespace mrs;
using namespace mrs::transcription;
void check(bool b){if(!b)throw std::runtime_error("transcription worker assertion");}
void write(const fs::path& p,std::string_view s){std::ofstream f(p,std::ios::binary);f.write(s.data(),static_cast<std::streamsize>(s.size()));check(f.good());}
void wav(const fs::path& p,int mode){
    std::string b="RIFF";const auto le=[&](unsigned n,int count){for(int i=0;i<count;++i)b+=static_cast<char>((n>>(8*i))&255);};
    le(32036,4);b+="WAVEfmt ";le(16,4);le(1,2);le(1,2);le(16000,4);le(32000,4);le(2,2);le(16,2);b+="data";le(32000,4);b.resize(32044,0);b[44]=static_cast<char>(mode);write(p,b);
}
void environment(const char* k,const char* v){
#ifdef _WIN32
    _putenv_s(k,v);
#else
    setenv(k,v,1);
#endif
}
}
int main(int argc,char** argv){try{
    check(argc==2);const auto root=fs::temp_directory_path()/("mrs-midi-tests "+new_id().value);fs::create_directory(root);
    struct Cleanup{fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);}} cleanup{root};
    const auto model=root/"model with spaces & symbols";fs::create_directory(model);const auto jobs=root/"jobs";fs::create_directory(jobs);
    Request request;request.executable=root/"native fixture & symbols.exe";fs::copy_file(fs::absolute(argv[1]),request.executable);request.model_directory=model;request.source=root/"source.wav";request.temporary_root=jobs;
    request.executable_sha256=sha256_file(request.executable);request.model_id="fake-fixture";request.model_revision="test";request.model_license="self-generated fixture";request.project_id=new_id();request.clip_id=new_id();request.project_revision=17;request.timeout=std::chrono::seconds(5);
    const auto prepare=[&](int mode){write(model/"config.json","{}");write(model/"model.safetensors","fixture");wav(request.source,mode);request.config_sha256=sha256_file(model/"config.json");request.weights_sha256=sha256_file(model/"model.safetensors");};
    // The child must override inherited backend/probe settings and strip secrets.
    environment("GGML_BACKEND","bad-device");environment("HF_TOKEN","fake-test-secret");environment("ACE_MIDI_PROBE","must-not-reach-child");
    prepare(0);const auto source=sha256_file(request.source);std::vector<int> progress;
    const auto good=run(request,{},[&](int n,int total){check(total==1);progress.push_back(n);});
    check(good.state==State::ready&&good.notes.size()==1&&good.notes[0].pitch==60&&good.raw_notes==1);
    check(good.project_id==request.project_id&&good.clip_id==request.clip_id&&good.project_revision==17&&good.source_sha256==source);
    check(good.midi_sha256.size()==64&&sha256_file(request.source)==source&&fs::is_empty(jobs));
    check(!progress.empty()&&progress.back()==1);
    prepare(15);const auto drums=run(request);check(drums.state==State::ready&&drums.notes.size()==1&&drums.notes[0].instrument=="drums");
    for(const int mode:{5,6}){prepare(mode);const auto result=run(request);check(result.state==State::empty&&result.notes.empty()&&fs::is_empty(jobs));}
    for(const int mode:{2,3,4,7,8,9,10,11,12,13,14}){prepare(mode);const auto result=run(request);if(result.state!=State::failed)throw std::runtime_error("Invalid native fixture accepted, mode "+std::to_string(mode));check(!result.error.empty()&&result.notes.empty()&&result.diagnostics.size()<=65536&&fs::is_empty(jobs));}
    prepare(1);request.timeout=std::chrono::milliseconds(200);check(run(request).state==State::timeout&&fs::is_empty(jobs));request.timeout=std::chrono::seconds(5);
    std::stop_source cancel;std::jthread canceller([&]{std::this_thread::sleep_for(std::chrono::milliseconds(150));cancel.request_stop();});
    check(run(request,cancel.get_token()).state==State::cancelled&&fs::is_empty(jobs));canceller.join();
    prepare(0);std::stop_source late;check(run(request,late.get_token(),[&](int,int){late.request_stop();}).state==State::cancelled&&fs::is_empty(jobs));
    prepare(0);bool changed=false;const auto stale=run(request,{},[&](int,int){if(!changed){changed=true;wav(request.source,9);}});check(stale.state==State::failed&&stale.notes.empty()&&fs::is_empty(jobs));
    prepare(0);auto bad=request;bad.weights_sha256=std::string(64,'0');check(run(bad).state==State::failed&&fs::is_empty(jobs));
    bad=request;bad.executable_sha256.clear();check(run(bad).state==State::failed&&fs::is_empty(jobs));
    bad=request;bad.source=bad.source.parent_path()/"missing.wav";check(run(bad).state==State::failed&&fs::is_empty(jobs));
    std::error_code link_error;const auto link=root/"linked.wav";fs::create_symlink(request.source,link,link_error);
    if(!link_error){bad=request;bad.source=link;check(run(bad).state==State::failed&&fs::is_empty(jobs));}
    bad=request;bad.timeout=std::chrono::milliseconds(0);check(run(bad).state==State::failed&&fs::is_empty(jobs));
    std::cout<<"PASS isolated native transcription worker\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
