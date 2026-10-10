#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Validate a wayland-sessions .desktop file.
#
# Session entries need DesktopNames= (display-manager convention), which
# desktop-file-validate rejects for Type=Application. Check that key here,
# then run desktop-file-validate on the rest of the file.
# Exit 77 (ctest "skipped") if desktop-file-validate is missing.
set -euo pipefail
file="${1:?usage: validate-session-desktop.sh FILE}"

grep -qx 'DesktopNames=Meadow' "$file" || { echo "$file: DesktopNames=Meadow missing" >&2; exit 1; }
grep -qx 'Exec=meadow-session' "$file" || { echo "$file: Exec=meadow-session missing" >&2; exit 1; }

command -v desktop-file-validate >/dev/null 2>&1 || { echo "desktop-file-validate not installed; skipping"; exit 77; }
tmp="$(mktemp --suffix=.desktop)"
trap 'rm -f "$tmp"' EXIT
grep -v '^DesktopNames=' "$file" > "$tmp"
desktop-file-validate "$tmp"
echo "$file: OK"
