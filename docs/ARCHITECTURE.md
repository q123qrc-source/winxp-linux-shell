# Meadow architecture

This document is the high-level design. Exact metrics and colors live in
[VISUAL_SPEC.md](VISUAL_SPEC.md) and `theme/<scheme>/theme.json`.

## 1. Components

| Component | Kind | Runs as | Package |
|---|---|---|---|
| `libmeadow-common` + QML module `Meadow.Controls` | shared library + QML plugin | in-process for the greeter and the shell | `meadow-common` |
| Welcome screen (`greeter/`) | SDDM QML theme | `sddm-greeter-qt6`, user `sddm` | `sddm-theme-meadow` |
| `FingerprintQuery` (in libmeadow-common) | read-only fprintd query | in the greeter (user `sddm`) | `meadow-common` |
| Session (`meadow-session`, `meadow.desktop`) | shell script + session entry | the logged-in user | `meadow-shell` |
| labwc config + `Meadow-Luna` decoration theme | config/data | read by labwc | `meadow-shell`, `meadow-themes` |
| `meadow-shell` | Qt 6 / QML / C++ app | the logged-in user | `meadow-shell` |
| `meadow-shellctl` | sh + `busctl`, calls `org.meadow.Shell` | run by labwc keybinds | `meadow-shell` |
| `meadow-setup` | root-only Python tool (stdlib), opt-in | root, interactive | `meadow-shell` |
| GTK, Kvantum, icon, cursor and sound themes | data | read by apps | `meadow-themes` |

## 2. Process model

```
 systemd
  └─ sddm.service (root)                       ← enabled only via `meadow-setup enable-greeter`
       ├─ sddm-helper → PAM "sddm"             ← fingerprint line only via `meadow-setup enable-fingerprint`
       └─ sddm-greeter-qt6 (user sddm)
            ├─ greeter/Main.qml
            │    imports Meadow.Controls (libmeadow-common, /usr/lib64/qt6/qml/Meadow/Controls)
            └─ FingerprintQuery ──system bus──▶ net.reactivated.Fprint (GetDefaultDevice, ListEnrolledFingers)

 user session "Meadow"  (/usr/share/wayland-sessions/meadow.desktop)
  └─ meadow-session  (exports XDG_CURRENT_DESKTOP=Meadow, toolkit theme vars)
       └─ labwc --config-dir ~/.config/meadow/labwc | /usr/share/meadow/labwc
            ├─ XWayland (on demand)
            ├─ autostart
            │    ├─ xdg-desktop-portal (+ -wlr, -gtk per meadow-portals.conf)
            │    └─ restart loop ─▶ meadow-shell
            └─ keybinds ─▶ meadow-shellctl ──session bus──▶ org.meadow.Shell

 meadow-shell (one process, several Wayland surfaces)
  ├─ layer "bottom" (exclusive zone 30px) : Taskbar (start button, quick launch, tasks, tray, clock)
  ├─ layer "background"                   : Desktop (wallpaper, icons)
  ├─ layer "overlay"                      : Start menu, All Programs, tray menus, balloons,
  │                                         volume/calendar popups, Alt+Tab, grayscale fade
  ├─ xdg toplevels                        : Run, Display Properties, Control Panel, polkit dialogs
  └─ ext-session-lock surfaces            : LockScreen (PAM service "meadow-lock")
```

The greeter and the shell never share a process. They share code only through
`libmeadow-common` and the `Meadow.Controls` QML module. The greeter only
**reads** data: it never writes into user home directories, and the only system
bus call it makes is the read-only fprintd query.

## 3. Interfaces

### Wayland protocols (client side, in meadow-shell)

| Protocol | Use | Via |
|---|---|---|
| `zwlr_layer_shell_v1` | taskbar, desktop, popups | LayerShellQt |
| `zwlr_foreign_toplevel_manager_v1` | task buttons, activate, minimize, maximize, close | `ForeignToplevelModel` (qtwaylandscanner) |
| `ext_session_lock_manager_v1` | lock screen | `SessionLock` |
| `zwlr_output_manager_v1` | Display Properties → Settings tab | phase 8 |
| `xdg_decoration` | server-side Luna decorations from labwc | labwc |

### D-Bus

| Bus | Name / interface | Used by | Purpose |
|---|---|---|---|
| system | `org.freedesktop.login1` | PowerService, SessionService | PowerOff/Reboot/Suspend/Hibernate + CanXxx; Lock/Unlock signals |
| system | `org.freedesktop.DisplayManager` | SessionService | SwitchToGreeter (Switch User) |
| system | `org.freedesktop.UPower` | BatteryService | battery icon/percentage |
| system | `org.freedesktop.NetworkManager` | NetworkService | network icon/menu |
| system | `org.freedesktop.Accounts` | Start menu header, Control Panel | real name, avatar |
| system | `org.freedesktop.PolicyKit1` | PolkitAgent (polkit-qt6) | authentication dialogs |
| system | `net.reactivated.Fprint` | FingerprintQuery (greeter), LockScreen | reader and enrolled-finger check (read-only) |
| session | `org.kde.StatusNotifierWatcher` / `…Item`, `com.canonical.dbusmenu` | TrayHost | tray icons and menus |
| session | `org.freedesktop.Notifications` | NotificationServer | balloon tips |
| session | `org.meadow.Shell` (ours, `shell/dbus/org.meadow.Shell.xml`) | ShellIpc | ToggleStartMenu, ShowRunDialog, ShowDesktop, Lock, ShowLogOffDialog, ShowTurnOffDialog, Version |

Audio uses PipeWire/WirePlumber. Phase 6 starts with `wpctl` plus change
monitoring, and moves to libpipewire only if profiling shows a need.

### SDDM QML API (greeter)

`sddm` (login, powerOff, reboot, suspend, hibernate, canXxx, loginFailed,
loginSucceeded), `userModel` (+ `lastIndex`), `sessionModel` (+ `lastIndex`),
`keyboard` (layouts, currentLayout, capsLock), `screenModel`, and `config`
(`theme.conf`).

## 4. Theme pipeline

```
theme/<scheme>/theme.json ──▶ ThemeLoader (C++) ──▶ QML singleton `Theme` (import Meadow.Controls)
         │                       ├─ search: $MEADOW_THEME_DIR, ~/.local/share/meadow/themes, /usr/share/meadow/themes
         │                       └─ resolveAsset(): ~/.local/share/meadow/assets-override → /usr/share/meadow/assets-override → bundled
         └──▶ generators: labwc themerc + button SVGs + rc.xml (tools/gen-labwc-theme.py, committed, sync-tested);
              GTK CSS variables and Kvantum colors (phase 9)
```

- `theme/schema.json` lists every token path and its type. It is generated from
  `luna-blue`, and `tools/theme-schema.py check` (a ctest) makes every scheme
  match it exactly.
- QML must not contain literal sizes or colors. Phase 2 adds a lint check.
- `MEADOW_THEME` selects the scheme. In dev mode (`-DMEADOW_DEV_MODE=ON`) a file
  watcher reloads the theme live (phase 2).
- `Theme.tokens` is one JS object built per load, so bindings read plain JS
  properties rather than converting a `QVariantMap` on every access.
  `Theme.px(n)` scales and rounds a metric. `Theme.fonts.<role>` is a `QFont`
  for the first *installed* family in the role's list. `Theme.duration(name)`
  returns 0 under reduced motion (`animation.reducedMotion` or
  `MEADOW_REDUCED_MOTION=1`). Every property notifies through `themeChanged`.
- Environment: `MEADOW_THEME` (scheme), `MEADOW_THEME_DIR` (source-tree
  themes), `MEADOW_SCALE`, `MEADOW_THEME_LIVE=1` (watch the file), and
  `MEADOW_MUTE=1` / `MEADOW_SOUND_DIR` for SoundPlayer.
- **One `Theme` per engine, one loader per process.** `ThemeLoader` (C++)
  holds the scheme, scale and fonts for the whole process. QML's `Theme` is
  a `ThemeObject` that each engine creates through `create()`, and it
  forwards to the loader. This split matters because SDDM creates one view,
  and so one engine, per screen, and the JS token object belongs to exactly
  one engine. `ThemeObject` has no default constructor on purpose: Qt would
  prefer one over `create()` and hand QML an unconnected instance.
- `Theme.asset(name)` resolves bundled artwork (`/usr/share/meadow/assets`, or
  `MEADOW_ASSET_DIR` in a source tree) after the override folders.
  `Theme.moduleAvailable(uri)` lets optional features (the on-screen
  keyboard) appear only when their QML module is installed.

### Controls

`Meadow.Controls` builds on `QtQuick.Templates`, so keyboard handling, focus
and accessibility come from Qt, while all visuals come from tokens. Three C++
painted items carry the drawing that QML can't express from data on Qt 6.5:

| Item | Purpose |
|---|---|
| `GradientBox` | fill from a token gradient list, per-corner radii, solid or dotted border |
| `LunaGlyph` | vector glyphs: check, arrows, chevrons, caption buttons, info/warning/error/question badges |
| `BalloonShape` | balloon body with a slanted tail (top or bottom) |
| `MeadowLogo` | the original logo: four glossy rounded panes in a diamond (sun, sky, grass, leaves); a monochrome variant for high contrast |
| `UserAccount` | read-only AccountsService lookup (real name, password hint, icon); unprivileged, async, empty when the service is absent |
| `FingerprintQuery` | read-only fprintd lookup: reader present, enrollment Enrolled/NotEnrolled/Unknown; `MEADOW_FINGERPRINT_SIMULATE` replaces D-Bus in tests |

The QML controls are LunaButton, LunaTextField, LunaCheckBox, LunaComboBox,
LunaScrollBar, LunaTooltip, BalloonTip, LunaWindowFrame, LunaDialog,
GlossyGradient and ShadowText. Each interactive control has a `previewState`
property ("hover", "pressed", "focus") that forces a visual state for the
gallery and screenshot tests. It never changes behavior.

### Welcome screen (greeter)

```
Main.qml                     SDDM context → scheme + scale from theme.conf; primary vs secondary screen
└─ WelcomeLayout             bands, dividers, radial glow, logo + hint, user list, bottom band, overlays
   ├─ UserTile ×n            Avatar + name; selected → PasswordPanel (field, green arrow, "?")
   ├─ TurnOffButton → TurnOffDialog   Stand By / Turn Off / Restart (Shift: Hibernate), hidden when sddm.canX is false
   ├─ SessionSwitcher, KeyboardLayoutSwitcher, on-screen keyboard and high-contrast pills
   ├─ CapsLockWarning, MessageBalloon   BalloonTips anchored under the password field
   └─ FingerprintIndicator   (phase 4)
```

- **Login flow:** select a tile (click, or Enter on the keyboard-highlighted
  tile), type the password, then Enter or the arrow calls
  `sddm.login(user, password, session)`. `loginFailed` shakes the field,
  clears it and shows "Did you forget your password?" with the
  AccountsService hint when one is set. `loginSucceeded` plays the logon sound
  and shows "welcome" until SDDM closes the view. A user without a password
  logs on when selected. Up/Down move between users and open the next one
  while one is selected. Esc goes back. Alt+Shift switches keyboard layout.
- **Scale:** `scale=auto` uses the screen's *logical* DPI snapped to 1, 1.25,
  1.5 or 2. A device pixel ratio set by the compositor is already applied by
  Qt, so it is not applied twice: a 2× output renders with scale 1 and DPR 2.
- **Multi-monitor:** SDDM loads `Main.qml` once per screen. Only the
  `primaryScreen` instance shows the UI; the others draw the background.
- **Avatars:** `~/.face.icon` or the AccountsService icon (both resolved by
  SDDM's `userModel`). SDDM's generic `…/faces/.face.icon` and unreadable
  pictures fall back to one of four original avatars, chosen stably from the
  user name.
- **Fingerprint:** `FingerprintQuery` asks fprintd read-only whether a
  reader exists and whether the selected user has enrolled fingers. Without the
  optional polkit grant the answer is "unknown" for the unprivileged greeter.
  A reading is a `sddm.login(user, "")`, with pam_fprintd first in the PAM
  stack. A password typed meanwhile is queued, because SDDM drops overlapping
  logins. Details: [PAM_FINGERPRINT.md](PAM_FINGERPRINT.md).
- **Power:** the dialog calls `sddm.suspend/hibernate/powerOff/reboot` and logs
  each request under `meadow.greeter.power`. SDDM's `--test-mode` reports no
  capabilities, so `debugPowerButtons=true` in `theme.conf` shows them all for
  testing.
- **Logging:** `meadow.greeter`, `meadow.greeter.auth` (user name and outcome
  only, never the password) and `meadow.greeter.power`.
- **Preview harness:** `tools/greeter-preview` loads the theme with C++ stand-ins
  for SDDM 0.21's `sddm`, `userModel`, `sessionModel`, `keyboard`,
  `screenModel` and `config`, with the same property, role and signal names.
  It drives the screenshot and flow tests. `tools/greeter-sddm-smoke.sh` and
  `tools/greeter-real-screenshot.sh` check the theme in the real
  `sddm-greeter-qt6`.

### Session (labwc)

```
/usr/share/wayland-sessions/meadow.desktop   Exec=meadow-session, DesktopNames=Meadow
meadow-session        exports XDG_CURRENT_DESKTOP=Meadow and the toolkit theme variables; keyboard layout from
                      localectl's X11 keymap; exec labwc --config-dir ~/.config/meadow/labwc | /usr/share/meadow/labwc
  rc.xml              generated from rc.xml.in (fonts and corner radius from tokens): server-side decorations,
                      click-to-focus with raise, automatic placement, icon:iconify,max,close, classic Alt+Tab OSD
  menu.xml            window menu (title-bar right-click, window icon, Alt+Space); no root menu, the shell owns the desktop
  autostart           exports the environment to D-Bus/systemd; meadow-shell in a crash loop with back-off
                      (stops after 5 crashes in 60 s); MEADOW_NO_SHELL=1 for a bare compositor
  environment         optional XKB overrides
/usr/share/themes/Meadow-Luna{,-Olive,-Silver}/labwc/   generated decorations (see VISUAL_SPEC §3)
```

| Key | Action |
|---|---|
| Win (alone) / Ctrl+Esc | `meadow-shellctl toggle-start-menu` |
| Win+D / Win+R / Win+L | `meadow-shellctl show-desktop` / `run` / `lock` |
| Win+E | `pcmanfm-qt` |
| Win+M | minimize all windows (labwc `ForEach` → `Iconify`) |
| Win+Up / Down / Left / Right | maximize / restore / snap left / snap right |
| Alt+Tab, Alt+Shift+Tab | labwc window switcher (interim, phase 8 replaces it) |
| Alt+F4 / Alt+Space | close / window menu |
| Ctrl+Alt+Delete | `meadow-shellctl log-off` |
| Volume keys | `wpctl` |

labwc has no show-desktop action, so Win+D needs the shell (phase 6,
foreign-toplevel). `meadow-shellctl` exits 1 with "meadow-shell is not
running" when no shell owns `org.meadow.Shell`. `lock` then falls back to
`loginctl lock-session`.

## 5. Source layout and build

| Path | Target | Notes |
|---|---|---|
| `common/` | `meadow-common` (SHARED, SONAME `.0`) + `meadow-commonplugin` | the QML module's qmldir is generated by `qt_add_qml_module` |
| `shell/src/**` | `meadow-shell-core` (STATIC) | all non-UI logic; unit tests link it |
| `shell/src/main.cpp`, `shell/qml/` | `meadow-shell` + QML module `Meadow.Shell` | `--preview`, `--theme`, `--exit-after-ms` |
| `greeter/` | installed as-is to `/usr/share/sddm/themes/meadow` | `helper/` built from phase 4 |
| `tools/gallery/` | `meadow-gallery` + `meadow-imgdiff` (not installed) | control gallery and screenshot comparison |
| `tools/greeter-preview/` | `meadow-greeter-preview` (not installed) | Welcome screen with mock SDDM objects |
| `tools/decor-test/` | `meadow-decor-test` (not installed) | plain titled windows for decoration screenshots |
| `decorations/labwc/` | installed to `/usr/share/themes/<name>/labwc` | generated; see `tools/gen-labwc-theme.py` |
| `theme/assets/` | installed to `/usr/share/meadow/assets` | original logo and avatar SVGs (CC-BY-SA-4.0) |
| `tests/` | Qt Test executables + Python checks | `MEADOW_THEME_DIR` points at the source tree |

Optional dependencies (`Qt6::WaylandClient`, `LayerShellQt`, `Qt6::Multimedia`,
`Qt6::LinguistTools`) are detected now and become required in the phase that
first uses them. The Fedora spec already BuildRequires all of them.

## 6. Configuration and state

| What | Where | Written by |
|---|---|---|
| Shell settings | `~/.config/meadow/shell.conf` (defaults in `/usr/share/meadow/`) | shell |
| labwc config (user copy) | `~/.config/meadow/labwc/` | user |
| MFU / pinned apps | `~/.local/share/meadow/` | shell |
| SDDM drop-in | `/etc/sddm.conf.d/10-meadow.conf` | `meadow-setup` only |
| PAM | `/etc/pam.d/sddm` (marked block), `/etc/pam.d/meadow-lock` | `meadow-setup` only |

## 7. Security model

- The greeter runs as `sddm` with no extra privileges. The fprintd helper only
  calls `ListEnrolledFingers`.
- There are no setuid binaries. Power actions go through logind and polkit.
- Passwords and PAM messages are never logged. Log categories follow the
  pattern `meadow.<component>.<area>`.
- Everything targets SELinux enforcing on Fedora. Files go to standard FHS
  paths so the default policy labels them, and no custom policy module is
  planned.
- `system/polkit/50-meadow.rules` (let the greeter see who has enrolled
  fingers) reaches `/etc` only through `meadow-setup enable-fingerprint
  --allow-greeter-query`. It never grants enrolling or deleting.

## 8. Risks

| Risk | Mitigation |
|---|---|
| A broken PAM stack locks users out | The package never touches PAM. `meadow-setup` uses markers, `.meadow-bak` backups and `pamtester`, and documents TTY recovery. PAM is tested only in the VM. |
| SDDM creates one QML engine per screen | `Theme` is a per-engine object over one shared loader; a test covers two engines. |
| SDDM starts PAM only on `login()` | Auto-start the fingerprint conversation with `login(user, "")` when fingers are enrolled; empty Enter is the fallback (phase 4). |
| The labwc / LayerShellQt API changes | Wrap it in a small helper and pin minimum versions in the spec. |
| Headless CI can't show real Wayland | `--preview` + `QT_QPA_PLATFORM=offscreen`; labwc with `WLR_BACKENDS=headless` for screenshot tests. |
| Fedora's Qt logs to journald, so test failures go unseen | ctest sets `QT_FORCE_STDERR_LOGGING=1`, and the smoke test fails on QML errors. |
| Microsoft assets creep in | `tools/check-assets.sh` runs in ctest and CI. |

## 9. Roadmap

| Phase | Deliverable |
|---|---|
| 1 | Scaffold, docs, theme tokens, CMake, spec draft (done) |
| 2 | ThemeLoader complete, SoundPlayer, all Luna controls, gallery, golden screenshots (done) |
| 3 | SDDM Welcome screen (done) |
| 4 | Fingerprint login (PAM snippet, greeter UX, meadow-setup fingerprint commands) (done) |
| 5 | Session script, labwc config, decorations, shell IPC (done) |
| 6 | Taskbar, tray, notifications, clock, volume/network/battery |
| 7 | Start menu, All Programs, Run, Log Off / Turn Off dialogs |
| 8 | Desktop, lock screen, Display Properties, Control Panel, Alt+Tab |
| 9 | GTK/Kvantum themes, icons, cursors, sounds, fontconfig |
| 10 | Generic installer, Arch/Debian packaging, performance, crash resilience |
| 11 | Fedora RPM, `meadow-setup`, SELinux, COPR/Packit, test VM |
| 12 | Verification and final report |
