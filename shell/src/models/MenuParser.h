// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "DesktopEntry.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

// Desktop Menu Specification (XDG menus, applications.menu) parser and
// evaluator, the subset Start menus need:
//   Menu, Name, Directory, DefaultAppDirs, DirectoryDir,
//   DefaultDirectoryDirs, Include/Exclude with Filename, Category, All, And,
//   Or, Not; OnlyUnallocated/NotOnlyUnallocated, Deleted/NotDeleted,
//   MergeFile (type="path"), MergeDir, DefaultMergeDirs; menus of the same
//   Name merge. Applications come from the XDG application directories
//   (AppRegistry); extra <AppDir>s, Layout, Move and the KDE legacy elements
//   are ignored. The Start menu sorts folders first, then items, by name.
class MenuParser
{
public:
    struct Rule; // Include/Exclude matching expression

    struct Menu {
        QString name;
        QStringList directoryFiles;   // <Directory>, last one wins
        QStringList appDirs;
        QStringList directoryDirs;
        bool onlyUnallocated = false;
        bool deleted = false;
        // Include (true) / Exclude (false) rules in document order.
        QList<QPair<bool, std::shared_ptr<Rule>>> rules;
        QList<std::shared_ptr<Menu>> children;
    };

    // The evaluated menu: localized folder name and icon, application ids.
    struct Node {
        QString name;   // display name (.directory Name, else <Name>)
        QString icon;
        QStringList apps; // desktop ids
        QList<Node> children;
    };

    // Looks an application up by desktop id (nullptr if unknown).
    using Lookup = std::function<const DesktopEntry *(const QString &id)>;

    // Parses `path`; false (and errorString) on a missing or broken file.
    bool load(const QString &path);
    QString errorString() const { return m_error; }

    // Evaluates the menu against `allIds` (every installed application).
    // Entries `visible` rejects are left out; empty menus are dropped.
    Node evaluate(const QStringList &allIds, const Lookup &lookup,
                  const std::function<bool(const QString &)> &visible) const;

    // Menu file search: user and system config dirs, then Meadow's own.
    static QString findMenuFile(const QString &basename, const QString &bundledDir);

private:
    bool parseFile(const QString &path, Menu &into, int depth);
    std::shared_ptr<Menu> m_root;
    QString m_error;
};
