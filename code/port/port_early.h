/*******************************************************************************
 *                                O P E N  T S
 ******************************************************************************/
/* Port shim: force-included FIRST in every non-Windows TU (via the build's
 * -include flag under OPENTS_EXPERIMENTAL_NONWIN32). It declares the helpers
 * that must be visible before any engine template is defined, so two-phase
 * name lookup at template-definition time succeeds for calls like
 * stricmp()/strupr()/strnicmp() and the MSVC _MAX_* path limits.
 *
 * windows_stub.h pulls in the same declarations (guarded) later, so there is
 * no duplication. Keep this file minimal and include-only. */
#include "port_string_shim.h"

/* The directory separator this port builds paths with.
 *
 * The engine's file layer reads either separator happily -- CDFileClass::
 * Has_Directory treats '\\', '/' and ':' alike -- but a path the port
 * CONSTRUCTS has to end in the one the host filesystem uses. Appending '\\'
 * produces a name no POSIX open() or stat() resolves, which is what made
 * -DATADIR and -USERDIR report a perfectly good directory as unusable. */
#if defined(OPENTS_EXPERIMENTAL_NONWIN32)
#define OPENTS_PATH_SEPARATOR '/'
#else
#define OPENTS_PATH_SEPARATOR '\\'
#endif

/* True only on a big-endian target.
 *
 * The engine picks several byte-order-dependent memory layouts with
 * `#ifdef BIG_ENDIAN` -- code/base64.cpp packs its 3-byte/4-char packet union
 * that way. That test is NOT portable. BSD and macOS <endian.h> define
 * BIG_ENDIAN as a *value* (4321), sitting next to LITTLE_ENDIAN (1234), so
 * `#ifdef BIG_ENDIAN` is TRUE on a little-endian arm64 Mac and base64.cpp
 * selects the big-endian bit layout.
 *
 * The damage is not confined to base64: Init_Keys() decodes the game's public
 * key through Base64Pipe, so a scrambled decode yields an empty RSA modulus,
 * `BitPrecision` becomes 0, and PKStraw cannot recover the Blowfish key. Every
 * ENCRYPTED mixfile header (TIBSUN.MIX, PATCH.MIX, EXPAND01.MIX ...) then
 * reads as garbage -- Count in the millions -- and the engine aborts in
 * Bootstrap. Plain mixfiles were unaffected, which is why only the archives
 * carrying the real game data failed.
 *
 * Ask the compiler rather than a header constant. */
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
	(__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define OPENTS_BIG_ENDIAN 1
#else
#define OPENTS_BIG_ENDIAN 0
#endif

/* Make the Win32 surface visible to EVERY translation unit, not just those that
 * happen to `#include <windows.h>` somewhere in their own include graph.
 * Most engine TUs reach windows_stub.h through the <windows.h> shim, but a few
 * (e.g. bgfxbackend.cpp, which only includes bgfx/bx and two engine headers)
 * never do, yet still reference Win32 APIs such as OutputDebugString().
 * windows_stub.h is guarded and self-sufficient, so pulling it in here is
 * idempotent and merely makes the existing coverage unconditional. */
#include "windows_stub.h"

/* MSVC <stdlib.h>/<stdio.h> define these path limits; the engine uses them in
 * files that never include <windows.h>. Guard so a later <windows.h> (which
 * also defines them) does not re-define. */
#ifndef _MAX_PATH
#define _MAX_PATH    260
#define _MAX_DRIVE   3
#define _MAX_DIR     256
#define _MAX_FNAME   255
#define _MAX_EXT     256
#define _MAX_BASE    255
#endif

/* Math constants + degree/radian conversions.
 *
 * code/visualc.h supplies these, but wraps the WHOLE set in a single
 * `#if !defined(M_PI)` guard. That is only sound on MSVC, where the constants
 * appear solely if _USE_MATH_DEFINES precedes <math.h>: either every constant
 * is absent, or every one is present. On this (libc++) toolchain <math.h>
 * defines M_PI -- and it can be pulled in very early, e.g. by <chrono>/<string>
 * -- so the guard evaluates false and visualc.h's block is skipped entirely,
 * silently dropping M_FPI, RAD_TO_DEG, DEG_TO_RAD, RAD_TO_DEGF and DEG_TO_RADF.
 * Any TU that includes <cmath> before visualc.h then fails to parse.
 *
 * visualc.h's own comment states the intent: "Each is guarded so a toolchain
 * whose headers already provide it keeps its own." Restore that intent here by
 * guarding every entry individually, before anything else is included. */
#ifndef M_E
#define M_E         2.71828182845904523536
#endif
#ifndef M_LOG2E
#define M_LOG2E     1.44269504088896340736
#endif
#ifndef M_LOG10E
#define M_LOG10E    0.434294481903251827651
#endif
#ifndef M_LN2
#define M_LN2       0.693147180559945309417
#endif
#ifndef M_LN10
#define M_LN10      2.30258509299404568402
#endif
#ifndef M_PI
#define M_PI        3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2      1.57079632679489661923
#endif
#ifndef M_PI_4
#define M_PI_4      0.785398163397448309616
#endif
#ifndef M_1_PI
#define M_1_PI      0.318309886183790671538
#endif
#ifndef M_2_PI
#define M_2_PI      0.636619772367581343076
#endif
#ifndef M_1_SQRTPI
#define M_1_SQRTPI  0.564189583547756286948
#endif
#ifndef M_2_SQRTPI
#define M_2_SQRTPI  1.12837916709551257390
#endif
#ifndef M_SQRT2
#define M_SQRT2     1.41421356237309504880
#endif
#ifndef M_SQRT_2
#define M_SQRT_2    0.707106781186547524401
#endif
/* Single precision pi, for the float paths that would otherwise round M_PI at every use. */
#ifndef M_FPI
#define M_FPI 3.141592654f
#endif
#ifndef RAD_TO_DEG
#define RAD_TO_DEG(x)	(((double)x)*180.0/M_PI)
#endif
#ifndef DEG_TO_RAD
#define DEG_TO_RAD(x)	(((double)x)*M_PI/180.0)
#endif
#ifndef RAD_TO_DEGF
#define RAD_TO_DEGF(x)	(((float)x)*180.0f/M_PI)
#endif
#ifndef DEG_TO_RADF
#define DEG_TO_RADF(x)	(((float)x)*M_PI/180.0f)
#endif
