# Meadow visual specification

These are starting values at 96 DPI and `scale.factor = 1.0`. **All values live
in `theme/<scheme>/theme.json`.** This document explains them and names the
token that holds each one. If this document and the JSON disagree, the JSON
wins: fix this document. Values are tuned against reference screenshots the
project owner provides, and none are measured from Microsoft files.

Notation: `taskbar.height` is a token path. Gradients are lists of
`{pos, color}` stops, ordered top→bottom (or left→right where noted). Colors are
`#RRGGBB` or `#AARRGGBB`.

## 0. Global

| Item | Value | Token |
|---|---|---|
| Base UI font | ≈11px (8pt) sans | `fonts.ui` |
| Title bar font | bold ≈13px | `fonts.title` |
| Start label | bold italic ≈18px, white, dark drop shadow | `fonts.start`, `startButton.label*` |
| Welcome user names | ≈20px | `fonts.welcomeUser` |
| Scale | 1.0 / 1.25 / 1.5 / 2.0; every metric multiplies by `scale.factor` | `scale.*` |
| Light source | top-left: highlights on top/left edges, shadows bottom/right | — |
| Reduced motion | all animations become instant | `animation.reducedMotion` (+ system setting) |

Font lists start with the classic family (used only if the user has installed
it), then fall back to Noto Sans → DejaVu Sans → Liberation Sans → `sans-serif`.

## 1. Taskbar (`taskbar.*`)

- 30px tall (`height`), full width, bottom edge, exclusive zone 30px (`exclusiveZone`).
- Background: `gradient`. A light edge on the top 1–2px (`topEdgeColor`,
  `topEdgeHeight`), a saturated body around #245EDC, and a darker bottom (#1941A5).
- Auto-hide: hides to a `autoHideRevealHeight` px strip after `autoHideDelayMs`.

### Start button (`startButton.*`)

| State | Look | Token |
|---|---|---|
| normal | green glossy gradient around #3C9A3C | `gradient.normal` |
| hover | brighter | `gradient.hover` |
| pressed / menu open | darker, inverted gradient (inset look) | `gradient.pressed` |

- 100×30 (`width`, `height`), left-aligned. The right edge is rounded with
  radius `cornerRadiusRight` and the left edge is square.
- Logo `logoSize` px at `logoMarginLeft`, then the label "start" (`label`) after
  `labelSpacing`.
- Label: white (`labelColor`) with a 1px shadow (`labelShadowColor`, `labelShadowOffset`).
- Gloss: a top half overlay (`highlightColor`) and a 1px border (`borderColor`).
- The logo is Meadow's original four-pane "meadow" mark, not the Windows flag.

### Quick Launch (`quickLaunch.*`)
16px icons (`iconSize`) with `itemPadding` padding. A grip
(`gripWidth`, dotted with `gripDotColor`/`gripDotHighlight`) separates it from
the task list. It can be toggled; the default is `enabledByDefault`.

### Task buttons (`taskButton.*`)

| State | Token |
|---|---|
| normal | `gradient.normal` |
| hover | `gradient.hover` |
| active (focused window) | `gradient.active` (darker, pressed look), border `activeBorderColor` |
| needs attention | pulses between normal and `gradient.attention` (orange), `attentionPulseMs` × `attentionPulseCount`, then stays orange |

- Width is clamped to [`minWidth`, `maxWidth`] = [40, 160]. Height is `height`
  and corners use `radius` = 3.
- 16px app icon, then the title elided at the right, in white (`textColor`).
- When the bar overflows, buttons group by app: one button showing a count
  opens a popup list (TaskGroupMenu).
- Middle-click closes the window; right-click opens the window menu.

### Notification area (`tray.*`, `clock.*`, `languageBar.*`)
- A separate lighter-blue segment (`tray.gradient`) with a darker left border
  (`leftBorderColor` plus a 1px `leftBorderHighlight`).
- 16px icons (`iconSize`) spaced by `iconSpacing`. The hide-inactive chevron
  (`chevronSize`) sits on the left.
- Clock: `clock.format` (HH:mm), with a tooltip in `clock.tooltipFormat`, white text.
- Language bar: a `languageBar.width` px box showing "EN", "AR", …
- Volume, network and battery icons sit before the clock.

## 2. Start menu (`startMenu.*`)

380×480 (`width`, `height`), docked above the Start button, top corners rounded
(`radiusTop`), with a 1px border (`borderColor`).

| Region | Spec | Tokens |
|---|---|---|
| Header | 64px dark-blue band (`header.gradient`). A 48px avatar with a 2px white rounded border, then the user name in bold white with a shadow | `header.*`, `fonts.menuHeader` |
| Header separator | 2px orange glow line that fades at both ends | `headerSeparator.*` |
| Left column | white (`left.background`), `left.width` px wide. Pinned items (browser, e-mail) with bold names (`fonts.menuItemBold`) and grey subtitles (`left.subtitleColor`); separator; MFU list (`left.mfuCount` = 6); separator; "All Programs ▸" with a green arrow (`left.allProgramsArrowColor`) | `left.*` |
| Right column | light blue #D3E5FA (`right.background`). My Documents, My Recent Documents ▸, My Pictures, My Music, My Computer │ Control Panel, Default Programs, Printers │ Help and Support, Search, Run…. Bold dark-blue text | `right.*` |
| Footer | dark-blue band (`footer.gradient`), `footer.height` px, with "Log Off" and "Turn Off Computer" on the right, each with an original 22px icon | `footer.*` |
| Hover (both columns) | selection blue #316AC5 with white text | `left.hoverBackground`, `right.hoverBackground` |

All Programs (`startMenu.allPrograms.*`):
- Cascading flyouts with folders sorted first, then entries, alphabetically.
- Hover opens a submenu after `hoverDelayMs` = 400ms. Keyboard: arrows, Enter, Esc, type-ahead.
- Programs installed within `newProgramDays` are highlighted with `newProgramHighlight`.
- Rows are `itemHeight` tall with a 16px icon.

Animation: a short slide (`animation.startMenuSlide` px) plus a fade over
`animation.startMenuOpenMs`. It is disabled under reduced motion.

## 3. Window decorations (labwc, `window.*`)

| Element | Spec | Tokens |
|---|---|---|
| Title bar | 30px, deep blue gradient with a bright top edge, 8px rounded top corners | `titlebar.height`, `titlebar.gradient.active`, `titlebar.radiusTop` |
| Title text | bold white (`fonts.title`) with a 1px dark shadow, left-aligned after a 16px icon | `titlebar.textColor`, `textShadow*` |
| Border | 4px beveled blue (highlight/body/shadow) | `border.*` |
| Buttons | 21×21, radius 3, 1px white border, 2px apart | `buttons.size`, `buttons.radius`, `buttons.spacing` |
| Min / Max / Restore | blue rounded squares with white glyphs | `buttons.normal.{normal,hover,pressed}` |
| Close | red-orange square with a white X | `buttons.close.{normal,hover,pressed}` |
| Inactive window | desaturated lighter blue title, border and buttons | `titlebar.gradient.inactive`, `border.inactive*`, `buttons.inactive` |

labwc's `themerc` only supports two-color gradients, so the full stop list is
used by our own SVG button art. The two-stop title approximation is generated
from these tokens (phase 5).

## 4. Welcome screen (greeter, `welcome.*`)

```
┌──────────────────────────── top band (80px, #00309C) ────────────────────────────┐
├─────────── divider: light line, fades at ends (dividerTop) ──────────────────────┤
│   ◯ soft radial glow (glow.*)          ┆                                         │
│        [meadow logo]                   ┆   ┌──────────────────────────────┐      │
│   To begin, click your user name       ┆   │ [48px avatar]  User Name     │ ◀ tile│
│                                        ┆   └──────────────────────────────┘      │
│        middle area (#5A7EDC)           ┆   (scrollable list)                     │
│                       vertical divider, fades at both ends (verticalDivider)     │
├─────────── divider: orange line, fades at ends (dividerBottom) ──────────────────┤
│ [⏻ Turn off computer]   [session ▾] [EN] [🖐]        info text (infoText)        │
└──────────────────────────── bottom band (80px) ──────────────────────────────────┘
```

- User tiles (`tile.*`): `width`×`height`, a 48px avatar with a 2px rounded
  white frame (`avatarFrameColor`), and the name in `fonts.welcomeUser`. An
  optional sub-line ("3 programs running") uses `subtextColor`.
  - hover: a rounded highlight (`hoverGradient`, fading in from the left) and the frame turns `avatarFrameHoverColor`
  - selected: `selectedGradient`, and the tile expands to show the password panel. Other tiles dim to `dimmedOpacity`.
- Password panel (`passwordPanel.*`): a `fieldWidth`×`fieldHeight` field, a
  green arrow submit button (`submitGradient`) and a blue "?" hint button
  (`hintGradient`). A wrong password shakes the field by `shakeDistance` over
  `shakeDurationMs` and shows a balloon.
- Turn Off button (`turnOffButton.*`): bottom-left, red rounded icon with label text.
- Bottom controls (`bottomControls.*`): the session switcher, keyboard layout
  indicator and fingerprint indicator are small translucent pills.
- High contrast: `colors.highContrast*` replaces all gradients with solid colors.
- Wording is our own: `hintText`, `infoText`, `passwordPanel.labelText`.

## 5. Dialogs (`dialogs.*`)

### Turn Off Computer (`dialogs.turnOff.*`)
- Compact (314×200), centered, with a blue header band (`dialogs.headerGradient`)
  titled "Turn off computer" and a body in `bodyGradient`.
- Three large round buttons (`roundButtonSize`), each with a white label below:
  Stand By (yellow, `standByGradient`), Turn Off (red, `turnOffGradient`),
  Restart (green, `restartGradient`). Cancel sits at the bottom-right.
  Hibernate is reached with Shift on Stand By, and each button is hidden when
  its action is unavailable.
- Background: in the shell, a grayscale screenshot fades in over
  `dialogs.grayscaleFadeMs`; in the greeter, a dim overlay (`dimOverlayColor`).

### Log Off (`dialogs.logOff.*`)
Same frame, with Switch User / Log Off buttons plus Cancel.

## 6. Controls (`controls.*`)

| Control | States | Key tokens |
|---|---|---|
| Button | normal, hover (orange ring), pressed, focused (dotted focus rect `focusRingInset` + blue ring), default (blue ring), disabled. The ring is `ringWidth` px, a vertical gradient through `hoverRingColors` / `defaultRingColors` | `button.*` |
| TextField | normal, focused (border `focusBorderColor`), disabled; `defaultWidth` | `textField.*` |
| CheckBox | unchecked, checked (green tick `checkColor`), hover ring, disabled | `checkBox.*` |
| ComboBox | normal, hover (button lightens), pressed/open (button gradient inverted), focused (text on selection blue), disabled; popup shows up to `popupMaxItems` rows | `comboBox.*` |
| ScrollBar | track, arrow buttons (auto-repeat `autoRepeatDelayMs`/`autoRepeatMs`), thumb normal/hover/pressed with `gripLines` grip lines; the gradient runs across the bar | `scrollBar.*` |
| Tooltip | pale yellow box with a 1px black border, delay `delayMs` | `tooltip.*` |
| BalloonTip | rounded (`radius`) yellow balloon with a slanted tail (top or bottom), `iconSize` icon, bold title, close button, `timeoutMs` | `balloon.*` |
| ProgressBar | green blocks (`blockWidth`, `blockSpacing`, `blockGradient`) | `progressBar.*` |
| Menu | white, selection blue, separators | `menu.*` |

## 7. Desktop (`desktop.*`)
- The wallpaper is the original procedural "meadow hills" image (`wallpaper`,
  overridable). It can also be a solid color (`background`).
- Icons sit on a 75×75 grid (`iconGridWidth/Height`) at 32px, with white labels
  and a drop shadow. Selection uses `selectionColor`, and the rubber band uses
  `rubberBand*`.

## 8. Motion (`animation.*`)
`fast`/`normal`/`slow` = 100/200/400ms, easing `OutCubic`. Under reduced motion
every duration is treated as 0 (except the "needs attention" color state, which
stays visible without pulsing).

## 9. Schemes
- **luna-blue**: reference values above.
- **luna-olive**: the same keys, with blue-range colors mapped to olive. The
  green start button, red close button and yellow/green dialog buttons are unchanged.
- **luna-silver**: blue-range colors desaturated to light silver. Text on
  silver surfaces is dark (`taskButton.textColor`, `clock.textColor`,
  `window.titlebar.textColor`), caption glyphs are dark
  (`window.buttons.glyphColor`), and the close button keeps its white X
  (`window.buttons.closeGlyphColor`). Surfaces that carry white text (Welcome
  screen, Start menu header/footer, dialog headers) stay darker steel.
- Olive and silver started as hue mappings of blue and were then tuned by hand.
  Edit their JSON directly; nothing regenerates them.

All three must pass `tools/theme-schema.py check theme`.
