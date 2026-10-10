#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Screenshot Meadow's labwc session (headless, no GPU): Meadow's rc.xml, menu
# and the generated decoration theme, with test windows open.
#
#   session-screenshot.sh --grab OUT.png [--scheme luna-blue] [--size 1024x600]
#                         [--maximized] [--xterm] [--menu]
#
# Needs labwc, grim, wlr-randr and a built tree (build/tools/decor-test).
# Exit 77 (skip) when a tool is missing. Runs no shell (MEADOW_NO_SHELL=1).
set -uo pipefail
src="$(cd "$(dirname "$0")/.." && pwd)"
build="${MEADOW_BUILD_DIR:-$src/build}"
scheme=luna-blue size=1024x600 out="" maximized=0 xterm=0
while [ $# -gt 0 ]; do
    case "$1" in
        --grab) out="$2"; shift ;;
        --scheme) scheme="$2"; shift ;;
        --size) size="$2"; shift ;;
        --maximized) maximized=1 ;;
        --xterm) xterm=1 ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
    shift
done
[ -n "$out" ] || { echo "usage: $0 --grab OUT.png [options]" >&2; exit 2; }
for tool in labwc grim wlr-randr; do
    command -v "$tool" >/dev/null || { echo "SKIP: $tool not installed"; exit 77; }
done
client="$build/tools/decor-test/meadow-decor-test"
[ -x "$client" ] || { echo "missing $client" >&2; exit 1; }
case "$scheme" in
    luna-blue) theme=Meadow-Luna ;;
    luna-olive) theme=Meadow-Luna-Olive ;;
    luna-silver) theme=Meadow-Luna-Silver ;;
    *) echo "unknown scheme $scheme" >&2; exit 2 ;;
esac

work="$(mktemp -d)"
chmod 700 "$work"
cleanup() { kill ${pids:-} "${compositor:-}" 2>/dev/null; wait 2>/dev/null; rm -rf "$work"; }
trap cleanup EXIT

# Theme from the source tree, config from session/labwc with this scheme.
mkdir -p "$work/share/themes" "$work/config" "$work/runtime"
chmod 700 "$work/runtime"
ln -s "$src/decorations/labwc/$theme" "$work/share/themes/$theme"
cp "$src/session/labwc/rc.xml" "$src/session/labwc/menu.xml" "$work/config/"
sed -i "s|<name>Meadow-Luna</name>|<name>$theme</name>|" "$work/config/rc.xml"

export XDG_RUNTIME_DIR="$work/runtime" XDG_DATA_HOME="$work/share" MEADOW_NO_SHELL=1
export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1 LC_ALL=C.UTF-8
x_before="$(ls /tmp/.X11-unix 2>/dev/null)"
labwc --config-dir "$work/config" >"$work/labwc.log" 2>&1 &
compositor=$!
for _ in $(seq 100); do
    socket="$(find "$work/runtime" -maxdepth 1 -name 'wayland-*' ! -name '*.lock' | head -n1)"
    [ -n "$socket" ] && break
    sleep 0.1
done
[ -n "${socket:-}" ] || { cat "$work/labwc.log"; exit 1; }
WAYLAND_DISPLAY="$(basename "$socket")"
export WAYLAND_DISPLAY
wlr-randr --output HEADLESS-1 --custom-mode "$size" >/dev/null

# Qt clients ask for server-side decorations (xdg-decoration).
export QT_QPA_PLATFORM=wayland QT_WAYLAND_DISABLE_WINDOWDECORATION=1 QT_QUICK_BACKEND=software
export MEADOW_THEME_DIR="$src/theme" MEADOW_THEME="$scheme"
pids=""
launch() { "$client" "$@" >>"$work/clients.log" 2>&1 & pids="$pids $!"; sleep 0.8; }
if [ "$maximized" = 1 ]; then
    launch --title "Maximized window" --maximized
else
    # Opened in order: the last one has focus, so the others are inactive.
    launch --title "My Documents" --size 420x240
    launch --title "Control Panel" --size 380x200
    if [ "$xterm" = 1 ] && command -v xterm >/dev/null; then
        # labwc gives DISPLAY only to processes it spawns; find the X socket
        # its (lazily started) Xwayland listens on.
        xsock="$(comm -13 <(printf '%s\n' "$x_before" | sort) <(ls /tmp/.X11-unix 2>/dev/null | sort) | head -n1)"
        if [ -n "$xsock" ]; then
            DISPLAY=":${xsock#X}" xterm -T "Command Prompt (XWayland)" -geometry 50x8 >>"$work/clients.log" 2>&1 &
            pids="$pids $!"
            sleep 2
        else
            echo "no Xwayland socket; skipping xterm" >&2
        fi
    fi
    launch --title "Untitled - Notepad" --size 400x220
fi
sleep 1
grim "$out"
grep -iE "error|warn" "$work/labwc.log" | grep -viE "xwayland|libinput|drm|seat|backend|EGL|session" >&2 || true
echo "wrote $out"
