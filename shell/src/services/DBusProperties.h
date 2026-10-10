// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QVariantMap>
#include <functional>

// Small helper for services that mirror D-Bus properties: an async GetAll
// plus a PropertiesChanged subscription for one object/interface.
class DBusProperties : public QObject
{
    Q_OBJECT

public:
    DBusProperties(const QDBusConnection &bus, const QString &service, const QString &path,
                   const QString &interface, QObject *parent = nullptr);

public Q_SLOTS:
    // Fetch everything again; `fetched` reports the full map (empty on error).
    void fetch();

public:
    QVariantMap values() const { return m_values; }
    QVariant value(const QString &name) const { return m_values.value(name); }
    bool valid() const { return m_valid; }

Q_SIGNALS:
    void fetched();
    void changed();

private Q_SLOTS:
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);

private:
    QDBusConnection m_bus;
    QString m_service, m_path, m_interface;
    QVariantMap m_values;
    bool m_valid = false;
};
