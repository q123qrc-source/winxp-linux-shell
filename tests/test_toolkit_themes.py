#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The GTK and Kvantum themes generated from theme.json are complete:

- the templates pass the literal lint and every placeholder resolves in
  every scheme (gen-toolkit-themes.py --lint);
- one GTK theme and one Kvantum theme per scheme, named by the tokens;
- every url("assets/...") in gtk.css exists and every SVG parses;
- every element the kvconfig names (frame/interior/indicator) is drawn in the
  Kvantum SVG, at least in its "normal" state.
"""
import configparser
import json
import re
import subprocess
import sys
import tempfile
import unittest
import xml.dom.minidom
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GEN = ROOT / "tools" / "gen-toolkit-themes.py"
SCHEMES = sorted(p.parent.name for p in (ROOT / "theme").glob("*/theme.json"))


class ToolkitThemes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.out = Path(cls.tmp.name)
        subprocess.run([sys.executable, GEN, "--out", cls.out], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def names(self, scheme):
        t = json.loads((ROOT / "theme" / scheme / "theme.json").read_text())
        return t["toolkit"]["gtkThemeName"], t["toolkit"]["kvantumThemeName"]

    def test_lint(self):
        r = subprocess.run([sys.executable, GEN, "--lint"], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_one_theme_per_scheme(self):
        for s in SCHEMES:
            gtk, kv = self.names(s)
            with self.subTest(scheme=s):
                for sub in ("gtk-3.0", "gtk-4.0"):
                    self.assertTrue((self.out / "themes" / gtk / sub / "gtk.css").is_file())
                self.assertTrue((self.out / "themes" / gtk / "index.theme").is_file())
                self.assertTrue((self.out / "Kvantum" / kv / f"{kv}.kvconfig").is_file())
                self.assertTrue((self.out / "Kvantum" / kv / f"{kv}.svg").is_file())

    def test_gtk_assets_exist_and_parse(self):
        for s in SCHEMES:
            gtk, _ = self.names(s)
            for sub in ("gtk-3.0", "gtk-4.0"):
                d = self.out / "themes" / gtk / sub
                css = (d / "gtk.css").read_text()
                self.assertNotIn("@{", css)
                for name in set(re.findall(r'url\("(assets/[^"]+)"\)', css)):
                    with self.subTest(scheme=s, toolkit=sub, asset=name):
                        self.assertTrue((d / name).is_file())
                for svg in d.glob("assets/*.svg"):
                    xml.dom.minidom.parse(str(svg))

    def test_kvantum_elements_drawn(self):
        for s in SCHEMES:
            _, kv = self.names(s)
            d = self.out / "Kvantum" / kv
            ids = set(re.findall(r'id="([^"]+)"', (d / f"{kv}.svg").read_text()))
            cfg = configparser.ConfigParser(interpolation=None, strict=False)
            cfg.optionxform = str
            cfg.read(d / f"{kv}.kvconfig")
            for section in cfg.sections():
                for key in ("frame.element", "interior.element"):
                    el = cfg[section].get(key)
                    enabled = cfg[section].get(key.split(".")[0], "true") != "false"
                    if not el or el == "none" or not enabled:
                        continue
                    with self.subTest(scheme=s, section=section, key=key):
                        self.assertTrue(any(i.startswith(f"{el}-") for i in ids), f"{el} not drawn")
            # The indicators Meadow relies on.
            for el in ("arrow-down-normal", "arrow-up-normal", "checkbox-checked-normal", "radio-checked-normal",
                       "progress-pattern-normal", "grip-normal", "button-normal-topleft", "tab-toggled-top"):
                self.assertIn(el, ids)


if __name__ == "__main__":
    unittest.main()
