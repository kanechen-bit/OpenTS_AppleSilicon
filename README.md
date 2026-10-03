<p align="center">
  <h1>OpenTS — Apple Silicon</h1>
  <p><em>A macOS/arm64 port of OpenTS, the community reconstruction of
  <strong>Command &amp; Conquer: Tiberian Sun</strong>.</em></p>
</p>

<p align="center">
  <a href="https://github.com/OpenTS-Developers/OpenTS"><img src="https://img.shields.io/badge/upstream-OpenTS--Developers%2FOpenTS-blue" alt="Upstream project"></a>
  <a href="LICENSE.md"><img src="https://img.shields.io/badge/license-GPL--3.0--or--later-blue" alt="GPL-3.0-or-later"></a>
  <img src="https://img.shields.io/badge/platform-macOS%20%7C%20arm64-lightgrey" alt="macOS arm64">
</p>

# About

This is a **derivative work**: a port of
[OpenTS](https://github.com/OpenTS-Developers/OpenTS) to Apple Silicon. It is
not a fork in the maintainers' sense, and it is not endorsed by them — it is one
branch of the work, taken in a direction they have not chosen.

OpenTS itself is a community-led, open-source reconstruction of *Command &
Conquer: Tiberian Sun* by [ZivDero](https://github.com/ZivDero),
[tomsons26](https://github.com/tomsons26) and contributors, built from
Electronic Arts' GPL-released source for related *Command & Conquer* games and
Tiberian Sun-specific reverse engineering. **All of that work is theirs.** This
port exists because of it, and every part of it that is not listed under
[Differences from upstream](#differences-from-upstream) is exactly as they left
it. Upstream's own README is kept verbatim at
[docs/UPSTREAM_README.md](docs/UPSTREAM_README.md).

If you want OpenTS proper — and on Windows, that is the supported target — use
[the upstream project](https://github.com/OpenTS-Developers/OpenTS). Issues
about the game engine, the campaign, multiplayer or modding belong there; issues
about macOS, arm64, or the `.app` belong here.

## What this port does

Upstream OpenTS targets 32-bit Windows with MSVC. This port makes the same engine
build and run as a native 64-bit macOS application:

- **A Win32 platform layer for macOS** (`code/port/`). A shim supplies the Win32
  and COM APIs the engine is written against — windowing through Cocoa, input,
  GDI, resources, file and time APIs — so the engine's own ~400 translation units
  compile unchanged on arm64. The engine is not forked per platform; only the
  platform underneath it is.
- **A Metal renderer path** through [bgfx](https://github.com/bkaradzic/bgfx), at
  native arm64, including 2× supersampling and a sharpen pass on the resolve.
- **A double-clickable `OpenTS_AppleSilicon.app`** that asks where your Tiberian Sun game files
  are, checks them, and starts the game — see
  [The macOS app bundle](docs/macos-app-bundle.md).
- **Arrow-key panning**, so the four arrow keys on a MacBook drive the view.

## Status

Working and playable: the app builds, launches, validates its data directory, and
runs the campaign on Apple Silicon. It is a port in progress, not a finished
alternative to the Windows build.

Known gaps are listed in [docs/apple-silicon-port.md](docs/apple-silicon-port.md),
along with what has been verified and how. In short: the engine runs, and some
subsystems are thinner than they are on Windows.

## Requirements

- macOS 12 or newer on Apple Silicon (arm64). Intel Macs are not targeted.
- A legal copy of *Command & Conquer: Tiberian Sun* (Steam or EA App). OpenTS
  supplies the engine, never the game assets.

## Building

```sh
git clone --recurse-submodules <this repository>
cd opents-apple-silicon
cmake -S . -B build -DOPENTS_EXPERIMENTAL_NONWIN32=ON
cmake --build build -j"$(sysctl -n hw.ncpu)"
```

Then either build the application bundle, or the disk image to hand out:

```sh
tools/macos-app/make_app.sh        # -> dist/OpenTS_AppleSilicon.app
tools/macos-app/make_dmg.sh        # -> dist/OpenTS_AppleSilicon.dmg
```

`make_dmg.sh` wraps the app in a disk image with a plain-text README for the
player and an `/Applications` symlink, and verifies the image before reporting
success. That is the artefact to give someone.

Or run the engine directly against a game install:

```sh
OPENTS_DEBUG_DIR=/tmp/opents-debug ./build/bin/Game \
  -DATADIR="/path/to/Command and Conquer Tiberian Sun" \
  -USERDIR=/tmp/opents-user -W
```

`-DATADIR=` points at your game files, `-USERDIR=` at a writable directory for
settings, saved games and logs, and `-W` opens a window rather than full screen.
`make_app.sh` is the easier route: the app asks for all of it through a folder
picker.

Full details are in [docs/BUILDING.md](docs/BUILDING.md) and
[docs/apple-silicon-port.md](docs/apple-silicon-port.md).

## Which game files you need

About 600 MB of the ~2.1 GB install. The exact list, measured rather than
assumed, is in [docs/game-data-requirements.md](docs/game-data-requirements.md).

## Differences from upstream

Everything else is upstream's. These are the changes:

| Area | Change |
| --- | --- |
| `code/port/` | The macOS platform layer: Win32/COM shim, Cocoa window backend, Metal paths |
| `code/bgfxbackend.*`, `code/ownrdraw.cpp` | arm64 Metal rendering, 2× supersampling, sharpen pass |
| `code/scroll.*`, `code/options.*` | Arrow-key panning (`ArrowPan`) |
| `code/gamedirs.*` | `-DATADIR=` / `-USERDIR=` support |
| `code/port/port_main.cpp`, `code/port/windows_stub.h` | Command-line round trip fixed: a path containing a space was being truncated |
| `code/dbgprint.cpp` | `OPENTS_DEBUG_DIR`, so logs stay out of the signed bundle |
| `code/init.cpp` | `-W` as a short form of `-WIN` |
| `tools/macos-app/` | The `.app` bundle: launcher, icon, signing, settings template |
| `docs/` | This port's documentation |

Upstream history is preserved intact, so `git log` shows their work followed by
this port's.

## Documentation

- [Apple Silicon port](docs/apple-silicon-port.md) — architecture, what is
  verified, and what is not
- [The macOS app bundle](docs/macos-app-bundle.md) — the `.app`, its rules and
  its traps
- [Game data requirements](docs/game-data-requirements.md) — which files are
  needed and which are optional
- [Building OpenTS](docs/BUILDING.md) — toolchain and commands
- [Image quality notes](docs/image-quality-survey.md) — supersampling and
  sharpening on the Metal path

The [OpenTS manual](https://opents-developers.github.io/OpenTS/) covers setup,
INI configuration, mapping and engine internals, and applies here unchanged.

## License

This derivative work is released under the **GNU General Public License, version
3 or later** — the same license as OpenTS. That is a requirement of the license,
not a preference.

Material derived from Electronic Arts source remains subject to the additional
GPL Section 7 terms in [LICENSE.md](LICENSE.md), and the files concerned carry an
Electronic Arts copyright notice in their headers. **Those notices must not be
removed**; doing so would break the license this work depends on.

- [LICENSE.md](LICENSE.md) — GPL-3.0 and the EA Section 7 additional terms
- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) — bundled dependencies
- [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md) — upstream credits, EA source
  releases, and the community

Game assets are **not** distributed here and must not be added: Tiberian Sun's
data files are Electronic Arts' property. This repository is engine source only.

## Credits

This port stands on the work of the OpenTS developers and the earlier
reconstruction community — see [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md) for the
full list, including the [TibSun archive](https://github.com/OpenTS-Developers/TibSun)
and [CCHyper](https://github.com/CCHyper)'s work on EA's open-source releases.

Command & Conquer is a trademark of Electronic Arts Inc. This project is not
affiliated with or endorsed by Electronic Arts.
