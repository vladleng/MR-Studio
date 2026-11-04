#include "native_resample.hpp"
#include "native_ipc.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace src=mrs::separation::native;
std::vector<float> samples(const cJSON* values){
    src::require(cJSON_IsArray(values)&&cJSON_GetArraySize(values)>0,"Invalid SRC fixture samples");
    std::vector<float> output;
    for(auto* value=values->child;value;value=value->next){
        src::require(cJSON_IsNumber(value)&&std::isfinite(value->valuedouble),"Non-finite SRC fixture");output.push_back(static_cast<float>(value->valuedouble));
    }
    return output;
}
unsigned integer(const cJSON* obj,const char* key){
    const auto* value=src::member(obj,key);src::require(cJSON_IsNumber(value),"Missing SRC fixture geometry");return static_cast<unsigned>(value->valuedouble);
}
template<class F>void rejects(F f){try{f();}catch(const std::invalid_argument&){return;}throw std::runtime_error("Invalid SRC geometry accepted");}
int run(const char* filename){
    std::ifstream stream(filename,std::ios::binary);src::require(bool(stream),"SRC fixture missing");
    const std::string text((std::istreambuf_iterator<char>(stream)),std::istreambuf_iterator<char>());
    const auto fixture=src::own(cJSON_ParseWithLength(text.data(),text.size()));
    const auto* cases=src::member(fixture.get(),"cases");src::require(cJSON_IsArray(cases)&&cJSON_GetArraySize(cases)==19,"Unexpected SRC fixture cases");
    float largest{};
    for(auto* item=cases->child;item;item=item->next){
        const auto input=samples(src::member(item,"input")),expected=samples(src::member(item,"expected"));
        const auto channels=integer(item,"channels"),from=integer(item,"source_rate"),to=integer(item,"target_rate");
        src::require(channels>0&&input.size()%channels==0&&expected.size()%channels==0,"SRC fixture alignment");
        const auto count=src::separationFrameCount(input.size()/channels,from,to);
        src::require(count==expected.size()/channels,"Source/project frame rounding differs");
        const auto actual=src::separationResample(input,channels,from,to,static_cast<std::size_t>(count));
        src::require(actual.size()==expected.size(),"SRC result length differs");
        float difference{};
        for(std::size_t i=0;i<actual.size();++i){
            difference=std::max(difference,std::abs(actual[i]-expected[i]));
            if(from==to)src::require(std::bit_cast<std::uint32_t>(actual[i])==std::bit_cast<std::uint32_t>(input[i]),"Identity SRC changed Float32 bits");
        }
        if(difference>2e-6f)throw std::runtime_error("SRC waveform differs from SciPy: "+src::field(item,"name")+" error="+std::to_string(difference));
        largest=std::max(largest,difference);
    }
    const std::vector<float> valid{0.1f,-0.2f,0.3f,0.4f};
    rejects([&]{(void)src::separationResample(valid,0,44100,48000,2);});
    rejects([&]{(void)src::separationResample(valid,2,0,48000,2);});
    rejects([&]{(void)src::separationResample(valid,2,44100,48000,4);});
    rejects([&]{(void)src::separationResample(valid,2,44100,48000,0);});
    auto bad=valid;bad.front()=std::numeric_limits<float>::quiet_NaN();rejects([&]{(void)src::separationResample(bad,2,44100,48000,2);});
    std::vector<float> longInput(50000,0.1f);unsigned checks{};bool cancelled{};
    try{(void)src::separationResample(longInput,2,48000,44100,20000,[&]{if(++checks>=4)throw std::runtime_error("unit SRC cancelled");});}
    catch(const std::runtime_error& error){cancelled=std::string_view(error.what())=="unit SRC cancelled";}
    src::require(cancelled,"SRC cancellation was swallowed");
    std::cout<<"PASS 19 SciPy Float32 SRC goldens, identity, gain, timebase, bounds/cancel; max_error="<<largest<<'\n';return 0;
}
int main(int argc,char** argv){try{src::require(argc==2,"Usage: SRC-test reference.json");return run(argv[1]);}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
