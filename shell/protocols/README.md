# Bundled Wayland protocols

Copies of two protocol definitions from
[wlr-protocols](https://gitlab.freedesktop.org/wlroots/wlr-protocols)
(snapshot a741f0a, 2025-08-16), used only when the build machine has no
`wlr-protocols` package (Debian 13 has none):

- `wlr-foreign-toplevel-management-unstable-v1.xml` (window list)
- `wlr-output-management-unstable-v1.xml` (Display Properties)

Each file carries its own permissive license in its `<copyright>` element;
they are not Meadow code and are not covered by Meadow's GPL. When the
system package is installed, its files are used instead.
