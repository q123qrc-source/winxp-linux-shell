#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Renders a review sheet of the icon or cursor theme to a PNG (screenshot
# goldens): every icon at 48 px with its name, every icon at 16 px (shown 2x),
# or every cursor (first frames, 2x).
#
#   theme-sheet.sh --grab OUT.png --kind icons-48|icons-16|cursors
# Exit 77 (skip) without rsvg-convert.
set -euo pipefail
src="$(cd "$(dirname "$0")/.." && pwd)"
out="" kind=""
while [ $# -gt 0 ]; do
    case "$1" in
        --grab) out="$2"; shift ;;
        --kind) kind="$2"; shift ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
    shift
done
[ -n "$out" ] && [ -n "$kind" ] || { echo "usage: $0 --grab OUT.png --kind icons-48|icons-16|cursors" >&2; exit 2; }
command -v rsvg-convert >/dev/null || { echo "SKIP: rsvg-convert not installed"; exit 77; }
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
case "$kind" in
    icons-48) python3 "$src/tools/gen-icons.py" --contact-sheet "$tmp/s.svg"; zoom=1 ;;
    icons-16) python3 "$src/tools/gen-icons.py" --contact-sheet "$tmp/s.svg" --small; zoom=2 ;;
    cursors) python3 "$src/tools/gen-cursors.py" --sheet "$tmp/s.svg"; zoom=2 ;;
    *) echo "unknown kind $kind" >&2; exit 2 ;;
esac
rsvg-convert -z "$zoom" "$tmp/s.svg" -o "$out"
