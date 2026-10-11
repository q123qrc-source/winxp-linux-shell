#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Alt+Tab end to end in a real compositor: headless labwc, meadow-shell,
# three test windows A, B, C (C last, so active), keys through wtype.
# labwc does not run keybinds for virtual keyboards (wtype), so the test does
# what the A-Tab keybind does, `meadow-shellctl switch-window next`, while a
# background wtype holds Alt and later presses Tab / Escape and releases Alt.
# labwc also sends only its physical keyboards' modifiers when focus moves,
# so for the held-Alt steps the test creates MEADOW_TEST_ALT_HELD_FILE (Alt
# then counts as held when the switcher gets the keyboard); releasing Alt is
# still a real key event.
#   1. a quick Alt+Tab switches to the previous window (B)
#   2. Alt held, Tab, Tab, then Alt released switches two back (C, then A)
#   3. Alt held, Tab, Escape cancels (no switch)
# Screenshot of the open switcher in OUT_DIR.
#
#   alttab-session-test.sh BUILD_DIR [OUT_DIR]
# Skips (77) without labwc, wtype or grim.
set -uo pipefail
src="$(cd "$(dirname "$0")/.." && pwd)"
build="$(cd "$1" && pwd)"
out="$(realpath -m "${2:-$build/alttab-session}")"
for tool in labwc wtype grim; do
    command -v "$tool" >/dev/null || { echo "SKIP: $tool not installed"; exit 77; }
done
if [ -z "${MEADOW_IN_LABWC:-}" ]; then
    export MEADOW_IN_LABWC=1
    exec dbus-run-session -- "$src/tools/with-headless-labwc.sh" --size 1024x768 -- "$0" "$build" "$out"
fi

mkdir -p "$out"
tmp=$(mktemp -d)
pids=()
shell=""
cleanup() { kill "${pids[@]}" $shell 2>/dev/null; rm -rf "$tmp"; }
trap cleanup EXIT
fail() { echo "FAIL: $*"; grep -v "^$" "$tmp/shell.log" | tail -40; exit 1; }
wait_for() { local t=$1; shift; for _ in $(seq $((t * 10))); do "$@" && return 0; sleep 0.1; done; return 1; }
switches() { grep -c "switching to" "$tmp/shell.log"; }
switched_since() { [ "$(switches)" -gt "$1" ]; }
last_switch() { grep "switching to" "$tmp/shell.log" | tail -n1 | sed 's/.*switching to //'; }

export LC_ALL=C.UTF-8 QT_FORCE_STDERR_LOGGING=1 QT_QUICK_BACKEND=software
export MEADOW_THEME_DIR="$src/theme" MEADOW_ASSET_DIR="$src/theme/assets" QML_IMPORT_PATH="$build/qml"
export XDG_DATA_DIRS="$src/tests/fixtures" XDG_DATA_HOME="$tmp/data" XDG_CONFIG_HOME="$tmp/config"
export MEADOW_SHELL_DEFAULTS="$src/shell/data/shell.conf" MEADOW_MENU_DIR="$src/session/menus" MEADOW_WPCTL=/bin/false
export PATH="$src/shell:$PATH" MEADOW_TEST_ALT_HELD_FILE="$tmp/alt-held"

start_shell() {
    "$build/shell/meadow-shell" >> "$tmp/shell.log" 2>&1 &
    shell=$!
    wait_for 10 meadow-shellctl version >/dev/null 2>&1 || fail "meadow-shell did not start"
}
start_shell
for w in A B C; do
    "$build/tools/decor-test/meadow-decor-test" --title "Window $w" --app-id "org.meadow.Test$w" > /dev/null 2>&1 &
    pids+=($!)
    sleep 1
done
wtype -k Shift_L; sleep 0.3

echo "== 1. quick Alt+Tab (Alt already up when the switcher gets the keyboard)"
n=$(switches)
meadow-shellctl switch-window next
wait_for 5 switched_since "$n" || fail "no switch after a quick Alt+Tab"
[ "$(last_switch)" = org.meadow.TestB ] || fail "quick Alt+Tab went to $(last_switch), not B"
echo "ok: switched to B"
sleep 0.5

touch "$tmp/alt-held"

echo "== 2. Alt held: Alt+Tab, Tab, release"
n=$(switches)
wtype -P Alt_L -s 1500 -k Tab -s 1200 -p Alt_L &
keys=$!
sleep 0.4
meadow-shellctl switch-window next   # the A-Tab keybind
sleep 1.6
grim "$out/alttab.png"
wait "$keys"
wait_for 5 switched_since "$n" || fail "no switch after Alt+Tab+Tab"
# MRU after step 1: B, C, A -> two steps reach A.
[ "$(last_switch)" = org.meadow.TestA ] || fail "Alt+Tab+Tab went to $(last_switch), not A"
echo "ok: switched to A"
sleep 0.5

echo "== 3. Alt+Tab, Escape"
n=$(switches)
wtype -P Alt_L -s 1500 -k Escape -s 300 -p Alt_L &
keys=$!
sleep 0.4
meadow-shellctl switch-window next
wait "$keys"
sleep 0.5
[ "$(switches)" -eq "$n" ] || fail "Escape did not cancel ($(last_switch))"
echo "ok: cancelled"

if grep -E "qrc:.*(Error|error)|Binding loop|ReferenceError|TypeError" "$tmp/shell.log"; then
    fail "QML problems in the shell log"
fi
echo "PASS (screenshot in $out)"
