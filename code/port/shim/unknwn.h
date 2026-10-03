#pragma once
/* Shim <unknwn.h> for the non-Windows experimental OpenTS build. */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/unknwn.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif
#include "com_stub.h"
