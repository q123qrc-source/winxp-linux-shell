#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Start headless labwc with Meadow's session config and each generated theme,
# and fail on anything labwc reports as an error. Skips (77) without labwc.
set -uo pipefail
src="$(cd "$(dirname "$0")/.." && pwd)"
command -v labwc >/dev/null || { echo "SKIP: labwc not installed"; exit 77; }
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
fails=0
for theme_dir in "$src"/decorations/labwc/*/; do
    theme="$(basename "$theme_dir")"
    rm -rf "$work/run" "$work/share" "$work/config"
    mkdir -p "$work/run" "$work/share/themes" "$work/config"
    chmod 700 "$work/run"
    ln -s "$theme_dir" "$work/share/themes/$theme"
    cp "$src/session/labwc/rc.xml" "$src/session/labwc/menu.xml" "$work/config/"
    sed -i "s|<name>Meadow-Luna</name>|<name>$theme</name>|" "$work/config/rc.xml"
    XDG_RUNTIME_DIR="$work/run" XDG_DATA_HOME="$work/share" MEADOW_NO_SHELL=1 \
    WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1 \
        timeout 3 labwc --config-dir "$work/config" >"$work/log" 2>&1
    errors="$(grep -E "\[ERROR\]" "$work/log" | grep -viE "xwayland|libinput|seat|session|drm" || true)"
    if [ -n "$errors" ]; then
        echo "$theme: labwc reported errors:"; echo "$errors"; fails=$((fails + 1))
    else
        echo "$theme: OK"
    fi
done
exit "$fails"
