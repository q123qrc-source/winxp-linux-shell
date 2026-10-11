#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""meadow-auth-helper, driven over its stdin/stdout protocol.

PAM is pam_wrapper + pam_matrix with a private service directory (nothing
under /etc is read or changed). Checks: right and wrong password, the frame
format, info/error messages, cancelling, protocol errors, that the helper
authenticates only the calling user, and that no secret reaches stderr.

    test_auth_helper.py HELPER
Skips (77) without pam_wrapper.
"""
import os
import pwd
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HELPER = None
PAM_DIRS = ["/usr/lib64/pam_wrapper", "/usr/lib/pam_wrapper", "/usr/lib/x86_64-linux-gnu/pam_wrapper"]


def find_module(name):
    for d in PAM_DIRS:
        if os.path.exists(os.path.join(d, name)):
            return os.path.join(d, name)
    return None


def frame(kind, payload=b""):
    return kind.encode() + b" " + str(len(payload)).encode() + b"\n" + payload


def parse(out):
    frames = []
    while out:
        header, _, rest = out.partition(b"\n")
        kind, length = header.split(b" ")
        n = int(length)
        frames.append((kind.decode(), rest[:n].decode()))
        out = rest[n:]
    return frames


class AuthHelperTests(unittest.TestCase):
    PASSWORD = "c0rrect horse ✓"

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        tmp = Path(cls.tmp.name)
        user = pwd.getpwuid(os.getuid()).pw_name
        matrix = find_module("pam_matrix.so")
        chatty = find_module("pam_chatty.so")
        (tmp / "passdb").write_text(f"{user}:{cls.PASSWORD}:svc\n")
        (tmp / "passdb-chatty").write_text(f"{user}:other:svc-chatty\n")
        (tmp / "svc").mkdir()
        (tmp / "svc" / "svc").write_text(
            f"auth required {matrix} passdb={tmp}/passdb\naccount required {matrix} passdb={tmp}/passdb\n")
        # pam_chatty sends info and error messages before the password check.
        (tmp / "svc" / "svc-chatty").write_text(
            f"auth required {chatty} info_msg error_msg\n"
            f"auth required {matrix} passdb={tmp}/passdb-chatty\naccount required {matrix} passdb={tmp}/passdb-chatty\n")
        cls.env = dict(os.environ, LD_PRELOAD="libpam_wrapper.so", PAM_WRAPPER="1",
                       PAM_WRAPPER_SERVICE_DIR=str(tmp / "svc"), MEADOW_PAM_SERVICE="svc")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_helper(self, stdin, service="svc", args=()):
        env = dict(self.env, MEADOW_PAM_SERVICE=service)
        p = subprocess.run([HELPER, *args], input=stdin, env=env, capture_output=True, timeout=20)
        return p.returncode, parse(p.stdout) if p.returncode != 2 or p.stdout else [], p.stderr.decode()

    def test_right_password(self):
        code, frames, err = self.run_helper(frame("RESPONSE", self.PASSWORD.encode()))
        self.assertEqual(code, 0)
        self.assertEqual(frames[0][0], "PROMPT_ECHO_OFF")
        self.assertEqual(frames[-1], ("RESULT", "ok"))
        self.assertNotIn(self.PASSWORD, err)
        self.assertIn("succeeded", err)

    def test_wrong_password(self):
        code, frames, err = self.run_helper(frame("RESPONSE", b"guess"))
        self.assertEqual(code, 1)
        self.assertEqual(frames[-1], ("RESULT", "fail"))
        self.assertNotIn("guess", err)

    def test_info_and_error_messages_are_forwarded_not_logged(self):
        code, frames, err = self.run_helper(frame("RESPONSE", b"other"), service="svc-chatty")
        kinds = [k for k, _ in frames]
        self.assertIn("INFO", kinds)
        self.assertIn("ERROR", kinds)
        self.assertEqual(code, 0, frames)
        for _, text in frames:
            if text and text not in ("ok", "fail"):
                self.assertNotIn(text, err)

    def test_cancel_fails(self):
        code, frames, _ = self.run_helper(frame("CANCEL"))
        self.assertEqual(code, 1)
        self.assertEqual(frames[-1], ("RESULT", "fail"))

    def test_eof_fails(self):
        code, frames, _ = self.run_helper(b"")
        self.assertEqual(code, 1)
        self.assertEqual(frames[-1], ("RESULT", "fail"))

    def test_malformed_frames_fail(self):
        for bad in (b"RESPONSE x\nabc", b"RESPONSE 99999\n", b"garbage\n", b"RESPONSE 10\nshort"):
            code, frames, _ = self.run_helper(bad)
            self.assertEqual(code, 1, bad)
            self.assertEqual(frames[-1], ("RESULT", "fail"), bad)

    def test_no_arguments_accepted(self):
        # The user cannot be chosen: any argument is refused.
        code, _, err = self.run_helper(b"", args=("root",))
        self.assertEqual(code, 2)
        self.assertIn("usage", err)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("usage: test_auth_helper.py HELPER")
    HELPER = sys.argv.pop(1)
    if not find_module("pam_matrix.so"):
        print("SKIP: pam_wrapper (pam_matrix.so) not installed")
        sys.exit(77)
    unittest.main(verbosity=2)
