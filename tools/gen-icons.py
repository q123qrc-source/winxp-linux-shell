#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate the original "meadow-icons" freedesktop icon theme (phase 9).

Every icon is drawn here from a small set of shared shapes (manila folder,
paper sheet, monitor, drive, disc, glossy badges, emblems) in one style:
saturated colours, a top-left light source, a darker outline and a soft
shadow. No icon is traced from or derived from any other icon set.

Each icon is written twice:
  scalable/<context>/<name>.svg   48-unit design, full detail
  16x16/<context>/<name>.svg      the same drawing with fine detail dropped,
                                  thicker outlines and shapes snapped to the
                                  16 px grid
Aliases (other freedesktop names, default applications) are symlinks.
The output is committed; `--check` (the icons_sync ctest) fails when the
committed theme differs from what this script draws.

  gen-icons.py [--output DIR]       write the theme (default: the source tree)
  gen-icons.py --check [DIR]        compare instead of writing
  gen-icons.py --list               print "context name [-> target]" lines
"""
import argparse
import filecmp
import math
import os
import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUT = ROOT / "toolkit-themes" / "icons" / "meadow-icons"
CONTEXTS = {"apps": "Applications", "actions": "Actions", "categories": "Categories", "devices": "Devices",
            "mimetypes": "MimeTypes", "places": "Places", "status": "Status"}

# --- Palette (artwork, not UI metrics: the UI reads theme.json) --------------------
P = {
    "outline": "#3A4459", "paper": ("#FFFFFF", "#E4E8F0"), "paperEdge": "#7D879B", "paperFold": "#C9D0DE",
    "line": "#A8B2C6",
    "manila": ("#FFEBA8", "#F4C653", "#DFA637"), "manilaBack": ("#F7D777", "#D49B2B"), "manilaEdge": "#A2741C",
    "metal": ("#F5F6F8", "#C3C7D0", "#9097A3"), "metalEdge": "#596070",
    "bezel": ("#F4F3EE", "#C9C6BC"), "bezelEdge": "#676459",
    "sky": ("#8CC8FA", "#2F7FD8"), "hill": ("#8ED46C", "#3F9A2E"), "sun": ("#FFE66B", "#F2B62A"),
    "blue": ("#7DB7FA", "#2B6FD6", "#1A4BA8"), "blueEdge": "#163E8A",
    "green": ("#9BE27A", "#3EAA2C", "#2A7F1E"), "greenEdge": "#1F6216",
    "red": ("#FF9A86", "#E04B2A", "#B02A12"), "redEdge": "#7F1C0A",
    "orange": ("#FFD48A", "#F59A24", "#C9700C"), "orangeEdge": "#8E4C06",
    "yellow": ("#FFF29A", "#F6CF2E", "#C89C12"), "yellowEdge": "#8C6A06",
    "gray": ("#E8EAEE", "#B5BAC4", "#8A909C"), "grayEdge": "#545A66",
    "dark": ("#5A6172", "#2B303B"), "black": "#1B1F27",
    "purple": ("#D5B3F5", "#8B54D0", "#5E2F9E"),
    "brown": ("#E6B98A", "#B07A44", "#7D4F23"), "brownEdge": "#5A3612",
    "skin": ("#FFE0BF", "#E8B98C"),
    "white": "#FFFFFF", "shadow": "#000000",
}


def stops(colors):
    n = len(colors)
    return [(i / (n - 1) if n > 1 else 0, c) for i, c in enumerate(colors)]


class Icon:
    """One SVG drawing in a 48-unit box. `small` drops detail for 16 px."""

    def __init__(self, small, uid):
        self.small, self.uid = small, uid
        self.defs, self.body = [], []
        self.n = 0
        self.depth = 0  # nesting of scaled emblem groups

    # Units: one output pixel is 1 unit at 48 px and 3 units at 16 px.
    @property
    def px(self):
        return 3.0 if self.small else 1.0

    def sw(self, k=1.0):
        # Outlines are thickened for 16 px, except inside emblem groups,
        # whose own scale already makes them about a pixel wide.
        return round(k * (2.4 if self.small and self.depth == 0 else 1.0), 2)

    def snap(self, v):
        return round(v / 3) * 3 if self.small else v

    def gid(self):
        self.n += 1
        return f"{self.uid}{self.n}"

    def lin(self, colors, x1=0, y1=0, x2=0, y2=1, opac=None):
        g = self.gid()
        st = []
        for i, (pos, c) in enumerate(stops(colors) if not isinstance(colors[0], tuple) else colors):
            o = "" if opac is None else f' stop-opacity="{opac[i]}"'
            st.append(f'<stop offset="{round(pos, 3)}" stop-color="{c}"{o}/>')
        self.defs.append(f'<linearGradient id="{g}" x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}">{"".join(st)}</linearGradient>')
        return f"url(#{g})"

    def rad(self, colors, cx=0.5, cy=0.5, r=0.5, fx=None, fy=None, opac=None):
        g = self.gid()
        st = []
        for i, (pos, c) in enumerate(stops(colors)):
            o = "" if opac is None else f' stop-opacity="{opac[i]}"'
            st.append(f'<stop offset="{round(pos, 3)}" stop-color="{c}"{o}/>')
        f = "" if fx is None else f' fx="{fx}" fy="{fy}"'
        self.defs.append(f'<radialGradient id="{g}" cx="{cx}" cy="{cy}" r="{r}"{f}>{"".join(st)}</radialGradient>')
        return f"url(#{g})"

    def add(self, s):
        self.body.append(s)

    def svg(self, size):
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 48 48">'
                f'<defs>{"".join(self.defs)}</defs>{"".join(self.body)}</svg>\n')

    # --- shapes ---------------------------------------------------------------------
    def rect(self, x, y, w, h, fill, stroke=None, r=0, sw=1.0, extra=""):
        if self.small:
            x2, y2 = self.snap(x + w), self.snap(y + h)
            x, y = self.snap(x), self.snap(y)
            w, h = max(3, x2 - x), max(3, y2 - y)
        s = f' stroke="{stroke}" stroke-width="{self.sw(sw)}" stroke-linejoin="round"' if stroke else ""
        rr = f' rx="{r}"' if r else ""
        self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}"{rr} fill="{fill}"{s}{extra}/>')

    def path(self, d, fill, stroke=None, sw=1.0, extra=""):
        s = f' stroke="{stroke}" stroke-width="{self.sw(sw)}" stroke-linejoin="round" stroke-linecap="round"' if stroke else ""
        self.add(f'<path d="{d}" fill="{fill}"{s}{extra}/>')

    def circle(self, cx, cy, r, fill, stroke=None, sw=1.0, extra=""):
        s = f' stroke="{stroke}" stroke-width="{self.sw(sw)}"' if stroke else ""
        self.add(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{fill}"{s}{extra}/>')

    def ellipse(self, cx, cy, rx, ry, fill, stroke=None, sw=1.0, extra=""):
        s = f' stroke="{stroke}" stroke-width="{self.sw(sw)}"' if stroke else ""
        self.add(f'<ellipse cx="{cx}" cy="{cy}" rx="{rx}" ry="{ry}" fill="{fill}"{s}{extra}/>')

    def line(self, x1, y1, x2, y2, color, sw=1.0, cap="round", extra=""):
        self.add(f'<path d="M{x1} {y1} L{x2} {y2}" stroke="{color}" stroke-width="{self.sw(sw)}" '
                 f'stroke-linecap="{cap}" fill="none"{extra}/>')

    def shadow(self, cx=24, cy=44, rx=19, ry=3.2):
        self.ellipse(cx, cy, rx, ry, self.rad([P["shadow"], P["shadow"]], opac=[0.35, 0]))

    def sheen(self, x, y, w, h, r=0, k=0.55):
        """White gloss over the upper part of a shape."""
        g = self.lin([P["white"], P["white"]], opac=[k, 0])
        rr = f' rx="{r}"' if r else ""
        self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}"{rr} fill="{g}"/>')

    def group(self, x, y, s, fn, *a, **kw):
        """Draw fn in a 16-unit box at (x, y) scaled to s units."""
        self.add(f'<g transform="translate({x} {y}) scale({round(s / 16, 4)})">')
        self.depth += 1
        fn(self, *a, **kw)
        self.depth -= 1
        self.add("</g>")


# --- Building blocks ---------------------------------------------------------------

def folder(ic, opened=False, shadow=True):
    if shadow:
        ic.shadow(24, 43.5, 21, 3)
    e = P["manilaEdge"]
    ic.path("M4 11 Q4 9 6 9 H17 Q18.5 9 19.5 10.5 L21 13 H42 Q44 13 44 15 V40 H4 Z",
            ic.lin(P["manilaBack"]), e, 1)
    if not ic.small:
        ic.rect(7, 15, 34, 10, P["paper"][0], P["paperEdge"], sw=0.6)
    if opened:
        ic.path("M9 20 H45 Q46.5 20 46 21.5 L41 40 Q40.6 41 39.5 41 H3 Q1.8 41 2.2 39.8 L7 21.2 Q7.4 20 9 20 Z",
                ic.lin(P["manila"], 0, 0, 0, 1), e, 1)
    else:
        ic.rect(4, 18, 40, 23, ic.lin(P["manila"]), e, r=1.5)
        ic.sheen(5, 19, 38, 6, k=0.6)


def sheet(ic, x=9, y=3, w=30, h=41, fold=9, lines=True, shadow=True, tint=None):
    if shadow:
        ic.shadow(x + w / 2, y + h + 0.5, w / 2 + 3, 2.5)
    e = P["paperEdge"]
    d = f"M{x} {y} H{x + w - fold} L{x + w} {y + fold} V{y + h} H{x} Z"
    ic.path(d, ic.lin(tint or P["paper"], 0, 0, 1, 1), e, 1)
    ic.path(f"M{x + w - fold} {y} V{y + fold} H{x + w} Z", P["paperFold"], e, 0.8)
    if lines and not ic.small:
        for i in range(int((h - 16) // 4)):
            ly = y + 13 + i * 4
            ic.line(x + 5, ly, x + w - 6 - (6 if i % 3 == 2 else 0), ly, P["line"], 1.2)


def monitor(ic, x=4, y=5, w=40, h=29, screen=True, stand=True, shadow=True):
    if shadow:
        ic.shadow(24, 44, 17, 2.5)
    if stand:
        ic.rect(x + w / 2 - 4, y + h - 1, 8, 6, ic.lin(P["metal"]), P["metalEdge"])
        ic.rect(x + w / 2 - 11, y + h + 5, 22, 4, ic.lin(P["metal"]), P["metalEdge"], r=1.5)
    ic.rect(x, y, w, h, ic.lin(P["bezel"], 0, 0, 1, 1), P["bezelEdge"], r=3)
    if screen:
        sx, sy, sw_, sh = x + 3.5, y + 3.5, w - 7, h - 8
        ic.rect(sx, sy, sw_, sh, ic.lin(P["sky"]), P["outline"], sw=0.8)
        if not ic.small:
            ic.path(f"M{sx} {sy + sh * 0.72} Q{sx + sw_ * 0.35} {sy + sh * 0.45} {sx + sw_ * 0.7} {sy + sh * 0.68} "
                    f"T{sx + sw_} {sy + sh * 0.6} V{sy + sh} H{sx} Z", ic.lin(P["hill"]))
            ic.sheen(sx, sy, sw_, sh * 0.45, k=0.35)
        else:
            ic.rect(sx, sy + sh * 0.6, sw_, sh * 0.4, P["hill"][1])
    ic.circle(x + w - 5, y + h - 2.3, 1.1 if not ic.small else 1.5, P["green"][1])


def window(ic, x=4, y=6, w=40, h=34, body=None):
    ic.shadow(24, y + h + 3, 19, 2.5)
    ic.rect(x, y, w, h, ic.lin(P["blue"]), P["blueEdge"], r=3)
    tb = 7 if not ic.small else 9
    ic.rect(x + 2, y + tb, w - 4, h - tb - 2, body or P["paper"][0], P["blueEdge"], sw=0.6)
    if not ic.small:
        ic.sheen(x, y, w, tb - 1, r=3, k=0.45)
        ic.rect(x + w - 7, y + 1.5, 5, 4, ic.lin(P["red"]), P["white"], r=1, sw=0.5)


def badge(ic, cx, cy, r, colors, edge, glyph=None, glyph_color=None):
    """A glossy round button with an optional glyph (a function in 16 units)."""
    ic.circle(cx, cy, r, ic.rad(colors, cx=0.4, cy=0.35, r=0.75), edge, 1)
    ic.ellipse(cx, cy - r * 0.42, r * 0.72, r * 0.45, ic.lin([P["white"], P["white"]], opac=[0.75, 0.05]))
    if glyph:
        s = r * 1.3
        ic.group(cx - s / 2, cy - s / 2, s, glyph, glyph_color or P["white"])


# Glyphs and emblems drawn in a 16-unit box (ic.group scales them).
def g_check(ic, c):
    ic.path("M2.5 8.5 L6.5 12.5 L13.5 4", "none", c, 2.4 if not ic.small else 1.4)


def g_cross(ic, c):
    ic.path("M4 4 L12 12 M12 4 L4 12", "none", c, 2.6 if not ic.small else 1.6)


def g_plus(ic, c):
    ic.path("M8 2.5 V13.5 M2.5 8 H13.5", "none", c, 2.8 if not ic.small else 1.6)


def g_minus(ic, c):
    ic.path("M2.5 8 H13.5", "none", c, 2.8 if not ic.small else 1.6)


def g_arrow(ic, c, direction="right"):
    rot = {"right": 0, "down": 90, "left": 180, "up": 270}[direction]
    ic.add(f'<g transform="rotate({rot} 8 8)">')
    ic.path("M2 6 H9 V2.5 L14.5 8 L9 13.5 V10 H2 Z", c)
    ic.add("</g>")


def g_info(ic, c):
    ic.circle(8, 3.6, 1.6, c)
    ic.path("M6 6.5 H9.3 V12.5 H10.8 V14 H5.6 V12.5 H7.1 V8 H6 Z", c)


def g_exclaim(ic, c):
    ic.path("M6.6 2 H9.4 L8.9 10 H7.1 Z", c)
    ic.circle(8, 12.6, 1.5, c)


def g_question(ic, c):
    ic.path("M4.6 5.4 Q4.8 2 8.2 2 Q11.6 2.1 11.6 5 Q11.6 7 9.4 8.1 Q8.8 8.5 8.8 9.6 V10.4 H7 V9.3 "
            "Q7 7.6 8.4 6.8 Q9.6 6.1 9.6 5.1 Q9.5 3.9 8.1 3.9 Q6.7 3.9 6.6 5.6 Z", c)
    ic.circle(7.9, 13, 1.4, c)


def g_power(ic, c):
    ic.path("M5 4.2 A5.6 5.6 0 1 0 11 4.2", "none", c, 2.2 if not ic.small else 1.4)
    ic.path("M8 1.5 V8", "none", c, 2.2 if not ic.small else 1.4)


def g_refresh(ic, c):
    ic.path("M12.6 6.2 A5 5 0 1 0 12.8 9.8", "none", c, 2.2 if not ic.small else 1.4)
    ic.path("M10.2 2.2 L14.4 6.6 L9.4 7.6 Z", c)


def g_play(ic, c):
    ic.path("M5 3 L13 8 L5 13 Z", c)


def g_stop(ic, c):
    ic.rect(4, 4, 8, 8, c)


def g_star(ic, c=None):
    pts = []
    for i in range(10):
        r = 7.5 if i % 2 == 0 else 3.2
        a = -math.pi / 2 + i * math.pi / 5
        pts.append(f"{8 + r * math.cos(a):.2f} {8.4 + r * math.sin(a):.2f}")
    ic.path("M" + " L".join(pts) + " Z", ic.lin(P["yellow"]), P["yellowEdge"], 0.6)


def e_note(ic):
    ic.path("M6 3 L14 1 V11 A2.6 2.2 0 1 1 12.4 9 V4.2 L7.6 5.4 V13 A2.6 2.2 0 1 1 6 11 Z",
            ic.lin(P["blue"]), P["blueEdge"], 0.5)


def e_picture(ic):
    ic.rect(1, 2, 14, 12, ic.lin(P["sky"]), P["outline"], sw=0.6)
    ic.circle(11, 5.5, 1.8, P["sun"][1])
    ic.path("M1 12 L5.5 7 L9 10.5 L11 8.8 L15 12.5 V14 H1 Z", ic.lin(P["hill"]))


def e_film(ic):
    ic.rect(2, 1, 12, 14, P["dark"][1], P["black"], sw=0.5)
    for i in range(4):
        ic.rect(2.8, 2 + i * 3.4, 1.6, 1.8, P["white"])
        ic.rect(11.6, 2 + i * 3.4, 1.6, 1.8, P["white"])
    ic.rect(5.3, 3, 5.4, 4, ic.lin(P["sky"]))
    ic.rect(5.3, 9, 5.4, 4, ic.lin(P["orange"]))


def e_globe(ic):
    ic.circle(8, 8, 7, ic.rad(["#BDE2FF", P["blue"][1], P["blue"][2]], cx=0.35, cy=0.3, r=0.8), P["blueEdge"], 0.6)
    ic.path("M3.2 5.4 Q5.6 3.6 7.4 4.6 Q8.2 6.4 6.4 7.2 Q5.6 8.8 6.8 10.6 Q5 11.6 3.4 9.4 Q2.4 7.4 3.2 5.4 Z",
            ic.lin(P["green"]))
    ic.path("M9.4 2.6 Q12.6 3.4 13.8 6.2 Q12.2 6.8 11.2 6 Q10 5.2 9.4 2.6 Z M10 9.2 Q12.4 8.6 13.2 10.4 "
            "Q12 13 10 13.6 Q9.6 11.4 10 9.2 Z", ic.lin(P["green"]))
    ic.ellipse(6.6, 4.6, 3.8, 2.2, ic.lin([P["white"], P["white"]], opac=[0.7, 0]))


def e_gear(ic, colors=None, edge=None):
    colors, edge = colors or P["gray"], edge or P["grayEdge"]
    teeth = 8
    pts = []
    for i in range(teeth * 2):
        a = i * math.pi / teeth
        for da, r in ((-0.18, 7.4 if i % 2 == 0 else 5.6), (0.18, 7.4 if i % 2 == 0 else 5.6)):
            pts.append(f"{8 + r * math.cos(a + da):.2f} {8 + r * math.sin(a + da):.2f}")
    ic.path("M" + " L".join(pts) + " Z", ic.lin(colors, 0, 0, 1, 1), edge, 0.6)
    ic.circle(8, 8, 2.2, P["white"], edge, 0.6)


def e_down(ic):
    g_arrow(ic, ic.lin(P["green"]), "down")


def e_lock(ic):
    ic.path("M4.5 7 V5 A3.5 3.5 0 0 1 11.5 5 V7", "none", P["metalEdge"], 1.8)
    ic.rect(3, 7, 10, 8, ic.lin(P["yellow"], 0, 0, 1, 1), P["yellowEdge"], r=1, sw=0.6)
    ic.circle(8, 10.6, 1.3, P["yellowEdge"])


def e_people(ic):
    ic.circle(5.5, 4.5, 2.6, ic.lin(P["skin"]), P["brownEdge"], 0.4)
    ic.path("M1 14 Q1 8 5.5 8 Q10 8 10 14 Z", ic.lin(P["blue"]), P["blueEdge"], 0.5)
    ic.circle(11, 6, 2.4, ic.lin(P["skin"]), P["brownEdge"], 0.4)
    ic.path("M7.2 15 Q7.2 9.6 11 9.6 Q15 9.6 15 15 Z", ic.lin(P["green"]), P["greenEdge"], 0.5)


def e_house(ic):
    ic.path("M1.5 8 L8 2 L14.5 8 H12.5 V14.5 H3.5 V8 Z", P["paper"][0], P["outline"], 0.6)
    ic.path("M0.5 8.4 L8 1.2 L15.5 8.4 L14.2 9.6 L8 3.8 L1.8 9.6 Z", ic.lin(P["red"]), P["redEdge"], 0.5)
    ic.rect(6.5, 10, 3, 4.5, ic.lin(P["brown"]))


def e_ruler(ic):
    ic.path("M1 11 L11 1 L15 5 L5 15 Z", ic.lin(P["yellow"]), P["yellowEdge"], 0.5)
    for i in range(1, 5):
        ic.line(1 + i * 2.2 + 0.6, 11 - i * 2.2 + 0.6, 2 + i * 2.2 + 1.5, 12 - i * 2.2 + 1.5, P["yellowEdge"], 0.6)


def e_pencil(ic):
    ic.path("M2 14 L3 10.5 L11.5 2 L14 4.5 L5.5 13 Z", ic.lin(P["orange"], 0, 0, 1, 1), P["orangeEdge"], 0.5)
    ic.path("M2 14 L3 10.5 L5.5 13 Z", P["skin"][1])
    ic.path("M11.5 2 L14 4.5 L15 3.5 Q15.4 2.8 14.6 2 L14 1.4 Q13.2 0.6 12.5 1 Z", P["red"][1])


def e_chart(ic):
    for i, (h, col) in enumerate(((6, P["blue"]), (10, P["green"]), (8, P["orange"]), (13, P["red"]))):
        ic.rect(1.5 + i * 3.4, 15 - h, 2.8, h, ic.lin(col, 0, 0, 1, 0))


def e_grid(ic):
    ic.rect(1, 2, 14, 12, P["white"], P["green"][2], sw=0.6)
    ic.rect(1, 2, 14, 3, P["green"][0])
    for i in range(1, 4):
        ic.line(1, 2 + i * 3, 15, 2 + i * 3, P["green"][2], 0.4)
    for i in range(1, 3):
        ic.line(1 + i * 4.6, 2, 1 + i * 4.6, 14, P["green"][2], 0.4)


def e_letter_a(ic):
    ic.path("M1.5 15 L6.6 1.5 H9.4 L14.5 15 H11.6 L10.4 11.6 H5.6 L4.4 15 Z M6.4 9.2 H9.6 L8 4.6 Z",
            ic.lin(P["dark"]))


def e_disc(ic):
    ic.circle(8, 8, 7, ic.rad(["#FFFFFF", "#DDE3EE", "#A9B4C8"], cx=0.4, cy=0.4, r=0.7), P["metalEdge"], 0.5)
    ic.path("M8 1 A7 7 0 0 1 15 8 L11 8 A3 3 0 0 0 8 5 Z", ic.lin(["#FFB3E6", "#9FD3FF"], 0, 0, 1, 1),
            extra=' fill-opacity="0.6"')
    ic.circle(8, 8, 1.6, P["white"], P["metalEdge"], 0.5)


def e_brackets(ic):
    ic.path("M5.5 2.5 L1.5 8 L5.5 13.5 M10.5 2.5 L14.5 8 L10.5 13.5", "none", P["blue"][2], 1.8)


def e_clock(ic):
    ic.circle(8, 8, 7, P["white"], P["blue"][2], 1.2)
    ic.path("M8 3.6 V8 L11 10", "none", P["black"], 1.2)


def e_palette(ic):
    ic.path("M8 1 Q15 1 15 7.5 Q15 11 12 11 Q10.5 11 10.5 12.5 Q10.5 15 8 15 Q1 15 1 8 Q1 1 8 1 Z",
            ic.lin(P["brown"], 0, 0, 1, 1), P["brownEdge"], 0.5)
    for (x, y), col in zip(((5, 5), (8.6, 3.8), (12, 5.6), (4.4, 9.4)), (P["red"][1], P["yellow"][1], P["blue"][1], P["green"][1])):
        ic.circle(x, y, 1.5, col)


def e_key(ic):
    ic.circle(4.5, 8, 3.5, ic.lin(P["yellow"]), P["yellowEdge"], 0.6)
    ic.path("M7.5 7 H15 V9 H13.5 V11 H12 V9 H7.5 Z", ic.lin(P["yellow"]), P["yellowEdge"], 0.5)
    ic.circle(3.6, 8, 1, P["yellowEdge"])


def e_envelope(ic):
    ic.rect(1, 3.5, 14, 10, ic.lin(P["paper"]), P["paperEdge"], sw=0.6)
    ic.path("M1 3.5 L8 9.5 L15 3.5", "none", P["paperEdge"], 0.6)


def e_speaker(ic, waves=3, muted=False):
    ic.path("M1.5 6 H4.5 L8.5 2.5 V13.5 L4.5 10 H1.5 Z", ic.lin(P["gray"], 0, 0, 1, 1), P["grayEdge"], 0.6)
    if muted:
        ic.path("M10.5 5.5 L15 10.5 M15 5.5 L10.5 10.5", "none", P["red"][1], 1.6)
        return
    for i in range(waves):
        r = 3 + i * 2.4
        ic.path(f"M{8.5 + r * 0.55:.2f} {8 - r * 0.8:.2f} A{r} {r} 0 0 1 {8.5 + r * 0.55:.2f} {8 + r * 0.8:.2f}",
                "none", P["blue"][2], 1.2)


def e_box(ic):
    ic.path("M1.5 5 L8 2 L14.5 5 V12 L8 15 L1.5 12 Z", ic.lin(P["brown"], 0, 0, 1, 1), P["brownEdge"], 0.6)
    ic.path("M1.5 5 L8 8 L14.5 5 M8 8 V15", "none", P["brownEdge"], 0.6)
    ic.path("M4.8 3.5 L11.2 6.5", "none", P["yellow"][0], 1.2)


def e_window(ic):
    ic.rect(1, 2, 14, 12, ic.lin(P["blue"]), P["blueEdge"], r=1.2, sw=0.6)
    ic.rect(2, 5, 12, 8, P["white"])


def e_flask(ic):
    ic.path("M6 1.5 H10 V5.5 L14.5 13.5 Q15 15 13.4 15 H2.6 Q1 15 1.5 13.5 L6 5.5 Z", P["white"], P["outline"], 0.6)
    ic.path("M3.6 10 H12.4 L14.3 13.6 Q14.6 14.4 13.6 14.4 H2.4 Q1.4 14.4 1.7 13.6 Z", ic.lin(P["green"]))


def e_wrench(ic):
    ic.path("M3 13 L9.5 6.5 A3.6 3.6 0 0 1 14.2 2 L12 4.2 L12.6 5.6 L14 6.2 L16 4 A3.6 3.6 0 0 1 11.5 8.6 L5 15 Z",
            ic.lin(P["metal"], 0, 0, 1, 1), P["metalEdge"], 0.6)


def e_die(ic):
    ic.rect(1.5, 1.5, 13, 13, ic.lin(P["paper"], 0, 0, 1, 1), P["outline"], r=2.5, sw=0.6)
    for x, y in ((4.6, 4.6), (8, 8), (11.4, 11.4), (11.4, 4.6), (4.6, 11.4)):
        ic.circle(x, y, 1.3, P["red"][2])


def e_scissors(ic):
    ic.circle(4, 12, 2.6, "none", P["red"][2], 1.4)
    ic.circle(12, 12, 2.6, "none", P["red"][2], 1.4)
    ic.path("M5.6 10 L11.5 1.5 M10.4 10 L4.5 1.5", "none", P["metalEdge"], 1.4)


def e_clipboard(ic):
    ic.rect(2, 2.5, 12, 13, ic.lin(P["brown"]), P["brownEdge"], r=1, sw=0.6)
    ic.rect(4, 5, 8, 9, P["white"], P["paperEdge"], sw=0.4)
    ic.rect(5.5, 1, 5, 3, ic.lin(P["metal"]), P["metalEdge"], r=0.8, sw=0.4)


def e_magnifier(ic):
    ic.line(9.6, 9.6, 14.5, 14.5, P["dark"][1], 2.6)
    ic.circle(6.5, 6.5, 5, ic.rad(["#FFFFFF", "#C9E4FF", "#7FB4EC"], cx=0.35, cy=0.35, r=0.75), P["dark"][1], 1.4)


def e_undo(ic, redo=False):
    if redo:
        ic.add('<g transform="translate(16 0) scale(-1 1)">')
    ic.path("M4.5 6 H10 A4.5 4.5 0 0 1 10 15 H6 V12.5 H10 A2 2 0 0 0 10 8.5 H4.5 V11 L0.5 7.2 L4.5 3.5 Z",
            ic.lin(P["blue"]), P["blueEdge"], 0.5)
    if redo:
        ic.add("</g>")


def e_door(ic):
    ic.rect(3, 1, 8, 14, ic.lin(P["brown"], 0, 0, 1, 0), P["brownEdge"], sw=0.6)
    ic.circle(9.2, 8.5, 0.9, P["yellow"][1])
    ic.path("M9 6 H13 V3.5 L16 8 L13 12.5 V10 H9 Z", ic.lin(P["green"]), P["greenEdge"], 0.4)


# --- Icon drawings (48 units) -------------------------------------------------------

def emblem_on_folder(fn):
    def draw(ic):
        folder(ic)
        if fn:
            s = 18 if not ic.small else 21
            ic.group(23 - s / 2 + 1, 29 - s / 2 + 1, s, fn)
    return draw


def emblem_on_sheet(fn, lines=True, size=20):
    def draw(ic):
        sheet(ic, lines=lines)
        if fn:
            s = size if not ic.small else 24
            ic.group(24 - s / 2, 26 - s / 2, s, fn)
    return draw


def corner_badge(base, colors, edge, glyph, cx=36, cy=36, r=8.5):
    def draw(ic):
        base(ic)
        rr = r if not ic.small else 10.5
        badge(ic, cx if not ic.small else 35, cy if not ic.small else 35, rr, colors, edge, glyph)
    return draw


def corner_emblem(base, fn, x=26, y=26, s=20):
    def draw(ic):
        base(ic)
        ss = s if not ic.small else s + 2
        ic.group(x if not ic.small else 24, y if not ic.small else 24, ss, fn)
    return draw


def big(fn, pad=4):
    def draw(ic):
        ic.shadow(24, 44.5, 17, 2.6)
        ic.group(pad, pad - 1, 48 - 2 * pad, fn)
    return draw


def computer(ic):
    ic.shadow(24, 44, 21, 2.8)
    ic.rect(31, 7, 13, 35, ic.lin(P["bezel"], 0, 0, 1, 0), P["bezelEdge"], r=2)
    if not ic.small:
        ic.rect(33.5, 11, 8, 2.2, P["dark"][0], r=0.8)
        ic.rect(33.5, 15, 8, 2.2, P["dark"][0], r=0.8)
        ic.circle(37.5, 36, 1.2, P["green"][1])
    monitor(ic, x=2, y=8, w=30, h=23, shadow=False)


def harddisk(ic, light=P["green"][1], slot=False, usb=False):
    ic.shadow(24, 41, 21, 2.6)
    ic.path("M4 22 L9 14 H44 L44 22 Z", ic.lin([P["metal"][0], P["metal"][1]]), P["metalEdge"], 1)
    ic.rect(4, 22, 40, 15, ic.lin(P["metal"]), P["metalEdge"], r=1.5)
    ic.path("M44 14 V36", "none", P["metalEdge"], 1)
    if slot:
        ic.rect(9, 27, 22, 3, P["dark"][1], r=1)
    elif not ic.small:
        for i in range(4):
            ic.line(9 + i * 3, 26, 9 + i * 3, 33, P["metal"][2], 1)
    ic.circle(39, 31, 1.8 if not ic.small else 2.4, light)


def usbstick(ic):
    ic.shadow(24, 39, 18, 2.4)
    ic.rect(34, 19, 10, 12, ic.lin(P["metal"]), P["metalEdge"], r=1)
    if not ic.small:
        ic.rect(37, 22, 3, 2, P["dark"][1])
        ic.rect(37, 26, 3, 2, P["dark"][1])
    ic.rect(5, 15, 30, 20, ic.lin(P["blue"]), P["blueEdge"], r=4)
    ic.sheen(6, 16, 28, 7, r=3)
    ic.circle(10.5, 25, 1.6, P["orange"][1])


def disc(ic):
    ic.shadow(24, 44, 18, 2.4)
    ic.group(4, 3, 40, e_disc)


def floppy(ic):
    ic.shadow(24, 44.5, 19, 2.5)
    ic.path("M6 4 H38 L44 10 V42 H6 Z", ic.lin(P["blue"], 0, 0, 1, 1), P["blueEdge"], 1)
    ic.rect(14, 4, 20, 13, ic.lin(P["metal"], 0, 0, 1, 0), P["metalEdge"], sw=0.8)
    ic.rect(26, 6.5, 4, 8, P["dark"][1])
    ic.rect(11, 22, 27, 18, ic.lin(P["paper"]), P["paperEdge"], sw=0.8)
    if not ic.small:
        for i in range(3):
            ic.line(14, 27 + i * 4, 35, 27 + i * 4, P["line"], 1)


def sdcard(ic):
    ic.shadow(24, 44.5, 15, 2.4)
    ic.path("M12 4 H32 L38 10 V42 H12 Z", ic.lin(P["blue"], 0, 0, 1, 1), P["blueEdge"], 1)
    for i in range(5):
        ic.rect(15 + i * 4, 6, 2.4, 6, P["yellow"][1])
    ic.rect(15, 22, 20, 14, ic.lin(P["paper"]), P["paperEdge"], sw=0.8)


def printer(ic):
    ic.shadow(24, 44.5, 21, 2.6)
    ic.rect(12, 4, 24, 16, ic.lin(P["paper"]), P["paperEdge"])
    ic.rect(3, 17, 42, 18, ic.lin(P["gray"]), P["grayEdge"], r=3)
    ic.sheen(4, 18, 40, 6, r=3)
    ic.rect(9, 31, 30, 12, ic.lin(P["paper"]), P["paperEdge"])
    if not ic.small:
        for i in range(2):
            ic.line(13, 35 + i * 3.5, 33, 35 + i * 3.5, P["line"], 1)
    ic.circle(39, 23, 1.6, P["green"][1])


def scanner(ic):
    ic.shadow(24, 42, 21, 2.6)
    ic.rect(3, 20, 42, 16, ic.lin(P["gray"]), P["grayEdge"], r=3)
    ic.path("M5 20 L9 11 H41 L44 20 Z", ic.lin(P["dark"]), P["black"], 1)
    ic.rect(8, 25, 32, 3, ic.lin(["#C8FFE0", P["green"][1]], 0, 0, 1, 0))
    ic.circle(39, 31, 1.6, P["green"][1])


def keyboard(ic):
    ic.shadow(24, 40, 22, 2.4)
    ic.rect(2, 16, 44, 21, ic.lin(P["bezel"]), P["bezelEdge"], r=3)
    rows = 3 if ic.small else 4
    for r in range(rows):
        for k in range(10 if not ic.small else 6):
            kw = 3.2 if not ic.small else 5.4
            ic.rect(5 + k * (kw + 0.9), 19 + r * (4.2 if not ic.small else 5.4), kw, 3.2 if not ic.small else 4.2,
                    P["white"], P["gray"][2], r=0.6, sw=0.5)
    if not ic.small:
        ic.rect(13, 32, 22, 3.2, P["white"], P["gray"][2], r=0.6, sw=0.5)


def mouse(ic):
    ic.shadow(26, 44, 12, 2.4)
    ic.path("M24 6 Q24 2 30 2 Q36 2 40 5", "none", P["dark"][1], 1.5)
    ic.path("M14 16 Q14 6 26 6 Q38 6 38 16 V30 Q38 43 26 43 Q14 43 14 30 Z", ic.lin(P["bezel"], 0, 0, 1, 1),
            P["bezelEdge"], 1)
    ic.path("M14 18 H38 M26 6 V18", "none", P["bezelEdge"], 1)
    ic.sheen(16, 7, 9, 9, r=4)


def gamepad(ic):
    ic.shadow(24, 41, 20, 2.4)
    ic.path("M8 14 H40 Q46 14 46 24 Q46 38 39 38 Q35 38 32 32 H16 Q13 38 9 38 Q2 38 2 24 Q2 14 8 14 Z",
            ic.lin(P["dark"]), P["black"], 1)
    ic.path("M10 23 H14 V19 H17 V23 H21 V26 H17 V30 H14 V26 H10 Z", P["gray"][1])
    for (x, y), c in zip(((34, 21), (38, 25), (30, 25), (34, 29)), (P["green"][1], P["red"][1], P["blue"][1], P["yellow"][1])):
        ic.circle(x, y, 2, c)


def audiocard(ic):
    ic.shadow(24, 42, 21, 2.4)
    ic.rect(3, 10, 42, 26, ic.lin(P["green"], 0, 0, 1, 1), P["greenEdge"], r=1.5)
    ic.rect(8, 15, 12, 10, P["dark"][1], r=1)
    ic.rect(24, 15, 8, 8, P["dark"][1], r=1)
    for i in range(9):
        ic.rect(6 + i * 4, 36, 2.4, 6, P["yellow"][1])
    ic.circle(38, 18, 3, ic.lin(P["metal"]), P["metalEdge"], 0.6)
    ic.circle(38, 28, 3, ic.lin(P["metal"]), P["metalEdge"], 0.6)


def camera(ic):
    ic.shadow(24, 42, 20, 2.4)
    ic.rect(4, 14, 40, 26, ic.lin(P["dark"]), P["black"], r=4)
    ic.rect(8, 10, 11, 6, P["dark"][0], r=1.5)
    ic.circle(26, 27, 10, ic.lin(P["metal"]), P["metalEdge"], 1)
    ic.circle(26, 27, 6.5, ic.rad(["#9FD3FF", "#1C3F7A", "#0B1A33"], cx=0.35, cy=0.35, r=0.8))
    ic.circle(38, 19, 1.8, P["orange"][1])


def webcam(ic):
    ic.shadow(24, 44, 12, 2.2)
    ic.rect(20, 33, 8, 6, ic.lin(P["metal"]), P["metalEdge"])
    ic.rect(13, 38, 22, 4, ic.lin(P["metal"]), P["metalEdge"], r=1.5)
    ic.circle(24, 19, 15, ic.lin(P["bezel"], 0, 0, 1, 1), P["bezelEdge"], 1)
    ic.circle(24, 19, 7, ic.rad(["#9FD3FF", "#1C3F7A", "#0B1A33"], cx=0.35, cy=0.35, r=0.8), P["black"], 1)


def phone(ic):
    ic.shadow(24, 45, 12, 2.2)
    ic.rect(13, 3, 22, 41, ic.lin(P["dark"]), P["black"], r=3.5)
    ic.rect(15.5, 7, 17, 31, ic.lin(P["sky"]))
    ic.circle(24, 41, 1.4, P["gray"][1])


def headphones(ic):
    ic.shadow(24, 44, 18, 2.4)
    ic.path("M8 30 V24 A16 16 0 0 1 40 24 V30", "none", P["dark"][1], 3.4)
    ic.rect(4, 26, 10, 16, ic.lin(P["blue"]), P["blueEdge"], r=4)
    ic.rect(34, 26, 10, 16, ic.lin(P["blue"]), P["blueEdge"], r=4)


def speakerbox(ic):
    ic.shadow(24, 45, 14, 2.4)
    ic.rect(12, 3, 24, 40, ic.lin(P["dark"]), P["black"], r=3)
    ic.circle(24, 13, 5, ic.rad([P["gray"][1], P["dark"][1]]), P["black"], 0.8)
    ic.circle(24, 31, 8, ic.rad([P["gray"][1], P["dark"][1]]), P["black"], 0.8)


def battery(ic, level=1.0, charging=False, color=None):
    ic.shadow(24, 39, 20, 2.2)
    ic.rect(41, 20, 4, 8, ic.lin(P["metal"]), P["metalEdge"], r=1)
    ic.rect(3, 13, 38, 22, ic.lin(P["paper"]), P["outline"], r=3)
    w = 34 * level
    if w > 0:
        col = color or (P["green"] if level > 0.35 else P["orange"] if level > 0.15 else P["red"])
        ic.rect(5, 15, max(w, 2), 18, ic.lin(col), r=1.5)
    ic.sheen(4, 14, 36, 8, r=3)
    if charging:
        ic.path("M24 14 L16 26 H23 L20 36 L30 22 H23 Z", ic.lin(P["yellow"]), P["yellowEdge"], 0.8)


def globe(ic):
    ic.shadow(24, 44.5, 16, 2.4)
    ic.group(4, 3, 40, e_globe)


def envelope(ic):
    ic.shadow(24, 42, 21, 2.5)
    ic.rect(3, 10, 42, 29, ic.lin(P["paper"], 0, 0, 1, 1), P["paperEdge"], r=1.5)
    ic.path("M3.5 11 L24 28 L44.5 11", "none", P["paperEdge"], 1)
    ic.path("M3.5 38.5 L18 24 M44.5 38.5 L30 24", "none", P["line"], 1)
    ic.rect(35, 14, 6, 7, ic.lin(P["red"]), P["redEdge"], sw=0.5)


def terminal(ic):
    window(ic, body=P["black"])
    ic.path("M10 20 L16 24 L10 28", "none", P["green"][0], 2.2 if not ic.small else 2.6)
    ic.line(18, 30, 26, 30, P["green"][0], 2.2 if not ic.small else 2.6, cap="butt")


def notepad(ic):
    sheet(ic, x=8, y=6, w=30, h=38, fold=0 or 0.01, lines=True)
    ic.rect(8, 4, 30, 7, ic.lin(P["blue"]), P["blueEdge"], r=1.5)
    if not ic.small:
        for i in range(6):
            ic.circle(12 + i * 4.4, 7.5, 1, P["white"])
    ic.group(26, 24, 20, e_pencil)


def calculator(ic):
    ic.shadow(24, 45, 15, 2.4)
    ic.rect(9, 3, 30, 41, ic.lin(P["gray"], 0, 0, 1, 1), P["grayEdge"], r=3)
    ic.rect(13, 7, 22, 8, ic.lin(["#E6F1D8", "#B9CC9E"]), P["outline"], sw=0.6)
    cols, rows = (4, 4) if not ic.small else (3, 3)
    kw = 22 / cols
    for r in range(rows):
        for c in range(cols):
            col = P["orange"] if (r == rows - 1 and c == cols - 1) else P["paper"]
            ic.rect(13 + c * kw + 0.4, 19 + r * (22 / rows) + 0.4, kw - 1.4, 22 / rows - 1.6,
                    ic.lin(col), P["grayEdge"], r=1, sw=0.5)


def helpbook(ic):
    ic.shadow(24, 45, 17, 2.4)
    ic.path("M8 6 Q8 3 11 3 H40 V40 H11 Q8 40 8 43 Z", ic.lin(P["blue"], 0, 0, 1, 1), P["blueEdge"], 1)
    ic.path("M8 43 Q8 40 11 40 H40 V45 H11 Q8 45 8 43 Z", P["paper"][0], P["paperEdge"], 0.8)
    ic.sheen(9, 4, 30, 14, r=2, k=0.4)
    ic.group(13, 12, 22, g_question, P["white"])


def magnifier(ic):
    ic.shadow(24, 44.5, 16, 2.4)
    ic.group(3, 3, 42, e_magnifier)


def gears(ic):
    ic.shadow(24, 44.5, 18, 2.4)
    ic.group(2, 4, 30, e_gear, P["gray"], P["grayEdge"])
    ic.group(22, 20, 24, e_gear, P["blue"], P["blueEdge"])


def clock(ic):
    ic.shadow(24, 45, 16, 2.4)
    ic.circle(24, 23, 20, ic.lin(P["blue"], 0, 0, 1, 1), P["blueEdge"], 1)
    ic.circle(24, 23, 16, ic.rad(["#FFFFFF", "#E6EBF4"], cx=0.4, cy=0.35, r=0.7), P["blueEdge"], 0.8)
    if not ic.small:
        for i in range(12):
            a = i * math.pi / 6
            ic.line(24 + 13 * math.cos(a), 23 + 13 * math.sin(a), 24 + 14.6 * math.cos(a), 23 + 14.6 * math.sin(a),
                    P["outline"], 1.4)
    ic.path("M24 12 V23 L32 28", "none", P["black"], 2)
    ic.circle(24, 23, 1.6, P["red"][1])


def users(ic):
    ic.shadow(24, 45, 20, 2.4)
    ic.group(2, 3, 44, e_people)


def user(ic):
    ic.shadow(24, 45, 16, 2.4)
    ic.circle(24, 14, 9, ic.lin(P["skin"]), P["brownEdge"], 0.8)
    ic.path("M7 44 Q7 26 24 26 Q41 26 41 44 Z", ic.lin(P["blue"]), P["blueEdge"], 1)


def trashcan(ic, full=False):
    ic.shadow(24, 45, 15, 2.4)
    ic.path("M10 12 H38 L35 44 H13 Z", ic.lin(["#D9F2EA", "#8CC9B6", "#4E9A84"], 0, 0, 1, 0), "#2F6B59", 1)
    if not ic.small:
        for i in range(5):
            x = 15 + i * 4.5
            ic.line(x, 16, x + (i - 2) * 0.4, 41, "#2F6B59", 0.8, extra=' stroke-opacity="0.6"')
    if full:
        ic.path("M12 12 Q14 4 20 7 Q23 2 28 6 Q34 3 36 12 Z", ic.lin(P["paper"], 0, 0, 1, 1), P["paperEdge"], 0.8)
        if not ic.small:
            ic.path("M17 8 L21 11 M27 6 L30 10", "none", P["line"], 0.8)
    ic.rect(8, 10, 32, 4, ic.lin(["#E6F7F1", "#7BBBA8"]), "#2F6B59", r=1.5)


def house(ic):
    ic.shadow(24, 44.5, 19, 2.4)
    ic.group(2, 1, 44, e_house)


def logo(ic):
    # The Meadow logo's four panes (same geometry as theme/assets/meadow-logo.svg).
    ic.shadow(24, 45, 16, 2.4)
    tile, gap, rr = 15.8, 2.4, 4.7
    colors = (("#FFE36A", "#F5C22B", "#C49B22", "#A3811C"), ("#6DB4F5", "#2E8BEA", "#256FBB", "#1F5D9C"),
              ("#A9DE73", "#7CC43E", "#639D32", "#53832A"), ("#83CF6E", "#4CAE3A", "#3D8B2E", "#337427"))
    ic.add('<g transform="translate(24 23) rotate(45)">')
    for (dx, dy), c in zip(((-1, -1), (1, -1), (-1, 1), (1, 1)), colors):
        x = -tile - gap / 2 if dx < 0 else gap / 2
        y = -tile - gap / 2 if dy < 0 else gap / 2
        ic.rect(x, y, tile, tile, ic.lin(list(c[:3]), 0, 0, 1, 1), c[3], r=rr, sw=0.6)
        ic.add(f'<rect x="{x}" y="{y}" width="{tile}" height="{tile}" rx="{rr}" '
               f'fill="{ic.lin([P["white"], P["white"]], 0, 0, 0.6, 0.6, opac=[0.5, 0])}"/>')
    ic.add("</g>")


def toolbox(ic):
    ic.shadow(24, 44, 20, 2.4)
    ic.path("M17 14 V9 Q17 7 19 7 H29 Q31 7 31 9 V14", "none", P["dark"][1], 2.4)
    ic.rect(4, 14, 40, 27, ic.lin(P["red"]), P["redEdge"], r=3)
    ic.rect(4, 22, 40, 3, P["redEdge"])
    ic.rect(21, 20, 6, 7, ic.lin(P["metal"]), P["metalEdge"], r=1)
    ic.sheen(5, 15, 38, 6, r=3)


def package(ic):
    ic.shadow(24, 45, 19, 2.4)
    ic.group(3, 2, 42, e_box)


def stopsign(ic, colors=P["red"], edge=P["redEdge"], glyph=None):
    ic.shadow(24, 45, 15, 2.2)
    pts = " L".join(f"{24 + 20 * math.cos(math.pi / 8 + i * math.pi / 4):.2f} {23 + 20 * math.sin(math.pi / 8 + i * math.pi / 4):.2f}"
                    for i in range(8))
    ic.path("M" + pts + " Z", ic.lin(colors, 0, 0, 1, 1), edge, 1)
    ic.ellipse(24, 14, 13, 7, ic.lin([P["white"], P["white"]], opac=[0.55, 0]))
    if glyph:
        ic.group(13, 12, 22, glyph, P["white"])


def warning_triangle(ic):
    ic.shadow(24, 45, 19, 2.2)
    ic.path("M24 3 Q26 3 27 5 L45 39 Q46 42 43 42 H5 Q2 42 3 39 L21 5 Q22 3 24 3 Z",
            ic.lin(P["yellow"]), P["yellowEdge"], 1)
    ic.group(15, 13, 18, g_exclaim, P["black"])


def round_badge_icon(colors, edge, glyph, gc=None):
    def draw(ic):
        ic.shadow(24, 45.5, 16, 2.2)
        badge(ic, 24, 23, 20, colors, edge, glyph, gc)
    return draw


def arrow_icon(direction):
    return round_badge_icon(P["green"], P["greenEdge"], lambda ic, c: g_arrow(ic, c, direction))


def network(ic, offline=False, wireless=None):
    ic.shadow(24, 45, 21, 2.4)
    monitor(ic, x=2, y=4, w=24, h=18, shadow=False)
    monitor(ic, x=20, y=18, w=24, h=18, shadow=False)
    if not ic.small:
        ic.path("M14 33 V38 H32", "none", P["dark"][1], 1.4)
    if offline:
        badge(ic, 12, 34, 8, P["red"], P["redEdge"], g_cross)


def wifi(ic, bars):
    ic.shadow(24, 45, 16, 2.2)
    for i in range(4):
        r = 8 + i * 9
        on = i < bars
        col = P["blue"][1] if on else P["gray"][1]
        ic.path(f"M{24 - r * 0.72:.2f} {41 - r * 0.7:.2f} A{r} {r} 0 0 1 {24 + r * 0.72:.2f} {41 - r * 0.7:.2f}",
                "none", col, 4.2 if not ic.small else 1.6)
    ic.circle(24, 41, 3.4, P["blue"][1] if bars > 0 else P["gray"][1])


def volume(waves, muted=False):
    def draw(ic):
        ic.shadow(24, 44, 16, 2.2)
        ic.group(2, 2, 44, e_speaker, waves, muted)
    return draw


def two_sheets(ic):
    sheet(ic, x=4, y=3, w=26, h=34, fold=8, shadow=False)
    sheet(ic, x=17, y=10, w=26, h=34, fold=8)


def server(ic):
    ic.shadow(24, 45, 16, 2.4)
    ic.rect(11, 3, 26, 41, ic.lin(P["dark"], 0, 0, 1, 0), P["black"], r=2)
    for i in range(4):
        ic.rect(14, 7 + i * 8, 20, 6, ic.lin(P["gray"]), P["grayEdge"], r=1, sw=0.6)
        ic.circle(30, 10 + i * 8, 1.1, P["green"][1])


# Icon table: context -> {name: drawing}.
ICONS = {
    "places": {
        "folder": lambda ic: folder(ic),
        "folder-open": lambda ic: folder(ic, opened=True),
        "folder-documents": emblem_on_folder(lambda ic: sheet(ic, 3, 1, 10, 13, 3, lines=False, shadow=False)),
        "folder-pictures": emblem_on_folder(e_picture),
        "folder-music": emblem_on_folder(e_note),
        "folder-videos": emblem_on_folder(e_film),
        "folder-download": emblem_on_folder(e_down),
        "folder-templates": emblem_on_folder(e_ruler),
        "folder-publicshare": emblem_on_folder(e_people),
        "folder-remote": emblem_on_folder(e_globe),
        "folder-saved-search": emblem_on_folder(e_magnifier),
        "user-desktop": lambda ic: monitor(ic),
        "user-home": house,
        "user-trash": lambda ic: trashcan(ic),
        "user-trash-full": lambda ic: trashcan(ic, True),
        "user-bookmarks": big(g_star),
        "network-workgroup": lambda ic: network(ic),
        "network-server": server,
        "start-here": logo,
    },
    "devices": {
        "computer": computer,
        "drive-harddisk": lambda ic: harddisk(ic),
        "drive-optical": lambda ic: harddisk(ic, slot=True),
        "drive-removable-media": usbstick,
        "media-optical": disc,
        "media-floppy": floppy,
        "media-flash": sdcard,
        "printer": printer,
        "scanner": scanner,
        "input-keyboard": keyboard,
        "input-mouse": mouse,
        "input-gaming": gamepad,
        "audio-card": audiocard,
        "audio-headphones": headphones,
        "audio-speakers": speakerbox,
        "camera-photo": camera,
        "camera-web": webcam,
        "phone": phone,
        "video-display": lambda ic: monitor(ic),
        "battery": lambda ic: battery(ic, 0.8),
        "multimedia-player": speakerbox,
    },
    "mimetypes": {
        "text-x-generic": lambda ic: sheet(ic),
        "text-html": emblem_on_sheet(e_globe, lines=False),
        "text-x-script": emblem_on_sheet(e_brackets),
        "application-x-executable": lambda ic: window(ic),
        "application-x-addon": emblem_on_sheet(e_gear, lines=False),
        "image-x-generic": emblem_on_sheet(e_picture, lines=False, size=22),
        "audio-x-generic": emblem_on_sheet(e_note, lines=False),
        "video-x-generic": emblem_on_sheet(e_film, lines=False),
        "package-x-generic": package,
        "application-pdf": lambda ic: (sheet(ic), ic.rect(9, 33, 30, 8, ic.lin(P["red"]), P["redEdge"], sw=0.6)),
        "x-office-document": emblem_on_sheet(None),
        "x-office-spreadsheet": emblem_on_sheet(e_grid, lines=False, size=22),
        "x-office-presentation": emblem_on_sheet(e_chart, lines=False),
        "x-office-drawing": emblem_on_sheet(e_palette, lines=False),
        "font-x-generic": emblem_on_sheet(e_letter_a, lines=False),
        "application-x-cd-image": emblem_on_sheet(e_disc, lines=False),
        "application-x-firmware": emblem_on_sheet(e_box, lines=False),
        "inode-directory": lambda ic: folder(ic),
        "unknown": emblem_on_sheet(lambda ic: g_question(ic, P["blue"][2]), lines=False),
    },
    "apps": {
        "internet-web-browser": globe,
        "internet-mail": envelope,
        "system-file-manager": corner_emblem(lambda ic: folder(ic), e_magnifier),
        "utilities-terminal": terminal,
        "accessories-text-editor": notepad,
        "accessories-calculator": calculator,
        "accessories-character-map": big(e_letter_a, 8),
        "help-browser": helpbook,
        "system-search": magnifier,
        "system-run": corner_badge(lambda ic: window(ic), P["green"], P["greenEdge"], g_play),
        "preferences-system": gears,
        "preferences-desktop-display": lambda ic: monitor(ic),
        "preferences-desktop-theme": corner_emblem(lambda ic: monitor(ic), e_palette),
        "preferences-desktop-peripherals": lambda ic: (keyboard(ic), ic.group(26, 6, 20, lambda j: mouse_small(j))),
        "preferences-desktop-default-applications": corner_badge(lambda ic: window(ic), P["green"], P["greenEdge"], g_check),
        "preferences-desktop-keyboard": keyboard,
        "preferences-desktop-locale": globe,
        "preferences-desktop-accessibility": round_badge_icon(P["blue"], P["blueEdge"], lambda ic, c: accessibility_glyph(ic, c)),
        "preferences-system-time": clock,
        "preferences-system-network": lambda ic: network(ic),
        "preferences-desktop-wallpaper": emblem_on_sheet(e_picture, lines=False, size=22),
        "multimedia-volume-control": volume(3),
        "system-users": users,
        "utilities-system-monitor": corner_emblem(lambda ic: monitor(ic), e_chart),
        "system-software-install": package,
        "preferences-desktop-screensaver": lambda ic: (monitor(ic), ic.group(14, 10, 20, lambda j: logo_glyph(j))),
    },
    "categories": {
        "applications-accessories": big(e_pencil),
        "applications-development": big(e_brackets),
        "applications-engineering": big(e_ruler),
        "applications-games": big(e_die),
        "applications-graphics": big(e_palette),
        "applications-internet": globe,
        "applications-multimedia": lambda ic: (ic.shadow(), ic.group(4, 4, 26, e_film), ic.group(20, 18, 26, e_note)),
        "applications-office": emblem_on_sheet(e_chart, lines=False),
        "applications-other": lambda ic: folder(ic),
        "applications-science": big(e_flask),
        "applications-system": gears,
        "applications-utilities": toolbox,
        "preferences-desktop": gears,
    },
    "actions": {
        "edit-copy": two_sheets,
        "edit-cut": big(e_scissors),
        "edit-paste": big(e_clipboard),
        "edit-delete": round_badge_icon(P["red"], P["redEdge"], g_cross),
        "edit-undo": big(e_undo),
        "edit-redo": big(lambda ic: e_undo(ic, True)),
        "edit-find": magnifier,
        "edit-clear": round_badge_icon(P["gray"], P["grayEdge"], g_cross, P["dark"][1]),
        "edit-select-all": lambda ic: (sheet(ic), ic.rect(13, 12, 22, 26, "none", P["blue"][2], sw=1.2,
                                                         extra=' stroke-dasharray="2 2"')),
        "document-new": corner_emblem(lambda ic: sheet(ic, x=6, w=28), g_star, 26, 2, 20),
        "document-open": lambda ic: folder(ic, opened=True),
        "document-save": floppy,
        "document-save-as": corner_emblem(floppy, e_pencil),
        "document-print": printer,
        "document-properties": corner_badge(lambda ic: sheet(ic), P["blue"], P["blueEdge"], g_info),
        "document-revert": corner_emblem(lambda ic: sheet(ic), e_undo),
        "folder-new": corner_emblem(lambda ic: folder(ic), g_star, 26, 2, 20),
        "go-previous": arrow_icon("left"),
        "go-next": arrow_icon("right"),
        "go-up": arrow_icon("up"),
        "go-down": arrow_icon("down"),
        "go-home": house,
        "view-refresh": round_badge_icon(P["green"], P["greenEdge"], g_refresh),
        "list-add": round_badge_icon(P["green"], P["greenEdge"], g_plus),
        "list-remove": round_badge_icon(P["red"], P["redEdge"], g_minus),
        "window-close": round_badge_icon(P["red"], P["redEdge"], g_cross),
        "process-stop": lambda ic: stopsign(ic, glyph=g_cross),
        "help-about": round_badge_icon(P["blue"], P["blueEdge"], g_info),
        "help-contents": helpbook,
        "mail-send": corner_badge(envelope, P["green"], P["greenEdge"], lambda ic, c: g_arrow(ic, c, "right")),
        "mail-message-new": corner_emblem(envelope, g_star, 28, 22, 18),
        "zoom-in": corner_badge(magnifier, P["green"], P["greenEdge"], g_plus, 16, 16, 7),
        "zoom-out": corner_badge(magnifier, P["red"], P["redEdge"], g_minus, 16, 16, 7),
        "media-playback-start": round_badge_icon(P["green"], P["greenEdge"], g_play),
        "media-playback-stop": round_badge_icon(P["blue"], P["blueEdge"], g_stop),
        "system-log-out": big(e_door),
        "system-shutdown": round_badge_icon(P["red"], P["redEdge"], g_power),
        "system-reboot": round_badge_icon(P["green"], P["greenEdge"], g_refresh),
        "system-lock-screen": big(e_lock),
        "view-list-icons": lambda ic: [ic.rect(6 + (i % 2) * 20, 6 + (i // 2) * 20, 16, 16, ic.lin(P["blue"]),
                                               P["blueEdge"], r=2) for i in range(4)],
        "view-list-details": lambda ic: [(ic.rect(5, 7 + i * 9, 7, 6, ic.lin(P["blue"]), P["blueEdge"], r=1),
                                          ic.rect(15, 8.5 + i * 9, 28, 3, P["dark"][0])) for i in range(4)],
        "view-fullscreen": lambda ic: (monitor(ic), ic.path("M12 12 L18 12 M12 12 V18 M36 12 H30 M36 12 V18",
                                                             "none", P["white"], 2)),
    },
    "status": {
        "dialog-information": round_badge_icon(P["blue"], P["blueEdge"], g_info),
        "dialog-warning": warning_triangle,
        "dialog-error": round_badge_icon(P["red"], P["redEdge"], g_cross),
        "dialog-question": round_badge_icon(P["blue"], P["blueEdge"], g_question),
        "dialog-password": big(e_key),
        "security-high": big(e_lock),
        "audio-volume-high": volume(3),
        "audio-volume-medium": volume(2),
        "audio-volume-low": volume(1),
        "audio-volume-muted": volume(0, True),
        "network-wired": lambda ic: network(ic),
        "network-offline": lambda ic: network(ic, offline=True),
        "network-wireless-signal-excellent": lambda ic: wifi(ic, 4),
        "network-wireless-signal-good": lambda ic: wifi(ic, 3),
        "network-wireless-signal-ok": lambda ic: wifi(ic, 2),
        "network-wireless-signal-weak": lambda ic: wifi(ic, 1),
        "network-wireless-signal-none": lambda ic: wifi(ic, 0),
        "battery-full": lambda ic: battery(ic, 1.0),
        "battery-good": lambda ic: battery(ic, 0.7),
        "battery-low": lambda ic: battery(ic, 0.3),
        "battery-caution": lambda ic: battery(ic, 0.12),
        "battery-empty": lambda ic: battery(ic, 0.0),
        "battery-full-charging": lambda ic: battery(ic, 1.0, True),
        "battery-good-charging": lambda ic: battery(ic, 0.7, True),
        "battery-low-charging": lambda ic: battery(ic, 0.3, True),
        "battery-caution-charging": lambda ic: battery(ic, 0.12, True),
        "battery-empty-charging": lambda ic: battery(ic, 0.0, True),
        "battery-full-charged": lambda ic: battery(ic, 1.0, True),
        "battery-missing": lambda ic: (battery(ic, 0.0), badge(ic, 36, 34, 8, P["red"], P["redEdge"], g_cross)),
        "mail-unread": envelope,
        "user-available": round_badge_icon(P["green"], P["greenEdge"], None),
        "user-offline": round_badge_icon(P["gray"], P["grayEdge"], None),
    },
}


def mouse_small(ic):
    ic.path("M4 5 Q4 1 8 1 Q12 1 12 5 V10 Q12 15 8 15 Q4 15 4 10 Z", ic.lin(P["bezel"], 0, 0, 1, 1), P["bezelEdge"], 0.5)
    ic.path("M4 6 H12 M8 1 V6", "none", P["bezelEdge"], 0.5)


def accessibility_glyph(ic, c):
    ic.circle(8, 2.8, 1.6, c)
    ic.path("M2.5 5.2 H13.5 V6.8 H9.4 L10.6 14.5 H8.9 L8 10 L7.1 14.5 H5.4 L6.6 6.8 H2.5 Z", c)


def logo_glyph(ic):
    for (x, y), c in zip(((5, 1), (9.6, 5.6), (0.4, 5.6), (5, 10.2)), ("#F5C22B", "#2E8BEA", "#7CC43E", "#4CAE3A")):
        ic.rect(x, y, 5.4, 5.4, c, r=1.4, extra=' transform="rotate(45 8 8)"')


# Aliases: name -> (context, target name). Same context unless noted.
ALIASES = {
    "places": {"folder-image": "folder-pictures", "folder-video": "folder-videos", "folder-downloads": "folder-download",
               "user-trash-empty": "user-trash",
               "emptytrash": "user-trash", "trashcan_full": "user-trash-full", "network": "network-workgroup",
               "folder-home": "user-home", "desktop": "user-desktop", "distributor-logo": "start-here"},
    "devices": {"drive-removable-media-usb": "drive-removable-media", "drive-harddisk-system": "drive-harddisk",
                "media-removable": "drive-removable-media", "computer-laptop": "computer",
                "input-tablet": "input-mouse", "multimedia-player-portable": "multimedia-player",
                "display": "video-display", "printers": "printer", "keyboard": "input-keyboard"},
    "mimetypes": {"text-plain": "text-x-generic", "text-x-readme": "text-x-generic", "application-x-shellscript":
                  "text-x-script", "text-x-python": "text-x-script", "application-zip": "package-x-generic",
                  "application-x-compressed-tar": "package-x-generic", "application-x-rpm": "package-x-generic",
                  "image-png": "image-x-generic", "image-jpeg": "image-x-generic", "image-svg+xml": "image-x-generic",
                  "audio-mpeg": "audio-x-generic", "audio-x-vorbis+ogg": "audio-x-generic", "video-mp4": "video-x-generic",
                  "application-vnd.oasis.opendocument.text": "x-office-document",
                  "application-vnd.oasis.opendocument.spreadsheet": "x-office-spreadsheet",
                  "application-vnd.oasis.opendocument.presentation": "x-office-presentation",
                  "application-x-iso9660-image": "application-x-cd-image", "text-x-generic-template": "text-x-generic",
                  "application-octet-stream": "unknown"},
    "apps": {
        # Default applications Meadow's Start menu and Quick Launch use.
        "pcmanfm-qt": "system-file-manager", "pcmanfm": "system-file-manager", "org.gnome.Nautilus": "system-file-manager",
        "qterminal": "utilities-terminal", "org.gnome.Terminal": "utilities-terminal", "org.gnome.Ptyxis": "utilities-terminal",
        "org.kde.konsole": "utilities-terminal", "xterm": "utilities-terminal",
        "org.gnome.TextEditor": "accessories-text-editor", "org.kde.kwrite": "accessories-text-editor",
        "featherpad": "accessories-text-editor", "gnome-calculator": "accessories-calculator",
        "org.gnome.Calculator": "accessories-calculator", "org.kde.kcalc": "accessories-calculator",
        "firefox": "internet-web-browser", "org.mozilla.firefox": "internet-web-browser",
        "web-browser": "internet-web-browser", "mail-client": "internet-mail", "thunderbird": "internet-mail",
        "org.mozilla.Thunderbird": "internet-mail", "utilities-file-manager": "system-file-manager",
        "preferences-desktop-personal": "system-users", "preferences-desktop-user": "system-users",
        "user-info": "system-users", "preferences-desktop-sound": "multimedia-volume-control",
        "preferences-system-sound": "multimedia-volume-control", "preferences-desktop-mouse": "preferences-desktop-peripherals",
        "preferences-system-power": "preferences-system", "gnome-control-center": "preferences-system",
        "systemsettings": "preferences-system", "meadow-control-panel": "preferences-system",
        "org.meadow.ControlPanel": "preferences-system", "org.meadow.DisplayProperties": "preferences-desktop-display",
        "kvantummanager": "preferences-desktop-theme", "help-faq": "help-browser", "yelp": "help-browser",
    },
    "categories": {"preferences-other": "applications-other"},
    "actions": {"document-open-recent": "document-open", "edit-find-replace": "edit-find", "go-first": "go-previous",
                "go-last": "go-next", "go-top": "go-up", "go-bottom": "go-down", "go-jump": "go-next",
                "process-working": "view-refresh", "application-exit": "system-log-out", "window-new": "document-new",
                "list-remove-all": "edit-clear", "edit-delete-shred": "edit-delete", "document-export": "document-save-as",
                "document-import": "document-open", "mail-reply-sender": "mail-send", "system-suspend": "system-shutdown",
                "system-hibernate": "system-shutdown", "system-switch-user": "system-log-out",
                "media-playback-pause": "media-playback-stop"},
    "status": {"dialog-warning-panel": "dialog-warning", "network-idle": "network-wired",
               "network-transmit-receive": "network-wired", "network-error": "network-offline",
               "network-wired-disconnected": "network-offline", "network-wireless-offline": "network-wireless-signal-none",
               "battery-level-100": "battery-full", "battery-level-70": "battery-good", "battery-level-30": "battery-low",
               "battery-level-10": "battery-caution", "battery-level-0": "battery-empty",
               "audio-volume-off": "audio-volume-muted", "security-medium": "security-high",
               "dialog-ok": "user-available", "emblem-important": "dialog-warning"},
}


def all_icons():
    out = []
    for ctx, icons in ICONS.items():
        for name, fn in icons.items():
            out.append((ctx, name, fn))
    return out


def draw(ctx, name, fn, small):
    uid = "s" if small else "l"
    ic = Icon(small, uid)
    fn(ic)
    return ic.svg(16 if small else 48)


def index_theme():
    dirs = []
    for size in ("16x16", "scalable"):
        for ctx in CONTEXTS:
            dirs.append(f"{size}/{ctx}")
    lines = ["[Icon Theme]", "Name=Meadow Icons",
             "Comment=Original glossy icons for the Meadow desktop (generated by tools/gen-icons.py)",
             "Inherits=Adwaita,hicolor", "Example=folder", "Directories=" + ",".join(dirs), ""]
    for d in dirs:
        size, ctx = d.split("/")
        lines.append(f"[{d}]")
        if size == "16x16":
            lines += ["Size=16", "Type=Fixed"]
        else:
            lines += ["Size=48", "MinSize=17", "MaxSize=512", "Type=Scalable"]
        lines += [f"Context={CONTEXTS[ctx]}", ""]
    return "\n".join(lines)


def generate(out):
    out.mkdir(parents=True, exist_ok=True)
    (out / "index.theme").write_text(index_theme())
    for size in ("16x16", "scalable"):
        for ctx in CONTEXTS:
            (out / size / ctx).mkdir(parents=True, exist_ok=True)
    for ctx, name, fn in all_icons():
        for size, small in (("16x16", True), ("scalable", False)):
            (out / size / ctx / f"{name}.svg").write_text(draw(ctx, name, fn, small))
    names = {(c, n) for c, n, _ in all_icons()}
    by_name = {n: c for c, n, _ in all_icons()}
    for ctx, aliases in ALIASES.items():
        for alias, target in aliases.items():
            if (ctx, alias) in names:
                continue
            tctx = ctx if (ctx, target) in names else by_name[target]
            for size in ("16x16", "scalable"):
                link = out / size / ctx / f"{alias}.svg"
                rel = f"{target}.svg" if tctx == ctx else f"../{tctx}/{target}.svg"
                if link.is_symlink() or link.exists():
                    link.unlink()
                os.symlink(rel, link)


def compare(a, b):
    """Differences between two theme trees, symlinks compared by target."""
    diffs = []
    files_a = {p.relative_to(a) for p in a.rglob("*") if not p.is_dir() or p.is_symlink()}
    files_b = {p.relative_to(b) for p in b.rglob("*") if not p.is_dir() or p.is_symlink()}
    for rel in sorted(files_a ^ files_b):
        diffs.append(f"only in {'committed' if rel in files_a else 'generated'}: {rel}")
    for rel in sorted(files_a & files_b):
        pa, pb = a / rel, b / rel
        if pa.is_symlink() or pb.is_symlink():
            if not (pa.is_symlink() and pb.is_symlink() and os.readlink(pa) == os.readlink(pb)):
                diffs.append(f"symlink differs: {rel}")
        elif not filecmp.cmp(pa, pb, shallow=False):
            diffs.append(f"differs: {rel}")
    return diffs


def contact_sheet(small=False, columns=12):
    """Every icon with its name, for review and the icons golden."""
    icons = all_icons()
    cell, label = (32, 0) if small else (96, 14)
    size = 16 if small else 48
    rows = (len(icons) + columns - 1) // columns
    w, h = columns * cell, rows * (cell + label)
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">',
             f'<rect width="{w}" height="{h}" fill="#ECE9D8"/>']
    for k, (ctx, name, fn) in enumerate(icons):
        ic = Icon(small, f"i{k}_")
        fn(ic)
        x = (k % columns) * cell + (cell - size) / 2
        y = (k // columns) * (cell + label) + (cell - size) / 2
        parts.append(f'<svg x="{x}" y="{y}" width="{size}" height="{size}" viewBox="0 0 48 48">'
                     f'<defs>{"".join(ic.defs)}</defs>{"".join(ic.body)}</svg>')
        if label:
            parts.append(f'<text x="{(k % columns) * cell + cell / 2}" y="{(k // columns) * (cell + label) + cell + 8}" '
                         f'font-family="DejaVu Sans" font-size="7" text-anchor="middle" fill="#333">{name[:20]}</text>')
    parts.append("</svg>\n")
    return "".join(parts)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--output", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--check", nargs="?", const=DEFAULT_OUT, type=Path, metavar="DIR")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--contact-sheet", type=Path, metavar="OUT.svg")
    ap.add_argument("--small", action="store_true", help="with --contact-sheet: the 16 px drawings, unlabelled")
    args = ap.parse_args()
    if args.contact_sheet:
        args.contact_sheet.write_text(contact_sheet(args.small))
        return 0
    if args.list:
        for ctx, name, _ in all_icons():
            print(ctx, name)
        for ctx, aliases in ALIASES.items():
            for alias, target in aliases.items():
                if target:
                    print(ctx, alias, "->", target)
        return 0
    if args.check:
        with tempfile.TemporaryDirectory() as tmp:
            generate(Path(tmp))
            diffs = compare(args.check, Path(tmp))
        if diffs:
            print("\n".join(diffs[:40]))
            print(f"{args.check} is out of date; run tools/gen-icons.py")
            return 1
        print(f"icons: {len(all_icons())} icons up to date")
        return 0
    if args.output.exists():
        for size in ("16x16", "scalable"):
            shutil.rmtree(args.output / size, ignore_errors=True)
    generate(args.output)
    print(f"wrote {len(all_icons())} icons to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
