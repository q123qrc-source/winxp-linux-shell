# Meadow

Meadow is a Wayland desktop for Linux that recreates the look and feel of the
classic early-2000s "Luna" desktop with entirely original artwork. It includes:

- **meadow-shell**: taskbar, green **start** button, two-column Start menu,
  notification area, desktop icons, Run/Log Off/Turn Off dialogs and a lock
  screen. Built with Qt 6, QML and LayerShellQt on the **labwc** compositor.
- **sddm-theme-meadow**: a Welcome screen for SDDM with user tiles, session and
  keyboard-layout switchers, fingerprint login and a Turn Off Computer dialog.
- **meadow-themes**: the Luna Blue, Olive Green and Silver color schemes, plus
  matching GTK 3/4, Kvantum, labwc, icon, cursor and sound themes.

Fedora is the primary target. Status: **phase 3 of 12 (SDDM Welcome screen)**. See
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the design and the roadmap.

## Building

```sh
sudo dnf builddep packaging/fedora/meadow.spec
cmake -B build -G Ninja
cmake --build build
ctest --test-dir build
```

Run the shell in a normal window without root or layer-shell:

```sh
MEADOW_THEME_DIR=$PWD/theme QML_IMPORT_PATH=$PWD/build/qml build/shell/meadow-shell --preview
```

Try the Welcome screen without SDDM:

```sh
MEADOW_THEME_DIR=$PWD/theme MEADOW_ASSET_DIR=$PWD/theme/assets QML_IMPORT_PATH=$PWD/build/qml \
    build/tools/greeter-preview/meadow-greeter-preview --size 1280x800
```

(The mock accepts the password `meadow`.) With SDDM installed, run it in the real
greeter with `sddm-greeter-qt6 --test-mode --theme greeter` and the same
variables.

Browse every control in every state: [tools/gallery](tools/gallery/README.md).

RPM builds, COPR and mock: [packaging/fedora/README-fedora.md](packaging/fedora/README-fedora.md).

## Safe by default

Installing Meadow never changes how your computer logs in. The display manager,
PAM and authselect change only when you run `sudo meadow-setup …`. Every change
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
example `meadow-hills.svg`, `logon.oga`). Meadow ships nothing in these folders.

Fonts work through fontconfig instead. If Tahoma or Trebuchet MS is installed,
the font lists in `theme.json` pick it automatically. Otherwise Noto Sans,
DejaVu Sans or Liberation Sans is used.

## Theming

Every size, color, gradient stop, radius, font and timing lives in one file:
`theme/<scheme>/theme.json`. A new color scheme is just a new JSON file with the
same keys, checked by `tools/theme-schema.py`. The token reference is
[docs/VISUAL_SPEC.md](docs/VISUAL_SPEC.md).

## License

Code: GPL-3.0-or-later. Original artwork and sounds: CC-BY-SA-4.0. See
[LICENSE](LICENSE).
