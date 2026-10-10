// SPDX-License-Identifier: GPL-3.0-or-later
#include "Launcher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcLaunch, "meadow.shell.launch")

Launcher::Launcher(QObject *parent)
    : QObject(parent)
{
}

Launcher *Launcher::instance()
{
    static Launcher launcher;
    return &launcher;
}

bool Launcher::available(const QString &program)
{
    if (program.isEmpty())
        return false;
    if (program.contains(QLatin1Char('/'))) {
        const QFileInfo fi(program);
        return fi.isFile() && fi.isExecutable();
    }
    return !QStandardPaths::findExecutable(program).isEmpty();
}

bool Launcher::start(const QString &program, const QStringList &args)
{
    const QString log = qEnvironmentVariable("MEADOW_LAUNCH_LOG");
    if (!log.isEmpty()) {
        QFile f(log);
        if (!f.open(QIODevice::Append | QIODevice::Text))
            return false;
        f.write((QStringList{program} + args).join(QLatin1Char('\t')).toUtf8() + '\n');
        Q_EMIT instance()->started(program, args);
        return true;
    }
    qint64 pid = 0;
    const bool ok = QProcess::startDetached(program, args, QDir::homePath(), &pid);
    // Arguments can be document paths; only the program name is logged.
    qCInfo(lcLaunch) << "start" << program << (ok ? "pid" : "failed") << pid;
    if (ok)
        Q_EMIT instance()->started(program, args);
    return ok;
}

bool Launcher::openFolder(const QString &pathOrUri)
{
    QStringList argv = QProcess::splitCommand(m_fileManager);
    if (!argv.isEmpty() && available(argv.first())) {
        const QString program = argv.takeFirst();
        return start(program, argv << pathOrUri);
    }
    return open(pathOrUri);
}

bool Launcher::open(const QString &pathOrUrl)
{
    return start(QStringLiteral("xdg-open"), {pathOrUrl});
}
