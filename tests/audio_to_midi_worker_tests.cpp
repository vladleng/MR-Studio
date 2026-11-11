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
void wav(const fs::path& p,int mode,unsigned rate=16000,unsigned frames=16000){
    std::string b="RIFF";const auto le=[&](unsigned n,int count){for(int i=0;i<count;++i)b+=static_cast<char>((n>>(8*i))&255);};
    le(36+frames*2,4);b+="WAVEfmt ";le(16,4);le(1,2);le(1,2);le(rate,4);le(rate*2,4);le(2,2);le(16,2);b+="data";le(frames*2,4);b.resize(44+frames*2,0);b[44]=static_cast<char>(mode);write(p,b);
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
    Request request;request.backend=Backend::protocol_fixture;request.executable=root/"native fixture & symbols.exe";fs::copy_file(fs::absolute(argv[1]),request.executable);request.model_directory=model;request.source=root/"source.wav";request.temporary_root=jobs;
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
    prepare(0);wav(request.source,0,48000,240001);check(run(request).state==State::ready&&fs::is_empty(jobs));
    // Only the requested one-second region of a ten-second, 48k source reaches native.
    prepare(0);wav(request.source,0,48000,480000);{std::fstream f(request.source,std::ios::binary|std::ios::in|std::ios::out);f.seekp(44+96000*2);f.put(16);}
    const auto original=sha256_file(request.source);request.range=SourceRange{64000,32000,32000};request.cpu_threads=1;request.stem_instrument="electric_bass";
    const auto clipped=run(request);check(clipped.state==State::ready&&clipped.analysis_seconds==1&&clipped.source_offset_seconds==2&&clipped.cpu_threads==1&&clipped.source_rate==48000&&clipped.source_first_frame==96000&&clipped.source_frame_count==48000&&clipped.input_sha256.size()==64);
    check(clipped.notes.size()==1&&clipped.notes[0].start==2.25&&clipped.notes[0].instrument=="electric_bass"&&sha256_file(request.source)==original&&fs::is_empty(jobs));
    check(sha256_file_range(request.source,44+96000*2,96000)==clipped.source_sha256);
    request.range.reset();request.cpu_threads=2;prepare(19);check(run(request).state==State::failed);request.stem_instrument="invalid";check(run(request).state==State::failed);request.stem_instrument.clear();
    auto invalid_range=request;invalid_range.range=SourceRange{-1,100,16000};check(run(invalid_range).state==State::failed);invalid_range.range=SourceRange{16000,100,16000};check(run(invalid_range).state==State::failed);
    write(root/"hash-range","prefixpayloadsuffix");check(sha256_file_range(root/"hash-range",6,7)==sha256("payload")&&sha256_file_range(root/"hash-range",0,0)==sha256(""));
    constexpr std::uint32_t features=(1U<<20)|(1U<<27)|(1U<<28)|(1U<<29)|(1U<<12),extended=(1U<<5)|(1U<<8);
    check(avx2_usable(features,extended,6));for(const auto bit:{20U,27U,28U,29U,12U})check(!avx2_usable(features&~(1U<<bit),extended,6));check(!avx2_usable(features,extended&~(1U<<5),6)&&!avx2_usable(features,extended&~(1U<<8),6)&&!avx2_usable(features,extended,2)&&!avx2_usable(features,extended,4));
    check(choose_cpu_threads(1)==1&&choose_cpu_threads(32)<=32&&choose_cpu_threads(0)>=1&&choose_cpu_threads(0)<=16);
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
    // The same synthetic process exercises Python argv/sidecars for the GUI
    // smoke without a real interpreter, ML packages or model weights.
    prepare(0);request.backend=Backend::python_medium;request.worker_script=root/"fixture.py";write(request.worker_script,"# synthetic adapter");request.worker_sha256=sha256_file(request.worker_script);
    const auto python_transport=run(request);check(python_transport.state==State::ready&&python_transport.notes.size()==1&&python_transport.notes.front().start==.25&&fs::is_empty(jobs));
    std::cout<<"PASS isolated native transcription worker\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
