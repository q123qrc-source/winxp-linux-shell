#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate the GTK 3, GTK 4 and Kvantum themes from theme.json (phase 9).

Every scheme in theme/*/theme.json becomes one GTK theme
(toolkit.gtkThemeName, e.g. Meadow-Luna) and one Kvantum theme
(toolkit.kvantumThemeName, e.g. MeadowLuna), so a new scheme needs only a
new JSON file. Run at build time; nothing it writes is committed.

  gen-toolkit-themes.py --out DIR [--theme-root theme] [--scheme ID]
      DIR/themes/<GtkName>/{index.theme,gtk-3.0/,gtk-4.0/}
      DIR/Kvantum/<KvName>/<KvName>.{kvconfig,svg}
  gen-toolkit-themes.py --lint      check the templates (no literal colors
                                    or pixel sizes; "theme-ok" exempts a line)
  gen-toolkit-themes.py --list      print "scheme gtk-name kvantum-name"
  gen-toolkit-themes.py --defaults DIR [--scheme luna-blue]
      the session-scoped defaults meadow-session installs for third-party
      applications (never written to ~ or /etc):
      DIR/dconf/meadow.d/00-meadow      dconf keyfile (compiled at build time)
      DIR/xdg/Kvantum/kvantum.kvconfig  Kvantum theme choice
      DIR/xdg/gtk-{3,4}.0/settings.ini  GTK fallback settings
      DIR/xdg/pcmanfm-qt/default/settings.conf

Templates (toolkit-themes/): gtk.css.in (shared by GTK 3 and 4, with
"@if gtk3" / "@if gtk4" / "@endif" blocks) and kvantum/MeadowLuna.kvconfig.in.
Placeholders are @{filter:token.path}:
  c:   color (#AARRGGBB tokens become rgba())
  px:  number + "px"           n:  plain number
  g:   vertical linear-gradient(...)   gh: horizontal
  ff:  font-family list        fs:  font pixel size + "px"
  kc:  Kvantum color (#RRGGBB or #RRGGBBAA)
  s:   string as is
The images GTK needs (check boxes, radio buttons, arrows, scrollbar grips)
and the whole Kvantum SVG are drawn here from the same tokens.
"""
import argparse
import importlib.util
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEMPLATES = ROOT / "toolkit-themes"

_spec = importlib.util.spec_from_file_location("labwc_theme", ROOT / "tools" / "gen-labwc-theme.py")
_labwc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_labwc)
parse_color, sample = _labwc.parse_color, _labwc.sample


# --- Token access ----------------------------------------------------------------

def token(t, path):
    v = t
    for part in path.split("."):
        if isinstance(v, list) and part.isdigit() and int(part) < len(v):
            v = v[int(part)]
        elif isinstance(v, dict) and part in v:
            v = v[part]
        else:
            raise KeyError(f"unknown token {path}")
    return v


def derive(t):
    """Values computed from tokens (never set by hand), under "derived"."""
    pb = t["controls"]["progressBar"]
    t = dict(t)
    t["derived"] = {"progressBlockPeriod": pb["blockWidth"] + pb["blockSpacing"]}
    return t


def css_color(c):
    r, g, b, a = parse_color(c) if isinstance(c, str) else c
    if a == 255:
        return f"#{r:02x}{g:02x}{b:02x}"
    return f"rgba({r}, {g}, {b}, {round(a / 255, 3)})"


def kv_color(c):
    r, g, b, a = parse_color(c) if isinstance(c, str) else c
    return f"#{r:02x}{g:02x}{b:02x}" + ("" if a == 255 else f"{a:02x}")


def css_gradient(stops, direction):
    parts = ", ".join(f"{css_color(s['color'])} {round(s['pos'] * 100, 2)}%" for s in stops)
    if len(stops) == 1:
        parts = f"{css_color(stops[0]['color'])}, {css_color(stops[0]['color'])}"
    return f"linear-gradient({direction}, {parts})"


def font_families(fams):
    return ", ".join(f'"{f}"' if " " in f else f for f in fams)


def num(v):
    return str(int(v)) if float(v).is_integer() else str(v)


FILTERS = {
    "c": lambda v: css_color(v),
    "kc": lambda v: kv_color(v),
    "px": lambda v: f"{num(v)}px",
    "n": lambda v: num(v),
    "g": lambda v: css_gradient(v, "to bottom"),
    "gh": lambda v: css_gradient(v, "to right"),
    "ff": lambda v: font_families(v),
    "fs": lambda v: f"{num(v)}px",
    "s": lambda v: str(v),
}
PLACEHOLDER = re.compile(r"@\{(\w+):([\w.]+)\}")


def expand(text, t, toolkit):
    out, stack = [], [True]
    for lineno, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        if stripped.startswith("@if "):
            stack.append(stack[-1] and stripped[4:].strip() == toolkit)
            continue
        if stripped == "@endif":
            if len(stack) == 1:
                raise ValueError(f"line {lineno}: @endif without @if")
            stack.pop()
            continue
        if not stack[-1]:
            continue

        def sub(m):
            f, path = m.group(1), m.group(2)
            if f not in FILTERS:
                raise ValueError(f"line {lineno}: unknown filter {f!r}")
            return FILTERS[f](token(t, path))

        out.append(PLACEHOLDER.sub(sub, line))
    if len(stack) != 1:
        raise ValueError("unterminated @if")
    return "\n".join(out) + "\n"


# --- Template lint -------------------------------------------------------------------

LITERAL_COLOR = re.compile(r"#[0-9A-Fa-f]{3,8}\b|\brgba?\(")
LITERAL_PX = re.compile(r"(?<![\w.@{:-])(\d+(?:\.\d+)?)px\b")


def lint_template(path):
    errors = []
    for lineno, line in enumerate(path.read_text().splitlines(), 1):
        if "theme-ok" in line:
            continue
        code = PLACEHOLDER.sub("", line)
        code = re.sub(r"^\s*#.*$", "", code) if path.suffix == ".in" and "kvconfig" in path.name else code
        code = re.sub(r"/\*.*?\*/", "", code)
        if LITERAL_COLOR.search(code):
            errors.append(f"{path.relative_to(ROOT)}:{lineno}: literal color; use a @{{c:...}} token")
        for m in LITERAL_PX.finditer(code):
            if float(m.group(1)) > 1:
                errors.append(f"{path.relative_to(ROOT)}:{lineno}: literal size {m.group(0)}; use a @{{px:...}} token")
    return errors


# --- SVG helpers ---------------------------------------------------------------------

def svg_paint(c):
    r, g, b, a = parse_color(c) if isinstance(c, str) else c
    return f'"#{r:02x}{g:02x}{b:02x}"' + ("" if a == 255 else f' fill-opacity="{round(a / 255, 3)}"')


def svg_stroke(c):
    r, g, b, a = parse_color(c) if isinstance(c, str) else c
    return f'"#{r:02x}{g:02x}{b:02x}"' + ("" if a == 255 else f' stroke-opacity="{round(a / 255, 3)}"')


def grad_def(gid, stops, x2=0, y2=1, x1=0, y1=0, user_space=False):
    st = "".join(
        f'<stop offset="{s["pos"]}" stop-color="#{"%02x%02x%02x" % parse_color(s["color"])[:3]}"'
        + ("" if parse_color(s["color"])[3] == 255 else f' stop-opacity="{round(parse_color(s["color"])[3] / 255, 3)}"')
        + "/>"
        for s in stops)
    units = ' gradientUnits="userSpaceOnUse"' if user_space else ""
    return f'<linearGradient id="{gid}"{units} x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}">{st}</linearGradient>'


def svg_doc(w, h, body, defs=""):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">'
            f"<defs>{defs}</defs>{body}</svg>\n")


def flat(c):
    return [{"pos": 0.0, "color": c}, {"pos": 1.0, "color": c}]


def reverse(stops):
    return [{"pos": round(1 - s["pos"], 4), "color": s["color"]} for s in reversed(stops)]


def mix(a, b, f):
    ca, cb = parse_color(a) if isinstance(a, str) else a, parse_color(b) if isinstance(b, str) else b
    return hexc(tuple(round(x + (y - x) * f) for x, y in zip(ca, cb)))


def hexc(c):
    r, g, b, a = c
    return f"#{a:02X}{r:02X}{g:02X}{b:02X}" if a != 255 else f"#{r:02X}{g:02X}{b:02X}"


# --- GTK images ----------------------------------------------------------------------

def check_svg(t, kind, state):
    """13x13 check box (kind: unchecked/checked/mixed) or radio (radio-*)."""
    cb = token(t, "controls.checkBox")
    rd = token(t, "controls.radio")
    disabled_c = token(t, "controls.button.disabledBorderColor")
    radio = kind.startswith("radio")
    s = cb["size"] if not radio else rd["size"]
    border = disabled_c if state == "disabled" else (rd if radio else cb)["borderColor"]
    if state == "disabled":
        face = flat(token(t, "controls.button.gradient.disabled")[0]["color"])
    elif state == "active":
        face = rd["pressedGradient"] if radio else reverse(cb["gradient"])
    else:
        face = rd["gradient"] if radio else cb["gradient"]
    defs = grad_def("face", face, x2=1, y2=1)
    body = []
    hover = state == "hover"
    ring = (rd if radio else cb)["hoverRingColor"]
    if radio:
        c = s / 2
        body.append(f'<circle cx="{c}" cy="{c}" r="{c - 0.5}" fill="url(#face)" stroke={svg_stroke(border)}/>')
        if hover:
            body.append(f'<circle cx="{c}" cy="{c}" r="{c - 2}" fill="none" stroke={svg_stroke(ring)} stroke-width="2"/>')
        if kind == "radio-checked":
            dot = flat(disabled_c) if state == "disabled" else rd["dotGradient"]
            defs += grad_def("dot", dot, x2=1, y2=1)
            body.append(f'<circle cx="{c}" cy="{c}" r="{rd["dotSize"] / 2 + 0.25}" fill="url(#dot)"/>')
    else:
        body.append(f'<rect x="0.5" y="0.5" width="{s - 1}" height="{s - 1}" fill="url(#face)" stroke={svg_stroke(border)}/>')
        if hover:
            body.append(f'<rect x="2" y="2" width="{s - 4}" height="{s - 4}" fill="none" stroke={svg_stroke(ring)} stroke-width="2"/>')
        mark = disabled_c if state == "disabled" else cb["checkColor"]
        if kind == "checked":
            # A thick tick, three pixels wide like the classic glyph.
            body.append(f'<path d="M3 6.2 L5.3 8.5 L10 3.8 L10 6 L5.3 10.7 L3 8.4 Z" fill={svg_paint(mark)}/>')
        elif kind == "mixed":
            body.append(f'<rect x="3" y="3" width="{s - 6}" height="{s - 6}" fill={svg_paint(mark)}/>')
    return svg_doc(s, s, "".join(body), defs)


def arrow_svg(t, direction, color, size):
    """A small solid triangle pointing in direction, centred in size x size."""
    c, a = size / 2, size * 0.22
    pts = {
        "down": [(c - a * 1.6, c - a * 0.6), (c + a * 1.6, c - a * 0.6), (c, c + a)],
        "up": [(c - a * 1.6, c + a * 0.6), (c + a * 1.6, c + a * 0.6), (c, c - a)],
        "left": [(c + a * 0.6, c - a * 1.6), (c + a * 0.6, c + a * 1.6), (c - a, c)],
        "right": [(c - a * 0.6, c - a * 1.6), (c - a * 0.6, c + a * 1.6), (c + a, c)],
    }[direction]
    d = "M" + " L".join(f"{x:.2f} {y:.2f}" for x, y in pts) + " Z"
    return f'<path d="{d}" fill={svg_paint(color)}/>'


def chevron_svg(direction, color, size, width=1.6):
    """The XP scrollbar / combo arrow: a two-stroke chevron."""
    c, a = size / 2, size * 0.2
    pts = {
        "down": [(c - a * 1.4, c - a * 0.7), (c, c + a * 0.7), (c + a * 1.4, c - a * 0.7)],
        "up": [(c - a * 1.4, c + a * 0.7), (c, c - a * 0.7), (c + a * 1.4, c + a * 0.7)],
        "left": [(c + a * 0.7, c - a * 1.4), (c - a * 0.7, c), (c + a * 0.7, c + a * 1.4)],
        "right": [(c - a * 0.7, c - a * 1.4), (c + a * 0.7, c), (c - a * 0.7, c + a * 1.4)],
    }[direction]
    d = "M" + " L".join(f"{x:.2f} {y:.2f}" for x, y in pts)
    return f'<path d="{d}" fill="none" stroke={svg_stroke(color)} stroke-width="{width}" stroke-linecap="square"/>'


def grip_svg(t, vertical, color):
    sb = token(t, "controls.scrollBar")
    n, length = sb["gripLines"], sb["gripLength"]
    w, h = (length, n * 2) if vertical else (n * 2, length)
    lines = []
    for i in range(n):
        if vertical:
            lines.append(f'<rect x="0" y="{i * 2}" width="{length - 1}" height="1" fill="#ffffff"/>'
                         f'<rect x="1" y="{i * 2 + 1}" width="{length - 1}" height="1" fill={svg_paint(color)}/>')
        else:
            lines.append(f'<rect x="{i * 2}" y="0" width="1" height="{length - 1}" fill="#ffffff"/>'
                         f'<rect x="{i * 2 + 1}" y="1" width="1" height="{length - 1}" fill={svg_paint(color)}/>')
    return svg_doc(w, h, "".join(lines))


def expander_svg(t, opened):
    """The tree view +/- box."""
    border = token(t, "controls.textField.borderColor")
    glyph = token(t, "colors.windowText")
    s = 9
    body = (f'<rect x="0.5" y="0.5" width="{s - 1}" height="{s - 1}" fill="#ffffff" stroke={svg_stroke(border)}/>'
            f'<rect x="2" y="4" width="5" height="1" fill={svg_paint(glyph)}/>')
    if not opened:
        body += f'<rect x="4" y="2" width="1" height="5" fill={svg_paint(glyph)}/>'
    return svg_doc(s, s, body)


def gtk_assets(t):
    files = {}
    for kind in ("unchecked", "checked", "mixed", "radio-unchecked", "radio-checked"):
        for state in ("normal", "hover", "active", "disabled"):
            name = ("check-" if not kind.startswith("radio") else "") + kind
            suffix = "" if state == "normal" else f"-{state}"
            files[f"{name}{suffix}.svg"] = check_svg(t, kind, state)
    menu = token(t, "controls.menu")
    text, hover_text = token(t, "colors.windowText"), menu["hoverText"]
    disabled = token(t, "colors.disabledText")
    g = menu["glyphSize"]
    for state, color in (("", text), ("-hover", hover_text), ("-disabled", disabled)):
        files[f"menu-check{state}.svg"] = svg_doc(
            g, g, f'<path d="M1 4 L3 6 L8 1 L8 3 L3 8 L1 6 Z" fill={svg_paint(color)}/>')
        r = menu["radioSize"] / 2
        files[f"menu-radio{state}.svg"] = svg_doc(
            g, g, f'<circle cx="{g / 2}" cy="{g / 2}" r="{r}" fill={svg_paint(color)}/>')
        files[f"menu-arrow{state}.svg"] = svg_doc(g, g, arrow_svg(t, "right", color, g))
    sb = token(t, "controls.scrollBar")
    arrow_c = sb["arrowColor"]
    size = sb["width"] - 2 * sb["arrowInset"] + 4
    for d in ("up", "down", "left", "right"):
        files[f"arrow-{d}.svg"] = svg_doc(size, size, chevron_svg(d, arrow_c, size))
        files[f"arrow-{d}-disabled.svg"] = svg_doc(size, size, chevron_svg(d, disabled, size))
        files[f"arrow-{d}-small.svg"] = svg_doc(size, size, arrow_svg(t, d, token(t, "controls.spinBox.arrowColor"), size))
    files["combo-arrow.svg"] = svg_doc(size, size, chevron_svg("down", token(t, "controls.comboBox.arrowColor"), size))
    files["combo-arrow-disabled.svg"] = svg_doc(size, size, chevron_svg("down", disabled, size))
    files["grip-v.svg"] = grip_svg(t, True, sb["gripColor"])
    files["grip-h.svg"] = grip_svg(t, False, sb["gripColor"])
    files["expander-closed.svg"] = expander_svg(t, False)
    files["expander-open.svg"] = expander_svg(t, True)
    return files


# --- Kvantum SVG ---------------------------------------------------------------------

class NinePatch:
    """Writes a Kvantum element as interior + 8 frame parts.

    A frame is described from the edge inwards: rows[0] is the outer border
    (or None for transparent), rows[1..] are ring rows; the interior is a
    gradient. Corners are cut at `radius` (outer row only)."""

    def __init__(self, out):
        self.out = out
        self.n = 0

    def grad(self, stops, vertical=True, box=None):
        """A gradient; with box=(x, y, w, h) it spans that area in user space,
        so the frame strips and the interior of one element share it."""
        self.n += 1
        gid = f"g{self.n}"
        if box:
            x, y, w, h = box
            self.out.defs.append(grad_def(gid, stops, x1=x, y1=y, x2=x if vertical else x + w,
                                          y2=y + h if vertical else y, user_space=True))
        else:
            self.out.defs.append(grad_def(gid, stops, x2=0 if vertical else 1, y2=1 if vertical else 0))
        return f"url(#{gid})"

    def element(self, name, x, y, frame, rows, interior, radius=0, size=12, sides="tlbr", vertical=True,
                interior_rows=None):
        """rows: per frame row from the outside: a color, or a (top, left, bottom, right) tuple."""
        f, w = frame, size
        o = self.out
        fill = self.grad(interior, vertical, (x, y, 2 * f + w, 2 * f + w)) if interior else "none"

        def row_color(i, side):
            if i >= len(rows):
                return None
            c = rows[i]
            if isinstance(c, tuple):
                c = c["tlbr".index(side)]
            return c

        # Interior.
        o.part(name, x + f, y + f, w, w, f'<rect x="{x + f}" y="{y + f}" width="{w}" height="{w}" fill="{fill}"/>'
               if interior else "")
        # Edges: f rows of 1px lines, then interior color for the rest.
        for side in "tlbr":
            if side == "t":
                rx, ry, rw, rh = x + f, y, w, f
            elif side == "b":
                rx, ry, rw, rh = x + f, y + f + w, w, f
            elif side == "l":
                rx, ry, rw, rh = x, y + f, f, w
            else:
                rx, ry, rw, rh = x + f + w, y + f, f, w
            body = []
            if interior:
                body.append(f'<rect x="{rx}" y="{ry}" width="{rw}" height="{rh}" fill="{fill}"/>')
            for i in range(f):
                c = row_color(i, side)
                if c is None:
                    continue
                if side == "t":
                    r = (rx, ry + i, rw, 1)
                elif side == "b":
                    r = (rx, ry + rh - 1 - i, rw, 1)
                elif side == "l":
                    r = (rx + i, ry, 1, rh)
                else:
                    r = (rx + rw - 1 - i, ry, 1, rh)
                body.append(f'<rect x="{r[0]}" y="{r[1]}" width="{r[2]}" height="{r[3]}" fill={svg_paint(c)}/>')
            o.part(f"{name}-{ {'t': 'top', 'b': 'bottom', 'l': 'left', 'r': 'right'}[side] }", rx, ry, rw, rh, "".join(body))
        # Corners.
        corners = {"topleft": (x, y, "t", "l"), "topright": (x + f + w, y, "t", "r"),
                   "bottomleft": (x, y + f + w, "b", "l"), "bottomright": (x + f + w, y + f + w, "b", "r")}
        for cname, (cx, cy, s1, s2) in corners.items():
            body = []
            for i in range(f):
                c = row_color(i, s1) if s1 in "tb" else row_color(i, s2)
                # Each ring is an L inside the corner square.
                if c is None:
                    continue
                # Outer row with a radius: drawn as an arc.
                if i == 0 and radius:
                    body.append(self.arc(cname, cx, cy, f, c, fill))
                    continue
                if i == 0:
                    body.append(self.corner_l(cname, cx, cy, f, i, c, fill if i == 0 else None))
                else:
                    body.append(self.corner_l(cname, cx, cy, f, i, c, None))
            if not body and interior:
                body.append(f'<rect x="{cx}" y="{cy}" width="{f}" height="{f}" fill="{fill}"/>')
            elif interior and not radius:
                body.insert(0, f'<rect x="{cx}" y="{cy}" width="{f}" height="{f}" fill="{fill}"/>')
            o.part(f"{name}-{cname}", cx, cy, f, f, "".join(body))

    @staticmethod
    def corner_l(cname, cx, cy, f, i, c, fill):
        # An L of 1px lines at distance i from the outer edges, within the corner square.
        top = "top" in cname
        left = "left" in cname
        hy = cy + i if top else cy + f - 1 - i
        vx = cx + i if left else cx + f - 1 - i
        hx0, hx1 = (vx, cx + f) if left else (cx, vx + 1)
        vy0, vy1 = (hy, cy + f) if top else (cy, hy + 1)
        return (f'<rect x="{hx0}" y="{hy}" width="{hx1 - hx0}" height="1" fill={svg_paint(c)}/>'
                f'<rect x="{vx}" y="{vy0}" width="1" height="{vy1 - vy0}" fill={svg_paint(c)}/>')

    @staticmethod
    def arc(cname, cx, cy, f, c, fill):
        # A rounded outer corner: the inside filled, the border stroked.
        r = f - 0.5
        if cname == "topleft":
            d = f"M{cx + 0.5} {cy + f} L{cx + 0.5} {cy + f} A{r} {r} 0 0 1 {cx + f} {cy + 0.5} L{cx + f} {cy + f} Z"
            s = f"M{cx + 0.5} {cy + f} A{r} {r} 0 0 1 {cx + f} {cy + 0.5}"
        elif cname == "topright":
            d = f"M{cx} {cy + 0.5} A{r} {r} 0 0 1 {cx + f - 0.5} {cy + f} L{cx} {cy + f} Z"
            s = f"M{cx} {cy + 0.5} A{r} {r} 0 0 1 {cx + f - 0.5} {cy + f}"
        elif cname == "bottomleft":
            d = f"M{cx + 0.5} {cy} A{r} {r} 0 0 0 {cx + f} {cy + f - 0.5} L{cx + f} {cy} Z"
            s = f"M{cx + 0.5} {cy} A{r} {r} 0 0 0 {cx + f} {cy + f - 0.5}"
        else:
            d = f"M{cx} {cy + f - 0.5} A{r} {r} 0 0 0 {cx + f - 0.5} {cy} L{cx} {cy} Z"
            s = f"M{cx} {cy + f - 0.5} A{r} {r} 0 0 0 {cx + f - 0.5} {cy}"
        return (f'<path d="{d}" fill="{fill}"/>' if fill != "none" else "") + \
            f'<path d="{s}" fill="none" stroke={svg_stroke(c)} stroke-width="1"/>'


class KvSvg:
    def __init__(self):
        self.defs, self.parts = [], []
        self.cursor_y = 0

    def part(self, pid, x, y, w, h, body):
        # The transparent rect fixes the element's bounds, which Kvantum
        # stretches into place.
        self.parts.append(f'<g id="{pid}"><rect x="{x}" y="{y}" width="{w}" height="{h}" fill="#000000" fill-opacity="0"/>{body}</g>')

    def next_row(self, height):
        y = self.cursor_y
        self.cursor_y += height + 4
        return y

    def render(self):
        h = self.cursor_y + 4
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="400" height="{h}" viewBox="0 0 400 {h}">'
                f'<defs>{"".join(self.defs)}</defs>\n' + "\n".join(self.parts) + "\n</svg>\n")


def kvantum_svg(t):
    k = KvSvg()
    np_ = NinePatch(k)
    c = t["controls"]
    colors = t["colors"]
    btn, f = c["button"], token(t, "toolkit.kvantum.frameWidth")
    hr, dr = btn["hoverRingColors"], btn["defaultRingColors"]
    disabled_face = btn["gradient"]["disabled"]

    # Buttons: border + 2px ring (orange when hovered).
    states = {
        "normal": ([btn["borderColor"]], btn["gradient"]["normal"]),
        "focused": ([btn["borderColor"], (hr[0], hr[1], hr[2], hr[1]), (hr[0], hr[1], hr[2], hr[1])], btn["gradient"]["hover"]),
        "pressed": ([btn["borderColor"]], btn["gradient"]["pressed"]),
        "toggled": ([btn["borderColor"]], btn["gradient"]["pressed"]),
        "disabled": ([btn["disabledBorderColor"]], disabled_face),
        "default": ([btn["borderColor"], (dr[0], dr[1], dr[2], dr[1]), (dr[0], dr[1], dr[2], dr[1])], btn["gradient"]["normal"]),
    }
    for i, (state, (rows, grad)) in enumerate(states.items()):
        y = k.next_row(12 + 2 * f)
        name = "button-default-indicator" if state == "default" else f"button-{state}"
        np_.element(name, 0, y, f, rows, grad, radius=btn["radius"])

    # Line edits and generic sunken frames (lists, text views).
    tf = c["textField"]
    for state, border, bg in (("normal", tf["borderColor"], tf["background"]),
                              ("focused", tf["focusBorderColor"], tf["background"]),
                              ("disabled", tf["borderColor"], tf["disabledBackground"])):
        y = k.next_row(16)
        np_.element(f"lineedit-{state}", 0, y, 2, [border, bg], flat(bg))
        y = k.next_row(16)
        np_.element(f"common-{state}", 0, y, 2, [border, bg], None)

    # Combo box drop-down button (blue, like the scrollbar arrows).
    cb = c["comboBox"]
    for state, grad in (("normal", cb["buttonGradient"]), ("focused", cb["buttonHoverGradient"]),
                        ("pressed", reverse(cb["buttonGradient"])), ("disabled", disabled_face)):
        y = k.next_row(16)
        np_.element(f"dropdown-{state}", 0, y, 2, [tf["background"], c["scrollBar"]["thumbBorderColor"]], grad, radius=0)

    # Spin box buttons.
    sp = c["spinBox"]
    for state, grad in (("normal", sp["buttonGradient"]), ("focused", sp["buttonHoverGradient"]),
                        ("pressed", sp["buttonPressedGradient"]), ("disabled", disabled_face)):
        y = k.next_row(16)
        np_.element(f"spin-{state}", 0, y, 2, [tf["background"], c["scrollBar"]["thumbBorderColor"]], grad)

    # Tabs: rounded top, hot line on the selected and hovered tab.
    tab = c["tab"]
    hot0, hot1 = tab["hotLineGradient"][0]["color"], tab["hotLineGradient"][-1]["color"]
    hot = (hot0, tab["borderColor"], tab["borderColor"], tab["borderColor"])
    hot_ring = (mix(hot0, hot1, 0.5), tab["gradient"][0]["color"], tab["gradient"][-1]["color"], tab["gradient"][0]["color"])
    hot_ring2 = (hot1, tab["gradient"][0]["color"], tab["gradient"][-1]["color"], tab["gradient"][0]["color"])
    plain = (tab["borderColor"],) * 4
    for state, rows, grad in (("normal", [plain], tab["gradient"]),
                              ("focused", [hot, hot_ring, hot_ring2], tab["gradient"]),
                              ("toggled", [hot, hot_ring, hot_ring2], flat(tab["paneBackground"])),
                              ("disabled", [plain], disabled_face)):
        y = k.next_row(12 + 2 * f)
        np_.element(f"tab-{state}", 0, y, f, rows, grad, radius=tab["radius"])
    y = k.next_row(16)
    np_.element("tabframe-normal", 0, y, 2, [tab["borderColor"], tab["paneBackground"]], tab["paneGradient"])

    # Group boxes.
    gb = c["groupBox"]
    y = k.next_row(12 + 2 * f)
    np_.element("group-normal", 0, y, f, [gb["borderColor"]], None, radius=gb["radius"])

    # Scrollbars: drawn horizontally; Kvantum rotates them for vertical bars.
    sb = c["scrollBar"]
    for state, grad in (("normal", sb["thumbGradient"]), ("focused", sb["thumbHoverGradient"]),
                        ("pressed", reverse(sb["thumbGradient"])), ("disabled", flat(sb["trackColor"]))):
        y = k.next_row(12 + 2 * f)
        border = sb["thumbBorderColor"] if state != "disabled" else colors["disabledText"]
        np_.element(f"scrollbarslider-{state}", 0, y, f, [border, colors["white"]], grad, radius=sb["thumbRadius"])
    y = k.next_row(16)
    np_.element("scrollbargroove-normal", 0, y, 1, [sb["trackColor"]], flat(sb["trackColor"]))
    grip = sb["gripColor"]
    for state in ("normal", "focused", "pressed"):
        y = k.next_row(sb["gripLength"])
        lines = "".join(
            f'<rect x="{1 + i * 2}" y="{y}" width="1" height="{sb["gripLength"] - 1}" fill="#ffffff"/>'
            f'<rect x="{2 + i * 2}" y="{y + 1}" width="1" height="{sb["gripLength"] - 1}" fill={svg_paint(grip)}/>'
            for i in range(sb["gripLines"]))
        k.part(f"grip-{state}", 0, y, sb["gripLines"] * 2 + 2, sb["gripLength"], lines)

    # Arrows (scrollbars, combos, spins, menus).
    arrow_size = 12
    for d in ("up", "down", "left", "right"):
        for state, color in (("normal", sb["arrowColor"]), ("focused", sb["arrowColor"]), ("pressed", sb["arrowColor"]),
                             ("toggled", colors["selectionText"]), ("disabled", colors["disabledText"])):
            y = k.next_row(arrow_size)
            k.part(f"arrow-{d}-{state}", 0, y, arrow_size, arrow_size,
                   f'<g transform="translate(0 {y})">{chevron_svg(d, color, arrow_size, 1.8)}</g>')
    for d, sym in (("plus", True), ("minus", False)):
        for state in ("normal", "focused", "pressed", "disabled"):
            y = k.next_row(9)
            body = expander_svg(t, not sym).split("</defs>", 1)[1].replace("</svg>\n", "")
            k.part(f"tree-{d}-{state}", 0, y, 9, 9, f'<g transform="translate(0 {y})">{body}</g>')

    # Progress bar: trough and the green block pattern.
    pb = c["progressBar"]
    y = k.next_row(16)
    np_.element("progress-normal", 0, y, 2, [pb["borderColor"], colors["white"]], flat(colors["white"]), radius=0)
    for state in ("normal", "disabled"):
        y = k.next_row(pb["height"])
        g = np_.grad(pb["blockGradient"] if state == "normal" else flat(colors["disabledText"]))
        k.part(f"progress-pattern-{state}", 0, y, pb["blockWidth"] + pb["blockSpacing"], pb["height"],
               f'<rect x="0" y="{y}" width="{pb["blockWidth"]}" height="{pb["height"]}" fill="{g}"/>')

    # Menus.
    menu = c["menu"]
    y = k.next_row(16)
    np_.element("menu-normal", 0, y, 2, [menu["borderColor"], menu["background"]], flat(menu["background"]))
    y = k.next_row(16)
    np_.element("menuitem-normal", 0, y, 1, [None], None)
    y = k.next_row(16)
    np_.element("menuitem-focused", 0, y, 1, [menu["hoverBackground"]], flat(menu["hoverBackground"]))
    y = k.next_row(16)
    np_.element("menubar-normal", 0, y, 1, [colors["windowBackground"]], flat(colors["windowBackground"]))
    for state in ("focused", "pressed", "toggled"):
        y = k.next_row(16)
        np_.element(f"menubaritem-{state}", 0, y, 1, [menu["hoverBackground"]], flat(menu["hoverBackground"]))

    # Item views: selection.
    lst = c["list"]
    for state, col in (("focused", mix(lst["selectionColor"], colors["white"], 0.8)),
                       ("pressed", lst["selectionColor"]), ("toggled", lst["selectionColor"])):
        y = k.next_row(16)
        np_.element(f"itemview-{state}", 0, y, 1, [col], flat(col))
    y = k.next_row(16)
    np_.element("itemview-toggled-inactive", 0, y, 1, [lst["inactiveSelectionColor"]], flat(lst["inactiveSelectionColor"]))

    # Header sections (list column headers).
    gtk = t["toolkit"]["gtk"]
    hdr = gtk["listHeaderGradient"]
    for state, rows in (("normal", [(None, None, gtk["listHeaderBorderColor"], gtk["listHeaderBorderColor"])]),
                        ("focused", [(None, None, gtk["listHeaderHotColor"], gtk["listHeaderBorderColor"]),
                                     (None, None, gtk["listHeaderHotColor"], None),
                                     (None, None, gtk["listHeaderHotColor"], None)]),
                        ("pressed", [(None, None, gtk["listHeaderBorderColor"], gtk["listHeaderBorderColor"])])):
        y = k.next_row(12 + 2 * f)
        np_.element(f"header-{state}", 0, y, f, rows, hdr if state != "pressed" else reverse(hdr))

    # Tool tips.
    tt = c["tooltip"]
    y = k.next_row(16)
    np_.element("tooltip-normal", 0, y, 1, [tt["borderColor"]], flat(tt["background"]))

    # Sliders: groove (and its filled part) and the thumb.
    sl = c["slider"]
    for state, col in (("normal", sl["trackColor"]), ("toggled", sl["trackColor"])):
        y = k.next_row(12)
        np_.element(f"slider-{state}", 0, y, 2, [sl["trackBorderColor"], sl["trackBorderColor"]], flat(col))
    for state, border, grad in (("normal", sl["thumbBorderColor"], sl["thumbGradient"]),
                                ("focused", sl["thumbHoverBorderColor"], sl["thumbGradient"]),
                                ("pressed", sl["thumbBorderColor"], reverse(sl["thumbGradient"])),
                                ("disabled", btn["disabledBorderColor"], disabled_face)):
        y = k.next_row(sl["thumbLength"])
        g = np_.grad(grad)
        w, h = sl["thumbWidth"], sl["thumbLength"]
        k.part(f"slidercursor-{state}", 0, y, w, h,
               f'<rect x="0.5" y="{y + 0.5}" width="{w - 1}" height="{h - 1}" rx="{sl["thumbRadius"]}" fill="{g}" stroke={svg_stroke(border)}/>')

    # Check boxes and radio buttons, as for GTK.
    size = c["checkBox"]["size"]
    kv_state = {"normal": "normal", "focused": "hover", "pressed": "active", "disabled": "disabled"}
    for kind, prefix in (("unchecked", "checkbox"), ("checked", "checkbox-checked"), ("mixed", "checkbox-tristate"),
                         ("radio-unchecked", "radio"), ("radio-checked", "radio-checked")):
        for state, gstate in kv_state.items():
            y = k.next_row(size)
            doc = check_svg(t, kind, gstate)
            defs, body = doc.split("<defs>", 1)[1].split("</defs>", 1)
            body = body.replace("</svg>\n", "")
            # Make gradient ids unique per element.
            uid = f"{prefix}-{state}".replace("-", "_")
            defs, body = defs.replace('id="', f'id="{uid}_'), body.replace("url(#", f"url(#{uid}_")
            k.defs.append(defs)
            k.part(f"{prefix}-{state}", 0, y, size, size, f'<g transform="translate(0 {y})">{body}</g>')

    # Focus rectangle: a thin, half-transparent line (Kvantum stretches it,
    # so it cannot be dotted).
    y = k.next_row(8)
    np_.element("focus", 0, y, 1, [mix(colors["focusRing"], "#00000000", 0.6)], None)
    return k.render()


# --- Session defaults ---------------------------------------------------------------

def point_size(px):
    """Pango/Qt point size for a pixel size at 96 dpi."""
    return max(1, round(px * 72 / 96))


def session_defaults(out, t):
    tk, a, f = t["toolkit"], t["assets"], t["fonts"]
    ui = f"{f['ui']['families'][0]} {point_size(f['ui']['pixelSize'])}"
    title = f"{f['title']['families'][0]} Bold {point_size(f['title']['pixelSize'])}"
    mono = f"{tk['monospaceFamily']} {point_size(tk['monospacePixelSize'])}"
    cursor_size = tk["cursors"]["sizes"][0]
    header = "# Generated by tools/gen-toolkit-themes.py from theme/{}/theme.json.\n".format(t["meta"]["id"])
    write(out / "dconf" / "meadow.d" / "00-meadow", header + "\n".join([
        "# Defaults for GTK/GNOME settings inside Meadow sessions only (DCONF_PROFILE);",
        "# anything the user sets explicitly still wins.",
        "[org/gnome/desktop/interface]",
        f"gtk-theme='{tk['gtkThemeName']}'",
        f"icon-theme='{a['iconTheme']}'",
        f"cursor-theme='{a['cursorTheme']}'",
        f"cursor-size={cursor_size}",
        f"font-name='{ui}'",
        f"document-font-name='{ui}'",
        f"monospace-font-name='{mono}'",
        "color-scheme='default'",
        "overlay-scrolling=false",
        "",
        "[org/gnome/desktop/wm/preferences]",
        f"titlebar-font='{title}'",
        "button-layout=':minimize,maximize,close'",
        "",
        "[org/gnome/desktop/sound]",
        f"theme-name='{a['soundTheme']}'",
        "event-sounds=true",
        "input-feedback-sounds=false",
        ""]))
    write(out / "xdg" / "Kvantum" / "kvantum.kvconfig",
          header + f"# Used only while ~/.config/Kvantum/kvantum.kvconfig does not exist.\n[General]\ntheme={tk['kvantumThemeName']}\n")
    for gtk in ("gtk-3.0", "gtk-4.0"):
        lines = ["[Settings]", f"gtk-theme-name={tk['gtkThemeName']}", f"gtk-icon-theme-name={a['iconTheme']}",
                 f"gtk-cursor-theme-name={a['cursorTheme']}", f"gtk-cursor-theme-size={cursor_size}",
                 f"gtk-font-name={ui}", f"gtk-sound-theme-name={a['soundTheme']}", "gtk-enable-event-sounds=true",
                 "gtk-decoration-layout=:minimize,maximize,close"]
        if gtk == "gtk-3.0":
            lines.append("gtk-overlay-scrolling=false")
        write(out / "xdg" / gtk / "settings.ini", header + "\n".join(lines) + "\n")
    write(out / "xdg" / "pcmanfm-qt" / "default" / "settings.conf", header + "\n".join([
        "# PCManFM-Qt defaults in Meadow sessions: Explorer-like, Meadow icons.",
        "[System]", f"IconThemeName={a['iconTheme']}", "Terminal=qterminal", "",
        "[Behavior]", "SingleClick=false", "QuickExec=false", "",
        "[FolderView]", "Mode=icon", "ShowHidden=false", "BigIconSize=48", "SmallIconSize=16",
        "SidePaneIconSize=16", "ThumbnailIconSize=128", "",
        "[Window]", "AlwaysShowTabs=false", "ShowMenuBar=true", "SidePaneMode=places", "ShowSidePane=true",
        "PathBarButtons=false", ""]))


# --- Driver --------------------------------------------------------------------------

def schemes(theme_root, only=None):
    out = []
    for p in sorted(Path(theme_root).glob("*/theme.json")):
        t = derive(json.loads(p.read_text()))
        if only and t["meta"]["id"] != only:
            continue
        out.append(t)
    if not out:
        raise SystemExit(f"no schemes found under {theme_root}")
    return out


def gtk_index(t):
    tk = t["toolkit"]
    return (f"[Desktop Entry]\nType=X-GNOME-Metatheme\nName={tk['gtkThemeName']}\n"
            f"Comment=Meadow {t['meta']['name']} (generated from theme.json)\nEncoding=UTF-8\n\n"
            f"[X-GNOME-Metatheme]\nGtkTheme={tk['gtkThemeName']}\nIconTheme={t['assets']['iconTheme']}\n"
            f"CursorTheme={t['assets']['cursorTheme']}\nButtonLayout=:minimize,maximize,close\n")


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text() != text:
        path.write_text(text)


def generate(out, theme_root, only=None):
    css_t = (TEMPLATES / "gtk.css.in").read_text()
    kv_t = (TEMPLATES / "kvantum" / "MeadowLuna.kvconfig.in").read_text()
    for t in schemes(theme_root, only):
        gname, kname = t["toolkit"]["gtkThemeName"], t["toolkit"]["kvantumThemeName"]
        base = out / "themes" / gname
        write(base / "index.theme", gtk_index(t))
        assets = gtk_assets(t)
        for tk in ("gtk3", "gtk4"):
            d = base / ("gtk-3.0" if tk == "gtk3" else "gtk-4.0")
            write(d / "gtk.css", f"/* Generated by tools/gen-toolkit-themes.py from theme/{t['meta']['id']}/theme.json. */\n"
                  + expand(css_t, t, tk))
            for name, svg in assets.items():
                write(d / "assets" / name, svg)
        kd = out / "Kvantum" / kname
        write(kd / f"{kname}.kvconfig", expand(kv_t, t, "kvantum"))
        write(kd / f"{kname}.svg", kvantum_svg(t))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path)
    ap.add_argument("--theme-root", type=Path, default=ROOT / "theme")
    ap.add_argument("--scheme")
    ap.add_argument("--lint", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--defaults", type=Path, metavar="DIR")
    args = ap.parse_args()
    if args.defaults:
        session_defaults(args.defaults, schemes(args.theme_root, args.scheme or "luna-blue")[0])
        return 0
    if args.lint:
        errors = []
        for p in (TEMPLATES / "gtk.css.in", TEMPLATES / "kvantum" / "MeadowLuna.kvconfig.in"):
            errors += lint_template(p)
        for e in errors:
            print(e)
        if errors:
            return 1
        # Every placeholder must resolve in every scheme.
        for t in schemes(args.theme_root):
            for tk in ("gtk3", "gtk4"):
                expand((TEMPLATES / "gtk.css.in").read_text(), t, tk)
            expand((TEMPLATES / "kvantum" / "MeadowLuna.kvconfig.in").read_text(), t, "kvantum")
        print("toolkit templates: OK")
        return 0
    if args.list:
        for t in schemes(args.theme_root, args.scheme):
            print(t["meta"]["id"], t["toolkit"]["gtkThemeName"], t["toolkit"]["kvantumThemeName"])
        return 0
    if not args.out:
        ap.error("--out is required")
    generate(args.out, args.theme_root, args.scheme)
    return 0


if __name__ == "__main__":
    sys.exit(main())
