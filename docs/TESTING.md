# Testing

## Automated (every build)

```sh
ctest --test-dir build --output-on-failure
```

| Test | What it checks |
|---|---|
| `test_themeloader` | scheme lookup, token paths, malformed JSON, asset override order, path traversal rejection |
| `test_appmenu`, `test_settings` | placeholders until phases 7/6 |
| `theme_schema` | every scheme has exactly the tokens/types in `theme/schema.json`; valid colors and gradients |
| `asset_policy` | no Microsoft-style assets or font/audio binaries in the tree; nothing in `assets-override/` |
| `shell_version`, `shell_preview_smoke` | the binary starts, and the full QML module loads offscreen with no QML errors |

ctest sets `QT_FORCE_STDERR_LOGGING=1`. Fedora's Qt otherwise logs to journald,
and QML errors would go unseen.

Screenshot tests (`tools/screenshot-test.py`, phase 2+) render components with
`--preview` (offscreen), or inside labwc with `WLR_BACKENDS=headless`, and diff
the results against `tests/golden/`.

## Manual checklist (Fedora VM, SELinux enforcing; phase 12)

Mark each item with the build tested. Items that need hardware or a VM are
never marked done from a container run.

- [ ] Password login (correct)
- [ ] Wrong password: shake, balloon with hint
- [ ] Caps Lock warning balloon
- [ ] Fingerprint login with a reader / libfprint virtual device
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
