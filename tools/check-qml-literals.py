#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Keep visual metrics in theme.json: flag hard-coded colors and numbers in
product QML (common/, shell/, greeter/).

Errors:
  * a color literal ("#RRGGBB", "#AARRGGBB", "#RGB") or Qt.rgba()/Qt.hsla()/
    Qt.lighter()/Qt.darker() call
  * a numeric literal other than 0, 1, 2 or 0.5

A line can opt out with a trailing comment containing "theme-ok: <reason>".
Comments, string contents and import/version lines are ignored.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIRS = ["common/qml", "shell/qml", "greeter"]
ALLOWED_NUMBERS = {"0", "1", "2", "0.5", "0.0", "1.0"}

COLOR_STR = re.compile(r"""["']#(?:[0-9A-Fa-f]{3}|[0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})["']""")
COLOR_CALL = re.compile(r"\bQt\.(rgba|hsla|hsva|lighter|darker|tint)\s*\(")
STRING = re.compile(r'"(?:[^"\\]|\\.)*"|\'(?:[^\'\\]|\\.)*\'')
NUMBER = re.compile(r"(?<![\w.#])(\d+\.\d+|\d+\.|\.\d+|\d+)(?![\w.])")


def strip_block_comments(text):
    return re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)


def check(path):
    errors = []
    lines = strip_block_comments(path.read_text(encoding="utf-8")).splitlines()
    for no, raw in enumerate(lines, 1):
        if "theme-ok" in raw:
            continue
        code = raw.split("//", 1)[0] if "//" in raw and not re.search(r"[\"'][^\"']*//", raw) else raw
        stripped = code.strip()
        if stripped.startswith(("import ", "pragma ")):
            continue
        if COLOR_STR.search(code):
            errors.append((no, "color literal; use a theme token"))
        if COLOR_CALL.search(code):
            errors.append((no, "computed color; add a theme token instead"))
        for m in NUMBER.finditer(STRING.sub('""', code)):
            if m.group(1) not in ALLOWED_NUMBERS:
                errors.append((no, f"numeric literal {m.group(1)}; use a theme token"))
    return errors


def main():
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT
    total = 0
    files = 0
    for d in DIRS:
        for path in sorted((root / d).rglob("*.qml")):
            files += 1
            for no, msg in check(path):
                print(f"{path.relative_to(root)}:{no}: {msg}")
                total += 1
    print(f"checked {files} QML files: {'OK' if total == 0 else f'{total} problem(s)'}")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
