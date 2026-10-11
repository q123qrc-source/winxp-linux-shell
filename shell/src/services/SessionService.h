// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QDBusUnixFileDescriptor>
#include <QObject>

// Ending or leaving the session (Log Off dialog).
//   logOff()      asks labwc to exit (`labwc --exit`, so applications get a
//                 normal close), or, outside labwc, logind's TerminateSession.
//   switchUser()  org.freedesktop.DisplayManager.Seat.SwitchToGreeter on the
//                 seat in XDG_SEAT_PATH (set by SDDM and LightDM); the
//                 session keeps running behind the greeter.
// It also follows logind for the lock screen:
//   lockRequested()/unlockRequested()  the session's Lock/Unlock signals
//                 (`loginctl lock-session`, idle managers, the greeter)
//   aboutToSleep() PrepareForSleep(true); while lockBeforeSleep is set a
//                 delay inhibitor is held so the lock can appear before the
//                 machine sleeps; call sleepReady() once locked
//   setLockedHint() tells logind whether the session is locked
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

    // Hold a sleep delay inhibitor so the session can lock before sleeping.
    void setLockBeforeSleep(bool enabled);
    // Releases the sleep inhibitor (the lock is up, or locking failed).
    void sleepReady();
    void setLockedHint(bool locked);

Q_SIGNALS:
    void changed();
    void failed(const QString &action, const QString &message);
    void simulated(const QString &action);
    void lockRequested();
    void unlockRequested();
    void aboutToSleep();
    void resumed();

private:
    QDBusConnection m_bus;
    bool m_simulate;
    bool m_canSwitchUser = false;
    void watchLogind();
    void takeSleepInhibitor();

private Q_SLOTS:
    // logind signal receivers (connected by name with QDBusConnection::connect).
    void onLock() { Q_EMIT lockRequested(); }
    void onUnlock() { Q_EMIT unlockRequested(); }
    void onPrepareForSleep(bool start);

private:
    QString m_seatPath;
    QString m_sessionPath;
    bool m_lockBeforeSleep = false;
    QDBusUnixFileDescriptor m_sleepInhibitor;
};
