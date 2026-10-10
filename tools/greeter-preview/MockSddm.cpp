// SPDX-License-Identifier: GPL-3.0-or-later
#include "MockSddm.h"

#include <QSettings>
#include <QTimer>

#include <cstdio>

namespace {
void report(const QString &line)
{
    // stdout, so tests can assert on actions; flushed for subprocess readers.
    std::printf("sddm-mock: %s\n", qPrintable(line));
    std::fflush(stdout);
}
} // namespace

MockGreeterProxy::MockGreeterProxy(bool powerActions, QObject *parent)
    : QObject(parent)
    , m_power(powerActions)
{
}

void MockGreeterProxy::powerOff() { report(QStringLiteral("powerOff")); }
void MockGreeterProxy::reboot() { report(QStringLiteral("reboot")); }
void MockGreeterProxy::suspend() { report(QStringLiteral("suspend")); }
void MockGreeterProxy::hibernate() { report(QStringLiteral("hibernate")); }
void MockGreeterProxy::hybridSleep() { report(QStringLiteral("hybridSleep")); }

void MockGreeterProxy::login(const QString &user, const QString &password, int sessionIndex)
{
    if (m_authActive) {
        report(QStringLiteral("login dropped: authentication ongoing (user=%1)").arg(user));
        return;
    }
    m_authActive = true;

    if (password.isEmpty() && !m_fingerprint.isEmpty()) {
        // pam_fprintd first: prompt, then match or fail; pam_unix then fails
        // on the empty password, which SDDM reports as loginFailed.
        report(QStringLiteral("login user=%1 session=%2 fingerprint=%3").arg(user).arg(sessionIndex).arg(m_fingerprint));
        QTimer::singleShot(100, this, [this] { Q_EMIT informationMessage(QStringLiteral("Place your finger on the fingerprint reader")); });
        if (m_fingerprint == QLatin1String("pending"))
            return;
        const bool ok = m_fingerprint == QLatin1String("success");
        if (!ok)
            QTimer::singleShot(450, this, [this] { Q_EMIT informationMessage(QStringLiteral("Failed to match fingerprint")); });
        QTimer::singleShot(600, this, [this, ok] { finishAuth(ok); });
        return;
    }

    const bool ok = password == QLatin1String(kPassword);
    // Never echo the password, not even in the mock.
    report(QStringLiteral("login user=%1 session=%2 result=%3").arg(user).arg(sessionIndex).arg(ok ? QStringLiteral("success") : QStringLiteral("failure")));
    QTimer::singleShot(150, this, [this, ok] { finishAuth(ok); });
}

void MockGreeterProxy::finishAuth(bool success)
{
    m_authActive = false;
    if (success)
        Q_EMIT loginSucceeded();
    else
        Q_EMIT loginFailed();
}

MockUserModel::MockUserModel(const QList<MockUser> &users, QObject *parent)
    : QAbstractListModel(parent)
    , m_users(users)
{
}

int MockUserModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_users.size());
}

QVariant MockUserModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_users.size())
        return {};
    const MockUser &u = m_users.at(index.row());
    switch (role) {
    case NameRole: return u.name;
    case RealNameRole: return u.realName;
    case HomeDirRole: return QStringLiteral("/home/") + u.name;
    case IconRole: return u.icon;
    case NeedsPasswordRole: return u.needsPassword;
    default: return {};
    }
}

QHash<int, QByteArray> MockUserModel::roleNames() const
{
    return {{NameRole, "name"}, {RealNameRole, "realName"}, {HomeDirRole, "homeDir"}, {IconRole, "icon"}, {NeedsPasswordRole, "needsPassword"}};
}

MockSessionModel::MockSessionModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_names{QStringLiteral("Meadow"), QStringLiteral("Plasma (Wayland)"), QStringLiteral("GNOME"), QStringLiteral("Openbox (X11)")}
{
}

int MockSessionModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_names.size());
}

QVariant MockSessionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_names.size())
        return {};
    const QString &name = m_names.at(index.row());
    switch (role) {
    case NameRole: return name;
    case FileRole: return name.toLower().section(QLatin1Char(' '), 0, 0) + QLatin1String(".desktop");
    case DirectoryRole: return QStringLiteral("/usr/share/wayland-sessions");
    case TypeRole: return 1;
    case ExecRole: return name.toLower();
    case CommentRole: return QString();
    default: return {};
    }
}

QHash<int, QByteArray> MockSessionModel::roleNames() const
{
    return {{DirectoryRole, "directory"}, {FileRole, "file"}, {TypeRole, "type"}, {NameRole, "name"}, {ExecRole, "exec"}, {CommentRole, "comment"}};
}

MockKeyboardModel::MockKeyboardModel(QObject *parent)
    : QObject(parent)
{
    m_layouts << new MockKeyboardLayout(QStringLiteral("us"), QStringLiteral("English (US)"), this)
              << new MockKeyboardLayout(QStringLiteral("ar"), QStringLiteral("Arabic"), this)
              << new MockKeyboardLayout(QStringLiteral("de"), QStringLiteral("German"), this);
}

void MockKeyboardModel::setCurrentLayout(int id)
{
    if (id == m_current || id < 0 || id >= m_layouts.size())
        return;
    m_current = id;
    report(QStringLiteral("layout %1").arg(m_layouts.at(id)->property("shortName").toString()));
    Q_EMIT currentLayoutChanged();
}

QVariant MockScreenModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};
    if (role == Qt::UserRole + 1)
        return QStringLiteral("preview");
    if (role == Qt::UserRole + 2)
        return m_geometry;
    return {};
}

QHash<int, QByteArray> MockScreenModel::roleNames() const
{
    return {{Qt::UserRole + 1, "name"}, {Qt::UserRole + 2, "geometry"}};
}

MockThemeConfig::MockThemeConfig(const QString &themeConf, const QStringList &overrides, QObject *parent)
    : QQmlPropertyMap(this, parent)
{
    QSettings settings(themeConf, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("General"));
    const QStringList keys = settings.childKeys();
    for (const QString &key : keys)
        insert(key, settings.value(key));
    for (const QString &kv : overrides) {
        const qsizetype eq = kv.indexOf(QLatin1Char('='));
        if (eq > 0)
            insert(kv.left(eq), kv.mid(eq + 1));
    }
}
