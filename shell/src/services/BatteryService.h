// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>

class DBusProperties;

// Battery state from UPower's display device (the combined battery).
// `present` is false on desktops without a battery (the tray icon hides).
class BatteryService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool present READ present NOTIFY changed)
    Q_PROPERTY(int percent READ percent NOTIFY changed)
    Q_PROPERTY(bool charging READ charging NOTIFY changed)
    Q_PROPERTY(bool onBattery READ onBattery NOTIFY changed)
    Q_PROPERTY(int minutesLeft READ minutesLeft NOTIFY changed)

public:
    explicit BatteryService(QObject *parent = nullptr);
    BatteryService(const QDBusConnection &bus, QObject *parent = nullptr);

    bool present() const;
    int percent() const;
    bool charging() const;
    bool onBattery() const;
    // Minutes to empty (discharging) or to full (charging); -1 if unknown.
    int minutesLeft() const;

Q_SIGNALS:
    void changed();

private:
    DBusProperties *m_props;
};
