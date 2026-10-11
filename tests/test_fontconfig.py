#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Meadow's font aliases (system/fontconfig) work, and only in its sessions:

- both files are valid against fontconfig's DTD (when xmllint has it);
- through the session fonts.conf, the classic names resolve to installed open
  fonts of the right kind (Lucida Console -> a monospaced font, Tahoma -> a
  proportional one), and the system's own matching is otherwise unchanged;
- the session file loads the system configuration (no fonts go missing).
Skips (77) without fc-match.
"""
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FC = ROOT / "system" / "fontconfig"
if not shutil.which("fc-match"):
    print("SKIP: fc-match not installed")
    sys.exit(77)


def match(name, conf=None, fmt="%{family[0]}|%{spacing}"):
    env = None
    if conf:
        import os
        env = dict(os.environ, FONTCONFIG_FILE=str(conf))
    return subprocess.run(["fc-match", "-f", fmt, name], capture_output=True, text=True, env=env, check=True).stdout


class FontConfig(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.conf = Path(cls.tmp.name) / "fonts.conf"
        text = (FC / "fonts.conf.in").read_text()
        cls.conf.write_text(text.replace("@MEADOW_FONT_ALIASES@", str(FC / "50-meadow.conf")))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_dtd(self):
        dtd = Path("/usr/share/xml/fontconfig/fonts.dtd")
        if not shutil.which("xmllint") or not dtd.exists():
            self.skipTest("xmllint or fonts.dtd missing")
        r = subprocess.run(["xmllint", "--noout", "--dtdvalid", str(dtd), str(FC / "50-meadow.conf"), str(self.conf)],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_classic_names(self):
        # spacing 100 = monospace (fontconfig FC_MONO).
        mono = match("Lucida Console", self.conf)
        self.assertTrue(mono.endswith("|100"), f"Lucida Console -> {mono}")
        for name in ("Tahoma", "Verdana", "MS Shell Dlg 2", "Trebuchet MS", "Franklin Gothic Medium"):
            with self.subTest(name=name):
                fam, spacing = match(name, self.conf).split("|")
                self.assertNotEqual(spacing, "100", f"{name} -> monospaced {fam}")
                self.assertNotEqual(fam, name, "the original font must not be needed")

    def test_system_config_still_loaded(self):
        # The session file includes the system's: the same fonts are known.
        def count(conf=None):
            import os
            env = dict(os.environ, FONTCONFIG_FILE=str(conf)) if conf else None
            return len(subprocess.run(["fc-list", ":", "file"], capture_output=True, text=True, env=env).stdout.splitlines())
        self.assertEqual(count(self.conf), count())
        self.assertEqual(match("sans-serif", self.conf), match("sans-serif"))


if __name__ == "__main__":
    unittest.main()
