// SPDX-License-Identifier: GPL-3.0-or-later
#include "DBusProperties.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

DBusProperties::DBusProperties(const QDBusConnection &bus, const QString &service, const QString &path,
                               const QString &interface, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
    , m_service(service)
    , m_path(path)
    , m_interface(interface)
{
    m_bus.connect(m_service, m_path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"),
                  this, SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
    fetch();
}

void DBusProperties::fetch()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(m_service, m_path, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                      QStringLiteral("GetAll"));
    msg << m_interface;
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg, 3000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QVariantMap> reply = *w;
        m_valid = !reply.isError();
        m_values = m_valid ? reply.value() : QVariantMap();
        Q_EMIT fetched();
    });
}

void DBusProperties::onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated)
{
    if (interface != m_interface)
        return;
    for (auto it = changed.cbegin(); it != changed.cend(); ++it)
        m_values.insert(it.key(), it.value());
    if (!invalidated.isEmpty()) {
        fetch();
        return;
    }
    Q_EMIT this->changed();
}
