// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QVariantList>

class Launcher;
class Settings;
class QFileSystemWatcher;

// The Start menu's right column and the "My Recent Documents" submenu.
//
// Places map to the XDG user directories (user-dirs.dirs): My Documents,
// My Pictures, My Music; My Computer opens computer:/// in pcmanfm-qt (/
// with other file managers). The tool items (Control Panel, Default
// Programs, Printers, Help and Support, Search) run the commands in shell.conf
// [StartMenu] and are hidden when their program is not installed. Run… is
// handled by the shell itself.
//
// `places` entries: {key, label, iconName, separator, submenu}.
// `recentDocuments`: {label, iconName, path} from recently-used.xbel, newest
// first, existing local files only.
class PlacesModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList places READ places NOTIFY changed)
    Q_PROPERTY(QVariantList recentDocuments READ recentDocuments NOTIFY recentChanged)

public:
    static constexpr int kMaxRecent = 15;

    PlacesModel(Settings *settings, QObject *parent = nullptr);

    QVariantList places() const { return m_places; }
    QVariantList recentDocuments() const { return m_recent; }

    // Opens the place or runs the tool for `key`; false if it cannot.
    Q_INVOKABLE bool activate(const QString &key);
    Q_INVOKABLE bool openDocument(const QString &path);

    // Parses a recently-used.xbel file (exposed for tests).
    static QVariantList parseRecent(const QString &xbelPath, int max);
    static QString recentFile();

Q_SIGNALS:
    void changed();
    void recentChanged();
    // Items the shell handles itself.
    void runRequested();

private:
    void rebuild();
    void reloadRecent();
    QString command(const QString &key) const;

    Settings *m_settings;
    QFileSystemWatcher *m_watcher;
    QVariantList m_places;
    QVariantList m_recent;
};
