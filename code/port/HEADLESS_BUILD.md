# Headless core build (non-Windows / Apple Silicon)

This documents the drop-in Win32/MSVC shim route that lets OpenTS build on
macOS/arm64 under `OPENTS_EXPERIMENTAL_NONWIN32`. The Win32 build path is
byte-for-byte unchanged.

## Status (verified)

**The tree builds and links.** With the shim route in place:

```
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DOPENTS_EXPERIMENTAL_NONWIN32=ON
cmake --build build
```

produces `build/bin/Game`, a **Mach-O 64-bit executable arm64**, with
**0 errors** across all 513 build steps. `lib/libVQALib.a` and the language
library build alongside it, and bgfx/bx/bimg and miniaudio build from
`thirdparty/`.

Measured in stages, because each stage proves something different:

| Stage | Command | Result |
| --- | --- | --- |
| Parse | `clang++ -fsyntax-only` | **423 / 423** TUs, 0 errors |
| Codegen | `clang++ -c -O0` | **423 / 423** TUs, 0 errors |
| Link | force-loaded archive | **0 undefined symbols** |
| Run | `Run/Game -XC -DATADIR=…` | **Reaches the main menu and draws it** |

The TU count is 423, not 412. `code/CMakeLists.txt` uses `file(GLOB_RECURSE)`,
so the target also contains `code/audio/*.cpp` (10 TUs) — an earlier sweep that
globbed `code/*.cpp` non-recursively under-counted and silently left the whole
audio layer out. `code/port/` contributes one TU, `port_main.cpp`.

Codegen is measured separately from parsing because `-fsyntax-only` cannot see
static_assert failures, template instantiation errors or inline-asm rejects.

## Runtime status: the engine runs the game and draws it

`Run/Game -XC` now plays a real startup, loads its data, and sits in the game's
own main menu — **the menu is on screen**. The `-XC` switch asks the engine for
its debug console, which on this target is the terminal it was started from.

```
[10:00:10.224] Free disk space is 790740 Mb
[PORT] opents_window_create ENTER popup=1 (0x0)
[PORT] opents_window_create -> 0x79411c4500 borderless 640x480
[10:00:10.529] Audio: Beoplay A1, 48000 Hz, 3 x 480 frames
[PORT] bgfx::init returned 1
[10:00:10.534] Video: renderer is Metal
[10:00:10.562] Focus gained
[10:00:10.569] Allocating new surfaces
[10:00:10.569] CompositeSurface (472x480)
[10:00:10.569] TileSurface (472x480)
[10:00:10.570] SidebarSurface (168x480)
[10:00:10.570] HiddenSurface (640x480)
[10:00:10.570] AlternateSurface (640x480)
[10:00:10.570] Capture_Mouse()
[10:00:10.570] Main_Game
[10:00:10.570] Init Game
[10:00:10.570] Init Encryption Keys.
[10:00:10.570] Bootstrap..... PATCH.MIX EXPAND03.MIX EXPAND02.MIX EXPAND01.MIX CACHE.MIX
[10:00:10.633] Game Init Completed.
[10:00:10.633] Theme::PlaySong(30) - Repeating
```

The run proceeds through `Init Rules`, `Theater 0: TEMPERATE`, `Theater 1: SNOW`,
`Processing sides`, all four sides, `Reading THEME.INI`, `Init Commands`, and
`Game Init Completed`, then begins the main menu's music. Nothing reports an
error or an assertion anywhere in the 3900-line run.

### Verification that it is actually on screen

A window that "exists" is not a window anyone can see, so this is measured rather
than assumed. `CGWindowListCopyWindowInfo` reports window *metadata*, which needs
no Screen Recording permission:

```
$ /tmp/winlist3 <pid>
pid=9965 num=7876 layer=0 640x480 at (1400,480) alpha=1 onscreen=1  REAL
```

`onscreen=1` with a real size and `alpha=1` means the window server is
compositing it. Capturing that window and hashing it across two independent runs
gives **byte-identical** output, so the menu is not merely drawn once by
accident:

```
$ shasum -a 256 run1.png run2.png
47d12c3537abdd9f498f612047c93530eb9a9970f6236119da39f6a9b218259d  run1.png
47d12c3537abdd9f498f612047c93530eb9a9970f6236119da39f6a9b218259d  run2.png
```

The captured frame is the Tiberian Sun main menu — both crests, the Exit button
and the build stamp. It is the game's own artwork, converted from its palette and
presented through bgfx's Metal backend.

### The remaining boundary is game data, not the port

`Init_Bootstrap_Mixfiles()` requires `CACHE.MIX`, and startup stops without it.
The four archives logged before it — `PATCH.MIX` and the three expansion
archives — mount successfully, so the archive layer works; `CACHE.MIX` is
simply not present as a loose file in the data directory used for that run.
`CONQUER.MIX`, `SOUNDS.MIX` and `LOCAL.MIX` are required later in startup and
were absent from that copy too. `manual/content/formats/mix.md` lists the
required set.

**That turned out not to be a data problem at all.** The Steam build in
`Command.and.Conquer.Tiberian.Sun.v15918072` is version 2.03 — the exact patch
level this project targets — but it is restructured: the installer-extracted
caches are not loose, they are **members of `TIBSUN.MIX`**. `CACHE.MIX`,
`LOCAL.MIX`, `CONQUER.MIX`, `SOUNDS.MIX` and `SPEECH01.MIX` all live inside it,
and `SOUNDS01.MIX` and `ECACHE01.MIX` inside `EXPAND01.MIX`. The engine mounts
all of them by name without ever reading a loose file, which is exactly the path
that the LP64 read bug below was corrupting.

`code/port/tools/mixdump.py` is the tool that established this. It is
self-validating (`mixdump.py selftest` checks Blowfish against the published
vectors, the CRC against zlib and against the engine's own `CRCEngine`, and the
archive index against its appended SHA-1 digest), and a full sweep of the 8.4 GB
data set takes about 0.3 s.

Data can be read from somewhere other than the executable's directory, which
avoids copying several gigabytes next to the build:

```
Run/Game -XC -DATADIR=/path/to/tiberian-sun
Run/Game -XC -USERDIR=/path/to/writable
```

`-USERDIR` separates settings and saved games from shared, possibly read-only
data. Both are described in `manual/content/using/game-data.md`.

### Known issue: the missing archive was reported uncleanly, sometimes

With an incomplete data set the failure was **not always the clean one above**.
Roughly one run in three instead aborted inside the mixfile layer:

```
[08:58:01.570] Bootstrap.....libc++abi: terminating due to uncaught exception of type std::bad_alloc: std::bad_alloc
```

`lldb` places it exactly — `MixFileClass::MixFileClass(char const*, PKey const*)`
calling `operator new[]` — and the disassembly shows why:

```
0x1001a4620 <+344>: ldrsh  x22, [sp, #0x10]        ; x22 = fileheader.count
0x1001a4630 <+360>: umulh  x8, x22, x8
0x1001a4634 <+364>: add    x9, x22, x22, lsl #1
0x1001a4638 <+368>: lsl    x9, x9, #2              ; x9 = count * sizeof(SubBlock)
0x1001a4644 <+380>: bl     operator new[](unsigned long)
```

At the stop, `x22 = 0xffffffffffffa3c0` — a member count of **-23616**. The count
comes from the archive header, which is read without checking that the read
succeeded, so a header that could not be read is used as **whatever the stack
held**. `manual/content/formats/mix.md` documents this hazard upstream ("an
archive too short to hold a header is mounted from uninitialized memory");
this is the same hole reached through a different door.

The path is: `CACHE.MIX` is *not* loose, so
`CCFileClass::Is_Available()` answers from `MFCD::Offset()`, which finds a
`CACHE.MIX` **member inside `EXPAND01.MIX`** — the Firestorm archive carries its
own. The constructor then reads that member and gets nothing, and `Count` is
garbage. Whether the garbage count is small enough to allocate, or absurd enough
to throw, is what varies between runs.

Reproduce it in isolation, without touching the installed data:

```
mkdir -p /tmp/tsdata
ln -s /path/to/data/EXPAND01.MIX /tmp/tsdata/
Run/Game -XC -DATADIR=/tmp/tsdata      # aborts about 2 runs in 3
```

`EXPAND01.MIX` alone is enough; `TIBSUN.MIX`, `expand02.mix` and `expand03.mix`
alone are not. A data directory holding *only* `PATCH.MIX`, or nothing at all,
fails cleanly.

**Resolved — and it was never a data problem.** `RawFileClass::Read` reports how
many bytes it read through `ReadFile`'s `LPDWORD` out-parameter, casting the
caller's `int` to `DWORD &`. This shim declared `DWORD` as `unsigned long`, which
is 8 bytes here and 4 on Win32. The shim therefore wrote a 64-bit count over a
32-bit `int`, and every archive read returned 0 while still filling the buffer
correctly. The bytes were right and the count was wrong, which is precisely why
loose archives worked and archived members did not: the caller ignores the
return value on one path and tests it on the other. A non-loose `CACHE.MIX`
resolves through an already-mounted archive, so the member read silently
produced nothing and `Count` became stack garbage. See **Win32 integer widths**
below.

Diagnosing where a run stops is now cheap and does not need guesses: the engine
exits through `exit()`, so

```
lldb -b -o "breakpoint set -n exit" -o "run -XC" -o "bt 20" ./Game
```

stops at the call and prints the `WinMain` cold block that made it, which
identifies the failing gate directly. That is what located the
`DSurface::Create_Primary` boundary in one run. Be aware that the last lines of
`DEBUG_*.LOG` are buffered: a run stopped under the debugger, or killed, shows
fewer trailing lines than one that exits normally, and lines can appear out of
order relative to the port's own unbuffered `stderr` traces.

### Win32 integer widths: `long` is not 4 bytes here

The single most productive bug class in this port, and the one worth checking
first in any Windows codebase moved to LP64. Win32's `LONG`, `ULONG`, `DWORD`,
`HRESULT` and `WPARAM`-adjacent types are all **4 bytes**; here `long` is 8.
Declaring them with `typedef long` compiles everywhere and is silently wrong
wherever a value crosses a declaration boundary.

The original `com_stub.h` did exactly that, with a comment claiming "Win32 widths
preserved". Four defects came out of it, each of which presented as something
completely unrelated:

| Type alias | How it failed |
| --- | --- |
| `DWORD` (used as `ReadFile`'s out-count) | Wrote 8 bytes over a 4-byte `int`. **Every** `RawFileClass::Read` returned 0 — which is why no archive member could be read, and why the bootstrap failure looked like missing data. |
| `LONG` / `ULONG` | `SHADigest` is a union of `unsigned long Long[5]` and `unsigned char Char[20]`. At 8 bytes per element that union is 40 bytes, so the digest's finalisation rotated **ten** words instead of five and the compression function rotated 64-bit values. The engine's own SHA-1 then disagreed with the digest stored in the archive, so `MixFileClass::Cache` rejected every archive that carried one. Pinning these to `uint32_t` fixed SHA-1 outright. |
| `HRESULT` | `loco.h` overrides a base method with a covariant `LONG` return. That is legal on Win32 only because both are `long`; here the override stopped matching. `HRESULT` has to be 32-bit **and** the same type as `LONG` for that pattern to keep working. |
| `DWORD` again, in `std::min` | `std::min(255ul, <DWORD expression>)` deduced two different types. Revealed a latent bug in `dropship.cpp`, fixed at the call site. |

Two separate rules follow, and both are easy to get wrong:

- **Do not use `long` for a pointer-rounded value either.** `VQAMemoryHandler`
  in `vqa.cpp` returns `malloc` results as `(int)`, which round-trips on Win32
  where `int` is pointer-width and truncates here. The library's own default
  handler already used `(long)`, which is why only the engine's override broke —
  and only inside the movie decoder, where the faulting store address was a
  suspiciously 32-bit-looking `0x2f9dde00`.
- **Check who owns the contract before widening a handler.** Two callers passed
  different types to the same `VQACMD_SIZE` query (`long size` in `task.cpp`,
  `int fsize` in `loader.cpp`). The library's own handler writes `unsigned int`,
  so the 32-bit write *is* the contract and the `long` caller was the outlier;
  widening the handler would have overflowed `fsize`. Fixed at the caller.

One caution about `#ifdef BIG_ENDIAN`. `base64.cpp` selects its union layout with
it, which reads as a platform test but is not one: on macOS `<machine/endian.h>`
defines `BIG_ENDIAN` as the *byte-order constant* `4321`, so `#ifdef BIG_ENDIAN`
is **true on a little-endian arm64 Mac**. The base64 decoder then produced
correct-length, bit-shifted garbage, which meant the RSA public key was never
decoded, which meant `PKey::Plain_Block_Size()` was 0 — and `56 / 0` on AArch64
yields 0 rather than trapping, so the failure surfaced as a successful-looking
read of zero bytes. The port now uses `OPENTS_BIG_ENDIAN`, defined from
`__BYTE_ORDER__` in `port_early.h`. `lzo_conf.h` already namespaced its own
correctly.

### What running the menu does not mean

The binary starts, opens a composited window, brings up Metal, allocates its
surfaces, mounts every archive, loads its rules and both theaters, and renders
the main menu. It is still a long way from playable, and the gap is specific:

- **No input.** `opents_window_pump` drains Cocoa's queue and hands each event to
  `NSApp`, but nothing translates them into the port's message queue, so no
  `WM_KEYDOWN`, `WM_MOUSEMOVE` or `WM_LBUTTONDOWN` is ever produced. The menu
  draws and then waits. The captured frame is byte-identical over time for
  exactly this reason, which is expected rather than a stall — the stack shows
  the engine alive in `MSEngine::Wait_Delay`.
- **The 2-D drawing layer is mostly hollow.** Dialogs, owner-draw controls, the
  shell and the font system are parsed, compiled and linked and then do nothing.
  What is proven working is the path the menu happens to use.
- **Nothing past the menu is exercised.** No scenario has been started, so the
  simulation, the sidebar, the map and the in-game present path are untested.

The 100% CPU is the engine's own busy-wait, not a port defect: `MSEngine::Wait_Delay`
spins while calling back into the message handler, which is what the original
does on Windows too.

### Stubs that had to stop lying

Getting from "links" to "starts" — and then from "starts" to "runs the game"
— was never a matter of adding symbols. It was stubs that returned a plausible
answer while doing nothing, each of which silently broke something the engine
trusted. This is the general hazard of the hollow-stub approach: **a stub that
reports success is more dangerous than one that fails visibly**, because callers
do not check.

| Stub | Hollow answer | What it actually broke |
| --- | --- | --- |
| `GetCurrentThreadId` | `0` | `dbgprint.cpp` uses **0 as the "nobody owns the lock" sentinel**. A thread reporting id 0 looks like the owner, so every `Emit()` took the re-entrancy path and *discarded the message after formatting it*. The engine then spun at 100% CPU producing a log nobody could see. |
| `CreateMutex` | `nullptr` | `startup.cpp` claims the AutoPlay mutex in `do { ... } while (AutoPlayMutex == NULL)`. A null answer is an **unconditional infinite loop**. |
| `GetCommandLineW` | one-shot cache | Static initialisers read the command line *before* `main` runs, so a cache built on first call froze an empty string for the whole process — making every `-X` switch unreachable, including the console. Now keyed on the source pointer, and populated from the platform's own `argc`/`argv` (`_NSGetArgc`/`_NSGetArgv`) so it is valid before `main`. |
| `CreateDirectory` | `TRUE`, creating nothing | The engine then wrote its log into a directory that did not exist. Now really creates it, and reports success when it is already there (callers test `GetLastError() == ERROR_ALREADY_EXISTS`, and this target's `GetLastError` answers with `errno`, whose `EEXIST` is not 183). |
| `CreateFile` | no `CREATE_NEW` | `CREATE_NEW` and `OPEN_ALWAYS` were simply **not defined**, so they fell through to a plain `O_RDWR` that never created anything. Now both are defined with their winbase.h values, and `CREATE_NEW` maps to `O_CREAT\|O_EXCL`. |
| `GetLocalTime` | nothing | Every record stamped `00:00:00.000`, and the log named `DEBUG_00-00-0000_00-00-00.LOG`. |
| `LoadLibrary` | `nullptr` | `Init_Language_Resources` treats a missing `Language.dll` as an unrecoverable bad install: it shows "please reinstall Tiberian Sun" and `WinMain` returns before the game starts. Now loads for real, translating the Windows module name — `Language.dll` is built in-tree as `libLanguage.dylib` beside the executable. |
| `GetDiskFreeSpaceEx` | `TRUE`, filling nothing | Reported **0 MB free**, which sent the engine down its "critically low disk space" path. Now reads `statvfs`. |
| `GetClientRect` | a zero rectangle | `Win_Window_Drawable_Size` tests `width > 0 && height > 0`, so it answered *false* and `startup.cpp` took its `Video_Init` failure branch before `Video_Init` ever ran. This was the single reason the engine exited at the same place no matter what the windowing work did. Now reports the real window's drawable size. |
| `RegisterClass` | `0`, storing nothing | The engine's `Windows_Procedure` was never remembered, so nothing could be dispatched to it. Now captured for `DispatchMessage`. |
| `DispatchMessage` | `TRUE`, calling nothing | `GameInFocus` is only ever set by a `WM_ACTIVATEAPP` arriving here, so the startup focus loop could not end. |
| `PostMessage` / `ShowWindow` | `TRUE`, doing nothing | Messages the engine posts to itself were discarded and the window was never shown. |
| `CreateCompatibleDC` | returned its argument | Called as `CreateCompatibleDC(NULL)`, so it answered `NULL` and `DSurface`'s constructor bailed out before allocating — leaving `Create_Primary` to return `NULL` and `startup.cpp` to exit. |
| `CreateDIBSection` | `nullptr` | The surfaces have no pixels at all without it: `DSurface::Get_Buffer()` is the bitmap's memory. Now a real allocation with GDI's four-byte row alignment (see `port_gdi.h`). |
| `GetObject` | `0`, writing nothing | `DSurface` reads the row pitch back through this and falls back to `width * 2` when it fails, which is wrong for any padded width. |
| `GetFileAttributesA` | `FILE_ATTRIBUTE_NORMAL` for everything | Never reported `FILE_ATTRIBUTE_DIRECTORY`, so `Is_Directory()` said no to **every** directory and `-DATADIR` / `-USERDIR` rejected good paths as unusable. |
| `FindFirstFile` / `FindNextFile` | ignored the wildcard entirely | The directory scan mounted **every file as an archive** — a member count of `28483` was simply the ASCII `"Co"` of `DDrawCompat-game.log`. Two independent bugs: the pattern was never matched, and `dwFileAttributes` was left zero so the engine's own "skip directories" filter could never fire. |
| `PeekMessage` | read a queue nothing ever filled | The window was created and ordered front, and `GetClientRect` reported it correctly, but the application never drained Cocoa's queue — so the window server never composited it. `CGWindowListCopyWindowInfo` showed the window as **0x0, `onscreen=no`**. See **The event pump is the message pump** below. |
| path separators | appended `'\\'` | `Terminate_Path` and `CDFileClass::Set_User_Path` built paths ending in a backslash, which no POSIX `stat` resolves — the other half of the same `-DATADIR` failure. Now `OPENTS_PATH_SEPARATOR`. |

Two of these are worth calling out as a pattern rather than a bug:

* **A sentinel value is part of the contract.** `GetCurrentThreadId` returning 0
  was "obviously" fine for a stub, and was indistinguishable from the state the
  engine uses to mean *unowned*. Any stub returning 0/`nullptr`/`TRUE` should be
  checked against how the caller's own sentinel logic reads it.
* **A loop that waits for a resource will wait forever.** `do { ... } while (h ==
  NULL)` has no timeout and no escape; a stub that cannot supply the resource has
  to supply a convincing stand-in, not an error.

Diagnostic technique, in order of cost: `sample <pid>` to see which function the
CPU is actually in (this is what found the `DebugString` spin), then temporary
`write(2, ...)` traces inside the port stubs themselves to see which are reached
at all (this is what proved `AllocConsole` was never called), and finally the
`lldb` exit breakpoint above, which names the failing gate outright instead of
narrowing it. The port stubs are the right place to instrument — they need no
engine edits and rebuild in seconds.

## The window, renderer and surface backend

Four new units in `code/port/`, each with one job:

| File | Job |
| --- | --- |
| `portwindow/port_window.h` / `.mm` | The only Objective-C++ in the port. Creates the `NSWindow`, reports its drawable size and refresh rate, drains nothing else. |
| `port_bridge.h` | The C/C++ seam: the `OpentsMsg` queue, the registered window procedure, and the `opents_window_*` declarations the shim calls. |
| `port_msgqueue.cpp` | The queue itself, and the `Windows_Procedure` registry. |
| `port_gdi.h` / `.cpp` | Device contexts and bitmaps, enough for `DSurface` and the cursor loader. |

Three decisions worth keeping:

* **`HWND` is an `NSWindow *`.** bgfx's Metal backend takes an `NSWindow*`
  directly as `platformData.nwh` (it reads `.contentView` and attaches a
  `CAMetalLayer`), and `Win_Native_Window` already packed the handle into a
  `NativeWindow` unchanged, so backing `HWND` with the real window needed no
  engine change at all. The `__bridge_retained` transfer in
  `opents_window_create` is what keeps it alive until `opents_window_destroy`.
* **The Objective-C++ lives in its own static library.** The engine's
  translation units are compiled with `-fms-extensions` and `-fdeclspec` to
  make MSVC-flavoured headers parse; those same flags break Objective-C
  parsing. `opents_portwindow` is built without them, with `-fobjc-arc`, and
  linked in. Keeping the ObjC behind a C-linkage header is what allows that.
* **The message pump is a real queue.** `CreateWindowEx` seeds
  `WM_ACTIVATEAPP` and `WM_SIZE` for the window it just made, the pump reads
  them back through `PeekMessage` / `GetMessage`, and `DispatchMessage` calls
  the procedure `RegisterClass` captured. Without all four of those the engine
  waits forever on focus.
* **`PeekMessage` also drains the platform queue.** Two queues are in play, not
  one. See the next section.

### The event pump is the message pump

Two queues are in play here, and for a while only one of them was being read.

The engine's queue is the port's own — `OpentsMsg` in `port_msgqueue.cpp`. It
carries what the shim puts there: `WM_ACTIVATEAPP`, `WM_SIZE` and anything the
engine posts to itself. Reading it is what releases the startup focus loop.

Cocoa's queue is separate, and **nothing was reading it**. On macOS a window is
not composited by the window server merely because it was created and ordered
front; the application has to process its event queue first. Until it does, the
window exists — `CGWindowListCopyWindowInfo` lists it — but reports **0x0** bounds
and `onscreen=no`, and the app never becomes active.

Nothing in the engine's own logs hints at this. They looked entirely healthy: the
window was created, `GetClientRect` reported 640x480, bgfx brought up Metal, and
the game ran through bootstrap, rules, both theaters and into its main menu. It
was drawing to nothing anyone could see. A standalone probe made the requirement
explicit:

```
stage                         window-server view
initWithContentRect 640x480      0x0     -
makeKeyAndOrderFront             0x0     -
  + pump                        640x480  ONSCREEN
```

`finishLaunching` and `activateIgnoringOtherApps:` were measured as well, and made
no difference either way — the pump is the entire requirement.

The fix is one line, and it belongs in the shim rather than the engine, because
the Win32 contract already means this: on Windows `PeekMessage` **is** how an
application receives events from the operating system, so that is where the
platform queue has to be drained.

```cpp
inline BOOL PeekMessage(MSG *lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {
	/* ... Windows delivers events through here, so drain the platform queue ... */
	opents_window_pump();
	...
}
```

The stack afterwards is exactly what it should be — the engine's own message loop
driving Cocoa's:

```
MSEngine::Wait_Delay -> Windows_Message_Handler -> opents_window_pump
  -> -[NSApplication nextEventMatchingMask:...] -> _DPSNextEvent
     -> RunCurrentEventLoopInMode
```

`opents_window_pump` passes `[NSDate distantPast]`, so a call with nothing pending
returns immediately rather than blocking. The engine calls `PeekMessage` several
times per frame from every path that has to stay responsive, including the menu
engine's own delay loop, so no separate timer or thread is needed.

The same function is the natural place to translate events later: it currently
hands each `NSEvent` to `NSApp` and lets it go, which is why the menu draws and
then waits. Converting `NSEvent` into the port's queue is what will make the game
respond to a keyboard.

## Force-included headers are invisible to the build

`windows_stub.h` reaches every translation unit through `-include
port_early.h`, which means the compiler's own dependency scan never records it:
it is not in any `#include` line the scan follows. Ninja therefore has no
reason to rebuild anything when the shim changes, and **the result is a
silently stale binary** — edits to the shim appear to have no effect, which
reads exactly like a fix that did not work. This cost real time twice before it
was recognised.

`code/CMakeLists.txt` now declares the dependency explicitly:

```cmake
set(OPENTS_FORCE_INCLUDED_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/port/port_early.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/windows_stub.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/com_stub.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/port_bridge.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/port/port_gdi.h"
)
foreach(f ${OPENTS_SRC})
    if(f MATCHES "\\.(cpp|cc|cxx|mm)$")
        set_property(SOURCE "${f}" APPEND PROPERTY OBJECT_DEPENDS ${OPENTS_FORCE_INCLUDED_HEADERS})
    endif()
endforeach()
```

A shim edit now rebuilds all 438 objects. Any new force-included header has to
be added to that list, or it inherits the same silent staleness.

## The link surface (the useful finding)

Force-loading every object into one link — a static archive is linked lazily, so
without `-Wl,-force_load` ld64 reports only the missing entry point and looks
like it succeeded — gives the exact set an external dependency or a backend must
supply. It came down to five buckets:

| Bucket | Count | Resolution |
| --- | --- | --- |
| bgfx / bx / bimg | 35 | vendored in `thirdparty/`, builds on macOS |
| miniaudio | 25 | vendored, `thirdparty/miniaudio-impl.c` |
| CoreMedia / VideoToolbox | 13 | bgfx's Metal video decoder; add the frameworks |
| COM interface IDs | 7 | **defined in `com_stub.h`** (see below) |
| Entry point | 1 | **`code/port/port_main.cpp`** |

**Zero Win32 symbols were undefined.** That is the headline: after the shim,
there is no longer any Win32 import the linker is waiting for. The port surface
is not "some large number of Win32 calls"; it is the hollow *behaviour* behind
those calls.

### The COM interface IDs are load-bearing

`IID_IUnknown`, `IID_IStream`, `IID_IPersist`, `IID_IPersistStream`,
`IID_IPropertySetStorage`, `IID_IPropertyStorage`, `IID_IClassFactory` and
`FMTID_SummaryInformation` reach the Windows build from `uuid.lib`, which is why
`code/CMakeLists.txt` marks the MIDL-generated `*_i.c` files `HEADER_FILE_ONLY`:
those files define the engine's *own* interfaces, and the standard ones are
simply expected to arrive from the platform. There is no `uuid.lib` here, so
`com_stub.h` now defines them with their well-known values.

The values matter at run time, not just at link time: `QueryInterface` compares
them. A placeholder GUID would compile, link, and then make every request for
`IStream` or `IPersistStream` fail — which is precisely the kind of silent
breakage a stub exists to avoid. They are `inline` because this header is
force-included into every TU; a plain external definition would be emitted once
per TU and collide at link.

### The entry point

The engine's entry point is `WinMain`, reached on Windows through the
`/SUBSYSTEM:WINDOWS` loader stub, which supplies the instance handle, the
command line and the show command. None of that exists here. `port_main.cpp`
supplies `main` and forwards to `WinMain`; it is the only file in the port that
adds a translation unit rather than a header, because an entry point has to be a
real external symbol rather than an inline one. The whole file is empty unless
`OPENTS_EXPERIMENTAL_NONWIN32` is defined, so the supported build compiles
exactly what it compiled before.

## Force-include is the key mechanism

`_opents_com_ptr` / `stricmp` / `_makepath` / `_MAX_*` / `_CONTROL` / the
`DEG_TO_RAD` family are used inside engine **templates** (two-phase name lookup)
and in files that never include `<windows.h>`. They must therefore be visible
*before* any engine template is defined. `code/port/port_early.h` is
force-included first (`-include`) on every non-Windows TU; it pulls in
`port_string_shim.h` and then `windows_stub.h`, so those names resolve
regardless of include order.

## Toolchain divergences handled in the port layer

Each is fixed inside `code/port/`, with no engine `#include` line changed.

* **`code/visualc.h` math block.** visualc.h wraps *all* of `M_E`..`M_FPI` and
  `RAD_TO_DEG`/`DEG_TO_RAD`/`RAD_TO_DEGF`/`DEG_TO_RADF` in one
  `#if !defined(M_PI)` guard. That is only sound on MSVC, where those constants
  appear solely if `_USE_MATH_DEFINES` precedes `<math.h>`: either all are
  present or all are absent. On libc++ `<math.h>` defines `M_PI` (and can be
  pulled in very early, e.g. by `<chrono>`/`<string>`), so the guard evaluated
  false and the degree-conversion macros silently vanished — breaking every TU
  that uses `DEG_TO_RAD`. `port_early.h` restores visualc.h's stated intent by
  defining each entry under its **own** guard.
* **SAL annotations.** The stub must not define `__in`/`__out`/`__inout`/`__opt`
  as empty macros: libc++ uses `__opt` as a parameter name inside
  `<filesystem>`, so an empty `#define __opt` corrupts that system header
  ("expected expression"). Only the uppercase `IN`/`OUT`/`OPTIONAL` forms are
  defined; nothing in the tree uses the `__`-prefixed annotations.
* **`GUID` type tag.** `com_stub.h` defines `typedef struct _GUID { ... } GUID;`.
  The tag **must** be `_GUID` (as in Windows `guiddef.h`), because some engine
  TUs spell out-of-line definitions as `... (struct _GUID const &guid, ...)`; a
  `GUID` tag would make `struct _GUID` a *different* incomplete type and the
  out-of-line `QueryInterface` would not match its declaration.
* **`timeb` vs `_timeb`.** macOS `<sys/timeb.h>` already provides `struct timeb`
  and `ftime()`; defining them again in the stub collides. The stub includes
  `<sys/timeb.h>` and defines only the MSVC `_timeb`/`_ftime` variants.
* **`O_BINARY` and `filelength`.** POSIX has no text/binary mode distinction, so
  MSVC's `open()` flag is a no-op; `filelength(fd)` is `fstat`, which — unlike
  an `lseek` to the end and back — leaves the read position untouched. Both are
  reached from `vqalib/dstream.cpp`, a subproject TU.
* **`MSGBOXPARAMS` strings.** Real Win32 uses `LPCTSTR`, not `LPWSTR`, for
  `lpszText`/`lpszCaption`/`lpszIcon`. The engine is built in the narrow
  character set, so these are `LPCTSTR` -> `const char*`.
* **MSVC aligned allocation.** `_aligned_malloc` / `_aligned_free` /
  `_aligned_realloc` / `_aligned_msize` are backed by `posix_memalign`, with a
  small header stored in the alignment slack so the original block and its size
  stay recoverable.
* **`_opents_com_ptr` converting constructor.** MSVC's `_com_ptr_t` converts
  from *any* interface pointer (routing through `QueryInterface`), which the
  engine relies on to wrap a base game-object pointer (e.g. `FootClass*`) as an
  `IFlyControlPtr` even though no static derivation exists. The stub mirrors
  that with a pointer-reinterpreting template constructor.

## Route

`code/port/shim/` holds drop-in headers placed **first** on the include path
when `OPENTS_EXPERIMENTAL_NONWIN32` is set:

* `windows.h`  -> `code/port/windows_stub.h` (which itself includes `com_stub.h`)
* `comdef.h`, `unknwn.h`, `objidl.h`, `objbase.h` -> `code/port/com_stub.h`
* `mmsystem.h`, `windowsx.h`, `winnt.h`, `winuser.h`, `commctrl.h`, `shellapi.h`,
  `basetyps.h`, `windef.h` -> windows_stub.h
* `io.h`, `direct.h`, `share.h`, `dos.h`, `conio.h`, `float.h` -> POSIX/hollow
  shims
* `new.h` -> `<new>`; `intrin.h` -> minimal intrinsics shim

`com_stub.h` includes `windows_stub.h` at its end, so a TU that reaches COM
through its own headers (e.g. `ini.cpp` via `ini.h`) but never includes
`<windows.h>` still sees the Win32 surface. `port_early.h` also force-includes
`windows_stub.h`, which makes the coverage unconditional — needed by
`bgfxbackend.cpp`, which includes neither `<windows.h>` nor any header that
pulls it in, yet uses `OutputDebugString` / `_aligned_*`.

## What the CMake wiring had to add

The shim is reached from the project's own build system now. Four things were
not obvious from the flags alone:

1. **`option()` is not a compile definition.** The port headers guard on the
   `OPENTS_EXPERIMENTAL_NONWIN32` *macro*, but `option()` only sets a CMake
   variable, which the compiler never sees. The root `CMakeLists.txt` calls
   `add_compile_definitions()` for it, scoped to the engine tree.
2. **VQALib needs the shim too.** It is linked into the same executable, so it
   has to agree on the shim and on where `<windows.h>` resolves. Its own
   `CMakeLists.txt` also defined `WIN32`/`_WINDOWS` unconditionally, which would
   have hidden the `#if !defined(_WIN32)` implementations — now guarded by
   `if(WIN32)`.
3. **Apple frameworks.** bgfx's Metal backend wants Cocoa, QuartzCore, Metal,
   IOKit and CoreVideo; its Metal video decoder wants CoreMedia and
   VideoToolbox; miniaudio wants CoreAudio and AudioToolbox. All `find_library`
   + `REQUIRED`.
4. **The tests are skipped.** 33 of the 34 test directories pass `/EHsc`, `/MT`
   and `/arch:SSE2`, and most link `kernel32` or `ws2_32`. They exist to test
   the engine on the platform it ships for, so porting them is a separate pass
   from porting the engine; leaving them in would fail a full build on a target
   the engine itself now builds cleanly on. The experimental configuration
   builds the engine, VQALib and the language library only, and says so.

`bgfxbackend.cpp` keeps its own include paths, scoped to that one TU, because
`bx/include/compat/osx` ships a `malloc.h` that would shadow the system header
everywhere else.

## Next steps

1. **Wire input.** This is now the single thing standing between the current
   state and a playable game. The engine reaches its main menu and renders it,
   then waits, because nothing translates Cocoa events into the port's own queue.
   `opents_window_pump` is the right place — it already sees every `NSEvent` and
   currently hands it to `NSApp` and lets it go. `SetCursor`, `GetCursorPos`,
   `GetKeyState`, `SetCapture` and `CreateIconIndirect` (still `nullptr`) are all
   no-ops behind it.
2. **Make the rest of the 2-D layer draw.** The surfaces hold real pixels and the
   menu's own path works, but `TextOut`, the font objects, the owner-draw
   controls and the dialog machinery are still hollow, so anything the menu does
   not happen to exercise cannot be seen. `GUI_BACKEND_SCOPE.md` scopes it.
3. **Complete the data set, then start a scenario.** The Steam layout supplies
   every archive the engine mounts, but not the loose caches some later paths
   name directly. No scenario has been started, so the simulation, the map, the
   sidebar and the in-game present path are all untested.
4. **Replace hollow COM persistence** (`Ole*`/`Co*`/`Stg*`) with a native
   stream. This also fixes the pointer-width save break: raw pointers are
   serialized, so cross-platform saves are impossible while that stands.
5. **Honour `-Wshorten-64-to-32`.** The sweep compiles with it to surface the
   LP64 narrowing sites that Win32's 32-bit pointers hid. The `long`-is-not-4-bytes
   section above is that worklist already paying off; the remaining warnings are
   the rest of it.
6. **Port the test harnesses** (34 directories) off MSVC flags, so the suite can
   run on the unsupported target too.
