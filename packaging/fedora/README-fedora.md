# Meadow on Fedora

Fedora (current stable and Rawhide; x86_64 and aarch64) is Meadow's primary
platform.

## Install from COPR (once published)

```sh
sudo dnf copr enable <owner>/meadow
sudo dnf install meadow
sudo meadow-setup enable-greeter        # opt-in: makes SDDM + Meadow the login screen
sudo meadow-setup enable-fingerprint    # opt-in: fingerprint login (see docs/PAM_FINGERPRINT.md)
```

Installing the packages does not change your login screen or PAM. Undo the
opt-in steps with `meadow-setup disable-fingerprint` and `disable-greeter`.

## Subpackages

| Package | Contents |
|---|---|
| `meadow` | metapackage |
| `meadow-common` | `libmeadow-common.so.0`, QML module `Meadow.Controls` |
| `meadow-shell` | `meadow-shell`, `meadow-session`, `meadow-setup`, session entry, labwc config, example system files |
| `sddm-theme-meadow` (noarch) | `/usr/share/sddm/themes/meadow` |
| `meadow-themes` (noarch) | scheme JSONs, GTK/Kvantum/labwc/icon/cursor/sound themes, fontconfig snippet |

## Build commands

```sh
# Local development build
sudo dnf builddep packaging/fedora/meadow.spec
cmake -B build -G Ninja && cmake --build build && ctest --test-dir build

# Local RPM build
tools/make-tarball.sh
rpmbuild -ba packaging/fedora/meadow.spec --define "_sourcedir $PWD"

# Clean chroot, current release and Rawhide
mock -r fedora-$(rpm -E %fedora)-x86_64 --rebuild ~/rpmbuild/SRPMS/meadow-*.src.rpm
mock -r fedora-rawhide-x86_64           --rebuild ~/rpmbuild/SRPMS/meadow-*.src.rpm
mock -r fedora-$(rpm -E %fedora)-aarch64 --rebuild ~/rpmbuild/SRPMS/meadow-*.src.rpm   # needs qemu-user-static

# Lint (target: zero errors; justify warnings in meadow.rpmlintrc)
rpmlint -r packaging/fedora/meadow.rpmlintrc packaging/fedora/meadow.spec ~/rpmbuild/RPMS/*/meadow*.rpm ~/rpmbuild/RPMS/noarch/sddm-theme-meadow*.rpm

# Install test
sudo dnf install ~/rpmbuild/RPMS/*/{meadow,sddm-theme-meadow}*.rpm
sudo meadow-setup --dry-run enable-greeter
```

## Versioning

`Version:` in the spec matches `project(VERSION)` in `CMakeLists.txt`. Release
tags are `vX.Y.Z`. `tools/make-tarball.sh` takes the version from the tag when
HEAD is tagged, and Packit updates the spec version from the tag for COPR builds.

## SELinux

Every file installs to a standard FHS path, so the default policy labels it
correctly. Meadow ships no policy module. For a manual (non-RPM) install, run
`sudo restorecon -Rv /usr/share/sddm/themes/meadow /usr/share/meadow /usr/bin/meadow-*`.
Check `ausearch -m AVC -ts recent` after testing.

## Test VM

`tools/vm/fedora-virt-install.sh` creates a VM from `tools/vm/fedora.ks`
(phase 11). Fingerprint testing uses a USB reader passed through in
virt-manager (Add Hardware → USB Host Device), or libfprint's virtual device
driver where the installed libfprint build includes it.
