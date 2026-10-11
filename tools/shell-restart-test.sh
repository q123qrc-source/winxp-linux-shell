#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Crash resilience of session/meadow-shell-supervise.
#   1. With a fake shell: a crash is restarted with MEADOW_RESTARTED=1; a
#      clean exit or SIGTERM ends supervision; a shell that always crashes is
#      given up after 6 runs, with a notification; a second supervisor
#      started while one runs exits at once.
#   2. With the real meadow-shell in headless labwc: SIGSEGV -> a new shell
#      answers on D-Bus and skipped the logon sound.
#
#   shell-restart-test.sh BUILD_DIR
# Part 2 is skipped (exit 77 if part 1 passed) without labwc.
set -uo pipefail
src="$(cd "$(dirname "$0")/.." && pwd)"
build="$(cd "${1:?build dir}" && pwd)"
supervise="$src/session/meadow-shell-supervise"

if [ -z "${MEADOW_IN_LABWC:-}" ]; then
    tmp=$(mktemp -d)
    trap 'rm -rf "$tmp"' EXIT
    fail() { echo "FAIL: $*"; exit 1; }
    export XDG_RUNTIME_DIR="$tmp/run" MEADOW_SHELL_RESTART_DELAY=0.05
    mkdir -p "$XDG_RUNTIME_DIR" "$tmp/bin"
    unset WAYLAND_DISPLAY
    # notify-send and logger stand-ins that record their arguments.
    printf '#!/bin/sh\necho "$*" >> "%s/notify.log"\n' "$tmp" > "$tmp/bin/notify-send"
    printf '#!/bin/sh\necho "$*" >> "%s/logger.log"\n' "$tmp" > "$tmp/bin/logger"
    chmod +x "$tmp/bin/"*
    export PATH="$tmp/bin:$PATH"

    # fake shell: logs "run <MEADOW_RESTARTED>", then does what $tmp/plan says
    # for this run (crash, exit0, term, or sleep).
    cat > "$tmp/fake-shell" <<EOF
#!/bin/bash
echo "run \${MEADOW_RESTARTED:-0}" >> "$tmp/runs"
n=\$(wc -l < "$tmp/runs")
action=\$(sed -n "\${n}p" "$tmp/plan")
case \$action in
    crash) kill -SEGV \$\$ ;;
    abort) kill -ABRT \$\$ ;;
    exit0) exit 0 ;;
    term) kill -TERM \$\$ ;;
    sleep) sleep 30 ;;
    *) kill -SEGV \$\$ ;;
esac
EOF
    chmod +x "$tmp/fake-shell"
    export MEADOW_SHELL="$tmp/fake-shell"
    reset() { rm -f "$tmp/runs" "$tmp/notify.log" "$tmp/logger.log"; printf '%s\n' "$@" > "$tmp/plan"; }

    echo "== crash, crash, then a clean exit"
    reset crash abort exit0
    timeout 20 "$supervise"; status=$?
    [ "$status" = 0 ] || fail "supervisor status $status"
    [ "$(cat "$tmp/runs" | tr '\n' ' ')" = "run 0 run 1 run 1 " ] || fail "runs: $(cat "$tmp/runs")"
    grep -q "restarting (2)" "$tmp/logger.log" || fail "restart not logged"
    echo ok

    echo "== SIGTERM stops supervision"
    reset term
    timeout 20 "$supervise"; status=$?
    [ "$status" = 0 ] && [ "$(wc -l < "$tmp/runs")" = 1 ] || fail "SIGTERM was restarted"
    echo ok

    echo "== a shell that always crashes is given up"
    reset
    timeout 30 "$supervise"; status=$?
    [ "$status" = 1 ] || fail "supervisor status $status"
    [ "$(wc -l < "$tmp/runs")" = 6 ] || fail "$(wc -l < "$tmp/runs") runs, expected 6"
    grep -q "Right-click the desktop" "$tmp/notify.log" 2>/dev/null || { sleep 0.5; grep -q "Right-click the desktop" "$tmp/notify.log"; } \
        || fail "no notification"
    grep -q "not restarting" "$tmp/logger.log" || fail "give-up not logged"
    echo ok

    echo "== one supervisor per session"
    reset sleep
    "$supervise" & first=$!
    for _ in $(seq 50); do [ -f "$tmp/runs" ] && break; sleep 0.1; done
    timeout 5 "$supervise"; status=$?
    [ "$status" = 0 ] && [ "$(wc -l < "$tmp/runs")" = 1 ] || fail "second supervisor started a shell"
    grep -q "already supervised" "$tmp/logger.log" || fail "second supervisor did not say why"
    kill $first; pkill -f "$tmp/fake-shell"; wait $first 2>/dev/null
    echo ok

    command -v labwc >/dev/null || { echo "SKIP: labwc not installed (real-shell part)"; exit 77; }
    python3 -c "import dbus, gi.repository.GLib" 2>/dev/null || { echo "SKIP: python3-dbus/gobject missing"; exit 77; }
    export MEADOW_IN_LABWC=1
    exec dbus-run-session -- "$src/tools/with-headless-labwc.sh" -- "$0" "$build"
fi

# --- Part 2: the real shell in labwc ---------------------------------------
tmp=$(mktemp -d)
pids=()
# Only this test's processes: ctest runs other session tests in parallel.
supervisor=""
own_shell() { [ -n "$supervisor" ] && pgrep -P "$supervisor" -x meadow-shell | head -1; }
cleanup() {
    local s; s=$(own_shell)
    kill "${pids[@]}" 2>/dev/null
    [ -n "$s" ] && kill -TERM "$s" 2>/dev/null
    wait 2>/dev/null; rm -rf "$tmp"
}
trap cleanup EXIT
fail() { echo "FAIL: $*"; tail -30 "$tmp/shell.log"; exit 1; }
wait_for() { local t=$1; shift; for _ in $(seq $((t * 10))); do "$@" && return 0; sleep 0.1; done; return 1; }

export LC_ALL=C.UTF-8 QT_FORCE_STDERR_LOGGING=1 QT_QUICK_BACKEND=software USER=meadow MEADOW_MUTE=1
export MEADOW_THEME_DIR="$src/theme" MEADOW_ASSET_DIR="$src/theme/assets" QML_IMPORT_PATH="$build/qml"
export XDG_DATA_DIRS="$src/tests/fixtures" XDG_DATA_HOME="$tmp/data" XDG_CONFIG_HOME="$tmp/config"
export MEADOW_SHELL_DEFAULTS="$src/shell/data/shell.conf" MEADOW_MENU_DIR="$src/session/menus" MEADOW_WPCTL=/bin/false
export MEADOW_SYSTEM_SERVICES_ON_SESSION_BUS=1 PATH="$src/shell:$PATH"
export MEADOW_SHELL="$build/shell/meadow-shell" MEADOW_SHELL_RESTART_DELAY=0.2
unset MEADOW_RESTARTED

python3 "$src/tools/mock-system-services.py" --no-battery --network none --logind > "$tmp/mock.out" &
pids+=($!)
wait_for 5 grep -q ready "$tmp/mock.out" || fail "mock services did not start"

"$src/session/meadow-shell-supervise" > "$tmp/shell.log" 2>&1 &
supervisor=$!
pids+=($supervisor)
wait_for 15 meadow-shellctl version >/dev/null 2>&1 || fail "meadow-shell did not start"
first=$(own_shell)
[ -n "$first" ] || fail "no shell process"
echo "== kill -SEGV $first"
kill -SEGV "$first"
new_shell() { local s; s=$(own_shell); [ -n "$s" ] && [ "$s" != "$first" ]; }
wait_for 15 new_shell || fail "no new shell process"
wait_for 15 meadow-shellctl version >/dev/null 2>&1 || fail "restarted shell does not answer"
second=$(own_shell)
wait_for 5 grep -q "shell restarted; no logon sound" "$tmp/shell.log" || fail "restarted shell played the logon sound"
echo "ok: shell $first crashed, $second took over"
