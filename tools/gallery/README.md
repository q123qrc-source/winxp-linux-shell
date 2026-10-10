# Control gallery

`meadow-gallery` shows every `Meadow.Controls` control in every state
(normal, hover, pressed, focused, default, disabled) for any scheme and scale.
It is a developer tool and is not installed.

```sh
cmake --build build
MEADOW_THEME_DIR=$PWD/theme QML_IMPORT_PATH=$PWD/build/qml \
    build/tools/gallery/meadow-gallery [--scheme luna-olive] [--scale 1.5]
```

The scheme and scale combo boxes at the top switch live. With
`MEADOW_THEME_LIVE=1`, edits to `theme/<scheme>/theme.json` appear as soon as
you save.

Static hover/pressed/focus states come from each control's `previewState`
property, so a screenshot shows them without a mouse.

## Screenshot tests

```sh
tools/screenshot-test.py --build-dir build            # compare with tests/golden
tools/screenshot-test.py --build-dir build --update   # rewrite goldens, then review them
```

`--grab out.png` renders once (offscreen, software renderer, animations off)
and exits. `meadow-imgdiff` compares the result with the golden: a pixel counts
as different when any channel is off by more than 24, and the test fails when
more than 0.05% of the pixels differ. On failure, `build/screenshots/` holds the
actual image and a red-on-grey diff.
