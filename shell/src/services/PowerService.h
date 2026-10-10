// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>

// Power actions through systemd-logind (org.freedesktop.login1.Manager):
// PowerOff, Reboot, Suspend, Hibernate, each shown only when the matching
// CanXxx answers "yes" or "challenge" (polkit will ask). Calls are
// interactive, so polkit can authenticate. The property and method names
// match SDDM's greeter proxy, so the shared Turn Off dialog works with both.
//
// In simulate mode (meadow-shell --preview, tests) nothing is called and
// every action is reported available; requests are only logged.
class PowerService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool canPowerOff READ canPowerOff NOTIFY changed)
    Q_PROPERTY(bool canReboot READ canReboot NOTIFY changed)
    Q_PROPERTY(bool canSuspend READ canSuspend NOTIFY changed)
    Q_PROPERTY(bool canHibernate READ canHibernate NOTIFY changed)
    Q_PROPERTY(bool simulate READ simulate CONSTANT)

public:
    explicit PowerService(bool simulate = false, QObject *parent = nullptr);
    PowerService(const QDBusConnection &bus, bool simulate, QObject *parent = nullptr);

    bool canPowerOff() const { return m_simulate || m_canPowerOff; }
    bool canReboot() const { return m_simulate || m_canReboot; }
    bool canSuspend() const { return m_simulate || m_canSuspend; }
    bool canHibernate() const { return m_simulate || m_canHibernate; }
    bool simulate() const { return m_simulate; }

    Q_INVOKABLE void powerOff() { request(QStringLiteral("PowerOff")); }
    Q_INVOKABLE void reboot() { request(QStringLiteral("Reboot")); }
    Q_INVOKABLE void suspend() { request(QStringLiteral("Suspend")); }
    Q_INVOKABLE void hibernate() { request(QStringLiteral("Hibernate")); }
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void changed();
    // A call was refused or failed (e.g. polkit authentication cancelled).
    void failed(const QString &action, const QString &message);
    // Simulate mode: the action that would have run.
    void simulated(const QString &action);

private:
    void request(const QString &method);

    QDBusConnection m_bus;
    bool m_simulate;
    bool m_canPowerOff = false;
    bool m_canReboot = false;
    bool m_canSuspend = false;
    bool m_canHibernate = false;
};
