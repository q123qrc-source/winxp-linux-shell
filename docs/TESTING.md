# Testing

## Automated (every build)

```sh
ctest --test-dir build --output-on-failure
```

| Test | What it checks |
|---|---|
| `test_themeloader` | scheme lookup and listing, token paths, bad/malformed schemes, scale + `px()`, font fallback, durations and reduced motion, live reload (including rename-over saves and broken edits), the QML singleton sharing one instance, asset override order, path traversal rejection |
| `test_controls` | sound event resolution (bundled vs override) and muting, gradient stop parsing, every `Meadow.Controls` type instantiates without QML warnings, dialog accept/reject |
| `test_appmenu` | placeholder until phase 7 |
| `test_settings` | shell.conf: built-in defaults, packaged defaults, user overrides, reset, change signals |
| `test_apps` | `.desktop` parsing (localized names, escaped list separators, `Exec` quoting and field codes, OnlyShowIn/NoDisplay) and app_id → desktop entry matching (exact, case, reverse DNS, StartupWMClass, last component) |
| `test_services` | under `dbus-run-session`: battery and network against mock UPower/NetworkManager (`tools/mock-system-services.py`: wired, wireless, limited, offline, none), volume through `tools/fake-wpctl` (parsing, set-volume/set-mute, 100% cap), keyboard layouts (xkb names from a fixture `evdev.xml`, rotation, the overlay env file, `grp:*` removed, a restarted shell continuing the order), notification markup sanitizing, the full `org.freedesktop.Notifications` API over D-Bus (capabilities, server info, replace, expiry, actions, critical urgency, CloseNotification) and the bounded queue |
| `test_toplevel_labwc` | the window list against a real headless labwc (`tools/with-headless-labwc.sh`): windows appear with titles and app_ids, activate, click-on-active minimizes and click again restores, maximize, show desktop and restore (active window last), close, 10 windows; skipped without labwc |
| `test_shell_qml` | the taskbar QML in preview mode driven by real mouse and key events: click/minimize/restore semantics, middle-click close, window menu, outside click and Escape close menus, grouping at 10 windows and the group menu, Close Group, attention highlight, show desktop (also from Quick Launch), Start button, balloon click runs the default action, language bar menu, clock calendar and hiding the clock, auto-hide, Taskbar Properties (applies only on OK), lock from the taskbar menu |
| `test_greeter` | Turn Off dialog: buttons follow `sddm.canX`, Hibernate replaces Stand By (no suspend or Shift held), calls reach SDDM; keyboard layout cycling; avatar fallback for generic faces |
| `greeter_flows` | nineteen end-to-end Welcome screen scenarios, including fingerprint: auto-start for enrolled users, no auto-start when unknown/not enrolled/off, `always`, Enter on an empty field, a failed reading leaving password login available, and a password typed during a reading being queued rather than dropped (the mock drops overlapping logins like SDDM's daemon). Also through the mock SDDM (wrong/right password, password-less user, selection without login, Turn Off without action, secondary screen, RTL, high contrast, silver at 2×, scrolling list). Fails on QML warnings and if the typed password appears in any output |
| `greeter_sddm_smoke` | loads `greeter/` in the real `sddm-greeter-qt6 --test-mode` (offscreen) and fails on QML errors; skipped when SDDM is not installed |
| `meadow_setup` | 20 tests of `meadow-setup` on sandbox PAM trees (Fedora, Debian with and without the fprintd profile, Arch): PAM parsing, reachability through include/substack/@include (with cycles), the marked sddm block before the password stack, the generated lock service, idempotent enable, byte-exact disable, admin edits surviving disable, dry runs changing nothing, foreign files backed up and restored, refusal without root, and the shipped `system/` files matching the tool's output |
| `test_shellipc` | `org.meadow.Shell` on a private session bus (`dbus-run-session`): every `meadow-shellctl` command reaches the service, `version` answers, a second shell cannot take the name, and the "not running" message and exit code |
| `labwc_theme_sync` | the committed labwc themes and rc.xml equal what `tools/gen-labwc-theme.py` makes from theme.json |
| `labwc_config` | rc.xml has every required keybind, uses only real labwc 0.9 actions and known `Execute` commands/`meadow-shellctl` subcommands, no duplicate keys; menu.xml actions; every generated themerc key is one labwc knows (labwc silently ignores unknown ones); 30 px title bar; all 16 button SVGs present |
| `labwc_config_smoke` | real headless labwc loads the session config with each theme without errors (skipped without labwc) |
| `qml_literals` | no color or numeric literals in product QML (`tools/check-qml-literals.py`); opt out per line with `// theme-ok: reason` |
| `screenshot_gallery` (label `screenshot`) | 39 goldens. 14 for the shell: taskbar in each scheme, grouped buttons with the group menu, tray with a StatusNotifierItem and its DBusMenu (`tools/sni-test-item.py`), volume popup, network menu (wireless, low battery), window menu, taskbar menu, calendar, balloon, Taskbar Properties (all via `tools/shell-preview.sh`), and the real shell on layer-shell surfaces in headless labwc with 10 windows and with the calendar open (`tools/shell-session-screenshot.sh`). The earlier 25: the labwc session (`tools/session-screenshot.sh`: blue, olive, silver and a maximized window, headless labwc + test windows), the button sheets of each scheme (all 16 states, including hover, which a headless session cannot show), plus the earlier 18: the control gallery (blue at 1×/1.25×/1.5×/2×, olive, silver) and the Welcome screen (1024×768, 1920×1080 selected, 2× selected, wrong password, Caps Lock, Turn Off dialog, high contrast, RTL, silver, secondary screen) |
| `theme_schema` | every scheme has exactly the tokens/types in `theme/schema.json`; valid colors and gradients |
| `asset_policy` | no Microsoft-style assets or font/audio binaries in the tree; nothing in `assets-override/` |
| `shell_version`, `shell_preview_smoke` | the binary starts, and the full QML module loads offscreen with no QML errors |

ctest sets `QT_FORCE_STDERR_LOGGING=1`. Fedora's Qt otherwise logs to journald,
and QML errors would go unseen.

### Screenshot tests

`tools/screenshot-test.py` renders `meadow-gallery` with the offscreen platform
and the Qt Quick software renderer, with animations off, then compares each
image with `tests/golden/` using `meadow-imgdiff`. A pixel differs when any
channel is off by more than 24, and the test fails when more than 0.05% of the
pixels differ. On failure, `build/screenshots/` keeps the actual and diff
images; CI uploads them as an artifact.

The goldens assume **Noto Sans** is installed and **Tahoma / Trebuchet MS are
not** (`dnf install google-noto-sans-fonts`). Otherwise the test reports SKIP
(exit 77). Because font updates can move pixels, the RPM `%check` excludes the
`screenshot` label, and only CI on Fedora `latest` runs it. After an
intentional visual change, run `tools/screenshot-test.py --update`, look at
every changed PNG, and commit them with the change.

### Fingerprint, end to end (disposable systems only)

`sudo MEADOW_DISPOSABLE_SYSTEM=1 tools/vm/fingerprint-e2e.sh` runs
`meadow-setup enable-fingerprint` on the real PAM stack, starts fprintd on
libfprint's **virtual reader** (`FP_VIRTUAL_DEVICE`), enrolls a finger for a
test user, and uses `pamtester` against the real `sddm` and `meadow-lock`
services. The scenarios are: matching finger without a password; three misses
then the right password; three misses then a wrong password; a user without
fingers; an untouched reader timing out after 15 s. Then it runs
`disable-fingerprint` and checks that `/etc/pam.d` and `/etc/authselect` are
byte-identical to the snapshot. It creates users and changes PAM, so it refuses
to run without the environment variable. Phase 4 passed it in a Fedora 44
container. Real hardware and a full SDDM login remain VM checks (below).

### Session

```sh
tools/session-screenshot.sh --grab out.png [--scheme luna-olive] [--maximized] [--xterm]
```

This runs labwc headless with `session/labwc` and the generated theme, opens
test windows (and, with `--xterm`, an XWayland window), and captures them with
`grim`. To try the session nested inside your own Wayland desktop:

```sh
tools/run-session.sh [--scheme luna-olive] [--no-shell]
```

This uses the source tree's config, themes and built shell, with nothing
installed. After installing the RPMs, `WLR_BACKENDS=wayland meadow-session` is
enough.

### Real SDDM

`tools/greeter-real-screenshot.sh OUT.png 1920 1080 [SCALE]` runs the theme in
the real `sddm-greeter-qt6 --test-mode` inside a headless labwc and captures it
with `grim`. Use it for a visual check after greeter changes. Phase 3 checked
1024×768, 1920×1080 and output scale 2 this way.

### Shell

```sh
tools/shell-preview.sh build [--mocks] [--sni] [--network wireless] -- --preview-windows 10 --open calendar
tools/shell-session-screenshot.sh build out.png [--windows 10] [--open calendar] [--notify]
```

The first runs `meadow-shell --preview` in a sandbox: a private session bus,
fixture applications, a fixed clock (`MEADOW_FAKE_TIME`) and drawn icons
(`MEADOW_ICONS_GENERIC=1`). It can add mock audio, battery and network
services and a test tray icon. Without `--grab` it stays open, which is
useful on a desktop. The second runs the real shell on layer-shell surfaces in
headless labwc, opens test windows and captures them with `grim`.

`meadow-shell --open POPUP` accepts `calendar`, `volume`, `network`,
`language`, `window-menu`, `group`, `taskbar-menu`, `tray-menu`, `properties`,
`balloon` and `start`.

Headless labwc has no keyboard, so Alt+Shift switching is a manual VM check.
The keymap rebuild on `--reconfigure` was verified from the labwc 0.9.6
source, and test_services checks the file it reads.

## Manual checklist (Fedora VM, SELinux enforcing; phase 12)

Mark each item with the build tested. Items that need hardware or a VM are
never marked done from a container run.

- [ ] Password login (correct)
- [ ] Wrong password: shake, balloon with hint
- [ ] Caps Lock warning balloon
- [ ] Fingerprint login with a real reader (USB passthrough) from the Welcome screen
- [ ] Lock screen unlock with a finger (phase 8)
- [ ] `--allow-greeter-query`: tiles of enrolled users start reading automatically (`auto`)
- [ ] Three fingerprint failures fall back to password
- [ ] No reader present: password login unaffected
- [ ] Switch to another session (Plasma/GNOME) from the session switcher
- [ ] Keyboard layout switching (click, dropdown, Alt+Shift)
- [ ] Turn off / Restart / Stand By / Hibernate from the greeter
- [ ] Same from Start → Turn Off Computer
- [ ] Log Off and Switch User
- [ ] Lock (Win+L) and unlock with password and fingerprint
- [ ] Multi-monitor: full UI on primary, background on others
- [ ] 2× scale
- [ ] RTL (Arabic) layout: taskbar and Start menu mirrored
- [ ] `ausearch -m AVC -ts recent` is empty after all of the above
- [ ] `meadow-setup disable-*` + `dnf remove meadow` restores the original login
