#pragma once
/* Shim <objidl.h> for the non-Windows experimental OpenTS build. */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/objidl.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif
#include "com_stub.h"
