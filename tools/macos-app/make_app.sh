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
clang \
	-x objective-c \
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
	<string>12.0</string>

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

plutil -lint "$app/Contents/Info.plist" > /dev/null

echo "make_app.sh: signing"
# Ad-hoc by default: enough to run on the machine that built it, and it keeps the
# build reproducible. A distributed build needs a Developer ID identity, which
# notarization also requires:
#
#   OPENTS_SIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
#       tools/macos-app/make_app.sh
#
# and then the notarization step, which has to come after signing:
#
#   xcrun notarytool submit dist/OpenTS.zip --keychain-profile <profile> --wait
#   xcrun stapler staple dist/OpenTS.app
#
# The zip is because notarytool takes an archive, not a bare bundle.
sign_identity="${OPENTS_SIGN_IDENTITY:--}"
codesign --force --sign "$sign_identity" --timestamp=none \
	--identifier org.opents.tiberiansun \
	"$app" 2>&1 | sed 's/^/  /'

echo "make_app.sh: verifying the seal"
codesign --verify --verbose=2 "$app" 2>&1 | sed 's/^/  /'

echo
echo "make_app.sh: built $app"
echo "  engine    $engine"
du -sh "$app" | sed 's/^/  size      /'
