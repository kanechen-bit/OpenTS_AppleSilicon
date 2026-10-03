#!/bin/sh
# Port-equivalent of cmake/GitStamp.cmake for NON-Windows / headless builds.
# Produces opents_version.h and opents_build.h into the directory given as $1
# (default: the script's own directory). These normally come from CMake at
# configure + build time; this lets the shim-route spike build without CMake.
set -eu
OUT="${1:-$(cd "$(dirname "$0")/generated" && pwd)}"
SRC_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

MAJOR=0
MINOR=2
PATCH=0
PRERELEASE=""
OFFICIAL=0

PACKED=$(printf '0x%X' $(( ($MAJOR << 16) | ($MINOR << 8) | $PATCH )) )
VERSION="$MAJOR.$MINOR.$PATCH"
IS_PRERELEASE=0
if [ -n "$PRERELEASE" ]; then
	VERSION="$VERSION-$PRERELEASE"
	IS_PRERELEASE=1
fi

COMMIT="unknown"
BRANCH="unknown"
DATE="unknown"
DIRTY=0
if command -v git >/dev/null 2>&1 && [ -d "$SRC_DIR/.git" ]; then
	COMMIT=$(git -C "$SRC_DIR" rev-parse --short HEAD 2>/dev/null || echo unknown)
	BRANCH=$(git -C "$SRC_DIR" rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)
	DATE=$(git -C "$SRC_DIR" log -1 --format=%cd --date=format:%Y-%m-%d 2>/dev/null || echo unknown)
	STATUS=$(git -C "$SRC_DIR" status --porcelain --untracked-files=no 2>/dev/null || true)
	[ -n "$STATUS" ] && DIRTY=1
fi

if [ "$OFFICIAL" -eq 1 ]; then
	DISPLAY="$VERSION"
elif [ "$COMMIT" = "unknown" ]; then
	DISPLAY="$VERSION"
elif [ "$DIRTY" -eq 1 ]; then
	DISPLAY="$VERSION ($COMMIT, modified)"
else
	DISPLAY="$VERSION ($COMMIT)"
fi
DESCRIPTION="$COMMIT on $BRANCH"
[ "$DIRTY" -eq 1 ] && DESCRIPTION="$DESCRIPTION (modified)"

cat > "$OUT/opents_version.h" <<EOF
/*******************************************************************************
 *                                O P E N  T S
 ******************************************************************************/
/* Port-generated stub of the CMake git-stamp header (cmake/GitStamp.cmake). */

#pragma once

#define OPENTS_VERSION_MAJOR  $MAJOR
#define OPENTS_VERSION_MINOR  $MINOR
#define OPENTS_VERSION_PATCH  $PATCH

#define OPENTS_VERSION_PACKED $PACKED

#define OPENTS_VERSION        "$VERSION"
#define OPENTS_IS_PRERELEASE  $IS_PRERELEASE

#define OPENTS_RC_FILEVERSION $MAJOR,$MINOR,$PATCH,0

#if OPENTS_IS_PRERELEASE
#define OPENTS_RC_PRERELEASE_FLAG VS_FF_PRERELEASE
#else
#define OPENTS_RC_PRERELEASE_FLAG 0x0L
#endif
EOF

cat > "$OUT/opents_build.h" <<EOF
/*******************************************************************************
 *                                O P E N  T S
 ******************************************************************************/
/* Port-generated stub of the CMake git-stamp header (cmake/GitStamp.cmake). */

#pragma once

#include "opents_version.h"

#define OPENTS_COMMIT            "$COMMIT"
#define OPENTS_BRANCH            "$BRANCH"
#define OPENTS_COMMIT_DATE       "$DATE"
#define OPENTS_COMMIT_DIRTY      $DIRTY

#define OPENTS_BUILD_DESCRIPTION "$DESCRIPTION"

#define OPENTS_VERSION_DISPLAY   "$DISPLAY"
EOF

echo "wrote $OUT/opents_version.h and opents_build.h (OpenTS $DISPLAY)"
