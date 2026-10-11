#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The built meadow-cursors theme is complete:

- every cursor in cursors.txt is an XCursor file with an image at each size;
- animated cursors (wait, progress) have more than one frame per size;
- every alias is a symlink to an existing cursor;
- the names toolkits ask for most (CSS and X11) are all present.

  test_cursors.py BUILT_CURSORS_DIR SRC_DIR
"""
import os
import struct
import sys
import unittest
from pathlib import Path

CURSORS, SRC = Path(sys.argv[1]), Path(sys.argv[2])
REQUIRED = ["default", "left_ptr", "text", "xterm", "pointer", "hand2", "wait", "watch", "progress",
            "left_ptr_watch", "help", "crosshair", "not-allowed", "move", "grab", "grabbing", "copy", "alias",
            "n-resize", "s-resize", "e-resize", "w-resize", "ne-resize", "nw-resize", "se-resize", "sw-resize",
            "ns-resize", "ew-resize", "nesw-resize", "nwse-resize", "col-resize", "row-resize",
            "top_left_corner", "bottom_right_corner", "sb_h_double_arrow", "sb_v_double_arrow", "fleur",
            "context-menu", "cell", "all-scroll", "zoom-in", "zoom-out", "pencil"]
IMAGE = 0xFFFD0002


def images(path):
    """(nominal size, delay) of every image chunk in an XCursor file."""
    data = path.read_bytes()
    magic, header, _version, ntoc = struct.unpack_from("<4sIII", data, 0)
    assert magic == b"Xcur", f"{path} is not an XCursor file"
    out = []
    for i in range(ntoc):
        kind, subtype, pos = struct.unpack_from("<III", data, header + i * 12)
        if kind == IMAGE:
            _h, _t, _st, _v, w, h, _x, _y, delay = struct.unpack_from("<IIIIIIIII", data, pos)
            out.append((subtype, delay))
    return out


class Cursors(unittest.TestCase):
    def table(self):
        rows = []
        sizes = []
        for line in (SRC / "cursors.txt").read_text().splitlines():
            if line.startswith("# sizes"):
                sizes = [int(x) for x in line.split("frame_ms")[0].split()[2:]]
            if not line or line.startswith("#"):
                continue
            name, _hx, _hy, frames, *aliases = line.split()
            rows.append((name, int(frames), aliases))
        return rows, sizes

    def test_each_cursor_has_every_size(self):
        rows, sizes = self.table()
        self.assertTrue(sizes)
        for name, frames, _ in rows:
            with self.subTest(cursor=name):
                imgs = images(CURSORS / name)
                for size in sizes:
                    n = sum(1 for s, _ in imgs if s == size)
                    self.assertEqual(n, frames, f"{name} at {size}px")
                if frames > 1:
                    self.assertTrue(all(d > 0 for _, d in imgs), "animated frames need a delay")

    def test_animated(self):
        rows, _ = self.table()
        frames = {n: f for n, f, _ in rows}
        self.assertGreater(frames["wait"], 1)
        self.assertGreater(frames["progress"], 1)

    def test_aliases(self):
        rows, _ = self.table()
        for name, _, aliases in rows:
            for a in aliases:
                with self.subTest(alias=a):
                    p = CURSORS / a
                    self.assertTrue(p.is_symlink())
                    self.assertEqual(os.readlink(p), name)

    def test_required_names(self):
        for n in REQUIRED:
            with self.subTest(name=n):
                self.assertTrue((CURSORS / n).exists())


if __name__ == "__main__":
    unittest.main(argv=sys.argv[:1])
