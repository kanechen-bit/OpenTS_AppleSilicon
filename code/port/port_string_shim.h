/*******************************************************************************
 *                                O P E N  T S
 ******************************************************************************/
/* Port shim: MSVC CRT helpers the engine uses in files that never include
 * <windows.h> (string case helpers, path builders, a few constants). Because
 * port_early.h force-includes this file FIRST in every non-Windows TU, these
 * symbols are visible before any engine template is defined (fixing two-phase
 * name lookup) and before <windows.h> would otherwise provide them.
 *
 * Everything is individually guarded so a later windows_stub.h include that
 * also defines the same symbol is a harmless no-op. */
#ifndef OPENTS_PORT_STRING_SHIM_H
#define OPENTS_PORT_STRING_SHIM_H

#include <cctype>
#include <cstring>
#include <cstdio>
#include <cstdarg>

/* ---- Case-insensitive string helpers ------------------------------------ */
inline char *strupr(char *s) {
	if (s) for (char *p = s; *p; ++p) *p = (char)::toupper((unsigned char)*p);
	return s;
}
inline char *strlwr(char *s) {
	if (s) for (char *p = s; *p; ++p) *p = (char)::tolower((unsigned char)*p);
	return s;
}
inline int stricmp(const char *a, const char *b)  { return ::strcasecmp(a, b); }
inline int strnicmp(const char *a, const char *b, size_t n) { return ::strncasecmp(a, b, n); }
/* The engine also calls the leading-underscore MSVC aliases directly. always.h
 * maps _stricmp/_strnicmp -> stricmp/strnicmp, but force-including this file
 * FIRST means the real functions are already visible even in TUs that do not
 * include always.h. */
inline int _stricmp(const char *a, const char *b)  { return ::strcasecmp(a, b); }
inline int _strnicmp(const char *a, const char *b, size_t n) { return ::strncasecmp(a, b, n); }

inline char *strrev(char *s) {
	if (s) { char *b = s, *e = s; while (*e) ++e; --e; while (b < e) { char t = *b; *b++ = *e; *e-- = t; } }
	return s;
}

/* ---- Path builders (MSVC <stdlib.h>) ------------------------------------ */
#ifndef _MAX_PATH
#define _MAX_PATH    260
#define _MAX_DRIVE   3
#define _MAX_DIR     256
#define _MAX_FNAME   255
#define _MAX_EXT     256
#define _MAX_BASE    255
#endif

#ifndef OPENTS_PORT_CRT_HELPERS
#define OPENTS_PORT_CRT_HELPERS
inline void _makepath(char *path, const char *drive, const char *dir, const char *fname, const char *ext) {
	path[0] = '\0';
	if (drive && drive[0]) { *path++ = drive[0]; *path++ = ':'; *path = '\0'; }
	if (dir && dir[0]) {
		size_t l = ::strlen(dir);
		::strcpy(path, dir);
		path += l;
		if (path[-1] != '/' && path[-1] != '\\') { *path++ = '/'; *path = '\0'; }
	}
	if (fname && fname[0]) { ::strcpy(path, fname); path += ::strlen(fname); }
	if (ext && ext[0]) {
		if (ext[0] != '.') *path++ = '.';
		::strcpy(path, ext);
	}
}
inline void _splitpath(const char *path, char *drive, char *dir, char *fname, char *ext) {
	if (drive) drive[0] = '\0';
	if (dir) dir[0] = '\0';
	if (fname) fname[0] = '\0';
	if (ext) ext[0] = '\0';
	if (!path) return;
	const char *lastslash = NULL;
	for (const char *q = path; *q; ++q) if (*q == '/' || *q == '\\') lastslash = q;
	const char *base = lastslash ? lastslash + 1 : path;
	if (drive) { if (path[0] && path[1] == ':') { drive[0] = path[0]; drive[1] = ':'; drive[2] = '\0'; } }
	if (dir && lastslash) { size_t n = (size_t)(lastslash - (drive && path[1]==':' ? path + 2 : path)) + 1; ::strncpy(dir, path, n); dir[n] = '\0'; }
	if (fname) { const char *e = base; while (*e && *e != '.') ++e; ::strncpy(fname, base, (size_t)(e - base)); fname[e - base] = '\0'; }
	if (ext) { const char *e = base; while (*e && *e != '.') ++e; if (*e == '.') ::strcpy(ext, e); }
}
inline int wsprintf(char *buf, const char *fmt, ...) {
	va_list ap; va_start(ap, fmt);
	int n = ::vsnprintf(buf, 4096, fmt, ap);
	va_end(ap);
	return n;
}
#endif /* OPENTS_PORT_CRT_HELPERS */

/* ---- Misc MSVC ctype / boolean constants -------------------------------- */
#ifndef _CONTROL
#define _CONTROL 0x20  /* MSVC ctype mask; engine uses as control-char threshold */
#endif
#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

#endif /* OPENTS_PORT_STRING_SHIM_H */
