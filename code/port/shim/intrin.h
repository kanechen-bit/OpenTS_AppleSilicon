#pragma once
/*
 * Shim <intrin.h> (MSVC compiler intrinsics) for the non-Windows experimental
 * OpenTS build. Provides the small set of intrinsics the engine uses, mapped to
 * compiler builtins or trivial implementations. _ReadBarrier/_WriteBarrier/
 * _ReadWriteBarrier/__debugbreak are clang builtins on this platform, so they
 * are intentionally NOT redefined here. Expand as needed.
 */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/intrin.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif

#include <stdint.h>

#ifndef __INTRIN_SHIM_H
#define __INTRIN_SHIM_H

#define __noop(...)            ((void)0)
#define __assume(c)            do { if (!(c)) { } } while (0)
#define MemoryBarrier()        __atomic_thread_fence(__ATOMIC_SEQ_CST)

static inline void __nop(void)     { __asm__ volatile("nop"); }
static inline void _mm_pause(void) { __asm__ volatile("yield"); }

#if defined(__aarch64__)
static inline uint64_t __rdtsc(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, CNTVCT_EL0" : "=r"(val));
    return val;
}
#elif defined(__x86_64__)
static inline uint64_t __rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
#else
static inline uint64_t __rdtsc(void) { return 0; }
#endif

static inline long _InterlockedIncrement(long volatile *a) { return __atomic_add_fetch(a, 1, __ATOMIC_SEQ_CST); }
static inline long _InterlockedDecrement(long volatile *a) { return __atomic_add_fetch(a, -1, __ATOMIC_SEQ_CST); }
static inline long _InterlockedExchange(long volatile *a, long v) { long r; __atomic_exchange(a, &v, &r, __ATOMIC_SEQ_CST); return r; }
static inline long _InterlockedCompareExchange(long volatile *a, long v, long c) { __atomic_compare_exchange(a, &c, &v, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return c; }
static inline long _InterlockedExchangeAdd(long volatile *a, long v) { return __atomic_fetch_add(a, v, __ATOMIC_SEQ_CST); }

/* _Interlocked*64 are clang builtins under -fms-extensions, so defining them as
 * plain functions trips "definition of builtin function". Provide a
 * differently-named implementation and alias the MSVC name to it via a macro. */
static inline long long __opents_InterlockedIncrement64(long long volatile *a) { return __atomic_add_fetch(a, 1, __ATOMIC_SEQ_CST); }
static inline long long __opents_InterlockedDecrement64(long long volatile *a) { return __atomic_add_fetch(a, -1, __ATOMIC_SEQ_CST); }
static inline long long __opents_InterlockedExchange64(long long volatile *a, long long v) { long long r; __atomic_exchange(a, &v, &r, __ATOMIC_SEQ_CST); return r; }
static inline long long __opents_InterlockedCompareExchange64(long long volatile *a, long long v, long long c) { __atomic_compare_exchange(a, &c, &v, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return c; }
static inline long long __opents_InterlockedExchangeAdd64(long long volatile *a, long long v) { return __atomic_fetch_add(a, v, __ATOMIC_SEQ_CST); }
#define _InterlockedIncrement64 __opents_InterlockedIncrement64
#define _InterlockedDecrement64 __opents_InterlockedDecrement64
#define _InterlockedExchange64  __opents_InterlockedExchange64
#define _InterlockedCompareExchange64 __opents_InterlockedCompareExchange64
#define _InterlockedExchangeAdd64 __opents_InterlockedExchangeAdd64

/* __cpuid / __cpuidex: query CPUID. On Apple Silicon this is meaningless, so
 * fill with zeros (the engine only reads leaf 1 features on x86). */
static inline void __cpuid(int a[4], int /*level*/) { a[0] = a[1] = a[2] = a[3] = 0; }
static inline void __cpuidex(int a[4], int /*level*/, int /*ecx*/) { a[0] = a[1] = a[2] = a[3] = 0; }

#endif /* __INTRIN_SHIM_H */
