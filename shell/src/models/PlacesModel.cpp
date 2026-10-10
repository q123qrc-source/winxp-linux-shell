// SPDX-License-Identifier: GPL-3.0-or-later
#include "PlacesModel.h"

#include "services/Launcher.h"
#include "services/Settings.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <QXmlStreamReader>

namespace {
QVariantMap item(const QString &key, const QString &label, const QString &icon)
{
    return {{QStringLiteral("key"), key}, {QStringLiteral("label"), label}, {QStringLiteral("iconName"), icon}};
}
QVariantMap separator()
{
    return {{QStringLiteral("separator"), true}};
}
QString userDir(QStandardPaths::StandardLocation location)
{
    // Qt reads ~/.config/user-dirs.dirs for these.
    return QStandardPaths::writableLocation(location);
}
} // namespace

PlacesModel::PlacesModel(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_watcher(new QFileSystemWatcher(this))
{
    connect(m_settings, &Settings::changed, this, &PlacesModel::rebuild);
    // Applications rewrite the file atomically, so watch the directory too.
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &PlacesModel::reloadRecent);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &PlacesModel::reloadRecent);
    rebuild();
    reloadRecent();
}

QString PlacesModel::recentFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/recently-used.xbel");
}

QString PlacesModel::command(const QString &key) const
{
    return m_settings->stringValue(QStringLiteral("StartMenu/") + key);
}

void PlacesModel::rebuild()
{
    Launcher::instance()->setFileManager(command(QStringLiteral("fileManager")));
    QVariantList places{
        item(QStringLiteral("documents"), tr("My Documents"), QStringLiteral("folder-documents")),
        QVariantMap{{QStringLiteral("key"), QStringLiteral("recent")},
                    {QStringLiteral("label"), tr("My Recent Documents")},
                    {QStringLiteral("iconName"), QStringLiteral("document-open-recent")},
                    {QStringLiteral("submenu"), true}},
        item(QStringLiteral("pictures"), tr("My Pictures"), QStringLiteral("folder-pictures")),
        item(QStringLiteral("music"), tr("My Music"), QStringLiteral("folder-music")),
        item(QStringLiteral("computer"), tr("My Computer"), QStringLiteral("computer")),
        separator(),
    };
    const auto tool = [this, &places](const QString &key, const QString &label, const QString &icon) {
        const QStringList argv = QProcess::splitCommand(command(key));
        if (!argv.isEmpty() && Launcher::available(argv.first()))
            places << item(key, label, icon);
    };
    tool(QStringLiteral("controlPanel"), tr("Control Panel"), QStringLiteral("preferences-system"));
    tool(QStringLiteral("defaultPrograms"), tr("Set Program Access and Defaults"), QStringLiteral("preferences-desktop-default-applications"));
    tool(QStringLiteral("printers"), tr("Printers and Faxes"), QStringLiteral("printer"));
    if (!places.last().toMap().value(QStringLiteral("separator")).toBool())
        places << separator();
    tool(QStringLiteral("help"), tr("Help and Support"), QStringLiteral("help-browser"));
    tool(QStringLiteral("search"), tr("Search"), QStringLiteral("system-search"));
    places << item(QStringLiteral("run"), tr("Run…"), QStringLiteral("system-run"));
    if (places != m_places) {
        m_places = places;
        Q_EMIT changed();
    }
}

bool PlacesModel::activate(const QString &key)
{
    Launcher *launcher = Launcher::instance();
    if (key == QLatin1String("documents"))
        return launcher->openFolder(userDir(QStandardPaths::DocumentsLocation));
    if (key == QLatin1String("pictures"))
        return launcher->openFolder(userDir(QStandardPaths::PicturesLocation));
    if (key == QLatin1String("music"))
        return launcher->openFolder(userDir(QStandardPaths::MusicLocation));
    if (key == QLatin1String("computer")) {
        // pcmanfm-qt understands computer:///; other file managers get /.
        const bool pcmanfm = command(QStringLiteral("fileManager")).startsWith(QLatin1String("pcmanfm"));
        return launcher->openFolder(pcmanfm ? QStringLiteral("computer:///") : QStringLiteral("/"));
    }
    if (key == QLatin1String("run")) {
        Q_EMIT runRequested();
        return true;
    }
    QStringList argv = QProcess::splitCommand(command(key));
    if (argv.isEmpty())
        return false;
    const QString program = argv.takeFirst();
    return Launcher::start(program, argv);
}

bool PlacesModel::openDocument(const QString &path)
{
    return Launcher::instance()->open(path);
}

void PlacesModel::reloadRecent()
{
    const QString file = recentFile();
    if (!m_watcher->files().isEmpty())
        m_watcher->removePaths(m_watcher->files());
    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    if (QFile::exists(file))
        m_watcher->addPath(file);
    if (QFileInfo::exists(QFileInfo(file).absolutePath()))
        m_watcher->addPath(QFileInfo(file).absolutePath());
    const QVariantList recent = parseRecent(file, kMaxRecent);
    if (recent != m_recent) {
        m_recent = recent;
        Q_EMIT recentChanged();
    }
}

QVariantList PlacesModel::parseRecent(const QString &xbelPath, int max)
{
    QFile f(xbelPath);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    struct Doc {
        QString path;
        QString mime;
        QDateTime when;
    };
    QList<Doc> docs;
    QXmlStreamReader xml(&f);
    Doc current;
    bool inBookmark = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("bookmark")) {
            const auto a = xml.attributes();
            const QUrl url(a.value(QStringLiteral("href")).toString());
            inBookmark = url.isLocalFile();
            current = {};
            current.path = url.toLocalFile();
            for (const char *attr : {"visited", "modified", "added"}) {
                const QDateTime t = QDateTime::fromString(a.value(QLatin1String(attr)).toString(), Qt::ISODateWithMs);
                if (t.isValid() && (!current.when.isValid() || t > current.when))
                    current.when = t;
            }
        } else if (inBookmark && xml.isStartElement() && xml.name() == QLatin1String("mime-type")) {
            current.mime = xml.attributes().value(QStringLiteral("type")).toString();
        } else if (xml.isEndElement() && xml.name() == QLatin1String("bookmark")) {
            if (inBookmark && QFileInfo(current.path).isFile())
                docs << current;
            inBookmark = false;
        }
    }
    std::stable_sort(docs.begin(), docs.end(), [](const Doc &a, const Doc &b) { return a.when > b.when; });
    QMimeDatabase mimes;
    QVariantList out;
    for (const Doc &d : std::as_const(docs)) {
        if (out.size() >= max)
            break;
        const QMimeType type = d.mime.isEmpty() ? mimes.mimeTypeForFile(d.path) : mimes.mimeTypeForName(d.mime);
        out << QVariantMap{{QStringLiteral("label"), QFileInfo(d.path).fileName()},
                           {QStringLiteral("iconName"), type.isValid() ? type.iconName() : QStringLiteral("text-x-generic")},
                           {QStringLiteral("path"), d.path}};
    }
    return out;
}
