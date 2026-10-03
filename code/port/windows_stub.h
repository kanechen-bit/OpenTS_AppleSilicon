/*
 * windows_stub.h -- minimal Win32 replacement for non-Windows OpenTS builds.
 *
 * OpenTS was written against <windows.h>. On Apple Silicon / macOS that header
 * does not exist. This stub supplies the symbols the engine actually uses:
 *   - opaque handle / struct TYPES (HWND, HDC, RECT, POINT, MSG, ...) so every
 *     translation unit parses;
 *   - CORE (non-GUI) APIs implemented as thin POSIX wrappers (_makepath,
 *     wsprintf, file I/O, timing, text conversion) so the simulation core can
 *     build and run headlessly;
 *   - GUI APIs (SendMessage, InvalidateRect, BeginPaint, CreateWindowEx, ...)
 *     as HOLLOW no-op stubs. They let dialog/UI translation units parse, but a
 *     real UI backend (SDL2 / Qt / Cocoa) is required to make them function.
 *
 * Included only under OPENTS_EXPERIMENTAL_NONWIN32 (the <windows.h> shim routes
 * here). The Win32 build is byte-for-byte unchanged.
 *
 * This is a FOUNDATION: constants and GUI APIs are filled in incrementally as
 * real translation units are compiled. See code/port/WIN32_API_INVENTORY.md.
 */

#ifndef OPENTS_PORT_WINDOWS_STUB_H
#define OPENTS_PORT_WINDOWS_STUB_H

#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "windows_stub.h must only be included on the non-Windows experimental build"
#endif

#include "com_stub.h"   /* pulls in GUID/HRESULT/DWORD/BOOL/LONG/ULONG/LPVOID + COM interfaces */

#include "port_bridge.h" /* real window/event backend bridge (Cocoa window + message queue) */
#include "port_gdi.h"    /* minimal in-process GDI (device contexts and bitmaps) */
#include "port_input.h"  /* shared keyboard/mouse state, written by the event pump */
#include "port_resource.h" /* Win32 resource modules (Language.dll dialogs and strings) */
#include "port_windows.h"  /* logical window manager (dialogs and their controls) */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <thread>
#include <chrono>
#include <dirent.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <cwchar>
#include <cstddef>

/* ---- Calling-convention / handle macros --------------------------------- */
#define WINAPI
#define CALLBACK
#define APIENTRY
#define CDECL
#define DECLARE_HANDLE(n) typedef void *n
#ifndef NULL
#define NULL nullptr
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

/* ---- Scalar typedefs not already in com_stub.h -------------------------- */
typedef unsigned int  UINT;
typedef intptr_t      INT_PTR;
typedef unsigned int  UINT_PTR;
typedef intptr_t      LONG_PTR;
typedef uintptr_t     DWORD_PTR;
typedef uintptr_t     ULONG_PTR;
typedef uintptr_t     WPARAM;     /* UINT_PTR on Win64 */
typedef intptr_t      LPARAM;     /* LONG_PTR on Win64 */
typedef intptr_t      LRESULT;    /* LONG_PTR on Win64 */
typedef char          TCHAR;
typedef char         *LPSTR;
typedef const char   *LPCSTR;
typedef wchar_t      *LPWSTR;
typedef const wchar_t *LPCWSTR;
typedef TCHAR        *LPTSTR;
typedef const TCHAR  *LPCTSTR;
typedef unsigned int  COLORREF;   /* 0x00BBGGRR */

#define RGB(r,g,b)          ((COLORREF)(((uint8_t)(r))|(((uint8_t)(g))<<8)|(((uint8_t)(b))<<16)))
#define MAKEWORD(l,h)       ((WORD)(((uint8_t)(l))|((uint16_t)((uint8_t)(h))<<8)))
#define MAKELONG(l,h)       ((LONG)(((WORD)(l))|((DWORD)((WORD)(h))<<16)))
#define MAKELPARAM(l,h)     ((LPARAM)MAKELONG(l,h))
#define LOWORD(l)           ((WORD)((l)&0xFFFF))
#define HIWORD(l)           ((WORD)(((l)>>16)&0xFFFF))
#define LOBYTE(w)           ((BYTE)((w)&0xFF))
#define HIBYTE(w)           ((BYTE)(((w)>>8)&0xFF))
#define GET_X_LPARAM(l)     ((int)(short)LOWORD(l))
#define GET_Y_LPARAM(l)     ((int)(short)HIWORD(l))

/* ---- Opaque handle types (void* in the stub) ---------------------------- */
typedef void *HANDLE;
typedef void *HWND;
typedef void *HINSTANCE;
typedef void *HDC;
typedef void *HMENU;
typedef void *HFONT;
typedef void *HBITMAP;
typedef void *HCURSOR;
typedef void *HICON;
typedef void *HMODULE;
typedef void *HKEY;
typedef void *HGLOBAL;
typedef void *HBRUSH;
typedef void *HACCEL;
typedef void *HTREEITEM;
typedef void *HIMAGELIST;
typedef void *HMETAFILE;
typedef void *HGDIOBJ;
typedef HINSTANCE HMODULE_;

#define INVALID_HANDLE_VALUE  ((HANDLE)(-1))
#define NULL_HANDLE           ((HANDLE)0)

/* ---- Common structs ----------------------------------------------------- */
typedef struct RECT     { LONG left; LONG top; LONG right; LONG bottom; } RECT;
typedef RECT           *LPRECT;
typedef struct POINT    { LONG x; LONG y; } POINT;
typedef struct SIZE     { LONG cx; LONG cy; } SIZE;
typedef struct MSG      { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG;
typedef struct PAINTSTRUCT { HDC hdc; BOOL fErase; RECT rcPaint; BOOL fRestore; BOOL fIncUpdate; BYTE rgbReserved[32]; } PAINTSTRUCT;
typedef struct WNDCLASS {
	UINT    style;
	LRESULT (*lpfnWndProc)(HWND, UINT, WPARAM, LPARAM);
	int     cbClsExtra;
	int     cbWndExtra;
	HINSTANCE hInstance;
	HICON   hIcon;
	HCURSOR hCursor;
	HBRUSH  hbrBackground;
	LPCSTR  lpszMenuName;
	LPCSTR  lpszClassName;
} WNDCLASS;
typedef struct LOGFONT {
	LONG   lfHeight;
	LONG   lfWidth;
	LONG   lfEscapement;
	LONG   lfOrientation;
	LONG   lfWeight;
	BYTE   lfItalic;
	BYTE   lfUnderline;
	BYTE   lfStrikeOut;
	BYTE   lfCharSet;
	BYTE   lfOutPrecision;
	BYTE   lfClipPrecision;
	BYTE   lfQuality;
	BYTE   lfPitchAndFamily;
	char   lfFaceName[32];
} LOGFONT;

/* ---- Minimal constant set (expand as real TUs are compiled) ------------- */
#define SW_HIDE            0
#define SW_SHOWNORMAL      1
#define SW_SHOW            5
#define SW_RESTORE         9
#define WM_NULL            0x0000
#define WM_CREATE          0x0001
#define WM_DESTROY         0x0002
#define WM_PAINT           0x000F
#define WM_COMMAND         0x0111
#define WM_TIMER           0x0113
#define WM_CLOSE           0x0010
#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define WS_VISIBLE         0x10000000
#define WS_CHILD           0x40000000
#define CS_VREDRAW         0x0001
#define CS_HREDRAW         0x0002
#define GWL_WNDPROC        (-4)
#define GWL_USERDATA       (-21)
#define CW_USEDEFAULT      ((int)0x80000000)
#define MB_OK              0x0000
#define MB_ICONERROR       0x0010
#define ERROR_SUCCESS      0
#define GENERIC_READ       0x80000000
#define GENERIC_WRITE      0x40000000
/* CreateFile dispositions, with the values winbase.h gives them. CREATE_NEW and
 * OPEN_ALWAYS were missing, so they fell through to the plain O_RDWR branch and
 * never created anything. */
#define CREATE_NEW         1
#define OPEN_EXISTING      3
#define CREATE_ALWAYS      2
#define OPEN_ALWAYS        4
#define TRUNCATE_EXISTING  5
#define FILE_SHARE_READ    0x00000001
#define FILE_ATTRIBUTE_NORMAL 0x80

/* ---- Core (non-GUI) APIs -- implemented via POSIX ----------------------- */

/* These CRT helpers are force-included (and guarded) from port_string_shim.h
 * so non-<windows.h> TUs get them too. Guard here so the duplicate definition
 * is a no-op when that file has already provided them. */
#ifndef OPENTS_PORT_CRT_HELPERS
#define OPENTS_PORT_CRT_HELPERS
inline void _makepath(char *path, const char *drive, const char *dir, const char *fname, const char *ext) {
	if (!path) return;
	*path = '\0';
	if (drive && *drive) { strcpy(path, drive); strcat(path, ":"); }
	if (dir && *dir)    { strcat(path, dir); }
	if (fname)          { strcat(path, fname); }
	if (ext && *ext) {
		if (*ext != '.') strcat(path, ".");
		strcat(path, ext);
	}
}

/* Splits a path the way _splitpath does, but with the directory filled in.
 *
 * dir has to come back with its trailing separator, because callers concatenate
 * it straight onto a file name: dbgprint.cpp builds its log directory as
 * "%s%sDebug" and would otherwise produce "Debug" relative to the working
 * directory rather than beside the executable. */
inline void _splitpath(const char *path, char *drive, char *dir, char *fname, char *ext) {
	if (drive) *drive = '\0';
	if (dir)   *dir = '\0';
	if (ext)   *ext = '\0';
	if (fname) *fname = '\0';
	if (!path) return;

	/* Either separator, so a path the shim produced and one the engine built
	 * with backslashes both split. */
	const char *slash = strrchr(path, '/');
	const char *backslash = strrchr(path, '\\');
	if (backslash && (!slash || backslash > slash)) slash = backslash;

	const char *base = slash ? slash + 1 : path;

	if (dir && slash) {
		size_t n = (size_t)(slash - path) + 1;   /* keep the trailing separator */
		if (n > 250) n = 250;
		memcpy(dir, path, n);
		dir[n] = '\0';
	}

	if (fname) strcpy(fname, base);
	if (base) {
		const char *dot = strrchr(base, '.');
		if (dot && dot != base) {
			if (fname) { size_t n = (size_t)(dot - base); strncpy(fname, base, n); fname[n] = '\0'; }
			if (ext) strcpy(ext, dot);
		}
	}
}

inline int wsprintf(char *buf, const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	int n = vsnprintf(buf, 4096, fmt, ap);
	va_end(ap);
	return n;
}
#endif /* OPENTS_PORT_CRT_HELPERS */

inline int _snprintf(char *buf, size_t count, const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	int n = vsnprintf(buf, count, fmt, ap);
	va_end(ap);
	return n;
}

inline DWORD GetTickCount(void) {
	static const auto start = std::chrono::steady_clock::now();
	auto now = std::chrono::steady_clock::now();
	return (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

inline void Sleep(DWORD ms) {
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline DWORD GetLastError(void) { return (DWORD)errno; }

inline HANDLE CreateFileA(const char *name, DWORD access, DWORD share, void *sa, DWORD disp, DWORD flags, HANDLE tmpl) {
	(void)share; (void)sa; (void)flags; (void)tmpl;
	if (name == nullptr || *name == '\0') return INVALID_HANDLE_VALUE;

	/* The engine builds nested paths with backslashes, which POSIX treats as an
	 * ordinary filename character: "Debug\DEBUG_x.LOG" would be one file with a
	 * backslash in its name rather than a file inside Debug. */
	std::string path(name);
	for (char &character : path) {
		if (character == '\\') character = '/';
	}

	int oflags = O_RDONLY;
	if ((access & GENERIC_WRITE) != 0) {
		oflags = (access & GENERIC_READ) != 0 ? O_RDWR : O_WRONLY;
	}

	/* CREATE_NEW has to exclude, or the second log of the same second would
	 * silently reopen the first one's file. */
	if (disp == CREATE_NEW)          oflags |= O_CREAT | O_EXCL;
	else if (disp == CREATE_ALWAYS)  oflags |= O_CREAT | O_TRUNC;
	else if (disp == OPEN_ALWAYS)    oflags |= O_CREAT;

	int fd = ::open(path.c_str(), oflags, 0666);
	if (fd < 0) return INVALID_HANDLE_VALUE;
	return (HANDLE)(intptr_t)fd;
}
inline HANDLE CreateFile(const char *name, DWORD access, DWORD share, void *sa, DWORD disp, DWORD flags, HANDLE tmpl) {
	return CreateFileA(name, access, share, sa, disp, flags, tmpl);
}

inline BOOL ReadFile(HANDLE h, void *buf, DWORD n, DWORD *read, void *ovl) {
	(void)ovl;
	int fd = (int)(intptr_t)h;
	ssize_t r = ::read(fd, buf, n);
	if (r < 0) { if (read) *read = 0; return FALSE; }
	if (read) *read = (DWORD)r;
	return TRUE;
}
inline BOOL WriteFile(HANDLE h, const void *buf, DWORD n, DWORD *written, void *ovl) {
	(void)ovl;
	int fd = (int)(intptr_t)h;
	ssize_t w = ::write(fd, buf, n);
	if (w < 0) { if (written) *written = 0; return FALSE; }
	if (written) *written = (DWORD)w;
	return TRUE;
}
inline BOOL CloseHandle(HANDLE h) {
	if (h == INVALID_HANDLE_VALUE || h == NULL_HANDLE) return FALSE;
	int fd = (int)(intptr_t)h;
	return ::close(fd) == 0 ? TRUE : FALSE;
}
inline DWORD GetFileSize(HANDLE h, DWORD *high) {
	(void)high;
	int fd = (int)(intptr_t)h;
	struct stat st;
	if (::fstat(fd, &st) != 0) return (DWORD)-1;
	return (DWORD)(st.st_size & 0xFFFFFFFFu);
}
inline DWORD SetFilePointer(HANDLE h, LONG dist, LONG *high, DWORD method) {
	(void)high;
	int fd = (int)(intptr_t)h;
	int whence = (method == 0) ? SEEK_SET : (method == 1) ? SEEK_CUR : SEEK_END;
	off_t r = ::lseek(fd, (off_t)dist, whence);
	if (r < 0) return (DWORD)-1;
	return (DWORD)((uint64_t)r & 0xFFFFFFFFu);
}
inline BOOL DeleteFileA(const char *p) { return ::unlink(p) == 0 ? TRUE : FALSE; }
inline BOOL DeleteFile(const char *p) { return DeleteFileA(p); }

/* ---- File information & time (POSIX-backed, functional) ------------------ */
/* NOTE: FILETIME is already defined in com_stub.h; reuse it here. */

typedef struct _BY_HANDLE_FILE_INFORMATION {
	DWORD    dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD    dwVolumeSerialNumber;
	DWORD    nFileSizeHigh;
	DWORD    nFileSizeLow;
	DWORD    nNumberOfLinks;
	DWORD    nFileIndexHigh;
	DWORD    nFileIndexLow;
} BY_HANDLE_FILE_INFORMATION, *PBY_HANDLE_FILE_INFORMATION, *LPBY_HANDLE_FILE_INFORMATION;

#ifndef FILE_SHARE_WRITE
#define FILE_SHARE_READ          0x00000001
#define FILE_SHARE_WRITE         0x00000002
#define FILE_SHARE_DELETE        0x00000004
#define FILE_FLAG_SEQUENTIAL_SCAN   0x08000000
#define FILE_FLAG_WRITE_THROUGH     0x80000000
#define FILE_FLAG_RANDOM_ACCESS     0x10000000
#define FILE_BEGIN    0
#define FILE_CURRENT  1
#define FILE_END      2
#define OPEN_ALWAYS        4
#define TRUNCATE_EXISTING  5
#define CREATE_NEW         1
#endif

/* Windows epoch (1601-01-01) is 11644473600 seconds before the Unix epoch. */
inline FILETIME PosixTimeToFileTime(time_t t) {
	uint64_t ft = ((uint64_t)t + 11644473600ULL) * 10000000ULL;
	FILETIME f;
	f.dwLowDateTime = (DWORD)(ft & 0xFFFFFFFFULL);
	f.dwHighDateTime = (DWORD)(ft >> 32);
	return f;
}
inline BOOL GetFileInformationByHandle(HANDLE h, BY_HANDLE_FILE_INFORMATION *info) {
	if (!info) return FALSE;
	int fd = (int)(intptr_t)h;
	struct stat st;
	if (::fstat(fd, &st) != 0) return FALSE;
	memset(info, 0, sizeof(*info));
	info->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
	info->ftLastWriteTime = PosixTimeToFileTime(st.st_mtime);
	info->ftLastAccessTime = PosixTimeToFileTime(st.st_atime);
	info->ftCreationTime = info->ftLastWriteTime;
	info->nFileSizeLow = (DWORD)((uint64_t)st.st_size & 0xFFFFFFFFULL);
	info->nFileSizeHigh = (DWORD)((uint64_t)st.st_size >> 32);
	return TRUE;
}
inline BOOL DosDateTimeToFileTime(WORD dosdate, WORD dostime, FILETIME *ft) {
	if (!ft) return FALSE;
	struct tm t;
	memset(&t, 0, sizeof(t));
	t.tm_year = (int)((dosdate >> 9) & 0x7F) + 80;
	t.tm_mon  = (int)((dosdate >> 5) & 0x0F) - 1;
	t.tm_mday = (int)(dosdate & 0x1F);
	t.tm_hour = (int)((dostime >> 11) & 0x1F);
	t.tm_min  = (int)((dostime >> 5) & 0x3F);
	t.tm_sec  = (int)((dostime & 0x1F) * 2);
	time_t tt = ::mktime(&t);
	if (tt == (time_t)-1) return FALSE;
	*ft = PosixTimeToFileTime(tt);
	return TRUE;
}
inline BOOL FileTimeToDosDateTime(const FILETIME *ft, WORD *dosdate, WORD *dostime) {
	if (!ft || !dosdate || !dostime) return FALSE;
	uint64_t v = ((uint64_t)ft->dwHighDateTime << 32) | (uint64_t)ft->dwLowDateTime;
	time_t tt = (time_t)(v / 10000000ULL) - 11644473600ULL;
	struct tm t;
	if (::gmtime_r(&tt, &t) == NULL) return FALSE;
	*dosdate = (WORD)(((t.tm_year - 80) << 9) | ((t.tm_mon + 1) << 5) | t.tm_mday);
	*dostime = (WORD)(((t.tm_hour) << 11) | ((t.tm_min) << 5) | (t.tm_sec / 2));
	return TRUE;
}
inline BOOL SetFileTime(HANDLE h, const FILETIME *lpCreationTime, const FILETIME *lpLastAccessTime, const FILETIME *lpLastWriteTime) {
	(void)lpCreationTime;
	int fd = (int)(intptr_t)h;
	struct timespec ts[2];
	auto ft2ts = [](const FILETIME *f, struct timespec *out) {
		if (f) {
			uint64_t v = ((uint64_t)f->dwHighDateTime << 32) | (uint64_t)f->dwLowDateTime;
			out->tv_sec = (time_t)(v / 10000000ULL) - 11644473600ULL;
			out->tv_nsec = (long)((v % 10000000ULL) * 100ULL);
		} else { out->tv_sec = 0; out->tv_nsec = 0; }
	};
	ft2ts(lpLastAccessTime, &ts[0]);
	ft2ts(lpLastWriteTime, &ts[1]);
	return (::futimens(fd, ts) == 0) ? TRUE : FALSE;
}

/*
 *	Directory enumeration.
 *
 *	Two details of the Win32 search are load-bearing and were both missing from
 *	the first cut here: it matches the wildcard, and it fills in attributes.
 *	The engine asks for patterns such as "MAPS*.MIX" and then discards any result
 *	carrying FILE_ATTRIBUTE_DIRECTORY or FILE_ATTRIBUTE_HIDDEN. A search that
 *	ignores the pattern and reports no attributes hands it every file and every
 *	directory, and the caller -- which trusts its input -- tries to mount each one
 *	as a mixfile.
 */
struct WIN32_FIND_DATAA {
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
	DWORD dwReserved0;
	DWORD dwReserved1;
	char cFileName[260];
	char cAlternateFileName[14];
};
typedef WIN32_FIND_DATAA WIN32_FIND_DATA;

/* The attribute bits are read below, but the constant block that groups them sits
 * further down the file. The guard makes that later definition a no-op. */
#ifndef FILE_ATTRIBUTE_DIRECTORY
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#endif

/* Matches one name against a DOS wildcard: '*' is any run of characters including
 * none, '?' is exactly one character, and the comparison is case-insensitive. */
inline bool Opents_Wildcard_Match(char const * pattern, char const * name) {
	if (*pattern == '\0') {
		return (*name == '\0');
	}

	if (*pattern == '*') {
		char const * rest = pattern;
		while (*rest == '*') rest++;			/* collapse runs */

		if (*rest == '\0') {
			return (true);
		}

		/* Try every split point, including the empty tail. */
		for (char const * probe = name; ; probe++) {
			if (Opents_Wildcard_Match(rest, probe)) {
				return (true);
			}
			if (*probe == '\0') {
				break;
			}
		}

		/* Windows quirk: a trailing ".*" also matches a name with no extension,
		 * which is why "*.*" matches everything. */
		return (rest[0] == '.' && rest[1] == '*' && rest[2] == '\0');
	}

	if (*name == '\0') {
		/* The same quirk, reached when the stars consumed the whole name. */
		return (pattern[0] == '.' && pattern[1] == '*' && pattern[2] == '\0');
	}

	if (*pattern != '?' && ::toupper((unsigned char)*pattern) != ::toupper((unsigned char)*name)) {
		return (false);
	}

	return (Opents_Wildcard_Match(pattern + 1, name + 1));
}

/* The directory and leaf pattern a search handle was opened with, so that
 * FindNextFile can go on filtering without the caller repeating itself. */
struct Opents_Find_Search {
	std::string directory;
	std::string pattern;
};
inline std::map<DIR *, Opents_Find_Search> & Opents_Find_Searches(void) {
	static std::map<DIR *, Opents_Find_Search> searches;
	return (searches);
}

inline bool Opents_Find_Fill(Opents_Find_Search const & search, struct dirent const * e, WIN32_FIND_DATAA * data) {
	if (data == NULL) {
		return (true);
	}

	std::string const full = search.directory + "/" + e->d_name;

	struct stat st;
	if (::stat(full.c_str(), &st) != 0) {
		return (false);
	}

	memset(data, 0, sizeof(*data));
	data->dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
	data->ftCreationTime = PosixTimeToFileTime(st.st_ctime);
	data->ftLastAccessTime = PosixTimeToFileTime(st.st_atime);
	data->ftLastWriteTime = PosixTimeToFileTime(st.st_mtime);
	data->nFileSizeHigh = (DWORD)((unsigned long long)st.st_size >> 32);
	data->nFileSizeLow = (DWORD)((unsigned long long)st.st_size & 0xFFFFFFFFu);
	strncpy(data->cFileName, e->d_name, sizeof(data->cFileName) - 1);
	/* cAlternateFileName is the 8.3 form, which modern Windows leaves empty. */

	return (true);
}

/* Advances to the next entry that exists on disk and matches the pattern. */
inline bool Opents_Find_Next(DIR * d, WIN32_FIND_DATAA * data) {
	std::map<DIR *, Opents_Find_Search> & searches = Opents_Find_Searches();
	std::map<DIR *, Opents_Find_Search>::const_iterator found = searches.find(d);
	if (found == searches.end()) {
		return (false);
	}
	Opents_Find_Search const & search = found->second;

	struct dirent *e;
	while ((e = ::readdir(d)) != NULL) {
		if (e->d_name[0] == '.') {
			continue;				/* ".", "..", and dotfiles, which Windows hides */
		}
		if (!Opents_Wildcard_Match(search.pattern.c_str(), e->d_name)) {
			continue;
		}
		if (Opents_Find_Fill(search, e, data)) {
			return (true);
		}
	}
	return (false);
}

inline HANDLE FindFirstFileA(const char *pattern, WIN32_FIND_DATAA *data) {
	std::string const p(pattern ? pattern : "");
	size_t const slash = p.find_last_of('/');

	Opents_Find_Search search;
	search.directory = (slash == std::string::npos) ? "." : p.substr(0, slash);
	search.pattern = (slash == std::string::npos) ? p : p.substr(slash + 1);
	if (search.directory.empty()) {
		search.directory = "/";
	}

	DIR *d = ::opendir(search.directory.c_str());
	if (!d) {
		return (INVALID_HANDLE_VALUE);
	}

	Opents_Find_Searches()[d] = search;
	if (Opents_Find_Next(d, data)) {
		return ((HANDLE)d);
	}

	Opents_Find_Searches().erase(d);
	::closedir(d);
	return (INVALID_HANDLE_VALUE);
}
inline BOOL FindNextFileA(HANDLE h, WIN32_FIND_DATAA *data) {
	if (h == INVALID_HANDLE_VALUE) {
		return (FALSE);
	}
	return (Opents_Find_Next((DIR *)h, data) ? TRUE : FALSE);
}
inline BOOL FindClose(HANDLE h) {
	if (h == INVALID_HANDLE_VALUE) {
		return (FALSE);
	}
	DIR *d = (DIR *)h;
	Opents_Find_Searches().erase(d);
	::closedir(d);
	return (TRUE);
}

inline int MultiByteToWideChar(UINT cp, DWORD flags, const char *src, int srclen, wchar_t *dst, int dstlen) {
	(void)cp; (void)flags;
	int n = (srclen < 0 && src) ? (int)strlen(src) + 1 : srclen;
	if (!dst) return n;  /* return required length */
	int m = (dstlen < n) ? dstlen : n;
	for (int i = 0; i < m; ++i) dst[i] = (wchar_t)(unsigned char)src[i];
	return n;
}
inline int WideCharToMultiByte(UINT cp, DWORD flags, const wchar_t *src, int srclen, char *dst, int dstlen, const char *def, int *used) {
	(void)cp; (void)flags; (void)def; (void)used;
	int n = (srclen < 0 && src) ? (int)wcslen(src) + 1 : srclen;
	if (!dst) return n;
	int m = (dstlen < n) ? dstlen : n;
	for (int i = 0; i < m; ++i) dst[i] = (char)src[i];
	return n;
}

/* ---- process identity ---------------------------------------------------- */
/* Hollow versions of these leave the engine with nowhere to put its own
 * diagnostics: dbgprint.cpp derives the log directory from GetModuleFileName
 * and reads its options through CommandLineToArgvW, so with both empty there is
 * no log file and no way for a caller to ask for a debug console. */

/* The command line, owned here for the life of the process. A reference to a
 * function-local static rather than a namespace-scope variable, because this
 * header is force-included into every translation unit.
 *
 * It has to be readable before main runs. Startup code that runs as a static
 * initializer reads the command line too, and main is too late for it: a value
 * established only there is an empty string to everything constructed earlier.
 * The platform's own argc/argv are available from process start, so this builds
 * from those on first use rather than waiting to be told. */
#ifdef __APPLE__
#include <crt_externs.h>
#include <mach-o/dyld.h>
#include <pthread.h>
#endif

inline char * &opents_port_command_line(void)
{
	static char *value = []() -> char * {
#ifdef __APPLE__
		int argc = *_NSGetArgc();
		char ** argv = *_NSGetArgv();

		/*
		 * The engine takes its directories as command line switches and reads them
		 * back through CommandLineToArgvW, so this string is a round trip: what is
		 * joined here is split apart again there. An argument holding whitespace has
		 * to carry its own quotes, or the space in it becomes a word boundary and
		 * "-USERDIR=/Users/me/Library/Application Support/OpenTS" arrives as two
		 * arguments -- which is not a directory anyone has, so the launch fails on a
		 * path that plainly exists. The quoting is the platform's own convention,
		 * and the engine's parser strips the quotes back off, so the value it reads
		 * is the one that was passed.
		 */
		std::string joined;
		for (int index = 0; index < argc; index++) {
			std::string argument = argv[index] != nullptr ? argv[index] : "";

			if (argument.find_first_of(" \t\"") != std::string::npos) {
				// A quote inside the argument cannot be escaped the Windows way -- the
				// engine's parser drops '"' from every token it reads -- so one is left
				// alone here and the argument simply stays split. See gamedirs.cpp.
				if (argument.find('"') == std::string::npos) {
					argument = "\"" + argument + "\"";
				}
			}

			if (index != 0) {
				joined += ' ';
			}
			joined += argument;
		}
		return strdup(joined.c_str());
#else
		return nullptr;
#endif
	}();

	return value;
}

inline DWORD GetModuleFileNameA(HINSTANCE h, char *buf, DWORD sz) {
	(void)h;
	if (buf == nullptr || sz == 0) return 0;
	buf[0] = '\0';

#ifdef __APPLE__
	/* _NSGetExecutablePath reports the length it needs when the buffer is too
	 * small, and resolves the path itself rather than trusting argv[0], which
	 * is empty when the caller is port_main and WinMain receives none. */
	uint32_t size = (uint32_t)sz;
	if (_NSGetExecutablePath(buf, &size) != 0) return 0;
	return (DWORD)strlen(buf);
#else
	return 0;
#endif
}
inline DWORD GetModuleFileName(HINSTANCE h, char *buf, DWORD sz) { return GetModuleFileNameA(h, buf, sz); }
/* The engine asks for the attributes of a directory to decide whether a path it was
 * handed is one, so this has to report FILE_ATTRIBUTE_DIRECTORY rather than the
 * catch-all NORMAL. Reported earlier than the constant block below because this
 * definition is read here; the guard makes the later one a no-op. */
#ifndef FILE_ATTRIBUTE_DIRECTORY
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#endif
inline DWORD GetFileAttributesA(const char *p) {
	struct stat st;
	if (::stat(p, &st) != 0) {
		return (DWORD)-1;
	}
	return S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}
inline DWORD GetFileAttributes(const char *p) { return GetFileAttributesA(p); }

inline void OutputDebugStringA(const char *s) { if (s) fprintf(stderr, "%s", s); }
inline void OutputDebugString(const char *s) { OutputDebugStringA(s); }

/* ---- GUI APIs -- HOLLOW no-op stubs (need a real backend) --------------- */
inline LRESULT SendMessage(HWND w, UINT m, WPARAM wP, LPARAM lP) {
	return (LRESULT)opents_win_send((void *)w, (unsigned)m, (uintptr_t)wP, (intptr_t)lP);
}
inline BOOL   PostMessage(HWND w, UINT m, WPARAM wP, LPARAM lP) {
	// Queue a real message: the pump (Windows_Message_Handler) reads it back
	// through PeekMessage/GetMessage and hands it to Windows_Procedure.
	{
		extern void Win_Diag(char const *, ...);
		if (m == 0x014F /* CB_SHOWDROPDOWN */) {
			Win_Diag("post CB_SHOWDROPDOWN hwnd=%p wparam=%zu", (void *)w, (size_t)wP);
		}
	}
	OpentsMsg msg;
	msg.hwnd   = (void *)w;
	msg.message = (unsigned)m;
	msg.wParam = (uintptr_t)wP;
	msg.lParam = (intptr_t)lP;
	opents_msg_push(msg);
	return TRUE;
}
inline BOOL   InvalidateRect(HWND w, const RECT *r, BOOL e) { (void)w;(void)r;(void)e; return TRUE; }
inline BOOL   ShowWindow(HWND w, int c) {
	/*
	**  This used to be a unconditional no-op because a naive
	**  opents_win_set_visible() here painted windows whose owner-draw records
	**  did not exist yet and blacked dialogs out; that record hole has since
	**  been closed, and the no-op now breaks real users of ShowWindow: the
	**  combo drop-down list is created with WS_CHILD only -- no WS_VISIBLE --
	**  so without this it never becomes visible, is skipped by the paint tree
	**  (Paint_Tree skips invisible children) and the side/colour lists never
	**  drop down. SW_HIDE (0) hides; the other commands the engine uses all
	**  show.
	*/
	opents_win_set_visible((void *)w, (c != 0) ? 1 : 0);
	return TRUE;
}
inline BOOL   UpdateWindow(HWND w) { (void)w; return TRUE; }
inline HDC    GetDC(HWND w) {
	// The engine only asks for a DC to read device capabilities (VREFRESH); the
	// stub's GetDeviceCaps ignores the handle but the caller checks for NULL, so
	// hand back a non-null sentinel.
	(void)w;
	return (HDC)(void *)(uintptr_t)1;
}
inline int    ReleaseDC(HWND w, HDC d) { (void)w;(void)d; return 1; }
inline HDC    BeginPaint(HWND w, PAINTSTRUCT *ps) { (void)w; (void)ps; return NULL_HANDLE; }
inline BOOL   EndPaint(HWND w, const PAINTSTRUCT *ps) { (void)w;(void)ps; return TRUE; }
inline HWND   CreateWindowEx(DWORD ex, const char *cls, const char *name, DWORD style, int x, int y, int w, int h, HWND parent, HMENU menu, HINSTANCE inst, void *param) {
	(void)ex;(void)inst;
	/*
	**  Two kinds of window reach this shim, and they need opposite backing.
	**
	**  1. The engine's single top-level game window (parent == NULL). That needs
	**     a real Cocoa NSWindow: bgfx's Metal backend takes the NSWindow* as its
	**     native handle, and the port's input pump reads pointer state from it.
	**     It alone is the main window (Fix 1: only a parentless window may claim
	**     that role, so child windows cannot clobber the hit-test's anchor).
	**
	**  2. Child windows created outside the dialog-template path -- the combo
	**     box's dropped list ("ComboDropWin") and its attached scroll bar. These
	**     are owner-drawn onto the dialog surface exactly like template controls,
	**     so they must be *logical* Win nodes in the dialog's child tree, never
	**     native Cocoa windows. A native window here would hijack bgfx's render
	**     target (opents_window_create overwrites the shared g_Window) and throw
	**     a blank frame over the game. Such a child therefore gets no Cocoa
	**     window: it is built through opents_win_create, given the procedure its
	**     class registered, and handed a synthetic WM_CREATE so its control proc
	**     can read the creation parameter (the owner combo handle).
	*/
	if (parent == NULL) {
		bool const popup = (style & 0x80000000u) != 0;
		void * window = opents_window_create(x, y, w, h, popup ? 1 : 0);
		if (window != nullptr) {
			opents_win_set_main_window(window);
			/* Recorded for opents_win_proc_for(g_MainWindow); only meaningful
			** for the main window. Kept out of the child path, where it would
			** otherwise overwrite g_MainClassName with "ComboDropWin" and
			** misroute every main-window proc lookup. */
			opents_win_register_class_name(cls);
			int cw = 0;
			int ch = 0;
			opents_window_get_client_size(window, &cw, &ch);

			OpentsMsg activate;
			activate.hwnd = window;
			activate.message = 0x001C;   // WM_ACTIVATEAPP
			activate.wParam = 1;
			activate.lParam = 0;
			opents_msg_push(activate);

			OpentsMsg resize;
			resize.hwnd = window;
			resize.message = 0x0005;     // WM_SIZE
			resize.wParam = 0;
			resize.lParam = ((intptr_t)ch << 16) | (intptr_t)cw;
			opents_msg_push(resize);
		}
		return (HWND)window;
	}

	/* Child window: a logical node only, registered under its parent (the dialog
	** for the combo drop-down, the drop-down for its scroll bar) so the hit-test
	** and paint walks reach it. */
	OpentsWin child = opents_win_create(cls, name, style, ex, x, y, w, h,
	                                   (OpentsWin)parent, (unsigned)(uintptr_t)menu,
	                                   nullptr, 0);
	if (child != nullptr) {
		/* Give it the procedure its class was registered with (e.g.
		** ComboDropWinCtrlProc for "ComboDropWin"). In Win32 this comes from the
		** WNDCLASS; here it must be applied explicitly, or every message -- and
		** the synthetic WM_CREATE below -- falls through to Default_Control_Proc
		** and the drop-down never learns its owner combo. */
		void * proc = opents_win_proc_for(child);
		if (proc != nullptr) {
			opents_win_set_long(child, GWL_WNDPROC, (uintptr_t)proc);
		}
		/* Synthetic WM_CREATE (0x0001). Win32 sends this from inside
		** CreateWindowEx; the port's opents_win_create does not, so emulate it.
		** lParam is the CREATESTRUCT's lpCreateParams -- a pointer to the owner
		** handle the caller passed as `param`. ComboDropWinCtrlProc_Internal
		** reads OwnerComboHandle from it: *(HWND *)lParam. */
		opents_win_send(child, 0x0001, 0, (intptr_t)&param);
	}
	return (HWND)child;
}
inline LRESULT DefWindowProc(HWND w, UINT m, WPARAM wP, LPARAM lP) {
	return (LRESULT)opents_win_def_proc((void *)w, (unsigned)m, (uintptr_t)wP, (intptr_t)lP);
}
inline LONG   GetWindowLong(HWND w, int i) { return (LONG)(intptr_t)opents_win_get_long((void *)w, i); }
inline LONG   SetWindowLong(HWND w, int i, LONG v) {
	return (LONG)(intptr_t)opents_win_set_long((void *)w, i, (uintptr_t)(intptr_t)v);
}
inline void   SetCapture(HWND w) { opents_input_set_capture((void *)w); }
inline void   ReleaseCapture(void) { opents_input_set_capture(nullptr); }
inline UINT   SetTimer(HWND w, UINT id, UINT elapse, void *proc) { (void)w;(void)id;(void)elapse;(void)proc; return 1; }
inline BOOL   KillTimer(HWND w, UINT id) { (void)w;(void)id; return TRUE; }
inline void   SetCursor(HCURSOR c) { opents_cursor_apply(c); }
inline int    MessageBoxA(HWND w, const char *t, const char *c, UINT type) { (void)w;(void)t;(void)c;(void)type; return 1; }
inline int    MessageBox(HWND w, const char *t, const char *c, UINT type) { return MessageBoxA(w, t, c, type); }
inline BOOL   GetClientRect(HWND w, RECT *r) {
	if (r != nullptr) { r->left = r->top = 0; r->right = r->bottom = 0; }
	if (w != nullptr) {
		int cw = 0;
		int ch = 0;
		if (opents_win_is_main((void *)w)) {
			// The drawable area of the real Cocoa window (already in physical
			// pixels, matching the process's DPI-aware expectation).
			opents_window_get_client_size((void *)w, &cw, &ch);
		} else {
			// A logical window has no frame to subtract, so client == window.
			opents_win_get_client_size((void *)w, &cw, &ch);
		}
		if (r != nullptr) { r->right = cw; r->bottom = ch; }
	}
	return TRUE;
}
inline BOOL   GetCursorPos(POINT *p) {
	// The pointer's position, which the event pump keeps current. Answering
	// without filling the POINT would leave callers reading an uninitialised
	// value -- WWMouseClass::Get_Bounded_Position passes one straight through
	// Convert_Coordinate, so the mouse would report a position that came from
	// whatever was on the stack.
	if (p != nullptr) {
		int x = 0;
		int y = 0;
		opents_window_get_cursor_pos(&x, &y);
		p->x = x;
		p->y = y;
	}
	return TRUE;
}
inline HGDIOBJ SelectObject(HDC d, HGDIOBJ o) { return (HGDIOBJ)opents_gdi_select_object((void *)d, (void *)o); }
inline BOOL   SetTextColor(HDC d, COLORREF c) { (void)d;(void)c; return TRUE; }
inline int    SetBkMode(HDC d, int m) { (void)d;(void)m; return 0; }

/* ---- MSVC limits / string helpers ---------------------------------------- */
#ifndef _MAX_PATH
#define _MAX_PATH   260
#define _MAX_DRIVE  3
#define _MAX_DIR    256
#define _MAX_FNAME  255
#define _MAX_EXT    256
#define _MAX_BASE   255
#endif
#ifndef MAX_PATH
#define MAX_PATH    260
#endif

#include "port/port_string_shim.h"

/* Real <windows.h> defines min/max as macros (unless NOMINMAX). Replicate so
 * engine code that relies on the macro form (e.g. min(a,b) with mixed types)
 * parses exactly as it does on Windows. */
#ifndef NOMINMAX
#ifndef min
#define min(a,b)   (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)   (((a) > (b)) ? (a) : (b))
#endif
#endif /* NOMINMAX */
/* MSVC <ctype.h> defines this as a ctype bitmask; the engine uses it as the
 * control-character threshold (c <= _CONTROL). 0x20 matches MSVC's _CONTROL. */
#ifndef _CONTROL
#define _CONTROL 0x20
#endif

/* ---- Misc Win32 constants / hollow stubs still referenced by the core ----- */
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif
#ifndef GW_CHILD
#define GW_CHILD 5
#endif
#ifndef FW_NORMAL
#define FW_NORMAL 400
#endif
#ifndef PM_NOREMOVE
#define PM_NOREMOVE 0x0000
#endif

inline SHORT  GetKeyState(int vKey) { return opents_input_get_key_state(vKey); }
inline HMODULE GetModuleHandle(const char * /*lpModuleName*/) { return NULL_HANDLE; }
inline BOOL   QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency) {
	if (lpFrequency) { lpFrequency->QuadPart = 1000000; }
	return TRUE;
}
/*
**  One desktop, as on Win32.
**
**  The engine's window math mixes three kinds of callers: Get_Display_Rect
**  subtracts ClientToScreen(MainWindow) from GetWindowRect(dialog) and needs
**  the difference to be the dialog's position in game-frame coordinates;
**  Move_Dialog and Center_Window_Within_Window run the same subtraction in
**  reverse; and the cursor path (vidscale) starts from GetCursorPos, which is
**  native desktop pixels. That only works when logical windows report their
**  rects in the same space ClientToScreen produces -- exactly what Win32
**  gives, since its game window's client was the game frame, one pixel each.
**
**  So a logical window's screen rect is the sum of the parent-relative rects
**  up the chain (game-frame units), anchored at the main window's client
**  origin in real desktop pixels. The main window itself reports its native
**  frame, and client<->screen conversion on it stays purely native.
*/
inline void Opents_Logical_Screen_Rect(HWND hWnd, int *sx, int *sy, int *sw, int *sh)
{
	int x = 0, y = 0, w = 0, h = 0;
	if (hWnd != nullptr) {
		opents_win_get_rect((void *)hWnd, &x, &y, &w, &h);
	}
	HWND parent = (hWnd != nullptr) ? (HWND)opents_win_get_parent((void *)hWnd) : nullptr;
	while (parent != nullptr && !opents_win_is_main((void *)parent)) {
		int px = 0, py = 0, pw = 0, ph = 0;
		opents_win_get_rect((void *)parent, &px, &py, &pw, &ph);
		x += px;
		y += py;
		parent = (HWND)opents_win_get_parent((void *)parent);
	}
	{
		/* Anchor the chain at the main window's client origin. The manager
		 * creates dialog frames top-level (owner is not a Win), so a chain
		 * that runs out of parents has still reached the window that owns
		 * the screen the dialog draws over. */
		HWND anchor = (parent != nullptr) ? parent : (HWND)opents_win_main_window();
		if (anchor != nullptr) {
			opents_window_client_to_screen((void *)anchor, &x, &y);
		}
	}
	if (sx) *sx = x;
	if (sy) *sy = y;
	if (sw) *sw = w;
	if (sh) *sh = h;
}

inline BOOL   ScreenToClient(HWND hWnd, POINT *lpPoint) {
	if (lpPoint != nullptr) {
		int x = (int)lpPoint->x;
		int y = (int)lpPoint->y;
		if (hWnd != nullptr && opents_win_is_main((void *)hWnd)) {
			opents_window_screen_to_client((void *)hWnd, &x, &y);
		} else {
			int ox = 0, oy = 0, ow = 0, oh = 0;
			Opents_Logical_Screen_Rect(hWnd, &ox, &oy, &ow, &oh);
			x -= ox;
			y -= oy;
		}
		lpPoint->x = x;
		lpPoint->y = y;
	}
	return TRUE;
}
inline BOOL   ClientToScreen(HWND hWnd, POINT *lpPoint) {
	// Note the engine sometimes passes the address of a RECT here and expects
	// both of its corners translated, exactly as Win32 allows.
	if (lpPoint != nullptr) {
		int x = (int)lpPoint->x;
		int y = (int)lpPoint->y;
		if (hWnd != nullptr && opents_win_is_main((void *)hWnd)) {
			opents_window_client_to_screen((void *)hWnd, &x, &y);
		} else {
			int ox = 0, oy = 0, ow = 0, oh = 0;
			Opents_Logical_Screen_Rect(hWnd, &ox, &oy, &ow, &oh);
			x += ox;
			y += oy;
		}
		lpPoint->x = x;
		lpPoint->y = y;
	}
	return TRUE;
}
/* Reads the string table out of the module's PE image.
 *
 * Every localized string in the game comes through here, and returning 0 made
 * Get_Res_String hand back an empty string for all of them. The text arrives as
 * Windows-1252 because that is what a narrow Win32 API produces and what the
 * caller converts from; port_resource.h carries the reasoning. */
inline int    LoadString(HINSTANCE hInst, UINT uID, char *lpBuffer, int nBufferMax) {
	return opents_resource_load_string((void *)hInst, (unsigned)uID, lpBuffer, nBufferMax);
}
inline BOOL   CloseWindow(HWND /*hWnd*/) { return TRUE; }
inline DWORD  GetFileVersionInfoSize(const char * /*lptstrFilename*/, DWORD * /*lpdwHandle*/) { return 0; }
inline HWND   GetParent(HWND h) { return (HWND)opents_win_get_parent((void *)h); }

// Keyboard messages carry no position in their lParam, so the engine's mouse
// router cannot pick their target -- the OS picks it by the focus instead. The
// values are written out because this header is parsed before shim/winuser.h
// defines the WM_* macros.
inline bool Opents_Is_Keyboard_Message(unsigned m) {
	switch (m) {
	case 0x0100: // WM_KEYDOWN
	case 0x0101: // WM_KEYUP
	case 0x0102: // WM_CHAR
	case 0x0103: // WM_DEADCHAR
	case 0x0104: // WM_SYSKEYDOWN
	case 0x0105: // WM_SYSKEYUP
	case 0x0106: // WM_SYSCHAR
	case 0x0107: // WM_SYSDEADCHAR
		return true;
	default:
		return false;
	}
}

inline BOOL   DispatchMessage(const MSG *lpMsg) {
	// Hand the message to the registered window procedure. This is what turns a
	// queued WM_ACTIVATEAPP into the GameInFocus flip the startup loop waits on.
	if (lpMsg != nullptr) {
		{
			extern void Win_Diag(char const *, ...);
			if (lpMsg->message == 0x014F /* CB_SHOWDROPDOWN */) {
				Win_Diag("dispatch CB_SHOWDROPDOWN hwnd=%p wparam=%zu", (void *)lpMsg->hwnd, (size_t)lpMsg->wParam);
			}
		}
		// Keyboard input goes to whichever window holds the focus, exactly as
		// the OS delivers it: with a control focused its own procedure gets the
		// keystroke, not the main window's. The port had no such rule, so a
		// control procedure never saw a key and an edit box stayed empty
		// whatever the player typed. Mouse messages keep their own window,
		// because the engine's router re-targets those from the position they
		// carry.
		void * target = (void *)lpMsg->hwnd;
		if (Opents_Is_Keyboard_Message((unsigned)lpMsg->message)) {
			void * focus = (void *)opents_win_get_focus();
			if (focus != nullptr) {
				target = focus;
			}
		}

		// Prefer the procedure registered for the target window's own class, and
		// fall back to the most recently registered one. Without the class lookup,
		// every queued message goes to whichever class was registered last.
		//
		// A logical child window (a dialog control) that the engine subclassed
		// carries its own procedure -- SetWindowLong(GWL_WNDPROC) stored it --
		// and Win32's DispatchMessage delivers to that procedure, not to the
		// class registry. The engine subclasses its owner-draw combos with
		// ComboBoxCtrlProc but never RegisterClass("ComboBox") (a system class),
		// so the class lookup below returns null and a posted message -- e.g.
		// the CB_SHOWDROPDOWN the combo posts on press -- was falling through
		// to the main window's global proc. The dropdown was therefore never
		// created. SendMessage reached the control because opents_win_send uses
		// the window's own proc; queued messages now do the same. This must be
		// consulted before the class lookup: a subclassed window's proc is the
		// authority for that window, exactly as on Win32.
		void * own_proc = nullptr;
		uintptr_t own_ptr = opents_win_get_long((void *)target, GWL_WNDPROC);
		if (own_ptr != 0) {
			own_proc = (void *)own_ptr;
		}
		void * by_class = opents_win_proc_for(target);
		void * resolved = own_proc != nullptr ? own_proc
			: (by_class != nullptr ? by_class : (void *)opents_get_wndproc());
		// A window the engine never subclassed reads back the def-proc trampoline
		// for GWL_WNDPROC; sending it the default procedure would skip the class
		// and global routing that would otherwise apply.
		if (resolved == (void *)opents_win_def_proc) {
			resolved = by_class != nullptr ? by_class : (void *)opents_get_wndproc();
		}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type"
		OpentsWndProc proc = (OpentsWndProc)resolved;
#pragma clang diagnostic pop
		if (proc != nullptr) {
			proc(target, (unsigned)lpMsg->message,
			     (uintptr_t)lpMsg->wParam, (intptr_t)lpMsg->lParam);
		}
	}
	return TRUE;
}
inline LONG_PTR SetWindowLongPtr(HWND hWnd, int nIndex, LONG_PTR dwNewLong) {
	return (LONG_PTR)opents_win_set_long((void *)hWnd, nIndex, (uintptr_t)dwNewLong);
}
inline LONG_PTR GetWindowLongPtr(HWND hWnd, int nIndex) {
	return (LONG_PTR)opents_win_get_long((void *)hWnd, nIndex);
}
inline BOOL   EnableWindow(HWND hWnd, BOOL bEnable) {
	opents_win_set_enabled((void *)hWnd, bEnable ? 1 : 0);
	return TRUE;
}
inline int    GetWindowText(HWND hWnd, char *lpString, int nMaxCount) {
	if (lpString != nullptr && nMaxCount <= 0) return 0;
	return opents_win_get_text((void *)hWnd, lpString, (unsigned)nMaxCount);
}

/* ---- More Win32 types / constants referenced by the engine --------------- */
#ifndef FILE_ATTRIBUTE_DIRECTORY
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#endif

/* Interlocked* : defined in com_stub.h (the universal COM/Win32 home), which is
 * included above. Kept there so the volatile-LONG overloads needed by cstream.cpp
 * are available everywhere; do NOT redefine here. */

typedef struct SYSTEMTIME {
	WORD wYear; WORD wMonth; WORD wDayOfWeek; WORD wDay;
	WORD wHour; WORD wMinute; WORD wSecond; WORD wMilliseconds;
} SYSTEMTIME;

typedef struct POINTS { SHORT x; SHORT y; } POINTS;
typedef POINT *LPPOINT;

typedef struct MEMORYSTATUS {
	DWORD dwLength;
	DWORD dwMemoryLoad;
	SIZE_T dwTotalPhys;
	SIZE_T dwAvailPhys;
	SIZE_T dwTotalPageFile;
	SIZE_T dwAvailPageFile;
	SIZE_T dwTotalVirtual;
	SIZE_T dwAvailVirtual;
} MEMORYSTATUS;

/* ---- Keyboard layout translation --------------------------------------
 *
 * WWKeyboardClass::To_ASCII turns a buffered key into a character with
 *
 *   MapVirtualKey(key & 0xFF, 0) -> ToUnicode(vk, scancode, KeyState, ...)
 *
 * so both of these have to do real work. They used to answer 0, which is
 * indistinguishable from "this key has no character" and left every text field
 * in the game accepting nothing at all.
 *
 * The layout is US, which is what the engine's own key names assume (keyboard.h
 * calls 0xBA the semicolon key and 0xC0 the grave key). A different layout
 * would need a real keymap; the structure below is where one would go.
 */

/* Scan code (set 1) for a virtual key. MapVirtualKey(vk, MAPVK_VK_TO_VSC). */
inline UINT opents_vk_to_scancode(UINT vk)
{
	/* The letters are contiguous in both spaces but in a scrambled order, so
	 * the run is a table rather than arithmetic. */
	if (vk >= 0x41 && vk <= 0x5A) {
		static const unsigned char letter[26] = {
			0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
			0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C
		};
		return letter[vk - 0x41];
	}

	/* The number row runs 0x02..0x0B in the order the keys are printed, so 0
	 * is the last of the run and '1' is the first. */
	if (vk >= 0x30 && vk <= 0x39) {
		return (vk == 0x30) ? 0x0B : (vk - 0x30 + 0x01);
	}

	switch (vk) {
		case 0x08: return 0x0E;  /* Backspace   */
		case 0x09: return 0x0F;  /* Tab         */
		case 0x0C: return 0x4C;  /* Clear       */
		case 0x0D: return 0x1C;  /* Return      */
		case 0x10: return 0x2A;  /* Shift       */
		case 0x11: return 0x1D;  /* Control     */
		case 0x12: return 0x38;  /* Menu / Alt  */
		case 0x13: return 0x45;  /* Pause       */
		case 0x14: return 0x3A;  /* Caps Lock   */
		case 0x1B: return 0x01;  /* Escape      */
		case 0x20: return 0x39;  /* Space       */
		case 0x21: return 0x49;  /* Prior       */
		case 0x22: return 0x51;  /* Next        */
		case 0x23: return 0x4F;  /* End         */
		case 0x24: return 0x47;  /* Home        */
		case 0x25: return 0x4B;  /* Left        */
		case 0x26: return 0x48;  /* Up          */
		case 0x27: return 0x4D;  /* Right       */
		case 0x28: return 0x50;  /* Down        */
		case 0x2C: return 0x37;  /* Print Screen (the keypad asterisk shares it) */
		case 0x2D: return 0x52;  /* Insert      */
		case 0x2E: return 0x53;  /* Delete      */
		case 0x5B: case 0x5C: case 0x5D:  /* Left/Right Windows, Apps */
			return 0x5B;
		case 0x60: return 0x52;  /* Keypad 0 shares Delete's code */
		case 0x61: return 0x4F;
		case 0x62: return 0x50;
		case 0x63: return 0x51;
		case 0x64: return 0x4B;
		case 0x65: return 0x4C;
		case 0x66: return 0x4D;
		case 0x67: return 0x47;
		case 0x68: return 0x48;
		case 0x69: return 0x49;
		case 0x6A: return 0x37;  /* Keypad *   */
		case 0x6B: return 0x4E;  /* Keypad +   */
		case 0x6D: return 0x4A;  /* Keypad -   */
		case 0x6E: return 0x53;  /* Keypad .   */
		case 0x6F: return 0x35;  /* Keypad /   */
		case 0x90: return 0x45;  /* Num Lock   */
		case 0x91: return 0x46;  /* Scroll Lock */
		case 0xBA: return 0x27;  /* ;  "        */
		case 0xBB: return 0x0D;  /* =  +        */
		case 0xBC: return 0x33;  /* ,  <        */
		case 0xBD: return 0x0C;  /* -  _        */
		case 0xBE: return 0x34;  /* .  >        */
		case 0xBF: return 0x35;  /* /  ?        */
		case 0xC0: return 0x29;  /* `  ~        */
		case 0xDB: return 0x1A;  /* [  {        */
		case 0xDC: return 0x2B;  /* \  |        */
		case 0xDD: return 0x1B;  /* ]  }        */
		case 0xDE: return 0x28;  /* '  "        */
		default: break;
	}

	/* F1..F10 then F11/F12, which Windows separates. */
	if (vk >= 0x70 && vk <= 0x79) {
		return 0x3B + (vk - 0x70);
	}
	if (vk == 0x7A) return 0x57;
	if (vk == 0x7B) return 0x58;

	return 0;
}


/* The character a virtual key produces on a US layout, or 0 if it produces
 * none. Caps Lock affects the letters only, and inverts with Shift. */
inline wchar_t opents_vk_to_ascii(UINT vk, bool shift, bool caps)
{
	if (vk >= 0x41 && vk <= 0x5A) {
		const bool upper = shift ? !caps : caps;
		return (wchar_t)(upper ? vk : (vk + 0x20));
	}

	if (vk >= 0x30 && vk <= 0x39) {
		static const char plain[10] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
		static const char shifted[10] = { ')', '!', '@', '#', '$', '%', '^', '&', '*', '(' };
		return (wchar_t)(shift ? shifted[vk - 0x30] : plain[vk - 0x30]);
	}

	/* The keypad reports its digit whatever Shift is doing. */
	if (vk >= 0x60 && vk <= 0x69) {
		return (wchar_t)('0' + (vk - 0x60));
	}

	switch (vk) {
		case 0x08: return L'\b';
		case 0x09: return L'\t';
		case 0x0D: return L'\r';
		case 0x1B: return (wchar_t)0x1B;
		case 0x20: return L' ';
		case 0x6A: return L'*';
		case 0x6B: return L'+';
		case 0x6D: return L'-';
		case 0x6E: return L'.';
		case 0x6F: return L'/';
		case 0xBA: return shift ? L':' : L';';
		case 0xBB: return shift ? L'+' : L'=';
		case 0xBC: return shift ? L'<' : L',';
		case 0xBD: return shift ? L'_' : L'-';
		case 0xBE: return shift ? L'>' : L'.';
		case 0xBF: return shift ? L'?' : L'/';
		case 0xC0: return shift ? L'~' : L'`';
		case 0xDB: return shift ? L'{' : L'[';
		case 0xDC: return shift ? L'|' : L'\\';
		case 0xDD: return shift ? L'}' : L']';
		case 0xDE: return shift ? L'"' : (wchar_t)0x27;
		default: break;
	}

	return 0;
}


inline UINT MapVirtualKey(UINT uCode, UINT uMapType) {
	/* Only MAPVK_VK_TO_VSC is needed; the reverse direction and the character
	 * mappings are not referenced anywhere in the engine. */
	if (uMapType != 0) {
		return 0;
	}
	return opents_vk_to_scancode(uCode);
}
inline BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount) {
	if (lpPerformanceCount) lpPerformanceCount->QuadPart = 0;
	return TRUE;
}
inline BOOL GetFileVersionInfo(const char * /*lptstrFilename*/, DWORD /*dwHandle*/, DWORD /*dwLen*/, void * /*lpData*/) { return FALSE; }

/* ---- Owner-draw / dialog types (referenced by the GUI dialog layer) ------- */
typedef struct DRAWITEMSTRUCT {
	UINT  CtlType;
	UINT  CtlID;
	UINT  itemID;
	UINT  itemAction;
	UINT  itemState;
	HWND  hwndItem;
	HDC   hDC;
	RECT  rcItem;
	ULONG_PTR itemData;
} DRAWITEMSTRUCT;
typedef DRAWITEMSTRUCT *LPDRAWITEMSTRUCT;

typedef struct DEVMODE {
	wchar_t dmDeviceName[32];
	WORD    dmSpecVersion;
	WORD    dmDriverVersion;
	WORD    dmSize;
	WORD    dmDriverExtra;
	DWORD   dmFields;
	/* Remaining fields elided; the headless build does not use them. */
	WORD    dmOrientation;
	WORD    dmPaperSize;
	WORD    dmCopies;
	WORD    dmDefaultSource;
	WORD    dmPrintQuality;
	WORD    dmColor;
	WORD    dmDuplex;
	WORD    dmYResolution;
	WORD    dmTTOption;
	WORD    dmCollate;
	wchar_t dmFormName[32];
	WORD    dmLogPixels;
	DWORD   dmBitsPerPel;
	DWORD   dmPelsWidth;
	DWORD   dmPelsHeight;
	DWORD   dmDisplayFlags;
	DWORD   dmDisplayFrequency;
} DEVMODE, *LPDEVMODE;

/* ---- Misc Win32 APIs still referenced by otherwise-portable code ---------- */
inline void GlobalMemoryStatus(MEMORYSTATUS *lpBuffer) {
	if (lpBuffer) { lpBuffer->dwLength = sizeof(MEMORYSTATUS); lpBuffer->dwMemoryLoad = 0; }
}
/* Real wall-clock time. Hollow versions leave the log timestamped
 * 0000-00-00 00:00:00, which names the file DEBUG_00-00-0000_00-00-00.LOG and
 * stamps every record 00:00:00.000 — including the ones a startup hang is
 * diagnosed from. */
inline void GetLocalTime(SYSTEMTIME *lpSystemTime) {
	if (lpSystemTime == nullptr) return;

	time_t now = ::time(nullptr);
	struct tm parts;
	localtime_r(&now, &parts);

	lpSystemTime->wYear         = static_cast<WORD>(parts.tm_year + 1900);
	lpSystemTime->wMonth        = static_cast<WORD>(parts.tm_mon + 1);
	lpSystemTime->wDayOfWeek    = static_cast<WORD>(parts.tm_wday);
	lpSystemTime->wDay          = static_cast<WORD>(parts.tm_mday);
	lpSystemTime->wHour         = static_cast<WORD>(parts.tm_hour);
	lpSystemTime->wMinute       = static_cast<WORD>(parts.tm_min);
	lpSystemTime->wSecond       = static_cast<WORD>(parts.tm_sec);

	struct timeval clock;
	gettimeofday(&clock, nullptr);
	lpSystemTime->wMilliseconds = static_cast<WORD>(clock.tv_usec / 1000);
}
inline BOOL VerQueryValue(const void * /*pBlock*/, const char * /*lpSubBlock*/, void **ppBuffer, UINT *puLen) {
	(void)ppBuffer; (void)puLen; return FALSE;
}
inline BOOL GetMessage(MSG *lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax) {
	(void)hWnd; (void)wMsgFilterMin; (void)wMsgFilterMax;
	OpentsMsg m;
	if (opents_msg_pop(&m)) {
		if (lpMsg != nullptr) {
			lpMsg->hwnd = (HWND)m.hwnd;
			lpMsg->message = m.message;
			lpMsg->wParam = (WPARAM)m.wParam;
			lpMsg->lParam = (LPARAM)m.lParam;
			lpMsg->time = 0;
			lpMsg->pt.x = 0;
			lpMsg->pt.y = 0;
		}
		return TRUE;   // non-zero: a message was retrieved (WM_QUIT would return 0)
	}
	return FALSE;
}
inline BOOL ClipCursor(const RECT *lpRect) {
	// Confines the pointer to a rectangle while the engine holds the mouse.
	// A null rectangle is Win32's "release the confinement".
	if (lpRect == nullptr) {
		opents_window_clip_cursor(nullptr);
		return TRUE;
	}
	int rect[4] = { (int)lpRect->left, (int)lpRect->top, (int)lpRect->right, (int)lpRect->bottom };
	opents_window_clip_cursor(rect);
	return TRUE;
}

/* ============================================================================
 * PHASE 1 STUB CLOSURE -- types & structs
 * Closes the remaining Win32 type/struct references so the full 412-TU tree
 * parses headlessly. Values/layouts are realistic; functionality is hollow.
 * ========================================================================== */
#ifndef CHAR
typedef char CHAR;
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
typedef const void *LPCVOID;

/* extra opaque handle / scalar aliases */
typedef BYTE  *PBYTE;
typedef void  *HRSRC;
typedef void  *HLOCAL;
typedef void (*FARPROC)(void);
typedef BOOL  (CALLBACK *WNDENUMPROC)(HWND, LPARAM);
typedef void  (CALLBACK *MSGBOXCALLBACK)(void *, UINT);
typedef SIZE  *LPSIZE;
typedef RECT   tagRECT;
typedef struct TVITEM *LPTVITEM;
typedef struct NMTREEVIEW *LPNMTREEVIEW;
typedef const struct DLGTEMPLATE *LPCDLGTEMPLATE;
#ifndef errno_t
typedef int errno_t;
#endif
typedef struct SRWLOCK { void *ptr; } SRWLOCK;
#define SRWLOCK_INIT { nullptr }
inline void InitializeSRWLock(SRWLOCK *) {}
inline void AcquireSRWLockExclusive(SRWLOCK *) {}
inline void ReleaseSRWLockExclusive(SRWLOCK *) {}

/* GDI / bitmap structures */
typedef struct RGBQUAD { BYTE rgbBlue; BYTE rgbGreen; BYTE rgbRed; BYTE rgbReserved; } RGBQUAD;
typedef struct BITMAPINFOHEADER {
	DWORD biSize; LONG biWidth; LONG biHeight; WORD biPlanes; WORD biBitCount;
	DWORD biCompression; DWORD biSizeImage; LONG biXPelsPerMeter; LONG biYPelsPerMeter;
	DWORD biClrUsed; DWORD biClrImportant;
} BITMAPINFOHEADER;
typedef struct BITMAPINFO { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[1]; } BITMAPINFO;
typedef struct BITMAPFILEHEADER {
	WORD bfType; DWORD bfSize; WORD bfReserved1; WORD bfReserved2; DWORD bfOffBits;
} BITMAPFILEHEADER;
typedef struct BITMAP {
	LONG bmType; LONG bmWidth; LONG bmHeight; LONG bmWidthBytes;
	WORD bmPlanes; WORD bmBitsPixel; LPVOID bmBits;
} BITMAP;
typedef struct DIBSECTION {
	BITMAP dsBm; BITMAPINFOHEADER dsBmih; DWORD dsBitfields[3]; HANDLE dshSection; DWORD dsOffset;
} DIBSECTION;
typedef struct ICONINFO {
	BOOL fIcon; DWORD xHotspot; DWORD yHotspot; HBITMAP hbmMask; HBITMAP hbmColor;
} ICONINFO;

/* file-search / PE-image structures */
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
typedef struct IMAGE_DOS_HEADER {
	WORD e_magic; WORD e_cblp; WORD e_cp; WORD e_crlc; WORD e_cparhdr; WORD e_minalloc;
	WORD e_maxalloc; WORD e_ss; WORD e_sp; WORD e_csum; WORD e_ip; WORD e_cs; WORD e_lfarlc;
	WORD e_ovno; WORD e_res[4]; WORD e_oemid; WORD e_oeminfo; WORD e_res2[10]; LONG e_lfanew;
} IMAGE_DOS_HEADER;
#define IMAGE_DOS_SIGNATURE 0x5A4D
typedef struct IMAGE_DATA_DIRECTORY { DWORD VirtualAddress; DWORD Size; } IMAGE_DATA_DIRECTORY;
typedef struct IMAGE_FILE_HEADER {
	WORD Machine; WORD NumberOfSections; DWORD TimeDateStamp; DWORD PointerToSymbolTable;
	DWORD NumberOfSymbols; WORD SizeOfOptionalHeader; WORD Characteristics;
} IMAGE_FILE_HEADER;
typedef struct IMAGE_OPTIONAL_HEADER32 {
	WORD Magic; BYTE MajorLinkerVersion; BYTE MinorLinkerVersion; DWORD SizeOfCode;
	DWORD SizeOfInitializedData; DWORD SizeOfUninitializedData; DWORD AddressOfEntryPoint;
	DWORD BaseOfCode; DWORD BaseOfData; DWORD ImageBase; DWORD SectionAlignment; DWORD FileAlignment;
	WORD MajorOperatingSystemVersion; WORD MinorOperatingSystemVersion; WORD MajorImageVersion;
	WORD MinorImageVersion; WORD MajorSubsystemVersion; WORD MinorSubsystemVersion;
	DWORD Win32VersionValue; DWORD SizeOfImage; DWORD SizeOfHeaders; DWORD CheckSum;
	WORD Subsystem; WORD DllCharacteristics; DWORD SizeOfStackReserve; DWORD SizeOfStackCommit;
	DWORD SizeOfHeapReserve; DWORD SizeOfHeapCommit; DWORD LoaderFlags; DWORD NumberOfRvaAndSizes;
	IMAGE_DATA_DIRECTORY DataDirectory[16];
} IMAGE_OPTIONAL_HEADER32;
#define IMAGE_NT_OPTIONAL_HDR32_MAGIC 0x10b
typedef struct IMAGE_NT_HEADERS32 {
	DWORD Signature; IMAGE_FILE_HEADER FileHeader; IMAGE_OPTIONAL_HEADER32 OptionalHeader;
} IMAGE_NT_HEADERS32;
typedef IMAGE_NT_HEADERS32 IMAGE_NT_HEADERS;
#define IMAGE_NT_SIGNATURE 0x00004550

/* scrollbar / monitor / console structures */
typedef struct SCROLLINFO {
	UINT cbSize; UINT fMask; int nMin; int nMax; UINT nPage; int nPos; int nTrackPos;
} SCROLLINFO;
typedef struct MONITORINFO { DWORD cbSize; RECT rcMonitor; RECT rcWork; DWORD dwFlags; } MONITORINFO;
typedef struct MONITORINFOEX { DWORD cbSize; RECT rcMonitor; RECT rcWork; DWORD dwFlags; CHAR szDevice[32]; } MONITORINFOEX;
#define MONITOR_DEFAULTTONEAREST 0x0002
typedef struct COORD { SHORT X; SHORT Y; } COORD;
typedef struct SMALL_RECT { SHORT Left; SHORT Top; SHORT Right; SHORT Bottom; } SMALL_RECT;
typedef struct CONSOLE_SCREEN_BUFFER_INFO {
	COORD dwSize; COORD dwCursorPosition; WORD wAttributes; SMALL_RECT srWindow; COORD dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO;

/* dialog / tree-view / notify structures */
/* NOTE: the real Win32 MSGBOXPARAMS uses LPCTSTR (generic TCHAR strings), not
 * LPWSTR. The engine is compiled in the narrow (ANSI) character set, so these
 * must be LPCTSTR -> const char* to accept Fetch_String() results. */
typedef struct MSGBOXPARAMS {
	UINT cbSize; HWND hwndOwner; HINSTANCE hInstance; LPCTSTR lpszText; LPCTSTR lpszCaption;
	DWORD dwStyle; LPCTSTR lpszIcon; DWORD dwContextHelpId; MSGBOXCALLBACK lpfnMsgBoxCallback; DWORD dwLanguageId;
} MSGBOXPARAMS;
typedef struct DLGTEMPLATE {
	DWORD style; DWORD dwExtendedStyle; WORD cdit; WORD x; WORD y; WORD cx; WORD cy;
} DLGTEMPLATE;
typedef struct NMHDR { HWND hwndFrom; UINT_PTR idFrom; UINT code; } NMHDR;
typedef struct TVITEM {
	UINT mask; HTREEITEM hItem; UINT state; UINT stateMask; LPTSTR pszText;
	int cchTextMax; int iImage; int iSelectedImage; int cChildren; LPARAM lParam;
} TVITEM, *LPTV_ITEM;
typedef struct TVINSERTSTRUCT {
	HTREEITEM hParent; HTREEITEM hInsertAfter;
	union { TVITEM item; TVITEM itemex; } DUMMYUNIONNAME;
} TVINSERTSTRUCT, *LPTVINSERTSTRUCT;
typedef struct NMTREEVIEW { NMHDR hdr; UINT action; TVITEM itemOld; TVITEM itemNew; POINT ptDrag; } NMTREEVIEW;
typedef struct NMTVDISPINFO { NMHDR hdr; TVITEM item; } NMTVDISPINFO;

/* ---- SAL-style annotations (IN/OUT/OPTIONAL) used in some signatures ------ */
/* NOTE: only the uppercase forms are defined. The double-underscore variants
 * (__in/__out/__inout/__opt) are deliberately NOT defined: libc++ itself uses
 * `__opt` as a parameter name inside <filesystem>, so an empty `#define __opt`
 * corrupts that system header ("expected expression"). Nothing in this tree
 * uses the __-prefixed annotations, so they are simply omitted. */
#ifndef IN
#define IN
#define OUT
#define OPTIONAL
#endif

/* ===========================================================================
 * PHASE 1 STUB CLOSURE -- Batch 2/3/4: remaining Win32 symbols so every TU
 * parses headlessly. Constants are #defines; windowsx-style control macros
 * expand to SendMessage (itself a hollow no-op); "functions" are hollow
 * no-ops. A real UI backend (SDL2/Qt/Cocoa) is still required for runtime.
 * =========================================================================== */

/* NOTE: HMODULE, HMENU, HICON, HCURSOR, HBRUSH, HFONT, HGDIOBJ, HACCEL,
 * HMONITOR, HIMAGELIST, COLORREF, BSTR, INT_PTR, UINT_PTR, ULONG_PTR,
 * PULARGE_INTEGER, LPCTSTR, LPTSTR, LPCWSTR, LPWSTR, REFCLSID, LPCOLESTR,
 * LPOLESTR, WNDPROC and DLGPROC are already defined by the baseline stub /
 * com_stub.h. Do NOT re-typedef them here (redefinition error). */

/* Types genuinely absent from the baseline stub / com_stub.h (referenced by
 * the hollow functions below and by the engine): */
#ifndef HMONITOR
typedef void *HMONITOR;
#endif
#ifndef WNDPROC
typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
#endif
#ifndef DLGPROC
typedef intptr_t (CALLBACK *DLGPROC)(HWND, UINT, WPARAM, LPARAM);
#endif
#ifndef PULARGE_INTEGER
typedef ULARGE_INTEGER *PULARGE_INTEGER;
#endif
#ifndef BSTR
typedef wchar_t *BSTR;
#endif

/* ---- Window / show / style constants ----------------------------------- */
#ifndef WM_APP
#define WM_APP 0x8000
#endif
#ifndef SW_NORMAL
#define SW_NORMAL 1
#endif
#ifndef SWP_NOZORDER
#define SWP_NOZORDER 0x0004
#endif
#ifndef SWP_NOMOVE
#define SWP_NOMOVE 0x0002
#endif
#ifndef SWP_NOSIZE
#define SWP_NOSIZE 0x0001
#endif
#ifndef SWP_NOACTIVATE
#define SWP_NOACTIVATE 0x0010
#endif
#ifndef HWND_DESKTOP
#define HWND_DESKTOP ((HWND)0)
#endif
#ifndef HWND_TOP
#define HWND_TOP ((HWND)0)
#endif
#ifndef HWND_TOPMOST
#define HWND_TOPMOST ((HWND)-1)
#endif
#ifndef HWND_BOTTOM
#define HWND_BOTTOM ((HWND)1)
#endif
#ifndef GWL_STYLE
#define GWL_STYLE (-16)
#endif
#ifndef GWL_EXSTYLE
#define GWL_EXSTYLE (-20)
#endif
#ifndef GWL_ID
#define GWL_ID (-12)
#endif
#ifndef GWLP_HWNDPARENT
#define GWLP_HWNDPARENT (-8)
#endif
#ifndef SIZE_MINIMIZED
#define SIZE_MINIMIZED 1
#endif
#ifndef HTCLIENT
#define HTCLIENT 1
#endif
#ifndef HTTRANSPARENT
#define HTTRANSPARENT (-1)
#endif
#ifndef CS_DBLCLKS
#define CS_DBLCLKS 0x0008
#endif
#ifndef SC_CLOSE
#define SC_CLOSE 0xF060
#endif
#ifndef SC_SCREENSAVE
#define SC_SCREENSAVE 0xF140
#endif
#ifndef WM_SHOWWINDOW
#define WM_SHOWWINDOW 0x0018
#endif
#ifndef WM_DISPLAYCHANGE
#define WM_DISPLAYCHANGE 0x007E
#endif

/* ---- Control message & notification constants --------------------------- */
#ifndef BST_CHECKED
#define BST_CHECKED 0x0001
#endif
#ifndef BST_UNCHECKED
#define BST_UNCHECKED 0x0000
#endif
#ifndef BM_GETCHECK
#define BM_GETCHECK 0x00F0
#endif
#ifndef BM_SETCHECK
#define BM_SETCHECK 0x00F1
#endif
#ifndef BN_CLICKED
#define BN_CLICKED 0
#endif
#ifndef LBN_SELCHANGE
#define LBN_SELCHANGE 1
#endif
#ifndef CBN_SELCHANGE
#define CBN_SELCHANGE 1
#endif
#ifndef EN_CHANGE
#define EN_CHANGE 0x0300
#endif
#ifndef EM_SETSEL
#define EM_SETSEL 0x00B1
#endif

/* Listbox messages */
#ifndef LB_ADDSTRING
#define LB_ADDSTRING 0x0180
#endif
#ifndef LB_INSERTSTRING
#define LB_INSERTSTRING 0x0181
#endif
#ifndef LB_DELETESTRING
#define LB_DELETESTRING 0x0182
#endif
#ifndef LB_GETCOUNT
#define LB_GETCOUNT 0x018B
#endif
#ifndef LB_GETCURSEL
#define LB_GETCURSEL 0x0188
#endif
#ifndef LB_GETTEXT
#define LB_GETTEXT 0x0189
#endif
#ifndef LB_GETTEXTLEN
#define LB_GETTEXTLEN 0x018A
#endif
#ifndef LB_SETCURSEL
#define LB_SETCURSEL 0x0186
#endif
#ifndef LB_GETITEMDATA
#define LB_GETITEMDATA 0x0199
#endif
#ifndef LB_GETSELCOUNT
#define LB_GETSELCOUNT 0x0190
#endif
#ifndef LB_GETSELITEMS
#define LB_GETSELITEMS 0x0191
#endif
#ifndef LB_GETITEMRECT
#define LB_GETITEMRECT 0x0198
#endif
#ifndef LB_SETITEMHEIGHT
#define LB_SETITEMHEIGHT 0x01A0
#endif
#ifndef LB_SETITEMDATA
#define LB_SETITEMDATA 0x019A
#endif
#ifndef LB_RESETCONTENT
#define LB_RESETCONTENT 0x0184
#endif
#ifndef LB_GETTOPINDEX
#define LB_GETTOPINDEX 0x018E
#endif
#ifndef LB_SETTOPINDEX
#define LB_SETTOPINDEX 0x0197
#endif
#ifndef LB_ERR
#define LB_ERR (-1)
#endif

/* Combobox messages */
#ifndef CB_ADDSTRING
#define CB_ADDSTRING 0x0143
#endif
#ifndef CB_INSERTSTRING
#define CB_INSERTSTRING 0x014A
#endif
#ifndef CB_DELETESTRING
#define CB_DELETESTRING 0x0144
#endif
#ifndef CB_GETCOUNT
#define CB_GETCOUNT 0x0146
#endif
#ifndef CB_GETCURSEL
#define CB_GETCURSEL 0x0147
#endif
#ifndef CB_GETLBTEXT
#define CB_GETLBTEXT 0x0148
#endif
#ifndef CB_GETLBTEXTLEN
#define CB_GETLBTEXTLEN 0x0149
#endif
#ifndef CB_SETCURSEL
#define CB_SETCURSEL 0x014E
#endif
#ifndef CB_GETITEMDATA
#define CB_GETITEMDATA 0x0150
#endif
#ifndef CB_SETITEMDATA
#define CB_SETITEMDATA 0x0151
#endif
#ifndef CB_GETTOPINDEX
#define CB_GETTOPINDEX 0x015B
#endif
#ifndef CB_SETTOPINDEX
#define CB_SETTOPINDEX 0x015C
#endif
#ifndef CB_ERR
#define CB_ERR (-1)
#endif

/* Edit / static messages */
#ifndef EM_SETLIMITTEXT
#define EM_SETLIMITTEXT 0x00C5
#endif
#ifndef STM_SETTEXT
#define STM_SETTEXT 0x000C
#endif

/* Scrollbar messages */
#ifndef SBM_SETPOS
#define SBM_SETPOS 0x00E0
#endif
#ifndef SBM_SETRANGE
#define SBM_SETRANGE 0x00E2
#endif
#ifndef SBM_SETSCROLLINFO
#define SBM_SETSCROLLINFO 0x00E9
#endif
#ifndef SBM_GETPOS
#define SBM_GETPOS 0x00E1
#endif
#ifndef SB_THUMBTRACK
#define SB_THUMBTRACK 4
#endif
#ifndef SIF_RANGE
#define SIF_RANGE 0x0001
#endif
#ifndef SIF_POS
#define SIF_POS 0x0002
#endif

/* Tree-view messages (commctrl) */
#ifndef TV_FIRST
#define TV_FIRST 0x1100
#endif
#ifndef TVM_GETINDENT
#define TVM_GETINDENT (TV_FIRST+6)
#endif
#ifndef TVM_SELECTITEM
#define TVM_SELECTITEM (TV_FIRST+11)
#endif
#ifndef TVM_GETNEXTITEM
#define TVM_GETNEXTITEM (TV_FIRST+10)
#endif
#ifndef TVM_GETITEMRECT
#define TVM_GETITEMRECT (TV_FIRST+14)
#endif
#ifndef TVM_CREATEDRAGIMAGE
#define TVM_CREATEDRAGIMAGE (TV_FIRST+33)
#endif
#ifndef TVGN_CARET
#define TVGN_CARET 0x9
#endif
#ifndef TVGN_DROPHILITE
#define TVGN_DROPHILITE 0x8
#endif
#ifndef TVGN_FIRSTVISIBLE
#define TVGN_FIRSTVISIBLE 0x5
#endif
#ifndef TVGN_NEXTVISIBLE
#define TVGN_NEXTVISIBLE 0x6
#endif
#ifndef TVGN_PREVIOUSVISIBLE
#define TVGN_PREVIOUSVISIBLE 0x7
#endif
#ifndef TVI_ROOT
#define TVI_ROOT ((HTREEITEM)0)
#endif

/* Hotkey messages */
#ifndef HKM_GETHOTKEY
#define HKM_GETHOTKEY 0x0401
#endif
#ifndef HKM_SETHOTKEY
#define HKM_SETHOTKEY 0x0402
#endif

/* ---- GDI / bitmap / font / text constants ------------------------------ */
#ifndef SRCCOPY
#define SRCCOPY 0x00CC0020
#endif
#ifndef COLORONCOLOR
#define COLORONCOLOR 3
#endif
#ifndef TRANSPARENT
#define TRANSPARENT 1
#endif
#ifndef DIB_RGB_COLORS
#define DIB_RGB_COLORS 0
#endif
#ifndef BI_RGB
#define BI_RGB 0
#endif
#ifndef BI_BITFIELDS
#define BI_BITFIELDS 3
#endif
#ifndef CLIP_DEFAULT_PRECIS
#define CLIP_DEFAULT_PRECIS 0
#endif
#ifndef PROOF_QUALITY
#define PROOF_QUALITY 2
#endif
#ifndef FF_SWISS
#define FF_SWISS 0x20
#endif
#ifndef DEFAULT_PITCH
#define DEFAULT_PITCH 0
#endif
#ifndef TA_CENTER
#define TA_CENTER 6
#endif

/* ---- File / console / system / misc constants --------------------------- */
#ifndef FILE_ATTRIBUTE_HIDDEN
#define FILE_ATTRIBUTE_HIDDEN 0x0002
#endif
#ifndef FILE_ATTRIBUTE_SYSTEM
#define FILE_ATTRIBUTE_SYSTEM 0x0004
#endif
#ifndef FILE_ATTRIBUTE_TEMPORARY
#define FILE_ATTRIBUTE_TEMPORARY 0x0100
#endif
#ifndef ERROR_ALREADY_EXISTS
#define ERROR_ALREADY_EXISTS 183
#endif
#ifndef STD_INPUT_HANDLE
#define STD_INPUT_HANDLE ((DWORD)-10)
#endif
#ifndef STD_OUTPUT_HANDLE
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#endif
#ifndef STD_ERROR_HANDLE
#define STD_ERROR_HANDLE ((DWORD)-12)
#endif
#ifndef SM_CXSCREEN
#define SM_CXSCREEN 0
#endif
#ifndef SM_CXDRAG
#define SM_CXDRAG 68
#endif
#ifndef SM_CYDRAG
#define SM_CYDRAG 69
#endif
#ifndef SM_SWAPBUTTON
#define SM_SWAPBUTTON 23
#endif
#ifndef VREFRESH
#define VREFRESH 116
#endif
#ifndef MUTEX_ALL_ACCESS
#define MUTEX_ALL_ACCESS 0x1F0001
#endif
#ifndef RT_DIALOG
#define RT_DIALOG MAKEINTRESOURCE(5)
#endif
#ifndef LANG_NEUTRAL
#define LANG_NEUTRAL 0x00
#endif
#ifndef SUBLANG_DEFAULT
#define SUBLANG_DEFAULT 0x01
#endif
#ifndef MB_ICONEXCLAMATION
#define MB_ICONEXCLAMATION 0x00000030L
#endif
#ifndef MB_ICONSTOP
#define MB_ICONSTOP 0x00000010L
#endif
#ifndef MB_OKCANCEL
#define MB_OKCANCEL 0x00000001L
#endif
#ifndef MB_YESNO
#define MB_YESNO 0x00000004L
#endif
#ifndef MB_PRECOMPOSED
#define MB_PRECOMPOSED 0x00000020L
#endif
#ifndef MB_SETFOREGROUND
#define MB_SETFOREGROUND 0x00010000L
#endif
#ifndef MB_TOPMOST
#define MB_TOPMOST 0x00040000L
#endif
#ifndef RDW_INVALIDATE
#define RDW_INVALIDATE 0x0001
#endif
#ifndef RDW_UPDATENOW
#define RDW_UPDATENOW 0x0100
#endif
#ifndef RDW_ERASE
#define RDW_ERASE 0x0004
#endif
#ifndef RDW_ALLCHILDREN
#define RDW_ALLCHILDREN 0x0080
#endif
#ifndef TRACKBAR_CLASS
#define TRACKBAR_CLASS TEXT("msctls_trackbar32")
#endif

/* ---- Function-like macros ---------------------------------------------- */
#ifndef MAKEINTRESOURCE
#define MAKEINTRESOURCE(i) ((LPTSTR)((ULONG_PTR)((WORD)(i))))
#endif
#ifndef MAKEWPARAM
#define MAKEWPARAM(l, h) ((WPARAM)(((WORD)(l)) | ((WORD)(h) << 16)))
#endif
#ifndef TEXT
#define TEXT(x) x
#endif
#ifndef IS_SURROGATE_PAIR
#define IS_SURROGATE_PAIR(w1, w2) (((w1) >= 0xD800 && (w1) <= 0xDBFF) && ((w2) >= 0xDC00 && (w2) <= 0xDFFF))
#endif

/* ---- windowsx-style control macros (expand to SendMessage) -------------- */
#ifndef ListBox_AddString
#define ListBox_AddString(hwnd, txt) ((int)(SendMessage((hwnd), LB_ADDSTRING, 0, (LPARAM)(txt))))
#endif
#ifndef ListBox_InsertString
#define ListBox_InsertString(hwnd, i, txt) ((int)(SendMessage((hwnd), LB_INSERTSTRING, (WPARAM)(i), (LPARAM)(txt))))
#endif
#ifndef ListBox_DeleteString
#define ListBox_DeleteString(hwnd, i) ((int)(SendMessage((hwnd), LB_DELETESTRING, (WPARAM)(i), 0)))
#endif
#ifndef ListBox_GetCount
#define ListBox_GetCount(hwnd) ((int)(SendMessage((hwnd), LB_GETCOUNT, 0, 0)))
#endif
#ifndef ListBox_GetCurSel
#define ListBox_GetCurSel(hwnd) ((int)(SendMessage((hwnd), LB_GETCURSEL, 0, 0)))
#endif
#ifndef ListBox_SetCurSel
#define ListBox_SetCurSel(hwnd, i) ((int)(SendMessage((hwnd), LB_SETCURSEL, (WPARAM)(i), 0)))
#endif
#ifndef ListBox_GetItemData
#define ListBox_GetItemData(hwnd, i) (SendMessage((hwnd), LB_GETITEMDATA, (WPARAM)(i), 0))
#endif
#ifndef ListBox_SetItemData
#define ListBox_SetItemData(hwnd, i, d) ((int)(SendMessage((hwnd), LB_SETITEMDATA, (WPARAM)(i), (LPARAM)(d))))
#endif
#ifndef ListBox_GetTopIndex
#define ListBox_GetTopIndex(hwnd) ((int)(SendMessage((hwnd), LB_GETTOPINDEX, 0, 0)))
#endif
#ifndef ListBox_SetTopIndex
#define ListBox_SetTopIndex(hwnd, i) ((int)(SendMessage((hwnd), LB_SETTOPINDEX, (WPARAM)(i), 0)))
#endif
#ifndef ListBox_ResetContent
#define ListBox_ResetContent(hwnd) ((int)(SendMessage((hwnd), LB_RESETCONTENT, 0, 0)))
#endif
#ifndef ComboBox_GetCurSel
#define ComboBox_GetCurSel(hwnd) ((int)(SendMessage((hwnd), CB_GETCURSEL, 0, 0)))
#endif
#ifndef ComboBox_GetLBText
#define ComboBox_GetLBText(hwnd, i, buf) ((int)(SendMessage((hwnd), CB_GETLBTEXT, (WPARAM)(i), (LPARAM)(buf))))
#endif
#ifndef Button_GetCheck
#define Button_GetCheck(hwnd) ((int)(SendMessage((hwnd), BM_GETCHECK, 0, 0)))
#endif
#ifndef Button_SetCheck
#define Button_SetCheck(hwnd, c) ((int)(SendMessage((hwnd), BM_SETCHECK, (WPARAM)(c), 0)))
#endif
#ifndef Static_SetText
#define Static_SetText(hwnd, txt) ((int)(SendMessage((hwnd), STM_SETTEXT, 0, (LPARAM)(txt))))
#endif
#ifndef Edit_SetSel
#define Edit_SetSel(hwnd, s, e) ((int)(SendMessage((hwnd), EM_SETSEL, (WPARAM)(s), (LPARAM)(e))))
#endif
#ifndef HKM_GETHOTKEY
#define HKM_GETHOTKEY(hwnd) ((int)(SendMessage((hwnd), HKM_GETHOTKEY, 0, 0)))
#endif
#ifndef TreeView_SelectItem
#define TreeView_SelectItem(hwnd, item) ((int)(SendMessage((hwnd), TVM_SELECTITEM, TVGN_CARET, (LPARAM)(item))))
#endif
#ifndef TreeView_CreateDragImage
#define TreeView_CreateDragImage(hwnd, item) ((HIMAGELIST)(SendMessage((hwnd), TVM_CREATEDRAGIMAGE, 0, (LPARAM)(item))))
#endif
#ifndef TreeView_GetItemRect
#define TreeView_GetItemRect(hwnd, item, prc, ...) ((int)(SendMessage((hwnd), TVM_GETITEMRECT, (WPARAM)(item), (LPARAM)(prc))))
#endif
#ifndef TreeView_GetIndent
#define TreeView_GetIndent(hwnd) ((int)(SendMessage((hwnd), TVM_GETINDENT, 0, 0)))
#endif
#ifndef TreeView_GetFirstVisible
#define TreeView_GetFirstVisible(hwnd) ((HTREEITEM)(SendMessage((hwnd), TVM_GETNEXTITEM, TVGN_FIRSTVISIBLE, 0)))
#endif
#ifndef TreeView_GetPrevVisible
#define TreeView_GetPrevVisible(hwnd, item) ((HTREEITEM)(SendMessage((hwnd), TVM_GETNEXTITEM, TVGN_PREVIOUSVISIBLE, (LPARAM)(item))))
#endif
#ifndef TreeView_GetNextVisible
#define TreeView_GetNextVisible(hwnd, item) ((HTREEITEM)(SendMessage((hwnd), TVM_GETNEXTITEM, TVGN_NEXTVISIBLE, (LPARAM)(item))))
#endif
#ifndef TreeView_SelectDropTarget
#define TreeView_SelectDropTarget(hwnd, item) ((int)(SendMessage((hwnd), TVM_SELECTITEM, TVGN_DROPHILITE, (LPARAM)(item))))
#endif
#ifndef TreeView_SelectSetFirstVisible
#define TreeView_SelectSetFirstVisible(hwnd, item) ((int)(SendMessage((hwnd), TVM_SELECTITEM, TVGN_FIRSTVISIBLE, (LPARAM)(item))))
#endif

/* ---- Hollow no-op "functions" (parse only; real UI backend needed) ------ */
inline HWND    SetFocus(HWND h) { return (HWND)opents_win_set_focus((void *)h); }
inline BOOL    SetWindowText(HWND h, LPCTSTR t) { opents_win_set_text((void *)h, t); return TRUE; }
inline BOOL    SetDlgItemText(HWND h, int id, LPCTSTR t) {
	OpentsWin child = opents_win_find_child((void *)h, (unsigned)id);
	opents_win_set_text(child, t);
	return (child != nullptr) ? TRUE : FALSE;
}
inline LRESULT SendDlgItemMessage(HWND h, int id, UINT m, WPARAM w, LPARAM l) {
	return (LRESULT)opents_win_send(opents_win_find_child((void *)h, (unsigned)id),
	                                (unsigned)m, (uintptr_t)w, (intptr_t)l);
}
inline BOOL    SetForegroundWindow(HWND h) { (void)h; return TRUE; }
inline BOOL    DestroyWindow(HWND h) { opents_win_destroy((void *)h); return TRUE; }
inline BOOL    MoveWindow(HWND h, int x, int y, int w, int hgt, BOOL b) {
	opents_win_set_rect((void *)h, x, y, w, hgt);
	if (b) opents_win_set_visible((void *)h, 1);
	return TRUE;
}
inline BOOL    IsWindow(HWND h) { return opents_win_is_valid((void *)h) ? TRUE : FALSE; }
inline BOOL    IsWindowEnabled(HWND h) { return opents_win_is_enabled((void *)h) ? TRUE : FALSE; }
inline BOOL    IsChild(HWND p, HWND c) { return (opents_win_get_parent((void *)c) == (void *)p) ? TRUE : FALSE; }
inline BOOL    PtInRect(const RECT *r, POINT p) {
	/* Was an unconditional FALSE, and the mouse hit test is built on it:
	** Child_From_Logical_Point drops every candidate whose rectangle does
	** not contain the point, so answering "no" for all of them meant no
	** control could ever own a click. Win32 excludes the right and bottom
	** edges. */
	return (r != nullptr && p.x >= r->left && p.x < r->right &&
	        p.y >= r->top && p.y < r->bottom) ? TRUE : FALSE;
}
inline void    SetRect(RECT *r, int l, int t, int ri, int b) { if(r){r->left=l;r->top=t;r->right=ri;r->bottom=b;} }
inline int     MapWindowPoints(HWND f, HWND t, POINT *p, UINT n) {
	/* Was a no-op. Both spaces are the logical desktop -- a window's
	** rectangle is its parent chain summed in frame units, anchored at the
	** main window's client origin -- so converting between two of them is
	** the difference of their client origins, with HWND_DESKTOP (NULL) at
	** zero. Without it the hit test compared a control's on-screen
	** rectangle against a frame position and never matched, and a
	** re-targeted mouse message kept the coordinates of the window it was
	** routed away from. */
	if (p == nullptr || n == 0) {
		return 0;
	}
	int fx = 0, fy = 0, tx = 0, ty = 0;
	if (f != nullptr) {
		int fw = 0, fh = 0;
		Opents_Logical_Screen_Rect(f, &fx, &fy, &fw, &fh);
	}
	if (t != nullptr) {
		int tw = 0, th = 0;
		Opents_Logical_Screen_Rect(t, &tx, &ty, &tw, &th);
	}
	for (UINT i = 0; i < n; i++) {
		p[i].x += fx - tx;
		p[i].y += fy - ty;
	}
	return 0;
}
inline HWND    GetTopWindow(HWND h) { return (HWND)opents_win_get_window((void *)h, 5 /* GW_CHILD */); }
inline void   *GetMenu(HWND h) { (void)h; return nullptr; }
inline HWND    ChildWindowFromPoint(HWND h, POINT p) {
	/* Was an unconditional NULL. Same walk Win32 does, topmost first; the
	** point is in h's client space and each candidate's rectangle is in the
	** shared desktop space, so the point is lifted into that space once.
	** Win32 answers with the parent when the point misses every child. */
	if (h == nullptr) {
		return nullptr;
	}
	int ox = 0, oy = 0, ow = 0, oh = 0;
	Opents_Logical_Screen_Rect(h, &ox, &oy, &ow, &oh);
	const int x = p.x + ox;
	const int y = p.y + oy;
	for (HWND c = (HWND)opents_win_get_window((void *)h, 5 /* GW_CHILD */);
	     c != nullptr; c = (HWND)opents_win_get_window((void *)c, 2 /* GW_HWNDNEXT */)) {
		if (!opents_win_is_visible((void *)c)) {
			continue;
		}
		int cx = 0, cy = 0, cw = 0, ch = 0;
		Opents_Logical_Screen_Rect(c, &cx, &cy, &cw, &ch);
		if (x >= cx && x < cx + cw && y >= cy && y < cy + ch) {
			return c;
		}
	}
	return h;
}
inline BOOL    EnumChildWindows(HWND h, WNDENUMPROC e, LPARAM l) {
	// WNDENUMPROC and OpentsEnumProc are the same ABI spelled with different
	// width types; both are (HWND, LPARAM) -> int.
	if (e == nullptr) return FALSE;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type"
	return opents_win_enum_children((void *)h, (OpentsEnumProc)e, (intptr_t)l) ? TRUE : FALSE;
#pragma clang diagnostic pop
}
inline BOOL    SetCursorPos(int x, int y) { opents_window_set_cursor_pos(x, y); return TRUE; }
inline HMONITOR MonitorFromWindow(HWND h, DWORD f) { (void)h; (void)f; return nullptr; }
inline SHORT   GetAsyncKeyState(int v) { return opents_input_get_async_key_state(v); }
inline int     ToUnicode(UINT wVirtKey, UINT /*wScanCode*/, const BYTE *lpKeyState, LPWSTR pwszBuff, int cchBuff, UINT /*wFlags*/) {
	/* The scan code is not consulted: the US layout is a function of the
	 * virtual key, so the table above is the whole of it. */
	if (pwszBuff == nullptr || cchBuff <= 0) {
		return 0;
	}

	const bool shift = (lpKeyState != nullptr) && ((lpKeyState[0x10] & 0x80) != 0);  /* VK_SHIFT   */
	const bool ctrl  = (lpKeyState != nullptr) && ((lpKeyState[0x11] & 0x80) != 0);  /* VK_CONTROL */
	const bool caps  = (lpKeyState != nullptr) && ((lpKeyState[0x14] & 0x01) != 0);  /* VK_CAPITAL */

	/*
	**	Control turns a letter into its control code -- the engine relies on
	**	this for its Ctrl+A..Ctrl+Z shortcuts, which are delivered as
	**	characters rather than as key codes.
	*/
	if (ctrl && wVirtKey >= 0x41 && wVirtKey <= 0x5A) {
		pwszBuff[0] = (wchar_t)(wVirtKey - 0x40);
		return 1;
	}

	const wchar_t ch = opents_vk_to_ascii(wVirtKey, shift, caps);
	if (ch == 0) {
		return 0;
	}

	pwszBuff[0] = ch;
	return 1;
}
inline BOOL    BringWindowToTop(HWND h) { (void)h; return TRUE; }
inline int     RegisterClass(const void *pcls) {
	// Remember the engine's window procedure so DispatchMessage can call it.
	const WNDCLASS * wc = (const WNDCLASS *)pcls;
	if (wc != nullptr && wc->lpfnWndProc != nullptr) {
		// Keep the procedure with the class, the way Win32 does, so that a later
		// registration cannot steal messages meant for an earlier class. The engine
		// relies on this: it registers its main window class and then a second one,
		// "ComboDropWin", for combo-box drop-downs.
		opents_win_register_class(wc->lpszClassName, (void *)wc->lpfnWndProc);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type"
		opents_set_wndproc((OpentsWndProc)wc->lpfnWndProc);
#pragma clang diagnostic pop
	}
	return 1;
}
/* Loads a module for real.
 *
 * The name has to be translated. The engine asks for "Language.dll"; CMake
 * builds the same sources as libLanguage.dylib beside the executable. A
 * LoadLibrary that always returns null is not a stub the engine tolerates:
 * Init_Language_Resources treats failure as an unrecoverable missing install
 * and WinMain returns before the game ever starts. */
inline HINSTANCE LoadLibrary(LPCTSTR n) {
	if (n == nullptr || *n == '\0') return nullptr;

	/* A resource module is not a library to load but a PE image to read. The game
	 * asks for "Language.dll" and means its dialogs and string tables, and CMake
	 * also builds libLanguage.dylib from the same sources -- which carries none of
	 * them, since no resource compiler ran. So the image is preferred whenever the
	 * file is there, and the dylib stays as the fallback for a data set that has
	 * no Language.dll to read. */
	void *resources = opents_resource_open_module(n);
	if (resources != nullptr) return (HINSTANCE)resources;

	/* As given first, so an explicit path wins over any translation. */
	void *handle = dlopen(n, RTLD_NOW);
	if (handle != nullptr) return reinterpret_cast<HINSTANCE>(handle);

	std::string leaf(n);
	size_t const slash = leaf.find_last_of("/\\");
	if (slash != std::string::npos) leaf = leaf.substr(slash + 1);
	size_t const dot = leaf.rfind('.');
	if (dot != std::string::npos) leaf = leaf.substr(0, dot);
	if (leaf.empty()) return nullptr;

	std::vector<std::string> candidates;

	char path_to_exe[4096];
	if (GetModuleFileNameA(nullptr, path_to_exe, sizeof(path_to_exe)) != 0) {
		std::string directory(path_to_exe);
		size_t const cut = directory.find_last_of('/');
		if (cut != std::string::npos) {
			directory = directory.substr(0, cut + 1);
			candidates.push_back(directory + "lib" + leaf + ".dylib");
			candidates.push_back(directory + "lib" + leaf + ".so");
			candidates.push_back(directory + leaf + ".dylib");
			candidates.push_back(directory + leaf);
		}
	}

	candidates.push_back("lib" + leaf + ".dylib");
	candidates.push_back("lib" + leaf + ".so");

	for (std::string const &candidate : candidates) {
		handle = dlopen(candidate.c_str(), RTLD_NOW);
		if (handle != nullptr) return reinterpret_cast<HINSTANCE>(handle);
	}

	return nullptr;
}
inline BOOL    FreeLibrary(HINSTANCE h) {
	if (h == nullptr) return FALSE;
	if (opents_resource_is_module((void *)h) != 0) {
		opents_resource_close_module((void *)h);
		return TRUE;
	}
	return dlclose(reinterpret_cast<void *>(h)) == 0 ? TRUE : FALSE;
}
inline FARPROC GetProcAddress(HINSTANCE h, const char *n) {
	if (h == nullptr || n == nullptr) return nullptr;
	/* A resource module was never dlopen'd, so dlsym on its token would fault.
	 * It has no exports either, so reporting none is both safe and true. */
	if (opents_resource_is_module((void *)h) != 0) return nullptr;
	return reinterpret_cast<FARPROC>(dlsym(reinterpret_cast<void *>(h), n));
}
/* Releases what CommandLineToArgvW allocated. Every caller frees exactly that
 * block and nothing else, so this is free() rather than a no-op. */
inline HLOCAL  LocalFree(HLOCAL h) { if (h != nullptr) free(static_cast<void *>(h)); return nullptr; }
/* LoadResource and LockResource are separate calls on Windows because a resource
 * is paged in on demand. Here the whole file is already resident, so LoadResource
 * only has to confirm that the handle points at real bytes and LockResource hands
 * them over. Fetch_Resource checks both for null, so neither may report success
 * without data behind it. */
inline void   *LoadResource(HINSTANCE h, HRSRC r) {
	(void)h;
	if (r == nullptr) return nullptr;
	return (opents_resource_lock((void *)r, nullptr) != nullptr) ? (void *)r : nullptr;
}
inline LPVOID  LockResource(void *h) { return (LPVOID)opents_resource_lock(h, nullptr); }
/* Nothing measured a resource before, because no dialog could be fetched. It is
 * here so a caller that asks how large one is gets the real answer. */
inline DWORD   SizeofResource(HINSTANCE h, HRSRC r) { (void)h; return (DWORD)opents_resource_size((void *)r); }
inline int     GetObject(HANDLE h, int c, LPVOID p) { return opents_gdi_get_object((void *)h, c, p); }
inline HDC     CreateCompatibleDC(HDC h) { (void)h; return (HDC)opents_gdi_create_dc(); }
inline BOOL    DeleteDC(HDC h) { opents_gdi_delete_dc((void *)h); return TRUE; }
inline HBITMAP CreateBitmap(int w, int h, UINT n, UINT b, const void *p) { return (HBITMAP)opents_gdi_create_bitmap(w, h, (int)n, (int)b, p); }
inline HBRUSH  CreateSolidBrush(COLORREF c) { (void)c; return nullptr; }
/* Builds a cursor object from the engine's color bitmap (alpha carries
 * transparency; the mask is all zero) and remembers the hotspot. The actual
 * pointer image is applied later, by SetCursor. */
inline HICON   CreateIconIndirect(const ICONINFO * info) {
	if (info == nullptr) {
		return nullptr;
	}
	return (HICON)opents_cursor_create(info->hbmColor, (int)info->xHotspot, (int)info->yHotspot);
}
inline BOOL    GetTextExtentPoint32(HDC h, LPCTSTR s, int n, LPSIZE p) { (void)h;(void)s;(void)n;(void)p; return TRUE; }
inline BOOL    TextOut(HDC h, int x, int y, LPCTSTR s, int n) { (void)h;(void)x;(void)y;(void)s;(void)n; return TRUE; }
inline BOOL    GdiFlush(void) { return TRUE; }
inline BOOL    DeleteObject(HANDLE h) { opents_gdi_delete_object((void *)h); return TRUE; }
inline HWND    CreateDialogIndirectParam(HINSTANCE h, LPCDLGTEMPLATE t, HWND w, DLGPROC p, LPARAM l) {
	// The resource layer has already located the template; all that is left is
	// to turn it into windows. The engine's dialog layer paints into its own
	// surfaces, so these are logical windows: a frame plus one child per item,
	// carrying the id, class name, geometry and per-window long words the
	// owner-draw code subclasses and drives. Size comes from the resource
	// layer, which reports it alongside the bytes.
	(void)h; (void)l;
	if (t == nullptr) return nullptr;
	unsigned long size = opents_resource_size_of(t);
	return (HWND)opents_win_create_dialog(t, (unsigned)size, (void *)w, (void *)p);
}
inline int     GetClassName(HWND h, LPTSTR b, int n) {
	if (opents_win_is_main((void *)h)) {
		return opents_win_get_main_class(b, (unsigned)(n > 0 ? n : 0));
	}
	return opents_win_get_class_name((void *)h, b, (unsigned)(n > 0 ? n : 0));
}
/* The command line main() received, widened. Widening rather than returning
 * null matters: every caller treats a null command line as "no arguments",
 * which is also how the -X options that ask for a debug console are read. */
inline LPWSTR  GetCommandLineW(void) {
	static std::wstring cached;

	/* Keyed on the source pointer rather than computed once, so that a call
	 * made before main runs cannot freeze an empty answer for the rest of the
	 * process. */
	static char const *cached_from = nullptr;
	char const *narrow = opents_port_command_line();

	if (narrow != cached_from) {
		cached.clear();
		if (narrow != nullptr) {
			/* One wchar_t per byte: a widened copy of the same ASCII text. */
			cached.assign(narrow, narrow + strlen(narrow));
		}
		cached_from = narrow;
	}

	/* The engine only reads it. */
	return const_cast<LPWSTR>(cached.c_str());
}
/* Real free space. Returning success without filling anything in reports 0 MB,
 * and startup.cpp treats low free space as a reason to ask the player whether
 * to continue — a question this target cannot ask, and one it would be asking
 * for no reason. */
inline BOOL    GetDiskFreeSpaceEx(LPCTSTR d, PULARGE_INTEGER f, PULARGE_INTEGER t, PULARGE_INTEGER a) {
	struct statvfs usage;
	if (statvfs(d != nullptr && *d != '\0' ? d : ".", &usage) != 0) return FALSE;

	unsigned long long const block = usage.f_frsize != 0 ? usage.f_frsize
												: (usage.f_bsize != 0 ? usage.f_bsize : 512ULL);

	if (f != nullptr) f->QuadPart = static_cast<ULONGLONG>(usage.f_bavail) * block;
	if (t != nullptr) t->QuadPart = static_cast<ULONGLONG>(usage.f_blocks) * block;
	if (a != nullptr) a->QuadPart = static_cast<ULONGLONG>(usage.f_bfree)  * block;

	return TRUE;
}
/* Parse a "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}" GUID string. This is what
 * turns the RULES.INI "Locomotor={...}" values into the engine's locomotor
 * CLSIDs. A stub that returned success without filling the output left the
 * caller's CLSID uninitialized, so every unit came back with a garbage
 * (or NULL) locomotor and faulted on first use. */
inline int _opents_guid_hex(wchar_t c) {
	if (c >= L'0' && c <= L'9') return (int)(c - L'0');
	if (c >= L'a' && c <= L'f') return (int)(c - L'a') + 10;
	if (c >= L'A' && c <= L'F') return (int)(c - L'A') + 10;
	return -1;
}
inline HRESULT CLSIDFromString(LPCOLESTR s, CLSID *p) {
	if (s == NULL || p == NULL) {
		return E_INVALIDARG;
	}
	while (*s == L'{' || *s == L' ' || *s == L'\t') ++s;
	unsigned int pair[16];
	int count = 0;
	int hi = -1;
	for (; *s != 0 && count < 16; ++s) {
		if (*s == L'-' || *s == L'}' || *s == L' ' || *s == L'\t') continue;
		int d = _opents_guid_hex(*s);
		if (d < 0) break;
		if (hi < 0) {
			hi = d;
		} else {
			pair[count++] = (unsigned int)((hi << 4) | d);
			hi = -1;
		}
	}
	if (count < 16) {
		return E_INVALIDARG;
	}
	p->Data1 = ((unsigned long)pair[0] << 24) | (pair[1] << 16) | (pair[2] << 8) | pair[3];
	p->Data2 = (unsigned short)((pair[4] << 8) | pair[5]);
	p->Data3 = (unsigned short)((pair[6] << 8) | pair[7]);
	for (int i = 0; i < 8; ++i) {
		p->Data4[i] = (unsigned char)pair[8 + i];
	}
	return S_OK;
}
inline HRESULT StringFromCLSID(REFGUID s, LPOLESTR *p) {
	if (p == NULL) {
		return E_INVALIDARG;
	}
	wchar_t *out = new wchar_t[40];
	swprintf(out, 40, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		(unsigned long)s.Data1, (unsigned)s.Data2, (unsigned)s.Data3,
		s.Data4[0], s.Data4[1], s.Data4[2], s.Data4[3],
		s.Data4[4], s.Data4[5], s.Data4[6], s.Data4[7]);
	*p = out;
	return S_OK;
}
inline void    SysFreeString(BSTR b) { (void)b; }
inline int     MessageBoxIndirect(const MSGBOXPARAMS *p) { (void)p; return 0; }
inline LONG_PTR GetWindowLongPtrA(HWND h, int i) { return GetWindowLongPtr(h, i); }
inline LONG_PTR SetWindowLongPtrA(HWND h, int i, LONG_PTR v) { return SetWindowLongPtr(h, i, v); }
inline BOOL    PostMessageA(HWND h, UINT m, WPARAM w, LPARAM l) {
	OpentsMsg msg;
	msg.hwnd = (void *)h;
	msg.message = (unsigned)m;
	msg.wParam = (uintptr_t)w;
	msg.lParam = (intptr_t)l;
	opents_msg_push(msg);
	return TRUE;
}
inline void    InitCommonControls(void) {}
inline BOOL    AllocConsole(void) { return TRUE; }
inline BOOL    SetConsoleTitle(LPCTSTR t) { (void)t; return TRUE; }
inline BOOL    SetConsoleOutputCP(UINT c) { (void)c; return TRUE; }
inline BOOL    SetConsoleCP(UINT c) { (void)c; return TRUE; }
inline BOOL    GetConsoleScreenBufferInfo(HANDLE h, CONSOLE_SCREEN_BUFFER_INFO *i) { (void)h; (void)i; return TRUE; }
inline BOOL    SetConsoleScreenBufferSize(HANDLE h, COORD c) { (void)h; (void)c; return TRUE; }
inline LONG    CompareFileTime(const FILETIME *a, const FILETIME *b) { (void)a;(void)b; return 0; }
inline BOOL    SystemTimeToFileTime(const SYSTEMTIME *s, FILETIME *f) { (void)s; (void)f; return TRUE; }
inline void    GetSystemTime(SYSTEMTIME *s) {
	if (s == nullptr) return;

	time_t now = ::time(nullptr);
	struct tm parts;
	gmtime_r(&now, &parts);

	s->wYear         = static_cast<WORD>(parts.tm_year + 1900);
	s->wMonth        = static_cast<WORD>(parts.tm_mon + 1);
	s->wDayOfWeek    = static_cast<WORD>(parts.tm_wday);
	s->wDay          = static_cast<WORD>(parts.tm_mday);
	s->wHour         = static_cast<WORD>(parts.tm_hour);
	s->wMinute       = static_cast<WORD>(parts.tm_min);
	s->wSecond       = static_cast<WORD>(parts.tm_sec);
	s->wMilliseconds = 0;
}
inline BOOL    CopyFile(LPCTSTR a, LPCTSTR b, BOOL f) { (void)a;(void)b;(void)f; return TRUE; }
inline int     freopen_s(FILE **p, const char *n, const char *m, FILE *s) { (void)p;(void)n;(void)m;(void)s; return 0; }
inline BOOL    ImageList_BeginDrag(HIMAGELIST h, int i, int x, int y) { (void)h;(void)i;(void)x;(void)y; return FALSE; }
inline BOOL    ImageList_DragEnter(HWND w, int x, int y) { (void)w;(void)x;(void)y; return FALSE; }
inline void    ImageList_DragShowNolock(BOOL b) { (void)b; }
inline BOOL    ImageList_EndDrag(void) { return FALSE; }
inline BOOL    ImageList_Destroy(HIMAGELIST h) { (void)h; return FALSE; }
/* FindFirstFile/FindNextFile map to the baseline A-versions (already defined) */
#ifndef FindFirstFile
#define FindFirstFile FindFirstFileA
#endif
#ifndef FindNextFile
#define FindNextFile FindNextFileA
#endif

/* ===========================================================================
 * PHASE 1 STUB CLOSURE -- Batch 5: second-layer Win32 symbols.
 * =========================================================================== */

/* ---- missing types ------------------------------------------------------ */
#ifndef HKEY
typedef void *HKEY;
#endif
#ifndef TEXTMETRIC
typedef struct TEXTMETRIC {
	LONG tmHeight, tmAscent, tmDescent, tmInternalLeading, tmExternalLeading, tmAveCharWidth,
	     tmMaxCharWidth, tmWeight, tmOverhang, tmDigitizedAspectX, tmDigitizedAspectY;
	char tmFirstChar, tmLastChar, tmDefaultChar, tmBreakChar;
	BYTE tmItalic, tmUnderlined, tmStruckOut, tmPitchAndFamily, tmCharSet;
} TEXTMETRIC;
#endif
#ifndef TVITEMA
typedef TVITEM TVITEMA;
#endif
#ifndef RTL_OSVERSIONINFOW
typedef struct RTL_OSVERSIONINFOW {
	DWORD dwOSVersionInfoSize, dwMajorVersion, dwMinorVersion, dwBuildNumber, dwPlatformId;
	wchar_t szCSDVersion[128];
} RTL_OSVERSIONINFOW, *PRTL_OSVERSIONINFOW;
#endif

/* ---- colour / control notification constants ---------------------------- */
#ifndef WM_CTLCOLORMSGBOX
#define WM_CTLCOLORMSGBOX 0x0132
#endif
#ifndef WM_CTLCOLOREDIT
#define WM_CTLCOLOREDIT 0x0133
#endif
#ifndef WM_CTLCOLORLISTBOX
#define WM_CTLCOLORLISTBOX 0x0134
#endif
#ifndef WM_CTLCOLORBTN
#define WM_CTLCOLORBTN 0x0135
#endif
#ifndef WM_CTLCOLORDLG
#define WM_CTLCOLORDLG 0x0136
#endif
#ifndef WM_CTLCOLORSCROLLBAR
#define WM_CTLCOLORSCROLLBAR 0x0137
#endif
#ifndef WM_CTLCOLORSTATIC
#define WM_CTLCOLORSTATIC 0x0138
#endif
#ifndef BLACK_BRUSH
#define BLACK_BRUSH 4
#endif
#ifndef LANG_USER_DEFAULT
#define LANG_USER_DEFAULT ((DWORD)0x0400)
#endif
#ifndef LBS_MULTIPLESEL
#define LBS_MULTIPLESEL 0x0008
#endif
#ifndef LBS_NOSEL
#define LBS_NOSEL 0x4000
#endif
#ifndef CB_SHOWDROPDOWN
#define CB_SHOWDROPDOWN 0x014F
#endif
#ifndef CB_RESETCONTENT
#define CB_RESETCONTENT 0x014B
#endif
#ifndef LB_GETITEMHEIGHT
/*
 * 0x01A1, NOT 0x018E. 0x018E is LB_GETTOPINDEX, and this file used to define
 * LB_GETITEMHEIGHT as that same value, so every caller that asked a list box for
 * its item height was really asking for its top index and got 0 back. The cost
 * was not just a wrong number: Ownerdraw's LB_GETITEMRECT divides the client
 * height by the item height, so the height of 0 also made the row rects
 * zero-height and made every row past the first fail with -1 -- the campaign
 * picker drew an empty box, and the mouse hit test (which divides the click's y
 * by the same height) could only ever select the top row.
 */
#define LB_GETITEMHEIGHT 0x01A1
#endif
#ifndef CB_GETITEMHEIGHT
#define CB_GETITEMHEIGHT 0x0154
#endif
#ifndef CB_FINDSTRING
#define CB_FINDSTRING 0x014C
#endif
#ifndef CB_GETDROPPEDSTATE
#define CB_GETDROPPEDSTATE 0x0157
#endif
#ifndef CB_GETDROPPEDCONTROLRECT
#define CB_GETDROPPEDCONTROLRECT 0x0152
#endif
#ifndef BS_OWNERDRAW
#define BS_OWNERDRAW 0x0000000BL
#endif
#ifndef BS_AUTOCHECKBOX
#define BS_AUTOCHECKBOX 0x00000003L
#endif
#ifndef SM_CYSCREEN
#define SM_CYSCREEN 1
#endif
#ifndef MF_BYCOMMAND
#define MF_BYCOMMAND 0x00000000L
#endif
#ifndef EN_SETFOCUS
#define EN_SETFOCUS 0x0100
#endif
#ifndef EN_KILLFOCUS
#define EN_KILLFOCUS 0x0200
#endif
#ifndef EN_MAXTEXT
#define EN_MAXTEXT 0x0501
#endif
#ifndef TIME_NOMINUTESORSECONDS
#define TIME_NOMINUTESORSECONDS 0x00000001L
#endif
#ifndef TIME_NOSECONDS
#define TIME_NOSECONDS 0x00000002L
#endif
#ifndef HKEY_LOCAL_MACHINE
#define HKEY_LOCAL_MACHINE ((HKEY)0x80000002)
#endif
#ifndef KEY_READ
#define KEY_READ 0x20019
#endif
#ifndef LBN_DBLCLK
#define LBN_DBLCLK 2
#endif
#ifndef LB_GETSEL
#define LB_GETSEL 0x0187
#endif
#ifndef LB_SETSEL
#define LB_SETSEL 0x0185
#endif
#ifndef LB_FINDSTRINGEXACT
#define LB_FINDSTRINGEXACT 0x01A2
#endif
#ifndef PROGRESS_CLASS
#define PROGRESS_CLASS TEXT("msctls_progress32")
#endif
#ifndef WC_TABCONTROL
#define WC_TABCONTROL TEXT("SysTabControl32")
#endif
#ifndef MB_ICONQUESTION
#define MB_ICONQUESTION 0x00000020L
#endif
#ifndef GM_ADVANCED
#define GM_ADVANCED 2
#endif
#ifndef MWT_IDENTITY
#define MWT_IDENTITY 1
#endif
#ifndef IDC_ARROW
#define IDC_ARROW ((LPTSTR)32512)
#endif
#ifndef IDC_NO
#define IDC_NO ((LPTSTR)32648)
#endif
#ifndef IDI_APPLICATION
#define IDI_APPLICATION ((LPTSTR)32512)
#endif
#ifndef TVIF_TEXT
#define TVIF_TEXT 0x0001
#endif
#ifndef TVIF_IMAGE
#define TVIF_IMAGE 0x0002
#endif
#ifndef TVIF_HANDLE
#define TVIF_HANDLE 0x0010
#endif
#ifndef TVIF_SELECTEDIMAGE
#define TVIF_SELECTEDIMAGE 0x0020
#endif
#ifndef TVE_COLLAPSE
#define TVE_COLLAPSE 0x0001
#endif
#ifndef TVE_EXPAND
#define TVE_EXPAND 0x0002
#endif
#ifndef TVM_SETITEM
#define TVM_SETITEM (TV_FIRST+13)
#endif
#ifndef TVM_GETEDITCONTROL
#define TVM_GETEDITCONTROL (TV_FIRST+15)
#endif
#ifndef WS_POPUP
#define WS_POPUP 0x80000000L
#endif
#ifndef MOD_ALT
#define MOD_ALT 0x0001
#endif
#ifndef MOD_CONTROL
#define MOD_CONTROL 0x0002
#endif
#ifndef MOD_SHIFT
#define MOD_SHIFT 0x0004
#endif
#ifndef MAKELANGID
#define MAKELANGID(p, s) ((WORD)(((WORD)(s) << 10) | (WORD)(p)))
#endif

/* ---- extra windowsx control macros ------------------------------------- */
#ifndef ListBox_GetText
#define ListBox_GetText(hwnd, i, buf) ((int)(SendMessage((hwnd), LB_GETTEXT, (WPARAM)(i), (LPARAM)(buf))))
#endif
#ifndef ListBox_GetSel
#define ListBox_GetSel(hwnd, i) ((int)(SendMessage((hwnd), LB_GETSEL, (WPARAM)(i), 0)))
#endif
#ifndef ListBox_SetSel
#define ListBox_SetSel(hwnd, f, i) ((int)(SendMessage((hwnd), LB_SETSEL, (WPARAM)(f), (LPARAM)(i))))
#endif
#ifndef ListBox_FindStringExact
#define ListBox_FindStringExact(hwnd, i, s) ((int)(SendMessage((hwnd), LB_FINDSTRINGEXACT, (WPARAM)(i), (LPARAM)(s))))
#endif
#ifndef ComboBox_GetItemData
#define ComboBox_GetItemData(hwnd, i) (SendMessage((hwnd), CB_GETITEMDATA, (WPARAM)(i), 0))
#endif
#ifndef ComboBox_AddString
#define ComboBox_AddString(hwnd, s) ((int)(SendMessage((hwnd), CB_ADDSTRING, 0, (LPARAM)(s))))
#endif
#ifndef ComboBox_SetCurSel
#define ComboBox_SetCurSel(hwnd, i) ((int)(SendMessage((hwnd), CB_SETCURSEL, (WPARAM)(i), 0)))
#endif
#ifndef ComboBox_FindString
#define ComboBox_FindString(hwnd, i, s) ((int)(SendMessage((hwnd), CB_FINDSTRING, (WPARAM)(i), (LPARAM)(s))))
#endif
#ifndef ComboBox_ResetContent
#define ComboBox_ResetContent(hwnd) ((int)(SendMessage((hwnd), CB_RESETCONTENT, 0, 0)))
#endif
#ifndef ComboBox_GetDroppedControlRect
#define ComboBox_GetDroppedControlRect(hwnd, prc) ((int)(SendMessage((hwnd), CB_GETDROPPEDCONTROLRECT, 0, (LPARAM)(prc))))
#endif
#ifndef TreeView_SetItem
#define TreeView_SetItem(hwnd, item) ((int)(SendMessage((hwnd), TVM_SETITEM, 0, (LPARAM)(item))))
#endif
#ifndef TreeView_GetEditControl
#define TreeView_GetEditControl(hwnd) ((HWND)(SendMessage((hwnd), TVM_GETEDITCONTROL, 0, 0)))
#endif

/* ---- hollow functions (second layer) ------------------------------------ */
inline LRESULT SendMessageA(HWND h, UINT m, WPARAM w, LPARAM l) {
	/*
	**  The engine's dialog-setup code populates combo and list-box controls with
	**  SendMessageA(CB_ADDSTRING / CB_FINDSTRING / CB_SETCURSEL / ...). This used
	**  to be a no-op stub, so every owner-draw combo opened with zero items --
	**  the drop-down came up 4px tall and empty and the closed box showed no
	**  selection. Route it to the real message dispatcher; it shares the exact
	**  ANSI signature of SendMessage.
	*/
	return SendMessage(h, m, w, l);
}
inline BOOL    SetWindowPos(HWND h, HWND a, int x, int y, int cx, int cy, UINT f) {
	/* Z-order is deliberately NOT applied here. The engine calls SetWindowPos
	 * with hWndInsertAfter == NULL to *resize* controls (see ListBoxCtrlProc
	 * attaching its scroll bar), and NULL means HWND_TOP -- honouring it
	 * raised those controls above their siblings and swallowed their clicks.
	 * The port keeps the child order the engine built. */
	(void)a;
	int rx = 0; int ry = 0; int rw = 0; int rh = 0;
	opents_win_get_rect((void *)h, &rx, &ry, &rw, &rh);
	if ((f & 0x0002) == 0) { rx = x; ry = y; }   // SWP_NOMOVE
	if ((f & 0x0001) == 0) { rw = cx; rh = cy; } // SWP_NOSIZE
	opents_win_set_rect((void *)h, rx, ry, rw, rh);
	if (f & 0x0040) opents_win_set_visible((void *)h, 1);  // SWP_SHOWWINDOW
	if (f & 0x0080) opents_win_set_visible((void *)h, 0);  // SWP_HIDEWINDOW
	return TRUE;
}
inline int     GetSystemMetrics(int i) { (void)i; return 0; }
/* Splits on whitespace into a single heap block: the pointer array first, then
 * the string data, so that the LocalFree the callers already perform releases
 * the whole thing. Two allocations would leak the strings, and leaving it null
 * makes every "-X" option invisible.
 *
 * Double quotes group, as they do on Windows and as opents_port_command_line()
 * writes them: a run of characters inside quotes is one argument whatever
 * whitespace it holds, and the quotes themselves are not part of it. That is
 * what keeps a directory name with a space in it arriving as the single
 * argument it was passed as. The engine's own parser strips any remaining
 * quotes, so a value quoted twice is still read correctly. */
inline LPWSTR *CommandLineToArgvW(LPCWSTR c, int *n) {
	if (n != nullptr) *n = 0;
	if (c == nullptr) return nullptr;

	std::vector<std::wstring> parts;
	std::wstring const line(c);
	size_t at = 0;

	while (at < line.size()) {
		while (at < line.size() && iswspace((wint_t)line[at])) at++;
		if (at >= line.size()) break;

		std::wstring part;
		bool quoted = false;
		bool had_quote = false;

		while (at < line.size()) {
			wchar_t const c = line[at];

			if (c == L'"') {
				// A quote toggles grouping; it is not itself part of the argument.
				quoted = !quoted;
				had_quote = true;
				at++;
				continue;
			}

			if (!quoted && iswspace((wint_t)c)) break;

			part += c;
			at++;
		}

		// An empty quoted argument ("" on the command line) is a real argument, so
		// what decides this is having been quoted at all, not being non-empty.
		if (!part.empty() || had_quote) {
			parts.push_back(part);
		}
	}

	if (parts.empty()) return nullptr;

	size_t bytes = (parts.size() + 1) * sizeof(wchar_t *);
	for (std::wstring const &part : parts) bytes += (part.size() + 1) * sizeof(wchar_t);

	void *block = calloc(1, bytes);
	if (block == nullptr) return nullptr;

	wchar_t **argv = static_cast<wchar_t **>(block);
	wchar_t *data = reinterpret_cast<wchar_t *>(static_cast<char *>(block)
							+ (parts.size() + 1) * sizeof(wchar_t *));

	for (size_t index = 0; index < parts.size(); index++) {
		argv[index] = data;
		wcscpy(data, parts[index].c_str());
		data += parts[index].size() + 1;
	}
	argv[parts.size()] = nullptr;

	if (n != nullptr) *n = static_cast<int>(parts.size());
	return argv;
}
inline BOOL    SetStdHandle(DWORD d, HANDLE h) { (void)d; (void)h; return TRUE; }
inline BOOL    IsDebuggerPresent(void) { return FALSE; }
/* A real, non-zero thread id. Zero is not a usable answer here: dbgprint.cpp
 * uses 0 as the sentinel for "nobody owns the logging lock", so a thread that
 * reports id 0 looks like the owner already and every message is dropped down
 * the re-entrancy path. The engine then formats messages it silently discards,
 * which reads as a hang that writes nothing. */
inline DWORD   GetCurrentThreadId(void) {
#ifdef __APPLE__
	uint64_t identifier = 0;
	if (pthread_threadid_np(nullptr, &identifier) == 0 && identifier != 0) {
		return static_cast<DWORD>(identifier);
	}
#endif
	/* Never zero, even if the platform query is unavailable. */
	return 1;
}
inline DWORD   GetCurrentProcessId(void) { return static_cast<DWORD>(::getpid()); }
inline HBITMAP CreateDIBSection(HDC h, const BITMAPINFO *b, UINT u, void **p, HANDLE s, DWORD o) {
	(void)h; (void)u; (void)s; (void)o;

	if (b == nullptr) {
		return nullptr;
	}

	/* The engine always asks for a top-down bitmap (negative height), which is
	 * the layout this GDI stores, so the sign is dropped rather than honoured. */
	int width = (int)b->bmiHeader.biWidth;
	int height = (int)b->bmiHeader.biHeight;
	if (height < 0) {
		height = -height;
	}

	void * bits = nullptr;
	void * bitmap = opents_gdi_create_dib_section(width, height, (int)b->bmiHeader.biBitCount, &bits);

	if (bitmap != nullptr && p != nullptr) {
		*p = bits;
	}
	return (HBITMAP)bitmap;
}
inline BOOL    AdjustWindowRectEx(RECT *r, DWORD s, BOOL m, DWORD e) { (void)r;(void)s;(void)m;(void)e; return TRUE; }
inline BOOL    DestroyCursor(HCURSOR c) { opents_cursor_destroy(c); return TRUE; }
/* Declared here because the named-template form below needs them; both are
 * defined further down, in the resource section. */
inline HRSRC   FindResource(HINSTANCE h, LPCTSTR n, LPCTSTR t);
inline void   *LoadResource(HINSTANCE h, HRSRC r);
inline LPVOID  LockResource(void *h);

inline HWND    CreateDialogParam(HINSTANCE h, LPCTSTR t, HWND w, DLGPROC p, LPARAM l) {
	// Named template: find it, then let the indirect form build it. The engine
	// only reaches this for dialogs it names by id, so MAKEINTRESOURCE applies.
	void * found = FindResource(h, t, (LPCTSTR)RT_DIALOG);
	if (found == nullptr) {
		/*
		** Dialog 198 is the one case that legitimately is not in
		** Language.dll. It is IDD_TEMPLATE from the executable's own
		** Sun.rc, declared DIALOGEX 0, 0, 200, 100, and the engine creates
		** it only to measure it: Resize_Dialog reads its client rectangle
		** to turn dialog units into pixels, then destroys it. Nothing is
		** ever drawn or interacted with.
		**
		** This target has no resource compiler, so the 200x100 template
		** Sun.rc declares is emitted directly, in the classic form the
		** parser here understands. The measurement is all that matters,
		** and 200x100 units is what makes the engine's own scale constants
		** (300x163) come out as very nearly identity -- the designed
		** geometry survives the conversion untouched.
		**
		** Without it CreateDialogParam returns NULL, the client rectangle
		** is 0x0, and every later dialog divides by zero.
		*/
		if (((uintptr_t)t >> 16) == 0 && (unsigned)(uintptr_t)t == 198) {
			// style, exStyle, cdit, x, y, cx, cy, then three absent names.
			static unsigned char const kTemplate198[24] = {
				0x00, 0x00, 0xC8, 0x90,   // style  = WS_POPUP|WS_VISIBLE|WS_CAPTION|WS_SYSMENU
				0x00, 0x00, 0x00, 0x00,   // exStyle
				0x00, 0x00,               // cdit   = 0
				0x00, 0x00, 0x00, 0x00,   // x = 0, y = 0
				0xC8, 0x00, 0x64, 0x00,   // cx = 200, cy = 100
				0x00, 0x00,               // menu  (absent)
				0x00, 0x00,               // class (absent)
				0x00, 0x00,               // title (absent)
			};
			return (HWND)opents_win_create_dialog(kTemplate198, sizeof(kTemplate198),
			                                      (void *)w, (void *)p);
		}
		return nullptr;
	}
	void const * bytes = LockResource(found);
	return CreateDialogIndirectParam(h, (LPCDLGTEMPLATE)bytes, w, p, l);
}
inline HFONT   CreateFontIndirect(const void *l) { (void)l; return nullptr; }
inline HFONT   CreateFont(int a,int b,int c,int d,int e,DWORD f,DWORD g,DWORD h,DWORD i,DWORD j,DWORD k,DWORD l,DWORD m,LPCTSTR n) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;(void)j;(void)k;(void)l;(void)m;(void)n; return nullptr; }
inline HCURSOR LoadCursor(HINSTANCE h, LPCTSTR n) { (void)h; (void)n; return nullptr; }
inline HICON   LoadIcon(HINSTANCE h, LPCTSTR n) { (void)h; (void)n; return nullptr; }
/* Looks the resource up in the PE image behind the module token.
 *
 * Both arguments may be identifiers rather than strings: every dialog lookup
 * passes MAKEINTRESOURCE(id) for the name and (LPCSTR)RT_DIALOG for the type, so
 * port_resource decodes that convention itself. A module that is not a resource
 * module -- or a name that is not in the image -- yields null, which the caller
 * reads as "no such dialog". */
inline HRSRC   FindResource(HINSTANCE h, LPCTSTR n, LPCTSTR t) {
	return (HRSRC)opents_resource_find((void *)h, (const void *)n, (const void *)t);
}
/* The standard streams are the real POSIX descriptors. Returning null made
 * dbgprint.cpp treat the console as unavailable and drop every message. */
inline HANDLE  GetStdHandle(DWORD d) {
	int fd = 1;                                  /* STD_OUTPUT_HANDLE */
	if (d == STD_ERROR_HANDLE) fd = 2;
	else if (d == STD_INPUT_HANDLE) fd = 0;
	return reinterpret_cast<HANDLE>(static_cast<intptr_t>(fd));
}
inline HWND    GetConsoleWindow(void) { return nullptr; }
inline HMENU   GetSystemMenu(HWND h, BOOL r) { (void)h;(void)r; return nullptr; }
/* Writes to the descriptor the handle carries. A headless target has no console
 * window to allocate, so the engine's debug console is the terminal it was
 * started from: this is the only place its diagnostics can go. */
inline BOOL    WriteConsole(HANDLE h, const void *b, DWORD n, DWORD *w, void *r) {
	(void)r;
	int fd = static_cast<int>(reinterpret_cast<intptr_t>(h));
	if (fd != 0 && fd != 1 && fd != 2) fd = 2;   /* not a standard stream: stderr */
	ssize_t done = ::write(fd, b, n);
	if (done < 0) { if (w != nullptr) *w = 0; return FALSE; }
	if (w != nullptr) *w = static_cast<DWORD>(done);
	return TRUE;
}
inline UINT    GetOEMCP(void) { return 0; }
inline int     _getch(void) { return -1; }
inline int     GetWindowTextLength(HWND h) { return opents_win_get_text_length((void *)h); }
inline int     SetStretchBltMode(HDC h, int m) { (void)h;(void)m; return 0; }
inline BOOL    StretchBlt(HDC d,int a,int b,int c,int e,HDC s,int f,int g,int h,int i,DWORD j) {
	(void)j;	/* SRCCOPY is the only raster operation the engine asks for. */
	return opents_gdi_stretch_blt((void *)d, a, b, c, e, (void *)s, f, g, h, i) ? TRUE : FALSE;
}
inline BOOL    RedrawWindow(HWND h, const RECT *r, void *g, UINT f) { (void)h;(void)r;(void)g;(void)f; return TRUE; }
inline BOOL    EndDialog(HWND h, intptr_t r) { opents_win_end_dialog((void *)h, r); return TRUE; }
inline intptr_t DialogBoxParam(HINSTANCE h, LPCTSTR t, HWND w, DLGPROC p, LPARAM l) { (void)h;(void)t;(void)w;(void)p;(void)l; return 0; }
inline BOOL    FileTimeToLocalFileTime(const FILETIME *a, FILETIME *b) { (void)a;(void)b; return TRUE; }
inline BOOL    FileTimeToSystemTime(const FILETIME *a, SYSTEMTIME *b) { (void)a;(void)b; return TRUE; }
inline BOOL    GetMonitorInfo(HMONITOR h, void *i) { (void)h;(void)i; return TRUE; }
inline LONG    RegQueryValueEx(HKEY k, LPCTSTR n, DWORD *r, DWORD *t, BYTE *d, DWORD *c) { (void)k;(void)n;(void)r;(void)t;(void)d;(void)c; return 0; }
inline LONG    RegCloseKey(HKEY k) { (void)k; return 0; }
inline void    OleUninitialize(void) {}
inline UINT    SetTextAlign(HDC h, UINT a) { (void)h;(void)a; return 0; }
inline int     SaveDC(HDC h) { (void)h; return 1; }
inline BOOL    SetViewportOrgEx(HDC h, int x, int y, POINT *p) { (void)h;(void)x;(void)y;(void)p; return TRUE; }
inline BOOL    SetWindowOrgEx(HDC h, int x, int y, POINT *p) { (void)h;(void)x;(void)y;(void)p; return TRUE; }
inline BOOL    DPtoLP(HDC h, POINT *p, int n) { (void)h;(void)p;(void)n; return TRUE; }
inline BOOL    RestoreDC(HDC h, int n) { (void)h;(void)n; return TRUE; }
inline BOOL    ImageList_DragMove(int x, int y) { (void)x;(void)y; return FALSE; }
inline int     GetDeviceCaps(HDC h, int i) {
	(void)h;
	if (i == VREFRESH) {
		return opents_window_get_refresh_hz(nullptr);
	}
	return 0;
}

/* ===========================================================================
 * PHASE 1 STUB CLOSURE -- Batch 6: third-layer Win32 symbols.
 * =========================================================================== */

/* ---- missing types ------------------------------------------------------ */
#ifndef WINDOWPOS
typedef struct WINDOWPOS {
	HWND hwnd; HWND hwndInsertAfter; int x, y, cx, cy; UINT flags;
} WINDOWPOS;
#endif
#ifndef HELPINFO
typedef struct HELPINFO {
	UINT cbSize; int iContextType; int iCtrlId; HANDLE hItemHandle; DWORD dwContextId; POINT MousePos;
} HELPINFO;
#endif

/* ---- constants ---------------------------------------------------------- */
#ifndef FORMAT_MESSAGE_FROM_SYSTEM
#define FORMAT_MESSAGE_FROM_SYSTEM 0x00001000
#endif
#ifndef MB_ICONWARNING
#define MB_ICONWARNING 0x00000030L
#endif
#ifndef LB_SELITEMRANGE
#define LB_SELITEMRANGE 0x019B
#endif
#ifndef HOTKEY_CLASS
#define HOTKEY_CLASS TEXT("msctls_hotkey32")
#endif
#ifndef GWLP_WNDPROC
#define GWLP_WNDPROC (-4)
#endif
#ifndef WM_NCMOUSEMOVE
#define WM_NCMOUSEMOVE 0x00A0
#endif
#ifndef WM_KEYLAST
#define WM_KEYLAST 0x0109
#endif
#ifndef WM_MOUSELAST
#define WM_MOUSELAST 0x020E
#endif
#ifndef WM_WINDOWPOSCHANGING
#define WM_WINDOWPOSCHANGING 0x0046
#endif
#ifndef SWP_NOOWNERZORDER
#define SWP_NOOWNERZORDER 0x0200
#endif
#ifndef SYSTEM_FONT
#define SYSTEM_FONT 13
#endif
#ifndef SB_THUMBPOSITION
#define SB_THUMBPOSITION 4
#endif
#ifndef WAIT_FAILED
#define WAIT_FAILED ((DWORD)0xFFFFFFFF)
#endif
#ifndef INFINITE
#define INFINITE ((DWORD)0xFFFFFFFF)
#endif
#ifndef HELP_CONTEXTPOPUP
#define HELP_CONTEXTPOPUP 0x0008
#endif
#ifndef HELP_CONTEXTMENU
#define HELP_CONTEXTMENU 0x000A
#endif
#ifndef WC_TREEVIEW
#define WC_TREEVIEW TEXT("SysTreeView32")
#endif
#ifndef TVM_EXPAND
#define TVM_EXPAND (TV_FIRST+2)
#endif
#ifndef TVGN_ROOT
#define TVGN_ROOT 0x0
#endif
#ifndef TVGN_NEXT
#define TVGN_NEXT 0x1
#endif

/* ---- extra windowsx control macros ------------------------------------- */
#ifndef TreeView_Expand
#define TreeView_Expand(hwnd, item, code) ((int)(SendMessage((hwnd), TVM_EXPAND, (WPARAM)(code), (LPARAM)(item))))
#endif
#ifndef TreeView_GetRoot
#define TreeView_GetRoot(hwnd) ((HTREEITEM)(SendMessage((hwnd), TVM_GETNEXTITEM, TVGN_ROOT, 0)))
#endif
#ifndef TreeView_GetNextSibling
#define TreeView_GetNextSibling(hwnd, item) ((HTREEITEM)(SendMessage((hwnd), TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)(item))))
#endif
#ifndef Button_Enable
#define Button_Enable(hwnd, f) EnableWindow((hwnd), (f))
#endif

/* ---- hollow functions (third layer) ------------------------------------- */
inline void    SetLastError(DWORD e) { (void)e; }
inline HGDIOBJ GetStockObject(int i) { (void)i; return nullptr; }
inline BOOL    CheckDlgButton(HWND h, int id, UINT c) { (void)h;(void)id;(void)c; return TRUE; }
inline LRESULT CallWindowProc(WNDPROC p, HWND h, UINT m, WPARAM w, LPARAM l) {
	// Reaches the procedure a subclass replaced. That is either the default one
	// GetWindowLong handed out for a window that was never subclassed, or a
	// procedure the engine installed earlier; both are the same ABI.
	if (p == nullptr) return 0;
	typedef intptr_t (*OpentsCallProc)(void *, unsigned, uintptr_t, intptr_t);
	if ((void *)p == (void *)&opents_win_def_proc) {
		return (LRESULT)opents_win_def_proc((void *)h, (unsigned)m, (uintptr_t)w, (intptr_t)l);
	}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-function-type"
	return (LRESULT)((OpentsCallProc)p)((void *)h, (unsigned)m, (uintptr_t)w, (intptr_t)l);
#pragma clang diagnostic pop
}
inline DWORD   GetWindowContextHelpId(HWND h) { (void)h; return 0; }
inline BOOL    DeleteMenu(HMENU m, UINT p, UINT f) { (void)m;(void)p;(void)f; return TRUE; }
inline int     GetDateFormat(DWORD loc, DWORD fl, const SYSTEMTIME *t, LPCTSTR fmt, LPTSTR out, int n) { (void)loc;(void)fl;(void)t;(void)fmt; if(out&&n>0)out[0]=0; (void)n; return 0; }
inline int     GetTimeFormat(DWORD loc, DWORD fl, const SYSTEMTIME *t, LPCTSTR fmt, LPTSTR out, int n) { (void)loc;(void)fl;(void)t;(void)fmt; if(out&&n>0)out[0]=0; (void)n; return 0; }
inline BOOL    SetWindowTextA(HWND h, const char *t) { (void)h; (void)t; return TRUE; }
inline LONG    RegOpenKeyEx(HKEY k, LPCTSTR n, DWORD o, DWORD s, HKEY *r) { (void)k;(void)n;(void)o;(void)s; if(r)*r=nullptr; return 0; }
inline BOOL    GetUpdateRect(HWND h, RECT *r, BOOL e) {
	(void)e;
	int x = 0, y = 0, w = 0, hh = 0;
	if (!opents_win_get_update((void *)h, &x, &y, &w, &hh)) {
		return FALSE;
	}
	if (r != nullptr) {
		r->left = x; r->top = y;
		r->right = x + w; r->bottom = y + hh;
	}
	return TRUE;
}
inline int     GetBkMode(HDC h) { (void)h; return 0; }
inline COLORREF GetBkColor(HDC h) { (void)h; return 0; }
inline COLORREF GetTextColor(HDC h) { (void)h; return 0; }
inline COLORREF SetBkColor(HDC h, COLORREF c) { (void)h; return c; }
inline HWND    WindowFromPoint(POINT p) { (void)p; return nullptr; }
inline int     GetDlgCtrlID(HWND h) { return (int)(intptr_t)opents_win_get_long((void *)h, GWL_ID); }
/* Named mutexes are per-process tokens here, and CreateMutex never reports that
 * one already existed.
 *
 * Returning null is not a neutral answer: startup.cpp claims the AutoPlay mutex
 * in a do/while that repeats until the handle is non-null, so a null result
 * spins forever calling the logger, which looks exactly like a hang that
 * produces no window and no output. Cross-process instance detection is a
 * convenience Windows gets from named kernel objects; this target is treated as
 * the only instance, so OpenMutex finds nothing and CreateMutex always succeeds.
 *
 * The handle is drawn from a range no file descriptor uses, so that the
 * CloseHandle callers make against it fails harmlessly instead of closing a
 * descriptor something else owns. */
inline HANDLE  CreateMutex(void *a, BOOL i, LPCTSTR n) {
	(void)a; (void)i; (void)n;
	static std::atomic<intptr_t> next{0x7F000000};
	return reinterpret_cast<HANDLE>(next.fetch_add(1) + 1);
}
inline HRESULT OleInitialize(void *r) { (void)r; return 0; }
inline BOOL    SetCurrentDirectory(LPCTSTR d) { (void)d; return TRUE; }
inline void    PostQuitMessage(int c) { (void)c; }
inline int     SetGraphicsMode(HDC h, int m) { (void)h;(void)m; return 1; }
inline BOOL    ModifyWorldTransform(HDC h, const void *x, DWORD m) { (void)h;(void)x;(void)m; return TRUE; }
inline BOOL    GetTextMetrics(HDC h, TEXTMETRIC *t) { (void)h;(void)t; return TRUE; }
inline BOOL    WinHelp(HWND h, LPCTSTR f, UINT c, ULONG_PTR d) { (void)h;(void)f;(void)c;(void)d; return TRUE; }
inline BOOL    RegisterHotKey(HWND h, int id, UINT m, UINT k) { (void)h;(void)id;(void)m;(void)k; return TRUE; }
inline HWND    FindWindow(LPCTSTR c, LPCTSTR w) { (void)c;(void)w; return nullptr; }
inline HANDLE  OpenMutex(DWORD a, BOOL i, LPCTSTR n) { (void)a;(void)i;(void)n; return nullptr; }
inline DWORD   WaitForSingleObject(HANDLE h, DWORD t) { (void)h;(void)t; return 0; }

/* ===========================================================================
 * PHASE 1 STUB CLOSURE -- Batch 7: fourth-layer Win32 symbols.
 * =========================================================================== */

/* ---- constants ---------------------------------------------------------- */
#ifndef DWLP_MSGRESULT
#define DWLP_MSGRESULT 0
#endif
#ifndef DWLP_DLGPROC
#define DWLP_DLGPROC ((int)(0 + sizeof(LRESULT)))
#endif
#ifndef DWLP_USER
#define DWLP_USER ((int)(0 + sizeof(LRESULT) + sizeof(LPARAM)))
#endif
#ifndef SM_CXFULLSCREEN
#define SM_CXFULLSCREEN 16
#endif
#ifndef SM_CYFULLSCREEN
#define SM_CYFULLSCREEN 17
#endif
#ifndef WS_DISABLED
#define WS_DISABLED 0x08000000L
#endif
#ifndef WC_LISTVIEW
#define WC_LISTVIEW TEXT("SysListView32")
#endif
#ifndef TVIS_EXPANDED
#define TVIS_EXPANDED 0x0020
#endif
#ifndef TVIF_STATE
#define TVIF_STATE 0x0008
#endif
#ifndef GW_HWNDPREV
#define GW_HWNDPREV 3
#endif
#ifndef WM_SETFONT
#define WM_SETFONT 0x0030
#endif
#ifndef NULL_BRUSH
#define NULL_BRUSH 5
#endif
#ifndef TCM_FIRST
#define TCM_FIRST 0x1300
#endif
#ifndef TCM_GETITEMCOUNT
#define TCM_GETITEMCOUNT (TCM_FIRST+4)
#endif
#ifndef TCM_GETITEMRECT
#define TCM_GETITEMRECT (TCM_FIRST+10)
#endif
#ifndef TCM_GETCURSEL
#define TCM_GETCURSEL (TCM_FIRST+11)
#endif
#ifndef TCM_SETITEMSIZE
#define TCM_SETITEMSIZE (TCM_FIRST+41)
#endif
#ifndef LVM_FIRST
#define LVM_FIRST 0x1000
#endif
#ifndef LVM_GETCOLUMNWIDTH
#define LVM_GETCOLUMNWIDTH (LVM_FIRST+29)
#endif
#ifndef LVM_SETCOLUMNWIDTH
#define LVM_SETCOLUMNWIDTH (LVM_FIRST+30)
#endif
#ifndef TVM_GETITEM
#define TVM_GETITEM (TV_FIRST+62)
#endif

/* ---- extra windowsx control macros ------------------------------------- */
#ifndef TabCtrl_GetItemCount
#define TabCtrl_GetItemCount(hwnd) ((int)(SendMessage((hwnd), TCM_GETITEMCOUNT, 0, 0)))
#endif
#ifndef TabCtrl_GetCurSel
#define TabCtrl_GetCurSel(hwnd) ((int)(SendMessage((hwnd), TCM_GETCURSEL, 0, 0)))
#endif
#ifndef TabCtrl_GetItemRect
#define TabCtrl_GetItemRect(hwnd, i, prc) ((BOOL)(SendMessage((hwnd), TCM_GETITEMRECT, (WPARAM)(i), (LPARAM)(prc))))
#endif
#ifndef ListView_GetColumnWidth
#define ListView_GetColumnWidth(hwnd, i) ((int)(SendMessage((hwnd), LVM_GETCOLUMNWIDTH, (WPARAM)(i), 0)))
#endif
#ifndef ListView_SetColumnWidth
#define ListView_SetColumnWidth(hwnd, i, w) ((BOOL)(SendMessage((hwnd), LVM_SETCOLUMNWIDTH, (WPARAM)(i), (LPARAM)(w))))
#endif
#ifndef TreeView_GetItem
#define TreeView_GetItem(hwnd, item) ((BOOL)(SendMessage((hwnd), TVM_GETITEM, 0, (LPARAM)(item))))
#endif

/* ---- hollow functions (fourth layer) ----------------------------------- */
inline BOOL    IntersectRect(RECT *d, const RECT *a, const RECT *b) { (void)d;(void)a;(void)b; return FALSE; }
inline DWORD   FormatMessage(DWORD f, const void *s, DWORD m, DWORD l, LPTSTR buf, DWORD n, void *a) {
	(void)f;(void)s;(void)m;(void)l;(void)a; if(buf&&n>0)buf[0]=0; (void)n; return 0;
}

/* ===========================================================================
 * PHASE 1 STUB CLOSURE -- Batch 8: fifth-layer symbols, moved file API, timeb.
 * =========================================================================== */

/* ---- file / codepage API (moved here from shim/winuser.h so non-GUI TUs see
 * them; the force-include of windows_stub.h precedes any <winuser.h>). ------- */
#ifndef CP_ACP
#define CP_ACP 0
#endif
/* Really creates the directory, and reports success when it is already there.
 *
 * Claiming success without creating anything left dbgprint.cpp writing a log
 * into a directory that did not exist. Reporting failure when it already exists
 * would be just as wrong: callers test
 * `CreateDirectory(...) || GetLastError() == ERROR_ALREADY_EXISTS`, and this
 * target's GetLastError answers with errno, whose EEXIST is not 183. */
inline BOOL CreateDirectory(const char *lpPathName, void * /*lpSecurityAttributes*/) {
	if (lpPathName == nullptr || *lpPathName == '\0') return FALSE;

	struct stat info;
	if (::stat(lpPathName, &info) == 0) {
		return S_ISDIR(info.st_mode) ? TRUE : FALSE;
	}
	return ::mkdir(lpPathName, 0777) == 0 ? TRUE : FALSE;
}

/* ---- MSVC _timeb/_ftime: macOS <sys/timeb.h> provides struct timeb/ftime but
 * not the MSVC underscore variants. Define only the latter (defining struct
 * timeb here would clash with the system header). --------------------------- */
#ifndef OPENTS_TIMEB_DEFINED
#define OPENTS_TIMEB_DEFINED
/* <sys/timeb.h> supplies the complete `struct timeb` and `ftime()` that some
 * engine TUs (e.g. init.cpp) use directly. Including the real header avoids a
 * hand-written copy that could clash with the system definition elsewhere. */
#include <sys/timeb.h>
struct _timeb { long time; unsigned short millitm; short timezone; short dstflag; };
inline void _ftime(struct _timeb *t) {
	/*
	**  This used to zero the structure unconditionally, which froze the clock
	**  at zero for the one caller -- the owner-draw dialog's opening wipe
	**  (ownrdraw.cpp). Its frame pacing computes the remaining wait as
	**  start + frame * interval - now, so with now == start == 0 every frame
	**  slept the whole elapsed target again: the wait grew quadratically and
	**  the wipe stretched from its designed ~1s to over 10s, which is the lag
	**  seen opening every menu (skirmish, map selection, ...).
	*/
	if (t) {
		struct timeval tv;
		gettimeofday(&tv, nullptr);
		t->time = (long)tv.tv_sec;
		t->millitm = (unsigned short)(tv.tv_usec / 1000);
		t->timezone = 0;
		t->dstflag = 0;
	}
}
#endif

/* ---- MSVC CRT aligned-allocation family ---------------------------------
 * bgfxbackend.cpp's allocator uses _aligned_free/_aligned_realloc to honour
 * bgfx's cache-line-aligned render records (the Win32 CRT only guarantees
 * 8-byte alignment). Back them with posix_memalign. A small header is stored
 * in the alignment slack so the original block and its size remain recoverable,
 * which keeps _aligned_realloc and _aligned_msize correct. ------------- */
#include <cstdlib>
namespace _opents_port {
	struct aligned_hdr { void *raw; size_t size; };
}
inline void *_aligned_malloc(size_t size, size_t alignment) {
	using _opents_port::aligned_hdr;
	if (alignment < sizeof(aligned_hdr)) alignment = sizeof(aligned_hdr);
	void *raw = nullptr;
	if (posix_memalign(&raw, alignment, size + alignment) != 0) return nullptr;
	char *aligned = static_cast<char *>(raw) + alignment;
	aligned_hdr *h = reinterpret_cast<aligned_hdr *>(aligned) - 1;
	h->raw = raw;
	h->size = size;
	return aligned;
}
inline void _aligned_free(void *p) {
	using _opents_port::aligned_hdr;
	if (!p) return;
	::free((reinterpret_cast<aligned_hdr *>(p) - 1)->raw);
}
inline void *_aligned_realloc(void *p, size_t size, size_t alignment) {
	using _opents_port::aligned_hdr;
	if (!p) return _aligned_malloc(size, alignment);
	size_t old = (reinterpret_cast<aligned_hdr *>(p) - 1)->size;
	void *q = _aligned_malloc(size, alignment);
	if (q && old) memcpy(q, p, old < size ? old : size);
	_aligned_free(p);
	return q;
}
inline size_t _aligned_msize(void *p, size_t /*alignment*/, size_t /*offset*/) {
	using _opents_port::aligned_hdr;
	if (!p) return 0;
	return (reinterpret_cast<aligned_hdr *>(p) - 1)->size;
}

/* ---- tab-control / misc types ------------------------------------------ */
#ifndef TCIF_TEXT
#define TCIF_TEXT 0x0001
#endif
#ifndef TC_ITEM
typedef struct TC_ITEM {
	UINT mask; UINT dwState; UINT dwStateMask; LPTSTR pszText; int cchTextMax; int iImage; LPARAM lParam;
} TC_ITEM;
#endif
#ifndef TCITEM
typedef TC_ITEM TCITEM;
#endif
#ifndef TCM_GETITEM
#define TCM_GETITEM (TCM_FIRST+60)
#endif

/* ---- constants (fifth layer) ------------------------------------------- */
#ifndef WS_TABSTOP
#define WS_TABSTOP 0x00010000L
#endif
#ifndef CB_SETITEMHEIGHT
#define CB_SETITEMHEIGHT 0x0153
#endif
#ifndef ES_PASSWORD
#define ES_PASSWORD 0x0020L
#endif
#ifndef SS_RIGHT
#define SS_RIGHT 0x00000002L
#endif
#ifndef RDW_INTERNALPAINT
#define RDW_INTERNALPAINT 0x0002
#endif
#ifndef RDW_FRAME
#define RDW_FRAME 0x0400
#endif
#ifndef WM_SYSDEADCHAR
#define WM_SYSDEADCHAR 0x0107
#endif
#ifndef WM_WINDOWPOSCHANGED
#define WM_WINDOWPOSCHANGED 0x0047
#endif
#ifndef EM_GETSEL
#define EM_GETSEL 0x00B0
#endif
#ifndef EM_POSFROMCHAR
#define EM_POSFROMCHAR 0x00D6
#endif
/* Matches except.h's definition (WM_APP + 0x54) so a TU including both does
 * not see a macro redefinition. */
#ifndef WM_EXCEPTION_TEST
#define WM_EXCEPTION_TEST (WM_APP + 0x54)
#endif

/* ---- extra windowsx control macro -------------------------------------- */
#ifndef TabCtrl_GetItem
#define TabCtrl_GetItem(hwnd, i, item) ((BOOL)(SendMessage((hwnd), TCM_GETITEM, (WPARAM)(i), (LPARAM)(item))))
#endif

/* ---- hollow functions (fifth layer) ------------------------------------ */
inline HWND    GetFocus(void) { return (HWND)opents_win_get_focus(); }
inline HWND    GetNextDlgTabItem(HWND h, HWND c, BOOL p) {
	return (HWND)opents_win_next_tab_item((void *)h, (void *)c, p ? 1 : 0, 0);
}

/* ---- ownerdraw.cpp closure (sixth layer) ------------------------------- */
#ifndef WM_GETDLGCODE
#define WM_GETDLGCODE 0x0087
#endif
#ifndef SB_LINEUP
#define SB_LINEUP 0
#endif
#ifndef SB_LINEDOWN
#define SB_LINEDOWN 1
#endif
#ifndef SB_ENDSCROLL
#define SB_ENDSCROLL 8
#endif
#ifndef TB_THUMBTRACK
#define TB_THUMBTRACK 5
#endif
#ifndef MK_LBUTTON
#define MK_LBUTTON 0x0001
#endif
#ifndef MK_RBUTTON
#define MK_RBUTTON 0x0002
#endif
#ifndef MK_SHIFT
#define MK_SHIFT 0x0004
#endif
#ifndef MK_CONTROL
#define MK_CONTROL 0x0008
#endif
#ifndef DT_SINGLELINE
#define DT_SINGLELINE 0x00000020
#endif
#ifndef DT_VCENTER
#define DT_VCENTER 0x00000004
#endif
#ifndef DT_WORDBREAK
#define DT_WORDBREAK 0x00000010
#endif
#ifndef DT_CENTER
#define DT_CENTER 0x00000001
#endif
#ifndef DT_NOPREFIX
#define DT_NOPREFIX 0x00000800
#endif
#ifndef DT_CALCRECT
#define DT_CALCRECT 0x00000400
#endif
#ifndef PBM_SETRANGE
#define PBM_SETRANGE 0x0401
#endif
#ifndef PBM_SETPOS
#define PBM_SETPOS 0x0402
#endif
#ifndef PBM_SETRANGE32
#define PBM_SETRANGE32 0x0406
#endif
#ifndef WS_BORDER
#define WS_BORDER 0x00800000
#endif
#ifndef SM_CXBORDER
#define SM_CXBORDER 5
#endif
#ifndef SM_CYBORDER
#define SM_CYBORDER 6
#endif
#ifndef ODT_BUTTON
#define ODT_BUTTON 4
#endif
#ifndef ODT_COMBOBOX
#define ODT_COMBOBOX 3
#endif
#ifndef ODT_LISTBOX
#define ODT_LISTBOX 2
#endif
#ifndef ODT_STATIC
#define ODT_STATIC 5
#endif
/* Real signatures: DrawTextA(HDC, LPCSTR, int, LPRECT, UINT) and
 * GetKeyNameTextA(LONG, LPSTR, int). */
inline int DrawText(HDC h, LPCTSTR s, int n, LPRECT r, UINT f) { (void)h;(void)s;(void)n;(void)r;(void)f; return 0; }
inline int GetKeyNameText(LONG p, LPTSTR s, int n) { (void)p; if(s&&n>0)s[0]=0; return 0; }

/* POSIX has no text/binary mode distinction, so the flag MSVC's open() takes is a no-op.
 * <fcntl.h> is already included above and does not define it on this platform. */
#ifndef O_BINARY
#define O_BINARY 0
#endif

/* MSVC's filelength(fd) reports the size of an open file descriptor. vqalib asks for it
 * when a VQA stream is queried for its length; fstat is the portable equivalent and,
 * unlike an lseek to the end and back, leaves the read position untouched. */
inline long filelength(int fd) {
	struct stat st;
	if (fd < 0 || fstat(fd, &st) != 0) return -1L;
	return (long)st.st_size;
}

#endif /* OPENTS_PORT_WINDOWS_STUB_H */
