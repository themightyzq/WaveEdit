#!/bin/bash
# Rewrite an absolute dylib reference in a macOS Mach-O executable to an
# @rpath reference, reading the executable's ACTUAL load command rather than
# guessing the library's install-id (which varies by which symlink
# find_library resolved). Idempotent and safe to run repeatedly.
#
# Usage: relink_bundled_dylib.sh <executable> <lib-name-substring> <rpath-target>
#   e.g. relink_bundled_dylib.sh WaveEdit libSoundTouch @rpath/libSoundTouch.1.dylib
set -euo pipefail

exe="${1:?executable path required}"
needle="${2:?lib name substring required}"
target="${3:?rpath target required}"

[ -f "$exe" ] || { echo "relink: executable not found: $exe" >&2; exit 0; }

ref="$(otool -L "$exe" | awk -v n="$needle" '$1 ~ n {print $1; exit}')"

if [ -n "$ref" ] && [ "$ref" != "$target" ]; then
    install_name_tool -change "$ref" "$target" "$exe"
    echo "relink: $ref -> $target"
else
    echo "relink: nothing to do for $needle (ref='${ref:-none}')"
fi
