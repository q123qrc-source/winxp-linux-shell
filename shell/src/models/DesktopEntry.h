// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

// One .desktop file (Desktop Entry Specification 1.5), the subset the shell
// needs. Localized keys use the current locale (Name[de_DE], Name[de], Name).
struct DesktopEntry {
    QString id;        // desktop file id, e.g. "org.mozilla.firefox.desktop"
    QString path;
    QString type;
    QString name;
    QString genericName;
    QString comment;
    QString icon;
    QString exec;
    QString tryExec;
    QString startupWMClass;
    QStringList categories;
    QStringList onlyShowIn;
    QStringList notShowIn;
    QStringList mimeTypes;
    bool noDisplay = false;
    bool hidden = false;
    bool terminal = false;

    bool isValid() const { return !name.isEmpty() && (type != QLatin1String("Application") || !exec.isEmpty()); }
    // Hidden, NoDisplay, OnlyShowIn/NotShowIn for `desktop` (e.g. "Meadow"),
    // and TryExec that is not installed all hide an entry from menus.
    bool shownIn(const QString &desktop) const;
    // Same for XDG_CURRENT_DESKTOP's list (any match counts).
    bool shownIn(const QStringList &desktops) const;

    static DesktopEntry parse(const QString &path, const QString &id, const QString &locale = QString());
    // Exec split into argv per the spec's quoting rules, with field codes
    // expanded (%f/%u to `files`, %F/%U to all, %i/%c/%k as specified).
    QStringList execArguments(const QStringList &files = {}) const;
};
