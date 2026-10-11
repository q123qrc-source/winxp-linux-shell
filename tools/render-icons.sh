#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Renders every meadow-icons SVG with rsvg-convert (fails on any that does not
# render) and, with --sheet DIR, writes contact sheets of the 48 px and 16 px
# drawings there (icons-48.png, icons-16.png) for review and the goldens.
#
#   render-icons.sh [--sheet DIR] [THEME_DIR]
# Exit 77 (skip) without rsvg-convert.
set -uo pipefail
src="$(cd "$(dirname "$0")/.." && pwd)"
sheet=""
if [ "${1:-}" = --sheet ]; then sheet="$2"; shift 2; fi
theme="${1:-$src/toolkit-themes/icons/meadow-icons}"
command -v rsvg-convert >/dev/null || { echo "SKIP: rsvg-convert not installed"; exit 77; }
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
count=0 failed=0
while IFS= read -r -d '' svg; do
    size=48
    case "$svg" in */16x16/*) size=16 ;; esac
    if ! rsvg-convert -w "$size" -h "$size" "$svg" -o "$tmp/out.png" 2>"$tmp/err"; then
        echo "FAIL: $svg: $(cat "$tmp/err")"
        failed=$((failed + 1))
    fi
    count=$((count + 1))
done < <(find -L "$theme" -name '*.svg' -print0)
[ "$failed" = 0 ] || exit 1
echo "rendered $count icon files"
if [ -n "$sheet" ]; then
    mkdir -p "$sheet"
    python3 "$src/tools/gen-icons.py" --contact-sheet "$tmp/48.svg"
    python3 "$src/tools/gen-icons.py" --contact-sheet "$tmp/16.svg" --small
    rsvg-convert "$tmp/48.svg" -o "$sheet/icons-48.png"
    rsvg-convert -z 2 "$tmp/16.svg" -o "$sheet/icons-16.png"
fi
