#!/bin/sh
#
# Draw the OpenTS.app icon.
#
#   ./make_icon.sh <output.icns>
#
# Split out from make_app.sh so the icon can be re-drawn on its own while
# iterating on it, without paying for a bundle layout and a codesign each time.
#
set -eu

if [ $# -ne 1 ]; then
	echo "usage: $(basename "$0") <output.icns>" >&2
	exit 1
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

python_bin="${OPENTS_PYTHON:-python3}"
if ! "$python_bin" -c 'import PIL' > /dev/null 2>&1; then
	for candidate in /opt/homebrew/bin/python3 /usr/local/bin/python3 /usr/bin/python3; do
		if [ -x "$candidate" ] && "$candidate" -c 'import PIL' > /dev/null 2>&1; then
			python_bin="$candidate"
			break
		fi
	done
fi

if ! "$python_bin" -c 'import PIL' > /dev/null 2>&1; then
	echo "make_icon.sh: Pillow is required to draw the icon: $python_bin -m pip install Pillow" >&2
	exit 1
fi

mkdir -p "$(dirname -- "$1")"
exec "$python_bin" "$script_dir/make_icon.py" "$1"
