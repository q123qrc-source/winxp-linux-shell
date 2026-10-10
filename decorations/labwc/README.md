# labwc window decorations

`Meadow-Luna/`, `Meadow-Luna-Olive/` and `Meadow-Luna-Silver/` are labwc themes
**generated** from `theme/<scheme>/theme.json` by `tools/gen-labwc-theme.py`:
a `themerc` plus the title-bar button SVGs (minimize, maximize, restore and
close; active and inactive; normal and hover). They install to
`/usr/share/themes/<name>/labwc/`.

Do not edit them by hand. Change the tokens, run `tools/gen-labwc-theme.py`,
and commit the result. The `labwc_theme_sync` test fails when they are stale.
The same tool writes `session/labwc/rc.xml` from `session/labwc/rc.xml.in`.
