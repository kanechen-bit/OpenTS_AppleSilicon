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
      LAN_Peers.txt        optional LAN peer IPs (see TROUBLESHOOTING)

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

The guest shows "Map: None" and its Accept button does nothing
    The host's game options (including the chosen map) are sent over the
    network as soon as a player joins and again whenever an option changes.
    This build fixes three things that could leave a guest stuck there: the
    options are now also fanned out through LAN_Peers.txt, they are accepted
    even if they arrive before the guest has learned the full player list,
    and opening the guest screen no longer wipes options that arrived early.
    The host also pushes a new map name immediately instead of on the next
    two-second tick.

    If it still happens, check /tmp/opents_lan.log on both Macs:
      host    "[LAN] gameopt to <name> at <ip>:<port>"
      guest   "[LAN] gameopt from '<name>' accepted" (or "ignored")
    A guest that only ever logs "ignored" or logs nothing is not receiving
    the host's packets -- recheck LAN_Peers.txt and the firewall on both
    Macs. A guest logging "accepted" but still showing "None" is a display
    problem; send the log lines in when reporting it.

A LAN game I hosted does not appear on the other Mac
    This is almost always a macOS permission or a network that filters
    broadcasts, not a game fault. LAN discovery broadcasts on UDP port 1234.
    Do this on BOTH Macs:

    1. If the LAN lobby shows a short "No peer traffic" or "Local Network
       access denied" message, read /tmp/opents_lan.log: it ends each lobby
       session with a "[LAN] diag:" line and a "[LAN] verdict:" line saying
       what the game saw. That is the fastest way to tell which Mac is at
       fault. (The message box only fits one line; the log has the details.)

    2. Put each Mac's IP in LAN_Peers.txt. This is the reliable fix when
       broadcasts are filtered (wireless client isolation, VPNs, guest
       networks, or a firewall dropping unsolicited inbound UDP). The game
       creates the file for you the first time you open the LAN lobby:

           ~/Library/Application Support/OpenTS/LAN_Peers.txt

       Add the OTHER Mac's IP, one per line, e.g.:

           192.168.1.20

       Find a Mac's IP with: ipconfig getifaddr en0
       (or en1 for Ethernet), or System Settings > Wi-Fi > Details.
       Do this on both Macs, then quit and relaunch the game -- the file is
       read when the LAN lobby opens. Every discovery packet is then also
       sent straight to those addresses, using the same UDP path the LAN
       probe already proved works. To confirm, check /tmp/opents_lan.log for
       "[LAN] peer added: <IP>:1234" on both machines.

    3. System Settings > Privacy & Security > Local Network: switch OpenTS on.
       If OpenTS is not listed, launch the game, choose Multiplayer >
       Network (IPX) once, then check the list again. macOS adds an app after
       its first attempt to reach the local network.
    4. System Settings > Network > Firewall > Options: if the firewall is on,
       add OpenTS and set it to "Allow incoming connections", or turn the
       firewall off for the test. The app is signed ad-hoc, so the
       "automatically allow signed software" option does not cover it.
    5. Both Macs must be on the same IP subnet (for example 192.168.1.x) and
       on a network that forwards broadcasts. Guest Wi-Fi, wireless isolation
       (AP/client isolation), and VPN/proxy software that installs a TUN
       interface suppress them: a full-tunnel VPN captures the 255.255.255.255
       broadcast even when the LAN subnet itself is reachable. Turn the VPN
       off for the test. Step 2 works even on networks that block broadcasts.
    6. Quit and relaunch the game after changing any setting, then host
       again. A permission granted while the game is running does not apply
       to the socket it already opened.
    7. This build is signed ad-hoc, so macOS may ask for Local Network access
       again after every rebuild. A Developer ID signature gives it a stable
       identity and makes the grant persist.

    To confirm the network itself is carrying the traffic, run the LAN probe
    on both Macs (see "Testing LAN reachability" below). If the probe sees
    packets but the lobby is still empty, the fault is in the game; if it sees
    nothing, it is a permission, firewall, or network-isolation problem.

    NOTE on the probe: it runs under Terminal, which has its own Local Network
    permission -- a working probe proves the NETWORK is fine, not that the
    game's own permission was granted. macOS grants Local Network access per
    application, so OpenTS has to be allowed separately.

The lobby tells me what macOS blocked
    The game diagnoses itself now. About five seconds after the LAN game-list
    dialog opens, it checks what the network transport actually did:

    - If macOS refused its sends, the game shows "Local Network access
      denied". Grant access in System Settings > Privacy & Security >
      Local Network, then relaunch.
    - If every received packet came from this Mac itself, the game shows
      "No peer traffic" and points at LAN_Peers.txt. That is the common
      case: macOS delivers our own all-ones broadcast back to us, which
      counts as received traffic but is not a peer.

    Every lobby session also appends a line to /tmp/opents_lan.log like:

      [LAN] diag: sendok=14 sendblocked=0 senderr=0 recv=25 selfdrop=25 lasterrno=0

      sendok        datagrams the system accepted for sending
      sendblocked   sends macOS refused (Local Network permission missing)
      recv          datagrams received from the network, before any filtering
      selfdrop      received datagrams that came from this Mac itself (normal:
                    the all-ones broadcast loops back; a healthy lobby shows
                    recv equal to selfdrop on a lone Mac)

    A "[LAN] verdict:" line follows with the full instructions. The
    "[LAN] peer added:" lines confirm that LAN_Peers.txt was read.

    When both Macs sit in the lobby with empty lists, look at the verdict on
    each: sendblocked>0 means the Local Network grant is missing on that Mac;
    sendok>0 with recv==selfdrop on both means every cross-machine packet is
    being dropped in transit -- the firewall allowlist, AP isolation, or
    LAN_Peers.txt are the places to look. Send the /tmp/opents_lan.log lines
    from both Macs when reporting the problem.

Testing LAN reachability
    lan_probe.py is in this disk image next to this README. On the first Mac,
    in Terminal (drag the file into Terminal to get its path):

        python3 /Volumes/OpenTS_AppleSilicon/lan_probe.py listen

    On the second Mac:

        python3 /Volumes/OpenTS_AppleSilicon/lan_probe.py send <first-Mac-IP>

    Every two seconds the sender emits three probes on UDP 1234: a unicast to
    the address given, a directed subnet broadcast, and the all-ones broadcast.
    The listener prints each datagram it receives and totals them by kind, so
    "unicast only" means broadcasts are being blocked while basic UDP works.
    Neither command needs the game data or administrator rights. Stop with
    Ctrl-C, then reverse the roles to test the other direction.


WHAT WORKS, AND WHAT DOES NOT
----------------------------

Works:
  - the campaign, skirmish, and save/load
  - full audio, including in-game speech
  - video playback
  - the whole main menu, with music

Not tested or not working:
  - Multiplayer on real LAN is not yet confirmed working. Two game instances
    on one Mac can form a session, exchange commands, recover from a forced
    desync, and reach the score screen using a local UDP relay. A two-Mac test
    did not show the hosted lobby; the engine's own send and receive paths were
    since verified working (the lobby now reports what macOS let through), so
    the remaining suspects are the per-application Local Network grant and the
    application firewall on the machines involved -- both are per-app and the
    probe script runs under Terminal, whose grant does not carry over. See
    "A LAN game I hosted does not appear on the other Mac" above. CnCNet has
    not been exercised.
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
