#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the meadow-cursors XCursor theme from the committed SVG sources
# (tools/gen-cursors.py writes them): each frame is rendered at every size
# with rsvg-convert, packed with xcursorgen, and every alias in cursors.txt
# becomes a symlink.
#
#   gen-cursors.sh SRC_DIR OUT_THEME_DIR
#     SRC_DIR        toolkit-themes/cursors/src (SVGs + cursors.txt)
#     OUT_THEME_DIR  receives cursors/ (index.theme is installed separately)
# Exit 77 when rsvg-convert or xcursorgen is missing.
set -euo pipefail
src="$1" out="$2"
for tool in rsvg-convert xcursorgen; do
    command -v "$tool" >/dev/null || { echo "SKIP: $tool not installed" >&2; exit 77; }
done
read -r _ _ sizes_and_ms < <(grep '^# sizes' "$src/cursors.txt")
sizes="${sizes_and_ms% frame_ms*}"
frame_ms="${sizes_and_ms##*frame_ms }"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
rm -rf "$out/cursors"
mkdir -p "$out/cursors"
grid=32
count=0
while read -r name hx hy frames aliases; do
    case "$name" in ''|'#'*) continue ;; esac
    cfg="$work/$name.cfg"
    : > "$cfg"
    for size in $sizes; do
        # Hot spot in pixels (grid units scaled, rounded).
        x=$(awk -v h="$hx" -v s="$size" -v g="$grid" 'BEGIN { printf "%d", h * s / g + 0.5 }')
        y=$(awk -v h="$hy" -v s="$size" -v g="$grid" 'BEGIN { printf "%d", h * s / g + 0.5 }')
        for ((i = 1; i <= frames; i++)); do
            if [ "$frames" = 1 ]; then svg="$src/$name.svg"; png="$work/$name-$size.png"; delay=""
            else svg="$src/$(printf '%s-%02d.svg' "$name" "$i")"; png="$work/$name-$size-$i.png"; delay=" $frame_ms"; fi
            rsvg-convert -w "$size" -h "$size" "$svg" -o "$png"
            echo "$size $x $y $png$delay" >> "$cfg"
        done
    done
    xcursorgen "$cfg" "$out/cursors/$name"
    for a in $aliases; do
        ln -sf "$name" "$out/cursors/$a"
    done
    count=$((count + 1))
done < "$src/cursors.txt"
echo "built $count cursors ($sizes px) in $out/cursors"
