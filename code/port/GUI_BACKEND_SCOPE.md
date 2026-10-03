# GUI / runtime backend scoping — OpenTS Apple-Silicon port

Scope of the 39 "heavy" translation units (TUs) that fail to compile under the
headless Win32 shim route, plus a backend recommendation and effort estimate.
Companion to `HEADLESS_BUILD.md`.

## Bottom line

The 39 remaining TUs are **not** 39 backend-integration tasks. Their
**~1,405 errors collapse to ~100 distinct missing Win32 symbols** — almost all
of them `commctrl`/`windowsx` *macros* (e.g. `Button_GetCheck`,
`ListBox_AddString`, `ComboBox_GetCurSel`, `SB_THUMBTRACK`, `BST_CHECKED`),
window-style/position *constants* (`GWL_STYLE`, `WS_DISABLED`, `SWP_NOZORDER`,
`TRANSPARENT`), and ~10 opaque *structs* (`SCROLLINFO`, `BITMAPINFO`,
`WIN32_FIND_DATA`, `LPCDLGTEMPLATE`, `PROPSPEC`, `IPropertyStoragePtr`).

That splits the work into two clean phases:

- **Phase 1 — Headless full-tree compile (stub closure).** Add the ~100 missing
  symbols as macros / constants / opaque structs to the shim. **No backend
  needed.** Result: 412/412 TUs *parse* at C++20 (enables CI / port
  validation), but the game still cannot *run* — the macros are hollow.
- **Phase 2 — Real runtime backend (run the game).** Implement windowing,
  input, audio, and 2-D rendering against a real library. This is the actual
  product and where the engineering effort lives.

## Evidence

True (uncapped) error counts for the 39 TUs, dominant subsystem from keyword
profiling (W=window/dialog, G=GDI/bitmap, C=commctrl/owner-draw,
D=ddraw/surface, P=COM-persist, T=misc-types):

| TU            | errors | dominant subsystem |
|---------------|-------:|--------------------|
| ownrdraw.cpp  |    341 | G (owner-draw/GDI) |
| netdlg2.cpp   |    145 | W + C (dialog)     |
| winfix.cpp    |     92 | C (commctrl)       |
| loaddlg.cpp   |     76 | W + C (dialog)     |
| netshare.cpp  |     70 | W + C (dialog)     |
| mapgen.cpp    |     65 | W (dialog)         |
| skirmish.cpp  |     53 | W + C (dialog)     |
| dbgprint.cpp  |     51 | T (SRWLOCK)        |
| savever.cpp   |     46 | P (COM-persist)    |
| desyncdlg.cpp |     46 | W + C (dialog)     |
| windlg.cpp    |     38 | W + G (dialog)     |
| options.cpp   |     38 | W + C (dialog)     |
| startup.cpp   |     36 | W + P (COM)        |
| winstub.cpp   |     30 | W (window)         |
| queue.cpp     |     29 | W + C (dialog)     |
| init.cpp      |     26 | W + C (dialog)     |
| sounddlg.cpp  |     25 | W + C (dialog)     |
| mpu.cpp       |     17 | T (LARGE_INTEGER)  |
| dsurface.cpp  |     16 | D + G (surface)    |
| mainopt.cpp   |     14 | W + C (dialog)     |
| gamedlg.cpp   |     14 | W (dialog)         |
| syncrechook.cpp |   12 | T (IMAGE_DOS)      |
| wincursor.cpp |     10 | T (BITMAPINFO)     |
| msgroute.cpp  |     10 | W                  |
| fly.cpp       |     10 | P (COM interface)  |
| srfcache.cpp  |      9 | T + D (bitmap)     |
| msgbox.cpp    |      9 | W (dialog)         |
| cdfile.cpp    |      9 | T (FindFirstFile)  |
| tactical.cpp  |      8 | D + G (surface)    |
| gamedirs.cpp  |      8 | T (CreateDirectory)|
| data.cpp      |      8 | T (HRSRC)          |
| conquer.cpp   |      8 | W (MSGBOXPARAMS)   |
| ini.cpp       |      7 | T (CP_ACP)         |
| cstream.cpp   |      7 | P (COM-persist)    |
| saveload.cpp  |      5 | P (LPPERSISTSTREAM)|
| keyboard.cpp  |      5 | W                  |
| scroll.cpp    |      4 | C (SB_*)           |
| gscreen.cpp   |      4 | D (surface)        |
| goptions.cpp  |      4 | W + C (dialog)     |

Top distinct missing symbols (count = # of TUs referencing, from the uncapped
sweep): `SendDlgItemMessage` (72), `BST_CHECKED` (40), `ListBox_AddString` (32),
`SetFocus` (25), `CB_INSERTSTRING` (25), `Button_GetCheck` (23), `GWL_STYLE`
(22), `CallWindowProc` (21), `SetWindowText`/`BM_SETCHECK`/`BM_GETCHECK` (16),
`LowPart` (15), `GWL_ID` (15), `CB_GETCOUNT` (13), `LB_GETCOUNT` (12),
`DestroyWindow` (12), `CB_SETCURSEL` (12), `BST_UNCHECKED` (12),
`ListBox_GetCount` (11), `BN_CLICKED` (11), `SCROLLINFO` (10),
`GetTextExtentPoint32` (10), `WIN32_FIND_DATA` (9), `MAKEWPARAM` (9),
`ListBox_*`/`ComboBox_*` families, `FindFirstFile`/`FindNextFile` (8),
`BITMAPINFO` (6), `SB_THUMBTRACK` (5), `LPCDLGTEMPLATE` (5), `PROPSPEC` (6),
`IPropertyStoragePtr` (6), `LARGE_INTEGER.HighPart` (6) ...

Subsystem map of the 39:

- **Dialog / options UI cluster (~18 files):** skirmish, mapgen, netdlg2,
  netshare, gamedlg, sounddlg, options, goptions, desyncdlg, loaddlg, msgbox,
  winstub, windlg, startup, queue, mainopt, keyboard, init. These are the menu /
  setup / multiplayer-dialog layer. Dominated by `commctrl` + window macros.
- **Owner-draw / GDI / 2-D render (~6 files):** ownrdraw (341 err, mostly GDI
  `BitBlt`/`DrawText`/`SelectObject` owner-draw), dsurface, tactical, srfcache,
  wincursor, gscreen. DirectDraw-style surface blitting for a **2-D sprite**
  engine (Tiberian Sun).
- **COM persistence (~4 files):** savever, saveload, cstream, scroll. Save/load
  via `IPersistStream`/`IPropertyStorage` — these need a native stream, not
  stubs (see risks).
- **Misc type gaps (remaining):** dbgprint (`SRWLOCK`), mpu (`LARGE_INTEGER`
  members), syncrechook (`IMAGE_DOS_HEADER`), gamedirs/data/conquer/cdfile/ini
  (single opaque types), fly (`IFlyControl` COM interface def).

**Audio and Net:** 0 keyword hits inside the 39 — those TUs already parse
(`mmsystem.h` is shimmed; net uses `winsock.h` which is in place). They are
*not* part of this backend scope.

## Backend recommendation

**Primary: SDL2 + Dear ImGui.**

| Layer        | Recommend            | Why |
|--------------|----------------------|-----|
| Window/input | **SDL2**             | Cross-platform (the whole point of the port), one lib covers window, event loop, keyboard/mouse, joystick. Maps cleanly onto the Win32 message loop. |
| 2-D render   | **SDL2 surfaces / `SDL_Render`** | TS is a 2-D sprite game using DirectDraw-style `Blt` blitting. SDL2 surfaces/textures are a direct 1:1 replacement — no 3-D pipeline needed. |
| Audio        | **SDL_audio / SDL_mixer** | Replaces the `mmsystem`/`waveOut` layer with minimal effort. |
| Dialog/UI    | **Dear ImGui** (immediate mode) | The ~18 dialog files are menus/option panels, not complex forms. ImGui binds trivially to SDL2 and avoids re-implementing 18 Win32 dialogs as native widgets. |

**Rejected alternatives:**
- **Qt** — widget toolkit for forms apps; fighting its event loop inside a
  real-time game loop is awkward and heavy. Overkill for sprite blitting.
- **Cocoa** — macOS-only; defeats the cross-platform goal.
- **bgfx** (3-D) — a `bgfxbackend.cpp` already exists, but TS is 2-D. bgfx adds
  3-D complexity for no current benefit, and covers *render only* (you'd still
  need SDL/GLFW for window+input and a separate audio lib). Optional later if
  GPU post-processing is wanted; SDL2 first.

## Effort estimate

| Phase | Work | Estimate |
|-------|------|---------:|
| **1** | Stub-closure: ~100 symbols (commctrl/windowsx macros, window-style constants, ~10 opaque structs, `LARGE_INTEGER` members) → 412/412 TUs parse | **3–5 dev-days** |
| **2a** | SDL2 window + event loop + main-loop integration | 1 week |
| **2b** | 2-D surface/blit renderer replacing DirectDraw (`dsurface`/`gscreen`/`tactical`/`srfcache`/`ownrdraw`/`wincursor`) | 4–6 weeks |
| **2c** | Audio playback (`SDL_audio`/`SDL_mixer`) over the `mmsystem` layer | 1–2 weeks |
| **2d** | Dialog/options UI via Dear ImGui (~18 dialog files) | 3–4 weeks |
| **2e** | Native save/load stream replacing COM persistence (`savever`/`saveload`/`cstream`/`scroll`) — also fixes the 4B-vs-8B pointer-width save break | 2–3 weeks |
| **2f** | Integration, Apple-Silicon testing, debugging | 2–3 weeks |
| **Total** | | **~13–21 person-weeks** |

Solo engineer: ~3–5 months. A 2–3 person team: ~1.5–2.5 months. Phase 1 is an
independent quick win that can ship first (enables CI on the full tree).

## Risks / open questions

1. **bgfx decision.** `bgfxbackend.cpp` already exists — someone started bgfx.
   Confirm we pivot to SDL2 blit (recommended) vs. continue bgfx (render-only,
   still needs SDL/GLFW + audio lib).
2. **DirectDraw blit semantics.** `dsurface.cpp` owns the surface/`Blt` model;
   must confirm how the engine composites (page-flipping vs. single back-buffer)
   before mapping to SDL2 textures.
3. **Save-format break is structural** (`savestream.h` writes `sizeof(pointer)`
   4B Win32 / 8B arm64 + `swizzle.h` `uintptr_t ID`). Cross-platform saves are
   impossible until Phase 2e's native stream lands — keeping CLSIDs does not fix
   it.
4. **Hollow stubs are not behavior.** Phase 1 makes TUs *parse*; the game will
   not run until Phase 2. Do not mistake a green Phase-1 tree for a working port.

## Suggested next step

1. Land **Phase 1** stub-closure now (fast, makes the whole tree CI-buildable).
2. Then kick off **Phase 2a** (SDL2 bootstrap) and decide bgfx vs. SDL2 blit
   (risk #1) before 2b.
