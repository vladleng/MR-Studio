#include "protocol.hpp"
#include <dbghelp.h>
#include <tlhelp32.h>
#include <filesystem>
#include <fstream>
#include <string>
namespace {
std::string utf8(const wchar_t* value){const int size=WideCharToMultiByte(CP_UTF8,0,value,-1,nullptr,0,nullptr,nullptr);if(size<1)return {};std::string result(static_cast<std::size_t>(size),'\0');WideCharToMultiByte(CP_UTF8,0,value,-1,result.data(),size,nullptr,nullptr);result.pop_back();return result;}
HANDLE handle(const wchar_t* value){wchar_t* end{};const auto n=wcstoull(value,&end,10);return end&&!*end?reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(n)):nullptr;}
}
int wmain(int argc,wchar_t** argv){
    if(argc!=7||std::wstring(argv[1])!=L"--watch")return 2;
    HANDLE mapping=handle(argv[2]),signal=handle(argv[3]),done=handle(argv[4]),parent=handle(argv[5]);
    auto* shared=static_cast<mrs::diagnostics::Shared*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(mrs::diagnostics::Shared)));
    if(!shared||shared->version!=mrs::diagnostics::protocol_version)return 3;
    const DWORD pid=GetProcessId(parent);if(!pid)return 4;
    const std::filesystem::path base(argv[6]);SetEvent(done);
    HANDLE waits[]{signal,parent};const auto reason=WaitForMultipleObjects(2,waits,FALSE,INFINITE);
    if(InterlockedCompareExchange(&shared->state,0,0)==2)return 0;
    try{
        std::ofstream report(base.wstring()+L".crash.txt",std::ios::binary);
        FILETIME time{};GetSystemTimeAsFileTime(&time);ULARGE_INTEGER timestamp{};timestamp.LowPart=time.dwLowDateTime;timestamp.HighPart=time.dwHighDateTime;
        report<<"MR Studio crash report v1\napplication="<<shared->application<<"\npid="<<pid<<"\nutc_filetime="<<timestamp.QuadPart<<"\n";
        const bool fault=reason==WAIT_OBJECT_0&&InterlockedCompareExchange(&shared->state,0,0)==1;
        if(fault){
            report<<"unhandled_exception=0x"<<std::hex<<shared->exception.ExceptionCode<<"\nthread="<<std::dec<<shared->thread<<"\nfault_address=0x"<<std::hex<<reinterpret_cast<std::uintptr_t>(shared->exception.ExceptionAddress)<<"\n";
            const auto modules=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
            if(modules!=INVALID_HANDLE_VALUE){MODULEENTRY32W module{};module.dwSize=sizeof(module);if(Module32FirstW(modules,&module))do{
                const auto begin=reinterpret_cast<std::uintptr_t>(module.modBaseAddr),address=reinterpret_cast<std::uintptr_t>(shared->exception.ExceptionAddress);
                report<<"module base=0x"<<std::hex<<begin<<" size=0x"<<module.modBaseSize<<" path="<<utf8(module.szExePath)<<"\n";
                if(address>=begin&&address-begin<module.modBaseSize)report<<"fault_module="<<utf8(module.szModule)<<" offset=0x"<<address-begin<<"\n";
            }while(Module32NextW(modules,&module));CloseHandle(modules);}
            report.flush();
            HANDLE file=CreateFileW((base.wstring()+L".dmp").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            bool ok=false;DWORD error=GetLastError();
            if(file!=INVALID_HANDLE_VALUE){EXCEPTION_POINTERS pointers{&shared->exception,&shared->context};MINIDUMP_EXCEPTION_INFORMATION info{shared->thread,&pointers,FALSE};
                ok=MiniDumpWriteDump(parent,pid,file,static_cast<MINIDUMP_TYPE>(MiniDumpNormal|MiniDumpWithThreadInfo|MiniDumpWithUnloadedModules),&info,nullptr,nullptr)!=FALSE;error=ok?0:GetLastError();FlushFileBuffers(file);CloseHandle(file);}
            report<<"dump_written="<<ok<<" error=0x"<<std::hex<<error<<"\n";
        }else{
            DWORD code{};GetExitCodeProcess(parent,&code);report<<"unexpected_process_exit=0x"<<std::hex<<code<<"\nNo exception context; fail-fast/forced termination or replaced handler possible.\nNo dump: process has already exited.\n";
        }
        report<<"Local report only. Module names/offsets identify a fault location, not its root cause.\n";report.flush();
    }catch(...){SetEvent(done);return 5;}
    SetEvent(done);UnmapViewOfFile(shared);CloseHandle(mapping);CloseHandle(signal);CloseHandle(done);CloseHandle(parent);return 0;
}
