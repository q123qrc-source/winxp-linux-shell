#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Tests for setup/meadow-setup against sandbox roots (--root): PAM parsing,
reachability through includes, the marked sddm patch, the generated lock
service, idempotence, dry runs and byte-exact reverts. Nothing here touches
the real /etc."""
import hashlib
import importlib.machinery
import importlib.util
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
_loader = importlib.machinery.SourceFileLoader("meadow_setup", str(ROOT / "setup" / "meadow-setup"))
_spec = importlib.util.spec_from_loader("meadow_setup", _loader)
ms = importlib.util.module_from_spec(_spec)
_loader.exec_module(ms)

# Same structure as Fedora's /etc/pam.d/sddm and authselect "local" profile
# (written for these tests).
FEDORA = {
    "etc/os-release": 'NAME="Fedora Linux"\nID=fedora\nVERSION_ID=44\n',
    "etc/pam.d/sddm": """\
auth     [success=done ignore=ignore default=bad] pam_selinux_permit.so
auth        substack      password-auth
-auth        optional      pam_gnome_keyring.so
-auth        optional      pam_kwallet5.so
-auth        optional      pam_kwallet.so
auth        include       postlogin

account     required      pam_nologin.so
account     include       password-auth

password    include       password-auth

session     required      pam_selinux.so close
session     required      pam_loginuid.so
session     include       password-auth
session     include       postlogin
""",
    "etc/pam.d/password-auth": """\
auth        required                                     pam_env.so
auth        required                                     pam_faildelay.so delay=2000000
auth        sufficient                                   pam_unix.so nullok
auth        required                                     pam_deny.so
account     required                                     pam_unix.so
password    sufficient                                   pam_unix.so yescrypt shadow nullok use_authtok
session     required                                     pam_unix.so
""",
    "etc/pam.d/system-auth": """\
auth        required                                     pam_env.so
auth        sufficient                                   pam_fprintd.so
auth        sufficient                                   pam_unix.so nullok
auth        required                                     pam_deny.so
account     required                                     pam_unix.so
""",
    "etc/pam.d/postlogin": "session     optional                   pam_umask.so silent\n",
}

DEBIAN = {
    "etc/os-release": "ID=debian\n",
    "etc/pam.d/sddm": """\
#%PAM-1.0
auth    include         common-auth
@include common-auth
-auth   optional        pam_gnome_keyring.so
@include common-account
@include common-password
session required        pam_limits.so
@include common-session
""",
    "etc/pam.d/common-auth": """\
auth    [success=2 default=ignore]      pam_unix.so nullok
auth    requisite                       pam_deny.so
auth    required                        pam_permit.so
""",
    "etc/pam.d/common-account": "account [success=1 new_authtok_reqd=done default=ignore] pam_unix.so\n",
}

DEBIAN_WITH_FPRINTD = dict(DEBIAN)
DEBIAN_WITH_FPRINTD["etc/pam.d/common-auth"] = """\
auth    [success=3 default=ignore]      pam_fprintd.so max-tries=1 timeout=10
auth    [success=2 default=ignore]      pam_unix.so nullok try_first_pass
auth    requisite                       pam_deny.so
auth    required                        pam_permit.so
"""

ARCH = {
    "etc/os-release": "ID=arch\n",
    "etc/pam.d/sddm": """\
#%PAM-1.0
auth        include     system-login
-auth       optional    pam_gnome_keyring.so
account     include     system-login
password    include     system-login
session     include     system-login
""",
    "etc/pam.d/system-login": "auth       include    system-auth\naccount    include    system-auth\n",
    "etc/pam.d/system-auth": "auth       required   pam_unix.so try_first_pass nullok\naccount    required   pam_unix.so\n",
}


def make_root(files):
    tmp = tempfile.TemporaryDirectory()
    root = Path(tmp.name)
    for rel, text in files.items():
        p = root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text)
    return tmp, root


def tree_hash(root):
    h = hashlib.sha256()
    for p in sorted(root.rglob("*")):
        if p.is_file():
            h.update(str(p.relative_to(root)).encode())
            h.update(p.read_bytes())
    return h.hexdigest()


def run(root, *args):
    out = io.StringIO()
    code = ms.main(["--root", str(root), "--yes", *args], out=out)
    return code, out.getvalue()


class ParsingTests(unittest.TestCase):
    def test_bracket_controls_and_comments(self):
        self.assertEqual(ms.parse_pam_line("auth [success=done ignore=ignore default=bad] pam_selinux_permit.so"),
                         ("auth", "[success=done ignore=ignore default=bad]", "pam_selinux_permit.so", ""))
        self.assertEqual(ms.parse_pam_line("-auth optional pam_kwallet5.so  # comment"),
                         ("auth", "optional", "pam_kwallet5.so", ""))
        self.assertEqual(ms.parse_pam_line("@include common-auth"), ("@include", "common-auth"))
        self.assertIsNone(ms.parse_pam_line("   # just a comment"))
        self.assertIsNone(ms.parse_pam_line(""))

    def test_reachability_follows_includes(self):
        for files, expected in ((FEDORA, False), (DEBIAN, False), (DEBIAN_WITH_FPRINTD, True), (ARCH, False)):
            tmp, root = make_root(files)
            with tmp:
                self.assertEqual(ms.auth_reaches(root / "etc/pam.d", "sddm"), expected, files["etc/os-release"])
        # Fedora system-auth has it, but sddm only reaches password-auth.
        tmp, root = make_root(FEDORA)
        with tmp:
            self.assertTrue(ms.auth_reaches(root / "etc/pam.d", "system-auth"))

    def test_include_cycles_terminate(self):
        tmp, root = make_root({"etc/pam.d/a": "auth include b\n", "etc/pam.d/b": "auth include a\n"})
        with tmp:
            self.assertFalse(ms.auth_reaches(root / "etc/pam.d", "a"))

    def test_block_goes_before_password_stack(self):
        lines = FEDORA["etc/pam.d/sddm"].splitlines()
        patched = ms.with_fingerprint_block(lines)
        i = patched.index(ms.FPRINT_LINE)
        self.assertEqual(patched[i + 2].split(), ["auth", "substack", "password-auth"])
        self.assertTrue(patched[0].startswith("auth     [success=done ignore=ignore default=bad] pam_selinux_permit.so"))
        stripped, removed = ms.strip_block(patched)
        self.assertTrue(removed)
        self.assertEqual(stripped, lines)

    def test_no_insertion_point_is_an_error(self):
        with self.assertRaises(ms.SetupError):
            ms.with_fingerprint_block(["auth required pam_permit.so"])

    def test_unterminated_block_is_an_error(self):
        with self.assertRaises(ms.SetupError):
            ms.strip_block([ms.FPRINT_BEGIN, ms.FPRINT_LINE])


class EnableDisableTests(unittest.TestCase):
    def check_roundtrip(self, files, expect_patch):
        tmp, root = make_root(files)
        with tmp:
            before = tree_hash(root)
            code, out = run(root, "enable-fingerprint")
            self.assertEqual(code, 0, out)
            sddm = (root / ms.SDDM_PAM).read_text()
            self.assertEqual(ms.FPRINT_BEGIN in sddm, expect_patch, out)
            self.assertTrue(ms.auth_reaches(root / "etc/pam.d", "sddm") or not expect_patch)
            self.assertEqual((root / (ms.SDDM_PAM + ".meadow-bak")).exists(), expect_patch)
            lock = (root / ms.LOCK_PAM).read_text()
            self.assertIn(ms.LOCK_MARKER, lock)
            self.assertTrue(ms.auth_reaches(root / "etc/pam.d", "meadow-lock") or not expect_patch)
            self.assertEqual(lock.count("pam_fprintd.so"), 1 if expect_patch else 0)
            self.assertNotIn("pam_selinux_permit", lock)
            self.assertNotIn("pam_gnome_keyring", lock)

            # Second run: no change at all.
            after_first = tree_hash(root)
            code, out = run(root, "enable-fingerprint")
            self.assertEqual(code, 0, out)
            self.assertEqual(tree_hash(root), after_first, "enable is not idempotent")

            code, out = run(root, "disable-fingerprint")
            self.assertEqual(code, 0, out)
            self.assertEqual(tree_hash(root), before, "disable did not restore the original files")

    def test_fedora(self):
        self.check_roundtrip(FEDORA, expect_patch=True)

    def test_debian_without_fprintd_profile(self):
        self.check_roundtrip(DEBIAN, expect_patch=True)

    def test_debian_with_fprintd_profile_is_left_alone(self):
        self.check_roundtrip(DEBIAN_WITH_FPRINTD, expect_patch=False)

    def test_arch(self):
        self.check_roundtrip(ARCH, expect_patch=True)

    def test_dry_run_changes_nothing(self):
        tmp, root = make_root(FEDORA)
        with tmp:
            before = tree_hash(root)
            code, out = run(root, "--dry-run", "enable-fingerprint", "--allow-greeter-query")
            self.assertEqual(code, 0, out)
            self.assertEqual(tree_hash(root), before)
            self.assertIn("+" + ms.FPRINT_LINE, out)  # the diff shows the change
            self.assertIn("Dry run", out)

    def test_foreign_lock_service_is_backed_up_and_restored(self):
        files = dict(FEDORA)
        files["etc/pam.d/meadow-lock"] = "auth required pam_unix.so\n"
        tmp, root = make_root(files)
        with tmp:
            before = tree_hash(root)
            self.assertEqual(run(root, "enable-fingerprint")[0], 0)
            self.assertTrue((root / (ms.LOCK_PAM + ".meadow-bak")).exists())
            self.assertEqual(run(root, "disable-fingerprint")[0], 0)
            self.assertEqual(tree_hash(root), before)

    def test_admin_edits_after_enable_survive_disable(self):
        tmp, root = make_root(FEDORA)
        with tmp:
            self.assertEqual(run(root, "enable-fingerprint")[0], 0)
            sddm = root / ms.SDDM_PAM
            sddm.write_text(sddm.read_text() + "session optional pam_admin_added.so\n")
            code, out = run(root, "disable-fingerprint")
            self.assertEqual(code, 0)
            text = sddm.read_text()
            self.assertIn("pam_admin_added.so", text)
            self.assertNotIn(ms.FPRINT_BEGIN, text)
            self.assertTrue((root / (ms.SDDM_PAM + ".meadow-bak")).exists())  # kept: file differs
            self.assertIn("backup kept", out)

    def test_polkit_rule_is_opt_in_and_removed(self):
        tmp, root = make_root(FEDORA)
        with tmp:
            run(root, "enable-fingerprint")
            self.assertFalse((root / ms.POLKIT_PATH).exists())
            run(root, "enable-fingerprint", "--allow-greeter-query")
            rule = (root / ms.POLKIT_PATH).read_text()
            self.assertIn("setusername", rule)
            self.assertNotIn("enroll", rule.split("It does NOT")[0].lower().replace("enrolled", ""))
            run(root, "disable-fingerprint")
            self.assertFalse((root / ms.POLKIT_PATH).exists())

    def test_missing_sddm_fails_cleanly(self):
        files = {k: v for k, v in FEDORA.items() if k != "etc/pam.d/sddm"}
        tmp, root = make_root(files)
        with tmp:
            before = tree_hash(root)
            code, _ = run(root, "enable-fingerprint")
            self.assertEqual(code, 2)
            self.assertEqual(tree_hash(root), before)

    def test_status_is_read_only(self):
        tmp, root = make_root(FEDORA)
        with tmp:
            before = tree_hash(root)
            code, out = run(root, "status")
            self.assertEqual(code, 0)
            self.assertIn("not reachable", out)
            self.assertEqual(tree_hash(root), before)

    @unittest.skipIf(os.geteuid() == 0, "running as root")
    def test_real_root_requires_root(self):
        out = io.StringIO()
        self.assertEqual(ms.main(["--yes", "enable-fingerprint"], out=out), 1)


class ShippedFilesTests(unittest.TestCase):
    """system/ holds the reference copies installed under /usr/share/meadow."""

    def test_snippet_matches_tool(self):
        self.assertEqual((ROOT / "system/pam.d/sddm.meadow-snippet").read_text().splitlines(), ms.FPRINT_BLOCK)

    def test_polkit_rule_matches_tool(self):
        self.assertEqual((ROOT / "system/polkit/50-meadow.rules").read_text(), ms.POLKIT_RULE)

    def test_lock_example_matches_fedora_output(self):
        tmp, root = make_root(FEDORA)
        with tmp:
            run(root, "enable-fingerprint")
            generated = (root / ms.LOCK_PAM).read_text()
        self.assertEqual((ROOT / "system/pam.d/meadow-lock").read_text(), generated)


if __name__ == "__main__":
    unittest.main(verbosity=2)
