#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# The real shell in a real compositor: headless labwc with Meadow's config,
# meadow-shell from the build tree on layer-shell surfaces, N test windows
# (one maximized, so the taskbar's exclusive zone shows), then a grim capture.
#
#   shell-session-screenshot.sh [BUILD_DIR OUT.png] [--windows N] [--open POPUP]
#                               [--notify] [--scheme luna-blue] [--grab OUT.png]
# BUILD_DIR defaults to $MEADOW_BUILD_DIR (screenshot-test.py passes --grab).
# Skips (77) without labwc or grim.
set -uo pipefail
src="$(cd "$(dirname "$0")/.." && pwd)"
build="${MEADOW_BUILD_DIR:-}" out=""
if [ $# -ge 2 ] && [ -d "$1" ]; then build="$1"; out="$2"; shift 2; fi
windows=10 open="" notify=0 scheme=luna-blue
while [ $# -gt 0 ]; do
    case "$1" in
        --windows) windows=$2; shift ;;
        --open) open=$2; shift ;;
        --notify) notify=1 ;;
        --scheme) scheme=$2; shift ;;
        --grab) out=$2; shift ;;
    esac
    shift
done
command -v grim >/dev/null || { echo "SKIP: grim not installed"; exit 77; }
build="$(cd "$build" && pwd)"; out="$(realpath -m "$out")"

if [ -z "${MEADOW_IN_LABWC:-}" ]; then
    export MEADOW_IN_LABWC=1
    exec dbus-run-session -- "$src/tools/with-headless-labwc.sh" --size 1024x768 -- \
        "$0" "$build" "$out" --windows "$windows" ${open:+--open "$open"} \
        $( [ $notify = 1 ] && echo --notify ) --scheme "$scheme"
fi

tmp=$(mktemp -d)
pids=()
cleanup() { kill "${pids[@]}" 2>/dev/null; wait 2>/dev/null; rm -rf "$tmp"; }
trap cleanup EXIT
export LC_ALL=C.UTF-8 QT_FORCE_STDERR_LOGGING=1 QT_QUICK_BACKEND=software
export MEADOW_THEME_DIR="$src/theme" MEADOW_ASSET_DIR="$src/theme/assets" QML_IMPORT_PATH="$build/qml"
# meadow-control-panel and meadow-shellctl from this tree (Start menu tools
# are shown only when found on PATH).
export PATH="$src/shell:$PATH"
export MEADOW_FAKE_TIME=2026-03-14T09:41:00 MEADOW_ICONS_GENERIC=1
export XDG_DATA_DIRS="$src/tests/fixtures" XDG_DATA_HOME="$tmp/data" XDG_CONFIG_HOME="$tmp/config"
export MEADOW_SHELL_DEFAULTS="$src/shell/data/shell.conf" MEADOW_WPCTL=/bin/false
export MEADOW_MENU_DIR="$src/session/menus" MEADOW_LAUNCH_LOG="$tmp/launches.log" USER=meadow
export MEADOW_THEME="$scheme"

"$build/shell/meadow-shell" ${open:+--open "$open"} > "$tmp/shell.log" 2>&1 &
pids+=($!)
sleep 1.5
apps=(pcmanfm-qt firefox xterm org.kde.kwrite thunderbird)
for i in $(seq 1 "$windows"); do
    app=${apps[$(( (i - 1) % ${#apps[@]} ))]}
    extra=()
    [ "$i" = 1 ] && extra=(--maximized)
    "$build/tools/decor-test/meadow-decor-test" --title "Window $i" --app-id "$app" \
        --size 360x220 "${extra[@]}" >/dev/null 2>&1 &
    pids+=($!)
    sleep 0.15
done
if [ $notify = 1 ]; then
    sleep 0.5
    gdbus call --session --dest org.freedesktop.Notifications --object-path /org/freedesktop/Notifications \
        --method org.freedesktop.Notifications.Notify meadow 0 "" "Updates are ready" \
        "Click here to review <b>3</b> updates." '[]' '{}' 0 >/dev/null 2>&1 ||
    busctl --user call org.freedesktop.Notifications /org/freedesktop/Notifications \
        org.freedesktop.Notifications Notify susssasa{sv}i meadow 0 "" "Updates are ready" \
        "Click here to review <b>3</b> updates." 0 0 0
fi
sleep 2.5
grim "$out"
status=$?
if grep -qE "qrc:|ERROR|CRITICAL|failed" "$tmp/shell.log"; then
    cat "$tmp/shell.log"; echo "meadow-shell reported problems (above)"; status=1
fi
exit $status
