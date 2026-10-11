#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""A still GTK window showing the controls the Meadow-Luna theme styles.

Used by tools/toolkit-screenshot.sh for the toolkit goldens; unlike the GTK
widget factories it has no spinners, pulsing bars or random content, so a
screenshot of it is the same on every run.

  gtk-demo.py [--gtk 3|4] [--menu] [--title TITLE]

--menu opens the File menu once the window is shown, with its first item
highlighted. Prints "ready" on stdout when the window is drawn.
"""
import argparse
import sys

ap = argparse.ArgumentParser()
ap.add_argument("--gtk", choices=("3", "4"), default="3")
ap.add_argument("--menu", action="store_true")
ap.add_argument("--title", default="Meadow GTK controls")
ap.add_argument("--icon-theme")
args = ap.parse_args()

import gi  # noqa: E402

gi.require_version("Gtk", args.gtk + ".0")
from gi.repository import GLib, Gtk  # noqa: E402

GTK4 = args.gtk == "4"
ROWS = [("My Documents", "File Folder", "2 KB"), ("Budget.ods", "Spreadsheet", "18 KB"),
        ("Holiday.png", "PNG Image", "640 KB"), ("Notes.txt", "Text Document", "1 KB"),
        ("Setup.sh", "Shell Script", "4 KB"), ("Meadow.svg", "SVG Image", "12 KB"),
        ("Readme.md", "Markdown", "3 KB"), ("Song.oga", "Audio", "2.1 MB"),
        ("Report.pdf", "PDF Document", "96 KB"), ("Archive.tar", "Archive", "1.4 MB")]


def add(box, child):
    if GTK4:
        box.append(child)
    else:
        box.pack_start(child, False, False, 0)


def hbox(spacing=8):
    return Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=spacing)


def vbox(spacing=8):
    return Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=spacing)


def settings():
    s = Gtk.Settings.get_default()
    s.props.gtk_enable_animations = False
    s.props.gtk_cursor_blink = False
    if args.icon_theme:
        s.props.gtk_icon_theme_name = args.icon_theme
    if hasattr(s.props, "gtk_overlay_scrolling"):
        s.props.gtk_overlay_scrolling = False


def buttons():
    row = hbox()
    ok = Gtk.Button(label="OK")
    ok.get_style_context().add_class("default") if not GTK4 else ok.add_css_class("default")
    add(row, ok)
    add(row, Gtk.Button(label="Cancel"))
    disabled = Gtk.Button(label="Apply")
    disabled.set_sensitive(False)
    add(row, disabled)
    toggle = Gtk.ToggleButton(label="Pressed")
    toggle.set_active(True)
    add(row, toggle)
    return row


def checks():
    row = hbox(12)
    for label, state, sensitive in (("Unchecked", False, True), ("Checked", True, True),
                                    ("Mixed", None, True), ("Disabled", True, False)):
        c = Gtk.CheckButton(label=label)
        if state is None:
            c.set_inconsistent(True)
        else:
            c.set_active(state)
        c.set_sensitive(sensitive)
        add(row, c)
    group = None
    for label, active, sensitive in (("Blue", True, True), ("Olive", False, True), ("Silver", False, False)):
        if GTK4:
            r = Gtk.CheckButton(label=label)
            if group:
                r.set_group(group)
        else:
            r = Gtk.RadioButton.new_with_label_from_widget(group, label)
        group = group or r
        r.set_active(active)
        r.set_sensitive(sensitive)
        add(row, r)
    return row


def inputs():
    row = hbox()
    e = Gtk.Entry()
    e.set_text("Meadow")
    add(row, e)
    d = Gtk.Entry()
    d.set_text("Read only")
    d.set_sensitive(False)
    add(row, d)
    spin = Gtk.SpinButton.new_with_range(0, 10, 1)
    spin.set_value(5)
    add(row, spin)
    if GTK4:
        combo = Gtk.DropDown.new_from_strings(["Luna Blue", "Olive Green", "Silver"])
    else:
        combo = Gtk.ComboBoxText()
        for s in ("Luna Blue", "Olive Green", "Silver"):
            combo.append_text(s)
        combo.set_active(0)
    add(row, combo)
    return row


def ranges():
    row = hbox(12)
    scale = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 100, 1)
    scale.set_value(40)
    scale.set_draw_value(False)
    scale.set_size_request(160, -1)
    add(row, scale)
    bar = Gtk.ProgressBar()
    bar.set_fraction(0.6)
    bar.set_size_request(200, -1)
    bar.set_valign(Gtk.Align.CENTER)
    add(row, bar)
    sw = Gtk.Switch()
    sw.set_active(True)
    sw.set_valign(Gtk.Align.CENTER)
    add(row, sw)
    return row


def file_list():
    store = Gtk.ListStore(str, str, str)
    for r in ROWS:
        store.append(r)
    view = Gtk.TreeView(model=store)
    for i, title in enumerate(("Name", "Type", "Size")):
        col = Gtk.TreeViewColumn(title, Gtk.CellRendererText(), text=i)
        col.set_resizable(True)
        col.set_min_width(110)
        view.append_column(col)
    view.get_selection().select_path(Gtk.TreePath.new_from_string("1"))
    scroller = Gtk.ScrolledWindow()
    scroller.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.ALWAYS)
    scroller.set_size_request(-1, 150)
    if GTK4:
        scroller.set_overlay_scrolling(False)
        scroller.set_child(view)
        scroller.set_has_frame(True)
    else:
        scroller.set_overlay_scrolling(False)
        scroller.add(view)
        scroller.set_shadow_type(Gtk.ShadowType.IN)
    return scroller


def notebook():
    nb = Gtk.Notebook()
    pages = (("General", file_list()), ("Details", Gtk.Label(label="Details")), ("Sharing", Gtk.Label(label="Sharing")))
    for title, child in pages:
        nb.append_page(child, Gtk.Label(label=title))
    return nb


def group():
    frame = Gtk.Frame(label="Group")
    inner = vbox(4)
    inner.set_margin_start(8)
    inner.set_margin_end(8)
    inner.set_margin_top(4)
    inner.set_margin_bottom(8)
    add(inner, Gtk.Label(label="Labels, links and frames"))
    add(inner, Gtk.LinkButton.new_with_label("https://example.org/", "A link"))
    if GTK4:
        frame.set_child(inner)
    else:
        frame.add(inner)
    return frame


def menubar_gtk3():
    bar = Gtk.MenuBar()
    first_item = None
    for title, items in (("File", ["New", "Open…", "-", "Save", "Exit"]), ("Edit", ["Copy", "Paste"]),
                         ("View", ["Status Bar"]), ("Help", ["About"])):
        top = Gtk.MenuItem(label=title)
        menu = Gtk.Menu()
        for it in items:
            if it == "-":
                menu.append(Gtk.SeparatorMenuItem())
            elif it == "Status Bar":
                c = Gtk.CheckMenuItem(label=it)
                c.set_active(True)
                menu.append(c)
            else:
                menu.append(Gtk.MenuItem(label=it))
        top.set_submenu(menu)
        bar.append(top)
        first_item = first_item or top
    bar.show_all()
    return bar, first_item


def menubar_gtk4(window):
    from gi.repository import Gio
    model = Gio.Menu()
    for title, items in (("File", ["New", "Open…", "Save", "Exit"]), ("Edit", ["Copy", "Paste"]),
                         ("View", ["Status Bar"]), ("Help", ["About"])):
        sub = Gio.Menu()
        for it in items:
            sub.append(it, "demo.noop")
        model.append_submenu(title, sub)
    group = Gio.SimpleActionGroup()
    group.add_action(Gio.SimpleAction.new("noop", None))
    window.insert_action_group("demo", group)
    return Gtk.PopoverMenuBar.new_from_model(model)


def content(window=None):
    root = vbox(0)
    if GTK4:
        add(root, menubar_gtk4(window))
        bar_item = None
    else:
        bar, bar_item = menubar_gtk3()
        add(root, bar)
    body = vbox(10)
    for m in ("start", "end", "top", "bottom"):
        getattr(body, f"set_margin_{m}")(12)
    for part in (buttons(), checks(), inputs(), ranges()):
        add(body, part)
    nb = notebook()
    if GTK4:
        nb.set_vexpand(True)
        body.append(nb)
    else:
        body.pack_start(nb, True, True, 0)
    add(body, group())
    if GTK4:
        body.set_vexpand(True)
        root.append(body)
    else:
        root.pack_start(body, True, True, 0)
    return root, bar_item


def ready(window, bar_item):
    if args.menu and bar_item is not None:
        bar_item.select()
        GLib.timeout_add(400, lambda: (bar_item.get_submenu().select_first(False), print("ready", flush=True))[1] and False)
    else:
        print("ready", flush=True)
    return False


def main():
    settings()
    if GTK4:
        # A plain window and main loop: no Gtk.Application, so no session bus.
        loop = GLib.MainLoop()
        win = Gtk.Window(title=args.title)
        win.set_default_size(720, 560)
        win.connect("close-request", lambda *_: loop.quit())
        root, _ = content(win)
        win.set_child(root)
        win.present()
        GLib.timeout_add(800, ready, win, None)
        loop.run()
        return 0
    win = Gtk.Window(title=args.title)
    win.set_default_size(720, 560)
    win.connect("destroy", Gtk.main_quit)
    root, bar_item = content()
    win.add(root)
    win.show_all()
    GLib.timeout_add(800, ready, win, bar_item)
    Gtk.main()
    return 0


if __name__ == "__main__":
    sys.exit(main())
