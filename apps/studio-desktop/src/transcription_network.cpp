#include <mrs/transcription_models.hpp>
#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace mrs::transcription {
namespace {
void need(bool ok,const char* error){if(!ok)throw std::runtime_error(error);}
#ifdef _WIN32
struct Internet {HINTERNET value{};~Internet(){if(value)WinHttpCloseHandle(value);}};
std::wstring wide(std::string_view value){return {value.begin(),value.end()};}
void fetch(const std::string& initial,const std::filesystem::path& target,std::uint64_t expected,
           std::stop_token stop,const ModelDownloadProgress& progress,const std::string& token){
    auto url=initial;
    for(int redirect=0;redirect<5;++redirect){
        need(!stop.stop_requested(),"Download cancelled");const auto endpoint=model_download_target(url);
        Internet session{WinHttpOpen(L"MR-Studio-MuScriptor/0.10.1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
        need(session.value!=nullptr,"Cannot start HTTPS download");need(WinHttpSetTimeouts(session.value,5000,5000,5000,10000)!=FALSE,"Cannot set download timeout");
        Internet connection{WinHttpConnect(session.value,wide(endpoint.host).c_str(),443,0)};need(connection.value!=nullptr,"Cannot connect to model host");
        Internet request{WinHttpOpenRequest(connection.value,L"GET",wide(endpoint.path).c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
        need(request.value!=nullptr,"Cannot create model request");
        DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;need(WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy))!=FALSE,"Cannot enforce redirect policy");
        // Never use WinHTTP automatic redirects: signed CDN requests must not
        // receive HF Authorization. No URL/query/header is written to logs.
        const auto headers=endpoint.authorize&&!token.empty()?wide("Authorization: Bearer "+token+"\r\n"):std::wstring{};
        need(WinHttpSendRequest(request.value,headers.empty()?WINHTTP_NO_ADDITIONAL_HEADERS:headers.c_str(),static_cast<DWORD>(headers.size()),WINHTTP_NO_REQUEST_DATA,0,0,0)!=FALSE&&WinHttpReceiveResponse(request.value,nullptr)!=FALSE,"Model network request failed");
        DWORD status{},size=sizeof(status);need(WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)!=FALSE,"Cannot read model response");
        if(status==401||status==403)throw std::runtime_error("Official access denied. Accept the model terms on Hugging Face and use an authorized read token.");
        if(status>=300&&status<400){
            DWORD bytes{};WinHttpQueryHeaders(request.value,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,nullptr,&bytes,WINHTTP_NO_HEADER_INDEX);
            need(bytes>0&&bytes<65536,"Invalid model redirect");std::vector<wchar_t> location(bytes/sizeof(wchar_t)+1);
            need(WinHttpQueryHeaders(request.value,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,location.data(),&bytes,WINHTTP_NO_HEADER_INDEX)!=FALSE,"Cannot read model redirect");
            const std::wstring_view next(location.data());need(std::all_of(next.begin(),next.end(),[](wchar_t c){return c>32&&c<127;}),"Invalid model redirect");
            std::string ascii;ascii.reserve(next.size());for(const auto c:next)ascii.push_back(static_cast<char>(c));
            url=model_download_redirect(url,ascii);continue;
        }
        need(status==200,"Official model server returned an error");need(!std::filesystem::exists(target),"Download target already exists");
        std::ofstream output(target,std::ios::binary);need(output.good(),"Cannot create model download file");
        std::array<char,65536> buffer{};std::uint64_t total{};
        for(;;){
            need(!stop.stop_requested(),"Download cancelled");DWORD count{};
            need(WinHttpReadData(request.value,buffer.data(),static_cast<DWORD>(buffer.size()),&count)!=FALSE,"Model read failed");
            if(!count)break;need(count<=expected-total,"Model download exceeded pinned size");total+=count;
            output.write(buffer.data(),count);need(output.good(),"Cannot write model download");if(progress)progress(total,expected);
        }
        output.close();need(output.good()&&total==expected,"Model download size mismatch");return;
    }
    throw std::runtime_error("Too many model redirects");
}
#endif
}
ModelFetch authenticated_model_fetch(std::string token){
    need(token.size()<=1024&&std::all_of(token.begin(),token.end(),[](unsigned char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-';}),"Invalid HF token format");
#ifdef _WIN32
    return [token=std::move(token)](const std::string& url,const std::filesystem::path& target,std::uint64_t bytes,std::stop_token stop,const ModelDownloadProgress& progress){fetch(url,target,bytes,stop,progress,token);};
#else
    (void)token;throw std::runtime_error("Official model download transport is available on Windows");
#endif
}
}
