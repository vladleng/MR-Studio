#include <mrs/vst3.hpp>
#include <windows.h>
#include <iostream>
int wmain(int argc,wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    try {
        if(argc!=3)return 2;
        auto path=std::filesystem::path(argv[1]).u8string();
        auto plugins=mrs::processing::probe_vst3(std::string(path.begin(),path.end()));
        mrs::processing::save_vst3_probe(argv[2],plugins);CoUninitialize();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';CoUninitialize();return 1;}
}
