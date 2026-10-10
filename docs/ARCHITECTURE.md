# Meadow architecture

This document is the high-level design. Exact metrics and colors live in
[VISUAL_SPEC.md](VISUAL_SPEC.md) and `theme/<scheme>/theme.json`.

## 1. Components

| Component | Kind | Runs as | Package |
|---|---|---|---|
| `libmeadow-common` + QML module `Meadow.Controls` | shared library + QML plugin | in-process for the greeter and the shell | `meadow-common` |
| Welcome screen (`greeter/`) | SDDM QML theme | `sddm-greeter-qt6`, user `sddm` | `sddm-theme-meadow` |
| `meadow-fprint-query` (phase 4) | tiny CLI, read-only | user `sddm`, run by the greeter | `sddm-theme-meadow` |
| Session (`meadow-session`, `meadow.desktop`) | shell script + session entry | the logged-in user | `meadow-shell` |
| labwc config + `Meadow-Luna` decoration theme | config/data | read by labwc | `meadow-shell`, `meadow-themes` |
| `meadow-shell` | Qt 6 / QML / C++ app | the logged-in user | `meadow-shell` |
| `meadow-shellctl` (phase 5) | CLI that calls `org.meadow.Shell` | run by labwc keybinds | `meadow-shell` |
| `meadow-setup` | root-only bash tool, opt-in | root, interactive | `meadow-shell` |
| GTK, Kvantum, icon, cursor and sound themes | data | read by apps | `meadow-themes` |

## 2. Process model

```
 systemd
  └─ sddm.service (root)                       ← enabled only via `meadow-setup enable-greeter`
       ├─ sddm-helper → PAM "sddm"             ← fingerprint line only via `meadow-setup enable-fingerprint`
       └─ sddm-greeter-qt6 (user sddm)
            ├─ greeter/Main.qml
            │    imports Meadow.Controls (libmeadow-common, /usr/lib64/qt6/qml/Meadow/Controls)
            └─ meadow-fprint-query  ──system bus──▶ net.reactivated.Fprint (ListEnrolledFingers only)

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
| system | `net.reactivated.Fprint` | meadow-fprint-query (greeter), LockScreen | enrolled-finger check (read-only) |
| session | `org.kde.StatusNotifierWatcher` / `…Item`, `com.canonical.dbusmenu` | TrayHost | tray icons and menus |
| session | `org.freedesktop.Notifications` | NotificationServer | balloon tips |
| session | `org.meadow.Shell` (ours) | ShellIpc | ToggleStartMenu, ShowRun, ShowDesktop, Lock, ShowLogOff, AltTab |

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
         └──▶ generators (phase 5/9): labwc themerc, GTK CSS variables, Kvantum colors
```

- `theme/schema.json` lists every token path and its type. It is generated from
  `luna-blue`, and `tools/theme-schema.py check` (a ctest) makes every scheme
  match it exactly.
- QML must not contain literal sizes or colors. Phase 2 adds a lint check.
- `MEADOW_THEME` selects the scheme. In dev mode (`-DMEADOW_DEV_MODE=ON`) a file
  watcher reloads the theme live (phase 2).
- **Construction rule:** `ThemeLoader` is deliberately not default-constructible.
  Qt prefers a default constructor over `create()` for `QML_SINGLETON` types, and
  that would give QML an empty, separate instance.

## 5. Source layout and build

| Path | Target | Notes |
|---|---|---|
| `common/` | `meadow-common` (SHARED, SONAME `.0`) + `meadow-commonplugin` | the QML module's qmldir is generated by `qt_add_qml_module` |
| `shell/src/**` | `meadow-shell-core` (STATIC) | all non-UI logic; unit tests link it |
| `shell/src/main.cpp`, `shell/qml/` | `meadow-shell` + QML module `Meadow.Shell` | `--preview`, `--theme`, `--exit-after-ms` |
| `greeter/` | installed as-is to `/usr/share/sddm/themes/meadow` | `helper/` built from phase 4 |
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
- `system/polkit/50-meadow.rules` is an example only and is never installed
  into `/etc`.

## 8. Risks

| Risk | Mitigation |
|---|---|
| A broken PAM stack locks users out | The package never touches PAM. `meadow-setup` uses markers, `.meadow-bak` backups and `pamtester`, and documents TTY recovery. PAM is tested only in the VM. |
| SDDM starts PAM only on `login()` | Auto-start the fingerprint conversation with `login(user, "")` when fingers are enrolled; empty Enter is the fallback (phase 4). |
| The labwc / LayerShellQt API changes | Wrap it in a small helper and pin minimum versions in the spec. |
| Headless CI can't show real Wayland | `--preview` + `QT_QPA_PLATFORM=offscreen`; labwc with `WLR_BACKENDS=headless` for screenshot tests. |
| Fedora's Qt logs to journald, so test failures go unseen | ctest sets `QT_FORCE_STDERR_LOGGING=1`, and the smoke test fails on QML errors. |
| Microsoft assets creep in | `tools/check-assets.sh` runs in ctest and CI. |

## 9. Roadmap

| Phase | Deliverable |
|---|---|
| 1 | Scaffold, docs, theme tokens, CMake, spec draft (this commit) |
| 2 | ThemeLoader complete, SoundPlayer, all Luna controls, gallery, golden screenshots |
| 3 | SDDM Welcome screen |
| 4 | Fingerprint login (PAM snippet, greeter UX, helper) |
| 5 | Session script, labwc config, decorations |
| 6 | Taskbar, tray, notifications, clock, volume/network/battery |
| 7 | Start menu, All Programs, Run, Log Off / Turn Off dialogs |
| 8 | Desktop, lock screen, Display Properties, Control Panel, Alt+Tab |
| 9 | GTK/Kvantum themes, icons, cursors, sounds, fontconfig |
| 10 | Generic installer, Arch/Debian packaging, performance, crash resilience |
| 11 | Fedora RPM, `meadow-setup`, SELinux, COPR/Packit, test VM |
| 12 | Verification and final report |
