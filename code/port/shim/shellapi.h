#pragma once
/* Shim <shellapi.h> (Win32 Shell API) for the non-Windows experimental OpenTS
 * build. Only the symbols the engine references are provided as hollow stubs. */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/shellapi.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif
#include "windows_stub.h"

typedef struct _SHFILEINFO {
	HICON hIcon;
	int   iIcon;
	DWORD dwAttributes;
	char  szDisplayName[MAX_PATH];
	char  szTypeName[80];
} SHFILEINFO;

#define SHGFI_ICON        0x000000100
#define SHGFI_LARGEICON   0x000000000
#define SHGFI_SMALLICON   0x000000001

inline HINSTANCE ShellExecute(HWND /*hwnd*/, const char * /*lpOperation*/, const char * /*lpFile*/,
                              const char * /*lpParameters*/, const char * /*lpDirectory*/, int /*nShowCmd*/) {
	return NULL_HANDLE;
}
