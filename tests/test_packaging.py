#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""The Fedora spec, the Arch PKGBUILD and the Debian *.install files agree
with what CMake installs, without building any package:

- every file `cmake --install` produces belongs to exactly one subpackage in
  each format (nothing unowned, nothing shipped twice);
- every path a package lists exists in the install;
- no package runs install scripts, and the only file any of them puts in
  /etc is the lock screen's own PAM service.
"""
import fnmatch
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = Path(os.environ.get("MEADOW_BUILD_DIR", ROOT / "build"))
SPEC = ROOT / "packaging/fedora/meadow.spec"
PKGBUILD = ROOT / "packaging/arch/PKGBUILD"
DEBIAN = ROOT / "packaging/debian"


def cache_var(name):
    for line in (BUILD / "CMakeCache.txt").read_text().splitlines():
        if line.startswith(name + ":"):
            return line.split("=", 1)[1]
    return None


def installed_files():
    """Paths cmake --install produces, in a layout with LIB/, QML/ and
    LIBEXEC/ standing for the distribution-specific directories."""
    prefix = cache_var("CMAKE_INSTALL_PREFIX")
    libdir = cache_var("CMAKE_INSTALL_LIBDIR") or "lib64"
    libexec = cache_var("CMAKE_INSTALL_LIBEXECDIR") or "libexec"
    qml = cache_var("MEADOW_QML_INSTALL_DIR")
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(["cmake", "--install", BUILD], env=dict(os.environ, DESTDIR=tmp),
                       check=True, capture_output=True)
        files = set()
        for p in Path(tmp).rglob("*"):
            if p.is_dir() and not p.is_symlink():
                continue
            full = "/" + str(p.relative_to(tmp))
            rel = full[len(prefix) + 1:] if full.startswith(prefix + "/") else full.lstrip("/")
            # Meadow's only libexec content is libexec/meadow/; Arch's libexec
            # is lib/, the same as its libdir.
            for real, token in ((qml, "QML"), (libexec + "/meadow", "LIBEXEC/meadow"), (libdir, "LIB")):
                if rel == real or rel.startswith(real + "/"):
                    rel = token + rel[len(real):]
                    break
            files.add(rel)
    return files


def spec_packages():
    text = SPEC.read_text()
    version = re.search(r"^Version:\s*(\S+)", text, re.M).group(1)
    macros = {"_bindir": "bin", "_libdir": "LIB", "_qt6_qmldir": "QML", "_libexecdir": "LIBEXEC",
              "_datadir": "share", "_sysconfdir": "etc", "name": "meadow", "version": version}
    packages, current = {}, None
    for line in text.splitlines():
        m = re.match(r"^%files(?:\s+(-n\s+)?(\S+))?\s*$", line)
        if m:
            current = (m.group(2) if m.group(1) else f"meadow-{m.group(2)}") if m.group(2) else "meadow"
            current = current.replace("%{name}", "meadow")
            packages[current] = []
            continue
        if line.startswith("%") and not line.startswith(("%{", "%dir", "%config", "%license", "%doc")):
            current = None
            continue
        if current is None or not line.strip() or line.startswith("#"):
            continue
        if line.startswith(("%license", "%doc", "%dir")):
            continue
        path = re.sub(r"^%config\([^)]*\)\s*", "", line.strip())
        path = re.sub(r"%\{\??(\w+)\}", lambda m: macros[m.group(1)], path).lstrip("/")
        packages[current].append(path)
    return packages


def pkgbuild_packages():
    names = {"common": "meadow-common", "shell": "meadow-shell", "greeter": "sddm-theme-meadow",
             "themes": "meadow-themes"}
    packages = {}
    for key, pkg in names.items():
        out = subprocess.run(["bash", "-c", f'source "{PKGBUILD}"; printf "%s\\n" "${{_files_{key}[@]}}"'],
                             capture_output=True, text=True, check=True).stdout.split()
        paths = []
        for p in out:
            p = p.removeprefix("usr/")
            p = re.sub(r"^lib/qt6/qml/", "QML/", p)
            p = re.sub(r"^lib/meadow(/|$)", r"LIBEXEC/meadow\1", p)  # CMAKE_INSTALL_LIBEXECDIR=lib
            p = re.sub(r"^lib/", "LIB/", p)
            paths.append(p)
        packages[pkg] = paths
    return packages


def debian_packages():
    packages = {}
    for f in sorted(DEBIAN.glob("*.install")):
        paths = []
        for p in f.read_text().split():
            p = p.removeprefix("usr/")
            p = re.sub(r"^lib/\*/qt6/qml/", "QML/", p)
            p = re.sub(r"^libexec/", "LIBEXEC/", p)
            p = re.sub(r"^lib/\*/", "LIB/", p)
            paths.append(p)
        packages[f.stem] = paths
    return packages


def owners(path, packages):
    found = []
    for pkg, patterns in packages.items():
        for pat in patterns:
            pat = pat.rstrip("/")
            if fnmatch.fnmatchcase(path, pat) or path.startswith(pat + "/"):
                found.append(pkg)
                break
    return found


class Packaging(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not (BUILD / "CMakeCache.txt").exists():
            raise unittest.SkipTest("needs a configured build in MEADOW_BUILD_DIR")
        cls.files = installed_files()

    def check_format(self, name, packages):
        self.assertTrue(packages, name)
        for path in sorted(self.files):
            with self.subTest(format=name, path=path):
                self.assertEqual(len(owners(path, packages)), 1,
                                 f"{path} is owned by {owners(path, packages) or 'no package'}")
        for pkg, patterns in packages.items():
            for pat in patterns:
                with self.subTest(format=name, package=pkg, pattern=pat):
                    p = pat.rstrip("/")
                    self.assertTrue(any(fnmatch.fnmatchcase(f, p) or f.startswith(p + "/") for f in self.files),
                                    f"{pat} matches nothing that is installed")
                    if p.startswith("etc/"):
                        self.assertEqual(p, "etc/pam.d/meadow-lock", "packages put nothing else in /etc")

    def test_fedora_spec(self):
        self.check_format("fedora", spec_packages())

    def test_arch_pkgbuild(self):
        self.check_format("arch", pkgbuild_packages())

    def test_debian_install_files(self):
        self.check_format("debian", debian_packages())


class NoInstallScripts(unittest.TestCase):
    def test_spec_has_no_scriptlets(self):
        text = SPEC.read_text()
        scriptlets = re.findall(r"^%(pre|post|preun|postun|pretrans|posttrans|trigger\w*|filetrigger\w*)\b",
                                text, re.M)
        self.assertEqual(scriptlets, [])

    def test_pkgbuild_has_no_install_file(self):
        text = PKGBUILD.read_text()
        self.assertNotRegex(text, r"(?m)^\s*install=")
        self.assertEqual(list(PKGBUILD.parent.glob("*.install")), [])

    def test_debian_has_no_maintainer_scripts(self):
        for kind in ("preinst", "postinst", "prerm", "postrm", "triggers", "config"):
            self.assertEqual(list(DEBIAN.glob(f"*{kind}")), [], kind)

    def test_nothing_mentions_login_changes(self):
        for f in (PKGBUILD, DEBIAN / "rules"):
            text = f.read_text()
            for word in ("authselect", "pam-auth-update", "systemctl enable", "sddm.conf.d/"):
                self.assertNotIn(word, text, f"{f.name} mentions {word}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
