// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QDateTime>
#include <QObject>
#include <QStringList>

// Control Panel > Date and Time: systemd-timedated (org.freedesktop.timedate1)
// for the time zone, network time and setting the clock. Changes ask polkit
// interactively (the session's agent shows the Authenticate dialog).
// In simulate mode (--preview, tests without a bus) nothing is changed.
class TimeDateService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString timezone READ timezone NOTIFY changed)
    Q_PROPERTY(bool ntp READ ntp NOTIFY changed)
    Q_PROPERTY(bool canNtp READ canNtp NOTIFY changed)
    Q_PROPERTY(QStringList timezones READ timezones CONSTANT)

public:
    TimeDateService(const QDBusConnection &bus, bool simulate, QObject *parent = nullptr);

    bool available() const { return m_available; }
    QString timezone() const { return m_timezone; }
    bool ntp() const { return m_ntp; }
    bool canNtp() const { return m_canNtp; }
    // IANA zone ids known to Qt (the system tz database), sorted.
    QStringList timezones() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setTimezone(const QString &zone);
    Q_INVOKABLE void setNtp(bool on);
    // Sets the system clock (only while network time is off).
    Q_INVOKABLE void setTime(const QDateTime &time);

Q_SIGNALS:
    void changed();
    // A change failed or was not authorized (message for the dialog).
    void failed(const QString &message);

private:
    void call(const QString &method, const QVariantList &args);

    QDBusConnection m_bus;
    bool m_simulate;
    bool m_available = false;
    bool m_ntp = false;
    bool m_canNtp = false;
    QString m_timezone;
};
