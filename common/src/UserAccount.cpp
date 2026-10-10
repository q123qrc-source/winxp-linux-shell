// SPDX-License-Identifier: GPL-3.0-or-later
#include "UserAccount.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

namespace {
const QString kService = QStringLiteral("org.freedesktop.Accounts");
const QString kPath = QStringLiteral("/org/freedesktop/Accounts");
const QString kUserInterface = QStringLiteral("org.freedesktop.Accounts.User");
constexpr int kTimeoutMs = 3000;
}

UserAccount::UserAccount(QObject *parent)
    : QObject(parent)
{
}

void UserAccount::setUserName(const QString &name)
{
    if (m_userName == name)
        return;
    m_userName = name;
    Q_EMIT userNameChanged();
    reset();
    fetch();
}

void UserAccount::reset()
{
    const bool wasLoaded = m_loaded;
    m_realName.clear();
    m_passwordHint.clear();
    m_iconFile.clear();
    m_loaded = false;
    if (wasLoaded)
        Q_EMIT loadedChanged();
}

void UserAccount::fetch()
{
    if (m_userName.isEmpty())
        return;
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected())
        return;

    // Ignore replies that arrive after the user name changed again.
    const quint64 generation = ++m_generation;
    QDBusMessage find = QDBusMessage::createMethodCall(kService, kPath, kService, QStringLiteral("FindUserByName"));
    find << m_userName;
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(find, kTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation, bus](QDBusPendingCallWatcher *w) mutable {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (generation != m_generation || reply.isError()) {
            if (reply.isError())
                qCDebug(lcTheme) << "AccountsService lookup failed:" << reply.error().name();
            return;
        }
        QDBusMessage get = QDBusMessage::createMethodCall(kService, reply.value().path(),
                                                          QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"));
        get << kUserInterface;
        auto *props = new QDBusPendingCallWatcher(bus.asyncCall(get, kTimeoutMs), this);
        connect(props, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *pw) {
            pw->deleteLater();
            QDBusPendingReply<QVariantMap> all = *pw;
            if (generation != m_generation || all.isError())
                return;
            const QVariantMap map = all.value();
            m_realName = map.value(QStringLiteral("RealName")).toString();
            m_passwordHint = map.value(QStringLiteral("PasswordHint")).toString();
            m_iconFile = map.value(QStringLiteral("IconFile")).toString();
            m_loaded = true;
            Q_EMIT loadedChanged();
        });
    });
}
