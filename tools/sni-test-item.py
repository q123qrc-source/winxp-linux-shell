#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""A StatusNotifierItem with a DBusMenu, for testing the notification area.

  sni-test-item.py [--status Active|Passive|NeedsAttention] [--title T]
                   [--log FILE]

Registers with org.kde.StatusNotifierWatcher (waits for it), draws an
original 16x16 icon, and appends every call it receives (Activate,
SecondaryActivate, Scroll, ContextMenu, menu Event) to --log. Prints
"ready" when registered. Runs until SIGTERM.
"""
import argparse
import os
import signal
import struct

import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

ITEM = "org.kde.StatusNotifierItem"
MENU = "com.canonical.dbusmenu"
PROPS = "org.freedesktop.DBus.Properties"


def icon_pixmap(size, rgb):
    # A rounded colored square with a white dot, ARGB32 big-endian.
    data = bytearray()
    for y in range(size):
        for x in range(size):
            corner = (x in (0, size - 1)) and (y in (0, size - 1))
            dot = (x - size / 2 + 0.5) ** 2 + (y - size / 2 + 0.5) ** 2 < (size / 5) ** 2
            a = 0 if corner else 255
            r, g, b = (255, 255, 255) if dot else rgb
            data += struct.pack(">BBBB", a, r, g, b)
    return dbus.Struct((dbus.Int32(size), dbus.Int32(size), dbus.ByteArray(bytes(data))), signature="iiay")


class Item(dbus.service.Object):
    def __init__(self, bus, args):
        super().__init__(bus, "/StatusNotifierItem")
        self.args = args
        self.props = {
            "Category": "ApplicationStatus",
            "Id": "meadow-sni-test",
            "Title": args.title,
            "Status": args.status,
            "IconName": "",
            "IconPixmap": dbus.Array([icon_pixmap(16, (0x2A, 0x9D, 0x3C)), icon_pixmap(32, (0x2A, 0x9D, 0x3C))],
                                     signature="(iiay)"),
            "ToolTip": dbus.Struct(("", dbus.Array([], signature="(iiay)"), args.title, "Test item"),
                                   signature="sa(iiay)ss"),
            "ItemIsMenu": dbus.Boolean(False),
            "Menu": dbus.ObjectPath("/MenuBar"),
        }

    def log(self, line):
        if self.args.log:
            with open(self.args.log, "a") as f:
                f.write(line + "\n")

    @dbus.service.method(PROPS, in_signature="ss", out_signature="v")
    def Get(self, iface, name):
        return self.props[name]

    @dbus.service.method(PROPS, in_signature="s", out_signature="a{sv}")
    def GetAll(self, iface):
        return dbus.Dictionary(self.props, signature="sv")

    @dbus.service.method(ITEM, in_signature="ii")
    def Activate(self, x, y):
        self.log("Activate")

    @dbus.service.method(ITEM, in_signature="ii")
    def SecondaryActivate(self, x, y):
        self.log("SecondaryActivate")

    @dbus.service.method(ITEM, in_signature="ii")
    def ContextMenu(self, x, y):
        self.log("ContextMenu")

    @dbus.service.method(ITEM, in_signature="is")
    def Scroll(self, delta, orientation):
        self.log(f"Scroll {delta} {orientation}")


class Menu(dbus.service.Object):
    def __init__(self, bus, item):
        super().__init__(bus, "/MenuBar")
        self.item = item

    @staticmethod
    def node(id_, props, children=()):
        return dbus.Struct((dbus.Int32(id_), dbus.Dictionary(props, signature="sv"),
                            dbus.Array([dbus.Struct(c, signature="ia{sv}av") for c in children], signature="v")),
                           signature="ia{sv}av")

    @dbus.service.method(MENU, in_signature="iias", out_signature="u(ia{sv}av)")
    def GetLayout(self, parent, depth, names):
        sub = [self.node(4, {"label": "Sub item"})]
        children = [
            self.node(1, {"label": "_Open Test"}),
            self.node(2, {"type": "separator"}),
            self.node(3, {"label": "Option", "toggle-type": "checkmark", "toggle-state": dbus.Int32(1)}),
            self.node(5, {"label": "More", "children-display": "submenu"}, sub),
            self.node(6, {"label": "Disabled", "enabled": dbus.Boolean(False)}),
        ]
        return dbus.UInt32(1), self.node(0, {"children-display": "submenu"}, children)

    @dbus.service.method(MENU, in_signature="i", out_signature="b")
    def AboutToShow(self, id_):
        return False

    @dbus.service.method(MENU, in_signature="isvu")
    def Event(self, id_, event, data, timestamp):
        self.item.log(f"Event {id_} {event}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--status", default="Active")
    ap.add_argument("--title", default="Meadow Test Item")
    ap.add_argument("--log")
    args = ap.parse_args()

    DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()
    name = dbus.service.BusName(f"org.kde.StatusNotifierItem-{os.getpid()}-1", bus)
    item = Item(bus, args)
    menu = Menu(bus, item)
    loop = GLib.MainLoop()

    def register():
        try:
            watcher = bus.get_object("org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher")
            watcher.RegisterStatusNotifierItem(name.get_name(), dbus_interface="org.kde.StatusNotifierWatcher")
        except dbus.DBusException:
            return True  # retry
        print("ready", flush=True)
        return False

    if register():
        GLib.timeout_add(200, register)
    signal.signal(signal.SIGTERM, lambda *a: loop.quit())
    loop.run()
    del menu


if __name__ == "__main__":
    main()
