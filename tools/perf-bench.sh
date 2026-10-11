#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Measures Meadow against its performance budget in a headless labwc:
#
#   shell idle memory   PSS of meadow-shell 5 s after its first frame   < 120 MB
#   Start menu open     first frame showing the menu after it was asked
#                       for (median of 10 opens; the first one is reported
#                       separately as "cold")                             < 100 ms
#   greeter ready       process start to the first frame of the Welcome
#                       screen (sddm-greeter-qt6 --test-mode, else
#                       meadow-greeter-preview; median of 5)             < 1500 ms
#
# The timings come from the MEADOW_PERF_LOG marks (common/src/Perf.h); 150
# fixture applications fill the menus.
#
#   perf-bench.sh BUILD_DIR [--check] [--headroom FACTOR] [--json FILE] [--opens N]
#
# --check exits 1 when a result exceeds budget x FACTOR (default 1.0).
# Skips (77) without labwc.
set -uo pipefail
src="$(cd "$(dirname "$0")/.." && pwd)"
build="$(cd "${1:?build dir}" && pwd)"; shift
check=0 headroom=1.0 json="" opens_n=11
while [ $# -gt 0 ]; do
    case $1 in
        --check) check=1 ;;
        --headroom) headroom=$2; shift ;;
        --json) json=$(realpath -m "$2"); shift ;;
        --opens) opens_n=$2; shift ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
    shift
done
for tool in labwc python3; do
    command -v "$tool" >/dev/null || { echo "SKIP: $tool not installed"; exit 77; }
done
python3 -c "import dbus, gi.repository.GLib" 2>/dev/null || { echo "SKIP: python3-dbus/gobject missing"; exit 77; }

if [ -z "${MEADOW_IN_LABWC:-}" ]; then
    export MEADOW_IN_LABWC=1
    args=("$build"); [ "$check" = 1 ] && args+=(--check)
    args+=(--headroom "$headroom" --opens "$opens_n"); [ -n "$json" ] && args+=(--json "$json")
    exec dbus-run-session -- "$src/tools/with-headless-labwc.sh" --size 1280x800 -- "$0" "${args[@]}"
fi

BUDGET_RAM_MB=120 BUDGET_MENU_MS=100 BUDGET_GREETER_MS=1500
tmp=$(mktemp -d)
pids=()
cleanup() { kill "${pids[@]}" 2>/dev/null; wait 2>/dev/null; rm -rf "$tmp"; }
trap cleanup EXIT
fail() { echo "FAIL: $*"; [ -f "$tmp/shell.log" ] && tail -30 "$tmp/shell.log"; exit 1; }
wait_for() { local t=$1; shift; for _ in $(seq $((t * 20))); do "$@" && return 0; sleep 0.05; done; return 1; }
mark() { sed -n "s/.*meadow\.$1\.perf: $2 \([0-9.]*\) ms.*/\1/p" "$3"; }
median() { sort -n | awk '{a[NR]=$1} END {if (NR==0) print "nan"; else if (NR%2) print a[(NR+1)/2]; else print (a[NR/2]+a[NR/2+1])/2}'; }

# 150 applications across the menu categories.
mkdir -p "$tmp/apps/applications"
cats=(Utility Development Graphics Network Office AudioVideo Game System Settings Education)
for i in $(seq 1 150); do
    c=${cats[$((i % ${#cats[@]}))]}
    cat > "$tmp/apps/applications/org.meadow.Bench$i.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Bench Application $i
Comment=Fixture for tools/perf-bench.sh
Exec=true
Icon=application-x-executable
Categories=$c;
DESKTOP
done

export LC_ALL=C.UTF-8 QT_FORCE_STDERR_LOGGING=1 USER=meadow MEADOW_PERF_LOG=1 MEADOW_MUTE=1
export MEADOW_THEME_DIR="$src/theme" MEADOW_ASSET_DIR="$src/theme/assets" QML_IMPORT_PATH="$build/qml"
export MEADOW_ICON_DIRS="$src/toolkit-themes/icons"
export XDG_DATA_DIRS="$tmp/apps:$src/tests/fixtures:/usr/share" XDG_DATA_HOME="$tmp/data" XDG_CONFIG_HOME="$tmp/config"
export MEADOW_SHELL_DEFAULTS="$src/shell/data/shell.conf" MEADOW_MENU_DIR="$src/session/menus" MEADOW_WPCTL=/bin/false
export MEADOW_SYSTEM_SERVICES_ON_SESSION_BUS=1 PATH="$src/shell:$PATH"
# The shell uses its configured renderer (shell.conf [Performance],
# software by default) unless QT_QUICK_BACKEND is set. The greeter is SDDM's
# to configure; without a GPU (containers, CI) Mesa's llvmpipe would make
# its OpenGL numbers meaningless, so it gets the software scene graph here.
backend=${QT_QUICK_BACKEND:-"shell.conf default"}
greeter_backend=${QT_QUICK_BACKEND:-software}

python3 "$src/tools/mock-system-services.py" --no-battery --network none --logind --log "$tmp/logind.log" > "$tmp/mock.out" &
pids+=($!)
wait_for 5 grep -q ready "$tmp/mock.out" || fail "mock services did not start"

# --- Shell start and idle memory -------------------------------------------
"$build/shell/meadow-shell" > "$tmp/shell.log" 2>&1 &
shell_pid=$!
pids+=($shell_pid)
wait_for 30 grep -q "shell.perf: first-frame" "$tmp/shell.log" || fail "meadow-shell showed no frame"
services_ms=$(mark shell services-ready "$tmp/shell.log")
qml_ms=$(mark shell qml-loaded "$tmp/shell.log")
frame_ms=$(mark shell first-frame "$tmp/shell.log")
sleep 5
pss_kb() { awk '/^Pss:/ {print $2}' "/proc/$1/smaps_rollup"; }
uss_kb() { awk '/^Private_(Clean|Dirty):/ {s+=$2} END {print s}' "/proc/$1/smaps_rollup"; }
rss_kb() { awk '/^Rss:/ {print $2}' "/proc/$1/smaps_rollup"; }
idle_pss=$(pss_kb $shell_pid) idle_uss=$(uss_kb $shell_pid) idle_rss=$(rss_kb $shell_pid)

# --- Start menu -------------------------------------------------------------
shown_more() { [ "$(grep -c "start-menu-shown" "$tmp/shell.log")" -gt "$1" ]; }
opens=()
for i in $(seq 1 "$opens_n"); do
    before=$(grep -c "start-menu-shown" "$tmp/shell.log")
    meadow-shellctl toggle-start-menu
    wait_for 15 shown_more "$before" || fail "Start menu open $i showed no frame"
    req=$(mark shell start-menu-requested "$tmp/shell.log" | tail -1)
    shown=$(mark shell start-menu-shown "$tmp/shell.log" | tail -1)
    opens+=("$(awk -v a="$req" -v b="$shown" 'BEGIN {printf "%.1f", b - a}')")
    sleep 0.4
    meadow-shellctl toggle-start-menu
    sleep 0.4
    [ $((i % 10)) = 0 ] && echo "after $i opens: PSS $(awk '/^Pss:/ {printf "%.1f", $2 / 1024}' /proc/$shell_pid/smaps_rollup) MB" >&2
done
menu_cold=${opens[0]}
menu_median=$(printf '%s\n' "${opens[@]:1}" | median)
menu_max=$(printf '%s\n' "${opens[@]:1}" | sort -n | tail -1)
after_pss=$(pss_kb $shell_pid)
kill $shell_pid 2>/dev/null; wait $shell_pid 2>/dev/null

# --- Greeter ----------------------------------------------------------------
greeter_exe=$(command -v sddm-greeter-qt6 || true)
if [ -n "$greeter_exe" ]; then
    greeter_kind="sddm-greeter-qt6 --test-mode"
    # The build tree's libmeadow-common, not an installed one.
    greeter_cmd=(env LD_LIBRARY_PATH="$build/common${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
                 QT_QUICK_BACKEND="$greeter_backend" "$greeter_exe" --test-mode --theme "$src/greeter")
else
    greeter_kind="meadow-greeter-preview"
    greeter_cmd=(env QT_QUICK_BACKEND="$greeter_backend" "$build/tools/greeter-preview/meadow-greeter-preview")
fi
greeter_runs=()
for i in 1 2 3 4 5; do
    "${greeter_cmd[@]}" > "$tmp/greeter$i.log" 2>&1 &
    gp=$!
    wait_for 20 grep -q "greeter.perf: first-frame" "$tmp/greeter$i.log" || { kill $gp; cat "$tmp/greeter$i.log"; fail "greeter showed no frame"; }
    greeter_runs+=("$(mark greeter first-frame "$tmp/greeter$i.log")")
    kill $gp 2>/dev/null; wait $gp 2>/dev/null
done
greeter_median=$(printf '%s\n' "${greeter_runs[@]}" | median)

# --- Report -----------------------------------------------------------------
mb() { awk -v k="$1" 'BEGIN {printf "%.1f", k / 1024}'; }
cat <<REPORT
renderer: shell $backend, greeter $greeter_backend; labwc headless (pixman)
shell start: services ${services_ms} ms, QML ${qml_ms} ms, first frame ${frame_ms} ms
shell idle memory: PSS $(mb "$idle_pss") MB (budget $BUDGET_RAM_MB), private $(mb "$idle_uss") MB, RSS $(mb "$idle_rss") MB
shell memory after $opens_n Start menu opens: PSS $(mb "$after_pss") MB
start menu open: median ${menu_median} ms (budget $BUDGET_MENU_MS), max ${menu_max} ms, cold ${menu_cold} ms
greeter ready ($greeter_kind): median ${greeter_median} ms (budget $BUDGET_GREETER_MS), runs ${greeter_runs[*]}
REPORT
if [ -n "$json" ]; then
    cat > "$json" <<JSON
{"shell_renderer": "$backend", "greeter_renderer": "$greeter_backend", "shell_services_ms": $services_ms, "shell_qml_ms": $qml_ms, "shell_first_frame_ms": $frame_ms,
 "shell_idle_pss_mb": $(mb "$idle_pss"), "shell_idle_private_mb": $(mb "$idle_uss"), "shell_idle_rss_mb": $(mb "$idle_rss"),
 "shell_pss_after_menus_mb": $(mb "$after_pss"),
 "start_menu_median_ms": $menu_median, "start_menu_max_ms": $menu_max, "start_menu_cold_ms": $menu_cold,
 "greeter": "$greeter_kind", "greeter_ready_median_ms": $greeter_median}
JSON
fi

if [ "$check" = 1 ]; then
    over=0
    over_budget() { awk -v v="$1" -v b="$2" -v h="$headroom" 'BEGIN {exit !(v > b * h)}'; }
    over_budget "$(mb "$idle_pss")" $BUDGET_RAM_MB && { echo "OVER BUDGET: shell idle memory"; over=1; }
    over_budget "$menu_median" $BUDGET_MENU_MS && { echo "OVER BUDGET: Start menu open"; over=1; }
    over_budget "$greeter_median" $BUDGET_GREETER_MS && { echo "OVER BUDGET: greeter ready"; over=1; }
    [ "$over" = 0 ] && echo "within budget (x$headroom)"
    exit $over
fi
