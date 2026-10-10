// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>

class DBusProperties;

// Network status from NetworkManager: connected or not, wired or wireless,
// the primary connection's name, and limited connectivity.
class NetworkService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool limited READ limited NOTIFY changed)
    Q_PROPERTY(QString kind READ kind NOTIFY changed)
    Q_PROPERTY(QString connectionName READ connectionName NOTIFY changed)

public:
    explicit NetworkService(QObject *parent = nullptr);
    NetworkService(const QDBusConnection &bus, QObject *parent = nullptr);

    bool available() const;
    bool connected() const;
    bool limited() const;
    // "wired", "wireless", "other" or "none".
    QString kind() const { return m_kind; }
    QString connectionName() const { return m_name; }

    // Opens the system's network settings tool, if one is installed.
    Q_INVOKABLE bool openSettings() const;

Q_SIGNALS:
    void changed();

private:
    void updatePrimary();

    QDBusConnection m_bus;
    DBusProperties *m_nm;
    DBusProperties *m_primary = nullptr;
    QString m_primaryPath;
    QString m_kind = QStringLiteral("none");
    QString m_name;
};
