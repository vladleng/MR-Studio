// Real C API extension regression: no RT work, DAW, Python or fake inference.
#include <audiocpp.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>
#ifndef AUDIOCPP_MRS_CHUNK_PROGRESS_VERSION
#error This test requires the pinned MR chunk-progress extension
#endif
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void check(audiocpp_status status){if(status!=AUDIOCPP_OK)throw std::runtime_error(audiocpp_last_error());}
struct Progress {
    std::size_t total{},completed{},calls{};
    bool rejectStart{},rejectEnd{},invalid{};
    static int callback(std::size_t done,std::size_t count,void* context) noexcept {
        auto& self=*static_cast<Progress*>(context);
        if(!count||count>1000000||done>count||(self.calls&&count!=self.total)||
           (!self.calls&&done!=0)||(self.calls&&done!=self.completed+1)){
            self.invalid=true;return 0;
        }
        self.total=count;self.completed=done;++self.calls;
        return (!self.rejectStart||done!=0)&&(!self.rejectEnd||done!=count)?1:0;
    }
};
std::vector<float> validate(audiocpp_result* result){
    require(result&&audiocpp_result_named_audio_count(result)==6,"Missing Demucs results");
    std::vector<float> output;
    for(std::size_t i=0;i<6;++i){
        const char* name{};const float* pcm{};std::size_t frames{};int rate{},channels{};
        check(audiocpp_result_named_audio(result,i,&name,&pcm,&frames,&rate,&channels));
        require(name&&pcm&&frames==44100&&rate==44100&&channels==2,"Result geometry changed");
        require(std::all_of(pcm,pcm+frames*2,[](float x){return std::isfinite(x);}),"Non-finite result");
        output.insert(output.end(),pcm,pcm+frames*2);
    }
    return output;
}
int run(const std::filesystem::path& path){
    audiocpp_registry* rawRegistry{};check(audiocpp_registry_create(nullptr,&rawRegistry));
    std::unique_ptr<audiocpp_registry,decltype(&audiocpp_registry_free)> registry(rawRegistry,audiocpp_registry_free);
    const auto utf8=path.u8string();const std::string modelPath(utf8.begin(),utf8.end());
    audiocpp_model* rawModel{};check(audiocpp_model_load(registry.get(),modelPath.c_str(),nullptr,nullptr,&rawModel));
    std::unique_ptr<audiocpp_model,decltype(&audiocpp_model_free)> model(rawModel,audiocpp_model_free);
    const audiocpp_backend_config backend{"cpu",0,2};audiocpp_session* rawSession{};
    check(audiocpp_session_create(model.get(),"sep","offline",&backend,nullptr,&rawSession));
    std::unique_ptr<audiocpp_session,decltype(&audiocpp_session_free)> session(rawSession,audiocpp_session_free);
    std::unique_ptr<audiocpp_request,decltype(&audiocpp_request_free)> request(audiocpp_request_create(),audiocpp_request_free);
    require(bool(request),"Request allocation failed");
    std::vector<float> source(44100*2);
    for(std::size_t i=0;i<44100;++i){const auto v=static_cast<float>(0.1*std::sin(2*std::numbers::pi*110*static_cast<double>(i)/44100));source[i*2]=v;source[i*2+1]=v*0.9f;}
    const auto before=source;check(audiocpp_request_set_audio(request.get(),source.data(),44100,44100,2));
    Progress start;start.rejectStart=true;
    check(mrs_audiocpp_request_set_chunk_progress(request.get(),Progress::callback,&start));
    audiocpp_result* result{};
    require(audiocpp_session_run(session.get(),request.get(),&result)!=AUDIOCPP_OK&&!result,"Start cancellation returned a result");
    require(start.calls==1&&!start.invalid,"Start cancellation ordering");
    Progress normal;check(mrs_audiocpp_request_set_chunk_progress(request.get(),Progress::callback,&normal));
    check(audiocpp_session_run(session.get(),request.get(),&result));
    std::unique_ptr<audiocpp_result,decltype(&audiocpp_result_free)> first(result,audiocpp_result_free);
    const auto expected=validate(first.get());
    require(!normal.invalid&&normal.completed==normal.total&&normal.calls==normal.total+1,"Completed chunks are not exact");
    Progress end;end.rejectEnd=true;
    check(mrs_audiocpp_request_set_chunk_progress(request.get(),Progress::callback,&end));
    result=nullptr;
    require(audiocpp_session_run(session.get(),request.get(),&result)!=AUDIOCPP_OK&&!result,"End cancellation returned a result");
    require(!end.invalid&&end.completed==end.total,"End cancellation ordering");
    check(mrs_audiocpp_request_set_chunk_progress(request.get(),nullptr,nullptr));
    check(audiocpp_session_run(session.get(),request.get(),&result));
    std::unique_ptr<audiocpp_result,decltype(&audiocpp_result_free)> last(result,audiocpp_result_free);
    require(validate(last.get())==expected,"Cancel/reset changed reused model output");
    require(source==before,"Input modified");
    std::cout<<"PASS real Demucs chunk progress, start/end cancel, reset and session reuse\n";
    return 0;
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
#else
int main(int argc,char** argv){
#endif
    try{require(argc==2,"Usage: progress-test HTDEMUC6.gguf");return run(std::filesystem::path(argv[1]));}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
