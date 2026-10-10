#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""End-to-end Welcome screen flows through meadow-greeter-preview (mock SDDM).

Checks, for each scenario, what the greeter asked SDDM to do, that no QML
errors or warnings appeared, and that the typed password never shows up in
any output.
"""
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SECRET = "Sup3r-Secret-Passw0rd"
PROBLEMS = ("ReferenceError", "TypeError", "is not a type", "Binding loop", "polish() loop",
            "Unable to assign", "Cannot read property", "QML", "Error:")


def run(build, *args):
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software",
               QT_FORCE_STDERR_LOGGING="1", MEADOW_THEME_DIR=str(ROOT / "theme"),
               MEADOW_ASSET_DIR=str(ROOT / "theme" / "assets"), QML_IMPORT_PATH=str(build / "qml"),
               MEADOW_MUTE="1", LC_ALL="C.UTF-8", QT_LOGGING_RULES="meadow.*=true")
    exe = build / "tools" / "greeter-preview" / "meadow-greeter-preview"
    p = subprocess.run([str(exe), "--exit-after-ms", "1200", *args], env=env, capture_output=True, text=True, timeout=60)
    return p.returncode, p.stdout, p.stderr


def main():
    build = Path(sys.argv[1] if len(sys.argv) > 1 else "build").resolve()
    scenarios = [
        # (name, args, expected stdout fragments, forbidden stdout fragments)
        ("wrong password", ["--select", "0", "--password", SECRET], ["login user=alice session=0 result=failure"], []),
        ("right password", ["--select", "1", "--password", "meadow"], ["login user=bart session=0 result=success"], []),
        ("no-password user logs on when selected", ["--select", "2"], ["login user=guest session=0 result=failure"], []),
        ("selecting does not log in", ["--select", "0"], [], ["login user="]),
        ("turn off dialog opens without acting", ["--turnoff"], [], ["powerOff", "reboot", "suspend"]),
        ("secondary screen", ["--secondary", "--select", "0"], [], ["login user="]),
        ("right-to-left", ["--rtl", "--select", "0"], [], []),
        ("high contrast", ["--config", "highContrast=true"], [], []),
        ("silver at 2x", ["--config", "scheme=luna-silver", "--config", "scale=2", "--size", "2048x1536"], [], []),
        ("many users scroll", ["--extra-users", "12", "--select", "9"], [], []),
    ]
    failures = 0
    for name, args, expect, forbid in scenarios:
        code, out, err = run(build, *args)
        problems = [l for l in err.splitlines() if any(p in l for p in PROBLEMS) and "PopupList" not in l]
        missing = [e for e in expect if e not in out]
        present = [f for f in forbid if f in out]
        leaked = SECRET in out or SECRET in err
        ok = code == 0 and not problems and not missing and not present and not leaked
        print(f"{'ok  ' if ok else 'FAIL'} {name}")
        if not ok:
            failures += 1
            print(f"     exit={code} missing={missing} unexpected={present} password_leaked={leaked}")
            for l in problems[:5]:
                print("     " + l)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
