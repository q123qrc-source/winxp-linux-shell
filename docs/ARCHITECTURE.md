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
  └─ meadow-session  (exports XDG_CURRENT_DESKTOP=Meadow:X-Meadow, toolkit theme vars)
       └─ labwc --config-dir ~/.config/meadow/labwc | /usr/share/meadow/labwc
            ├─ XWayland (on demand)
            ├─ autostart
            │    ├─ xdg-desktop-portal (+ -wlr, -gtk per meadow-portals.conf)
            │    └─ meadow-shell-supervise ─▶ meadow-shell (restarted after a crash)
            └─ keybinds ─▶ meadow-shellctl ──session bus──▶ org.meadow.Shell

 meadow-shell (one process, several Wayland surfaces; shell/qml/Session.qml)
  ├─ layer "overlay", full screen, keyboard exclusive : Log Off / Turn Off over a gray screen capture
  ├─ xdg toplevel                                  : Run, Taskbar Properties
  ├─ layer "top", bottom edge, exclusive zone 30px : Taskbar (start, quick launch, tasks, language bar, tray, clock)
  ├─ layer "overlay", full screen, while open      : popups (menus, calendar, volume); keyboard exclusive;
  │                                                  a press outside every popup closes them
  ├─ layer "overlay", full screen, input-transparent: tooltips
  ├─ layer "overlay", bottom-right                 : notification balloons
  ├─ layer "background"                            : Desktop (wallpaper, icons; phase 8)
  ├─ (in the popups overlay)                       : Start menu, All Programs and its cascades
  ├─ layer "overlay"                               : Alt+Tab (phase 8)
  ├─ xdg toplevels                        : Run, Display Properties, Control Panel, polkit dialogs
  └─ ext-session-lock surfaces            : LockScreen (PAM service "meadow-lock")
```

The greeter and the shell never share a process. They share code only through
`libmeadow-common` and the `Meadow.Controls` and `Meadow.Welcome` QML modules
(the lock screen reuses the Welcome screen's layout). The greeter only
**reads** data: it never writes into user home directories, and the only system
bus call it makes is the read-only fprintd query.

## 3. Interfaces

### Wayland protocols (client side, in meadow-shell)

| Protocol | Use | Via |
|---|---|---|
| `zwlr_layer_shell_v1` | taskbar, desktop, popups | LayerShellQt |
| `zwlr_foreign_toplevel_manager_v1` | task buttons, activate, minimize, maximize, close | `ForeignToplevelModel` (qtwaylandscanner) |
| `ext_session_lock_manager_v1` | lock screen | `SessionLock` (own `QWaylandShellIntegration`, Qt private API) |
| `zwlr_output_manager_v1` (v4) | Display Properties → Settings tab | `OutputManager` |
| `ext_idle_notifier_v1` | screen saver | `IdleMonitor` (honours idle inhibitors) |
| `xdg_decoration` | server-side Luna decorations from labwc | labwc |

### D-Bus

| Bus | Name / interface | Used by | Purpose |
|---|---|---|---|
| system | `org.freedesktop.login1` | PowerService, SessionService | PowerOff/Reboot/Suspend/Hibernate (interactive, so polkit can ask) shown when CanXxx is "yes" or "challenge"; TerminateSession when not under labwc; Session `Lock`/`Unlock` signals, `SetLockedHint`; `PrepareForSleep` with a delay inhibitor (lock before suspend) |
| system | `org.freedesktop.timedate1` | TimeDateService | Date and Time applet: time zone list and change, NTP, set time (interactive authorization) |
| system | `org.freedesktop.DisplayManager` | SessionService | Seat (`$XDG_SEAT_PATH`) SwitchToGreeter for Switch User, hidden without it |
| session | `org.freedesktop.portal.FileChooser` | RunService | Run dialog Browse… |
| system | `org.freedesktop.UPower` | BatteryService | battery icon/percentage |
| system | `org.freedesktop.NetworkManager` | NetworkService | network icon/menu |
| system | `org.freedesktop.Accounts` | Start menu header, UserAccountsService | real name, avatar; User Accounts applet (own name and picture) |
| system | `org.freedesktop.PolicyKit1` | PolkitAgent (polkit-qt6) | the session's authentication agent; Luna "Authenticate" dialog |
| system | `net.reactivated.Fprint` | FingerprintQuery (greeter), LockScreen | reader and enrolled-finger check (read-only) |
| session | `org.kde.StatusNotifierWatcher` (provided by the shell unless another component does), `…Item`, `com.canonical.dbusmenu` | StatusNotifierWatcher, TrayModel | tray icons and menus |
| session | `org.freedesktop.Notifications` (spec 1.2; skipped if another daemon owns the name) | NotificationServer | balloon tips |
| session | `org.meadow.Shell` (ours, `shell/dbus/org.meadow.Shell.xml`) | ShellIpc | ToggleStartMenu, ShowRunDialog, ShowDesktop, Lock, ShowLogOffDialog, ShowTurnOffDialog, NextKeyboardLayout, SwitchWindow, ShowControlPanel, ShowDisplayProperties, IsLocked + LockedChanged, Version |

Audio uses PipeWire/WirePlumber through `wpctl` (`VolumeService` polls
`wpctl get-volume @DEFAULT_AUDIO_SINK@` every 3 s and runs `set-volume` /
`set-mute`); it moves to libpipewire only if profiling shows a need.

### Shell internals (phase 6)

| Piece | Where | Notes |
|---|---|---|
| `Shell` QML singleton | `shell/src/ui/ShellContext` | exposes `windows`, `settings`, `apps`, `tray`, `notifications`, `volume`, `network`, `battery`, `keyboard`, `ipc`, `now`, `startMenuOpen`, `preview` |
| `ShellSurface` | `shell/src/ui/ShellSurface` | makes a QML `Window` a layer surface (LayerShellQt); a no-op in `--preview` or off Wayland, so the same QML runs anywhere |
| Window list | `ToplevelModel` → `ForeignToplevelModel` (wlr-foreign-toplevel v3) / `PreviewToplevelModel` | taskbar click = activate or minimize the active window; Win+D minimizes all and restores them (active one last) |
| App lookup | `AppRegistry`, `DesktopEntry` | app_id → .desktop (exact, case-insensitive, StartupWMClass, last reverse-DNS part) → name and icon; `image://icon` draws a generic icon for unknown names |
| Tray | `StatusNotifierWatcher`, `TrayModel`, `TrayImageProvider` | SNI pixmaps/icon names, tooltips, Activate/SecondaryActivate/Scroll, DBusMenu `GetLayout` + `Event` |
| Notifications | `NotificationServer` | one balloon at a time (oldest first, at most 20 queued); body markup reduced to b/i/u/br; actions; urgency 2 and timeout 0 never expire; texts are never logged |
| Status icons | `VolumeService`, `NetworkService`, `BatteryService`, `TrayGlyph` | icons drawn procedurally; UPower `DisplayDevice`, NM `PrimaryConnection` |
| Language bar | `KeyboardLayoutService` | see below |
| Settings | `Settings` | `~/.config/meadow/shell.conf` over `/usr/share/meadow/shell.conf` |

**Keyboard layouts.** labwc has no IPC to query or switch the layout, and
xkb's own group switching would happen behind the shell's back. So the shell
owns the order: the first entry of `XKB_DEFAULT_LAYOUT` is the active layout.
`meadow-session` starts labwc from a per-session overlay directory
(`$XDG_RUNTIME_DIR/meadow/labwc-<pid>/`: symlinks to the real config plus a
writable `environment.d/`). Switching writes
`environment.d/90-meadow-keyboard.env` (layouts rotated, `grp:*` options
removed) and runs `labwc --reconfigure`. In labwc 0.9.6 that re-reads the
environment files and rebuilds each keyboard's keymap from `XKB_DEFAULT_*`
(`seat_reconfigure` → `keyboard_configure` → `set_layout`). Alt+Shift is an
`onRelease` keybind to `meadow-shellctl next-layout`. A side effect: each
switch also reloads rc.xml and the theme.

### Start menu and system dialogs (phase 7)

| Piece | Where | Notes |
|---|---|---|
| All Programs | `MenuParser`, `AppMenuModel` | XDG Desktop Menu Specification: Include/Exclude with Filename, Category, All, And, Or, Not; OnlyUnallocated, Deleted, MergeFile, MergeDir/DefaultMergeDirs, same-name merging, `.directory` names and icons. Meadow ships `meadow-applications.menu` (Accessories with System Tools and Accessibility, Education, Games, Graphics, Internet, Multimedia, Office, Programming, then unplaced programs); `~/.config/menus/meadow-applications.menu` or shell.conf `menuFile` replace it. NoDisplay, Hidden, OnlyShowIn/NotShowIn (`XDG_CURRENT_DESKTOP`, default Meadow:X-Meadow; Meadow's own entries use `OnlyShowIn=X-Meadow`) and TryExec are honored. Rebuilt live (application dirs and the menu file are watched, debounced 500 ms) |
| Pinned / frequent | `RecentAppsModel`, `UsageStore` | `~/.local/share/meadow/start-menu.json`: launch scores with a 7-day half-life, pins, removed entries, first-seen times. `meadow:internet` / `meadow:email` resolve through mimeapps.list (`x-scheme-handler/http`, `mailto`). Pin by right-click or by dragging from All Programs; reorder by dragging |
| New programs | `UsageStore::isNew` | first seen within `newProgramDays` and never launched; nothing is "new" on the first run |
| Places | `PlacesModel` | XDG user dirs; My Computer = `computer:///` in pcmanfm-qt; tools from shell.conf `[StartMenu]` (hidden when not installed); Recent Documents from `recently-used.xbel` (15 newest existing local files, watched) |
| Launching | `Launcher` | one place that starts processes: detached, in `$HOME`, never through a shell; folders via the configured file manager, documents/URLs via `xdg-open`. `MEADOW_LAUNCH_LOG` records instead of starting (tests) |
| Run | `RunService`, `RunDialog.qml` | existing paths, then URLs, then PATH programs with shell-style quoting (no expansion, pipes or redirection), then applications by id or name; history in `~/.local/share/meadow/run-history` (26); inline completion; Browse… through the FileChooser portal |
| Log Off / Turn Off | `SessionService`, `PowerService`, `SystemDialogHost.qml` | the screen is captured with `grim -o <output>` and cross-faded to gray (`dialogs.grayscaleFadeMs`); without grim a dark overlay. Log Off asks labwc to exit (`labwc --exit`); in `--preview` both services only simulate |

Keyboard in the Start menu: arrows within a column, Left/Right between
columns, Right/Enter on All Programs or My Recent Documents to open them,
Tab cycles columns and footer, a letter selects the next item starting with
it (also in every cascade), Escape closes one level at a time.

**Popups.** Menus, the calendar and the volume slider live in one PopupHost
inside a full-screen overlay surface that maps only while a popup is open.
This gives XP's "click anywhere else closes the menu" without compositor grabs.
The cost is that the closing click is consumed rather than passed on (XP passes
it through).

### Desktop, lock screen and settings (phase 8)

| Piece | Where | Notes |
|---|---|---|
| Desktop | `DesktopModel`, `Desktop.qml` on a background layer surface per screen | Wallpaper: the original "Meadow Hills" SVG (`tools/gen-wallpaper.py`, checked by the `wallpaper_sync` test), any picked image, or a solid colour; Center / Tile / Stretch (Stretch fills and crops, it never distorts). Icons on the primary screen only: My Computer, My Documents, Recycle Bin and the XDG Desktop folder (watched). Positions snap to the icon grid and persist in `~/.local/share/meadow/desktop-layout.json`. `.desktop` launchers run only when trusted (executable, or the user's own), as file managers do. New Folder / Text Document / Shortcut, rename, move to trash (`QFile::moveToTrash`), Empty Recycle Bin |
| Lock | `SessionLock`, `LockAuthenticator`, `AuthSession`, `meadow-auth-helper`, `LockScreen.qml` | States Unlocked → Locking → Locked. One lock surface per screen; the primary screen shows `Meadow.Welcome`'s WelcomeLayout in lock mode (the same QML as the greeter), the others the welcome background. `$XDG_RUNTIME_DIR/meadow/locked` exists while locked; a restarted shell that finds it locks at once (labwc keeps an abandoned lock in place). Triggers: Win+L / `meadow-shellctl lock`, logind Lock, before suspend, password-protected screen saver. logind Unlock unlocks (console recovery). PAM: see `PAM_FINGERPRINT.md` |
| Auth helper protocol | `shell/auth-helper/` | Frames `<TYPE> <len>\n<payload>`; helper → shell `PROMPT_ECHO_OFF`, `PROMPT_ECHO_ON`, `INFO`, `ERROR`, `RESULT ok\|fail`; shell → helper `RESPONSE`, `CANCEL`. Exit 0/1/2. Not setuid; authenticates `getuid()` only |
| Screen saver | `IdleMonitor`, `ScreenSaverController.qml`, `ScreenSaver.qml` | `[ScreenSaver] mode` (none, blank, logo), `timeoutMin`, `passwordProtect`, `lockOnSuspend`. An overlay surface per screen; input after a short grace period ends it, and a protected saver locks first |
| Alt+Tab | `ToplevelModel::mruRows`, `AltTabSwitcher.qml` | labwc binds A-Tab / A-S-Tab to `meadow-shellctl switch-window next\|prev`. The switcher is an overlay surface with exclusive keyboard focus: Tab/Shift+Tab/arrows move, releasing Alt activates, Esc cancels. A quick tap (Alt already up) switches to the previous window without showing anything. W-Tab stays bound to labwc's own switcher as a fallback when the shell isn't running |
| Display Properties | `DisplayProperties.qml`, `OutputManager` | Themes, Desktop, Screen Saver, Appearance, Settings tabs. Resolution and scale: wlr-output-management test, then apply, then "Keep these settings?" with a 15 s automatic revert |
| Control Panel | `ControlPanel.qml`, applets | Category and classic views; Mouse, Keyboard, Sounds and Audio Devices, Date and Time, User Accounts, Network Connections. Runs inside meadow-shell (`meadow-control-panel [page]` → `meadow-shellctl control-panel`) so it shares the shell's services |
| Runtime labwc config | `LabwcConfig` | `meadow-session` exports `MEADOW_LABWC_OVERLAY` and `MEADOW_LABWC_SOURCE`. The shell writes `overlay/rc.xml` (a copy of the real rc.xml with the decoration theme for the colour scheme, keyboard repeat, double-click time, pointer speed and left-handed overridden) through a temporary file and `rename()`, replacing the overlay symlink, then runs `labwc --reconfigure`. The user's and the packaged config are never written |
| Polkit agent | `PolkitAgent`, `AuthenticateDialog.qml` | polkit-qt6 Listener registered for the session's subject (skipped outside a logind session, or when another agent owns it). Wrong passwords ask again; the password is never logged |

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
              GTK 3/4 CSS + images and Kvantum kvconfig + SVG for every scheme, and the
              session's application defaults (tools/gen-toolkit-themes.py, at build time)

tools/gen-icons.py ───▶ meadow-icons (SVG, 48-unit + 16 px, committed, icons_sync)
tools/gen-cursors.py ─▶ cursor SVG sources (committed, cursors_sync) ─▶ gen-cursors.sh (rsvg-convert +
                        xcursorgen, at build time) ─▶ meadow-cursors
tools/gen-sounds.py ──▶ meadow sound theme (.oga, committed; PCM hashes in pcm.sha256, sounds_sync)
```

### Toolkit themes and application defaults (phase 9)

| Part | How |
|---|---|
| GTK 3 / GTK 4 | `toolkit-themes/gtk.css.in` imports the toolkit's own default theme, then restyles the common controls as Luna ones. Placeholders such as `@{g:controls.button.gradient.normal}` are filled from the scheme's tokens; `--lint` (ctest `toolkit_themes`) rejects literal colors and pixel sizes. Check boxes, radio buttons, arrows, grips and expanders are SVGs drawn from the same tokens into `assets/`. One theme per scheme (`toolkit.gtkThemeName`). |
| Kvantum (Qt) | `toolkit-themes/kvantum/MeadowLuna.kvconfig.in` plus an SVG drawn element by element (nine-patch frames, indicators, the green progress block pattern). One theme per scheme (`toolkit.kvantumThemeName`). |
| Icons | `meadow-icons`: about 170 original icons and 220 alias symlinks (places, devices, mimetypes, apps and categories, actions, status), each in a full 48-unit drawing and a simplified 16 px one. `Inherits=Adwaita,hicolor`. The shell uses it through `assets.iconTheme`. |
| Cursors | `meadow-cursors`: 26 cursors (animated wait and progress) at 24/32/48 px with the X11, CSS and legacy hash names as symlinks. |
| Sounds | `meadow`: 13 synthesized sounds named after the freedesktop sound-naming spec, so libcanberra applications use them too. |
| Fonts | `50-meadow.conf` aliases the classic UI font names to open fonts. It is not linked into `/etc/fonts/conf.d`; Meadow sessions load it through `FONTCONFIG_FILE` (`/usr/share/meadow/fontconfig/fonts.conf`, which loads the aliases and then the system configuration). |
| Defaults | `meadow-session` sets `DCONF_PROFILE` (`user-db:user` then a read-only Meadow database: GTK theme, icons, cursors, fonts, sound theme), puts `$XDG_RUNTIME_DIR/meadow/xdg-<pid>` and `/usr/share/meadow/xdg` (Kvantum theme choice, PCManFM-Qt and GTK `settings.ini` defaults) in front of `XDG_CONFIG_DIRS`, and exports both to the D-Bus/systemd activation environment so portals see them (GTK 4 reads its settings through xdg-desktop-portal-gtk). The user's own settings always win. |
| Leak guard | `meadow-session` runs labwc as a child instead of `exec`. When labwc exits it unsets Meadow's variables from the user's systemd environment, stops xdg-desktop-portal-gtk (which read Meadow's defaults) and removes the per-session directories, so a later GNOME or KDE session of the same user is not affected. SIGTERM is forwarded to labwc. |
| Scheme switch | `ToolkitThemeSync` (shell) follows the loaded scheme: it sets `GTK_THEME` in the shell's environment, so applications it starts inherit it, and writes the scheme's Kvantum theme to the per-session config directory. Running applications keep their theme until restarted. |
| Sound events | `ShellSounds` (shell): logon when the session starts (not after a crash restart, `MEADOW_RESTARTED`), log-off and shutdown sounds before those actions (waiting at most `sounds.maxWaitMs`), notification sounds (`sound-name`, `sound-file`, `suppress-sound`; normal and critical urgency), Run errors and emptying the Recycle Bin. The greeter plays the startup sound when it appears within `welcome.startupSoundUptimeSec` of boot; unlocking is silent. `[Sounds] enabled` mutes every player live. |

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
- **Power:** the Turn Off dialog (`TurnOffDialog` in Meadow.Controls, shared
  with the shell) calls `sddm.suspend/hibernate/powerOff/reboot` and logs
  each request under `meadow.power`. SDDM's `--test-mode` reports no
  capabilities, so `debugPowerButtons=true` in `theme.conf` shows them all for
  testing.
- **Logging:** `meadow.greeter`, `meadow.greeter.auth` (user name and outcome
  only, never the password) and `meadow.power`.
- **Preview harness:** `tools/greeter-preview` loads the theme with C++ stand-ins
  for SDDM 0.21's `sddm`, `userModel`, `sessionModel`, `keyboard`,
  `screenModel` and `config`, with the same property, role and signal names.
  It drives the screenshot and flow tests. `tools/greeter-sddm-smoke.sh` and
  `tools/greeter-real-screenshot.sh` check the theme in the real
  `sddm-greeter-qt6`.

### Session (labwc)

```
/usr/share/wayland-sessions/meadow.desktop   Exec=meadow-session, DesktopNames=Meadow
meadow-session        exports XDG_CURRENT_DESKTOP=Meadow:X-Meadow and the toolkit theme variables; keyboard layout from
                      localectl's X11 keymap; builds the runtime overlay of ~/.config/meadow/labwc |
                      /usr/share/meadow/labwc (see "Keyboard layouts"), runs labwc --config-dir <overlay> and
                      cleans up after it; its data directory is found next to it (<prefix>/share/meadow)
  rc.xml              generated from rc.xml.in (fonts and corner radius from tokens): server-side decorations,
                      click-to-focus with raise, automatic placement, icon:iconify,max,close, classic Alt+Tab OSD
  menu.xml            window menu (title-bar right-click, window icon, Alt+Space); a recovery root menu
                      (restart the desktop, log off), reachable only while the shell is down
  autostart           exports the environment to D-Bus/systemd; starts meadow-shell-supervise;
                      MEADOW_NO_SHELL=1 for a bare compositor
meadow-shell-supervise  runs meadow-shell; restarts it after a crash (1, 2, 4 s back-off, MEADOW_RESTARTED=1),
                      stops on a clean exit or SIGTERM, gives up after 5 crashes in 60 s with a notification;
                      one per session (flock)
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
| Alt+Shift (either order) | `meadow-shellctl next-layout` |

labwc has no show-desktop action, so Win+D goes to the shell, which
minimizes through foreign-toplevel. `meadow-shellctl` exits 1 with "meadow-shell is not
running" when no shell owns `org.meadow.Shell`. `lock` then falls back to
`loginctl lock-session`.

### Installation, packaging and resilience (phase 10)

| Part | How |
|---|---|
| `install.sh` | Checks dependencies (prints the `dnf`/`pacman`/`apt` line), builds with `MEADOW_INSTALL_PAM=OFF`, stages `cmake --install --component Unspecified` into a temporary directory, refuses anything outside the prefix, then copies it in, backing up files it would replace (`*.meadow-bak`) and writing `<prefix>/share/meadow/install-manifest.txt` (files with SHA-256, links, created directories, backups, options). Default prefix `/usr/local` (`--prefix`, `--root`, `DESTDIR`, `--dry-run`, `--no-pam`, `--enable-greeter`). |
| Changes outside the prefix | Only through `meadow-setup`, on every distribution: `link-prefix` (session entry with an absolute `Exec`, `/usr/share/sddm/themes/meadow` and the Qt QML directory's `Meadow` link, for a prefix other than `/usr`), `enable-lock` (the distribution's password-only `meadow-lock`), `enable-greeter` (only with `--enable-greeter`; never switches the display manager). Each is recorded in `/var/lib/meadow/setup-state.json` and reverted by its `disable-`/`unlink-` command. |
| `uninstall.sh` | Runs those reverts first (and stops if one fails, e.g. fingerprint still enabled), then removes manifest files whose checksum still matches (changed files are kept with a warning), restores backups and removes the directories install.sh created. |
| Packages | Fedora RPM, Arch `PKGBUILD` and `debian/`, all with the same split (`meadow`, `-common`, `-shell`, `sddm-theme-meadow`, `-themes`) and no install scripts. `MEADOW_PAM_FLAVOUR` picks the distribution's `meadow-lock` (password-auth / common-auth / system-auth). ctest `packaging` checks that every installed file belongs to exactly one package in all three. |
| Lock without PAM service | `SessionLock` refuses to lock when no `meadow-lock` service exists (`--no-pam`), since nobody could unlock. |
| Crash resilience | `meadow-shell-supervise` (above). The compositor holds a lock across a crash and the restarted shell re-locks. While the shell is down, a right-click on the desktop opens labwc's recovery menu. A Meadow QML error in the greeter makes SDDM fall back to its built-in theme; `meadow-setup disable-greeter` from a text console restores the previous theme setting. |
| Performance | Opt-in `MEADOW_PERF_LOG=1` marks (`Perf`, `meadow.<component>.perf`); `tools/perf-bench.sh` measures the budget. Numbers and method: [PERFORMANCE.md](PERFORMANCE.md). |

## 5. Source layout and build

| Path | Target | Notes |
|---|---|---|
| `common/` | `meadow-common` (SHARED, SONAME `.0`) + `meadow-commonplugin` | the QML module's qmldir is generated by `qt_add_qml_module` |
| `shell/src/**` (except `ui/`) | `meadow-shell-core` (STATIC) | all non-UI logic; unit tests link it |
| `shell/src/ui/`, `shell/qml/` | `meadow-shell-ui` (STATIC) + QML module `Meadow.Shell` | static so QML tests can load it |
| `shell/src/main.cpp` | `meadow-shell` | `--preview [--preview-windows N] [--size WxH]`, `--open POPUP`, `--grab FILE`, `--theme`, `--exit-after-ms` |
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
| meadow-setup's record of its changes | `/var/lib/meadow/setup-state.json` | `meadow-setup` only |
| install.sh manifest | `<prefix>/share/meadow/install-manifest.txt` | `install.sh` (read by `uninstall.sh`) |
| Prefix links (non-`/usr` installs) | `/usr/share/wayland-sessions/meadow.desktop`, `/usr/share/sddm/themes/meadow`, `<Qt QML dir>/Meadow` | `meadow-setup link-prefix` only |
| Desktop icon positions | `~/.local/share/meadow/desktop-layout.json` | shell |
| Lock flag | `$XDG_RUNTIME_DIR/meadow/locked` | shell |
| labwc runtime rc.xml | `$XDG_RUNTIME_DIR/meadow/labwc-<pid>/rc.xml` | shell (overlay only) |
| Kvantum theme for the scheme | `$XDG_RUNTIME_DIR/meadow/xdg-<pid>/Kvantum/kvantum.kvconfig` | shell (session only) |
| Application defaults | `/usr/share/meadow/dconf/`, `/usr/share/meadow/xdg/`, `/usr/share/meadow/fontconfig/` | package (read-only; used only by Meadow sessions) |
| PAM | `/etc/pam.d/sddm` (marked block) | `meadow-setup` only |
| PAM, lock screen | `/etc/pam.d/meadow-lock` | package (password-only, `%config(noreplace)`) or `meadow-setup enable-lock` (install.sh); `meadow-setup` adds or removes the marked fingerprint block |

## 7. Security model

- The greeter runs as `sddm` with no extra privileges. The fprintd helper only
  calls `ListEnrolledFingers`.
- There are no setuid binaries. Power actions go through logind and polkit.
- The lock screen runs PAM in `meadow-auth-helper`, a separate unprivileged
  process that can only authenticate the user running it. The lock itself is
  the compositor's (`ext-session-lock-v1`): if the shell dies, the screen
  stays locked.
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
| A broken PAM stack locks users out | The package never touches existing PAM services; it adds only the lock screen's own `meadow-lock`, which no login path uses. `meadow-setup` uses markers, `.meadow-bak` backups and `pamtester`, and documents TTY recovery. PAM is tested only in the VM. |
| SDDM creates one QML engine per screen | `Theme` is a per-engine object over one shared loader; a test covers two engines. |
| SDDM starts PAM only on `login()` | Auto-start the fingerprint conversation with `login(user, "")` when fingers are enrolled; empty Enter is the fallback (phase 4). |
| The labwc / LayerShellQt API changes | Wrap it in a small helper and pin minimum versions in the spec. |
| Headless CI can't show real Wayland | `--preview` + `QT_QPA_PLATFORM=offscreen`; labwc with `WLR_BACKENDS=headless` for screenshot tests. |
| Fedora's Qt logs to journald, so test failures go unseen | ctest sets `QT_FORCE_STDERR_LOGGING=1`, and the smoke test fails on QML errors. |
| Microsoft assets creep in | `tools/check-assets.sh` runs in ctest and CI; audio must be listed in the generated sound manifest. |
| A generic install conflicts with distribution files | install.sh writes only into its prefix (checked before copying); outside it, `meadow-setup link-prefix` refuses to replace a session entry or theme it did not create. |
| The shell crashes in a loop | `meadow-shell-supervise` gives up after 5 crashes in a minute and says so; the recovery root menu restarts it or logs off. |
| Meadow's application defaults leak into other desktops | Session-scoped only (`DCONF_PROFILE`, `XDG_CONFIG_DIRS`, `FONTCONFIG_FILE`); unset from the systemd environment when the session ends (ctest `session_defaults`). |

## 9. Roadmap

| Phase | Deliverable |
|---|---|
| 1 | Scaffold, docs, theme tokens, CMake, spec draft (done) |
| 2 | ThemeLoader complete, SoundPlayer, all Luna controls, gallery, golden screenshots (done) |
| 3 | SDDM Welcome screen (done) |
| 4 | Fingerprint login (PAM snippet, greeter UX, meadow-setup fingerprint commands) (done) |
| 5 | Session script, labwc config, decorations, shell IPC (done) |
| 6 | Taskbar, tray, notifications, clock, volume/network/battery (done) |
| 7 | Start menu, All Programs, Run, Log Off / Turn Off dialogs (done) |
| 8 | Desktop, lock screen, Display Properties, Control Panel, Alt+Tab (done) |
| 9 | GTK/Kvantum themes, icons, cursors, sounds, fontconfig (done) |
| 10 | Generic installer, Arch/Debian packaging, performance, crash resilience (done) |
| 11 | Fedora RPM, `meadow-setup`, SELinux, COPR/Packit, test VM |
| 12 | Verification and final report |
