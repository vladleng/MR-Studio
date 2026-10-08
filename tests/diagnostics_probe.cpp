#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mrs/diagnostics.hpp>
#include <chrono>
#include <thread>
#include <string>
__declspec(noinline) void nativeFault(){RaiseException(EXCEPTION_ACCESS_VIOLATION,EXCEPTION_NONCONTINUABLE,0,nullptr);}
int wmain(int argc,wchar_t** argv){
    if(argc!=4)return 2;SetErrorMode(SEM_NOGPFAULTERRORBOX|SEM_FAILCRITICALERRORS);
    mrs::diagnostics::Session session(argv[1],argv[2],"diagnostics-fixture");
    if(!session.available()||!session.crash_capture_available())return 3;
    mrs::diagnostics::event("fixture_action midi_note_commit_end");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const std::wstring mode(argv[3]);
    if(mode==L"main")nativeFault();
    if(mode==L"worker"){std::thread thread(nativeFault);thread.join();}
    if(mode==L"abrupt")TerminateProcess(GetCurrentProcess(),0xc0000409);
    if(mode!=L"clean"&&mode!=L"main"&&mode!=L"worker"&&mode!=L"abrupt")return 4;
    return 0;
}
