#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# End-to-end fingerprint check on a DISPOSABLE Fedora VM or container, using
# libfprint's virtual reader (FP_VIRTUAL_DEVICE) instead of hardware:
#
#   1. snapshot /etc/pam.d and /etc/authselect
#   2. meadow-setup enable-fingerprint
#   3. run fprintd on the virtual reader and enroll a finger for a test user
#   4. pamtester against the real sddm and meadow-lock services:
#      matching finger, three misses then password, misses then wrong
#      password, no enrolled finger, untouched reader (timeout)
#   5. meadow-setup disable-fingerprint and compare with the snapshot
#
# It creates a user, changes PAM and restarts fprintd, so it refuses to run
# unless MEADOW_DISPOSABLE_SYSTEM=1. Needs: fprintd, fprintd-pam, pamtester,
# dbus-daemon, polkit, python3, authselect (Fedora).
#   sudo MEADOW_DISPOSABLE_SYSTEM=1 tools/vm/fingerprint-e2e.sh [path/to/meadow-setup]
set -uo pipefail

[ "${MEADOW_DISPOSABLE_SYSTEM:-}" = 1 ] || { echo "refusing: set MEADOW_DISPOSABLE_SYSTEM=1 on a throwaway VM/container" >&2; exit 2; }
[ "$(id -u)" = 0 ] || { echo "run as root" >&2; exit 2; }
setup="${1:-$(cd "$(dirname "$0")/../.." && pwd)/setup/meadow-setup}"
work="$(mktemp -d)"
sock="$work/fpv"
user=meadowtest
pass='Meadow-T3st!'
other=meadownofinger
fails=0

send() { python3 - "$sock" "$@" <<'PY'
import socket, sys, time
path = sys.argv[1]
for cmd in sys.argv[2:]:
    for _ in range(100):
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); s.connect(path); break
        except OSError:
            time.sleep(0.1)
    s.sendall(cmd.encode()); s.close(); time.sleep(0.05)
PY
}

check() { # name expected-exit service user password-stdin [scans...]
    local name=$1 expect=$2 svc=$3 who=$4 pw=$5; shift 5
    # Scans go out after pam_fprintd has claimed the device; earlier commands can be lost.
    local sender=""
    if [ $# -gt 0 ]; then (sleep 1.5; send "$@") & sender=$!; fi
    local start=$SECONDS
    printf '%s' "$pw" | timeout 60 pamtester "$svc" "$who" authenticate >"$work/pt.log" 2>&1
    local rc=$?
    # Wait for the sender only: fprintd and polkitd are background jobs too.
    [ -n "$sender" ] && wait "$sender"
    printf '%-58s exit=%s expected=%s (%ss)\n' "$name" "$rc" "$expect" "$((SECONDS - start))"
    if [ "$rc" != "$expect" ]; then fails=$((fails + 1)); sed 's/^/    /' "$work/pt.log"; fi
}

stop_proc() { for p in $(ps -eo pid=,comm= | awk -v n="$1" '$2==n{print $1}'); do kill "$p" 2>/dev/null; done; }

echo "== snapshot"
cp -a /etc/pam.d "$work/pam.d.before"
[ -d /etc/authselect ] && cp -a /etc/authselect "$work/authselect.before"

for u in "$user" "$other"; do id "$u" >/dev/null 2>&1 || useradd -m "$u"; echo "$u:$pass" | chpasswd; done

echo "== enable"
"$setup" --yes enable-fingerprint >"$work/enable.log" 2>&1 || { cat "$work/enable.log"; exit 1; }

echo "== fprintd on the virtual reader"
if command -v systemctl >/dev/null && systemctl is-active --quiet dbus 2>/dev/null; then
    systemctl stop fprintd 2>/dev/null
else
    stop_proc dbus-daemon; mkdir -p /run/dbus; rm -f /run/dbus/pid
    dbus-daemon --system --fork
    stop_proc polkitd; /usr/lib/polkit-1/polkitd --no-debug >"$work/polkitd.log" 2>&1 &
    polkitd_pid=$!
    sleep 0.5
fi
stop_proc fprintd
FP_VIRTUAL_DEVICE="$sock" /usr/libexec/fprintd -t >"$work/fprintd.log" 2>&1 &
fprintd_pid=$!
sleep 1
fprintd-delete "$user" >/dev/null 2>&1
(sleep 1; send "SCAN finger-A" "SCAN finger-A" "SCAN finger-A" "SCAN finger-A" "SCAN finger-A") &
enroll_sender=$!
timeout 30 fprintd-enroll -f right-index-finger "$user" | tail -1
wait "$enroll_sender"

echo "== scenarios"
check "matching finger, no password typed (sddm)"          0 sddm        "$user"  ""            "SCAN finger-A"
check "three misses, then the right password (sddm)"       0 sddm        "$user"  "$pass"$'\n' "SCAN finger-B" "SCAN finger-B" "SCAN finger-B"
check "three misses, then a wrong password (sddm)"         1 sddm        "$user"  $'nope\n'    "SCAN finger-B" "SCAN finger-B" "SCAN finger-B"
check "matching finger (meadow-lock)"                      0 meadow-lock "$user"  ""            "SCAN finger-A"
check "no enrolled finger, password (sddm)"                0 sddm        "$other" "$pass"$'\n'
check "untouched reader times out, then password (sddm)"   0 sddm        "$user"  "$pass"$'\n'

echo "== disable and compare"
kill "$fprintd_pid" ${polkitd_pid:-} 2>/dev/null
"$setup" --yes disable-fingerprint >"$work/disable.log" 2>&1 || { cat "$work/disable.log"; fails=$((fails + 1)); }
diff -r "$work/pam.d.before" /etc/pam.d && echo "/etc/pam.d identical to the snapshot" || fails=$((fails + 1))
if [ -d "$work/authselect.before" ]; then
    diff -r "$work/authselect.before" /etc/authselect && echo "/etc/authselect identical to the snapshot" || fails=$((fails + 1))
fi
check "password still works after disable (sddm)"          0 sddm        "$user"  "$pass"$'\n'
command -v systemctl >/dev/null && systemctl start fprintd 2>/dev/null || true

echo "== $([ "$fails" = 0 ] && echo PASS || echo "FAIL ($fails)")"
rm -rf "$work"
[ "$fails" = 0 ]
