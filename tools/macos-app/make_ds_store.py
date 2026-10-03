#!/usr/bin/env python3
"""Write the Finder window layout for the disk image.

The Finder reads icon positions and the window's own settings from a
`.DS_Store` in the directory it is showing, and no macOS command line tool
writes one -- `SetFile` sets file attributes, not view settings. Without it the
image mounts as a plain unsorted folder, which for a disk image whose entire job
is "drag this to Applications" looks unfinished.

This writes the two records that matter, and only those, using the real
`ds_store` encoding rather than a hand-rolled approximation:

  * `Iloc` per item -- where its icon sits, in points from the window origin
  * `icvp` on the directory -- the icon view's own rectangle

It deliberately does not write `bwsp`, the window-placement plist. That record
carries a serialized binary plist this library cannot encode, and a `bwsp`
written by hand is the classic way to produce an image the Finder silently
falls back to a default window for. A default window with correctly placed
icons is the right trade: the positions are what the eye notices, and the size
is close enough not to matter.

    make_ds_store.py <directory>

Fails loudly rather than emitting a file it cannot fully write, because a
malformed .DS_Store is worse than none -- the Finder ignores it silently.
"""

import os
import struct
import sys

from ds_store import DSStore, DSStoreEntry

# Points from the window's top-left. The app sits top-left, the Applications
# symlink to its right, and the README underneath: read left to right, it is
# "here is the app, here is where it goes, here is what to read".
#
# The names must match what make_dmg.sh put in the image. The app is named with
# underscores because that is what the bundle is called, and the Finder shows
# that name verbatim -- matching on a prettified "OpenTS Apple Silicon" instead
# is the kind of thing that silently places nothing.
POSITIONS = {
    "OpenTS_AppleSilicon.app": (150, 190),
    "Applications": (430, 190),
    "README.txt": (150, 330),
}

# The icon-view rectangle, as (x, y, width, height) in the window.
ICON_VIEW_BOUNDS = (0, 0, 620, 440)


def main():
    if len(sys.argv) != 2:
        print(f"usage: {os.path.basename(sys.argv[0])} <directory>", file=sys.stderr)
        return 1

    directory = sys.argv[1]
    if not os.path.isdir(directory):
        print(f"make_ds_store.py: {directory} is not a directory", file=sys.stderr)
        return 1

    path = os.path.join(directory, ".DS_Store")
    placed = 0
    skipped = []

    # "w+" truncates, so a stale layout from a previous build cannot survive
    # into an image whose contents have changed.
    store = DSStore.open(path, "w+")
    try:
        for name, (x, y) in POSITIONS.items():
            if not os.path.exists(os.path.join(directory, name)):
                skipped.append(name)
                continue

            # Iloc is a blob of four big-endian int16s: x, y, then two fields
            # the Finder uses for its own bookkeeping and that must be zero.
            value = struct.pack(">4h", x, y, 0, 0)
            store.insert(DSStoreEntry(name, "Iloc", b"blob", value))
            placed += 1

        bounds = struct.pack(">4I", *ICON_VIEW_BOUNDS)
        store.insert(DSStoreEntry(".", "icvp", b"blob", bounds))
    finally:
        store.close()

    if placed == 0:
        print("make_ds_store.py: nothing placed; not writing an empty layout",
              file=sys.stderr)
        return 1

    size = os.path.getsize(path)
    print(f"wrote {path}: {placed} icons placed, {size} bytes")
    if skipped:
        print("  not found in the image, so not placed: " + ", ".join(skipped),
              file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main())
