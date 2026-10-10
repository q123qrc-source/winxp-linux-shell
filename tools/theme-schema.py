#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate or check theme/schema.json and validate every theme scheme.

theme/luna-blue/theme.json is the reference scheme. The schema records the
key path and value type of every token in it. Every other scheme must have
exactly the same keys and types, so adding a scheme is "copy JSON, recolor".

  theme-schema.py generate <theme-dir>   rewrite <theme-dir>/schema.json
  theme-schema.py check    <theme-dir>   validate all <theme-dir>/*/theme.json
"""
import json
import re
import sys
from pathlib import Path

HEX = re.compile(r"^#(?:[0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})$")
REFERENCE = "luna-blue"


def type_name(v):
    if isinstance(v, bool):
        return "bool"
    if isinstance(v, (int, float)):
        return "number"
    if isinstance(v, str):
        return "color" if HEX.match(v) else "string"
    if isinstance(v, list):
        if v and all(isinstance(x, dict) and set(x) == {"pos", "color"} for x in v):
            return "gradient"
        return "list"
    if isinstance(v, dict):
        return "object"
    return "null"


def flatten(obj, prefix=""):
    out = {}
    for k, v in obj.items():
        path = f"{prefix}.{k}" if prefix else k
        t = type_name(v)
        out[path] = t
        if t == "object":
            out.update(flatten(v, path))
    return out


def check_values(obj, path, errors):
    t = type_name(obj)
    if t == "object":
        for k, v in obj.items():
            check_values(v, f"{path}.{k}" if path else k, errors)
    elif t == "gradient":
        last = -1.0
        for stop in obj:
            if not HEX.match(stop["color"]):
                errors.append(f"{path}: bad gradient color {stop['color']!r}")
            if not (0.0 <= stop["pos"] <= 1.0) or stop["pos"] < last:
                errors.append(f"{path}: gradient stops must be ascending within [0,1]")
            last = stop["pos"]
    elif t == "number" and obj < 0:
        errors.append(f"{path}: negative metric {obj}")


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("generate", "check"):
        print(__doc__)
        return 2
    theme_dir = Path(sys.argv[2])
    ref = json.loads((theme_dir / REFERENCE / "theme.json").read_text())
    schema = flatten(ref)
    schema.pop("meta.description", None)
    schema_file = theme_dir / "schema.json"

    if sys.argv[1] == "generate":
        schema_file.write_text(json.dumps({"reference": REFERENCE, "tokens": schema}, indent=2, sort_keys=True) + "\n")
        print(f"wrote {schema_file} ({len(schema)} tokens)")
        return 0

    errors = []
    stored = json.loads(schema_file.read_text())["tokens"]
    if stored != schema:
        errors.append(f"{schema_file} is out of date with {REFERENCE}; run: tools/theme-schema.py generate theme")
    schemes = sorted(p for p in theme_dir.glob("*/theme.json"))
    for path in schemes:
        data = json.loads(path.read_text())
        flat = flatten(data)
        flat.pop("meta.description", None)
        for k in sorted(set(stored) - set(flat)):
            errors.append(f"{path}: missing token {k}")
        for k in sorted(set(flat) - set(stored)):
            errors.append(f"{path}: unknown token {k}")
        for k in sorted(set(flat) & set(stored)):
            if flat[k] != stored[k]:
                errors.append(f"{path}: {k} is {flat[k]}, expected {stored[k]}")
        if data.get("meta", {}).get("id") != path.parent.name:
            errors.append(f"{path}: meta.id must equal directory name {path.parent.name!r}")
        check_values(data, "", errors)
    for e in errors:
        print("ERROR:", e)
    print(f"checked {len(schemes)} schemes against {len(stored)} tokens: {'FAIL' if errors else 'OK'}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
