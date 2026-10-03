# Which Tiberian Sun game files this port needs

Measured, not assumed: the engine was booted against progressively smaller sets of
a real install, and the smallest set that reaches each milestone is what's listed
here. Everything is reproducible — see [How this was measured](#how-this-was-measured).

The install measured was `Command.and.Conquer.Tiberian.Sun.v15918072` (the GOG
build, ~2.1 GB).

## Required to reach the main menu

| File | Why |
|---|---|
| `TIBSUN.MIX` | the bootstrap archive; every other file is reached through it |
| one of `MOVIES01.MIX`, `MOVIES02.MIX`, `MOVIES03.MIX` | intro movies |
| `SCORES.MIX` | menu music |

That is the whole set. Nothing else is read before the main menu appears.

## Additionally required to play a mission

| File | Why |
|---|---|
| `MAPS01.MIX` | the solo campaign maps |
| one of `SIDECD01.MIX`, `SIDECD02.MIX` | in-game speech; GDI or Nod, interchangeable |

The first mission is **`GDI1A.MAP`** (or `NOD1A.MAP` for Nod), named in
`BATTLE.INI` — *not* `SCG01EA`, which is a scenario-editor map that a normal
launch never opens.

A minimal playable set is therefore ~600 MB against a ~2.1 GB install.

## Optional, and what they add

| File | What it adds |
|---|---|
| `MAPS02.MIX`, `MAPS03.MIX` | later campaign missions |
| `MULTI.MIX` | multiplayer maps |
| `PATCH.MIX` | title-screen and main-menu patching |
| `EXPAND01.MIX`, `expand02.MIX`, `expand03.MIX` | expansion content; **`FIRESTRM.INI` inside one of these is what makes the game detect Firestorm** |
| `SOUNDS01.MIX` | title-screen music |
| `SCORES01.MIX` | extended music set |
| `E01SCD01.MIX`, `E01SCD02.MIX` | expansion speech |
| `MOVIES02.MIX`, `MOVIES03.MIX` | the other two movie discs |
| `GMENU.MIX`, `WDT.MIX`, `WDTVOX.MIX` | World Domination Tour assets — see below |
| `LANGUAGES` | additional language data |

## Not used at all

`SUN.exe`, the original DLLs, `installScript.vdf`, and the `RMCache` / `Themes`
folders. This is a from-source port; none of the original binaries are read.

## World Domination Tour cannot work regardless

`WDT.MIX` and `WDTVOX.MIX` are present in a complete install, and the WDT code
(`wdtnet.cpp`, `wdtcampaign.cpp`, `worlddom.cpp`) is all in the tree. It is
disabled on purpose, not missing:

- `Display_Firestorm_Menu()` pushes both `NSEL_INTERNET` and `NSEL_WDT` into the
  menu's disable list, because the online service and its servers are gone.
- The `FirestormWDT` menu item has **empty `Disabled=` art**, so it still *looks*
  live and silently swallows clicks rather than greying out — which is why it reads
  as a bug.
- `New_Main_Menu`'s selection switch has **no `case NSEL_WDT`** at all, so
  selecting 7 falls to `default: SEL_NONE`.

So even with every file present and every patch applied, the button cannot be made
to work: there is no server to talk to. Re-enabling it would produce a menu entry
that leads nowhere.

## How this was measured

Symlink sets were built up in a scratch directory and the engine booted against
each, reading the milestone out of `~/Library/Application Support/OpenTS/Debug/`:

| Data directory | Result |
|---|---|
| *(empty)* | `Failed to initialize bootstrap mixfiles!` |
| `TIBSUN.MIX` only | secondary mixfiles fail; no menu |
| `TIBSUN` + one `MOVIES0*` + `SCORES` | **main menu reached**, 25 scenarios listed |
| the above + `MAPS01` + `SIDECD01` | `Start_Scenario('GDI1A.MAP') -> 1` |

The `OpenTS_AppleSilicon.app` launcher validates the "reaches a mission" set — see
[macos-app-bundle.md](macos-app-bundle.md), which lists the same files and
explains why a missing one is reported by name rather than as a generic failure.
