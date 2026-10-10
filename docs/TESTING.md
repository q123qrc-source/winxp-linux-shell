# Testing

## Automated (every build)

```sh
ctest --test-dir build --output-on-failure
```

| Test | What it checks |
|---|---|
| `test_themeloader` | scheme lookup and listing, token paths, bad/malformed schemes, scale + `px()`, font fallback, durations and reduced motion, live reload (including rename-over saves and broken edits), the QML singleton sharing one instance, asset override order, path traversal rejection |
| `test_controls` | sound event resolution (bundled vs override) and muting, gradient stop parsing, every `Meadow.Controls` type instantiates without QML warnings, dialog accept/reject |
| `test_appmenu`, `test_settings` | placeholders until phases 7/6 |
| `qml_literals` | no color or numeric literals in product QML (`tools/check-qml-literals.py`); opt out per line with `// theme-ok: reason` |
| `screenshot_gallery` (label `screenshot`) | the control gallery for blue at 1×/1.25×/1.5×/2× and for olive and silver at 1× matches `tests/golden/` |
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

Shell surfaces inside a headless labwc (`WLR_BACKENDS=headless`) join the
screenshot set in phase 6.

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
