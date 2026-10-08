#include "protocol.hpp"
#include <mrs/diagnostics.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>
namespace mrs::diagnostics {
namespace {
Shared* crashData{};HANDLE crashSignal{},crashDone{};
LONG WINAPI capture(EXCEPTION_POINTERS* fault) noexcept {
    auto* data=crashData;
    if(!data||!fault||!fault->ExceptionRecord||!fault->ContextRecord)return EXCEPTION_CONTINUE_SEARCH;
    // Fatal path only: fixed storage and kernel signalling. No heap, logger,
    // project/plugin access, UI or DbgHelp inside the damaged process.
    if(InterlockedCompareExchange(&data->state,-1,0)!=0)return EXCEPTION_EXECUTE_HANDLER;
    data->thread=GetCurrentThreadId();data->exception=*fault->ExceptionRecord;
    data->exception.ExceptionRecord=nullptr;data->context=*fault->ContextRecord;
    InterlockedExchange(&data->state,1);SetEvent(crashSignal);
    WaitForSingleObject(crashDone,8000); // bounded, only after a fatal exception
    return EXCEPTION_EXECUTE_HANDLER;
}
void close(HANDLE& h){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);h=nullptr;}
std::wstring handleText(HANDLE h){return std::to_wstring(reinterpret_cast<std::uintptr_t>(h));}
std::string clean(std::string_view text){std::string result(text.substr(0,2048));for(auto& c:result)if(c=='\r'||c=='\n'||c=='\0')c=' ';return result;}
struct Group {std::vector<std::filesystem::path> files;std::uintmax_t size{};std::filesystem::file_time_type time{};};
void prune(const std::filesystem::path& directory){
    std::map<std::wstring,Group> groups;std::error_code ec;
    for(const auto& entry:std::filesystem::directory_iterator(directory,ec)){
        if(entry.is_symlink(ec)||!entry.is_regular_file(ec))continue;
        const auto name=entry.path().filename().wstring();if(!name.starts_with(L"mr-session-"))continue;
        const auto dot=name.find(L'.');if(dot==std::wstring::npos)continue;
        const auto suffix=name.substr(dot);if(suffix!=L".log"&&suffix!=L".crash.txt"&&suffix!=L".dmp"&&suffix!=L".lock")continue;
        ec.clear();const auto bytes=entry.file_size(ec);if(ec)continue;const auto time=entry.last_write_time(ec);if(ec)continue;
        auto& group=groups[name.substr(0,dot)];group.files.push_back(entry.path());group.size+=bytes;group.time=std::max(group.time,time);
    }
    std::vector<std::pair<std::wstring,Group>> ordered(groups.begin(),groups.end());
    std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.second.time<b.second.time;});
    std::uintmax_t total{};for(const auto& g:ordered)total+=g.second.size;auto remaining=ordered.size();
    for(const auto& [stem,group]:ordered){
        if(remaining<20&&total<256ULL*1024*1024)break;
        const auto lock=directory/(stem+L".lock");
        HANDLE probe=CreateFileW(lock.c_str(),DELETE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
        if(probe==INVALID_HANDLE_VALUE&&GetLastError()!=ERROR_FILE_NOT_FOUND)continue;
        if(probe!=INVALID_HANDLE_VALUE)CloseHandle(probe);
        bool removed=true;for(const auto& file:group.files){ec.clear();std::filesystem::remove(file,ec);removed=removed&&!ec;}
        if(removed){--remaining;total-=group.size;}
    }
}
}
struct Session::Impl {
    std::filesystem::path directory,stem,previous;
    HANDLE mapping{},signal{},done{},process{},lock{},parent{};
    Shared* shared{};LPTOP_LEVEL_EXCEPTION_FILTER oldFilter{};bool installed{};std::string application;
    std::ofstream output;std::mutex mutex;std::condition_variable wake;
    std::deque<std::string> pending;std::thread writer;bool stopping{};
    std::atomic<unsigned> dropped{};std::atomic<bool> journalFailed{};std::size_t bytes{};
    std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
    void push(std::string_view text) noexcept {
        try{std::unique_lock guard(mutex,std::try_to_lock);if(!guard||pending.size()>=128){++dropped;return;}
            pending.push_back(std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count())+"ms tid="+std::to_string(GetCurrentThreadId())+" "+clean(text));wake.notify_one();}catch(...){++dropped;}
    }
    void write(){
        for(;;){std::deque<std::string> batch;bool end;
            {std::unique_lock guard(mutex);wake.wait_for(guard,std::chrono::milliseconds(250),[&]{return stopping||!pending.empty();});batch.swap(pending);end=stopping;}
            const auto lost=dropped.exchange(0);if(lost)batch.push_back("diagnostic_events_dropped="+std::to_string(lost));
            for(const auto& line:batch)if(bytes<2*1024*1024){output<<line<<'\n';bytes+=line.size()+1;if(bytes>=2*1024*1024)output<<"session_log_limit_reached\n";}
            output.flush();if(!output)journalFailed=true;if(end)break;
        }
    }
    bool startHelper(const std::filesystem::path& helper){
        if(!std::filesystem::is_regular_file(helper))return false;
        SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
        mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,&security,PAGE_READWRITE,0,sizeof(Shared),nullptr);
        signal=CreateEventW(&security,TRUE,FALSE,nullptr);done=CreateEventW(&security,TRUE,FALSE,nullptr);
        parent=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,GetCurrentProcessId());
        if(!mapping||!signal||!done||!parent)return false;
        SetHandleInformation(parent,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT);
        shared=static_cast<Shared*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));if(!shared)return false;
        *shared=Shared{};
        const auto version=clean(application);memcpy(shared->application,version.data(),std::min(version.size(),sizeof(shared->application)-1));
        // Inherit only the four diagnostic handles, never device/project handles.
        SIZE_T size{};InitializeProcThreadAttributeList(nullptr,1,0,&size);
        std::vector<std::byte> storage(size);auto* attrs=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if(!InitializeProcThreadAttributeList(attrs,1,0,&size))return false;
        HANDLE handles[]{mapping,signal,done,parent};
        const auto updated=UpdateProcThreadAttribute(attrs,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,handles,sizeof(handles),nullptr,nullptr);
        STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.lpAttributeList=attrs;
        startup.StartupInfo.dwFlags=STARTF_USESHOWWINDOW;startup.StartupInfo.wShowWindow=SW_HIDE;
        PROCESS_INFORMATION child{};
        auto command=L"\""+helper.wstring()+L"\" --watch "+handleText(mapping)+L" "+handleText(signal)+L" "+handleText(done)+L" "+handleText(parent)+L" \""+stem.wstring()+L"\"";
        const auto launched=updated&&CreateProcessW(helper.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,helper.parent_path().c_str(),&startup.StartupInfo,&child);
        DeleteProcThreadAttributeList(attrs);
        for(auto h:handles)SetHandleInformation(h,HANDLE_FLAG_INHERIT,0);
        if(!launched)return false;process=child.hProcess;CloseHandle(child.hThread);
        if(WaitForSingleObject(done,5000)!=WAIT_OBJECT_0)return false;
        ResetEvent(done);crashData=shared;crashSignal=signal;crashDone=done;
        oldFilter=SetUnhandledExceptionFilter(capture);installed=true;return true;
    }
    ~Impl(){
        if(writer.joinable()){{std::lock_guard guard(mutex);stopping=true;}wake.notify_one();writer.join();}
        if(installed){SetUnhandledExceptionFilter(oldFilter);crashData=nullptr;crashSignal=crashDone=nullptr;}
        if(shared){InterlockedExchange(&shared->state,2);SetEvent(signal);}
        if(process)WaitForSingleObject(process,1000);
        if(shared)UnmapViewOfFile(shared);close(process);close(parent);close(mapping);close(signal);close(done);close(lock);
        if(!stem.empty()){std::error_code ec;std::filesystem::remove(stem.wstring()+L".lock",ec);}
    }
};
Session::Impl* Session::active{}; // normal access is on the message/control thread
Session::Session(std::filesystem::path directory,std::filesystem::path helper,std::string version) noexcept {
    try{
        if(active)return;impl=std::make_unique<Impl>();impl->application=version;impl->directory=std::filesystem::absolute(directory);
        std::filesystem::create_directories(impl->directory);
        std::filesystem::file_time_type latest{};
        for(const auto& entry:std::filesystem::directory_iterator(impl->directory))if(entry.is_regular_file()&&entry.path().filename().wstring().starts_with(L"mr-session-")&&entry.path().wstring().ends_with(L".crash.txt"))if(entry.last_write_time()>latest){latest=entry.last_write_time();impl->previous=entry.path();}
        prune(impl->directory);static std::atomic<unsigned> serial{};
        const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        impl->stem=impl->directory/(L"mr-session-"+std::to_wstring(now)+L"-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++serial));
        impl->lock=CreateFileW((impl->stem.wstring()+L".lock").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(impl->lock==INVALID_HANDLE_VALUE)throw std::runtime_error("diagnostic session lock unavailable");
        impl->output.open(impl->stem.wstring()+L".log",std::ios::binary);if(!impl->output)throw std::runtime_error("diagnostic log unavailable");
        impl->output<<"MR Studio diagnostics v1 version="<<clean(version)<<" pid="<<GetCurrentProcessId()<<" utc_epoch_ms="<<now<<"\nLocal only; paths/plugin names may be recorded; dumps contain stack memory.\n";impl->output.flush();
        const bool helperReady=impl->startHelper(helper);
        impl->writer=std::thread([p=impl.get()]{try{p->write();}catch(...){p->journalFailed=true;}});active=impl.get();impl->push(helperReady?"crash_helper_ready":"crash_helper_unavailable; session journal only");
    }catch(...){impl.reset();}
}
Session::~Session(){if(impl&&active==impl.get()){impl->push("clean_shutdown");active=nullptr;}impl.reset();}
bool Session::available() const noexcept{return impl&&impl->writer.joinable();}
bool Session::crash_capture_available() const noexcept{return impl&&impl->installed;}
std::filesystem::path Session::folder() const{return impl?impl->directory:std::filesystem::path{};}
std::filesystem::path Session::base() const{return impl?impl->stem:std::filesystem::path{};}
std::filesystem::path Session::previous_report() const{return impl?impl->previous:std::filesystem::path{};}
void event(std::string_view text) noexcept{if(Session::active)Session::active->push(text);}
std::string status(){return !Session::active?"Diagnostics unavailable":Session::active->journalFailed?"Session journal write failed":Session::active->installed?"Session log and crash reporter active":"Session log active; crash reporter unavailable";}
std::filesystem::path folder(){return Session::active?Session::active->directory:std::filesystem::path{};}
}
