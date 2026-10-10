#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Runs COMMAND inside a fresh headless labwc (Meadow's rc.xml and theme, no
# shell autostart) and returns its exit status. Skips (77) without labwc.
#
#   with-headless-labwc.sh [--size WxH] -- COMMAND [ARGS...]
#
# COMMAND sees WAYLAND_DISPLAY and XDG_RUNTIME_DIR of that compositor, plus
# LABWC_PID and MEADOW_LABWC_CONFIG_DIR (the config labwc was started with).
set -uo pipefail
src="$(cd "$(dirname "$0")/.." && pwd)"
size=1024x768
while [ $# -gt 0 ]; do
    case "$1" in
        --size) size=$2; shift 2 ;;
        --) shift; break ;;
        *) break ;;
    esac
done
command -v labwc >/dev/null || { echo "SKIP: labwc not installed"; exit 77; }

work="$(mktemp -d)"
mkdir -p "$work/run" "$work/share/themes" "$work/config"
chmod 700 "$work/run"
ln -s "$src/decorations/labwc/Meadow-Luna" "$work/share/themes/Meadow-Luna"
cp "$src/session/labwc/rc.xml" "$src/session/labwc/menu.xml" "$work/config/"
export XDG_RUNTIME_DIR="$work/run" XDG_DATA_HOME="$work/share"
export MEADOW_LABWC_CONFIG_DIR="$work/config"
unset WAYLAND_DISPLAY DISPLAY

WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1 WLR_HEADLESS_OUTPUTS=1 \
    labwc --config-dir "$work/config" >"$work/labwc.log" 2>&1 &
labwc_pid=$!
cleanup() { kill "$labwc_pid" 2>/dev/null; wait "$labwc_pid" 2>/dev/null; rm -rf "$work"; }
trap cleanup EXIT

for _ in $(seq 50); do
    sock=$(ls "$work/run" 2>/dev/null | grep -E '^wayland-[0-9]+$' | head -n1)
    [ -n "$sock" ] && break
    sleep 0.1
done
if [ -z "${sock:-}" ]; then
    echo "labwc did not start:"; cat "$work/labwc.log"; exit 1
fi
export WAYLAND_DISPLAY="$sock" LABWC_PID="$labwc_pid" QT_QPA_PLATFORM=wayland
# A fixed output size so screenshots are comparable.
command -v wlr-randr >/dev/null && wlr-randr --output HEADLESS-1 --custom-mode "$size" >/dev/null 2>&1
"$@"
status=$?
[ -n "${MEADOW_LABWC_LOG:-}" ] && cp "$work/labwc.log" "$MEADOW_LABWC_LOG"
exit $status
