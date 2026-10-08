#pragma once
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
#include <xmmintrin.h>
#endif
namespace mrs {
// Audio-thread-local SSE mode. Tiny DSP tails must not enter slow subnormal math.
// Restore the caller's complete mode when leaving the callback, including errors.
class ScopedNoDenormals {
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
    unsigned previous_;
public:
    ScopedNoDenormals() noexcept:previous_(_mm_getcsr()){_mm_setcsr(previous_|0x8040U);}
    ~ScopedNoDenormals(){_mm_setcsr(previous_);}
#else
public:
    ScopedNoDenormals() noexcept=default;
#endif
    ScopedNoDenormals(const ScopedNoDenormals&)=delete;
    ScopedNoDenormals& operator=(const ScopedNoDenormals&)=delete;
};
}
