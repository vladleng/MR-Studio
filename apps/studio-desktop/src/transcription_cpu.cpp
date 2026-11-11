#include <mrs/audio_to_midi_worker.hpp>
#include <algorithm>
#include <bit>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#if defined(_M_X64) || defined(_M_IX86)
#include <intrin.h>
#endif
#else
#include <sched.h>
#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif
#endif
namespace mrs::transcription {
unsigned available_cpu_threads(){
#ifdef _WIN32
    DWORD_PTR process{},system{};if(GetProcessAffinityMask(GetCurrentProcess(),&process,&system)&&process)return static_cast<unsigned>(std::popcount(process));
#else
    cpu_set_t set;CPU_ZERO(&set);if(sched_getaffinity(0,sizeof(set),&set)==0)return std::max(1U,static_cast<unsigned>(CPU_COUNT(&set)));
#endif
    return std::max(1U,std::thread::hardware_concurrency());
}
unsigned choose_cpu_threads(unsigned requested){const auto n=available_cpu_threads();return requested?std::clamp(requested,1U,std::min(n,32U)):std::clamp(n>1?n-1:1U,1U,16U);}
bool avx2_usable(std::uint32_t a,std::uint32_t b,std::uint64_t xcr0){
    constexpr std::uint32_t required=(1U<<20)|(1U<<27)|(1U<<28)|(1U<<29)|(1U<<12); // SSE4.2, OSXSAVE, AVX, F16C, FMA
    return (a&required)==required&&(b&((1U<<5)|(1U<<8)))==((1U<<5)|(1U<<8))&&(xcr0&6)==6; // AVX2, BMI2, OS XMM/YMM state
}
bool optimized_cpu_available(){
    std::uint32_t a{},b{};std::uint64_t state{};
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    int regs[4]{};__cpuid(regs,0);if(regs[0]<7)return false;__cpuidex(regs,1,0);a=static_cast<std::uint32_t>(regs[2]);if(!(a&(1U<<27)))return false;
    state=_xgetbv(0);__cpuidex(regs,7,0);b=static_cast<std::uint32_t>(regs[1]);
#elif defined(__x86_64__) || defined(__i386__)
    unsigned eax{},ebx{},ecx{},edx{};if(__get_cpuid_max(0,nullptr)<7)return false;__cpuid_count(1,0,eax,ebx,ecx,edx);a=ecx;if(!(a&(1U<<27)))return false;
    unsigned lo{},hi{};__asm__ volatile("xgetbv":"=a"(lo),"=d"(hi):"c"(0));state=(static_cast<std::uint64_t>(hi)<<32)|lo;
    __cpuid_count(7,0,eax,ebx,ecx,edx);b=ebx;
#endif
    return avx2_usable(a,b,state);
}
}
