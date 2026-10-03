# Acknowledgements

OpenTS follows years of community research, reconstruction, engineering, and
testing. This page honors the people, projects, and communities whose earlier
work made OpenTS possible.

> **This repository is a derivative work.** It is a port of
> [OpenTS](https://github.com/OpenTS-Developers/OpenTS) to Apple Silicon, and
> the project it derives from is the work of the people listed below. Upstream's
> acknowledgements are kept here unchanged, because this port would not exist
> without them; the Apple Silicon work is credited at the end. Upstream's
> original acknowledgements are also preserved verbatim at
> [docs/UPSTREAM_ACKNOWLEDGEMENTS.md](docs/UPSTREAM_ACKNOWLEDGEMENTS.md).

## Upstream project

- [OpenTS](https://github.com/OpenTS-Developers/OpenTS) — the community
  reconstruction this port is based on, and the source of the engine, the
  reverse-engineering work, and the reconstruction of the campaign data. Every
  part of this repository that is not listed under *Apple Silicon port* below is
  theirs.

## Project leads

- [ZivDero](https://github.com/ZivDero) — project co-lead and developer
  ([Patreon](https://www.patreon.com/c/ZivDero))
- [tomsons26](https://github.com/tomsons26) — project co-lead and developer

## Previous work and research

OpenTS builds on work by Command & Conquer community members who studied,
documented, patched, tested, and extended Tiberian Sun before this project.

[CCHyper](https://github.com/CCHyper) worked on Electronic Arts' Command &
Conquer open-source releases for the Remastered Collection and the 2025 The
Ultimate Collection. Those releases provide much of OpenTS's source
foundation.

## Special thanks

- [Kerbiter](https://github.com/Metadorius) — advice on preparing the public
  release

## Electronic Arts source releases

OpenTS uses GPL-licensed source from these Electronic Arts releases:

- [Command & Conquer Remastered Collection](https://github.com/electronicarts/CnC_Remastered_Collection)
- [Command & Conquer Red Alert](https://github.com/electronicarts/CnC_Red_Alert)
- [Command & Conquer Tiberian Dawn](https://github.com/electronicarts/CnC_Tiberian_Dawn)
- [Command & Conquer Renegade](https://github.com/electronicarts/CnC_Renegade)
- [Command & Conquer Generals and Zero Hour](https://github.com/electronicarts/CnC_Generals_Zero_Hour)

The additional GPL Section 7 terms covering that material are in
[LICENSE.md](LICENSE.md).

## ts-patches and Vinifera contributors

Community fixes for the original executable live in
[ts-patches](https://github.com/CnCNet/ts-patches), and
[Vinifera](https://github.com/Vinifera-Developers/Vinifera) carries that work
into a source-level engine extension. When OpenTS ports a fix, its manual
change record credits the author where possible.

## Artwork

- [Kerbiter](https://github.com/Metadorius) — the OpenTS logo
- [tomsons26](https://github.com/tomsons26) — the OpenTS icon

The Apple Silicon port reuses the OpenTS logo and icon as upstream's assets. The
port's own application icon, in `tools/macos-app/`, was drawn for this repository
and is not a work of theirs.

## Apple Silicon port

The macOS/arm64 port — the Win32 platform shim, the Cocoa window backend, the
Metal rendering work, and the `OpenTS.app` bundle — is derivative work released
under the same GPL-3.0-or-later license as OpenTS.

It also rests on the work of:

- **[bgfx](https://github.com/bkaradzic/bgfx)** — the renderer, through
  `thirdparty/bgfx.cmake`, carrying the Metal backend this port runs on.
- **[Cocoa](https://developer.apple.com/documentation/cocoa)** — the window and
  input layer behind `code/port/portwindow/`.
- The OpenTS developers, for the engine this port exists to run.

## Communities

- [C&C Mod Haven](https://discord.gg/k4SVuMm)
- [CnCNet](https://cncnet.org)
