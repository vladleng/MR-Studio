#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <mrs/diagnostics.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
std::string read(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
std::vector<std::filesystem::path> files(const std::filesystem::path& folder,const std::wstring& suffix){std::vector<std::filesystem::path> result;for(const auto& e:std::filesystem::directory_iterator(folder))if(e.path().wstring().ends_with(suffix))result.push_back(e.path());return result;}
DWORD run(const std::filesystem::path& probe,const std::filesystem::path& helper,const std::filesystem::path& directory,const wchar_t* mode){
    std::filesystem::create_directories(directory);auto command=L"\""+probe.wstring()+L"\" \""+directory.wstring()+L"\" \""+helper.wstring()+L"\" "+mode;
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    check(CreateProcessW(probe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=FALSE,"probe spawn");
    const auto wait=WaitForSingleObject(process.hProcess,15000);if(wait!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,99);WaitForSingleObject(process.hProcess,2000);}
    DWORD code{};GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hThread);CloseHandle(process.hProcess);check(wait==WAIT_OBJECT_0,"probe timeout");return code;
}
void waitReport(const std::filesystem::path& folder){for(int i=0;i<100;++i){const auto reports=files(folder,L".crash.txt");if(!reports.empty()&&read(reports.front()).find("Local report only")!=std::string::npos)return;std::this_thread::sleep_for(std::chrono::milliseconds(20));}throw std::runtime_error("report missing");}
void dump(const std::filesystem::path& path){
    const auto bytes=read(path);check(bytes.size()>sizeof(MINIDUMP_HEADER),"dump size");const auto* header=reinterpret_cast<const MINIDUMP_HEADER*>(bytes.data());check(header->Signature==MINIDUMP_SIGNATURE,"dump signature");
    PMINIDUMP_DIRECTORY directory{};void* stream{};ULONG size{};
    check(MiniDumpReadDumpStream(const_cast<char*>(bytes.data()),ExceptionStream,&directory,&stream,&size)!=FALSE,"exception stream");
    const auto* fault=static_cast<const MINIDUMP_EXCEPTION_STREAM*>(stream);check(fault->ExceptionRecord.ExceptionCode==EXCEPTION_ACCESS_VIOLATION&&fault->ThreadId!=0,"exception identity");
    check(fault->ThreadContext.DataSize>=sizeof(CONTEXT),"fault context");
    check(MiniDumpReadDumpStream(const_cast<char*>(bytes.data()),ThreadListStream,&directory,&stream,&size)!=FALSE,"threads stream");
    const auto* threads=static_cast<const MINIDUMP_THREAD_LIST*>(stream);bool faultStack=false;
    for(ULONG i=0;i<threads->NumberOfThreads;++i)if(threads->Threads[i].ThreadId==fault->ThreadId)faultStack=threads->Threads[i].Stack.Memory.DataSize>0;
    check(faultStack,"faulting thread stack present");check(MiniDumpReadDumpStream(const_cast<char*>(bytes.data()),ModuleListStream,&directory,&stream,&size)!=FALSE,"modules stream");
}
}
int wmain(int argc,wchar_t** argv){
    if(argc!=3)return 2;
    const auto root=std::filesystem::temp_directory_path()/(L"mrs-diagnostics-test-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    try{
        std::filesystem::create_directory(root);const std::filesystem::path probe(argv[1]),helper(argv[2]);
        for(const auto* mode:{L"main",L"worker"}){const auto directory=root/mode;check(run(probe,helper,directory,mode)!=0,"fault exits nonzero");waitReport(directory);const auto reports=files(directory,L".crash.txt");const auto report=read(reports.front());check(report.find("unhandled_exception=0xc0000005")!=std::string::npos&&report.find("dump_written=1")!=std::string::npos&&report.find("fault_module=")!=std::string::npos,"fault report fields");const auto dumps=files(directory,L".dmp");check(dumps.size()==1,"one dump");dump(dumps.front());check(read(files(directory,L".log").front()).find("fixture_action")!=std::string::npos,"last action journal");}
        const auto clean=root/L"clean";check(run(probe,helper,clean,L"clean")==0,"clean exit");check(files(clean,L".crash.txt").empty()&&files(clean,L".dmp").empty(),"clean no false crash");check(read(files(clean,L".log").front()).find("clean_shutdown")!=std::string::npos,"clean log flush");
        const auto abrupt=root/L"abrupt";check(run(probe,helper,abrupt,L"abrupt")!=0,"abrupt exit");waitReport(abrupt);check(read(files(abrupt,L".crash.txt").front()).find("unexpected_process_exit=0xc0000409")!=std::string::npos&&files(abrupt,L".dmp").empty(),"abrupt no fake context/dump");
        const auto scoped=root/L"retention";std::filesystem::create_directory(scoped);{std::ofstream unrelated(scoped/L"user.txt");unrelated<<"preserve";}
        const auto live=scoped/L"mr-session-active.log";{std::ofstream file(live);file<<"active";}
        HANDLE liveLock=CreateFileW((scoped/L"mr-session-active.lock").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,0,nullptr);check(liveLock!=INVALID_HANDLE_VALUE,"live retention fixture");
        for(int i=0;i<25;++i){const auto file=scoped/(L"mr-session-old-"+std::to_wstring(i)+L".log");{std::ofstream old(file);old<<"old";}std::filesystem::last_write_time(file,std::filesystem::file_time_type::clock::now()+std::chrono::hours(1));}
        {mrs::diagnostics::Session session(scoped,root/L"missing-helper.exe","fixture");check(session.available()&&!session.crash_capture_available(),"missing helper degrades gracefully");check(files(scoped,L".log").size()<=20,"retention count");mrs::diagnostics::event("line\n\rtest");for(int i=0;i<5000;++i)mrs::diagnostics::event(std::string(3000,'x'));}
        check(read(scoped/L"user.txt")=="preserve","retention preserves unrelated files");for(const auto& log:files(scoped,L".log"))check(std::filesystem::file_size(log)<2*1024*1024+4096,"bounded journal");
        check(read(live)=="active","retention preserves live locked session");CloseHandle(liveLock);
        {mrs::diagnostics::Session blocked(scoped/L"user.txt",helper,"fixture");check(!blocked.available(),"unwritable path is nonfatal");}
        std::filesystem::remove_all(root);std::cout<<"Crash diagnostics PASS: main/worker dumps, exception/stacks/modules, abrupt/clean, retention, bounds, graceful failure\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"; fixture preserved at "<<root<<'\n';return 1;}
}
