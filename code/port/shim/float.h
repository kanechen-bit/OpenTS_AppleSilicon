#pragma once
/* Shim <float.h> (MSVC floating-point control) for the non-Windows
 * experimental OpenTS build. _controlfp sets FP precision/rounding; the
 * headless build has no need to manipulate the FPU, so it is a no-op. */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/float.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif
#include <cfloat>

#ifndef _MCW_PC
#define _MCW_PC    0x00030000
#define _PC_24     0x00010000
#define _PC_53     0x00020000
#define _PC_64     0x00030000
#define _MCW_RC    0x00000300
#define _RC_NEAR   0x00000000
#endif

inline unsigned int _controlfp(unsigned int /*newVal*/, unsigned int /*mask*/) { return 0; }
