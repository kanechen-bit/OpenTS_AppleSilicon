OpenTS — Apple Silicon
======================

A macOS/arm64 build of OpenTS, the open-source reconstruction of
Command & Conquer: Tiberian Sun.

This disk image contains the application and this file. It does NOT contain
the game. OpenTS supplies the engine; the original Tiberian Sun data files are
Electronic Arts' property and have to come from your own copy of the game.


WHAT YOU NEED
-------------

1. macOS 12 (Monterey) or newer, on an Apple Silicon Mac.
   Intel Macs are not supported by this build.

2. Your own copy of Command & Conquer: Tiberian Sun, from either:
     - Steam:  Command & Conquer The Ultimate Collection
     - EA App: the Tiberian Sun edition
   You do not need the whole install. See GAME FILES below.

If you only have the launcher and not the game itself, install the game first.
There is nothing to play without it.


HOW TO RUN
----------

1. Open this disk image by double-clicking it.

2. Drag OpenTS_AppleSilicon.app into your Applications folder.

3. Double-click OpenTS_AppleSilicon.app.

4. The first time, the app asks where your Tiberian Sun game files are.
   A folder picker opens. Navigate to your Tiberian Sun install and select it.

   The app looks for TIBSUN.MIX in that folder, so select the folder that
   contains it -- not its parent. On a Steam install that is usually:

     ~/Library/Application Support/Steam/steamapps/common/C&C Ultimate
     Collection/C&C Tiberian Sun/

   The choice is remembered, so this only happens once. If you move your game
   later, the app follows it; if you delete the app, you are asked again.

5. The game starts in a window. Use the arrow keys to pan the view.

If macOS refuses to open the app the first time
------------------------------------------------

This build is not notarized with an Apple Developer ID, because it is a
community port. Gatekeeper therefore blocks it on first launch. Either:

  - Right-click (or Control-click) the app in Finder and choose "Open", then
    confirm "Open" in the dialog that appears. It opens normally from then on.

or, from a Terminal:

  xattr -dr com.apple.quarantine /Applications/OpenTS_AppleSilicon.app

Only do this if you trust the source of this disk image.


GAME FILES
----------

The app checks the folder you give it and tells you by name if anything is
missing. To reach the main menu and play the campaign you need, in your
Tiberian Sun folder:

  TIBSUN.MIX            (72 MB)   required -- the engine's core archive
  MAPS01.MIX           (3.5 MB)   required -- the campaign maps
  SCORES.MIX            (49 MB)   required -- menu music
  MOVIES01.MIX         (467 MB)   required -- one of MOVIES01/02/03.MIX
  MOVIES02.MIX         (468 MB)      (they are alternatives; any one of
  MOVIES03.MIX         (441 MB)      the three will do)
  SIDECD01.MIX          (12 MB)   required -- one of SIDECD01/02.MIX
  SIDECD02.MIX          (11 MB)      (either sound CD will do)

That is roughly 600 MB of the files, out of a full install of a couple of
gigabytes. The rest of the install is optional: later campaigns, multiplayer
maps, expansion content and soundtracks. Nothing else is required to play.

A note on MOVIES: you only need one of the three. They are alternatives, and
which one you have depends on how the game was installed.

If the app says a file is missing, it will name the file and offer to let you
pick a different folder -- you are not stuck.


WHERE YOUR FILES LIVE
---------------------

Everything the game writes goes to your user folder, not into the application:

  ~/Library/Application Support/OpenTS/
      SUN.INI              settings
      Saved Games/         your saved games
      Debug/               a log per run, kept for 14 days

Deleting that folder resets the settings and loses your saved games. Nothing
outside it is written, so removing the app leaves your saves alone.


TROUBLESHOOTING
---------------

The app says the folder has no game files
    You pointed at the wrong folder. It wants the one holding TIBSUN.MIX.

Nothing happens when it starts
    Check ~/Library/Application Support/OpenTS/Debug/ for the most recent
    log, named DEBUG_<date>.LOG. It records the exact command line used and
    every file the engine tried to open.

The screen looks wrong or has stray tiles
    Press Escape to leave full screen, or delete the Debug folder and start
    again. The log there will show whether a file failed to load.

It runs slowly
    Check Activity Monitor for another process using the GPU. The Metal
    renderer needs the graphics core, and some browsers and video apps take
    it while they are playing.


WHAT WORKS, AND WHAT DOES NOT
----------------------------

Works:
  - the campaign, skirmish, and save/load
  - full audio, including in-game speech
  - video playback
  - the whole main menu, with music

Not tested or not working:
  - Multiplayer and CnCNet. The code compiles but has not been exercised on
    this platform. Assume it does not work.
  - Intel Macs.

Known limitation, on any platform:
  - The "World Domination Tour" item on the main menu is greyed out and cannot
    be selected. Its online service has been shut down for years, so there is
    nothing to connect to. This is not a fault in this port, and it cannot be
    fixed by installing the missing files.


VERSION
-------

  OpenTS Apple Silicon 0.2.0
  Engine 0.2.0, arm64, Metal renderer
  Built for macOS 12 or newer


CREDITS AND LICENCE
-------------------

OpenTS is a community project led by ZivDero and tomsons26, built from
Electronic Arts' GPL-released source for related Command & Conquer games and
from reverse engineering of the original Tiberian Sun. This Apple Silicon port
is a derivative work; the reconstruction, the campaign data handling and the
gameplay code are theirs.

  https://github.com/OpenTS-Developers/OpenTS   -- the upstream project
  https://github.com/kanechen-bit/OpenTS_AppleSilicon  -- this port

Licensed under the GNU General Public License version 3 or later. Material
derived from Electronic Arts source remains subject to the additional GPL
Section 7 terms in LICENSE.md, included in the application bundle.

Command & Conquer is a trademark of Electronic Arts Inc. This project is not
affiliated with or endorsed by Electronic Arts.

The full licence texts are in
  OpenTS_AppleSilicon.app/Contents/Resources/
