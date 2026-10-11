#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""install.sh / uninstall.sh against a sandbox root (--root), using the
already built tree in $MEADOW_BUILD_DIR (--no-build):

- --dry-run changes nothing;
- install + uninstall leaves the sandbox exactly as it was, including a file
  install.sh had to replace (restored from its *.meadow-bak);
- everything outside the prefix is done by meadow-setup and recorded in its
  state file; --no-pam installs no PAM service;
- a file changed after installing survives uninstall;
- --destdir staging never touches anything outside the prefix;
- missing dependencies are reported with the distribution's package names.
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = Path(os.environ.get("MEADOW_BUILD_DIR", ROOT / "build"))


def cache_var(name):
    for line in (BUILD / "CMakeCache.txt").read_text().splitlines():
        if line.startswith(name + ":"):
            return line.split("=", 1)[1]
    return None


PREFIX = cache_var("CMAKE_INSTALL_PREFIX") if (BUILD / "CMakeCache.txt").exists() else None
QML_DIR = "/usr/lib64/qt6/qml"


def tree_state(root):
    state = {}
    for p in sorted(root.rglob("*")):
        rel = str(p.relative_to(root))
        if p.is_symlink():
            state[rel] = "-> " + os.readlink(p)
        elif p.is_file():
            state[rel] = hashlib.sha256(p.read_bytes()).hexdigest()
    return state


@unittest.skipUnless(PREFIX and shutil.which("cmake"), "needs a configured build in MEADOW_BUILD_DIR")
class InstallScript(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name) / "root"
        files = {
            "etc/os-release": "ID=arch\n",
            "etc/pam.d/system-auth": "auth required pam_unix.so\naccount required pam_unix.so\n",
            "etc/pam.d/sddm": "auth include system-auth\n",
        }
        for rel, text in files.items():
            p = self.root / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(text)
        (self.root / QML_DIR.lstrip("/")).mkdir(parents=True)
        (self.root / "var/lib").mkdir(parents=True)

    def tearDown(self):
        self.tmp.cleanup()

    def script(self, name, *args, check=True):
        env = dict(os.environ, MEADOW_SYSTEM_QML_DIR=QML_DIR)
        env.pop("DESTDIR", None)
        r = subprocess.run(["bash", ROOT / name, *args], capture_output=True, text=True, env=env)
        if check:
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return r

    def install(self, *args, check=True):
        return self.script("install.sh", "--root", self.root, "--no-build", "--build-dir", BUILD,
                           "--prefix", PREFIX, *args, check=check)

    def uninstall(self, *args, check=True):
        return self.script("uninstall.sh", "--root", self.root, "--prefix", PREFIX, *args, check=check)

    def p(self, path):
        return self.root / path.lstrip("/")

    def test_dry_run_changes_nothing(self):
        before = tree_state(self.root)
        r = self.install("--dry-run", "--enable-greeter")
        self.assertIn("Dry run: nothing was changed.", r.stdout)
        self.assertIn("would install", r.stdout)
        self.assertEqual(tree_state(self.root), before)

    def test_round_trip_restores_everything(self):
        # A file of someone else's where Meadow installs one.
        foreign = self.p(f"{PREFIX}/share/meadow/shell.conf")
        foreign.parent.mkdir(parents=True)
        foreign.write_text("someone else's\n")
        before = tree_state(self.root)
        r = self.install("--enable-greeter")
        self.assertTrue(self.p(f"{PREFIX}/bin/meadow-shell").exists())
        self.assertTrue(self.p(f"{PREFIX}/share/meadow/install-manifest.txt").exists())
        self.assertTrue(Path(str(foreign) + ".meadow-bak").exists())
        self.assertTrue(self.p("/etc/pam.d/meadow-lock").exists())
        self.assertIn("Current=meadow", self.p("/etc/sddm.conf.d/10-meadow.conf").read_text())
        if PREFIX != "/usr":
            self.assertEqual(os.readlink(self.p("/usr/share/sddm/themes/meadow")),
                             f"{PREFIX}/share/sddm/themes/meadow")
            self.assertIn(f"Exec={PREFIX}/bin/meadow-session",
                          self.p("/usr/share/wayland-sessions/meadow.desktop").read_text())
        # Everything outside the prefix is meadow-setup's, and recorded.
        state = json.loads(self.p("/var/lib/meadow/setup-state.json").read_text())
        self.assertTrue(state["lock"]["installed"])
        self.assertTrue(state["greeter"]["enabled"])
        outside = [k for k in tree_state(self.root) if not ("/" + k).startswith(PREFIX + "/")]
        allowed = {"etc/pam.d/meadow-lock", "etc/sddm.conf.d/10-meadow.conf", "var/lib/meadow/setup-state.json",
                   "usr/share/sddm/themes/meadow", "usr/share/wayland-sessions/meadow.desktop",
                   f"{QML_DIR.lstrip('/')}/Meadow"}
        self.assertLessEqual(set(outside) - set(before), allowed)
        # Installing again (an update) keeps the original backup.
        self.install("--enable-greeter")
        self.assertEqual(Path(str(foreign) + ".meadow-bak").read_text(), "someone else's\n")
        r = self.uninstall()
        self.assertEqual(tree_state(self.root), before, r.stdout + r.stderr)

    def test_no_pam_and_changed_files(self):
        before = tree_state(self.root)
        self.install("--no-pam")
        self.assertFalse(self.p("/etc/pam.d/meadow-lock").exists())
        manifest = self.p(f"{PREFIX}/share/meadow/install-manifest.txt").read_text()
        self.assertIn("option pam=0", manifest)
        conf = self.p(f"{PREFIX}/share/meadow/shell.conf")
        conf.write_text(conf.read_text() + "# local change\n")
        r = self.uninstall()
        self.assertIn("changed since it was installed; kept", r.stderr)
        self.assertTrue(conf.exists())
        after = tree_state(self.root)
        self.assertEqual({k: v for k, v in after.items() if not k.endswith("shell.conf")}, before)

    def test_fedora_changes_only_through_meadow_setup(self):
        (self.root / "etc/os-release").write_text("ID=fedora\n")
        (self.root / "etc/pam.d/password-auth").write_text("auth sufficient pam_unix.so\naccount required pam_unix.so\n")
        etc_before = {k: v for k, v in tree_state(self.root).items() if k.startswith("etc/")}
        r = self.install("--no-pam")
        self.assertIn("RPM packages are the supported way", r.stdout)
        etc_after = {k: v for k, v in tree_state(self.root).items() if k.startswith("etc/")}
        self.assertEqual(etc_after, etc_before, "install.sh wrote under /etc on Fedora")
        r = self.install()
        self.assertIn("meadow-setup --root", r.stdout)
        self.assertEqual(self.p("/etc/pam.d/meadow-lock").read_text(),
                         (ROOT / "system/pam.d/meadow-lock").read_text())
        self.uninstall()

    def test_destdir_stages_inside_prefix_only(self):
        stage = Path(self.tmp.name) / "stage"
        env = dict(os.environ, DESTDIR=str(stage))
        r = subprocess.run(["bash", ROOT / "install.sh", "--no-build", "--build-dir", BUILD, "--prefix", PREFIX],
                           capture_output=True, text=True, env=env)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("run as root", r.stdout)
        outside = [k for k in tree_state(stage) if not ("/" + k).startswith(PREFIX + "/")]
        self.assertEqual(outside, [])

    def test_prefix_mismatch_is_refused(self):
        r = self.install("--prefix", "/opt/elsewhere", check=False)
        self.assertEqual(r.returncode, 2)
        self.assertIn("configured for prefix", r.stderr)

    def test_uninstall_without_manifest(self):
        r = self.uninstall(check=False)
        self.assertEqual(r.returncode, 3)


class DependencyReport(unittest.TestCase):
    def check(self, family):
        empty = tempfile.mkdtemp()
        try:
            r = subprocess.run(["bash", "-c", f'. "{ROOT}/tools/install-lib.sh"; PATH="{empty}"; check_deps {family}'],
                               capture_output=True, text=True)
        finally:
            os.rmdir(empty)
        return r

    def test_package_names_per_family(self):
        expected = {"fedora": "sudo dnf install cmake ninja-build gcc-c++",
                    "arch": "sudo pacman -S --needed cmake ninja gcc",
                    "debian": "sudo apt install cmake ninja-build g++"}
        for family, line in expected.items():
            with self.subTest(family=family):
                r = self.check(family)
                self.assertEqual(r.returncode, 1)
                self.assertIn(line, r.stderr)
                self.assertIn("labwc", r.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
