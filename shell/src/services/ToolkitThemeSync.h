// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

// Keeps third-party applications on the current color scheme's toolkit
// themes (theme.json toolkit.gtkThemeName / toolkit.kvantumThemeName):
//   - GTK: GTK_THEME in the shell's own environment, which every application
//     the shell starts inherits;
//   - Qt (Kvantum): Kvantum/kvantum.kvconfig in the session's runtime config
//     directory (MEADOW_RUNTIME_XDG, first in XDG_CONFIG_DIRS), which Kvantum
//     reads when the user has not chosen a theme of their own.
// Applications already running keep their theme until restarted. Nothing
// outside the session's runtime directory is written.
class ToolkitThemeSync : public QObject
{
    Q_OBJECT

public:
    // runtimeXdgDir empty: MEADOW_RUNTIME_XDG; still empty: only GTK_THEME.
    explicit ToolkitThemeSync(const QString &runtimeXdgDir = QString(), QObject *parent = nullptr);

    QString runtimeXdgDir() const { return m_dir; }

public Q_SLOTS:
    // Applies the loaded scheme now (also called on every theme change).
    void apply();

Q_SIGNALS:
    void applied(const QString &gtkTheme, const QString &kvantumTheme);

private:
    QString m_dir;
    QString m_lastKvantum;
};
