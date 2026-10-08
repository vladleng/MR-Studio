#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
namespace {
struct Region {ULONG64 address;ULONG size;ULONG rva;};std::vector<Region> regions;std::vector<char> bytes;
BOOL WINAPI memory(HANDLE,DWORD64 address,PVOID output,DWORD size,LPDWORD read){
    for(const auto& r:regions)if(address>=r.address&&address-r.address<=r.size&&size<=r.size-(address-r.address)){
        const auto offset=r.rva+address-r.address;if(offset+size>bytes.size())return FALSE;memcpy(output,bytes.data()+offset,size);*read=size;return TRUE;}
    return FALSE;
}
void* stream(ULONG kind){PMINIDUMP_DIRECTORY dir{};void* data{};ULONG size{};return MiniDumpReadDumpStream(bytes.data(),kind,&dir,&data,&size)?data:nullptr;}
void symbol(HANDLE process,DWORD64 address){
    alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};auto* info=reinterpret_cast<SYMBOL_INFO*>(storage);info->SizeOfStruct=sizeof(SYMBOL_INFO);info->MaxNameLen=MAX_SYM_NAME;DWORD64 displacement{};
    std::cout<<"0x"<<std::hex<<address;
    if(SymFromAddr(process,address,&displacement,info))std::cout<<" "<<info->Name<<" +0x"<<displacement;
    IMAGEHLP_LINE64 line{};line.SizeOfStruct=sizeof(line);DWORD delta{};if(SymGetLineFromAddr64(process,address,&delta,&line))std::cout<<" "<<line.FileName<<":"<<std::dec<<line.LineNumber;
    std::cout<<'\n';
}
}
int wmain(int argc,wchar_t** argv){
    if(argc!=3){std::cerr<<"mrs_dump_inspect DUMP LOCAL_SYMBOL_DIRECTORY\n";return 2;}
    std::ifstream input(std::filesystem::path(argv[1]),std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(input),{});
    if(bytes.size()<sizeof(MINIDUMP_HEADER)||reinterpret_cast<const MINIDUMP_HEADER*>(bytes.data())->Signature!=MINIDUMP_SIGNATURE)return 3;
    auto* fault=static_cast<const MINIDUMP_EXCEPTION_STREAM*>(stream(ExceptionStream));auto* modules=static_cast<const MINIDUMP_MODULE_LIST*>(stream(ModuleListStream));auto* threads=static_cast<const MINIDUMP_THREAD_LIST*>(stream(ThreadListStream));if(!fault||!modules||!threads)return 4;
    HANDLE process=GetCurrentProcess();SymSetOptions(SYMOPT_LOAD_LINES|SYMOPT_FAIL_CRITICAL_ERRORS|SYMOPT_NO_PROMPTS|SYMOPT_EXACT_SYMBOLS);
    if(!SymInitializeW(process,argv[2],FALSE))return 5;
    for(ULONG i=0;i<modules->NumberOfModules;++i){const auto& module=modules->Modules[i];const auto* name=reinterpret_cast<const MINIDUMP_STRING*>(bytes.data()+module.ModuleNameRva);const std::wstring path(name->Buffer,name->Length/sizeof(wchar_t));
        if(std::filesystem::is_regular_file(path))SymLoadModuleExW(process,nullptr,path.c_str(),nullptr,module.BaseOfImage,module.SizeOfImage,nullptr,0);}
    for(ULONG i=0;i<threads->NumberOfThreads;++i){const auto& stack=threads->Threads[i].Stack;regions.push_back({stack.StartOfMemoryRange,stack.Memory.DataSize,stack.Memory.Rva});}
    if(auto* list=static_cast<const MINIDUMP_MEMORY_LIST*>(stream(MemoryListStream)))for(ULONG i=0;i<list->NumberOfMemoryRanges;++i){const auto& r=list->MemoryRanges[i];regions.push_back({r.StartOfMemoryRange,r.Memory.DataSize,r.Memory.Rva});}
    std::cout<<"exception=0x"<<std::hex<<fault->ExceptionRecord.ExceptionCode<<" thread="<<std::dec<<fault->ThreadId<<'\n';symbol(process,fault->ExceptionRecord.ExceptionAddress);
    CONTEXT context{};if(fault->ThreadContext.Rva+sizeof(context)>bytes.size())return 6;memcpy(&context,bytes.data()+fault->ThreadContext.Rva,sizeof(context));
    std::cout<<"access="<<fault->ExceptionRecord.ExceptionInformation[0]<<" address=0x"<<std::hex<<fault->ExceptionRecord.ExceptionInformation[1]<<" rcx="<<context.Rcx<<" rdx="<<context.Rdx<<" rax="<<context.Rax<<" rsp="<<context.Rsp<<" rdi="<<context.Rdi<<" r15="<<context.R15<<'\n';
    for(unsigned n=0;n<256;++n){DWORD64 address{};DWORD read{};if(!memory(process,context.Rsp+n*sizeof(address),&address,sizeof(address),&read))break;
        for(ULONG i=0;i<modules->NumberOfModules;++i){const auto& m=modules->Modules[i];if(address>=m.BaseOfImage&&address-m.BaseOfImage<m.SizeOfImage){std::cout<<"stack+0x"<<std::hex<<n*sizeof(address)<<" ";symbol(process,address);break;}}}
    STACKFRAME64 frame{};frame.AddrPC={context.Rip,AddrModeFlat};frame.AddrStack={context.Rsp,AddrModeFlat};frame.AddrFrame={context.Rbp,AddrModeFlat};
    for(int n=0;n<48&&frame.AddrPC.Offset;++n){symbol(process,frame.AddrPC.Offset);if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,nullptr,&frame,&context,memory,SymFunctionTableAccess64,SymGetModuleBase64,nullptr))break;}
    SymCleanup(process);return 0;
}
