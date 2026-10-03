#pragma once
/* Shim <winnt.h> for the non-Windows experimental OpenTS build. */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/winnt.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif
#include "windows_stub.h"
