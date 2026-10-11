#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""meadow-session's session-scoped defaults for third-party applications.

Runs the real session/meadow-session against a sandbox (a fake labwc and a
fake systemctl on PATH, a private XDG_RUNTIME_DIR and MEADOW_DATADIR):
- labwc sees DCONF_PROFILE, FONTCONFIG_FILE and XDG_CONFIG_DIRS with the
  per-session runtime directory first, then $MEADOW_DATADIR/xdg, then the
  system's directories; nothing in HOME is created;
- when labwc exits, Meadow's variables are unset from the user's systemd
  environment again, the GTK portal is stopped, the per-session directories
  are removed and labwc's exit status is the session's;
- SIGTERM to the session reaches labwc;
- with the generated dconf database: gsettings inside the profile reports the
  Meadow defaults, a user's own setting still wins, and without the profile
  nothing changes.

  test_session_defaults.py [BUILD_DIR]   (BUILD_DIR/session-defaults for dconf)
"""
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build"
SESSION = ROOT / "session" / "meadow-session"

FAKE_LABWC = """#!/bin/sh
env > "$SANDBOX/labwc.env"
echo $$ > "$SANDBOX/labwc.pid"
trap 'echo TERM > "$SANDBOX/labwc.signal"; exit 143' TERM
if [ -n "$LABWC_WAIT" ]; then while :; do sleep 0.1; done; fi
exit "${LABWC_STATUS:-0}"
"""
FAKE_SYSTEMCTL = """#!/bin/sh
echo "$*" >> "$SANDBOX/systemctl.log"
"""


class SessionDefaults(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.box = Path(self.tmp.name)
        for d in ("bin", "run", "home", "data/meadow/labwc", "data/meadow/xdg/Kvantum", "data/meadow/dconf",
                  "data/meadow/fontconfig"):
            (self.box / d).mkdir(parents=True, exist_ok=True)
        os.chmod(self.box / "run", 0o700)
        for name, text in (("labwc", FAKE_LABWC), ("systemctl", FAKE_SYSTEMCTL)):
            p = self.box / "bin" / name
            p.write_text(text)
            p.chmod(0o755)
        data = self.box / "data" / "meadow"
        (data / "labwc" / "rc.xml").write_text("<labwc_config/>\n")
        (data / "dconf" / "profile").write_text("user-db:user\n")
        (data / "fontconfig" / "fonts.conf").write_text("<fontconfig/>\n")
        (data / "xdg" / "Kvantum" / "kvantum.kvconfig").write_text("[General]\ntheme=MeadowLuna\n")
        self.env = {
            "PATH": f"{self.box / 'bin'}:/usr/bin:/bin", "HOME": str(self.box / "home"),
            "XDG_RUNTIME_DIR": str(self.box / "run"), "MEADOW_DATADIR": str(data), "SANDBOX": str(self.box),
            "XDG_CONFIG_DIRS": "/etc/xdg",
        }

    def tearDown(self):
        self.tmp.cleanup()

    def labwc_env(self):
        env = {}
        for line in (self.box / "labwc.env").read_text().splitlines():
            k, _, v = line.partition("=")
            env[k] = v
        return env

    def test_environment_and_cleanup(self):
        r = subprocess.run(["sh", str(SESSION)], env=dict(self.env, LABWC_STATUS="3"), capture_output=True, text=True,
                           timeout=30)
        self.assertEqual(r.returncode, 3, r.stderr)  # labwc's status is the session's
        env = self.labwc_env()
        data = self.env["MEADOW_DATADIR"]
        self.assertEqual(env["DCONF_PROFILE"], f"{data}/dconf/profile")
        self.assertEqual(env["FONTCONFIG_FILE"], f"{data}/fontconfig/fonts.conf")
        dirs = env["XDG_CONFIG_DIRS"].split(":")
        self.assertEqual(dirs[0], env["MEADOW_RUNTIME_XDG"])
        self.assertTrue(dirs[0].startswith(self.env["XDG_RUNTIME_DIR"]))
        self.assertEqual(dirs[1:], [f"{data}/xdg", "/etc/xdg"])
        self.assertEqual(env["GTK_THEME"], "Meadow-Luna")
        self.assertEqual(env["XCURSOR_THEME"], "meadow-cursors")
        # Cleaned up afterwards.
        log = (self.box / "systemctl.log").read_text()
        self.assertIn("--user unset-environment", log)
        for var in ("DCONF_PROFILE", "FONTCONFIG_FILE", "XDG_CONFIG_DIRS", "GTK_THEME", "QT_STYLE_OVERRIDE"):
            self.assertIn(var, log)
        self.assertIn("--user stop xdg-desktop-portal-gtk.service", log)
        self.assertFalse(Path(env["MEADOW_RUNTIME_XDG"]).exists())
        self.assertEqual(list((self.box / "home").iterdir()), [], "nothing may be written to HOME")

    def test_user_values_win(self):
        own = dict(self.env, DCONF_PROFILE="/my/profile", FONTCONFIG_FILE="/my/fonts.conf")
        subprocess.run(["sh", str(SESSION)], env=own, capture_output=True, timeout=30)
        env = self.labwc_env()
        self.assertEqual(env["DCONF_PROFILE"], "/my/profile")
        self.assertEqual(env["FONTCONFIG_FILE"], "/my/fonts.conf")

    def test_sigterm_reaches_labwc(self):
        p = subprocess.Popen(["sh", str(SESSION)], env=dict(self.env, LABWC_WAIT="1"),
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            if (self.box / "labwc.pid").exists():
                break
            time.sleep(0.05)
        p.send_signal(signal.SIGTERM)
        p.wait(timeout=10)
        self.assertEqual((self.box / "labwc.signal").read_text().strip(), "TERM")
        self.assertIn("unset-environment", (self.box / "systemctl.log").read_text())

    def test_dconf_layering(self):
        db = BUILD / "session-defaults" / "dconf" / "meadow.db"
        if not db.exists() or not shutil.which("gsettings") or not shutil.which("dbus-run-session"):
            self.skipTest("no compiled dconf database or gsettings")
        schemas = subprocess.run(["gsettings", "list-schemas"], capture_output=True, text=True).stdout.split()
        if "org.gnome.desktop.interface" not in schemas or "org.gnome.desktop.sound" not in schemas:
            self.skipTest("gsettings-desktop-schemas not installed")
        profile = self.box / "profile"
        profile.write_text(f"user-db:user\nfile-db:{db}\n")
        env = dict(os.environ, HOME=str(self.box / "home"), XDG_CONFIG_HOME=str(self.box / "home" / ".config"),
                   GSETTINGS_BACKEND="dconf")

        def get(key, schema="org.gnome.desktop.interface", profile_on=True, extra=""):
            e = dict(env, DCONF_PROFILE=str(profile)) if profile_on else dict(env)
            e.pop("DCONF_PROFILE", None) if not profile_on else None
            cmd = f"{extra} gsettings get {schema} {key}"
            return subprocess.run(["dbus-run-session", "--", "sh", "-c", cmd], env=e, capture_output=True,
                                  text=True).stdout.strip()

        self.assertEqual(get("icon-theme"), "'meadow-icons'")
        self.assertEqual(get("cursor-theme"), "'meadow-cursors'")
        self.assertEqual(get("theme-name", "org.gnome.desktop.sound"), "'meadow'")
        self.assertNotEqual(get("icon-theme", profile_on=False), "'meadow-icons'")
        # A value the user set explicitly wins over Meadow's default.
        self.assertEqual(get("icon-theme", extra="gsettings set org.gnome.desktop.interface icon-theme 'Mine' &&"),
                         "'Mine'")


if __name__ == "__main__":
    unittest.main(argv=sys.argv[:1])
