#!/bin/sh
#
# Build OpenTS_AppleSilicon.dmg -- a disk image holding the application and a
# README for the person who opens it.
#
#   ./make_dmg.sh [output-directory]
#
# The image is what a player downloads, so it is the only place most of them
# will ever see the port. Two things follow from that:
#
#   * The README is plain text, written for someone who has never heard of a
#     port, and it says where the game files come from -- the single most
#     common question. It is a .txt rather than a document because it has to be
#     readable from the Finder with nothing installed.
#   * The app is built here rather than expected to exist, so the image cannot
#     be assembled from a stale bundle. It is signed after copying, because a
#     signature covers the path as well as the bytes and moving the bundle
#     invalidates it.
#
# The volume is left compressed (UDZO) and read-only, which is what makes it
# mount as a normal disk image rather than a folder.
#
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)

output_dir="${1:-$repo_root/dist}"
app_name="OpenTS_AppleSilicon"
dmg="$output_dir/$app_name.dmg"
staging="$output_dir/.dmg-staging"

bundle="$output_dir/$app_name.app"

echo "make_dmg.sh: building the application bundle"
OPENTS_PYTHON="${OPENTS_PYTHON:-python3}" "$script_dir/make_app.sh" "$output_dir"

if [ ! -d "$bundle" ]; then
	echo "make_dmg.sh: expected a bundle at $bundle and there is none" >&2
	exit 1
fi

echo "make_dmg.sh: staging the image contents"
rm -rf "$staging"
mkdir -p "$staging"

# -R preserves the bundle's symlinks and extended attributes. Copying without
# it produces an app that Gatekeeper rejects, and the .app has to survive a
# round trip through a disk image.
cp -R "$bundle" "$staging/"
cp "$script_dir/DMG_README.txt" "$staging/README.txt"

# A symlink to Applications, which is what makes the window a drag-and-drop
# target rather than something the user has to open and copy by hand.
ln -s /Applications "$staging/Applications"

echo "make_dmg.sh: laying the window out"
# Icon positions for the mounted window. The Finder reads these from a
# .DS_Store, and nothing on macOS writes one from the command line -- SetFile
# sets attributes, not view settings. Skipped with a warning rather than fatal,
# because the image is entirely usable without it: the icons just land in a
# default arrangement.
if ! "$script_dir/make_ds_store.py" "$staging"; then
	echo "make_dmg.sh: continuing without a custom window layout" >&2
	echo "  (install the ds_store module: python3 -m pip install ds_store)" >&2
fi

echo "make_dmg.sh: signing the staged bundle"
# Re-signed in place because the signature records the bundle's location, and
# staging is a different path from where make_app.sh signed it.
sign_identity="${OPENTS_SIGN_IDENTITY:--}"
if [ "$sign_identity" = "-" ]; then
	timestamp_flag="--timestamp=none"
else
	timestamp_flag="--timestamp"
fi

codesign --force --sign "$sign_identity" "$timestamp_flag" \
	--options runtime \
	--identifier org.opents.tiberiansun \
	"$staging/$app_name.app" 2>&1 | sed 's/^/  /'

codesign --verify --verbose=2 "$staging/$app_name.app" 2>&1 | sed 's/^/  /'

# Notarization has to happen before the image is made, because stapling attaches
# a ticket to the bundle and the image is built from what is inside it. A stapled
# bundle inside an unstapled disk image still works -- Gatekeeper reads the ticket
# from the bundle -- but stapling after the image exists would mean re-making the
# image, so the order here is the cheap one.
if [ -n "${OPENTS_NOTARY_PROFILE:-}" ]; then
	if [ "$sign_identity" = "-" ]; then
		echo "make_dmg.sh: OPENTS_NOTARY_PROFILE is set but the bundle is ad-hoc signed" >&2
		echo "  Notarization needs a Developer ID. Set OPENTS_SIGN_IDENTITY too." >&2
		exit 1
	fi

	echo "make_dmg.sh: notarizing"
	# ditto rather than zip: it is what preserves the bundle's symlinks and
	# extended attributes, which notarytool checks.
	archive="$output_dir/.notarize.zip"
	rm -f "$archive"
	ditto -c -k --keepParent "$staging/$app_name.app" "$archive"

	xcrun notarytool submit "$archive" \
		--keychain-profile "$OPENTS_NOTARY_PROFILE" \
		--wait 2>&1 | sed 's/^/  /'

	xcrun stapler staple "$staging/$app_name.app" 2>&1 | sed 's/^/  /'
	xcrun stapler validate "$staging/$app_name.app" 2>&1 | sed 's/^/  /'
	rm -f "$archive"

	# Re-signed above, so the seal is checked again after stapling touched it.
	codesign --verify --verbose=2 "$staging/$app_name.app" 2>&1 | sed 's/^/  /'
else
	echo "make_dmg.sh: skipping notarization (set OPENTS_NOTARY_PROFILE to submit)"
fi

echo "make_dmg.sh: creating the image"
rm -f "$dmg"

# -srcfolder rather than a staging copy, so the image is made from exactly what
# was just verified. UDZO is the compressed read-only format; the default for a
# .dmg anyone is going to download.
#
# hdiutil create is deprecated in favour of `diskutil image create`, but it is
# the only one of the two that takes -srcfolder, and building a staging image by
# hand only to convert it would be a longer way to the same file. The warning is
# harmless and the format it produces is current.
hdiutil create \
	-volname "$app_name" \
	-fs HFS+ \
	-fsargs "-c c=64,a=16,e=16" \
	-srcfolder "$staging" \
	-format UDZO \
	"$dmg" 2>&1 | sed 's/^/  /'

echo "make_dmg.sh: verifying the image"
# Mounted and read back, because an image that cannot be mounted is not
# distributable and that is cheap to find out here rather than after a download.
verify_mount=$(mktemp -d)

missing=0
if ! hdiutil attach "$dmg" -nobrowse -readonly -mountpoint "$verify_mount" > /dev/null 2>&1; then
	echo "make_dmg.sh: the image could not be mounted; not calling it a release" >&2
	rmdir "$verify_mount" 2>/dev/null || true
	exit 1
fi

# Recorded so the device can be detached by name. Detaching by mountpoint path
# is not reliable here and leaves the device attached, which makes the *next*
# run fail to mount with "Resource busy" -- a self-inflicted failure that looks
# like a corrupt image.
verify_device=$(hdiutil info | awk -v img="$dmg" '
	$1 == "image-path" && $3 == img { found = 1; next }
	found && $1 ~ /^\/dev\/disk/ { print $1; exit }
')

for expected in "$app_name.app" README.txt Applications; do
	if [ ! -e "$verify_mount/$expected" ]; then
		echo "make_dmg.sh: $expected is missing from the image" >&2
		missing=1
	fi
done

# The signature has to hold on the copy inside the image too, not just the
# staged one, since that is the copy that will run.
if ! codesign --verify "$verify_mount/$app_name.app" 2>/dev/null; then
	echo "make_dmg.sh: the app's signature does not verify inside the image" >&2
	missing=1
fi

if [ -n "$verify_device" ]; then
	hdiutil detach "$verify_device" > /dev/null 2>&1 || diskutil eject "$verify_device" > /dev/null 2>&1 || true
fi
rmdir "$verify_mount" 2>/dev/null || true

if [ "$missing" -ne 0 ]; then
	echo "make_dmg.sh: the image did not verify; not calling it a release" >&2
	exit 1
fi

rm -rf "$staging"

echo
echo "make_dmg.sh: built $dmg"
du -h "$dmg" | sed 's/^/  size     /'
echo "  contents $app_name.app, README.txt, Applications (symlink)"

if [ -n "${OPENTS_NOTARY_PROFILE:-}" ]; then
	echo "  signed with a Developer ID and notarized"
else
	cat <<'NOTE'

  Ad-hoc signed, so first launch on another Mac needs a right-click -> Open, or
  the xattr command in README.txt. To sign and notarize:

    OPENTS_SIGN_IDENTITY="Developer ID Application: ..." \
    OPENTS_NOTARY_PROFILE=<profile> \
        tools/macos-app/make_dmg.sh

  The profile holds the notarytool password:
    xcrun notarytool store-credentials <profile>
NOTE
fi
