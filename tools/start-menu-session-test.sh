#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# End to end in a real compositor: headless labwc, meadow-shell on layer-shell
# surfaces, keyboard input through the virtual-keyboard protocol (wtype).
#   1. Windows key -> type-ahead "a" (All Programs) -> Right -> Games ->
#      Right -> Enter launches a fixture program; its window must appear.
#   2. Win+R (meadow-shellctl run) -> type a command -> Enter launches it.
#   3. Ctrl+Alt+Del (meadow-shellctl log-off) shows Log Off over the gray
#      capture; Escape closes it.
#   4. Turn Off Computer -> Right, Right (Restart) -> Enter reaches logind
#      (a mock on the session bus; nothing restarts).
# Screenshots of the open Start menu and dialogs go to OUT_DIR.
#
#   start-menu-session-test.sh BUILD_DIR [OUT_DIR]
# Skips (77) without labwc, wtype or grim.
set -uo pipefail
src="$(cd "$(dirname "$0")/.." && pwd)"
build="$(cd "$1" && pwd)"
out="$(realpath -m "${2:-$build/start-menu-session}")"
for tool in labwc wtype grim python3; do
    command -v "$tool" >/dev/null || { echo "SKIP: $tool not installed"; exit 77; }
done
python3 -c "import dbus, gi.repository.GLib" 2>/dev/null || { echo "SKIP: python3-dbus/gobject missing"; exit 77; }

if [ -z "${MEADOW_IN_LABWC:-}" ]; then
    export MEADOW_IN_LABWC=1
    exec dbus-run-session -- "$src/tools/with-headless-labwc.sh" --size 1024x768 -- "$0" "$build" "$out"
fi

mkdir -p "$out"
tmp=$(mktemp -d)
pids=()
cleanup() { kill "${pids[@]}" 2>/dev/null; pkill -f "meadow-decor-test --title (Launched from Start|From Run)" 2>/dev/null; rm -rf "$tmp"; }
trap cleanup EXIT
fail() { echo "FAIL: $*"; [ -f "$tmp/shell.log" ] && grep -v "^$" "$tmp/shell.log" | tail -40; exit 1; }
wait_for() { # seconds, command...
    local t=$1; shift
    for _ in $(seq $((t * 10))); do "$@" && return 0; sleep 0.1; done
    return 1
}

# A program in Games that opens a real window.
mkdir -p "$tmp/apps/applications"
cat > "$tmp/apps/applications/org.meadow.LaunchTest.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Launch Test
Exec=$build/tools/decor-test/meadow-decor-test --title "Launched from Start" --app-id org.meadow.LaunchTest
Icon=applications-games
Categories=Game;
DESKTOP

export LC_ALL=C.UTF-8 QT_FORCE_STDERR_LOGGING=1 QT_QUICK_BACKEND=software USER=meadow
export MEADOW_THEME_DIR="$src/theme" MEADOW_ASSET_DIR="$src/theme/assets" QML_IMPORT_PATH="$build/qml"
export XDG_DATA_DIRS="$tmp/apps:$src/tests/fixtures" XDG_DATA_HOME="$tmp/data" XDG_CONFIG_HOME="$tmp/config"
export MEADOW_SHELL_DEFAULTS="$src/shell/data/shell.conf" MEADOW_MENU_DIR="$src/session/menus" MEADOW_WPCTL=/bin/false
export MEADOW_SYSTEM_SERVICES_ON_SESSION_BUS=1 PATH="$src/shell:$PATH"

python3 "$src/tools/mock-system-services.py" --no-battery --network none --logind --log "$tmp/logind.log" > "$tmp/mock.out" &
pids+=($!)
wait_for 5 grep -q ready "$tmp/mock.out" || fail "mock services did not start"

"$build/shell/meadow-shell" > "$tmp/shell.log" 2>&1 &
pids+=($!)
wait_for 10 meadow-shellctl version >/dev/null 2>&1 || fail "meadow-shell did not start"
sleep 1

# The first key from a new virtual keyboard can be lost while clients switch
# to its keymap: send a harmless one first.
wtype -k Shift_L
sleep 0.3

echo "== 1. Start menu -> All Programs -> Games -> Launch Test"
meadow-shellctl toggle-start-menu
sleep 0.6
grim "$out/start-menu.png"
wtype a; sleep 0.2
wtype -k Right; sleep 0.4
wtype g; sleep 0.2
wtype -k Right; sleep 0.4
grim "$out/all-programs.png"
wtype -k Return
wait_for 10 pgrep -f "meadow-decor-test --title Launched from Start" >/dev/null || fail "Launch Test did not start"
echo "ok: launched from the Start menu"
sleep 1.5   # let its window map (and take focus) before the next step

echo "== 2. Run dialog"
meadow-shellctl run
sleep 0.8
wtype "$build/tools/decor-test/meadow-decor-test --title 'From Run' --app-id org.meadow.FromRun"
sleep 0.3
grim "$out/run.png"
wtype -k Return
wait_for 10 pgrep -f "meadow-decor-test --title From Run" >/dev/null || fail "Run did not start the program"
echo "ok: launched from Run"

echo "== 3. Log Off dialog"
meadow-shellctl log-off
sleep 2.5   # capture, then the fade to gray
grim "$out/log-off.png"
wtype -k Escape
sleep 0.5

echo "== 4. Turn Off Computer -> Restart"
meadow-shellctl turn-off
sleep 2.5
grim "$out/turn-off.png"
wtype -k Right; wtype -k Right; sleep 0.2
wtype -k Return
wait_for 5 grep -q "Reboot true" "$tmp/logind.log" || fail "Restart did not reach logind ($(cat "$tmp/logind.log" 2>/dev/null))"
echo "ok: Restart reached logind (interactive)"

if grep -E "qrc:.*(Error|error)|Binding loop|ReferenceError|TypeError" "$tmp/shell.log"; then
    fail "QML problems in the shell log"
fi
echo "PASS (screenshots in $out)"
