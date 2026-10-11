#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Runs `meadow-shell --preview` in a deterministic sandbox for screenshots and
# QML tests: a private session bus, fixture applications, a fake clock,
# drawn icons, and optional mock services.
#
#   shell-preview.sh [BUILD_DIR] [--mocks] [--sni] [--battery N] [--network KIND]
#                    -- [meadow-shell args...]
#
# BUILD_DIR defaults to $MEADOW_BUILD_DIR (set by screenshot-test.py).
#
#   --mocks         fake wpctl + mock UPower/NetworkManager (tray icons)
#   --sni           a StatusNotifierItem test icon in the notification area
#   --recent        a few recent documents (My Recent Documents)
set -eu
src=$(cd "$(dirname "$0")/.." && pwd)
if [ $# -gt 0 ] && [ -d "$1" ]; then build=$1; shift; else build=${MEADOW_BUILD_DIR:?BUILD_DIR}; fi
mocks=0 sni=0 recent=0 battery=72 network=wired
while [ $# -gt 0 ]; do
    case "$1" in
        --mocks) mocks=1 ;;
        --sni) sni=1 ;;
        --recent) recent=1 ;;
        --battery) battery=$2; shift ;;
        --network) network=$2; shift ;;
        --) shift; break ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
    shift
done

if [ -z "${MEADOW_PREVIEW_INNER:-}" ]; then
    export MEADOW_PREVIEW_INNER=1
    exec dbus-run-session -- "$0" "$build" \
        $( [ $mocks = 1 ] && echo --mocks ) $( [ $sni = 1 ] && echo --sni ) $( [ $recent = 1 ] && echo --recent ) \
        --battery "$battery" --network "$network" -- "$@"
fi

tmp=$(mktemp -d)
pids=""
cleanup() { [ -n "$pids" ] && kill $pids 2>/dev/null; rm -rf "$tmp"; }
trap cleanup EXIT

export LC_ALL=C.UTF-8
export QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FORCE_STDERR_LOGGING=1
export MEADOW_THEME_DIR="$src/theme" MEADOW_ASSET_DIR="$src/theme/assets" QML_IMPORT_PATH="$build/qml"
# meadow-control-panel and meadow-shellctl from this tree (Start menu tools
# are shown only when found on PATH).
export PATH="$src/shell:$PATH"
export MEADOW_FAKE_TIME="${MEADOW_FAKE_TIME:-2026-03-14T09:41:00}" MEADOW_ICONS_GENERIC=1
export XDG_DATA_DIRS="$src/tests/fixtures" XDG_DATA_HOME="$tmp/data" XDG_CONFIG_HOME="$tmp/config"
export MEADOW_SHELL_DEFAULTS="$src/shell/data/shell.conf" MEADOW_MENU_DIR="$src/session/menus"
# Start menu items are recorded, not started (set MEADOW_LAUNCH_LOG= to run them).
export MEADOW_LAUNCH_LOG="${MEADOW_LAUNCH_LOG-$tmp/launches.log}"
export USER=meadow
export XKB_DEFAULT_LAYOUT="${XKB_DEFAULT_LAYOUT:-us}"
export MEADOW_WPCTL=/bin/false
unset MEADOW_LABWC_ENV_FILE

# A sandbox desktop folder (never the real ~/Desktop) with a few items.
mkdir -p "$tmp/config" "$tmp/Desktop/Holiday photos" "$tmp/Documents"
printf 'XDG_DESKTOP_DIR="%s"\nXDG_DOCUMENTS_DIR="%s"\n' "$tmp/Desktop" "$tmp/Documents" > "$tmp/config/user-dirs.dirs"
: > "$tmp/Desktop/Shopping list.txt"; : > "$tmp/Desktop/Budget 2026.ods"

wait_ready() { # file
    i=0; while [ $i -lt 50 ] && ! grep -q ready "$1" 2>/dev/null; do sleep 0.1; i=$((i + 1)); done
}
if [ $recent = 1 ]; then
    mkdir -p "$tmp/data" "$tmp/docs"
    : > "$tmp/docs/Shopping list.txt"; : > "$tmp/docs/Holiday plan.odt"; : > "$tmp/docs/Budget 2026.ods"
    cat > "$tmp/data/recently-used.xbel" <<XBEL
<?xml version="1.0" encoding="UTF-8"?>
<xbel version="1.0" xmlns:mime="http://www.freedesktop.org/standards/shared-mime-info">
  <bookmark href="file://$tmp/docs/Shopping%20list.txt" added="2026-03-14T09:00:00Z" modified="2026-03-14T09:00:00Z" visited="2026-03-14T09:30:00Z"/>
  <bookmark href="file://$tmp/docs/Holiday%20plan.odt" added="2026-03-13T09:00:00Z" modified="2026-03-13T09:00:00Z" visited="2026-03-13T09:00:00Z"/>
  <bookmark href="file://$tmp/docs/Budget%202026.ods" added="2026-03-12T09:00:00Z" modified="2026-03-12T09:00:00Z" visited="2026-03-12T09:00:00Z"/>
</xbel>
XBEL
fi
if [ $mocks = 1 ]; then
    export MEADOW_WPCTL="$src/tools/fake-wpctl" FAKE_WPCTL_STATE="$tmp/wpctl"
    export MEADOW_SYSTEM_SERVICES_ON_SESSION_BUS=1
    python3 "$src/tools/mock-system-services.py" --battery "$battery" --network "$network" > "$tmp/mock.out" &
    pids="$pids $!"
    wait_ready "$tmp/mock.out"
fi
if [ $sni = 1 ]; then
    # Registers once the shell's StatusNotifierWatcher is up.
    python3 "$src/tools/sni-test-item.py" --log "${SNI_LOG:-$tmp/sni.log}" > "$tmp/sni.out" &
    pids="$pids $!"
fi
"$build/shell/meadow-shell" --preview "$@"
