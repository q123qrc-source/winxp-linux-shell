// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusContext>
#include <QObject>
#include <QStringList>

class QDBusServiceWatcher;

// org.kde.StatusNotifierWatcher: the registry tray items announce themselves
// to. meadow-shell provides it when no other desktop component does.
// Items are kept as "<bus name><object path>", e.g. ":1.42/StatusNotifierItem".
class StatusNotifierWatcher : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierWatcher")
    Q_PROPERTY(QStringList RegisteredStatusNotifierItems READ registeredItems)
    Q_PROPERTY(bool IsStatusNotifierHostRegistered READ isHostRegistered)
    Q_PROPERTY(int ProtocolVersion READ protocolVersion)

public:
    static constexpr const char *kService = "org.kde.StatusNotifierWatcher";
    static constexpr const char *kPath = "/StatusNotifierWatcher";

    explicit StatusNotifierWatcher(QObject *parent = nullptr);
    // Claims the name on the session bus; false if another watcher runs.
    bool registerOnSessionBus();

    QStringList registeredItems() const { return m_items; }
    bool isHostRegistered() const { return !m_hosts.isEmpty(); }
    int protocolVersion() const { return 0; }

public Q_SLOTS:
    void RegisterStatusNotifierItem(const QString &serviceOrPath);
    void RegisterStatusNotifierHost(const QString &service);

Q_SIGNALS:
    void StatusNotifierItemRegistered(const QString &item);
    void StatusNotifierItemUnregistered(const QString &item);
    void StatusNotifierHostRegistered();

private:
    void serviceGone(const QString &service);

    QDBusServiceWatcher *m_serviceWatcher;
    QStringList m_items;
    QStringList m_hosts;
};
