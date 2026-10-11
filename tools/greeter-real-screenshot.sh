#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Screenshot the theme running in the *real* SDDM greeter (test mode) inside a
# headless labwc, as a check on top of the mock-based screenshot tests.
#
#   greeter-real-screenshot.sh OUT.png [WIDTH HEIGHT [SCALE]]
#
# Needs: sddm (sddm-greeter-qt6), labwc, xorg-x11-server-Xwayland, grim,
# wlr-randr. Run from a source tree built in ./build. No root needed.
set -euo pipefail
out="$1"; w="${2:-1024}"; h="${3:-768}"; scale="${4:-1}"
src="$(cd "$(dirname "$0")/.." && pwd)"
build="${MEADOW_BUILD_DIR:-$src/build}"

runtime="$(mktemp -d)"
chmod 700 "$runtime"
export XDG_RUNTIME_DIR="$runtime" WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1
labwc >"$runtime/labwc.log" 2>&1 &
compositor=$!
cleanup() { kill "$compositor" ${greeter:-} 2>/dev/null || true; wait 2>/dev/null || true; rm -rf "$runtime"; }
trap cleanup EXIT

for _ in $(seq 100); do
    socket="$(find "$runtime" -maxdepth 1 -name 'wayland-*' ! -name '*.lock' | head -n1)"
    [ -n "$socket" ] && break
    sleep 0.1
done
[ -n "${socket:-}" ] || { echo "labwc did not start:"; cat "$runtime/labwc.log"; exit 1; }
WAYLAND_DISPLAY="$(basename "$socket")"
export WAYLAND_DISPLAY
wlr-randr --output HEADLESS-1 --custom-mode "${w}x${h}" --scale "$scale"

QT_QPA_PLATFORM=wayland QT_FORCE_STDERR_LOGGING=1 MEADOW_MUTE=1 LD_LIBRARY_PATH="$build/common" \
MEADOW_THEME_DIR="$src/theme" MEADOW_ASSET_DIR="$src/theme/assets" QML_IMPORT_PATH="$build/qml" \
    sddm-greeter-qt6 --test-mode --theme "$src/greeter" >"$runtime/greeter.log" 2>&1 &
greeter=$!
sleep 4
grim "$out"
grep -E "meadow\.|Error|TypeError|ReferenceError" "$runtime/greeter.log" || true
echo "wrote $out"
