// SPDX-License-Identifier: GPL-3.0-or-later
#include "NetworkService.h"
#include "DBusProperties.h"

#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QProcess>
#include <QStandardPaths>

namespace {
const QString kService = QStringLiteral("org.freedesktop.NetworkManager");
// NM_STATE_CONNECTED_SITE = 60, CONNECTED_GLOBAL = 70; connectivity FULL = 4.
constexpr uint kConnectedSite = 60;
constexpr uint kConnectedGlobal = 70;
constexpr uint kConnectivityFull = 4;
}

NetworkService::NetworkService(QObject *parent)
    : NetworkService(QDBusConnection::systemBus(), parent)
{
}

NetworkService::NetworkService(const QDBusConnection &bus, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
    , m_nm(new DBusProperties(bus, kService, QStringLiteral("/org/freedesktop/NetworkManager"), kService, this))
{
    connect(m_nm, &DBusProperties::fetched, this, &NetworkService::updatePrimary);
    connect(m_nm, &DBusProperties::changed, this, &NetworkService::updatePrimary);
}

bool NetworkService::available() const
{
    return m_nm->valid();
}

bool NetworkService::connected() const
{
    const uint state = m_nm->value(QStringLiteral("State")).toUInt();
    return state == kConnectedSite || state == kConnectedGlobal;
}

bool NetworkService::limited() const
{
    return connected() && m_nm->value(QStringLiteral("Connectivity")).toUInt() != kConnectivityFull;
}

void NetworkService::updatePrimary()
{
    const QString path = qdbus_cast<QDBusObjectPath>(m_nm->value(QStringLiteral("PrimaryConnection"))).path();
    if (path != m_primaryPath) {
        m_primaryPath = path;
        delete m_primary;
        m_primary = nullptr;
        if (!path.isEmpty() && path != QLatin1String("/")) {
            m_primary = new DBusProperties(m_bus, kService, path, QStringLiteral("org.freedesktop.NetworkManager.Connection.Active"), this);
            const auto apply = [this] {
                const QString type = m_primary->value(QStringLiteral("Type")).toString();
                m_kind = type == QLatin1String("802-11-wireless") ? QStringLiteral("wireless")
                       : type == QLatin1String("802-3-ethernet") ? QStringLiteral("wired")
                       : type.isEmpty() ? QStringLiteral("none") : QStringLiteral("other");
                m_name = m_primary->value(QStringLiteral("Id")).toString();
                Q_EMIT changed();
            };
            connect(m_primary, &DBusProperties::fetched, this, apply);
            connect(m_primary, &DBusProperties::changed, this, apply);
            return;
        }
        m_kind = QStringLiteral("none");
        m_name.clear();
    }
    Q_EMIT changed();
}

bool NetworkService::openSettings() const
{
    const QString exe = QStandardPaths::findExecutable(QStringLiteral("nm-connection-editor"));
    return !exe.isEmpty() && QProcess::startDetached(exe, {});
}
