#pragma once
/* Shim <basetyps.h> (MSVC base types / interface macros) for the non-Windows
 * experimental OpenTS build. The relevant macros (interface, STDMETHOD, GUID)
 * are supplied by com_stub.h. */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/basetyps.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif
#include "com_stub.h"
