#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Render Meadow components offscreen and compare them with tests/golden/.

  screenshot-test.py --build-dir build            compare (CI, ctest label "screenshot")
  screenshot-test.py --build-dir build --update   rewrite the golden images

Renders with the Qt Quick software backend on the offscreen platform, so the
result does not depend on a GPU. Text depends on the installed fonts: goldens
are produced with "Noto Sans" installed and Tahoma/Trebuchet MS absent (as on a
stock Fedora with google-noto-sans-fonts). The test is skipped (exit 77) when
that is not the case, rather than failing on font differences.

Failures leave actual and diff images in <build-dir>/screenshots/.
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

SKIP = 77
ROOT = Path(__file__).resolve().parent.parent
GOLDEN = ROOT / "tests" / "golden"

# (path, relative to) — built programs live in the build dir, scripts in the source tree.
GALLERY = ("tools/gallery/meadow-gallery", "build")
GREETER = ("tools/greeter-preview/meadow-greeter-preview", "build")
SESSION = ("tools/session-screenshot.sh", "source")
BUTTONS = ("tools/gen-labwc-theme.py", "source")
SHELL = ("tools/shell-preview.sh", "source")
SHELL_SESSION = ("tools/shell-session-screenshot.sh", "source")
TOOLKIT = ("tools/toolkit-screenshot.sh", "source")
SHEET = ("tools/theme-sheet.sh", "source")


def theme(scheme):
    return ["--theme", str(ROOT / "theme" / scheme / "theme.json")]

# (name, tool, arguments)
CASES = [
    ("gallery-luna-blue-1.0", GALLERY, ["--scheme", "luna-blue", "--scale", "1"]),
    ("gallery-luna-blue-1.25", GALLERY, ["--scheme", "luna-blue", "--scale", "1.25"]),
    ("gallery-luna-blue-1.5", GALLERY, ["--scheme", "luna-blue", "--scale", "1.5"]),
    ("gallery-luna-blue-2.0", GALLERY, ["--scheme", "luna-blue", "--scale", "2"]),
    ("gallery-luna-olive-1.0", GALLERY, ["--scheme", "luna-olive", "--scale", "1"]),
    ("gallery-luna-silver-1.0", GALLERY, ["--scheme", "luna-silver", "--scale", "1"]),
    ("greeter-1024x768", GREETER, ["--size", "1024x768"]),
    ("greeter-1920x1080-selected", GREETER, ["--size", "1920x1080", "--select", "0"]),
    ("greeter-2x-selected", GREETER, ["--size", "2048x1536", "--config", "scale=2", "--select", "1"]),
    ("greeter-wrong-password", GREETER, ["--select", "0", "--password", "wrong"]),
    ("greeter-capslock", GREETER, ["--select", "0", "--caps"]),
    ("greeter-turnoff", GREETER, ["--turnoff"]),
    ("greeter-high-contrast", GREETER, ["--config", "highContrast=true", "--select", "0"]),
    ("greeter-rtl", GREETER, ["--rtl", "--select", "0"]),
    ("greeter-silver", GREETER, ["--config", "scheme=luna-silver", "--select", "0"]),
    ("greeter-secondary", GREETER, ["--secondary", "--size", "800x600"]),
    ("greeter-fingerprint-reading", GREETER, ["--fingerprint", "enrolled:alice", "--fp-result", "pending", "--select", "0"]),
    ("session-luna-blue", SESSION, ["--scheme", "luna-blue"]),
    ("session-luna-olive", SESSION, ["--scheme", "luna-olive"]),
    ("session-luna-silver", SESSION, ["--scheme", "luna-silver"]),
    ("session-maximized", SESSION, ["--maximized"]),
    ("labwc-buttons-luna-blue", BUTTONS, ["--scheme", "luna-blue"]),
    ("labwc-buttons-luna-olive", BUTTONS, ["--scheme", "luna-olive"]),
    ("labwc-buttons-luna-silver", BUTTONS, ["--scheme", "luna-silver"]),
    # Taskbar (meadow-shell --preview, sample windows, fixed clock, drawn icons).
    ("shell-taskbar", SHELL, ["--", "--size", "1024x120", "--preview-windows", "5"]),
    ("shell-taskbar-olive", SHELL, ["--", "--size", "1024x120"] + theme("luna-olive")),
    ("shell-taskbar-silver", SHELL, ["--", "--size", "1024x120"] + theme("luna-silver")),
    ("shell-grouped", SHELL, ["--", "--size", "800x200", "--preview-windows", "10", "--open", "group"]),
    ("shell-tray", SHELL, ["--mocks", "--sni", "--", "--size", "640x260", "--delay", "2500", "--open", "tray-menu"]),
    ("shell-volume", SHELL, ["--mocks", "--", "--size", "640x260", "--delay", "1200", "--open", "volume"]),
    ("shell-network-wireless", SHELL, ["--mocks", "--network", "wireless", "--battery", "8", "--", "--size", "640x260", "--delay", "1200", "--open", "network"]),
    ("shell-window-menu", SHELL, ["--", "--size", "640x260", "--preview-windows", "3", "--open", "window-menu"]),
    ("shell-taskbar-menu", SHELL, ["--", "--size", "640x260", "--preview-windows", "3", "--open", "taskbar-menu"]),
    ("shell-calendar", SHELL, ["--", "--size", "640x300", "--open", "calendar"]),
    ("shell-balloon", SHELL, ["--", "--size", "640x260", "--delay", "1200", "--open", "balloon"]),
    ("shell-properties", SHELL, ["--", "--size", "640x480", "--open", "properties"]),
    # Start menu and system dialogs (phase 7).
    ("shell-start-menu", SHELL, ["--", "--size", "800x600", "--open", "start"]),
    ("shell-start-menu-olive", SHELL, ["--", "--size", "800x600", "--open", "start"] + theme("luna-olive")),
    ("shell-start-menu-silver", SHELL, ["--", "--size", "800x600", "--open", "start"] + theme("luna-silver")),
    ("shell-all-programs", SHELL, ["--", "--size", "800x600", "--open", "all-programs"]),
    ("shell-recent-documents", SHELL, ["--recent", "--", "--size", "800x600", "--open", "recent"]),
    ("shell-run", SHELL, ["--", "--size", "640x400", "--open", "run"]),
    ("shell-log-off", SHELL, ["--", "--size", "800x600", "--delay", "1200", "--open", "logoff"]),
    ("shell-turn-off", SHELL, ["--", "--size", "800x600", "--delay", "1200", "--open", "turnoff"]),
    # Desktop, lock screen, Alt+Tab, Display Properties, Control Panel (phase 8).
    ("shell-desktop", SHELL, ["--", "--size", "1024x768", "--preview-windows", "0"]),
    ("shell-desktop-olive", SHELL, ["--", "--size", "1024x768", "--preview-windows", "0"] + theme("luna-olive")),
    ("shell-desktop-silver", SHELL, ["--", "--size", "1024x768", "--preview-windows", "0"] + theme("luna-silver")),
    ("shell-desktop-menu", SHELL, ["--", "--size", "800x600", "--preview-windows", "0", "--open", "desktop-menu"]),
    ("shell-desktop-item-menu", SHELL, ["--", "--size", "800x600", "--preview-windows", "0", "--open", "item-menu"]),
    ("shell-lock", SHELL, ["--", "--size", "1024x768", "--open", "lock"]),
    ("shell-lock-silver", SHELL, ["--", "--size", "1024x768", "--open", "lock"] + theme("luna-silver")),
    ("shell-alttab", SHELL, ["--", "--size", "800x600", "--preview-windows", "8", "--open", "alttab"]),
    ("shell-display-themes", SHELL, ["--", "--size", "800x600", "--open", "display:themes"]),
    ("shell-display-desktop", SHELL, ["--", "--size", "800x600", "--open", "display:desktop"]),
    ("shell-display-screensaver", SHELL, ["--", "--size", "800x600", "--open", "display:screensaver"]),
    ("shell-display-appearance", SHELL, ["--", "--size", "800x600", "--open", "display:appearance"]),
    ("shell-display-settings", SHELL, ["--", "--size", "800x600", "--open", "display:settings"]),
    ("shell-control-panel", SHELL, ["--", "--size", "1024x768", "--open", "control-panel"]),
    ("shell-control-panel-category", SHELL, ["--", "--size", "1024x768", "--open", "control-panel:appearance"]),
    ("shell-control-panel-olive", SHELL, ["--", "--size", "1024x768", "--open", "control-panel"] + theme("luna-olive")),
    ("shell-applet-mouse", SHELL, ["--", "--size", "800x600", "--open", "applet:mouse"]),
    ("shell-applet-keyboard", SHELL, ["--", "--size", "800x600", "--open", "applet:keyboard"]),
    ("shell-applet-datetime", SHELL, ["--", "--size", "800x600", "--open", "applet:datetime"]),
    ("shell-applet-users", SHELL, ["--", "--size", "800x600", "--open", "applet:users:picture"]),
    ("shell-applet-sounds", SHELL, ["--", "--size", "800x600", "--open", "applet:sounds"]),
    ("shell-authenticate", SHELL, ["--", "--size", "800x600", "--open", "authenticate"]),
    ("shell-screensaver", SHELL, ["--", "--size", "800x600", "--open", "screensaver"]),
    # The real shell on layer-shell surfaces in headless labwc, 10 windows.
    ("shell-session-10-windows", SHELL_SESSION, ["--windows", "10"]),
    ("shell-session-calendar", SHELL_SESSION, ["--windows", "3", "--open", "calendar"]),
    ("shell-session-start-menu", SHELL_SESSION, ["--windows", "2", "--open", "start"]),
    ("shell-session-desktop", SHELL_SESSION, ["--windows", "0"]),
    ("shell-session-desktop-olive", SHELL_SESSION, ["--windows", "0", "--scheme", "luna-olive"]),
    # Third-party applications with the generated toolkit themes (phase 9).
    ("toolkit-gtk3", TOOLKIT, ["--app", "gtk3"]),
    ("toolkit-gtk3-menu-olive", TOOLKIT, ["--app", "gtk3", "--menu", "--scheme", "luna-olive"]),
    ("toolkit-gtk4", TOOLKIT, ["--app", "gtk4"]),
    ("toolkit-gtk4-silver", TOOLKIT, ["--app", "gtk4", "--scheme", "luna-silver"]),
    ("toolkit-qt", TOOLKIT, ["--app", "qt"]),
    ("toolkit-qt-silver", TOOLKIT, ["--app", "qt", "--scheme", "luna-silver"]),
    ("icons-48", SHEET, ["--kind", "icons-48"]),
    ("icons-16", SHEET, ["--kind", "icons-16"]),
    ("cursors", SHEET, ["--kind", "cursors"]),
    ("greeter-fingerprint-failed", GREETER, ["--fingerprint", "enrolled:alice", "--fp-result", "fail", "--select", "0", "--delay", "1500"]),
]

QML_PROBLEMS = ("ReferenceError", "TypeError", "is not a type", "is not installed", "Binding loop", "Unable to assign", "polish() loop")


def fonts_ok():
    try:
        families = subprocess.run(["fc-list", ":", "family"], capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return False, "fc-list not available"
    names = {n.strip().lower() for line in families.splitlines() for n in line.split(",")}
    if "noto sans" not in names:
        return False, "font 'Noto Sans' is not installed (dnf install google-noto-sans-fonts)"
    for proprietary in ("tahoma", "trebuchet ms"):
        if proprietary in names:
            return False, f"'{proprietary}' is installed; goldens assume the open fallback fonts"
    return True, ""


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--update", action="store_true", help="rewrite golden images")
    ap.add_argument("--only", help="run only cases whose name contains this text")
    args = ap.parse_args()

    build = Path(args.build_dir).resolve()
    imgdiff = build / "tools" / "gallery" / "meadow-imgdiff"
    for tool in (build / GALLERY[0], build / GREETER[0], imgdiff):
        if not tool.exists():
            print(f"missing {tool}; build with -DMEADOW_BUILD_GALLERY=ON", file=sys.stderr)
            return 1

    ok, why = fonts_ok()
    if not ok:
        print(f"SKIP: {why}")
        return SKIP

    out_dir = build / "screenshots"
    out_dir.mkdir(exist_ok=True)
    env = dict(os.environ,
               QT_QPA_PLATFORM="offscreen",
               QT_QUICK_BACKEND="software",
               QT_FORCE_STDERR_LOGGING="1",
               QT_ENABLE_HIGHDPI_SCALING="0",
               MEADOW_THEME_DIR=str(ROOT / "theme"),
               MEADOW_ASSET_DIR=str(ROOT / "theme" / "assets"),
               MEADOW_MUTE="1",
               MEADOW_REDUCED_MOTION="1",
               QML_IMPORT_PATH=str(build / "qml"),
               LANG="C.UTF-8", LC_ALL="C.UTF-8")
    env.pop("MEADOW_SCALE", None)
    env.pop("MEADOW_THEME", None)

    failures = []
    missing = []
    for name, tool, tool_args in CASES:
        if args.only and args.only not in name:
            continue
        actual = out_dir / f"{name}.png"
        exe = (build if tool[1] == "build" else ROOT) / tool[0]
        proc = subprocess.run([str(exe), *tool_args, "--grab", str(actual)],
                              env=dict(env, MEADOW_BUILD_DIR=str(build)), capture_output=True, text=True, timeout=120)
        if proc.returncode == SKIP:
            print(f"SKIP {name}: {proc.stdout.strip()}")
            continue
        problems = [l for l in proc.stderr.splitlines() if any(p in l for p in QML_PROBLEMS)]
        if proc.returncode != 0 or problems or not actual.exists():
            failures.append(f"{name}: render failed (exit {proc.returncode})\n  " + "\n  ".join(problems or proc.stderr.splitlines()[-5:]))
            continue
        golden = GOLDEN / f"{name}.png"
        if args.update:
            shutil.copyfile(actual, golden)
            print(f"updated {golden.relative_to(ROOT)}")
            continue
        if not golden.exists():
            missing.append(name)
            continue
        diff = out_dir / f"{name}.diff.png"
        cmp = subprocess.run([str(imgdiff), str(golden), str(actual), "--diff", str(diff)], capture_output=True, text=True)
        status = "ok" if cmp.returncode == 0 else "FAIL"
        print(f"{status:4} {name}: {cmp.stdout.strip()}")
        if cmp.returncode != 0:
            failures.append(f"{name}: {cmp.stdout.strip()} (see {diff})")

    for m in missing:
        print(f"MISSING golden for {m}; run with --update and review the image")
    for f in failures:
        print("FAIL", f, file=sys.stderr)
    if failures:
        return 1
    if missing:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
