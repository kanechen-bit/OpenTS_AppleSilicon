#pragma once
/*
 * Shim <io.h> (MSVC low-level IO) for the non-Windows experimental OpenTS
 * build. OpenTS uses a small subset; the rest maps to POSIX in <fcntl.h> /
 * <unistd.h>. Flags mirror the MSVC _O_* constants; _O_BINARY/_O_TEXT are
 * no-ops on POSIX.
 */
#if !defined(OPENTS_EXPERIMENTAL_NONWIN32)
#error "code/port/shim/io.h may only be included under OPENTS_EXPERIMENTAL_NONWIN32"
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <cstdarg>

#ifndef _O_RDONLY
#define _O_RDONLY   O_RDONLY
#define _O_WRONLY   O_WRONLY
#define _O_RDWR     O_RDWR
#define _O_APPEND   O_APPEND
#define _O_CREAT    O_CREAT
#define _O_TRUNC    O_TRUNC
#define _O_EXCL     O_EXCL
#define _O_BINARY   0
#define _O_TEXT     0
#define _O_TEMPORARY 0
#endif

#define _A_NORMAL 0x00
#define _A_RDONLY 0x01
#define _A_HIDDEN 0x02
#define _A_SYSTEM 0x04
#define _A_SUBDIR 0x10
#define _A_ARCH   0x20

inline int    _access(const char *path, int mode) { return ::access(path, mode); }
inline int    _chmod(const char *path, int mode)  { return ::chmod(path, mode); }
inline int    _open(const char *path, int flags, int mode = 0) { return ::open(path, flags, (mode_t)mode); }
inline int    _close(int fd)                       { return ::close(fd); }
inline long   _lseek(int fd, long offset, int origin) { return (long)::lseek(fd, (off_t)offset, origin); }
inline long   _tell(int fd)                        { return (long)::lseek(fd, 0, SEEK_CUR); }
inline int    _read(int fd, void *buf, unsigned int count)  { return (int)::read(fd, buf, count); }
inline int    _write(int fd, const void *buf, unsigned int count) { return (int)::write(fd, buf, count); }
inline int    _unlink(const char *path)           { return ::unlink(path); }
