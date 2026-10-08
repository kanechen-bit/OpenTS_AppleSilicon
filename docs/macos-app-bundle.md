# OpenTS_AppleSilicon.app — the macOS application bundle

`OpenTS_AppleSilicon.app` packages the engine as a double-clickable Mac application: it asks
where the Tiberian Sun game files are, checks them, starts the game, and keeps
everything the game writes out of the application bundle.

Build it with:

```sh
tools/macos-app/make_app.sh          # -> dist/OpenTS_AppleSilicon.app
tools/macos-app/make_dmg.sh          # -> dist/OpenTS_AppleSilicon.dmg
```

The engine must already be built (`cmake --build build --target Game`); the
script refuses to run without it. Nothing here is part of the CMake build, so the
engine still builds with nothing but a compiler and CMake.

`make_dmg.sh` calls `make_app.sh` itself, then wraps the result in a disk image
along with `DMG_README.txt` — a plain-text README for the player, not the
developer documentation in `docs/` — and an `/Applications` symlink. It mounts the
finished image and checks the contents and the signature before reporting success.

## What is in the bundle

The bundle is `OpenTS_AppleSilicon.app`, named so it is distinguishable from
upstream's `OpenTS.app` in the Finder and the Dock — two bundles with the same
name and the same icon, differing only by which one is selected, is a support
problem waiting to happen. `CFBundleName` stays `OpenTS`, because that is what
the menu bar and the About window show, and the project is still OpenTS.

```
OpenTS_AppleSilicon.app/Contents/
  MacOS/OpenTS            the launcher -- this is CFBundleExecutable
  MacOS/OpenTS-engine     the engine, named apart so the two cannot collide
  Resources/OpenTS.icns   the icon
  Resources/SUN.INI       settings template, copied to the user dir on first run
  Resources/LICENSE.md    and the notices, which cannot be fetched at runtime
  Info.plist
```

## First launch

The game data cannot be shipped inside the app: it is the game's own content and
is still in the Steam or GOG install on the player's disk. So the first launch
asks.

1. **Remembered folder, or the picker.** The chosen folder is stored as a
   *bookmark* in `NSUserDefaults`, not as a path — a Steam library gets moved and
   renamed, and a stored path would go stale and ask again. A bookmark follows the
   folder. A bookmark that no longer resolves is simply treated as no bookmark.
2. **Validation.** The folder must hold `TIBSUN.MIX`, `MAPS01.MIX`, `SCORES.MIX`,
   and at least one of `MOVIES01/02/03.MIX` and one of `SIDECD01/02.MIX` (the two
   sound CDs are interchangeable). Anything missing is listed **by name** in an
   error dialog offering *Choose Another Folder…* or *Quit* — re-picking, not a
   dead end. These are the files measured as required; see
   [game-data-requirements.md](game-data-requirements.md).
3. **Writable directory.** Everything the game writes goes to
   `~/Library/Application Support/OpenTS` — settings, saved games, debug logs.
4. **Hand-off.** The launcher `execv()`s the engine with `-DATADIR=`, `-USERDIR=`
   and `-W`, then is gone.

### Why `execv()` and not `NSTask`

The engine opens its Metal window and owns the run loop. The process holding the
Dock tile and the activation policy has to be the process that draws the window.
Replacing the launcher outright makes the engine a first-class app; a child would
have to be waited on and reaped, and the game would be quitting its parent.

## What the launcher passes the engine

| | |
|---|---|
| `-DATADIR=<chosen folder>` | where the game files are, never written to |
| `-USERDIR=<Application Support>/OpenTS` | where the game's own files go |
| `-W` | windowed rather than borderless full screen |
| `OPENTS_DEBUG_DIR=<user dir>/Debug` | keeps the log out of the sealed bundle |

`-W` is the one the engine did not have: `-WIN` already existed, and `-W` was
added as the short form (`code/init.cpp`) because a launcher building its own
command line has room for the shorter switch.

**Windowed is the default** and not a preference. On macOS the borderless full
screen the game asks for puts the player in a separate Space they have to swipe
out of to reach the Finder, which is a poor fit for an app that *is* a Dock icon.
`OPENTS_FULLSCREEN=1` in the environment asks for full screen instead. There is no
`-FULLSCREEN` switch and inventing one would have been a no-op: the engine takes
full screen only from `Fullscreen` in `SUN.INI`, so that is what the launcher
rewrites. The shipped template says `Fullscreen=no` so the switch and the setting
agree — if they disagree the switch wins and the setting is a lie in the file.

## Two things that break a bundle, both of which this avoids

**1. `SUN.INI` in `Contents/MacOS/` makes the app fail to launch.** macOS refuses
to seal a bundle whose executable directory holds files the signature does not
describe, and the result is a bare "damaged" error. Hence the template lives in
`Contents/Resources/` and is *copied* to the user directory on first run — it also
has to be there, because the engine reads `SUN.INI` through its file layer, which
resolves it in the user directory.

**2. Anything the game writes inside the bundle breaks the seal.** The obvious
offender is the debug log, which `dbgprint.cpp` derives from `GetModuleFileName` —
i.e. beside the executable. Hence `OPENTS_DEBUG_DIR` (`code/dbgprint.cpp`), which
the launcher sets before handing over. A broken seal does *not* stop an ad-hoc
build from running, which is what makes this worth catching: it passes every test
on the build machine and then fails notarization and Gatekeeper everywhere else.
`make_app.sh` verifies the seal, and the end-to-end test checks the file list is
unchanged after a run.

## Signing

Ad-hoc (`codesign -s -`) by default, which is enough to run on the machine that
built it and keeps the build reproducible. Gatekeeper will also need the
quarantine attribute cleared (`xattr -dr com.apple.quarantine`) or the user
right-clicking the app and choosing *Open*.

For distribution, three things are needed and only the first is missing:

1. **A Developer ID Application certificate.** Free-ish ($99/year, individual or
   organisation) from Apple. Install the `.p12` in the keychain, add it to the
   login keychain, and confirm:

   ```sh
   security find-identity -v -p codesigning    # must list it
   ```

   A self-signed certificate does not work: Gatekeeper rejects it and
   notarization refuses it.

2. **A notarytool keychain profile**, holding an app-specific password. Created
   once, interactively:

   ```sh
   xcrun notarytool store-credentials <profile>
   ```

3. **Both variables set**, and then it is one command:

   ```sh
   OPENTS_SIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
   OPENTS_NOTARY_PROFILE=<profile> \
       tools/macos-app/make_dmg.sh
   ```

`make_dmg.sh` signs with `--timestamp` and `--options runtime` (notarization
rejects a Developer ID signature without a secure timestamp), submits with
`notarytool --wait`, staples, validates the ticket, and re-verifies the seal
afterwards. Notarization happens **before** the image is built, because stapling
attaches a ticket to the bundle and the image is made from what is inside it.

The archive submitted is built with `ditto -c -k --keepParent` rather than `zip`,
because notarytool checks the bundle's symlinks and extended attributes and `zip`
strips them.

Notarization is not cosmetic: an un-notarized app distributed by download is
blocked outright, and users see "Apple could not verify" rather than a right-click
workaround. It has to happen before release, not after reports arrive.

## Testing

`OPENTS_DATA_DIR=<folder>` skips the picker, which is the seam the rest is tested
through. `OPENTS_VALIDATE_ONLY=1` runs validation alone and prints the report the
dialog would show, then exits `0` (complete) or `2` (incomplete) — the two dialogs
are modal and need a person, so this is how the decision they hang off is checked:

```sh
OPENTS_DATA_DIR=<folder> OPENTS_VALIDATE_ONLY=1 dist/OpenTS_AppleSilicon.app/Contents/MacOS/OpenTS
```

Full run, engine boots to the menu:

```sh
OPENTS_DATA_DIR=/path/to/ts OPENTS_SELFTEST=1 OPENTS_NOREDRAW=1 \
  dist/OpenTS_AppleSilicon.app/Contents/MacOS/OpenTS
```

`tools/macos-app/test_arg_roundtrip.cpp` covers the command line round trip on its
own; see below for why that is not a formality.

## A real bug this found: spaces in paths

The bundle put the user directory at `~/Library/Application Support/OpenTS`, and
the game silently came up with its user directory truncated to
`/Users/kanechen/Library/Application` — a path that does not exist.

The engine never sees `argv` directly. `main()` rebuilds a single command line
string, and the engine re-splits it through `CommandLineToArgvW()`. The join
added no quotes, so `-USERDIR=/tmp/My Games/OpenTS` came back as three arguments
and the directory was cut at the space. Fixed in both halves:

- `code/port/port_main.cpp` quotes an argument holding whitespace when joining.
- `code/port/windows_stub.h` — `opents_port_command_line()` does the same, and
  `CommandLineToArgvW()` now honours quotes instead of splitting blindly.

Verified: `-USERDIR=/tmp/opents user dir` arrives as one argument and
`[GameDirs] User directory is /tmp/opents user dir/.`

The lesson worth keeping: **this class of bug is invisible to any test that uses a
path without a space.** It was found only because the default user directory on
macOS contains one. Test paths deliberately include a space from now on.

Note also that `Parse_Command_Line` strips every `"` from each token, so a folder
name containing a quote cannot be passed to the engine at all. The launcher
rejects such a folder with an explanation rather than quietly handing over a
different path.

## Engine changes this required

| File | Change | Why |
|---|---|---|
| `code/dbgprint.cpp` | `OPENTS_DEBUG_DIR` override for the log directory | the log would otherwise be written into the sealed bundle |
| `code/init.cpp` | `-W` as a short form of `-WIN` | the launcher needs the short switch |
| `code/port/port_main.cpp` | quote arguments when joining the command line | spaces in `-DATADIR=`/`-USERDIR=` split the argument |
| `code/port/windows_stub.h` | quote-aware `CommandLineToArgvW`, matching join | the other half of the round trip |
