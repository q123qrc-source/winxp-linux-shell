#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Run the Meadow session from this source tree, nested in the current Wayland
# desktop (labwc's wayland backend opens it as a window):
#
#   tools/run-session.sh [--scheme luna-olive] [--no-shell]
#
# Uses session/labwc, the generated decorations and build/shell/meadow-shell.
# Nothing is installed or written outside a temporary directory.
set -euo pipefail
src="$(cd "$(dirname "$0")/.." && pwd)"
build="${MEADOW_BUILD_DIR:-$src/build}"
scheme=luna-blue
while [ $# -gt 0 ]; do
    case "$1" in
        --scheme) scheme="$2"; shift ;;
        --no-shell) export MEADOW_NO_SHELL=1 ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
    shift
done
[ -n "${WAYLAND_DISPLAY:-}" ] || { echo "run this inside a Wayland session" >&2; exit 1; }
case "$scheme" in
    luna-blue) theme=Meadow-Luna ;; luna-olive) theme=Meadow-Luna-Olive ;; luna-silver) theme=Meadow-Luna-Silver ;;
    *) echo "unknown scheme $scheme" >&2; exit 2 ;;
esac

stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/data/labwc" "$stage/share/themes" "$stage/bin"
for t in "$src"/decorations/labwc/*/; do ln -s "$t" "$stage/share/themes/$(basename "$t")"; done
cp "$src"/session/labwc/{rc.xml,menu.xml,environment,autostart} "$stage/data/labwc/"
sed -i "s|<name>Meadow-Luna</name>|<name>$theme</name>|" "$stage/data/labwc/rc.xml"
ln -s "$build/shell/meadow-shell" "$stage/bin/meadow-shell"
ln -s "$src/shell/meadow-shellctl" "$stage/bin/meadow-shellctl"

export PATH="$stage/bin:$PATH" XDG_DATA_HOME="$stage/share" MEADOW_DATADIR="$stage/data"
export MEADOW_THEME_DIR="$src/theme" MEADOW_ASSET_DIR="$src/theme/assets" QML_IMPORT_PATH="$build/qml" MEADOW_THEME="$scheme"
export WLR_BACKENDS=wayland XDG_CONFIG_HOME="$stage/config"   # ignore ~/.config/meadow/labwc
exec "$src/session/meadow-session"
