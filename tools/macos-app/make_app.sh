#!/bin/sh
#
# Assemble OpenTS_AppleSilicon.app -- the macOS application bundle for the
# OpenTS engine.
#
#   ./make_app.sh [output-directory]
#
# The bundle holds three things:
#
#   Contents/MacOS/OpenTS           the launcher, and the bundle's main executable
#   Contents/MacOS/OpenTS-engine    the engine, named apart from the launcher so
#                                   the two never collide
#   Contents/Resources/             the icon, the shipped SUN.INI, and the licence
#                                   texts, none of which the engine opens at runtime
#
# The .app is named OpenTS_AppleSilicon so it is distinguishable from upstream's
# OpenTS.app in the Finder and in the Dock: two bundles with the same name and
# the same icon, differing only by which is selected, is a support problem
# waiting to happen. CFBundleName stays "OpenTS" because that is the name the
# menu bar and the About window show, and the project is still OpenTS.
#
# Two layout rules that are easy to get wrong and hard to debug afterwards:
#
#   * SUN.INI goes in Resources, NOT in MacOS. macOS refuses to seal a bundle whose
#     executable directory holds files the signature does not describe, and the
#     resulting app dies at launch with a bare "damaged" message.
#   * Nothing may be written inside the bundle at runtime. The engine's debug log
#     would be the obvious offender, so the launcher points it at the user
#     directory with OPENTS_DEBUG_DIR before handing over. A broken seal does not
#     stop an unsigned-ad-hoc app from running, but it does break notarization and
#     it makes Gatekeeper refuse the app on any machine but this one.
#
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)

output_dir="${1:-$repo_root/dist}"
app="$output_dir/OpenTS_AppleSilicon.app"

engine="${OPENTS_ENGINE:-$repo_root/build/bin/Game}"

if [ ! -x "$engine" ]; then
	echo "make_app.sh: no engine binary at $engine" >&2
	echo "  build it first:" >&2
	echo "    cmake --build $repo_root/build --target Game -j\$(sysctl -n hw.ncpu)" >&2
	exit 1
fi

mkdir -p "$output_dir"

# Settings the port was tuned with. Copied to the user directory on first run, so a
# fresh install comes up supersampled and sharp rather than on engine defaults.
settings_template="$repo_root/tools/macos-app/SUN.INI"

echo "make_app.sh: building the launcher"
# The same deployment target the engine is built with, so the launcher does not
# end up requiring a newer macOS than the engine it launches -- or than the
# bundle's LSMinimumSystemVersion claims. Read from CMake's cache so there is
# one value rather than two that can drift.
#
# Falls back to 12.0 when the cache is absent, which is the same default
# CMakeLists.txt applies.
deployment_target=$(sed -n 's/^CMAKE_OSX_DEPLOYMENT_TARGET:[^=]*=//p' \
	"$repo_root/build/CMakeCache.txt" 2>/dev/null | head -1)
: "${deployment_target:=12.0}"

clang \
	-x objective-c \
	-mmacosx-version-min="$deployment_target" \
	-framework Cocoa \
	-Wall -Wextra -Wno-unused-parameter \
	-o "$output_dir/.launcher.tmp" \
	"$script_dir/launcher.m"

echo "make_app.sh: laying out the bundle"
rm -rf "$app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources" "$app/Contents/Resources/Saved Games"

mv "$output_dir/.launcher.tmp" "$app/Contents/MacOS/OpenTS"
chmod +x "$app/Contents/MacOS/OpenTS"

cp "$engine" "$app/Contents/MacOS/OpenTS-engine"
chmod +x "$app/Contents/MacOS/OpenTS-engine"

cp "$settings_template" "$app/Contents/Resources/SUN.INI"

for licence in LICENSE.md THIRD_PARTY_NOTICES.md ACKNOWLEDGEMENTS.md; do
	if [ -f "$repo_root/$licence" ]; then
		cp "$repo_root/$licence" "$app/Contents/Resources/$licence"
	fi
done

echo "make_app.sh: drawing the icon"
"$script_dir/make_icon.sh" "$app/Contents/Resources/OpenTS.icns"

cat > "$app/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key>
	<string>OpenTS</string>
	<key>CFBundleDisplayName</key>
	<string>OpenTS</string>
	<key>CFBundleExecutable</key>
	<string>OpenTS</string>
	<key>CFBundleIdentifier</key>
	<string>org.opents.tiberiansun</string>
	<key>CFBundleIconFile</key>
	<string>OpenTS</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>0.2.0</string>
	<key>CFBundleVersion</key>
	<string>0.2.0</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>LSMinimumSystemVersion</key>
	<string>__DEPLOYMENT_TARGET__</string>

	<!-- The engine reads its data files from a folder the player points at, never
	     from inside the bundle, so the app has nothing to open on its own. -->
	<key>LSApplicationCategoryType</key>
	<string>public.app-category-games</string>
	<key>NSHighResolutionCapable</key>
	<true/>
	<key>NSSupportsAutomaticGraphicsSwitching</key>
	<true/>

	<!-- A Metal game, so it wants the discrete GPU on a dual-GPU Mac. -->
	<key>NSRequiresAquaSystemAppearance</key>
	<false/>

	<key>NSHumanReadableCopyright</key>
	<string>OpenTS is released under the GPL-3.0-or-later. Command &amp; Conquer: Tiberian Sun and its data files are the property of their respective owners; this bundle contains neither.</string>
</dict>
</plist>
PLIST

# Substituted after the heredoc rather than interpolated into it: the plist is
# quoted ('PLIST') so that nothing else in it is expanded by the shell, and
# unquoting it to interpolate one value would put every $ and backtick in the
# plist at the mercy of the shell.
#
# Written to a temporary file and moved into place, rather than edited in place
# with `sed -i`: the in-place form differs between BSD and GNU sed, and this
# script has to run on a build machine as well as this one.
plutil_file="$app/Contents/Info.plist"
sed "s/__DEPLOYMENT_TARGET__/$deployment_target/" "$plutil_file" > "$plutil_file.tmp"
mv "$plutil_file.tmp" "$plutil_file"

plutil -lint "$app/Contents/Info.plist" > /dev/null

echo "make_app.sh: verifying the deployment target"
# The failure this guards against is quiet and total: LSMinimumSystemVersion
# says one thing, the Mach-O says another, and the app is refused at launch on
# every machine older than the *build* machine's macOS -- with no message saying
# which version is required. It happened here, when a build on macOS 27 produced
# a bundle advertising 12.0 that would not start on 26.
#
# So: the plist, the launcher and the engine must all agree, and the agreement
# is checked rather than assumed.
plist_target=$(plutil -extract LSMinimumSystemVersion raw "$app/Contents/Info.plist")

for binary in OpenTS OpenTS-engine; do
	if [ ! -x "$app/Contents/MacOS/$binary" ]; then
		echo "make_app.sh: $binary is missing from the bundle" >&2
		exit 1
	fi

	# The minos field of LC_BUILD_VERSION, as "major.minor".
	binary_target=$(otool -l "$app/Contents/MacOS/$binary" \
		| awk '/LC_BUILD_VERSION/ { found = 1; next }
		       found && $1 == "minos" { print $2; exit }')

	if [ "$binary_target" != "$plist_target" ]; then
		echo "make_app.sh: $binary targets macOS $binary_target but the bundle" >&2
		echo "  advertises $plist_target. The app would fail to launch on" >&2
		echo "  anything older than $binary_target with no useful message." >&2
		echo "  Rebuild the engine with -DCMAKE_OSX_DEPLOYMENT_TARGET=$plist_target." >&2
		exit 1
	fi

	echo "  $binary: macOS $binary_target"
done

echo "  LSMinimumSystemVersion: $plist_target"

echo "make_app.sh: signing"
# Ad-hoc by default: enough to run on the machine that built it, and it keeps the
# build reproducible. A distributed build needs a Developer ID identity:
#
#   OPENTS_SIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
#       tools/macos-app/make_app.sh
#
# --timestamp is left off for an ad-hoc signature because there is no timestamp
# authority to ask; with a real identity it is switched on, since notarization
# rejects a Developer ID signature without one.
sign_identity="${OPENTS_SIGN_IDENTITY:--}"

if [ "$sign_identity" = "-" ]; then
	timestamp_flag="--timestamp=none"
else
	timestamp_flag="--timestamp"
fi

codesign --force --sign "$sign_identity" "$timestamp_flag" \
	--options runtime \
	--identifier org.opents.tiberiansun \
	"$app" 2>&1 | sed 's/^/  /'

echo "make_app.sh: verifying the seal"
codesign --verify --verbose=2 "$app" 2>&1 | sed 's/^/  /'

if [ "$sign_identity" = "-" ]; then
	cat <<'NOTE'

  Signed ad-hoc, so this build runs on this machine and nowhere else. To
  distribute it:

    1. Get a Developer ID Application certificate from Apple
       (free, $99/year, individual or organisation) and install it in the
       keychain, then confirm it appears:
         security find-identity -v -p codesigning

    2. Build with it:
         OPENTS_SIGN_IDENTITY="Developer ID Application: ..." make_app.sh

    3. Notarize. notarytool takes an archive rather than a bare bundle:
         ditto -c -k --keepParent dist/OpenTS_AppleSilicon.app dist/OpenTS.zip
         xcrun notarytool submit dist/OpenTS.zip \
             --keychain-profile <profile> --wait
         xcrun stapler staple dist/OpenTS_AppleSilicon.app

       The keychain profile stores an app-specific password:
         xcrun notarytool store-credentials <profile>

    4. Re-zip the stapled bundle for distribution, or run make_dmg.sh, which
       does the archive and submit steps for you when OPENTS_NOTARY_PROFILE
       is set.

  Until then, users need to right-click the app and choose Open on first
  launch, or clear the quarantine attribute. The README in the disk image
  explains this.
NOTE
fi

echo
echo "make_app.sh: built $app"
echo "  engine    $engine"
du -sh "$app" | sed 's/^/  size      /'
