// SPDX-License-Identifier: GPL-3.0-or-later
#include "BatteryService.h"
#include "DBusProperties.h"

#include <cmath>

namespace {
// UPower Device.State
enum { Charging = 1, Discharging = 2, FullyCharged = 4, PendingCharge = 5, PendingDischarge = 6 };
}

BatteryService::BatteryService(QObject *parent)
    : BatteryService(QDBusConnection::systemBus(), parent)
{
}

BatteryService::BatteryService(const QDBusConnection &bus, QObject *parent)
    : QObject(parent)
    , m_props(new DBusProperties(bus, QStringLiteral("org.freedesktop.UPower"),
                                 QStringLiteral("/org/freedesktop/UPower/devices/DisplayDevice"),
                                 QStringLiteral("org.freedesktop.UPower.Device"), this))
{
    connect(m_props, &DBusProperties::fetched, this, &BatteryService::changed);
    connect(m_props, &DBusProperties::changed, this, &BatteryService::changed);
}

bool BatteryService::present() const
{
    // Type 2 = battery; desktops report a display device that is not present.
    return m_props->valid() && m_props->value(QStringLiteral("IsPresent")).toBool()
        && m_props->value(QStringLiteral("Type")).toUInt() == 2;
}

int BatteryService::percent() const
{
    return int(std::lround(m_props->value(QStringLiteral("Percentage")).toDouble()));
}

bool BatteryService::charging() const
{
    const uint s = m_props->value(QStringLiteral("State")).toUInt();
    return s == Charging || s == PendingCharge;
}

bool BatteryService::onBattery() const
{
    const uint s = m_props->value(QStringLiteral("State")).toUInt();
    return s == Discharging || s == PendingDischarge;
}

int BatteryService::minutesLeft() const
{
    const qint64 secs = charging() ? m_props->value(QStringLiteral("TimeToFull")).toLongLong()
                                   : m_props->value(QStringLiteral("TimeToEmpty")).toLongLong();
    return secs > 0 ? int(secs / 60) : -1;
}
