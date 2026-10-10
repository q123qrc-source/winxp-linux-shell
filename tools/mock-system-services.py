#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Mock UPower, NetworkManager, systemd-logind and the display manager's
seat API on the *session* bus, for screenshots and tests. meadow-shell talks
to them when MEADOW_SYSTEM_SERVICES_ON_SESSION_BUS=1.

  mock-system-services.py [--battery PERCENT] [--charging] [--no-battery]
                          [--network wired|wireless|limited|offline|none]
                          [--logind] [--can yes|no|challenge|na] [--no-hibernate]
                          [--display-manager] [--log FILE]

logind and display manager calls are appended to --log (e.g. "PowerOff true",
"SwitchToGreeter"); nothing is powered off.

Runs until SIGTERM. Prints "ready" once the names are owned.
"""
import argparse
import signal
import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

PROPS = "org.freedesktop.DBus.Properties"


class PropObject(dbus.service.Object):
    def __init__(self, bus, path, iface, props):
        super().__init__(bus, path)
        self.iface = iface
        self.props = props

    @dbus.service.method(PROPS, in_signature="ss", out_signature="v")
    def Get(self, iface, name):
        return self.props[name]

    @dbus.service.method(PROPS, in_signature="s", out_signature="a{sv}")
    def GetAll(self, iface):
        return dbus.Dictionary(self.props if iface == self.iface else {}, signature="sv")

    @dbus.service.signal(PROPS, signature="sa{sv}as")
    def PropertiesChanged(self, iface, changed, invalidated):
        pass


def record(args, line):
    if args.log:
        with open(args.log, "a") as f:
            f.write(line + "\n")


class Logind(dbus.service.Object):
    IFACE = "org.freedesktop.login1.Manager"

    def __init__(self, bus, args):
        super().__init__(bus, "/org/freedesktop/login1")
        self.args = args

    def _can(self, what):
        return "na" if what == "Hibernate" and self.args.no_hibernate else self.args.can

    @dbus.service.method(IFACE, out_signature="s")
    def CanPowerOff(self):
        return self._can("PowerOff")

    @dbus.service.method(IFACE, out_signature="s")
    def CanReboot(self):
        return self._can("Reboot")

    @dbus.service.method(IFACE, out_signature="s")
    def CanSuspend(self):
        return self._can("Suspend")

    @dbus.service.method(IFACE, out_signature="s")
    def CanHibernate(self):
        return self._can("Hibernate")

    @dbus.service.method(IFACE, in_signature="b")
    def PowerOff(self, interactive):
        record(self.args, f"PowerOff {str(bool(interactive)).lower()}")

    @dbus.service.method(IFACE, in_signature="b")
    def Reboot(self, interactive):
        record(self.args, f"Reboot {str(bool(interactive)).lower()}")

    @dbus.service.method(IFACE, in_signature="b")
    def Suspend(self, interactive):
        record(self.args, f"Suspend {str(bool(interactive)).lower()}")

    @dbus.service.method(IFACE, in_signature="b")
    def Hibernate(self, interactive):
        record(self.args, f"Hibernate {str(bool(interactive)).lower()}")

    @dbus.service.method(IFACE, in_signature="s")
    def TerminateSession(self, session_id):
        record(self.args, f"TerminateSession {session_id}")


class Seat(dbus.service.Object):
    def __init__(self, bus, args):
        super().__init__(bus, "/org/freedesktop/DisplayManager/Seat0")
        self.args = args

    @dbus.service.method("org.freedesktop.DisplayManager.Seat")
    def SwitchToGreeter(self):
        record(self.args, "SwitchToGreeter")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--battery", type=float, default=72.0)
    ap.add_argument("--charging", action="store_true")
    ap.add_argument("--no-battery", action="store_true")
    ap.add_argument("--network", default="wired",
                    choices=["wired", "wireless", "limited", "offline", "none"])
    ap.add_argument("--logind", action="store_true")
    ap.add_argument("--can", default="yes")
    ap.add_argument("--no-hibernate", action="store_true")
    ap.add_argument("--display-manager", action="store_true")
    ap.add_argument("--log")
    args = ap.parse_args()

    DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()
    keep = []

    if not args.no_battery:
        upower = dbus.service.BusName("org.freedesktop.UPower", bus)
        keep.append(upower)
        keep.append(PropObject(bus, "/org/freedesktop/UPower/devices/DisplayDevice",
                               "org.freedesktop.UPower.Device", {
            "IsPresent": dbus.Boolean(True),
            "Type": dbus.UInt32(2),
            "Percentage": dbus.Double(args.battery),
            # 1 charging, 2 discharging
            "State": dbus.UInt32(1 if args.charging else 2),
            "TimeToEmpty": dbus.Int64(0 if args.charging else 2 * 3600 + 10 * 60),
            "TimeToFull": dbus.Int64(45 * 60 if args.charging else 0),
        }))

    if args.network != "none":
        nm = dbus.service.BusName("org.freedesktop.NetworkManager", bus)
        keep.append(nm)
        connected = args.network != "offline"
        active = "/org/freedesktop/NetworkManager/ActiveConnection/1"
        keep.append(PropObject(bus, "/org/freedesktop/NetworkManager", "org.freedesktop.NetworkManager", {
            "State": dbus.UInt32(70 if connected else 20),
            "Connectivity": dbus.UInt32(2 if args.network == "limited" else (4 if connected else 1)),
            "PrimaryConnection": dbus.ObjectPath(active if connected else "/"),
        }))
        if connected:
            wireless = args.network == "wireless"
            keep.append(PropObject(bus, active, "org.freedesktop.NetworkManager.Connection.Active", {
                "Type": "802-11-wireless" if wireless else "802-3-ethernet",
                "Id": "Meadow Wi-Fi" if wireless else "Wired connection 1",
            }))

    if args.logind:
        keep.append(dbus.service.BusName("org.freedesktop.login1", bus))
        keep.append(Logind(bus, args))
    if args.display_manager:
        keep.append(dbus.service.BusName("org.freedesktop.DisplayManager", bus))
        keep.append(Seat(bus, args))

    loop = GLib.MainLoop()
    signal.signal(signal.SIGTERM, lambda *a: loop.quit())
    print("ready", flush=True)
    loop.run()


if __name__ == "__main__":
    main()
