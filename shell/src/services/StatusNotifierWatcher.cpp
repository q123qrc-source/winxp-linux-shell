// SPDX-License-Identifier: GPL-3.0-or-later
#include "StatusNotifierWatcher.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcSniWatcher, "meadow.shell.tray")

StatusNotifierWatcher::StatusNotifierWatcher(QObject *parent)
    : QObject(parent)
    , m_serviceWatcher(new QDBusServiceWatcher(this))
{
    m_serviceWatcher->setConnection(QDBusConnection::sessionBus());
    m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, &StatusNotifierWatcher::serviceGone);
}

bool StatusNotifierWatcher::registerOnSessionBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(QString::fromLatin1(kPath), this,
                            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties))
        return false;
    if (!bus.registerService(QString::fromLatin1(kService))) {
        bus.unregisterObject(QString::fromLatin1(kPath));
        return false;
    }
    qCInfo(lcSniWatcher) << "providing" << kService;
    return true;
}

void StatusNotifierWatcher::RegisterStatusNotifierItem(const QString &serviceOrPath)
{
    // Items pass either their bus name or (libappindicator style) an object path.
    QString service = serviceOrPath;
    QString path = QStringLiteral("/StatusNotifierItem");
    if (serviceOrPath.startsWith(QLatin1Char('/'))) {
        service = calledFromDBus() ? message().service() : QString();
        path = serviceOrPath;
    }
    if (service.isEmpty())
        return;
    const QString item = service + path;
    if (m_items.contains(item))
        return;
    m_items.append(item);
    m_serviceWatcher->addWatchedService(service);
    qCInfo(lcSniWatcher) << "item registered" << item;
    Q_EMIT StatusNotifierItemRegistered(item);
}

void StatusNotifierWatcher::RegisterStatusNotifierHost(const QString &service)
{
    if (m_hosts.contains(service))
        return;
    m_hosts.append(service);
    m_serviceWatcher->addWatchedService(service);
    Q_EMIT StatusNotifierHostRegistered();
}

void StatusNotifierWatcher::serviceGone(const QString &service)
{
    m_hosts.removeAll(service);
    for (int i = m_items.size() - 1; i >= 0; --i) {
        const QString item = m_items.at(i);
        if (item.startsWith(service + QLatin1Char('/'))) {
            m_items.removeAt(i);
            Q_EMIT StatusNotifierItemUnregistered(item);
        }
    }
    m_serviceWatcher->removeWatchedService(service);
}
