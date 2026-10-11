#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Mock UPower, NetworkManager, systemd-logind, the display manager's seat
API, systemd-timedated and AccountsService on the *session* bus, for
screenshots and tests. meadow-shell talks to them when
MEADOW_SYSTEM_SERVICES_ON_SESSION_BUS=1.

  mock-system-services.py [--battery PERCENT] [--charging] [--no-battery]
                          [--network wired|wireless|limited|offline|none]
                          [--logind] [--can yes|no|challenge|na] [--no-hibernate]
                          [--display-manager] [--timedate] [--accounts]
                          [--log FILE]

logind, display manager, timedated and accounts calls are appended to --log
(e.g. "PowerOff true", "SwitchToGreeter", "SetTimezone Europe/Paris true");
nothing is powered off or changed.

The logind mock has one session (/org/freedesktop/login1/session/_31,
returned by GetSession and GetSessionByPID). Tests make it emit signals with
  busctl --user call org.freedesktop.login1 /org/freedesktop/login1 \
      org.meadow.Test Emit s Lock|Unlock|PrepareForSleep|Resume

Runs until SIGTERM. Prints "ready" once the names are owned.
"""
import argparse
import os
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
        self.inhibitors = []
        self.session = LogindSession(bus, args)

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

    @dbus.service.method(IFACE, in_signature="s", out_signature="o")
    def GetSession(self, session_id):
        return dbus.ObjectPath(LogindSession.PATH)

    @dbus.service.method(IFACE, in_signature="u", out_signature="o")
    def GetSessionByPID(self, pid):
        return dbus.ObjectPath(LogindSession.PATH)

    @dbus.service.method(IFACE, in_signature="ssss", out_signature="h")
    def Inhibit(self, what, who, why, mode):
        record(self.args, f"Inhibit {what} {mode}")
        r, w = os.pipe()
        self.inhibitors.append(r)  # held until released by the client
        return dbus.types.UnixFd(w)

    @dbus.service.signal(IFACE, signature="b")
    def PrepareForSleep(self, start):
        pass

    # Test hook: make the mock emit logind's signals.
    @dbus.service.method("org.meadow.Test", in_signature="s")
    def Emit(self, what):
        if what == "Lock":
            self.session.Lock()
        elif what == "Unlock":
            self.session.Unlock()
        elif what == "PrepareForSleep":
            self.PrepareForSleep(True)
        elif what == "Resume":
            self.PrepareForSleep(False)


class LogindSession(dbus.service.Object):
    PATH = "/org/freedesktop/login1/session/_31"
    IFACE = "org.freedesktop.login1.Session"

    def __init__(self, bus, args):
        super().__init__(bus, self.PATH)
        self.args = args

    @dbus.service.method(IFACE, in_signature="b")
    def SetLockedHint(self, locked):
        record(self.args, f"SetLockedHint {bool(locked)}")

    @dbus.service.signal(IFACE)
    def Lock(self):
        pass

    @dbus.service.signal(IFACE)
    def Unlock(self):
        pass


class TimeDate(PropObject):
    IFACE = "org.freedesktop.timedate1"

    def __init__(self, bus, args):
        super().__init__(bus, "/org/freedesktop/timedate1", self.IFACE, {
            "Timezone": "Europe/London", "NTP": dbus.Boolean(True), "CanNTP": dbus.Boolean(True)})
        self.args = args

    def _set(self, name, value):
        self.props[name] = value
        self.PropertiesChanged(self.IFACE, {name: value}, [])

    @dbus.service.method(IFACE, in_signature="sb")
    def SetTimezone(self, zone, interactive):
        record(self.args, f"SetTimezone {zone} {bool(interactive)}")
        self._set("Timezone", zone)

    @dbus.service.method(IFACE, in_signature="bb")
    def SetNTP(self, on, interactive):
        record(self.args, f"SetNTP {bool(on)} {bool(interactive)}")
        self._set("NTP", dbus.Boolean(on))

    @dbus.service.method(IFACE, in_signature="xbb")
    def SetTime(self, usec, relative, interactive):
        record(self.args, f"SetTime {int(usec)} {bool(relative)} {bool(interactive)}")


class AccountsUser(PropObject):
    IFACE = "org.freedesktop.Accounts.User"

    def __init__(self, bus, args, path, props):
        super().__init__(bus, path, self.IFACE, props)
        self.args = args

    @dbus.service.method(IFACE, in_signature="s")
    def SetIconFile(self, path):
        record(self.args, f"SetIconFile {self.props['UserName']} {path}")
        self.props["IconFile"] = path

    @dbus.service.method(IFACE, in_signature="s")
    def SetRealName(self, name):
        record(self.args, f"SetRealName {self.props['UserName']} {name}")
        self.props["RealName"] = name


class Accounts(dbus.service.Object):
    IFACE = "org.freedesktop.Accounts"

    def __init__(self, bus, args):
        super().__init__(bus, "/org/freedesktop/Accounts")
        uid = os.getuid()
        self.users = [
            AccountsUser(bus, args, "/org/freedesktop/Accounts/User1000", {
                "UserName": os.environ.get("USER", "meadow"), "RealName": "Meadow User", "IconFile": "",
                "AccountType": dbus.Int32(1), "Uid": dbus.UInt64(uid), "SystemAccount": dbus.Boolean(False)}),
            AccountsUser(bus, args, "/org/freedesktop/Accounts/User1001", {
                "UserName": "guest", "RealName": "Guest", "IconFile": "",
                "AccountType": dbus.Int32(0), "Uid": dbus.UInt64(uid + 1), "SystemAccount": dbus.Boolean(False)}),
        ]

    @dbus.service.method(IFACE, out_signature="ao")
    def ListCachedUsers(self):
        return [dbus.ObjectPath(u._object_path) for u in self.users]


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
    ap.add_argument("--timedate", action="store_true")
    ap.add_argument("--accounts", action="store_true")
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
    if args.timedate:
        keep.append(dbus.service.BusName("org.freedesktop.timedate1", bus))
        keep.append(TimeDate(bus, args))
    if args.accounts:
        keep.append(dbus.service.BusName("org.freedesktop.Accounts", bus))
        keep.append(Accounts(bus, args))

    loop = GLib.MainLoop()
    signal.signal(signal.SIGTERM, lambda *a: loop.quit())
    print("ready", flush=True)
    loop.run()


if __name__ == "__main__":
    main()
