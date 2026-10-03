# OpenTS → Apple Silicon (arm64): COM-layer porting

This document tracks the replacement of the Windows COM dependency in OpenTS
with a portable, non-Windows equivalent. Everything here lives under the
`OPENTS_EXPERIMENTAL_NONWIN32` build flag (CMake gate `port/apple-silicon-phase0`).
The Win32 build path is byte-for-byte unchanged: those translation units keep
`#include <comdef.h>` / `<unknwn.h>` / `<objidl.h>` and never reach the stub.

## Why a stub and not a real COM port

OpenTS uses COM only as a *serialization plumbing* layer:

- `IStream` is implemented by `CStreamClass` (a memory buffer with LZO
  compression) — not a system stream.
- `IPersistStream` / `Save_Members` / `Load_Members` write raw bytes.
- `OleSaveToStream` / `OleLoadFromStream` round-trip objects by CLSID.
- `CoRegisterClassObject` builds the per-CLSID factory table at startup.

None of this needs the Component Object Model at runtime. The stub in
`com_stub.h` supplies the *types and symbols* the engine references so the
translation units parse, plus hollow runtime stubs (`OleSaveToStream`,
`CoRegisterClassObject`, smart-pointer `CreateInstance`, …) that compile but
are explicitly non-functional. The real port replaces the persistence layer
with a native stream — that is tracked separately (Piece B / save-container
decision) and is also what resolves the cross-platform save-format break.

## Inventory (17 COM-using files, confirmed by grep)

### Piece A — type-only headers (DONE, committed)
Wired via conditional include to `port/com_stub.h`; no COM method bodies.
`base.h, blowfish.h, enviro.h, iblockci.h, iblowfish.h, iflyctrl.h,
ilinkstm.h, ini.h, ion.h, isotile.h, isun.h, typelist.h`

### Piece B — runtime COM surface (DONE, committed)
The full COM runtime layer is now supplied by `code/port/com_stub.h`:
`IStream` (Read/Write/Seek/SetSize/CopyTo/Commit/Revert/LockRegion/
UnlockRegion/Stat/Clone), `ILinkStream`, `IPersist`/`IPersistStream`,
`IClassFactory`, `IStorage`, `IPropertySetStorage`, `ULARGE_INTEGER`/
`LARGE_INTEGER`/`STATSTG`/`FILETIME`/`PROPVARIANT`, `GUID::operator==`,
the `CLSCTX_*`/`REGCLS_*`/`STGM_*`/`PROPSETFLAG_*`/`VT_*` flags, the
`S_*`/`E_*`/`CLASS_E_*` HRESULT codes, `STDMETHODIMP`, standard `I*Ptr`
aliases, a `_opents_com_ptr<T>` smart-pointer template with a hollow
`CreateInstance`, and hollow `OleSaveToStream`/`OleLoadFromStream`/
`CoRegisterClassObject`/`CoCreateInstance`/`StgOpenStorage` stubs.

Wired headers (conditional include to com_stub.h, Win32 path unchanged):
`abstract.h, cstream.h, iloco.h, ipiggy.h, savever.cpp`
These pull the stub into the 4 call-site `.cpp` files transitively
(`saveload.cpp, mouse.cpp, startup.cpp, cstream.cpp`).

Verified with `opents-spike/com_stub_selfcheck.cpp` — a TU mirroring every
COM construct the 17 files use — compiled clean under
`clang++ -fsyntax-only -arch arm64 -DOPENTS_EXPERIMENTAL_NONWIN32`.

**Runtime status:** the `Ole*`/`Co*`/`Stg*` entry points are hollow
(`E_NOTIMPL`/`S_OK`). They make the call sites parse and link but do not
actually persist. The real port REPLACES this layer with a native stream
(see Piece C below) — that also fixes the pointer-width save break.

### Piece C — replace the persistence layer (next strategic decision)
Rather than emulate COM at runtime, swap `OleSaveToStream`/`OleLoadFromStream`/
`CoRegisterClassObject` for a native `PortableStream` + factory table. This is
the clean path and resolves the cross-platform save-format break
(`savestream.h:136` writes `sizeof(pointer)` bytes; `swizzle.h` uses
`uintptr_t ID`). Cross-platform saves remain impossible while raw pointers are
serialized, independent of COM.

## Verification method

There is no `windows.h` shim yet (the Win32 GUI layer is a separate, larger
milestone). So the COM layer is verified **in isolation** with a self-check
translation unit (`opents-spike/com_stub_selfcheck.cpp`) that mirrors every
COM construct the 17 files actually use, compiled with:

```
clang++ -std=c++17 -fsyntax-only -arch arm64 -Wshorten-64-to-32 \
        -fms-extensions -fdeclspec -DOPENTS_EXPERIMENTAL_NONWIN32 \
        com_stub_selfcheck.cpp
```

When the `windows.h` shim exists, the real engine TUs will be re-checked in
context.

## Open questions / next gates
1. **windows.h shim** — the bounded but large Win32 surface (`_makepath` ×66,
   `InvalidateRect` ×56, `GetWindowLong` ×34, …). UI/GUI layer.
2. **Persistence replacement** — replace `Ole*`/`Co*` with a native stream;
   this also fixes the pointer-width save break (`savestream.h:136` writes
   `sizeof(pointer)` bytes). Cross-platform saves are impossible while raw
   pointers are serialized regardless of COM.
3. **Two-tree hazard** — edit only `/Users/kanechen/Downloads/OpenTS` (git).
   `OpenTS-0.1.0` is a stale no-`.git` copy; do not touch it.
