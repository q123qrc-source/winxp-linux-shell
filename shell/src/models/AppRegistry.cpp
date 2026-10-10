// SPDX-License-Identifier: GPL-3.0-or-later
#include "AppRegistry.h"

#include <QDir>
#include <QDirIterator>
#include <QLoggingCategory>
#include <QProcess>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcApps, "meadow.shell.apps")

AppRegistry::AppRegistry(QObject *parent)
    : QObject(parent)
{
    reload();
}

QStringList AppRegistry::applicationDirs()
{
    // GenericDataLocation = XDG_DATA_HOME then XDG_DATA_DIRS, in priority order.
    QStringList dirs;
    const QStringList data = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &d : data)
        dirs << d + QLatin1String("/applications");
    return dirs;
}

void AppRegistry::reload()
{
    m_entries.clear();
    m_byLowerId.clear();
    m_byWmClass.clear();
    for (const QString &dir : applicationDirs()) {
        QDirIterator it(dir, {QStringLiteral("*.desktop")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            // Desktop file id: path relative to applications/, '/' -> '-'.
            const QString id = QDir(dir).relativeFilePath(path).replace(QLatin1Char('/'), QLatin1Char('-'));
            if (m_entries.contains(id))
                continue; // a higher-priority directory already provided it
            const DesktopEntry e = DesktopEntry::parse(path, id);
            if (!e.isValid())
                continue;
            m_entries.insert(id, e);
            m_byLowerId.insert(id.toLower(), id);
            if (!e.startupWMClass.isEmpty())
                m_byWmClass.insert(e.startupWMClass.toLower(), id);
        }
    }
    qCDebug(lcApps) << m_entries.size() << "applications";
}

QString AppRegistry::desktopIdForApp(const QString &appId) const
{
    if (appId.isEmpty())
        return {};
    const QString file = appId + QLatin1String(".desktop");
    if (m_entries.contains(file))
        return file;
    const QString lower = file.toLower();
    if (auto it = m_byLowerId.constFind(lower); it != m_byLowerId.cend())
        return *it;
    if (auto it = m_byWmClass.constFind(appId.toLower()); it != m_byWmClass.cend())
        return *it;
    // org.example.App -> app.desktop
    const QString last = appId.section(QLatin1Char('.'), -1).toLower() + QLatin1String(".desktop");
    if (auto it = m_byLowerId.constFind(last); it != m_byLowerId.cend())
        return *it;
    return {};
}

QString AppRegistry::iconForApp(const QString &appId) const
{
    const QString id = desktopIdForApp(appId);
    return id.isEmpty() ? QString() : m_entries.value(id).icon;
}

QString AppRegistry::nameForApp(const QString &appId) const
{
    const QString id = desktopIdForApp(appId);
    return id.isEmpty() ? appId : m_entries.value(id).name;
}

QString AppRegistry::iconForDesktopId(const QString &desktopId) const
{
    return m_entries.value(desktopId).icon;
}

QString AppRegistry::nameForDesktopId(const QString &desktopId) const
{
    return m_entries.value(desktopId).name;
}

bool AppRegistry::launch(const QString &desktopId, const QStringList &files) const
{
    const auto it = m_entries.constFind(desktopId);
    if (it == m_entries.cend()) {
        qCWarning(lcApps) << "unknown application" << desktopId;
        return false;
    }
    QStringList argv = it->execArguments(files);
    if (argv.isEmpty())
        return false;
    if (it->terminal) {
        // A terminal that understands "-e program args" (xterm-style).
        const QString terminal = qEnvironmentVariable("TERMINAL", QStringLiteral("xterm"));
        argv.prepend(QStringLiteral("-e"));
        argv.prepend(terminal);
    }
    const QString program = argv.takeFirst();
    qint64 pid = 0;
    const bool ok = QProcess::startDetached(program, argv, QDir::homePath(), &pid);
    qCInfo(lcApps) << "launch" << desktopId << (ok ? "pid" : "failed") << pid;
    if (ok)
        Q_EMIT const_cast<AppRegistry *>(this)->launched(desktopId);
    return ok;
}
