# The Apple Silicon port

How OpenTS runs on macOS/arm64: what the port covers, how it is put together, what
has been verified, and what is still thin. Written for someone reading the tree
for the first time, or deciding whether to trust it.

Scope: this is a **port of the engine**, not a fork of the project. The
reconstruction, the campaign data handling, and the gameplay code are upstream's
work and are unchanged. See
[UPSTREAM_README.md](UPSTREAM_README.md) for the project this derives from.

## The shape of it

The engine is ~400 translation units of C++ written against Win32: it calls
`CreateFile`, `GetAsyncKeyState`, `MessageBox`, COM, GDI. Rather than fork the
engine per platform, the port supplies a **platform layer** underneath it, so the
same engine sources build for Windows and for macOS.

```
        game logic, unchanged (~400 TUs)
  ─────────────────────────────────────────────
  code/port/     the macOS platform layer
    windows_stub.h   Win32 + COM surface, POSIX-backed
    port_*.cpp       windowing-independent pieces
    portwindow/      the only Objective-C++ file: NSWindow and input
  ─────────────────────────────────────────────
        macOS / arm64
```

The engine sees `CreateFile` and gets a POSIX `open()` with the backslashes
translated. It sees `GetAsyncKeyState` and gets a real key state. It never knows.

Two consequences worth knowing before reading the code:

- **The engine assumes a 4-byte pointer.** The port is LP64: `long` is 8 bytes,
  `int` is still 4. Every Win32 type in `windows_stub.h` is sized deliberately,
  and the port is compiled 64-bit with a static assertion guarding the assumption
  the engine makes. Anything relying on `long` being 32-bit is a latent bug, and
  the shim exists partly to make those loud rather than silent.
- **`BIG_ENDIAN` is defined on macOS.** BSD headers define it as a *value*
  (4321), so `#ifdef BIG_ENDIAN` is true on an arm64 Mac. That scrambles the
  game's public-key decode, which yields an empty RSA modulus, which makes every
  encrypted `.MIX` header read as garbage. `port_early.h` handles it; this one
  cost a long time to find.

## What is in `code/port/`

| Path | Role |
| --- | --- |
| `port_early.h` | Force-included into every TU. Path separator, `BIG_ENDIAN`, helpers the engine's templates need before two-phase lookup |
| `windows_stub.h` | The Win32 and COM surface: types, file and time APIs, window messages, registry stubs, and the command line |
| `windows_stub/` headers | `<windows.h>`, `<shlobj.h>` and friends resolve here rather than to the system |
| `port_gdi.*` | The GDI subset the engine's blitters and dialogs use |
| `port_windows.cpp` | Window management, message dispatch, focus, the input queue |
| `port_input.*` | Keyboard and mouse state, translated to Win32 messages |
| `port_resource.cpp` | Dialog and string-table resources, so the engine's own UI is drawn by its own code |
| `port_msgqueue.cpp` | The message queue, in plain C++ so the shim stays non-Objective-C |
| `portwindow/port_window.mm` | The **only** Objective-C++ file. `NSWindow`, `CAMetalLayer`, `NSEvent` → Win32 messages |
| `port_trace.h` | The `[PORT]` tracing the headless harness relies on |

Keeping Objective-C++ to exactly one file is deliberate: everything else stays
plain C++, so the shim can be read and tested without an Objective-C compiler,
and only the window boundary needs Apple's frameworks.

`HEADLESS_BUILD.md` in the same directory is the working log of getting the tree
to build headlessly, including the measurement method and the traps. It is a
record of the port as it happened and is kept because the next person to extend
the shim will hit the same walls.

## Verifying without a human

The standing rule for this port is that **every change is verified headlessly** —
in-engine instrumentation plus logs and pixel readback — never by asking someone
to launch the game and look. The environment knobs make that possible:

| Knob | Effect |
| --- | --- |
| `OPENTS_SELFTEST=1` | Skip the attract movies, drive straight to a chosen point |
| `OPENTS_ANIM=1` | Keep animating rather than freezing on the first frame |
| `OPENTS_NOREDRAW=1` | Present without drawing, for headless runs |
| `OPENTS_HOLD=1` | Hold at a breakpoint until released |
| `OPENTS_DEBUG_DIR=<dir>` | Write logs somewhere other than beside the executable |
| `OPENTS_DATA_DIR=<folder>` | The bundle launcher: skip the folder picker |
| `OPENTS_VALIDATE_ONLY=1` | Validate a data folder and exit, printing the report |

The engine writes a `DEBUG_*.LOG` per run, with the resolved command line, the
directories in use, the mixfile bootstrap, and per-subsystem traces. Most port
questions are answered by grepping that file rather than by looking at a screen.

## What has been verified

Each of these was measured, not assumed:

- **The engine builds and links** on arm64 — all 412 translation units, from
  source, with the shim.
- **It boots to the main menu**, and into a mission: `Start_Scenario('GDI1A.MAP')`
  returns 1, with `MAPS01.MIX` and `SIDECD01.MIX` present.
- **Metal rendering** is the active path (`Video: renderer is Metal`), with 2×
  supersampling and a sharpen pass on the resolve.
- **Arrow-key panning** moves the tactical view in all four directions and clamps
  at map edges, at ~537 px/s. It is gated on `ArrowPan` in `SUN.INI` and does
  nothing when off — both confirmed.
- **The `.app` bundle** builds, signs, and launches; the seal survives a full run
  with nothing written inside it.
- **Folder validation** reports the correct missing files for an empty folder, one
  with only `TIBSUN.MIX`, one missing only `MAPS01.MIX`, and one satisfied by
  alternate `MOVIES`/`SIDECD` members.
- **Windowed mode** measurably differs from full screen: `Calc_Confining_Rect`
  reports a game-sized window with `-W` and a screen-filling surface without it.

## Known gaps

Stated plainly, because a port that hides its gaps is worse than one that lists
them:

- **Intel Macs are not supported.** The shim assumes LP64 and arm64. A Rosetta
  translation of a 32-bit engine was not attempted.
- **Multiplayer and CnCNet are untested.** The network code is upstream's and
  compiles, but it has not been exercised on this platform. Assume it does not
  work.
- **Some subsystems are thinner than on Windows.** Dialogs and file pickers in
  particular: the engine draws its own UI from Win32 resources, which the port
  supplies, but anything that wanted a *native* control has a stub rather than an
  implementation.
- **The World Domination Tour cannot work on any platform.** Its servers are gone.
  It is disabled in the menu deliberately — see
  [game-data-requirements.md](game-data-requirements.md).
- **Only single-player has been played through.** Campaign, skirmish and
  save/load work; the rest is inference from the code compiling.

## A bug worth knowing about

The engine never sees `argv` on this platform. `main()` rebuilds a single command
line string, and the engine re-splits it through `CommandLineToArgvW()`. Joined
without quotes, `-USERDIR=/Users/me/Library/Application Support/OpenTS` came back
as three arguments and the directory was silently cut to
`/Users/kanechen/Library/Application` — a path that does not exist, so the game
failed to launch on a folder that plainly existed.

It is fixed on both sides (`port_main.cpp` quotes when joining,
`windows_stub.h` parses quotes), and covered by
`tools/macos-app/test_arg_roundtrip.cpp`.

**The lesson generalizes: this class of bug is invisible to any test that uses a
path without a space.** It surfaced only because the default macOS user directory
contains one. Test paths should include a space deliberately.

## Build gotcha

Editing a file under `code/port/` does **not** reliably trigger a rebuild through
`cmake --build`, despite the declared `OBJECT_DEPENDS` edges. A stale binary then
makes a correct fix look broken. After touching the shim:

```sh
touch code/port/windows_stub.h && ninja -C build Game   # ~438 TUs should recompile
```

If far fewer recompile, the change did not propagate. Suspect the binary before
suspecting the fix.
