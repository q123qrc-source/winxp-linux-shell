// SPDX-License-Identifier: GPL-3.0-or-later
#include "AppMenuModel.h"

#include "AppRegistry.h"
#include "MenuParser.h"
#include "UsageStore.h"

#include <QCollator>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcMenu)

namespace {
QString bundledMenuDir()
{
    const QString env = qEnvironmentVariable("MEADOW_MENU_DIR");
    return env.isEmpty() ? QStringLiteral(MEADOW_DATADIR "/menus") : env;
}
} // namespace

AppMenuModel::AppMenuModel(QObject *parent)
    : QObject(parent)
{
}

AppMenuModel::AppMenuModel(AppRegistry *apps, UsageStore *usage, QObject *parent)
    : QObject(parent)
    , m_apps(apps)
    , m_usage(usage)
    , m_watcher(new QFileSystemWatcher(this))
{
    connect(m_apps, &AppRegistry::changed, this, &AppMenuModel::rebuild);
    connect(m_usage, &UsageStore::changed, this, &AppMenuModel::rebuild);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &AppMenuModel::rebuild);
    rebuild();
}

void AppMenuModel::setMenuFile(const QString &path)
{
    if (path == m_menuFile)
        return;
    m_menuFile = path;
    rebuild();
}

void AppMenuModel::setNewProgramDays(int days)
{
    if (days == m_newDays)
        return;
    m_newDays = days;
    rebuild();
}

void AppMenuModel::rebuild()
{
    if (!m_apps)
        return;
    QStringList ids = m_apps->entries().keys();
    ids.sort();
    m_usage->noteInstalled(ids);

    const auto lookup = [this](const QString &id) -> const DesktopEntry * {
        const auto &entries = m_apps->entries();
        const auto it = entries.constFind(id);
        return it == entries.cend() ? nullptr : &*it;
    };
    const auto visible = [this](const QString &id) { return m_apps->isShown(id); };

    MenuParser::Node root;
    m_effectiveFile = m_menuFile.isEmpty() ? MenuParser::findMenuFile(QStringLiteral("meadow-applications.menu"), bundledMenuDir())
                                           : m_menuFile;
    MenuParser parser;
    if (!m_effectiveFile.isEmpty() && parser.load(m_effectiveFile)) {
        root = parser.evaluate(ids, lookup, visible);
    } else {
        if (!m_effectiveFile.isEmpty())
            qCWarning(lcMenu).noquote() << parser.errorString() << "- listing all applications";
        m_effectiveFile.clear();
        for (const QString &id : std::as_const(ids)) {
            if (visible(id))
                root.apps << id;
        }
    }
    if (m_watcher) {
        if (!m_watcher->files().isEmpty())
            m_watcher->removePaths(m_watcher->files());
        if (!m_effectiveFile.isEmpty())
            m_watcher->addPath(m_effectiveFile);
    }

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::function<QVariantList(const MenuParser::Node &, bool *)> convert = [&](const MenuParser::Node &node, bool *anyNew) {
        QVariantList folders, programs;
        for (const MenuParser::Node &child : node.children) {
            bool childNew = false;
            const QVariantList children = convert(child, &childNew);
            if (children.isEmpty())
                continue;
            folders << QVariantMap{{QStringLiteral("label"), child.name},
                                   {QStringLiteral("iconName"), child.icon.isEmpty() ? QStringLiteral("folder") : child.icon},
                                   {QStringLiteral("isNew"), childNew},
                                   {QStringLiteral("children"), children}};
            *anyNew = *anyNew || childNew;
        }
        for (const QString &id : node.apps) {
            const DesktopEntry *e = lookup(id);
            const bool isNew = m_usage->isNew(id, m_newDays);
            programs << QVariantMap{{QStringLiteral("label"), e->name},
                                    {QStringLiteral("iconName"), e->icon},
                                    {QStringLiteral("desktopId"), id},
                                    {QStringLiteral("isNew"), isNew}};
            *anyNew = *anyNew || isNew;
        }
        const auto byLabel = [&collator](const QVariant &a, const QVariant &b) {
            return collator.compare(a.toMap().value(QStringLiteral("label")).toString(),
                                    b.toMap().value(QStringLiteral("label")).toString()) < 0;
        };
        std::sort(folders.begin(), folders.end(), byLabel);
        std::sort(programs.begin(), programs.end(), byLabel);
        return folders + programs;
    };
    bool anyNew = false;
    const QVariantList tree = convert(root, &anyNew);
    if (tree != m_tree) {
        m_tree = tree;
        Q_EMIT changed();
    }
}
