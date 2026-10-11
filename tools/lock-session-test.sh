#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Lock screen end to end in a real compositor: headless labwc,
# meadow-shell, ext-session-lock-v1, and meadow-auth-helper running PAM.
# PAM is pam_wrapper + pam_matrix with a private service directory, so no
# system PAM file is read or changed and no real password is involved.
#   1. meadow-shellctl lock -> the session locks (IsLocked), lock screen shown
#   2. a wrong password keeps it locked
#   3. the right password unlocks it
#   4. the shell is killed while locked -> the compositor keeps the session
#      locked; the restarted shell locks again (flag file) and unlocks
#   5. logind's Lock signal (`loginctl lock-session`; a mock logind on the
#      session bus) locks, and its Unlock (the console recovery path) unlocks
#   6. idle (a short MEADOW_IDLE_TIMEOUT_MS) starts the screen saver; a key
#      ends it and, password-protected, the session is locked
# Screenshots go to OUT_DIR.
#
#   lock-session-test.sh BUILD_DIR [OUT_DIR]
# Skips (77) without labwc, wtype, grim or pam_wrapper.
set -uo pipefail
src="$(cd "$(dirname "$0")/.." && pwd)"
build="$(cd "$1" && pwd)"
out="$(realpath -m "${2:-$build/lock-session}")"
for tool in labwc wtype grim; do
    command -v "$tool" >/dev/null || { echo "SKIP: $tool not installed"; exit 77; }
done
pam_matrix=""
for d in /usr/lib64/pam_wrapper /usr/lib/pam_wrapper /usr/lib/x86_64-linux-gnu/pam_wrapper; do
    [ -f "$d/pam_matrix.so" ] && pam_matrix="$d/pam_matrix.so"
done
[ -n "$pam_matrix" ] || { echo "SKIP: pam_wrapper (pam_matrix.so) not installed"; exit 77; }

if [ -z "${MEADOW_IN_LABWC:-}" ]; then
    export MEADOW_IN_LABWC=1
    exec dbus-run-session -- "$src/tools/with-headless-labwc.sh" --size 1024x768 -- "$0" "$build" "$out"
fi

mkdir -p "$out"
tmp=$(mktemp -d)
pids=()
shell_pid=""
cleanup() { kill "${pids[@]}" $shell_pid 2>/dev/null; rm -rf "$tmp"; }
trap cleanup EXIT
fail() { echo "FAIL: $*"; grep -hv "^$" "$tmp"/shell*.log 2>/dev/null | tail -40; exit 1; }
wait_for() { # seconds, command...
    local t=$1; shift
    for _ in $(seq $((t * 10))); do "$@" && return 0; sleep 0.1; done
    return 1
}
locked() { [ "$(meadow-shellctl is-locked 2>/dev/null)" = yes ]; }
unlocked() { [ "$(meadow-shellctl is-locked 2>/dev/null)" = no ]; }

# PAM: a private "meadow-lock-test" service checking a pam_matrix password
# file. The helper alone is preloaded with pam_wrapper.
user=$(id -un)
password="correct horse"
mkdir -p "$tmp/pam"
printf '%s:%s:meadow-lock-test\n' "$user" "$password" > "$tmp/passdb"
cat > "$tmp/pam/meadow-lock-test" <<PAM
auth     required $pam_matrix passdb=$tmp/passdb
account  required $pam_matrix passdb=$tmp/passdb
PAM
cat > "$tmp/auth-helper" <<HELPER
#!/bin/sh
LD_PRELOAD=libpam_wrapper.so PAM_WRAPPER=1 PAM_WRAPPER_SERVICE_DIR="$tmp/pam" \\
MEADOW_PAM_SERVICE=meadow-lock-test exec "$build/shell/meadow-auth-helper" "\$@"
HELPER
chmod +x "$tmp/auth-helper"

export LC_ALL=C.UTF-8 QT_FORCE_STDERR_LOGGING=1 QT_QUICK_BACKEND=software
export MEADOW_THEME_DIR="$src/theme" MEADOW_ASSET_DIR="$src/theme/assets" QML_IMPORT_PATH="$build/qml"
export XDG_DATA_DIRS="$src/tests/fixtures" XDG_DATA_HOME="$tmp/data" XDG_CONFIG_HOME="$tmp/config"
export MEADOW_SHELL_DEFAULTS="$src/shell/data/shell.conf" MEADOW_MENU_DIR="$src/session/menus" MEADOW_WPCTL=/bin/false
export MEADOW_AUTH_HELPER="$tmp/auth-helper" PATH="$src/shell:$PATH"
export MEADOW_SYSTEM_SERVICES_ON_SESSION_BUS=1 XDG_SESSION_ID=31

have_mock=0
if python3 -c "import dbus, gi.repository.GLib" 2>/dev/null; then
    python3 "$src/tools/mock-system-services.py" --no-battery --network none --logind --log "$tmp/logind.log" > "$tmp/mock.out" &
    pids+=($!)
    wait_for 5 grep -q ready "$tmp/mock.out" && have_mock=1
fi

start_shell() { # log name
    "$build/shell/meadow-shell" > "$tmp/$1" 2>&1 &
    shell_pid=$!
    wait_for 10 meadow-shellctl version >/dev/null 2>&1 || fail "meadow-shell did not start"
}
type_password() {
    wtype -- "$1"; sleep 0.2
    wtype -k Return
}

start_shell shell.log
"$build/tools/decor-test/meadow-decor-test" --title "Private work" --app-id org.meadow.Private > /dev/null 2>&1 &
pids+=($!)
sleep 1.5
wtype -k Shift_L   # the first key of a new virtual keyboard can be lost
sleep 0.3

echo "== 1. lock"
meadow-shellctl lock
wait_for 5 locked || fail "the session did not lock"
[ -f "$XDG_RUNTIME_DIR/meadow/locked" ] || fail "no lock flag file"
sleep 1
grim "$out/locked.png"
echo "ok: locked"

echo "== 2. wrong password"
type_password "not the password"
sleep 3   # pam_matrix answers at once; the lock screen shakes and shows a balloon
grim "$out/wrong-password.png"
locked || fail "a wrong password unlocked the session"
grep -q "authentication failed" "$tmp/shell.log" || fail "the failed attempt was not reported"
echo "ok: still locked"

echo "== 3. right password"
type_password "$password"
wait_for 5 unlocked || fail "the right password did not unlock"
[ -f "$XDG_RUNTIME_DIR/meadow/locked" ] && fail "lock flag left behind"
sleep 0.5
grim "$out/unlocked.png"
echo "ok: unlocked"

echo "== 4. shell killed while locked"
meadow-shellctl lock
wait_for 5 locked || fail "the session did not lock again"
sleep 0.5
kill -9 "$shell_pid"; wait "$shell_pid" 2>/dev/null
sleep 0.5
grim "$out/crashed-while-locked.png"
start_shell shell2.log
wait_for 5 locked || fail "the restarted shell did not lock again"
grep -q "left the session locked" "$tmp/shell2.log" || fail "relock not logged"
sleep 1
wtype -k Shift_L; sleep 0.3
type_password "$password"
wait_for 5 unlocked || fail "could not unlock after the restart"
echo "ok: relocked after a crash and unlocked"

if [ $have_mock = 1 ]; then
    echo "== 5. logind Lock signal"
    busctl --user call org.freedesktop.login1 /org/freedesktop/login1 org.meadow.Test Emit s Lock >/dev/null
    wait_for 5 locked || fail "logind's Lock did not lock the session"
    grep -q "SetLockedHint True" "$tmp/logind.log" || fail "the locked hint was not set"
    sleep 1
    wtype -k Shift_L; sleep 0.3
    type_password "$password"
    wait_for 5 unlocked || fail "could not unlock after logind's Lock"
    echo "ok: loginctl lock-session locks"
    # The console recovery path: loginctl unlock-session.
    meadow-shellctl lock
    wait_for 5 locked || fail "the session did not lock for the Unlock check"
    busctl --user call org.freedesktop.login1 /org/freedesktop/login1 org.meadow.Test Emit s Unlock >/dev/null
    wait_for 5 unlocked || fail "logind's Unlock did not unlock"
    echo "ok: loginctl unlock-session unlocks"
else
    echo "-- 5. skipped (python3-dbus missing)"
fi

echo "== 6. screen saver on idle, password protected"
kill "$shell_pid"; wait "$shell_pid" 2>/dev/null
MEADOW_IDLE_TIMEOUT_MS=1500 start_shell shell3.log
wait_for 10 grep -q "screen saver started" "$tmp/shell3.log" || fail "the screen saver did not start when idle"
sleep 1   # past the grace period
grim "$out/screen-saver.png"
unlocked || fail "locked before the screen saver ended"
wtype -k space
wait_for 5 locked || fail "ending a password-protected screen saver did not lock"
sleep 1
wtype -k Shift_L; sleep 0.3
type_password "$password"
wait_for 5 unlocked || fail "could not unlock after the screen saver"
echo "ok: idle -> screen saver -> lock"

# Secrets never reach the logs.
if grep -hF -e "$password" -e "not the password" "$tmp"/shell*.log; then
    fail "a password appeared in the shell log"
fi
if grep -hE "qrc:.*(Error|error)|Binding loop|ReferenceError|TypeError" "$tmp"/shell*.log; then
    fail "QML problems in the shell log"
fi
echo "PASS (screenshots in $out)"
