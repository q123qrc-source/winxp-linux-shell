// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QVariantList>

// Control Panel > User Accounts, through AccountsService
// (org.freedesktop.Accounts). Lists the accounts and lets the user change
// their own picture and display name; changes to other accounts need an
// administrator, which polkit asks for through the session's agent.
//
// `users`: [{name, realName, iconFile, admin, self, path}].
// Simulated in --preview (and with no bus): a few sample accounts.
class UserAccountsService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QVariantList users READ users NOTIFY changed)

public:
    UserAccountsService(const QDBusConnection &bus, bool simulate, QObject *parent = nullptr);

    bool available() const { return m_available; }
    QVariantList users() const { return m_users; }

    Q_INVOKABLE void refresh();
    // `path` is the account's object path from `users`.
    Q_INVOKABLE void setIconFile(const QString &path, const QString &file);
    Q_INVOKABLE void setRealName(const QString &path, const QString &name);
    // Meadow's own account pictures (avatar-*.svg), as file paths.
    Q_INVOKABLE QStringList pictures() const;

Q_SIGNALS:
    void changed();
    void failed(const QString &message);

private:
    void loadUser(const QString &path);
    void set(const QString &path, const QString &method, const QVariant &value);

    QDBusConnection m_bus;
    bool m_simulate;
    bool m_available = false;
    QVariantList m_users;
};
