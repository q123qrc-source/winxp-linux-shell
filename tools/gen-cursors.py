#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Draw the original "meadow-cursors" XCursor theme as SVG sources (phase 9).

Classic shapes in Meadow's style (white body, black outline, a soft shadow),
designed on a 32-unit grid. Colours and frame timing come from theme.json
(toolkit.cursors). The SVGs and cursors.txt are committed; the build turns
them into XCursor files with tools/gen-cursors.sh (rsvg-convert + xcursorgen),
and the cursors_sync ctest fails when the committed sources are stale.

  gen-cursors.py [--output DIR]     write DIR/*.svg and DIR/cursors.txt
  gen-cursors.py --check [DIR]      compare instead of writing
  gen-cursors.py --sheet OUT.svg    every cursor (first frame) side by side

cursors.txt: one cursor per line, "name hot_x hot_y frames alias...", hot
spot in grid units (0..32). Animated cursors have frames name-01.svg...
"""
import argparse
import filecmp
import json
import math
import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUT = ROOT / "toolkit-themes" / "cursors" / "src"
GRID = 32


def tokens():
    t = json.loads((ROOT / "theme" / "luna-blue" / "theme.json").read_text())
    return t["toolkit"]["cursors"]


T = tokens()


def argb(c):
    h = c.lstrip("#")
    if len(h) == 8:
        return f"#{h[2:]}", round(int(h[:2], 16) / 255, 3)
    return f"#{h}", 1.0


FILL, _ = argb(T["fillColor"])
INK, _ = argb(T["outlineColor"])
SHADOW, SHADOW_A = argb(T["shadowColor"])
ACCENT, _ = argb(T["accentColor"])
SAND, _ = argb(T["sandColor"])
GLASS, _ = argb(T["glassColor"])
SW = 1.25  # outline width in grid units


def doc(body):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{GRID}" height="{GRID}" viewBox="0 0 {GRID} {GRID}">'
            f"{body}</svg>\n")


def shaped(d, fill=FILL, transform=""):
    """A shape with its shadow (offset down-right) and outline."""
    t = f' transform="{transform}"' if transform else ""
    return (f'<g{t}><path d="{d}" transform="translate(1.4 1.6)" fill="{SHADOW}" fill-opacity="{SHADOW_A}"/>'
            f'<path d="{d}" fill="{fill}" stroke="{INK}" stroke-width="{SW}" stroke-linejoin="round"/></g>')


ARROW = "M3 2 V24.5 L8.4 19.4 L12.2 28 L15.8 26.4 L12.1 18 H19.6 Z"


def arrow():
    return shaped(ARROW)


def hourglass(cx, cy, s, phase, flip=0.0):
    """An hourglass centred at (cx, cy), s units tall; phase 0..1 of sand run."""
    w, h = s * 0.62, s
    top, bot = cy - h / 2, cy + h / 2
    cap = s * 0.12
    g = []
    g.append(f'<g transform="rotate({flip * 180:.1f} {cx} {cy})">')
    glass = (f"M{cx - w / 2 + 1} {top + cap} H{cx + w / 2 - 1} Q{cx + w / 2 - 1} {cy - h * 0.12} {cx + 1} {cy} "
             f"Q{cx + w / 2 - 1} {cy + h * 0.12} {cx + w / 2 - 1} {bot - cap} H{cx - w / 2 + 1} "
             f"Q{cx - w / 2 + 1} {cy + h * 0.12} {cx - 1} {cy} Q{cx - w / 2 + 1} {cy - h * 0.12} {cx - w / 2 + 1} {top + cap} Z")
    g.append(f'<path d="{glass}" transform="translate(1.2 1.4)" fill="{SHADOW}" fill-opacity="{SHADOW_A}"/>')
    g.append(f'<path d="{glass}" fill="{GLASS}" stroke="{INK}" stroke-width="{SW * 0.8}" stroke-linejoin="round"/>')
    # Sand: the top bulb empties as the bottom one fills.
    left = 1 - phase
    if left > 0.02:
        ty = cy - (h / 2 - cap) * left * 0.85
        tw = (w / 2 - 1.6) * (0.35 + 0.65 * left)
        g.append(f'<path d="M{cx - tw} {ty} H{cx + tw} L{cx} {cy - 0.6} Z" fill="{SAND}"/>')
    if phase > 0.02:
        by = bot - cap - (h / 2 - cap) * phase * 0.7
        g.append(f'<path d="M{cx} {by} L{cx + w / 2 - 1.8} {bot - cap - 0.4} H{cx - w / 2 + 1.8} Z" fill="{SAND}"/>')
    if 0.02 < phase < 0.98:
        g.append(f'<rect x="{cx - 0.45}" y="{cy - 0.6}" width="0.9" height="{h * 0.36}" fill="{SAND}"/>')
    for y in (top, bot - cap):
        g.append(f'<rect x="{cx - w / 2}" y="{y}" width="{w}" height="{cap}" rx="0.8" fill="{ACCENT}" '
                 f'stroke="{INK}" stroke-width="{SW * 0.7}"/>')
    g.append("</g>")
    return "".join(g)


def frames_hourglass(n, cx, cy, s, with_arrow=False):
    out = []
    run = n - 3  # the last frames turn the glass over
    for i in range(n):
        if i < run:
            phase, flip = i / (run - 1), 0.0
        else:
            phase, flip = 1.0, (i - run + 1) / 3
        body = (arrow() if with_arrow else "") + hourglass(cx, cy, s, phase if flip < 0.5 else 0.0, flip)
        out.append(doc(body))
    return out


def double_arrow(angle, bar=False):
    """A two-headed arrow through the centre, rotated by angle degrees."""
    d = ("M16 3 L22 9.5 H18.2 V22.5 H22 L16 29 L10 22.5 H13.8 V9.5 H10 Z")
    body = shaped(d, transform=f"rotate({angle} 16 16)")
    if bar:
        # A splitter bar across the arrow (col-resize, row-resize).
        body += shaped("M7 14.8 H25 V17.2 H7 Z", INK, transform=f"rotate({angle} 16 16)")
    return doc(body)


def ibeam():
    d = ("M10.5 3.5 H14.5 Q16 3.5 16 5 Q16 3.5 17.5 3.5 H21.5 V6 H17.6 V26 H21.5 V28.5 H17.5 Q16 28.5 16 27 "
         "Q16 28.5 14.5 28.5 H10.5 V26 H14.4 V6 H10.5 Z")
    return doc(shaped(d))


def hand(closed=False, pointing=True):
    if pointing:
        d = ("M11 3.5 Q11 2 12.6 2 Q14.2 2 14.2 3.5 V13 Q14.6 11.6 16 11.6 Q17.4 11.6 17.6 13.2 Q18 12 19.4 12 "
             "Q20.8 12.2 21 13.8 Q21.4 12.8 22.6 12.9 Q24 13.1 24 15 V21 Q24 26.5 20.5 29 H13.2 Q11.6 27.2 9.6 24.4 "
             "L6.2 19.6 Q5.4 18.2 6.6 17.4 Q7.8 16.7 9 17.8 L11 20 Z")
    elif closed:
        d = ("M9.5 13 Q9.5 11 11.4 11 Q12.6 11 13 12.2 Q13.4 10.6 15 10.6 Q16.6 10.6 17 12 Q17.4 10.8 18.8 10.8 "
             "Q20.4 10.8 20.6 12.6 Q21 11.8 22.2 11.9 Q24 12.1 24 14 V21 Q24 26.5 20.5 29 H13.4 Q9.5 26 9.5 21 Z")
    else:
        d = ("M8.5 9 Q8.5 7.4 10 7.4 Q11.6 7.4 11.6 9 V15 V6 Q11.6 4.4 13.2 4.4 Q14.8 4.4 14.8 6 V14.5 V5 "
             "Q14.8 3.4 16.4 3.4 Q18 3.4 18 5 V14.5 V6.6 Q18 5 19.6 5 Q21.2 5 21.2 6.6 V16 Q22 14 23.6 14.4 "
             "Q25 15 24.4 17 L22.4 23 Q21 28 17.5 29 H13.4 Q9.5 27 8.5 22 Z")
    return doc(shaped(d))


def badge(kind):
    """A small sign at the lower right of the arrow (copy, alias, help, menu)."""
    g = [f'<circle cx="23.5" cy="24" r="6.4" fill="{FILL}" stroke="{INK}" stroke-width="{SW}"/>']
    if kind == "copy":
        g.append(f'<path d="M23.5 20.4 V27.6 M19.9 24 H27.1" stroke="{ACCENT}" stroke-width="2.2"/>')
    elif kind == "alias":
        g.append(f'<path d="M20.6 26.8 Q20.6 21.6 25.6 21.6 M23.6 19.8 L25.8 21.6 L23.6 23.4" fill="none" '
                 f'stroke="{ACCENT}" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>')
    elif kind == "help":
        g.append(f'<path d="M21.3 22.4 Q21.4 20.2 23.6 20.2 Q25.8 20.3 25.8 22.1 Q25.8 23.3 24.4 24 Q23.9 24.3 23.9 25.1 '
                 f'V25.5" fill="none" stroke="{ACCENT}" stroke-width="1.5" stroke-linecap="round"/>'
                 f'<circle cx="23.9" cy="27.4" r="0.95" fill="{ACCENT}"/>')
    elif kind == "menu":
        g = [f'<rect x="17" y="18" width="12" height="12" fill="{FILL}" stroke="{INK}" stroke-width="{SW}"/>']
        for i in range(3):
            g.append(f'<rect x="19" y="{20.4 + i * 3}" width="8" height="1.3" fill="{ACCENT}"/>')
    return "".join(g)


def crosshair():
    d = "M15 3 H17 V15 H29 V17 H17 V29 H15 V17 H3 V15 H15 Z"
    return doc(f'<path d="{d}" fill="{INK}" stroke="{FILL}" stroke-width="0.9" paint-order="stroke"/>')


def cell():
    d = "M13 4 H19 V13 H28 V19 H19 V28 H13 V19 H4 V13 H13 Z"
    return doc(shaped(d))


def not_allowed():
    return doc(f'<circle cx="17" cy="17.5" r="10" fill="none" stroke="{SHADOW}" stroke-opacity="{SHADOW_A}" stroke-width="4"/>'
               f'<circle cx="16" cy="16" r="10" fill="none" stroke="{INK}" stroke-width="5"/>'
               f'<circle cx="16" cy="16" r="10" fill="none" stroke="#E04B2A" stroke-width="3"/>'
               f'<path d="M9 9 L23 23" stroke="{INK}" stroke-width="5"/><path d="M9 9 L23 23" stroke="#E04B2A" stroke-width="3"/>')


def move():
    d = ("M16 1.5 L21 7 H17.6 V14.4 H25 V11 L30.5 16 L25 21 V17.6 H17.6 V25 H21 L16 30.5 L11 25 H14.4 V17.6 "
         "H7 V21 L1.5 16 L7 11 V14.4 H14.4 V7 H11 Z")
    return doc(shaped(d))


def pencil():
    d = "M3 29 L4.6 22 L21 5.6 Q22.6 4 24.2 5.6 L26.4 7.8 Q28 9.4 26.4 11 L10 27.4 Z"
    return doc(shaped(d, SAND) + f'<path d="M3 29 L4.6 22 L10 27.4 Z" fill="{FILL}" stroke="{INK}" stroke-width="{SW}"/>')


def up_arrow():
    d = "M16 2 L25 13 H19 V29 H13 V13 H7 Z"
    return doc(shaped(d))


def zoom(sign):
    g = (f'<path d="M19 19 L28 28" stroke="{INK}" stroke-width="4" stroke-linecap="round"/>'
         + shaped("M12.5 3 A9.5 9.5 0 1 1 12.49 3 Z")
         + f'<path d="M8.5 12.5 H16.5{" M12.5 8.5 V16.5" if sign == "+" else ""}" stroke="{ACCENT}" stroke-width="2.2"/>')
    return doc(g)


def cursors():
    """name -> (frames [svg...], hotspot (x, y), aliases)."""
    n = T["frames"]
    table = {
        "default": ([doc(arrow())], (3, 2), ["left_ptr", "arrow", "top_left_arrow", "X_cursor", "right_ptr",
                                             "draft_large", "draft_small", "wayland-cursor"]),
        "text": ([ibeam()], (16, 16), ["xterm", "ibeam"]),
        "vertical-text": ([doc(shaped("M3.5 10.5 V14.5 Q3.5 16 5 16 Q3.5 16 3.5 17.5 V21.5 H6 V17.6 H26 V21.5 H28.5 "
                                      "V17.5 Q28.5 16 27 16 Q28.5 16 28.5 14.5 V10.5 H26 V14.4 H6 V10.5 Z"))], (16, 16), []),
        "pointer": ([hand()], (12.6, 2.5), ["hand2", "hand1", "hand", "pointing_hand",
                                             "9d800788f1b08800ae810202380a0822", "e29285e634086352946a0e7090d73106"]),
        "grab": ([hand(pointing=False)], (16, 14), ["openhand", "fleur-open", "5aca4d189052212118709018842178c0"]),
        "grabbing": ([hand(closed=True, pointing=False)], (16, 14), ["closedhand", "dnd-move", "dnd-none",
                                                                    "208530c400c041818281048008011002"]),
        "help": ([doc(arrow() + badge("help"))], (3, 2), ["question_arrow", "whats_this", "left_ptr_help",
                                                          "d9ce0ab605698f320427677b458ad60b",
                                                          "5c6cd98b3f3ebcb1f9c7f1c204630408"]),
        "copy": ([doc(arrow() + badge("copy"))], (3, 2), ["dnd-copy", "1081e37283d90000800003c07f3ef6bf",
                                                          "6407b0e94181790501fd1e167b474872"]),
        "alias": ([doc(arrow() + badge("alias"))], (3, 2), ["link", "dnd-link", "640fb0e74195791501fd1ed57b41487f",
                                                            "3085a0e285430894940527032f8b26df"]),
        "context-menu": ([doc(arrow() + badge("menu"))], (3, 2), []),
        "wait": (frames_hourglass(n, 16, 16, 26), (16, 16), ["watch"]),
        "progress": (frames_hourglass(n, 23, 23, 15, with_arrow=True), (3, 2),
                     ["left_ptr_watch", "half-busy", "3ecb610c1bf2410f44200f48c40d3599",
                      "08e8e1c95fe2fc01f976f1e063a24ccd"]),
        "crosshair": ([crosshair()], (16, 16), ["cross", "tcross", "cross_reverse", "diamond_cross"]),
        "cell": ([cell()], (16, 16), ["plus"]),
        "not-allowed": ([not_allowed()], (16, 16), ["no-drop", "forbidden", "crossed_circle", "circle",
                                                    "03b6e0fcb3499374a867c041f52298f0"]),
        "move": ([move()], (16, 16), ["all-scroll", "fleur", "size_all", "4498f0e0c1937ffe01fd06f973665830",
                                      "9081237383d90e509aa00f00170e968f"]),
        "ns-resize": ([double_arrow(0)], (16, 16), ["n-resize", "s-resize", "sb_v_double_arrow", "v_double_arrow",
                                                    "size_ver", "top_side", "bottom_side", "double_arrow",
                                                    "00008160000006810000408080010102"]),
        "ew-resize": ([double_arrow(90)], (16, 16), ["e-resize", "w-resize", "sb_h_double_arrow", "h_double_arrow",
                                                     "size_hor", "left_side", "right_side",
                                                     "028006030e0e7ebffc7f7070c0600140"]),
        "nesw-resize": ([double_arrow(45)], (16, 16), ["ne-resize", "sw-resize", "fd_double_arrow", "size_bdiag",
                                                       "top_right_corner", "bottom_left_corner",
                                                       "fcf1c3c7cd4491d801f1e1c78f100000"]),
        "nwse-resize": ([double_arrow(-45)], (16, 16), ["nw-resize", "se-resize", "bd_double_arrow", "size_fdiag",
                                                        "top_left_corner", "bottom_right_corner",
                                                        "c7088f0f3e6c8088236ef8e1e3e70000"]),
        "col-resize": ([double_arrow(90, bar=True)], (16, 16), ["split_h", "sb_h_double_arrow_bar",
                                                                 "14fef782d02440884392942c11205230"]),
        "row-resize": ([double_arrow(0, bar=True)], (16, 16), ["split_v", "2870a09082c103050810ffdffffe0204"]),
        "pencil": ([pencil()], (3, 29), ["draft"]),
        "up-arrow": ([up_arrow()], (16, 2), ["up_arrow", "center_ptr", "sb_up_arrow"]),
        "zoom-in": ([zoom("+")], (12.5, 12.5), []),
        "zoom-out": ([zoom("-")], (12.5, 12.5), []),
    }
    return table


def frame_name(name, i, count):
    return f"{name}.svg" if count == 1 else f"{name}-{i + 1:02d}.svg"


def generate(out):
    out.mkdir(parents=True, exist_ok=True)
    lines = ["# name hot_x hot_y frames aliases... (generated by tools/gen-cursors.py)",
             f"# sizes {' '.join(str(s) for s in T['sizes'])} frame_ms {T['frameMs']}"]
    for name, (frames, (hx, hy), aliases) in cursors().items():
        for i, svg in enumerate(frames):
            (out / frame_name(name, i, len(frames))).write_text(svg)
        lines.append(" ".join([name, f"{hx:g}", f"{hy:g}", str(len(frames))] + aliases))
    (out / "cursors.txt").write_text("\n".join(lines) + "\n")


def sheet():
    items = list(cursors().items())
    cols, cell = 9, 48
    rows = (len(items) + cols - 1) // cols
    w, h = cols * cell, rows * cell
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">'
             f'<rect width="{w}" height="{h}" fill="#3A6EA5"/>']
    for k, (name, (frames, _, _)) in enumerate(items):
        x, y = (k % cols) * cell + 8, (k // cols) * cell + 8
        inner = frames[len(frames) // 3].split(">", 1)[1].rsplit("</svg>", 1)[0]
        parts.append(f'<svg x="{x}" y="{y}" width="32" height="32" viewBox="0 0 32 32">{inner}</svg>')
    parts.append("</svg>\n")
    return "".join(parts)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--output", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--check", nargs="?", const=DEFAULT_OUT, type=Path, metavar="DIR")
    ap.add_argument("--sheet", type=Path, metavar="OUT.svg")
    args = ap.parse_args()
    if args.sheet:
        args.sheet.write_text(sheet())
        return 0
    if args.check:
        with tempfile.TemporaryDirectory() as tmp:
            generate(Path(tmp))
            a = sorted(p.name for p in args.check.glob("*"))
            b = sorted(p.name for p in Path(tmp).glob("*"))
            stale = a != b or any(not filecmp.cmp(args.check / n, Path(tmp) / n, shallow=False) for n in b)
        if stale:
            print(f"{args.check} is out of date; run tools/gen-cursors.py")
            return 1
        print(f"cursors: {len(cursors())} cursors up to date")
        return 0
    shutil.rmtree(args.output, ignore_errors=True)
    generate(args.output)
    print(f"wrote {len(cursors())} cursors to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
