# Developer documentation

The developer guides are split by subject:

- [Building OpenTS](BUILDING.md) — supported toolchain, commands, outputs,
  build identity, and continuous integration. Also covers the macOS build.
- [Style](STYLE.md) — source formatting, naming, C++ use, and comments.
- [History](HISTORY.md) — source lineage and reconstruction history.
- [Rationale](RATIONALE.md) — reconstruction tools, recovered structure, and
  non-obvious implementation choices.
- [Project direction](DIRECTION.md) — long-term architecture.
- [UI system design](UI_DESIGN.md) — proposed RmlUi and ImGui integration,
  screen-level interchangeable views, and the migration from OwnerDraw.

## This port's documentation

- [The Apple Silicon port](apple-silicon-port.md) — how the macOS/arm64 port is
  put together, what is verified, what is not, and the known gaps.
- [The macOS app bundle](macos-app-bundle.md) — packaging as `OpenTS_AppleSilicon.app`: the
  data-folder picker, the writable-directory rules, signing, and the traps that
  break a bundle.
- [Game data requirements](game-data-requirements.md) — which Tiberian Sun files
  the engine reads, measured by booting against progressively smaller sets.
- [Image quality on Metal](image-quality-survey.md) — supersampling and
  sharpening, and what the alternatives would have cost.
- [MTLFXSpatialScaler scope](metalfx-spatial-scaler-scope.md) — why Apple's
  spatial scaler was investigated and not adopted.
- [Upstream README](UPSTREAM_README.md) and
  [upstream acknowledgements](UPSTREAM_ACKNOWLEDGEMENTS.md) — preserved verbatim
  from [OpenTS](https://github.com/OpenTS-Developers/OpenTS), the project this
  repository derives from.

See [CONTRIBUTING.md](../CONTRIBUTING.md) for contribution and review rules.
Player and modder documentation is under [manual/](../manual/README.md). When a
guide already covers a subject, link to it instead of copying the same facts.

**Issues about the engine, the campaign, multiplayer or modding belong
[upstream](https://github.com/OpenTS-Developers/OpenTS/issues).** This repository
is the macOS/arm64 port; issues here should be about macOS, arm64, or the
application bundle.
