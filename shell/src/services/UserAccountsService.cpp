// SPDX-License-Identifier: GPL-3.0-or-later
#include "UserAccountsService.h"

#include "ThemeLoader.h"

#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QLoggingCategory>

#include <unistd.h>

Q_LOGGING_CATEGORY(lcAccounts, "meadow.shell.accounts")

namespace {
const QString kService = QStringLiteral("org.freedesktop.Accounts");
const QString kPath = QStringLiteral("/org/freedesktop/Accounts");
const QString kUserIface = QStringLiteral("org.freedesktop.Accounts.User");
}

UserAccountsService::UserAccountsService(const QDBusConnection &bus, bool simulate, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
    , m_simulate(simulate || !bus.isConnected())
{
    refresh();
}

void UserAccountsService::refresh()
{
    if (m_simulate) {
        const QString me = qEnvironmentVariable("USER", QStringLiteral("meadow"));
        m_available = true;
        m_users = {
            QVariantMap{{QStringLiteral("name"), me}, {QStringLiteral("realName"), QStringLiteral("Meadow User")},
                        {QStringLiteral("iconFile"), QString()}, {QStringLiteral("admin"), true},
                        {QStringLiteral("self"), true}, {QStringLiteral("path"), QStringLiteral("/sim/1000")}},
            QVariantMap{{QStringLiteral("name"), QStringLiteral("guest")}, {QStringLiteral("realName"), QStringLiteral("Guest")},
                        {QStringLiteral("iconFile"), QString()}, {QStringLiteral("admin"), false},
                        {QStringLiteral("self"), false}, {QStringLiteral("path"), QStringLiteral("/sim/1001")}},
        };
        Q_EMIT changed();
        return;
    }
    const QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kService, QStringLiteral("ListCachedUsers"));
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QList<QDBusObjectPath>> reply = *w;
        m_available = !reply.isError();
        m_users.clear();
        Q_EMIT changed();
        if (!m_available) {
            qCInfo(lcAccounts) << "AccountsService unavailable";
            return;
        }
        for (const QDBusObjectPath &p : reply.value())
            loadUser(p.path());
    });
}

void UserAccountsService::loadUser(const QString &path)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, path, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                      QStringLiteral("GetAll"));
    msg << kUserIface;
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, path](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QVariantMap> reply = *w;
        if (reply.isError())
            return;
        const QVariantMap p = reply.value();
        if (p.value(QStringLiteral("SystemAccount")).toBool())
            return;
        const QVariantMap user{{QStringLiteral("name"), p.value(QStringLiteral("UserName"))},
                               {QStringLiteral("realName"), p.value(QStringLiteral("RealName"))},
                               {QStringLiteral("iconFile"), p.value(QStringLiteral("IconFile"))},
                               {QStringLiteral("admin"), p.value(QStringLiteral("AccountType")).toInt() == 1},
                               {QStringLiteral("self"), p.value(QStringLiteral("Uid")).toULongLong() == getuid()},
                               {QStringLiteral("path"), path}};
        // Replace or add, keeping the list ordered by name.
        for (int i = 0; i < m_users.size(); ++i) {
            if (m_users.at(i).toMap().value(QStringLiteral("path")) == path) {
                m_users[i] = user;
                Q_EMIT changed();
                return;
            }
        }
        m_users << user;
        std::sort(m_users.begin(), m_users.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap().value(QStringLiteral("name")).toString() < b.toMap().value(QStringLiteral("name")).toString();
        });
        Q_EMIT changed();
    });
}

void UserAccountsService::set(const QString &path, const QString &method, const QVariant &value)
{
    qCInfo(lcAccounts) << method << (m_simulate ? "(simulated)" : "");
    if (m_simulate) {
        for (QVariant &u : m_users) {
            QVariantMap m = u.toMap();
            if (m.value(QStringLiteral("path")) == path) {
                m.insert(method == QLatin1String("SetIconFile") ? QStringLiteral("iconFile") : QStringLiteral("realName"), value);
                u = m;
            }
        }
        Q_EMIT changed();
        return;
    }
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, path, kUserIface, method);
    msg << value;
    msg.setInteractiveAuthorizationAllowed(true);
    auto *w = new QDBusPendingCallWatcher(m_bus.asyncCall(msg, 5 * 60 * 1000), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, path, method](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (w->isError()) {
            qCWarning(lcAccounts) << method << "failed:" << w->error().name();
            Q_EMIT failed(tr("The account could not be changed."));
        }
        loadUser(path);
    });
}

void UserAccountsService::setIconFile(const QString &path, const QString &file)
{
    set(path, QStringLiteral("SetIconFile"), file);
}

void UserAccountsService::setRealName(const QString &path, const QString &name)
{
    set(path, QStringLiteral("SetRealName"), name.trimmed());
}

QStringList UserAccountsService::pictures() const
{
    QStringList out;
    for (const QString &dir : ThemeLoader::assetDirs()) {
        const QFileInfoList files = QDir(dir).entryInfoList({QStringLiteral("avatar-*.svg"), QStringLiteral("avatar-*.png")},
                                                            QDir::Files, QDir::Name);
        for (const QFileInfo &fi : files)
            out << fi.absoluteFilePath();
        if (!out.isEmpty())
            break;
    }
    return out;
}
