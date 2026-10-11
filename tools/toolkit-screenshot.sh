#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Screenshot a GTK 3, GTK 4 or Qt (Kvantum) window themed by the toolkit
# themes generated from theme.json, in a headless labwc with Meadow's
# decorations. The window is tools/toolkit-demo (no animation), so the
# result is the same on every run.
#
#   toolkit-screenshot.sh --grab OUT.png --app gtk3|gtk4|qt
#                         [--scheme luna-blue] [--menu] [--size 760x620]
#
# Fails when GTK reports a theme parsing error. Exit 77 (skip) when labwc,
# grim, python3-gobject (GTK) or the Qt demo / Kvantum (qt) is missing.
set -uo pipefail
src="$(cd "$(dirname "$0")/.." && pwd)"
build="${MEADOW_BUILD_DIR:-$src/build}"
scheme=luna-blue size=760x620 out="" app="" menu=0
while [ $# -gt 0 ]; do
    case "$1" in
        --grab) out="$2"; shift ;;
        --app) app="$2"; shift ;;
        --scheme) scheme="$2"; shift ;;
        --size) size="$2"; shift ;;
        --menu) menu=1 ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
    shift
done
[ -n "$out" ] && [ -n "$app" ] || { echo "usage: $0 --grab OUT.png --app gtk3|gtk4|qt [options]" >&2; exit 2; }
for tool in labwc grim wlr-randr; do
    command -v "$tool" >/dev/null || { echo "SKIP: $tool not installed"; exit 77; }
done
case "$app" in
    gtk3|gtk4)
        python3 -c "import gi; gi.require_version('Gtk', '${app#gtk}.0')" 2>/dev/null \
            || { echo "SKIP: python3-gobject with GTK ${app#gtk} not installed"; exit 77; } ;;
    qt)
        [ -x "$build/tools/toolkit-demo/meadow-qt-demo" ] || { echo "SKIP: meadow-qt-demo not built"; exit 77; }
        qtstyles="$(qtpaths6 --query QT_INSTALL_PLUGINS 2>/dev/null || echo /usr/lib64/qt6/plugins)/styles"
        ls "$qtstyles"/libkvantum.so >/dev/null 2>&1 || { echo "SKIP: Kvantum not installed"; exit 77; } ;;
    *) echo "unknown app $app" >&2; exit 2 ;;
esac

work="$(mktemp -d)"
chmod 700 "$work"
cleanup() { kill ${pids:-} "${compositor:-}" 2>/dev/null; wait 2>/dev/null; rm -rf "$work"; }
trap cleanup EXIT
mkdir -p "$work/share" "$work/config" "$work/runtime" "$work/home"
chmod 700 "$work/runtime"

# Themes straight from the tokens; icons from the source tree.
python3 "$src/tools/gen-toolkit-themes.py" --out "$work/share" --theme-root "$src/theme" --scheme "$scheme" || exit 1
read -r _ gtk_theme kv_theme < <(python3 "$src/tools/gen-toolkit-themes.py" --list --scheme "$scheme")
labwc_theme="$gtk_theme"
ln -s "$src/decorations/labwc/$labwc_theme/labwc" "$work/share/themes/$labwc_theme/labwc"
ln -s "$src/toolkit-themes/icons" "$work/share/icons"
cp "$src/session/labwc/rc.xml" "$src/session/labwc/menu.xml" "$work/config/"
sed -i "s|<name>Meadow-Luna</name>|<name>$labwc_theme</name>|" "$work/config/rc.xml"
mkdir -p "$work/home/.config/Kvantum"
cp -r "$work/share/Kvantum/$kv_theme" "$work/home/.config/Kvantum/"
printf '[General]\ntheme=%s\n' "$kv_theme" > "$work/home/.config/Kvantum/kvantum.kvconfig"

export HOME="$work/home" XDG_CONFIG_HOME="$work/home/.config" XDG_RUNTIME_DIR="$work/runtime"
export XDG_DATA_HOME="$work/share" XDG_DATA_DIRS="$work/share:/usr/share" MEADOW_NO_SHELL=1
export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1 LC_ALL=C.UTF-8
labwc --config-dir "$work/config" >"$work/labwc.log" 2>&1 &
compositor=$!
for _ in $(seq 100); do
    socket="$(find "$work/runtime" -maxdepth 1 -name 'wayland-*' ! -name '*.lock' | head -n1)"
    [ -n "$socket" ] && break
    sleep 0.1
done
[ -n "${socket:-}" ] || { cat "$work/labwc.log"; exit 1; }
export WAYLAND_DISPLAY="$(basename "$socket")"
wlr-randr --output HEADLESS-1 --custom-mode "$size" >/dev/null

# No session bus, portals or a11y bus: settings come from the demo itself.
unset DBUS_SESSION_BUS_ADDRESS
export NO_AT_BRIDGE=1 GSETTINGS_BACKEND=memory GDK_DEBUG=no-portals GDK_BACKEND=wayland GSK_RENDERER=cairo
font_families="$(python3 -c "import json,sys; t=json.load(open(sys.argv[1])); print(','.join(t['fonts']['ui']['families']))" "$src/theme/$scheme/theme.json")"
font_px="$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['fonts']['ui']['pixelSize'])" "$src/theme/$scheme/theme.json")"
args=()
[ "$menu" = 1 ] && args+=(--menu)
case "$app" in
    gtk3|gtk4)
        GTK_THEME="$gtk_theme" python3 "$src/tools/toolkit-demo/gtk-demo.py" --gtk "${app#gtk}" \
            --icon-theme meadow-icons "${args[@]}" >"$work/app.out" 2>"$work/app.err" & ;;
    qt)
        QT_QPA_PLATFORM=wayland QT_WAYLAND_DISABLE_WINDOWDECORATION=1 QT_STYLE_OVERRIDE=kvantum \
        QT_QPA_PLATFORMTHEME= "$build/tools/toolkit-demo/meadow-qt-demo" --icon-theme meadow-icons \
            --font "$font_families" --font-px "$font_px" "${args[@]}" >"$work/app.out" 2>"$work/app.err" & ;;
esac
pids=$!
for _ in $(seq 150); do
    grep -q ready "$work/app.out" 2>/dev/null && break
    kill -0 "$pids" 2>/dev/null || break
    sleep 0.1
done
grep -q ready "$work/app.out" || { echo "the demo did not start"; cat "$work/app.err"; exit 1; }
sleep 0.5
grim "$out" || exit 1
if grep -iE "theme parsing error|Failed to load|not a valid" "$work/app.err"; then
    echo "FAIL: the toolkit reported theme problems"
    exit 1
fi
