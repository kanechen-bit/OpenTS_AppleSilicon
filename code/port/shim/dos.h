#pragma once
/* Shim <dos.h> (MSVC legacy/DOS CRT) for the non-Windows experimental OpenTS
 * build. The engine's conquer.cpp / session.cpp include it for a few legacy
 * symbols; most are not needed headlessly. Route to the common stubs. */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/dos.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif
#include "windows_stub.h"

/* The few dos.h symbols the engine may reference. Add real shims as needed. */
#ifndef _dos_getdrive
inline unsigned _dos_getdrive(unsigned *drive) { (void)drive; return 0; }
#endif
#ifndef _dos_setdrive
inline void _dos_setdrive(unsigned /*drive*/, unsigned * /*dummy*/) {}
#endif
