#pragma once
/* Shim <mmsystem.h> (Win32 multimedia) for the non-Windows experimental
 * OpenTS build. All symbols route to the combined windows_stub.h. */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/mmsystem.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif
#include "windows_stub.h"

#include <time.h>
#include <sys/time.h>

typedef DWORD MMRESULT;
#define TIMERR_NOERROR       0
#define TIMERR_NOCANDO       97
#define TIME_PERIODIC        1
#define TIME_ONESHOT         0
#define TIME_CALLBACK_FUNCTION 0x0000

inline DWORD timeGetTime(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (DWORD)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}
inline MMRESULT timeBeginPeriod(UINT /*uPeriod*/) { return TIMERR_NOERROR; }
inline MMRESULT timeEndPeriod(UINT /*uPeriod*/)   { return TIMERR_NOERROR; }
