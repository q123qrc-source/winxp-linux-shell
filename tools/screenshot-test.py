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

GALLERY = ("tools/gallery/meadow-gallery",)
GREETER = ("tools/greeter-preview/meadow-greeter-preview",)

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
        proc = subprocess.run([str(build / tool[0]), *tool_args, "--grab", str(actual)],
                              env=env, capture_output=True, text=True, timeout=120)
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
