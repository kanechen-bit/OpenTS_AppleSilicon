# OpenTS → Apple Silicon (arm64): Win32 API dependency inventory

This is the feasibility map of the **Win32 API surface** — the largest remaining
port gate after the COM layer (Piece A/B) was stubbed. Counts are grep call-site
occurrences across `code/`. The goal is to quantify the dependency so the
adaptation path and backend decision can be made with data, not guesswork.

## Headline numbers

- ~1,000+ Win32 API call sites in `code/`.
- The surface is **overwhelmingly the GUI / dialog layer**, not the simulation core.
- The core only needs a small set of POSIX-mappable primitives.

## Call-site counts (top symbols)

| Symbol | Sites | Layer | Shim strategy |
|---|---:|---|---|
| SendMessage | 179 | GUI | hollow no-op (needs real backend) |
| _makepath | 67 | file path (CRT) | POSIX `snprintf`/`mkpath` |
| InvalidateRect | 57 | GUI | hollow no-op |
| ShowWindow | 44 | GUI | hollow no-op |
| GetWindowLong | 35 | GUI | hollow (window-long store) |
| wsprintf | 33 | string (CRT) | `snprintf` |
| GetClientRect | 28 | GUI | hollow |
| UpdateWindow | 27 | GUI | hollow |
| Sleep | 27 | timing | `usleep` |
| GetDC / ReleaseDC | 19 / 14 | GDI | hollow |
| GetLastError | 18 | error | real (errno) |
| ReleaseCapture / SetCapture | 17 / 11 | GUI | hollow |
| SelectObject | 14 | GDI | hollow |
| FindFirstFile / CreateFile / FindClose | 11 / 11 / 10 | file | POSIX `opendir`/`open` |
| SetCursor / PostMessage | 9 / 8 | GUI | hollow |
| FindNextFile / WriteFile / SetTimer / SetTextColor / SetBkMode | 8 / 7 / 7 / 7 / 7 | mixed | mixed |
| MultiByteToWideChar / WideCharToMultiByte | 6 / 6 | text | UTF-8 passthrough |
| MessageBox | 6 | GUI | hollow (log) |
| GetModuleFileName | 6 | path | stub (returns 0) |
| DefWindowProc / CreateWindowEx | 6 / 5 | GUI | hollow |
| RegisterClass / DispatchMessage / TranslateMessage / PeekMessage / GetMessage | 2–5 | GUI msg loop | hollow |

Full set also includes `ReadFile` (4), `GetCursorPos` (4), `CreateFileA` (4),
`TextOut` (3), `QueryPerformanceCounter` (3), `OutputDebugString` (3),
`LoadLibrary`/`GetProcAddress` (2), `GetFileAttributes` (2), `StretchBlt`,
`Rectangle`, `LoadIcon`, `GetTickCount`, `DrawText`, `CreateFont(Indirect)`, etc.

## Type usage (top symbols)

| Type | Uses | Notes |
|---|---:|---|
| HWND | 523 | window handle — opaque `void*` in stub |
| LPARAM | 293 | 64-bit on LP64 (`intptr_t`) |
| WPARAM | 193 | 64-bit on LP64 (`uintptr_t`) |
| RECT | 128 | struct |
| LRESULT | 105 | 64-bit on LP64 |
| LPSTR | 77 | `char*` |
| COLORREF | 59 | `uint32_t` |
| POINT | 50 | struct |
| HANDLE | 35 | opaque `void*` |
| SIZE | 33 | struct |
| HDC | 32 | GDI context — opaque |
| HFONT | 20 | GDI — opaque |
| HINSTANCE | 11 | opaque |
| MSG / HTREEITEM / HCURSOR / HACCEL | 6–9 | structs / opaque |

## Files by Win32 density (the GUI hotspot)

| File | Win32 sites | Role |
|---|---:|---|
| ownrdraw.cpp | 198 | owner-draw list / control rendering |
| netdlg2.cpp | 33 | network dialog |
| netshare.cpp | 24 | network share dialog |
| objtype.cpp | 16 | object type browser |
| builtype.cpp | 15 | building type UI |
| goptions.cpp / desyncdlg.cpp | 14 / 14 | options / desync dialog |
| winfix.cpp / winstub.cpp | 13 / 11 | Win32 glue |
| options.cpp / init.cpp / windlg.cpp | 11 / 10 / 9 | options / init / dialogs |

These are the files that genuinely need a **UI backend** (SDL2 / Qt / Cocoa)
rather than a no-op shim.

## Strategic conclusion

1. **Core simulation** depends on a *small, POSIX-mappable* Win32 subset
   (`_makepath`, `wsprintf`, file I/O, timing, text conversion). A headless
   build is achievable with a thin `windows_stub.h` that wraps these to POSIX
   and defines the handle/struct types opaquely.
2. **The GUI layer** (`ownrdraw`, dialogs) cannot be satisfied by a no-op shim —
   it needs a real backend. This is the architectural fork: **headless core
   first** (proves the engine logic compiles & runs logic-only) vs. **full GUI
   backend** (SDL2/Qt) up front.
3. The COM layer (Piece A/B) is now fully stubbed; the Win32 shim is the next
   gate. `windows_stub.h` is laid down as a foundation (types + core APIs real,
   GUI APIs hollow) and expanded incrementally per-TU as real compilation is
   attempted.

## Next gates
- **windows_stub.h** — types + core (non-GUI) APIs real, GUI APIs hollow
  (foundation committed; expand per-TU).
- **GUI backend decision** — headless core vs. SDL2/Qt. Drives whether the
  dialog files get a real implementation or stay hollow.
- **Piece C** — replace the hollow COM persistence layer with a native stream
  (also fixes the pointer-width save break).
