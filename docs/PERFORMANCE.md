# Performance

## Budget

| What | Budget | Measured as |
|---|---|---|
| Shell idle memory | < 120 MB | PSS of `meadow-shell` 5 s after its first frame (`/proc/<pid>/smaps_rollup`; PSS charges shared Qt and system libraries proportionally) |
| Start menu open | < 100 ms | from the request (Start button, Win key or `meadow-shellctl toggle-start-menu`) to the first frame that shows the menu; median of 10 opens after a first, "cold" one |
| Welcome screen ready | < 1.5 s | from the greeter process starting to the first frame of the Welcome screen; median of 5 |

## Method

`tools/perf-bench.sh BUILD_DIR` measures all three in a headless labwc, with
mock system services and 150 fixture applications in the menus. The timings
come from opt-in marks: with `MEADOW_PERF_LOG=1`, the shell and greeter log
`meadow.<component>.perf: <mark> <ms since process start>` (`common/src/Perf.h`):
`services-ready`, `qml-loaded`, `first-frame`, `start-menu-requested`,
`start-menu-shown` and, in the greeter, `ready` and `first-frame`. Without the
variable the marks cost a single check.

The greeter is measured in the real `sddm-greeter-qt6 --test-mode` when it is
installed, otherwise in `meadow-greeter-preview`.

ctest `perf_budget` (label `perf`) runs the bench with `--check --headroom 1.5`
and fails above budget × 1.5. Packages exclude the label, because build
machines are too noisy for timing.

## Results

Fedora 44 container, Qt 6.11.2, labwc 0.9.6 headless with the pixman renderer,
no GPU, 1280×800. Real hardware numbers are on the VM checklist
(docs/TESTING.md).

| | Before phase 10 fixes | After |
|---|---|---|
| Shell: services ready / QML loaded / first frame | 94 / 294 / 299 ms | 115 / 296 / 301 ms |
| Shell idle PSS | 87.7 MB (private 79.6 MB) | 88.0 MB (private 79.9 MB) |
| Shell PSS after 11 Start menu opens | 100.1 MB | 98.5 MB |
| Start menu open, median / max / cold | 44.2 / 56.8 / 63.7 ms | 24.0 / 39.0 / 63.8 ms |
| Welcome screen ready, median | 382 ms | 361 ms |

Memory levels off: after 60 Start menu opens PSS is still 100.2 MB (98.2 MB
after 10), so the growth after the first opens is Qt's caches (glyphs,
pixmaps, compiled components), not a leak.

### Renderer

With Qt's default OpenGL renderer the same shell measured **261 MB** idle PSS
(323 MB after 11 menu opens). Each shell surface (taskbar, desktop, popups,
tooltips, balloons, ...) gets its own OpenGL context and buffers. These numbers
come from Mesa's software rasterizer, since the container has no GPU, but a GPU
driver also maps its memory into each process. The startup and menu timings
were similar (first frame 625 ms, menu median 27 ms).

The shell is flat 2D UI, so **it now renders with Qt's software scene graph by
default**: `[Performance] renderer=software` in `shell.conf`, with `opengl` as
the alternative. This affects only the shell's own surfaces, not applications.
`QT_QUICK_BACKEND` overrides the setting.

## Fixes made

- **Font family lookups are cached** (`ThemeLoader::fontFamily`). Every text
  item asked for its role's family, and each call built
  `QFontDatabase::families()` anew. This halved the Start menu open time
  (44 → 24 ms) and benefits the greeter too. The cache is dropped when another
  scheme loads.
- **The All Programs tree is built once at startup** instead of up to three
  times. Before, the constructor built it, then each of the two setters that
  followed rebuilt it.
- **The shell renders in software by default** (see above): 261 MB → 88 MB.

Looked at and left as they are:

- **Lazy creation of the hidden dialog windows** (Run, Taskbar Properties,
  authentication, screen saver). A hidden window has no backing store, so
  this would save little next to the risk.
- **Keeping one Start menu instance alive** instead of creating it on each
  open. 24 ms is well inside the budget, and a fresh menu always reflects
  the current pins and frequent list.
- **QML compilation.** The shell, controls and Welcome modules are already
  compiled ahead of time by qmlcachegen. Only the greeter's small `Main.qml`
  is loaded from disk, because SDDM requires that.

## Reproducing

```sh
cmake --build build
tools/perf-bench.sh build                      # report
tools/perf-bench.sh build --json perf.json     # also as JSON
tools/perf-bench.sh build --opens 60           # watch memory over many opens
QT_QUICK_BACKEND=opengl tools/perf-bench.sh build
ctest --test-dir build -L perf
```
