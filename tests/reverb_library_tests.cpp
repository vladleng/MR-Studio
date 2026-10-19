#include <mrs/reverb_library.hpp>
#include <mrs/audio.hpp>
#include <mrs/recording.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <new>
#include <bit>

namespace {thread_local bool probe{};thread_local std::size_t allocations{},deallocations{};}
void* operator new(std::size_t n){if(probe)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc{};}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {if(probe&&p)++deallocations;std::free(p);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
namespace {
using namespace mrs;using namespace mrs::audio;using namespace mrs::processing;namespace fs=std::filesystem;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class F>void wait(F predicate){for(int i=0;i<10000;++i){if(predicate())return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}throw std::runtime_error("async timeout");}
template<class F>void rejects(F f){try{f();}catch(const std::exception&){return;}throw std::runtime_error("invalid request accepted");}
struct Fixture{
    fs::path root;
    Fixture(){const auto parent=fs::weakly_canonical(fs::temp_directory_path());root=parent/("mrs-reverb-library-"+new_id().value);check(root.parent_path()==parent&&!fs::exists(root),"fixture path");fs::create_directories(root);}
    ~Fixture(){std::error_code ec;fs::remove_all(root,ec);}
};
void wav(const fs::path& path,std::uint32_t channels,std::uint32_t rate=48000,std::size_t frames=512,bool silent=false){
    fs::create_directories(path.parent_path());std::ofstream output(path,std::ios::binary|std::ios::trunc);
    auto put=[&](std::uint32_t n,int bytes){for(int i=0;i<bytes;++i)output.put(static_cast<char>((n>>(i*8))&255));};
    const auto bytes=static_cast<std::uint32_t>(frames*channels*4);output.write("RIFF",4);put(36+bytes,4);output.write("WAVEfmt ",8);put(16,4);put(3,2);put(channels,2);put(rate,4);put(rate*channels*4,4);put(channels*4,2);put(32,2);output.write("data",4);put(bytes,4);
    for(std::size_t f=0;f<frames;++f)for(std::uint32_t ch=0;ch<channels;++ch)put(std::bit_cast<std::uint32_t>(!silent&&f==0?.1f*(ch+1):0.f),4);
    check(static_cast<bool>(output),"fixture WAV write");
}
std::shared_ptr<PreparedGraph> graph(bool two=false){GraphState state;state.id={"library-fixture"};state.patch_name="IR library fixture";
    state.nodes={{{"reverb"},"mr_reverb",ProcessorFormat::native,{},{{2,1}},false}};state.edges={{std::nullopt,{"reverb"},1}};state.outputs={{{"reverb"}}};
    if(two){state.nodes.push_back({{"second"},"mr_reverb",ProcessorFormat::native,{},{{2,1}},false});state.edges.push_back({std::nullopt,{"second"},1});state.outputs.push_back({{"second"}});}
    return std::make_shared<PreparedGraph>(GraphSnapshot{std::make_shared<const GraphState>(state),0,false,false},ProcessConfig{48000,2,128});
}
const ReverbLibraryEntry& named(const ReverbLibraryIndex& index,const fs::path& name){for(const auto& e:index.entries)if(e.first.filename()==name)return e;throw std::runtime_error("missing index entry");}
void catalogue(){Fixture f;wav(f.root/"mono.wav",1);wav(f.root/"stereo.wav",2);wav(f.root/"quad.wav",4);
    wav(f.root/"Hall, 48K L.wav",2);wav(f.root/"Hall, 48K R.wav",2);wav(f.root/"Hall, 44K L.wav",2,44100);wav(f.root/"Hall, 44K R.wav",2,44100);
    wav(f.root/"rate_L.wav",2,44100);wav(f.root/"rate_R.wav",2,48000);wav(f.root/"length-L.wav",2);wav(f.root/"length-R.wav",2,48000,513);
    wav(f.root/"orphan L.wav",2);wav(f.root/"ambiguous L.wav",2);wav(f.root/"ambiguous R.wav",2);wav(f.root/"ambiguous_L.wav",2);
    wav(f.root/"silent.wav",1,48000,512,true);std::ofstream(f.root/"corrupt.wav")<<"broken";
    wav(f.root/fs::path(u8"Подпапка")/fs::path(u8"Очень длинное имя импульса.wav"),2);
    for(int i=0;i<1000;++i){auto directory=f.root/"many"/std::to_string(i%10);fs::create_directories(directory);fs::copy_file(f.root/"mono.wav",directory/("room"+std::to_string(i)+".WAV"));}
    const auto start=std::chrono::steady_clock::now();const auto index=index_reverb_library(f.root);check(index.phase==ReverbLibraryPhase::ready,"catalogue phase");
    check(index.entries.size()==1016,"one logical pair, no duplicated halves");
    check(named(index,"Hall, 48K L.wav").kind==ReverbFileKind::pair&&named(index,"Hall, 44K L.wav").sample_rate==44100,"rate duplicates stay separate presets");
    for(const auto& n:{"rate_L.wav","length-L.wav","orphan L.wav","ambiguous L.wav"})check(named(index,n).kind==ReverbFileKind::unsupported,"unsafe pair accepted");
    check(named(index,"quad.wav").kind==ReverbFileKind::quad&&named(index,"corrupt.wav").kind==ReverbFileKind::unsupported,"quad/corrupt metadata");
    std::stop_source stop;stop.request_stop();rejects([&]{index_reverb_library(f.root,stop.get_token());});
    ReverbLibrary service;service.set_root(f.root);service.set_root(f.root/"absent");wait([&]{return service.snapshot()->phase==ReverbLibraryPhase::missing;});
    check(service.snapshot()->root.filename()=="absent","obsolete scan replaced latest root");service.set_root(f.root);wait([&]{return service.snapshot()->phase==ReverbLibraryPhase::ready;});check(service.snapshot()->entries.size()==1016,"async recursive catalogue");
    std::cout<<"1018 files -> 1016 entries / scan_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" PASS\n";
}
void loading(){Fixture f;wav(f.root/"stereo.wav",2);wav(f.root/"quad.wav",4);wav(f.root/"pair L.wav",2);wav(f.root/"pair R.wav",2);wav(f.root/"silent.wav",1,48000,512,true);
    wav(f.root/"new.wav",1,44100);auto index=index_reverb_library(f.root);auto chain=graph(true);ReverbLoader service(chain,{"reverb"},{48000,2,128});ReverbLoader second(chain,{"second"},{48000,2,128});
    std::atomic<bool> running{true};std::atomic<std::size_t> callbacks{};std::atomic<bool> rt_bad{};
    std::jthread audio([&]{std::array<float,256> buffer{};while(running){buffer.fill(.001f);allocations=deallocations=0;probe=true;chain->process(buffer.data(),128,0,true);probe=false;
        if(allocations||deallocations)rt_bad=true;++callbacks;std::this_thread::sleep_for(std::chrono::microseconds(100));}});
    struct Stop{std::atomic<bool>& running;std::jthread& thread;~Stop(){running=false;thread.join();}} stop{running,audio};
    ReverbLibrary catalogue;
    for(int i=0;i<1000;++i){const auto folder=f.root/"many"/std::to_string(i%10);fs::create_directories(folder);fs::copy_file(f.root/"stereo.wav",folder/("room"+std::to_string(i)+".wav"));}
    catalogue.set_root(f.root);wait([&]{return catalogue.snapshot()->phase==ReverbLibraryPhase::ready;});check(catalogue.snapshot()->entries.size()==1005,"background 1000-file scan while DSP runs");
    auto select=[&](const char* name,ReverbQuadMap map=ReverbQuadMap::unspecified){service.select(named(index,name),f.root,map);wait([&]{return service.snapshot()->phase!=ReverbLoadPhase::loading;});};
    select("stereo.wav");check(service.snapshot()->phase==ReverbLoadPhase::ready,"stereo load");const auto previous=service.snapshot()->active->first;
    check(previous.relative=="stereo.wav"&&!previous.digest.empty(),"external identity fingerprint");
    select("quad.wav");check(service.snapshot()->phase==ReverbLoadPhase::error&&service.snapshot()->active->first.digest==previous.digest,"unconfirmed quad preserves old IR");
    select("quad.wav",ReverbQuadMap::ll_lr_rl_rr);check(service.snapshot()->phase==ReverbLoadPhase::ready,"confirmed quad load");
    select("pair L.wav");check(service.snapshot()->active->entry.kind==ReverbFileKind::pair&&!service.snapshot()->active->second.digest.empty(),"paired identity");
    const auto good=service.snapshot()->active->entry.key;select("silent.wav");check(service.snapshot()->phase==ReverbLoadPhase::error&&service.snapshot()->active->entry.key==good,"silent IR retains active selection");
    fs::remove(f.root/"silent.wav");select("silent.wav");check(service.snapshot()->phase==ReverbLoadPhase::missing&&service.snapshot()->active->entry.key==good,"missing file fallback");
    auto changed=named(index,"stereo.wav");wav(changed.first,1);service.select(changed,f.root);wait([&]{return service.snapshot()->phase!=ReverbLoadPhase::loading;});check(service.snapshot()->phase==ReverbLoadPhase::error,"changed layout rejected");
    for(int i=0;i<100;++i)service.select(named(index,i%2?"new.wav":"quad.wav"),f.root,ReverbQuadMap::ll_lr_rl_rr);
    service.select(named(index,"new.wav"),f.root);wait([&]{return service.snapshot()->phase==ReverbLoadPhase::ready;});check(service.snapshot()->active->entry.key==named(index,"new.wav").key,"last selection wins");
    service.select(named(index,"quad.wav"),f.root,ReverbQuadMap::ll_lr_rl_rr);service.cancel();const auto cancelled_key=service.snapshot()->active->entry.key;
    const auto cancelled_revision=service.snapshot()->revision;std::this_thread::sleep_for(std::chrono::milliseconds(30));
    check(service.snapshot()->active->entry.key==cancelled_key&&service.snapshot()->revision==cancelled_revision,"cancelled request not published");check(callbacks>0&&!rt_bad,"async load audio callback allocation/free");
    for(int i=0;i<20;++i){second.select(named(index,i%2?"quad.wav":"new.wav"),f.root,ReverbQuadMap::ll_lr_rl_rr);service.select(named(index,i%2?"new.wav":"quad.wav"),f.root,ReverbQuadMap::ll_lr_rl_rr);
        wait([&]{return second.snapshot()->phase==ReverbLoadPhase::ready&&service.snapshot()->phase==ReverbLoadPhase::ready;});}
    check(!rt_bad,"independent per-node reclamation introduced RT allocation/free");
    std::cout<<"Async quad/pair/resample/failed/missing/changed/100 rapid selections PASS callback allocations/deallocations=0/0\n";
}
#include "reverb_restore_cases.hpp"

void prepared_and_paths(){Fixture f;auto ir=reverb_impulse(48000,1,std::array{.5f});std::stop_source stop;stop.request_stop();rejects([&]{ReverbProcessor::prepare_kernel(ir,{48000,2,128},stop.get_token());});
    auto prepared=ReverbProcessor::prepare_kernel(ir,{48000,2,128});ReverbProcessor other;other.prepare({44100,2,128});rejects([&]{other.stage_kernel(prepared);});other.prepare({48000,2,128});check(other.stage_kernel(prepared),"prepared publish");other.warm();
    std::array<float,256> buffer{};buffer[0]=1;MidiBuffer midi;other.set_parameter(2,1);other.reset();other.process({buffer,128,2,{},{},midi});check(buffer[0]==.5f,"prepared kernel output");
    auto long_root=f.root/fs::path(u8"Русская папка с пробелами")/std::string(120,'d');fs::create_directories(long_root);auto long_file=long_root/(std::string(160,'n')+".wav");
#ifdef _WIN32
    // Explicit extended path exercises >260 chars even on machines without LongPathsEnabled.
    long_root=fs::path(L"\\\\?\\"+fs::absolute(long_root).wstring());long_file=long_root/long_file.filename();
#endif
    wav(long_file,1);check(fs::absolute(long_file).native().size()>260,"long path fixture");const auto index=index_reverb_library(long_root);check(index.entries.size()==1&&index.entries[0].kind==ReverbFileKind::mono,"Unicode extended long path index/decode");
    const auto decoded=load_reverb_ir(long_file);check(decoded.kernels[0][0]==.1f,"long path decoder");
    const auto huge=f.root/"oversized"/"huge.wav";wav(huge,1);{std::fstream out(huge,std::ios::binary|std::ios::in|std::ios::out);auto put=[&](std::uint32_t value){for(int i=0;i<4;++i)out.put(static_cast<char>((value>>(i*8))&255));};const std::uint32_t bytes=80'000'000;out.seekp(4);put(36+bytes);out.seekp(40);put(bytes);out.seekp(43ULL+bytes);out.put(0);}
    check(index_reverb_library(huge.parent_path()).entries[0].kind==ReverbFileKind::unsupported,"huge file isolated by metadata budget");rejects([&]{load_reverb_ir(huge);});
    auto library=std::make_shared<ReverbLibrary>();library->set_root(long_root);wait([&]{return library->snapshot()->phase==ReverbLibraryPhase::ready;});auto chain=graph();ReverbLoader loader(chain,{"reverb"},{48000,2,128},library);
    library->set_root(f.root/"missing");loader.select(index.entries[0],long_root);std::this_thread::sleep_for(std::chrono::milliseconds(20));check(loader.snapshot()->phase!=ReverbLoadPhase::loading&&!loader.snapshot()->active,"stale shared-root selection not published");
}
void user_library(const fs::path& quad,const fs::path& pairs){auto q=index_reverb_library(quad),p=index_reverb_library(pairs);check(q.entries.size()==134&&p.entries.size()==134,"real library logical counts");
    for(const auto& e:q.entries)check(e.kind==ReverbFileKind::quad,"real quad catalogue");for(const auto& e:p.entries)check(e.kind==ReverbFileKind::pair,"real paired catalogue");
    std::cout<<"USER CATALOGUE PASS quad=134 paired_entries=134 stereo_files=268 no duplicate playback\n";
}
}
int main(int argc,char** argv){try{if(argc==4&&std::string_view(argv[1])=="--library")user_library(argv[2],argv[3]);else{catalogue();loading();prepared_and_paths();restore_references();std::cout<<"MR Reverb library PASS\n";}return 0;}catch(const std::exception& e){probe=false;std::cerr<<e.what()<<'\n';return 1;}}
