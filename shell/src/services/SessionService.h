// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>

// Ending or leaving the session (Log Off dialog).
//   logOff()      asks labwc to exit (`labwc --exit`, so applications get a
//                 normal close), or, outside labwc, logind's TerminateSession.
//   switchUser()  org.freedesktop.DisplayManager.Seat.SwitchToGreeter on the
//                 seat in XDG_SEAT_PATH (set by SDDM and LightDM); the
//                 session keeps running behind the greeter.
// In simulate mode (--preview, tests) nothing happens; requests are logged.
class SessionService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool canSwitchUser READ canSwitchUser NOTIFY changed)
    Q_PROPERTY(bool simulate READ simulate CONSTANT)

public:
    explicit SessionService(bool simulate = false, QObject *parent = nullptr);
    SessionService(const QDBusConnection &bus, bool simulate, QObject *parent = nullptr);

    bool canSwitchUser() const { return m_simulate || m_canSwitchUser; }
    bool simulate() const { return m_simulate; }

    Q_INVOKABLE void logOff();
    Q_INVOKABLE void switchUser();

Q_SIGNALS:
    void changed();
    void failed(const QString &action, const QString &message);
    void simulated(const QString &action);

private:
    QDBusConnection m_bus;
    bool m_simulate;
    bool m_canSwitchUser = false;
    QString m_seatPath;
};
