#pragma once
/*
 * Shim <new.h> (MSVC) for the non-Windows experimental OpenTS build.
 * The standard <new> header already declares the nothrow operator new/delete
 * overloads that MSVC's <new.h> provides, so route there.
 */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/new.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif
#include <new>
