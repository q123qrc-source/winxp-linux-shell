// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QVariantList>

class AppRegistry;
class UsageStore;
class QFileSystemWatcher;

// The "All Programs" tree: an XDG menu file evaluated against the installed
// applications. Rebuilt when applications or the menu file change.
//
// The menu file is the first of: `menuFile` if set, then
// meadow-applications.menu in ~/.config/menus, $XDG_CONFIG_DIRS/menus and
// Meadow's own data dir (MEADOW_MENU_DIR overrides that for tests). Without
// any, all applications are listed flat.
//
// `tree` entries: {label, iconName, desktopId, isNew, children}; folders have
// children and no desktopId. Folders come first, then programs, each sorted
// by name for the current locale.
class AppMenuModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList tree READ tree NOTIFY changed)
    Q_PROPERTY(QString menuFile READ menuFile WRITE setMenuFile NOTIFY changed)
    // Applications first seen within this many days (and never launched)
    // are highlighted, as are the folders containing them.
    Q_PROPERTY(int newProgramDays READ newProgramDays WRITE setNewProgramDays NOTIFY changed)

public:
    AppMenuModel(AppRegistry *apps, UsageStore *usage, QObject *parent = nullptr);
    // With the settings known up front, so the tree is built once at startup.
    AppMenuModel(AppRegistry *apps, UsageStore *usage, int newProgramDays, const QString &menuFile,
                 QObject *parent = nullptr);
    explicit AppMenuModel(QObject *parent = nullptr); // empty, for tests of the wiring

    QVariantList tree() const { return m_tree; }
    QString menuFile() const { return m_menuFile; }
    void setMenuFile(const QString &path);
    // The file actually used (empty: the flat fallback).
    QString effectiveMenuFile() const { return m_effectiveFile; }
    int newProgramDays() const { return m_newDays; }
    void setNewProgramDays(int days);

    Q_INVOKABLE void rebuild();

Q_SIGNALS:
    void changed();

private:

    AppRegistry *m_apps = nullptr;
    UsageStore *m_usage = nullptr;
    QFileSystemWatcher *m_watcher = nullptr;
    QString m_menuFile;
    QString m_effectiveFile;
    int m_newDays = 7;
    QVariantList m_tree;
};
