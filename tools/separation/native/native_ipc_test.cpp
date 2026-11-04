#include "native_ipc.hpp"
#include <iostream>
namespace ipc=mrs::separation::native;
template<class F>void rejects(F f,const char* code="protocol_error"){
    try{f();}catch(const ipc::Failure& failure){ipc::require(std::string_view(failure.what())==code,"Unexpected error classification");return;}
    throw std::runtime_error("Invalid input accepted");
}
int main(){
    try{
        auto value=ipc::parse("{\"counter\":\"9007199254740992\",\"path\":\"C:\\\\u0000\\\\audio.wav\",\"music\":\"\\ud83c\\udfb5\"}");
        ipc::require(ipc::counter(value.get(),"counter")==9007199254740992ULL,"Counter precision");
        ipc::require(ipc::field(value.get(),"music")=="\xf0\x9f\x8e\xb5","Unicode surrogate decoding");
        auto roundtrip=ipc::parse(ipc::dump(value.get()));ipc::require(ipc::field(roundtrip.get(),"path")=="C:\\u0000\\audio.wav","Escaped path altered");
        for(const auto* bytes:{"{} trailing","[]","null","{\"a\":1,\"a\":2}","{\"a\":1,\"\\u0061\":2}","{\"a\":\"\\u0000\"}","{\"a\":\"\\ud800\"}","{\"a\":1e999}","{\"a\":NaN}"})
            rejects([&]{(void)ipc::parse(bytes);});
        rejects([]{(void)ipc::parse(std::string("{\"a\":\"")+char(0xc0)+char(0x80)+"\"}");});
        rejects([]{(void)ipc::parse("{\"a\":\"raw\nnewline\"}");});
        rejects([]{(void)ipc::parse(std::string(65537,' '));});
        std::string deep="{}";for(int i=0;i<40;++i)deep="{\"a\":"+deep+"}";rejects([&]{(void)ipc::parse(deep);});
        std::string many="{\"a\":[0";for(int i=0;i<4100;++i)many+=",0";many+="]}";rejects([&]{(void)ipc::parse(many);});
        for(const auto* text:{"00","-1","+1","1.0","9007199254740993","18446744073709551616"}){
            auto obj=ipc::object();ipc::put(obj.get(),"value",text);rejects([&]{(void)ipc::counter(obj.get(),"value");},"invalid_input");
        }
        auto wrong=ipc::parse("{\"value\":1}");rejects([&]{(void)ipc::counter(wrong.get(),"value");},"invalid_input");
        std::cout<<"PASS strict JSON/counters/Unicode/duplicate keys/depth/frame limits\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
