// Experimental isolated native worker. Never linked into the DAW/ASIO process.
#include "native_ipc.hpp"
#include "sw_frontend.hpp"
#include "native_profiles.h"
#include <audiocpp.h>
#include <mrs/audio.hpp>
#include <mrs/sha256.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <span>
#include <thread>
#ifndef AUDIOCPP_MRS_CHUNK_PROGRESS_VERSION
#error Native worker requires the pinned MR Studio progress extension
#endif
namespace {
namespace fs=std::filesystem;
namespace ipc=mrs::separation::native;
using ipc::Json;using ipc::Failure;using ipc::require;
using Clock=std::chrono::steady_clock;
constexpr const char* backendVersion="v0.9.3-mrs1";
std::string pathText(const fs::path& path){const auto text=path.u8string();return {text.begin(),text.end()};}
bool shortId(const std::string& text){
    return !text.empty()&&text.size()<=512&&ipc::utf8(text)&&
        std::count_if(text.begin(),text.end(),[](char value){return (static_cast<unsigned char>(value)&0xc0)!=0x80;})<=128;
}
fs::path pathFrom(const std::string& text){require(!text.empty()&&text.size()<=32768&&ipc::utf8(text),"invalid_input");return fs::path(std::u8string(text.begin(),text.end()));}
bool plain(const fs::path& path){
    const auto status=fs::symlink_status(path);
#ifdef _WIN32
    const auto flags=GetFileAttributesW(path.c_str());
    if(flags==INVALID_FILE_ATTRIBUTES||(flags&FILE_ATTRIBUTE_REPARSE_POINT))return false;
#endif
    return !fs::is_symlink(status)&&(fs::is_directory(status)||fs::is_regular_file(status));
}
void native(audiocpp_status status){if(status!=AUDIOCPP_OK){std::cerr<<audiocpp_last_error()<<'\n';throw Failure("process_failed");}}
void writeWav(const fs::path& path,std::span<const float> samples){
    static_assert(std::endian::native==std::endian::little);
    require(!fs::exists(path)&&samples.size()%2==0&&samples.size()<=44100u*600u*2u,"invalid_output");
    require(std::all_of(samples.begin(),samples.end(),[](float value){return std::isfinite(value);}),"invalid_output");
    std::ofstream stream(path,std::ios::binary);const auto bytes=static_cast<std::uint32_t>(samples.size()*4);
    const auto put=[&](std::uint32_t value,unsigned width){for(unsigned i=0;i<width;++i)stream.put(static_cast<char>((value>>(i*8))&255u));};
    stream.write("RIFF",4);put(bytes+36,4);stream.write("WAVEfmt ",8);put(16,4);put(3,2);put(2,2);put(44100,4);put(44100*8,4);put(8,2);put(32,2);
    stream.write("data",4);put(bytes,4);stream.write(reinterpret_cast<const char*>(samples.data()),static_cast<std::streamsize>(bytes));stream.close();require(bool(stream),"io_error");
}
struct Model {
    const char* id;const char* file;const char* sha;bool sw;
};
constexpr Model models[]{
    {"htdemucs-6s","htdemucs-6stems-f16.gguf","1db9bd58af6995d8caab209c99f5a284212e5bd0654fbfd7e50ce1d5ac140ba9",false},
    {"bs-roformer-sw-6-v1","sw-fixed-f32.gguf",nativeSwPackageSha,true},
};
struct Control {
    ipc::Wire& wire;std::string id;Clock::time_point deadline;
    std::atomic<bool> cancelled{},protocol{},timeout{};
    unsigned pass{1},passes{1};
    void check()const{
        if(protocol.load())throw Failure("protocol_error");
        if(cancelled.load())throw Failure("cancelled");
        if(timeout.load()||Clock::now()>deadline)throw Failure("timeout");
    }
    void phase(const char* phase){check();auto event=ipc::object();ipc::put(event.get(),"phase",phase);ipc::put(event.get(),"device","cpu");wire.send("progress",std::move(event));}
    void progress(std::size_t done,std::size_t total){
        check();require(total>0&&total<=1000000&&done<=total,"invalid_output");
        auto event=ipc::object();ipc::put(event.get(),"phase","inference");ipc::put(event.get(),"device","cpu");
        ipc::put(event.get(),"completed",std::to_string(done));ipc::put(event.get(),"total",std::to_string(total));
        ipc::put(event.get(),"current_pass",std::to_string(pass));ipc::put(event.get(),"passes",std::to_string(passes));wire.send("progress",std::move(event));
    }
    static int callback(std::size_t done,std::size_t total,void* context)noexcept{
        auto& self=*static_cast<Control*>(context);
        try{self.progress(done,total);return 1;}catch(const Failure& failure){
            if(std::string_view(failure.what())=="cancelled")self.cancelled.store(true);
            else if(std::string_view(failure.what())=="timeout")self.timeout.store(true);
            else self.protocol.store(true);
        }catch(...){self.protocol.store(true);}return 0;
    }
    void monitor(std::stop_token stop)noexcept{
        std::vector<char> pending;auto heartbeat=Clock::now();
        try{while(!stop.stop_requested()){
            if(Clock::now()>deadline)timeout.store(true);
            if(wire.readable()){
                std::array<char,4096> block{};const auto count=wire.readSome(block.data(),block.size());pending.insert(pending.end(),block.begin(),block.begin()+static_cast<std::ptrdiff_t>(count));
                require(pending.size()<=65540,"protocol_error");
                if(pending.size()>=4){
                    std::uint32_t size{};for(unsigned i=0;i<4;++i)size|=std::uint32_t(static_cast<unsigned char>(pending[i]))<<(i*8);
                    require(size>0&&size<=65536,"protocol_error");
                    if(pending.size()>=size+4){
                        require(pending.size()==size+4,"protocol_error");const auto command=ipc::parse(std::string_view(pending.data()+4,size));
                        require(ipc::field(command.get(),"type")=="cancel"&&ipc::field(command.get(),"job_id")==id,"protocol_error");cancelled.store(true);return;
                    }
                }
            }
            if(Clock::now()-heartbeat>=std::chrono::seconds(1)){wire.send("heartbeat");heartbeat=Clock::now();}
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }}catch(...){protocol.store(true);}
    }
};
std::vector<std::string> labels(const cJSON* values){
    require(cJSON_IsArray(values)&&cJSON_GetArraySize(values)>0&&cJSON_GetArraySize(values)<=53,"invalid_stems");
    std::vector<std::string> result;
    for(auto* value=values->child;value;value=value->next){auto label=ipc::string(value);require(std::find(result.begin(),result.end(),label)==result.end(),"invalid_stems");result.push_back(std::move(label));}
    return result;
}
std::string lower(std::string text){for(auto& value:text){require((value>='A'&&value<='Z')||(value>='a'&&value<='z'),"invalid_stems");if(value>='A'&&value<='Z')value=static_cast<char>(value-'A'+'a');}return text;}
using NativeOutputs=std::map<std::string,std::vector<float>>;
void copyOutputs(audiocpp_result* result,NativeOutputs& outputs,std::size_t frames,const std::vector<std::string>& selected){
    const auto count=audiocpp_result_named_audio_count(result);require(count>0&&count<=6,"invalid_output");
    std::set<std::string> seen;
    for(std::size_t i=0;i<count;++i){
        const char* name{};const float* pcm{};std::size_t length{};int rate{},channels{};
        native(audiocpp_result_named_audio(result,i,&name,&pcm,&length,&rate,&channels));
        require(name&&pcm&&length==frames&&rate==44100&&channels==2&&seen.insert(name).second,"invalid_output");
        require(std::all_of(pcm,pcm+length*2,[](float value){return std::isfinite(value);}),"invalid_output");
        if(std::find(selected.begin(),selected.end(),name)!=selected.end())require(outputs.emplace(name,std::vector<float>(pcm,pcm+length*2)).second,"invalid_output");
    }
}
NativeOutputs infer(const Model& descriptor,const fs::path& file,std::span<const float> input,int threads,
                    const std::vector<std::string>& selected,const std::vector<std::string>& roles,
                    const std::string& chunk,Control& control){
    control.phase("loading");
    audiocpp_registry* rawRegistry{};native(audiocpp_registry_create(nullptr,&rawRegistry));
    std::unique_ptr<audiocpp_registry,decltype(&audiocpp_registry_free)> registry(rawRegistry,audiocpp_registry_free);
    audiocpp_model* rawModel{};const auto filename=pathText(file);native(audiocpp_model_load(registry.get(),filename.c_str(),nullptr,nullptr,&rawModel));
    std::unique_ptr<audiocpp_model,decltype(&audiocpp_model_free)> model(rawModel,audiocpp_model_free);
    const audiocpp_backend_config backend{"cpu",0,threads};
    const auto frames=input.size()/2;NativeOutputs outputs;
    control.passes=descriptor.sw?static_cast<unsigned>(selected.size()):1;
    for(std::size_t index=0;index<(descriptor.sw?selected.size():1);++index){
        control.check();control.pass=static_cast<unsigned>(index+1);
        std::unique_ptr<audiocpp_options,decltype(&audiocpp_options_free)> options(audiocpp_options_create(),audiocpp_options_free);require(bool(options),"resource_limit");
        const std::size_t segment=chunk=="small"?65024u:130560u;
        if(descriptor.sw){
            const auto role=std::find(roles.begin(),roles.end(),selected[index]);require(role!=roles.end(),"invalid_stems");
            native(audiocpp_options_set(options.get(),"bs_roformer.stem_index",std::to_string(role-roles.begin()+1).c_str()));
            native(audiocpp_options_set(options.get(),"bs_roformer.chunk_size",std::to_string(std::min(segment,frames)).c_str()));
            native(audiocpp_options_set(options.get(),"bs_roformer.num_overlap","1"));
        }
        audiocpp_session* rawSession{};native(audiocpp_session_create(model.get(),"sep","offline",&backend,options.get(),&rawSession));
        std::unique_ptr<audiocpp_session,decltype(&audiocpp_session_free)> session(rawSession,audiocpp_session_free);
        std::unique_ptr<audiocpp_request,decltype(&audiocpp_request_free)> request(audiocpp_request_create(),audiocpp_request_free);require(bool(request),"resource_limit");
        control.phase("inference");
        if(descriptor.sw){
            const auto starts=ipc::swChunkStarts(frames,segment,segment/2);ipc::SwOverlapAdd accumulator(frames,2,segment);control.progress(0,starts.size());
            for(std::size_t n=0;n<starts.size();++n){
                control.check();const auto begin=starts[n],length=std::min(segment,frames-begin);
                native(audiocpp_request_set_audio(request.get(),input.data()+begin*2,length,44100,2));
                audiocpp_result* raw{};native(audiocpp_session_run(session.get(),request.get(),&raw));
                std::unique_ptr<audiocpp_result,decltype(&audiocpp_result_free)> result(raw,audiocpp_result_free);
                require(audiocpp_result_named_audio_count(result.get())==1,"invalid_output");
                const char* name{};const float* pcm{};std::size_t count{};int rate{},channels{};
                native(audiocpp_result_named_audio(result.get(),0,&name,&pcm,&count,&rate,&channels));
                require(name&&selected[index]==name&&pcm&&count==length&&rate==44100&&channels==2,"invalid_output");
                accumulator.add(begin,pcm,length,length/512*512);control.progress(n+1,starts.size());
            }
            outputs.emplace(selected[index],accumulator.finish());
        }else{
            native(audiocpp_request_set_audio(request.get(),input.data(),frames,44100,2));
            native(mrs_audiocpp_request_set_chunk_progress(request.get(),Control::callback,&control));
            audiocpp_result* raw{};const auto status=audiocpp_session_run(session.get(),request.get(),&raw);
            std::unique_ptr<audiocpp_result,decltype(&audiocpp_result_free)> result(raw,audiocpp_result_free);control.check();native(status);copyOutputs(result.get(),outputs,frames,selected);
        }
    }
    require(outputs.size()==selected.size(),"invalid_output");return outputs;
}
Json process(const cJSON* req,ipc::Wire& wire){
    const auto* version=ipc::member(req,"version");require(cJSON_IsNumber(version)&&version->valuedouble==1&&ipc::field(req,"type")=="submit","protocol_error");
    const auto job=ipc::field(req,"job_id");require(shortId(job),"invalid_input");wire.job(job);
    const auto session=ipc::field(req,"session_id"),revision=ipc::field(req,"source_revision");
    require(shortId(session)&&shortId(revision),"invalid_input");
    const auto timeout=ipc::counter(req,"timeout_ms",100,24*3600000),memory=ipc::counter(req,"memory_bytes",256ULL*1024*1024,16ULL*1024*1024*1024);
    Control control{wire,job,Clock::now()+std::chrono::milliseconds(timeout)};
    std::jthread monitor([&](std::stop_token stop){control.monitor(stop);});
    control.phase("validating");
    const auto abi=audiocpp_abi_version();require((abi>>16)==AUDIOCPP_ABI_VERSION_MAJOR&&((abi>>8)&255)>=AUDIOCPP_ABI_VERSION_MINOR,"unsupported_backend");
    require(std::string_view(audiocpp_build_version())==backendVersion,"unsupported_backend");
    const auto id=ipc::field(req,"model");const auto descriptor=std::find_if(std::begin(models),std::end(models),[&](const auto& item){return id==item.id;});
    require(descriptor!=std::end(models),"unsupported_model");
    const auto device=ipc::field(req,"device");require(device=="cpu"||device=="auto","unsupported_device");
    const auto chunk=ipc::field(req,"chunk");require(chunk=="default"||(descriptor->sw&&chunk=="small"),"unsupported_input");
    // Vertical migration slice: explicit rejection until native SRC parity is
    // verified. It is not selected by the production Model Manager/runner.
    const auto projectRate=ipc::counter(req,"project_rate",8000,96000);require(projectRate==44100,"unsupported_input");
    const auto threads=ipc::counter(req,"threads",1,4),start=ipc::counter(req,"source_start"),frames=ipc::counter(req,"source_frames",1,96000ULL*600);
    const auto timeline=ipc::counter(req,"timeline_start");
    const auto source=fs::absolute(pathFrom(ipc::field(req,"source_path")));
    require(fs::is_regular_file(source)&&plain(source)&&fs::file_size(source)<=512ULL*1024*1024,"invalid_input");
    const auto sourceHash=ipc::field(req,"source_sha256");require(sourceHash==mrs::sha256_file(source),"stale_source");
    const auto info=mrs::audio::inspect_wav(source);require(info.sample_rate==44100&&(info.channels==1||info.channels==2),"unsupported_input");
    require(start<=static_cast<std::uint64_t>(info.frame_count)&&frames<=static_cast<std::uint64_t>(info.frame_count)-start&&frames<=44100ULL*600,"invalid_input");
    require(!descriptor->sw||frames>1024,"unsupported_input");
    const std::uint64_t reserve=descriptor->sw?512ULL*1024*1024:1024ULL*1024*1024;
    require(frames*2*4*32+reserve<=memory,"resource_limit");
    auto catalogue=ipc::own(cJSON_Parse(nativeProfilesJson));const cJSON* profile{};
    for(auto* item=catalogue->child;item;item=item->next)if(ipc::field(item,"id")==id){profile=item;break;}
    require(profile!=nullptr,"unsupported_model");const auto available=labels(ipc::member(profile,"stems")),requested=labels(ipc::member(req,"stems"));
    std::vector<std::string> selected,selectedLabels,roles;
    for(const auto& role:labels(ipc::member(profile,"native_stems")))roles.push_back(lower(role));
    for(const auto& label:requested)require(std::find(available.begin(),available.end(),label)!=available.end(),"invalid_stems");
    for(const auto& label:available)if(std::find(requested.begin(),requested.end(),label)!=requested.end()){selectedLabels.push_back(label);selected.push_back(lower(label));}
    const auto model=fs::absolute(pathFrom(ipc::field(req,"model_dir")))/descriptor->file;
    require(fs::is_regular_file(model)&&plain(model)&&mrs::sha256_file(model)==descriptor->sha,"integrity_failure");
    const auto work=fs::current_path(),pending=work/"result.part",finished=work/"result";
    require(plain(work)&&!fs::exists(pending)&&!fs::exists(finished),"io_error");
    control.phase("preparing");
    std::vector<float> input(static_cast<std::size_t>(frames)*2);
    if(info.channels==2)info.read(static_cast<mrs::Sample>(start),input);
    else{
        std::vector<float> mono(static_cast<std::size_t>(frames));info.read(static_cast<mrs::Sample>(start),mono);
        for(std::size_t i=0;i<mono.size();++i)input[i*2]=input[i*2+1]=mono[i];
    }
    require(std::all_of(input.begin(),input.end(),[](float value){return std::isfinite(value);}),"invalid_input");
    require(mrs::sha256_file(source)==sourceHash,"stale_source");control.check();
    writeWav(work/"snapshot.wav",input);const auto snapshotHash=mrs::sha256_file(work/"snapshot.wav");
    auto outputs=infer(*descriptor,model,input,static_cast<int>(threads),selected,roles,chunk,control);input.clear();input.shrink_to_fit();
    control.phase("verifying");require(fs::create_directory(pending),"io_error");bool owned=true;
    try{
        auto manifest=ipc::object(),entries=ipc::array(),weights=ipc::object(),selection=ipc::array();
        for(const auto& label:selectedLabels)ipc::append(selection.get(),ipc::own(cJSON_CreateString(label.c_str())));
        for(auto* file=ipc::member(profile,"files")->child;file;file=file->next)ipc::put(weights.get(),ipc::field(file,"name").c_str(),ipc::field(file,"sha256"));
        for(const auto& role:selected){
            control.check();const auto& audio=outputs.at(role);require(audio.size()==frames*2,"invalid_output");
            const auto file=pending/(role+".wav");writeWav(file,audio);auto entry=ipc::object();
            ipc::put(entry.get(),"stem",role);ipc::put(entry.get(),"path",role+".wav");ipc::put(entry.get(),"sha256",mrs::sha256_file(file));
            ipc::put(entry.get(),"bytes",std::to_string(fs::file_size(file)));ipc::put(entry.get(),"frames",std::to_string(frames));
            ipc::put(entry.get(),"rate","44100");ipc::put(entry.get(),"channels",2);ipc::put(entry.get(),"format","WAV_FLOAT32");ipc::put(entry.get(),"origin","0");ipc::append(entries.get(),std::move(entry));
        }
        ipc::put(manifest.get(),"version",1);ipc::put(manifest.get(),"job_id",job);ipc::put(manifest.get(),"session_id",session);
        ipc::put(manifest.get(),"source_revision",revision);ipc::put(manifest.get(),"source_sha256",sourceHash);
        ipc::put(manifest.get(),"source_start",std::to_string(start));ipc::put(manifest.get(),"source_frames",std::to_string(frames));
        ipc::put(manifest.get(),"source_rate","44100");ipc::put(manifest.get(),"timeline_start",std::to_string(timeline));ipc::put(manifest.get(),"project_rate","44100");
        ipc::put(manifest.get(),"snapshot_sha256",snapshotHash);ipc::put(manifest.get(),"model",id);
        ipc::attach(manifest.get(),"available_stems",ipc::duplicate(ipc::member(profile,"stems")));ipc::attach(manifest.get(),"selected_stems",std::move(selection));
        ipc::attach(manifest.get(),"weights",std::move(weights));ipc::attach(manifest.get(),"outputs",std::move(entries));
        ipc::put(manifest.get(),"backend","audio.cpp");ipc::put(manifest.get(),"backend_version",backendVersion);
        ipc::put(manifest.get(),"requested_device",device);ipc::put(manifest.get(),"effective_device","cpu");ipc::put(manifest.get(),"precision","fp32");
        ipc::put(manifest.get(),"weight_precision",descriptor->sw?"fp32":"fp16");ipc::put(manifest.get(),"native_package_sha256",descriptor->sha);
        ipc::put(manifest.get(),"gain_policy","unnormalized_float");ipc::put(manifest.get(),"derivation","model-native");ipc::put(manifest.get(),"chunk",chunk);
        ipc::put(manifest.get(),"source_channels",static_cast<int>(info.channels));ipc::put(manifest.get(),"mono_policy","duplicate_to_stereo");
        ipc::put(manifest.get(),"model_rate",44100);ipc::put(manifest.get(),"shifts",0);ipc::put(manifest.get(),"seed",0);ipc::put(manifest.get(),"additive_outputs",true);
        require(cJSON_AddNumberToObject(manifest.get(),"overlap",descriptor->sw?2.0:0.25)!=nullptr,"resource_limit");
        const auto body=ipc::dump(manifest.get());require(body.size()<=49152,"invalid_output");
        std::ofstream stream(pending/"manifest.json",std::ios::binary);stream.write(body.data(),static_cast<std::streamsize>(body.size()));stream.close();require(bool(stream),"io_error");
        require(mrs::sha256_file(source)==sourceHash,"stale_source");control.check();fs::rename(pending,finished);owned=false;
        return manifest;
    }catch(...){if(owned){std::error_code ignored;fs::remove_all(pending,ignored);}throw;}
}
}
int main(){
    try{
        ipc::Wire wire;wire.send("hello");
        try{auto request=wire.receive();auto manifest=process(request.get(),wire);auto event=ipc::object();ipc::attach(event.get(),"manifest",std::move(manifest));wire.send("result",std::move(event));return 0;}
        catch(const Failure& failure){auto event=ipc::object();ipc::put(event.get(),"code",failure.what());wire.send("error",std::move(event));return 1;}
        catch(const std::bad_alloc&){auto event=ipc::object();ipc::put(event.get(),"code","resource_limit");wire.send("error",std::move(event));return 1;}
        catch(const std::exception& error){std::cerr<<error.what()<<'\n';auto event=ipc::object();ipc::put(event.get(),"code","process_failed");wire.send("error",std::move(event));return 1;}
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
