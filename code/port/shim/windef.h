#pragma once
/* Shim <windef.h> (Win32 base types) for the non-Windows experimental OpenTS
 * build. All base types live in windows_stub.h, so this just routes there. */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/windef.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif
#include "windows_stub.h"
