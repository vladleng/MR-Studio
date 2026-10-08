#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
namespace mrs::diagnostics {
inline constexpr DWORD protocol_version=1;
struct Shared {
    DWORD version{protocol_version};
    char application[64]{};
    volatile LONG state{}; // 0 watching, 1 unhandled exception, 2 orderly shutdown
    DWORD thread{};
    EXCEPTION_RECORD exception{};
    CONTEXT context{};
};
}
