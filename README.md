# Meadow

Meadow is a Wayland desktop for Linux that recreates the look and feel of the
classic early-2000s "Luna" desktop with entirely original artwork. It includes:

- **meadow-shell**: taskbar, green **start** button, two-column Start menu,
  notification area, desktop with icons, Run/Log Off/Turn Off dialogs, a lock
  screen and screen saver, Alt+Tab, Display Properties and a Control Panel.
  Built with Qt 6, QML and LayerShellQt on the **labwc** compositor.
- **sddm-theme-meadow**: a Welcome screen for SDDM with user tiles, session and
  keyboard-layout switchers, fingerprint login and a Turn Off Computer dialog.
- **meadow-themes**: the Luna Blue, Olive Green and Silver color schemes, plus
  matching GTK 3/4, Kvantum and labwc themes for each scheme (generated from
  the scheme files), and original icon, cursor and sound themes
  (`meadow-icons`, `meadow-cursors`, `meadow`). Inside a Meadow session,
  GTK and Qt applications pick them up automatically; other sessions are not
  affected.

Fedora is the primary target. Status: **phase 10 of 12 (installers, packaging, performance and crash resilience)**. See
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the design and the roadmap.

## Installing

**Fedora**: use the RPM packages ([packaging/fedora](packaging/fedora/README-fedora.md)).

**Arch Linux**: `makepkg -s` in [packaging/arch](packaging/arch/PKGBUILD) with a
release tarball (`tools/make-tarball.sh packaging/arch`).

**Debian / Ubuntu**: `cp -r packaging/debian . && dpkg-buildpackage -b -us -uc`.
Meadow needs labwc 0.9 or newer, which Debian 13 does not have yet.

**Anything else** (or to try Meadow from a checkout), the generic installer:

```sh
./install.sh --dry-run          # show every step, change nothing
./install.sh                    # build, then install to /usr/local (asks for sudo)
./install.sh --enable-greeter   # also make the Meadow Welcome screen SDDM's theme
./uninstall.sh                  # undo all of it
```

It checks dependencies first and prints the `dnf`/`pacman`/`apt` line for
anything missing. Options: `--prefix DIR`, `--no-pam`, `--build-dir DIR`,
`--root DIR` (image builds) and `DESTDIR=` (staging). It installs only into
the prefix; the few changes outside it (the session entry, links that let
SDDM and Qt find a `/usr/local` install, the lock screen's PAM service and,
with `--enable-greeter`, SDDM's theme setting) are made by `meadow-setup`,
which backs up and records each one. `uninstall.sh` reverts them and removes
exactly the files listed in the install manifest, keeping any you changed.
The display manager is never switched: `meadow-setup enable-greeter` prints
the command for that. On Fedora the installer works too, but the RPMs are
the supported way.

## Building

```sh
sudo dnf builddep packaging/fedora/meadow.spec
cmake -B build -G Ninja
cmake --build build
ctest --test-dir build
```

Run the taskbar in a normal window, without root or layer-shell, with sample
windows (add `--mocks --sni` for volume/network/battery and a tray icon):

```sh
tools/shell-preview.sh build -- --preview-windows 10
tools/shell-preview.sh build --recent -- --open start    # Start menu
```

In the preview, Log Off and Turn Off only log what they would do, and Start
menu items are recorded instead of started (set `MEADOW_LAUNCH_LOG=` to start
them).

Try the Welcome screen without SDDM:

```sh
MEADOW_THEME_DIR=$PWD/theme MEADOW_ASSET_DIR=$PWD/theme/assets QML_IMPORT_PATH=$PWD/build/qml \
    build/tools/greeter-preview/meadow-greeter-preview --size 1280x800
```

(The mock accepts the password `meadow`.) With SDDM installed, run it in the real
greeter with `sddm-greeter-qt6 --test-mode --theme greeter` and the same
variables.

Try the session nested in your current Wayland desktop, straight from the
build tree, or headless with a screenshot:

```sh
tools/run-session.sh                      # installed: WLR_BACKENDS=wayland meadow-session
tools/session-screenshot.sh --grab session.png --xterm
```

Browse every control in every state: [tools/gallery](tools/gallery/README.md).

RPM builds, COPR and mock: [packaging/fedora/README-fedora.md](packaging/fedora/README-fedora.md).

## Safe by default

Installing Meadow never changes how your computer logs in. The display manager,
PAM and authselect change only when you run `sudo meadow-setup …` (or
`install.sh --enable-greeter`). The package
adds one PAM file, `/etc/pam.d/meadow-lock`: a password-only service that only
Meadow's lock screen reads. Application defaults
(icon, cursor, font and sound themes) are applied per session only: nothing is
written to your home directory or to `/etc`. Every change
is backed up and can be undone with the matching `disable-*` command. See
[docs/PAM_FINGERPRINT.md](docs/PAM_FINGERPRINT.md) for recovery steps.

## Asset policy

Meadow ships **no Microsoft assets**: no bitmaps from luna.msstyles, no Bliss
wallpaper, no Windows flag, no original sounds, no Tahoma, Trebuchet MS or
Franklin Gothic font files, and no "Windows" wordmark. Every visual is drawn
procedurally in QML or as original SVG, and the sounds are synthesized. The logo
is an original four-pane "meadow" mark. `tools/check-assets.sh` enforces this
in CI.

### Using assets you own

If you legally own assets you would like to use (wallpapers, fonts, sounds), put
them in an override folder. Meadow prefers a file there over its bundled file
with the same name:

| Folder | Scope |
|---|---|
| `~/.local/share/meadow/assets-override/` | your user |
| `/usr/share/meadow/assets-override/` | all users (create it yourself) |

File names follow `assets` and `sounds` in `theme/<scheme>/theme.json` (for
example `meadow-hills.svg`, `desktop-login.oga`). Meadow ships nothing in these folders.

Fonts work through fontconfig instead. If Tahoma or Trebuchet MS is installed,
the font lists in `theme.json` pick it automatically. Otherwise Noto Sans,
DejaVu Sans or Liberation Sans is used, and inside Meadow sessions the classic
names (Tahoma, Verdana, MS Shell Dlg, Lucida Console, ...) are aliased to them
for other applications too (`system/fontconfig/50-meadow.conf`).

## Theming

Every size, color, gradient stop, radius, font and timing lives in one file:
`theme/<scheme>/theme.json`. A new color scheme is just a new JSON file with the
same keys, checked by `tools/theme-schema.py`. The token reference is
[docs/VISUAL_SPEC.md](docs/VISUAL_SPEC.md). The GTK and Kvantum themes are
generated from these files at build time, so a new scheme gets them too; see
[toolkit-themes/README.md](toolkit-themes/README.md).

## License

Code: GPL-3.0-or-later. Original artwork and sounds: CC-BY-SA-4.0. See
[LICENSE](LICENSE).
