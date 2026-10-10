// SPDX-License-Identifier: GPL-3.0-or-later
#include "RunService.h"

#include "Launcher.h"
#include "models/AppRegistry.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

Q_LOGGING_CATEGORY(lcRun, "meadow.shell.run")

namespace {
// Shell-style word splitting: '…' and "…" quoting and backslash escapes.
// Only splitting: nothing is expanded or interpreted.
QStringList splitArgs(const QString &text)
{
    QStringList args;
    QString current;
    bool inWord = false;
    QChar quote;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (!quote.isNull()) {
            if (c == quote)
                quote = QChar();
            else if (c == QLatin1Char('\\') && quote == QLatin1Char('"') && i + 1 < text.size())
                current += text.at(++i);
            else
                current += c;
        } else if (c == QLatin1Char('\'') || c == QLatin1Char('"')) {
            quote = c;
            inWord = true;
        } else if (c == QLatin1Char('\\') && i + 1 < text.size()) {
            current += text.at(++i);
            inWord = true;
        } else if (c.isSpace()) {
            if (inWord)
                args << current;
            current.clear();
            inWord = false;
        } else {
            current += c;
            inWord = true;
        }
    }
    if (inWord)
        args << current;
    return args;
}
} // namespace

RunService::RunService(AppRegistry *apps, QObject *parent)
    : RunService(apps, QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/meadow/run-history"), parent)
{
}

RunService::RunService(AppRegistry *apps, const QString &historyFile, QObject *parent)
    : QObject(parent)
    , m_apps(apps)
    , m_historyFile(historyFile)
{
    QFile f(m_historyFile);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        m_history = lines.mid(0, kMaxHistory);
    }
}

QString RunService::expandHome(const QString &path)
{
    if (path == QLatin1String("~"))
        return QDir::homePath();
    if (path.startsWith(QLatin1String("~/")))
        return QDir::homePath() + path.mid(1);
    return path;
}

QString RunService::run(const QString &input)
{
    const QString text = input.trimmed();
    if (text.isEmpty())
        return tr("Type the name of a program, folder, document or Internet address.");

    // A local path (possibly with spaces, so before any splitting; Browse
    // quotes such paths).
    QString unquoted = text;
    if (text.size() > 1 && text.startsWith(QLatin1Char('"')) && text.endsWith(QLatin1Char('"')) && text.count(QLatin1Char('"')) == 2)
        unquoted = text.mid(1, text.size() - 2);
    QString path = expandHome(unquoted);
    if (path.startsWith(QLatin1String("file://")))
        path = QUrl(path).toLocalFile();
    if (QFileInfo(path).isAbsolute() && QFileInfo::exists(path)) {
        const QFileInfo fi(path);
        bool ok;
        if (fi.isDir())
            ok = Launcher::instance()->openFolder(fi.absoluteFilePath());
        else if (fi.isExecutable())
            ok = Launcher::start(fi.absoluteFilePath());
        else
            ok = Launcher::instance()->open(fi.absoluteFilePath());
        if (ok)
            remember(text);
        return ok ? QString() : tr("Meadow could not open “%1”.").arg(text);
    }

    // A URL.
    static const QRegularExpression scheme(QStringLiteral("^[a-zA-Z][a-zA-Z0-9+.-]*://\\S+$|^mailto:\\S+$"));
    if (scheme.match(text).hasMatch()) {
        if (Launcher::instance()->open(text)) {
            remember(text);
            return {};
        }
        return tr("Meadow could not open “%1”.").arg(text);
    }

    // A program with arguments.
    QStringList argv = splitArgs(text);
    if (!argv.isEmpty()) {
        const QString program = expandHome(argv.first());
        if (Launcher::available(program)) {
            argv.removeFirst();
            for (QString &a : argv)
                a = expandHome(a);
            if (Launcher::start(program, argv)) {
                remember(text);
                return {};
            }
            return tr("Meadow could not start “%1”.").arg(program);
        }
    }

    // An application by desktop id or name.
    if (m_apps) {
        QString id = m_apps->hasDesktopId(text) ? text : (m_apps->hasDesktopId(text + QLatin1String(".desktop")) ? text + QLatin1String(".desktop") : QString());
        if (id.isEmpty()) {
            for (auto it = m_apps->entries().cbegin(); it != m_apps->entries().cend(); ++it) {
                if (it->name.compare(text, Qt::CaseInsensitive) == 0 && m_apps->isShown(it.key())) {
                    id = it.key();
                    break;
                }
            }
        }
        if (!id.isEmpty() && m_apps->launch(id)) {
            remember(text);
            return {};
        }
    }
    return tr("Meadow cannot find “%1”. Check the spelling and try again.").arg(text);
}

void RunService::remember(const QString &text)
{
    m_history.removeAll(text);
    m_history.prepend(text);
    while (m_history.size() > kMaxHistory)
        m_history.removeLast();
    saveHistory();
    Q_EMIT historyChanged();
}

void RunService::saveHistory() const
{
    QDir().mkpath(QFileInfo(m_historyFile).absolutePath());
    QSaveFile f(m_historyFile);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(m_history.join(QLatin1Char('\n')).toUtf8() + '\n');
        f.commit();
    }
}

QStringList RunService::complete(const QString &prefix) const
{
    QStringList out;
    if (prefix.isEmpty())
        return out;
    for (const QString &h : m_history) {
        if (h.startsWith(prefix, Qt::CaseInsensitive) && h != prefix)
            out << h;
    }
    const auto add = [&out](const QString &s) {
        if (!out.contains(s) && out.size() < kMaxCompletions)
            out << s;
    };
    if (prefix.startsWith(QLatin1Char('/')) || prefix.startsWith(QLatin1Char('~'))) {
        // Complete the last path component.
        const QString expanded = expandHome(prefix);
        const qsizetype slash = expanded.lastIndexOf(QLatin1Char('/'));
        const QString dir = expanded.left(slash + 1);
        const QString stem = expanded.mid(slash + 1);
        const QString shown = prefix.left(prefix.lastIndexOf(QLatin1Char('/')) + 1);
        const QFileInfoList entries = QDir(dir).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
        for (const QFileInfo &fi : entries) {
            if (fi.fileName().startsWith(QLatin1Char('.')) && !stem.startsWith(QLatin1Char('.')))
                continue;
            if (fi.fileName().startsWith(stem))
                add(shown + fi.fileName() + (fi.isDir() ? QStringLiteral("/") : QString()));
        }
        return out;
    }
    if (prefix.contains(QLatin1Char(' ')))
        return out;
    QStringList programs;
    const QStringList pathDirs = qEnvironmentVariable("PATH").split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &dir : pathDirs) {
        const QFileInfoList entries = QDir(dir).entryInfoList({prefix + QLatin1Char('*')}, QDir::Files | QDir::Executable);
        for (const QFileInfo &fi : entries)
            programs << fi.fileName();
    }
    programs.sort();
    programs.removeDuplicates();
    for (const QString &p : std::as_const(programs))
        add(p);
    return out;
}

void RunService::browse(const QString &title)
{
    // org.freedesktop.portal.FileChooser: subscribe to the Response signal on
    // the predictable request path before calling, so it cannot be missed.
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString token = QStringLiteral("meadow%1").arg(QRandomGenerator::global()->generate());
    QString sender = bus.baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString handle = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
    if (!m_requestPath.isEmpty())
        bus.disconnect({}, m_requestPath, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                       this, SLOT(onPortalResponse(uint, QVariantMap)));
    m_requestPath = handle;
    bus.connect({}, handle, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"), this,
                SLOT(onPortalResponse(uint, QVariantMap)));
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
                                                      QStringLiteral("/org/freedesktop/portal/desktop"),
                                                      QStringLiteral("org.freedesktop.portal.FileChooser"), QStringLiteral("OpenFile"));
    msg << QString() << title << QVariantMap{{QStringLiteral("handle_token"), token}};
    bus.asyncCall(msg);
}

void RunService::onPortalResponse(uint response, const QVariantMap &results)
{
    if (response != 0)
        return; // cancelled
    const QStringList uris = results.value(QStringLiteral("uris")).toStringList();
    if (!uris.isEmpty())
        Q_EMIT browsed(QUrl(uris.first()).toLocalFile());
}
