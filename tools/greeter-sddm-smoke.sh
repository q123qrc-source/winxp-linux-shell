#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Load greeter/ in the real SDDM greeter (test mode, offscreen) and fail on
# QML errors. Skips (77) when sddm-greeter-qt6 is not installed.
#   greeter-sddm-smoke.sh SOURCE_DIR BUILD_DIR
set -uo pipefail
src="${1:?source dir}"; build="${2:?build dir}"
greeter="$(command -v sddm-greeter-qt6 || true)"
[ -n "$greeter" ] || { echo "sddm-greeter-qt6 not installed; skipping"; exit 77; }

log="$(mktemp)"
trap 'rm -f "$log"' EXIT
QT_QPA_PLATFORM=offscreen QT_FORCE_STDERR_LOGGING=1 MEADOW_MUTE=1 \
MEADOW_THEME_DIR="$src/theme" MEADOW_ASSET_DIR="$src/theme/assets" QML_IMPORT_PATH="$build/qml" \
    timeout 6 "$greeter" --test-mode --theme "$src/greeter" >"$log" 2>&1
status=$?
cat "$log"
if grep -E "ReferenceError|TypeError|is not a type|is not installed|Binding loop|polish\(\) loop|Cannot assign|Type .* unavailable" "$log" | grep -v PopupList; then
    echo "QML problems while loading the theme in SDDM" >&2
    exit 1
fi
grep -q "welcome screen ready" "$log" || { echo "theme did not finish loading" >&2; exit 1; }
# 124: still running when the timeout stopped it, which is the expected outcome.
[ "$status" -eq 124 ] || { echo "greeter exited early with status $status" >&2; exit 1; }
echo "sddm greeter smoke test: OK"
