#pragma once
/*
 * Shim <windows.h> for the non-Windows experimental OpenTS build.
 * Routes every <windows.h> include to code/port/windows_stub.h, which in turn
 * pulls in code/port/com_stub.h. The real Win32 path never reaches this file.
 * Included only under OPENTS_EXPERIMENTAL_NONWIN32.
 */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/windows.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif
#include "windows_stub.h"
