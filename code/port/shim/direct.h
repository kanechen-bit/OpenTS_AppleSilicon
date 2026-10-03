#pragma once
/*
 * Shim <direct.h> (MSVC directory ops) for the non-Windows experimental
 * OpenTS build. Maps to POSIX in <unistd.h> / <sys/stat.h>.
 */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/direct.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif

#include <unistd.h>
#include <sys/stat.h>

inline char *_getcwd(char *buf, int maxlen) { return ::getcwd(buf, (size_t)maxlen); }
inline int   _chdir(const char *path)       { return ::chdir(path); }
inline int   _mkdir(const char *path)       { return ::mkdir(path, (mode_t)0755); }
inline int   _rmdir(const char *path)       { return ::rmdir(path); }
