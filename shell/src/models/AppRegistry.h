// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "DesktopEntry.h"

#include <QHash>
#include <QObject>
#include <QStringList>

// Installed applications (.desktop files in XDG data dirs): finds the entry
// for a window's app_id, resolves icons and launches applications.
class AppRegistry : public QObject
{
    Q_OBJECT

public:
    explicit AppRegistry(QObject *parent = nullptr);

    // applications/ directories, highest priority first.
    static QStringList applicationDirs();
    void reload();

    const QHash<QString, DesktopEntry> &entries() const { return m_entries; }
    DesktopEntry entry(const QString &desktopId) const { return m_entries.value(desktopId); }

    // Best match for a Wayland app_id / X11 class: exact desktop id, the same
    // case-insensitively, StartupWMClass, then the last reverse-DNS part.
    Q_INVOKABLE QString desktopIdForApp(const QString &appId) const;
    Q_INVOKABLE QString iconForApp(const QString &appId) const;
    Q_INVOKABLE QString nameForApp(const QString &appId) const;
    Q_INVOKABLE QString iconForDesktopId(const QString &desktopId) const;
    Q_INVOKABLE QString nameForDesktopId(const QString &desktopId) const;
    Q_INVOKABLE bool hasDesktopId(const QString &desktopId) const { return m_entries.contains(desktopId); }

    // Starts the application detached; returns false if it could not start.
    Q_INVOKABLE bool launch(const QString &desktopId, const QStringList &files = {}) const;

Q_SIGNALS:
    void launched(const QString &desktopId);

private:
    QHash<QString, DesktopEntry> m_entries;
    QHash<QString, QString> m_byLowerId;
    QHash<QString, QString> m_byWmClass;
};
