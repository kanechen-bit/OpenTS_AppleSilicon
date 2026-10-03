#pragma once
/*
 * Shim <share.h> (MSVC file-sharing flags for _sopen/_fsopen) for the
 * non-Windows experimental OpenTS build. Values mirror the MSVC constants;
 * OpenTS rarely uses them on the POSIX path.
 */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/share.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif

#ifndef _SH_DENYRW
#define _SH_DENYRW   0x10
#define _SH_DENYWR   0x20
#define _SH_DENYRD   0x30
#define _SH_DENYNO   0x40
#define _SH_SECURE   0x80
#endif
