# Alternatives considered

## Display manager: LightDM + web greeter (documented, not implemented)

LightDM with a web greeter (`web-greeter` / `nody-greeter`) would also allow a
fully custom Welcome screen, written in HTML/CSS/JS instead of QML.

| | SDDM (chosen) | LightDM + web greeter |
|---|---|---|
| UI toolkit | Qt 6 / QML, shared with meadow-shell | HTML/CSS/JS in a Chromium or WebKit runtime |
| Code reuse with shell | `Meadow.Controls` and `ThemeLoader` reused directly | none; the controls would need a second implementation |
| Models | users, sessions, keyboard layouts, screens built in | users, sessions, layouts via the JS API |
| Wayland greeter | yes (`sddm-wayland-generic`, `-plasma`) | X11 only in practice |
| Fedora packaging | `sddm` in the main repos and actively maintained | web greeters are mostly third-party/COPR |
| Memory/startup | light (QML) | heavier (embedded browser) |

We would implement it only if asked, as a separate `lightdm-theme-meadow`
package that reads the same `theme.json`.

## Compositor alternatives
- **Wayfire / Sway:** Sway is tiling-first. Wayfire is stacking, but labwc's
  Openbox-style config and theme format fit classic decorations better.
- **KWin:** feature-rich, but heavy, and it brings Plasma's shell assumptions.
