#pragma once
// Existing uint32-LE / bounded JSON wire. Worker/control threads only.
#include <cJSON.h>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <cerrno>
#include <poll.h>
#include <unistd.h>
#endif

namespace mrs::separation::native {
struct Failure : std::runtime_error { explicit Failure(const char* code):std::runtime_error(code){} };
inline void require(bool ok,const char* code){if(!ok)throw Failure(code);}
using Json=std::unique_ptr<cJSON,decltype(&cJSON_Delete)>;
inline Json own(cJSON* value){require(value!=nullptr,"resource_limit");return {value,cJSON_Delete};}
inline Json object(){return own(cJSON_CreateObject());}
inline Json array(){return own(cJSON_CreateArray());}
inline const cJSON* member(const cJSON* value,const char* key){return cJSON_GetObjectItemCaseSensitive(value,key);}
inline std::string string(const cJSON* value){require(cJSON_IsString(value)&&value->valuestring,"invalid_input");return value->valuestring;}
inline std::string field(const cJSON* value,const char* key){return string(member(value,key));}
inline void put(cJSON* obj,const char* key,const std::string& value){require(cJSON_AddStringToObject(obj,key,value.c_str())!=nullptr,"resource_limit");}
inline void put(cJSON* obj,const char* key,const char* value){put(obj,key,std::string(value));}
inline void put(cJSON* obj,const char* key,int value){require(cJSON_AddNumberToObject(obj,key,value)!=nullptr,"resource_limit");}
inline void put(cJSON* obj,const char* key,bool value){require(cJSON_AddBoolToObject(obj,key,value)!=nullptr,"resource_limit");}
inline void attach(cJSON* obj,const char* key,Json value){require(cJSON_AddItemToObject(obj,key,value.get())!=0,"resource_limit");(void)value.release();}
inline void append(cJSON* obj,Json value){require(cJSON_AddItemToArray(obj,value.get())!=0,"resource_limit");(void)value.release();}
inline Json duplicate(const cJSON* value){return own(cJSON_Duplicate(value,1));}
inline std::string dump(const cJSON* obj){
    char* raw=cJSON_PrintUnformatted(obj);require(raw!=nullptr,"resource_limit");
    std::unique_ptr<char,decltype(&cJSON_free)> bytes(raw,cJSON_free);return raw;
}
inline bool utf8(std::string_view text){
    for(std::size_t i=0;i<text.size();){
        auto ch=static_cast<unsigned char>(text[i++]);if(ch<128){if(!ch)return false;continue;}
        unsigned count{},code{},minimum{};
        if(ch>=0xc2&&ch<=0xdf){count=1;code=ch&31u;minimum=0x80;}
        else if(ch>=0xe0&&ch<=0xef){count=2;code=ch&15u;minimum=0x800;}
        else if(ch>=0xf0&&ch<=0xf4){count=3;code=ch&7u;minimum=0x10000;}
        else return false;
        if(count>text.size()-i)return false;
        for(unsigned j=0;j<count;++j){ch=static_cast<unsigned char>(text[i++]);if((ch&0xc0)!=0x80)return false;code=(code<<6)|(ch&63u);}
        if(code<minimum||code>0x10ffff||(code>=0xd800&&code<=0xdfff))return false;
    }
    return true;
}
inline void checkTree(const cJSON* value,unsigned depth,std::size_t& nodes){
    require(value&&depth<=32&&++nodes<=4096,"protocol_error");
    if(cJSON_IsNumber(value))require(std::isfinite(value->valuedouble),"protocol_error");
    if(cJSON_IsObject(value)){
        std::set<std::string> keys;
        for(auto* child=value->child;child;child=child->next){
            require(child->string&&keys.insert(child->string).second,"protocol_error");checkTree(child,depth+1,nodes);
        }
    }else if(cJSON_IsArray(value))for(auto* child=value->child;child;child=child->next)checkTree(child,depth+1,nodes);
}
inline Json parse(std::string_view bytes){
    require(!bytes.empty()&&bytes.size()<=65536&&utf8(bytes),"protocol_error");
    // cJSON strings cannot represent embedded NUL. Respect escaped backslashes.
    bool inString{};
    for(std::size_t i=0;i<bytes.size();++i){
        if(bytes[i]=='"')inString=!inString;
        else if(inString&&bytes[i]=='\\'){
            if(i+5<bytes.size()&&bytes.substr(i,6)=="\\u0000")throw Failure("protocol_error");
            if(i+1<bytes.size())++i;
        }else if(inString)require(static_cast<unsigned char>(bytes[i])>=32,"protocol_error");
    }
    const char* end{};auto* decoded=cJSON_ParseWithLengthOpts(bytes.data(),bytes.size(),&end,0);
    require(decoded!=nullptr,"protocol_error");auto value=own(decoded);
    require(end!=nullptr,"protocol_error");
    while(end<bytes.data()+bytes.size()&&(*end==' '||*end=='\t'||*end=='\r'||*end=='\n'))++end;
    require(end==bytes.data()+bytes.size()&&cJSON_IsObject(value.get()),"protocol_error");
    std::size_t nodes{};checkTree(value.get(),0,nodes);return value;
}
inline std::uint64_t counter(const cJSON* obj,const char* key,std::uint64_t low=0,std::uint64_t high=9007199254740992ULL){
    const auto text=field(obj,key);require(!text.empty()&&text.size()<=20&&(text.size()==1||text.front()!='0'),"invalid_input");
    for(char ch:text)require(ch>='0'&&ch<='9',"invalid_input");
    std::uint64_t value{};const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    require(result.ec==std::errc{}&&result.ptr==text.data()+text.size()&&value>=low&&value<=high,"invalid_input");return value;
}
class Wire {
public:
    Wire(){
#ifdef _WIN32
        input_=GetStdHandle(STD_INPUT_HANDLE);
        require(DuplicateHandle(GetCurrentProcess(),GetStdHandle(STD_OUTPUT_HANDLE),GetCurrentProcess(),&output_,0,FALSE,DUPLICATE_SAME_ACCESS)!=0,"io_error");
        require(_dup2(_fileno(stderr),_fileno(stdout))==0,"io_error");
        require(SetStdHandle(STD_OUTPUT_HANDLE,GetStdHandle(STD_ERROR_HANDLE))!=0,"io_error");
#else
        output_=::dup(STDOUT_FILENO);require(output_>=0&&::dup2(STDERR_FILENO,STDOUT_FILENO)>=0,"io_error");
#endif
    }
    ~Wire(){
#ifdef _WIN32
        if(output_)CloseHandle(output_);
#else
        if(output_>=0)::close(output_);
#endif
    }
    Wire(const Wire&)=delete;Wire& operator=(const Wire&)=delete;
    Json receive(){
        char header[4];readExact(header,4);std::uint32_t size{};
        for(unsigned i=0;i<4;++i)size|=std::uint32_t(static_cast<unsigned char>(header[i]))<<(i*8);
        require(size&&size<=65536,"protocol_error");std::string bytes(size,'\0');readExact(bytes.data(),size);return parse(bytes);
    }
    bool readable(){
#ifdef _WIN32
        DWORD bytes{};require(PeekNamedPipe(input_,nullptr,0,nullptr,&bytes,nullptr)!=0,"protocol_error");return bytes!=0;
#else
        pollfd descriptor{STDIN_FILENO,POLLIN,0};const auto status=::poll(&descriptor,1,0);
        require(status>=0||errno==EINTR,"protocol_error");return status>0;
#endif
    }
    // Pollers must not call receive() after a partial frame; accumulate bounded
    // chunks and parse only after the entire frame is available.
    std::size_t readSome(char* output,std::size_t capacity){
#ifdef _WIN32
        DWORD bytes{};require(ReadFile(input_,output,static_cast<DWORD>(capacity),&bytes,nullptr)!=0&&bytes>0,"protocol_error");return bytes;
#else
        const auto bytes=::read(STDIN_FILENO,output,capacity);require(bytes>0,"protocol_error");return static_cast<std::size_t>(bytes);
#endif
    }
    void job(std::string id){std::lock_guard lock(mutex_);job_=std::move(id);}
    void send(const char* type,Json body=object()){
        std::lock_guard lock(mutex_);put(body.get(),"version",1);put(body.get(),"type",std::string(type));
        put(body.get(),"sequence",std::to_string(++sequence_));put(body.get(),"job_id",job_);
        const auto bytes=dump(body.get());require(!bytes.empty()&&bytes.size()<=65536,"protocol_error");
        const auto size=static_cast<std::uint32_t>(bytes.size());char header[4];for(unsigned i=0;i<4;++i)header[i]=static_cast<char>((size>>(i*8))&255u);
        writeExact(header,4);writeExact(bytes.data(),bytes.size());
    }
private:
    void readExact(char* dest,std::size_t bytes){while(bytes){const auto count=readSome(dest,bytes);dest+=count;bytes-=count;}}
    void writeExact(const char* data,std::size_t bytes){
        while(bytes){
#ifdef _WIN32
            DWORD count{};require(WriteFile(output_,data,static_cast<DWORD>(bytes),&count,nullptr)!=0&&count>0,"io_error");
#else
            const auto count=::write(output_,data,bytes);if(count<0&&errno==EINTR)continue;require(count>0,"io_error");
#endif
            data+=count;bytes-=static_cast<std::size_t>(count);
        }
    }
    std::mutex mutex_;std::string job_;std::uint64_t sequence_{};
#ifdef _WIN32
    HANDLE input_{},output_{};
#else
    int output_{-1};
#endif
};
}
