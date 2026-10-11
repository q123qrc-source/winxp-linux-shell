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

### Popups and balloons (phase 6)

| Item | Spec | Tokens |
|---|---|---|
| Context menus | white, 1px grey border, soft 2px shadow, 18px rows, blue hover with white text, bold default item, check/radio column, separators, ▸ submenus opening after `submenuDelayMs` | `controls.menu.*` |
| Group button | "N App name" with a small ▾; groups form when buttons would get narrower than `groupThreshold`, largest app first | `taskButton.groupThreshold`, `groupArrowSize` |
| Volume popup | beige panel: "Volume", a vertical slider with `tickCount` ticks, Mute check box | `tray.volume.*` |
| Calendar | the long date, a blue month header with arrows, locale-ordered weekday initials, today highlighted | `clock.calendar.*` |
| Chevron | round translucent button with ‹‹ / ›› | `tray.chevron*` |
| Balloons | `controls.balloon.*`, tail pointing at the notification area; app icon, bold summary, body with b/i/u, optional action buttons | `controls.balloon.*` |
| Tooltips | pale yellow above the taskbar after `delayMs` | `controls.tooltip.*` |
| Status icons | volume, network (wired/wireless/limited/offline), battery (charge, charging) and Show Desktop are drawn procedurally (`TrayGlyph`), original art | — |
| Taskbar Properties | dialog body with group boxes | `dialogs.taskbarProperties.*`, `controls.groupBox.*` |

Attention: wlr-foreign-toplevel has no "urgent" state, so the orange
attention flash triggers only through the model's `needsAttention` role (preview
and tests). Real windows cannot request it until labwc exposes urgency.

## 2. Start menu (`startMenu.*`)

380×480 (`width`, `height`; taller when the program list needs it), docked
above the Start button, top corners rounded (`radiusTop`), with a 1px border
(`borderColor`) and the menu shadow.

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

Pinning by drag shows an insertion line (`dropIndicatorColor`,
`dropIndicatorHeight`) under a translucent icon (`dragIconOpacity`). The
footer icons are small rounded tiles (`footer.logOffTileGradient`,
`turnOffTileGradient`) with original LogOff and Power glyphs.

### Run (`dialogs.run.*`)
A `width` px dialog: an original "run" glyph, the explanation, "Open:" with
the field, an error line with the Error glyph when needed, and OK / Cancel /
Browse….

### Log Off and Turn Off Computer (`dialogs.*`)
Both share one frame (`PowerDialogFrame`): `headerHeight` dark-blue header
with the title and the logo, the blue body (`turnOff.bodyGradient`), a
`footerColor` footer with Cancel, `radius` corners. Turn Off has round
buttons (Stand By / Hibernate with Shift, Turn Off, Restart); Log Off has
rounded-square ones (`logOff.buttonRadius`, `switchUserGradient`,
`logOffGradient`) with original SwitchUser and LogOff glyphs. Behind them the
screen fades from color to gray over `grayscaleFadeMs`.

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

**How labwc renders this** (`tools/gen-labwc-theme.py` → `decorations/labwc/<theme>/labwc/`):

| Token | labwc | Limitation |
|---|---|---|
| `titlebar.gradient.*` | `Gradient SplitVertical`: top edge = the stop at 0, middle = the gradient at 0.5, bottom = the stop at 1 | Only four colors, so intermediate stops are approximated |
| `titlebar.height`, `buttons.size` | `window.button.height` = height − 2×padding, `window.titlebar.padding.height` = (height − size) / 2 | Exactly 30 px (button cell 21×22, art centered) |
| `titlebar.radiusTop` | rc.xml `<cornerRadius>` | Square when maximized (labwc) |
| `border.width`, `border.activeColor`/`inactiveColor` | `border.width`, `window.*.border.color` | One color: no highlight/shadow bevel |
| `buttons.*` gradients, `glyphColor`, `closeGlyphColor`, `glyphInset` | 16 SVGs per theme: iconify, max, max_toggled (restore) and close; active/inactive; normal/hover | **No pressed image** in labwc; inactive windows show the hover set on hover |
| `titlebar.textColor` / `inactiveTextColor`, `fonts.title` | label colors; rc.xml fonts (family fallback list, size in points = px × 0.75, bold) | No title text shadow |
| `controls.menu.*` | `menu.*` (window menu, Alt+Space) | — |
| `windowSwitcher.*`, `colors.selection` | `osd.*` classic window switcher | Interim Alt+Tab until the shell's switcher (phase 8) |

The caption glyphs (minimize bar, maximize box with a thick top, two
overlapping boxes for restore, a bold X) share one geometry. It is used by
`LunaGlyph` in the shell's own window frames and by the generated SVGs.

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
- Wording is our own and lives in the QML as `qsTr()` strings, so it can be
  translated. theme.json holds no user-visible text except the Start label.
- Layout (`welcome.layout`): the panels sit `panelGap` either side of the
  center divider. The hint wraps at `hintMaxWidth` and the info text at
  `infoMaxWidth`. Bottom controls are `bottomControlsSpacing` apart and
  `sidePadding` from the edges.
- Glow and divider fade into `glow.edgeColor` / `verticalDivider.edgeColor`
  (the same RGB with alpha 0) rather than `transparent`. That avoids a dark
  fringe, because gradients interpolate unpremultiplied colors.
- After a successful login, "welcome" appears in `fonts.welcomeTitle` with
  `successText.*`.

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

Greeter variant: footer band (`footerHeight`, `dialogs.footerColor`) with the
Cancel button, the logo (`logoSize`) in the header, buttons `buttonSpacing`
apart with labels `labelSpacing` below, and `hoverOverlay`/`pressedOverlay` on
the round buttons.

### Logo (`logo.*`)
Four rounded panes (`radiusRatio` of a pane) separated by `gapRatio` of the
grid, turned 45° into a diamond. The colors run top, right, bottom, left (sun,
sky, grass, leaves), with a glossy highlight of `glossAlpha` on each pane. A
user's `meadow-logo.svg` in an override folder replaces it.

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
- "Meadow Hills" (`theme/assets/meadow-hills.svg`, generated by
  `tools/gen-wallpaper.py`) is original: a vertical sky gradient, soft cloud
  ellipses and three layered hill curves in greens. No photograph or traced
  shape of any existing wallpaper is used.
- Position: Center (natural size on `background`), Tile, Stretch (fills the
  screen preserving aspect, cropping the overflow).
- Icon labels wrap to two lines; a selected icon gets a `selectionColor` tint
  over the icon and a highlighted label, with a dotted focus rectangle on the
  current item. F2 renames in place.
- The desktop menu: Arrange Icons By ▸ (Name, Size, Type, Modified, Auto
  Arrange, Align to Grid), Refresh, New ▸ (Folder, Shortcut, Text Document),
  Properties (Display Properties, Desktop tab).

### Lock screen (`lockScreen.*`)
The Welcome screen's layout (`welcome.*`) with only the locked user's tile, the
wording "This computer is in use and has been locked." / "To unlock it, type
your password." on the left, Switch User when available and Turn Off. A
successful unlock fades out over `unlockFadeMs` (instant under reduced motion).
Secondary screens show the welcome background with the logo.

### Screen saver (`screenSaver.*`)
Blank (`background`) or the four-pane logo (`logoSize`) drifting at
`driftPxPerSecond` and bouncing off the edges; still under reduced motion.
Input in the first `graceMs` is ignored.

### Alt+Tab (`altTab.*`)
A centred dialog-coloured panel (`background`, `borderColor`, `radius`) with a
drop shadow; up to `columns` icons per row at `iconSize` in `cellSize` cells.
The selected cell has a `selectionColor` frame. Below, the selected window's
title is shown in a white sunken box (`title*`). Rows mirror under RTL.

### Display Properties (`dialogs.displayProperties.*`)
A `width`×`height` dialog with five tabs (Themes, Desktop, Screen Saver,
Appearance, Settings) and OK / Cancel / Apply. Desktop and Screen Saver show a
monitor (`MonitorPreview`) previewing the choice; Settings shows the monitor
arrangement, a resolution slider and the "Keep these settings?" countdown
(`revertSeconds`). The colour popup offers `basicColors` in `colorColumns`.

### Control Panel (`dialogs.controlPanel.*`)
An Explorer-style window: a blue task pane (`taskPaneGradient`) with white task
boxes on the left, and on the right "Pick a category" with original category
icons (category view), or one icon per applet (classic view). Applets are small
tabbed dialogs (`dialogs.applet.*`, `dialogs.mouse.*`, `dialogs.keyboard.*`,
`dialogs.dateTime.*`); Date and Time draws an original analog clock.

### Authenticate (`dialogs.authenticate.*`)
A small Luna dialog: the requesting program's icon and message, an identity
combo when several accounts may authorize, a password field, and the
"incorrect password" line in `dialogs.errorTextColor`.

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
