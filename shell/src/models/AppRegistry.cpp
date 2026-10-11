// SPDX-License-Identifier: GPL-3.0-or-later
#include "AppRegistry.h"
#include "services/Launcher.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QLoggingCategory>
#include <QProcess>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcApps, "meadow.shell.apps")

AppRegistry::AppRegistry(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
    , m_reloadTimer(new QTimer(this))
{
    // Package installs touch many files at once; reload once they settle.
    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(500);
    connect(m_reloadTimer, &QTimer::timeout, this, [this] {
        reload();
        Q_EMIT changed();
    });
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, m_reloadTimer, qOverload<>(&QTimer::start));
    reload();
}

QStringList AppRegistry::currentDesktops()
{
    const QStringList desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':'), Qt::SkipEmptyParts);
    return desktops.isEmpty() ? QStringList{QStringLiteral("Meadow"), QStringLiteral("X-Meadow")} : desktops;
}

bool AppRegistry::isShown(const QString &desktopId) const
{
    const auto it = m_entries.constFind(desktopId);
    return it != m_entries.cend() && it->isValid() && it->shownIn(currentDesktops());
}

void AppRegistry::watchDirs()
{
    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    QStringList dirs;
    for (const QString &dir : applicationDirs()) {
        if (QFileInfo::exists(dir)) {
            dirs << dir;
            QDirIterator it(dir, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext())
                dirs << it.next();
        } else if (QFileInfo::exists(QFileInfo(dir).absolutePath())) {
            // Notice applications/ being created (e.g. the first user-installed app).
            dirs << QFileInfo(dir).absolutePath();
        }
    }
    if (!dirs.isEmpty())
        m_watcher->addPaths(dirs);
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
    watchDirs();
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
    const bool ok = Launcher::start(program, argv);
    qCInfo(lcApps) << "launch" << desktopId << (ok ? "ok" : "failed");
    if (ok)
        Q_EMIT const_cast<AppRegistry *>(this)->launched(desktopId);
    return ok;
}
