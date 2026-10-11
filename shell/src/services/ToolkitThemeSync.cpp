// SPDX-License-Identifier: GPL-3.0-or-later
#include "ToolkitThemeSync.h"

#include "ThemeLoader.h"

#include <QDir>
#include <QLoggingCategory>
#include <QSaveFile>

Q_LOGGING_CATEGORY(lcToolkit, "meadow.shell.toolkit")

ToolkitThemeSync::ToolkitThemeSync(const QString &runtimeXdgDir, QObject *parent)
    : QObject(parent)
    , m_dir(runtimeXdgDir.isEmpty() ? qEnvironmentVariable("MEADOW_RUNTIME_XDG") : runtimeXdgDir)
{
    connect(ThemeLoader::instance(), &ThemeLoader::themeChanged, this, &ToolkitThemeSync::apply);
}

void ToolkitThemeSync::apply()
{
    const ThemeLoader *theme = ThemeLoader::instance();
    const QString gtk = theme->token(QStringLiteral("toolkit.gtkThemeName")).toString();
    const QString kvantum = theme->token(QStringLiteral("toolkit.kvantumThemeName")).toString();
    if (gtk.isEmpty() || kvantum.isEmpty())
        return;
    if (qEnvironmentVariable("GTK_THEME") != gtk)
        qputenv("GTK_THEME", gtk.toUtf8());

    if (!m_dir.isEmpty() && kvantum != m_lastKvantum) {
        const QString dir = m_dir + QStringLiteral("/Kvantum");
        QSaveFile f(dir + QStringLiteral("/kvantum.kvconfig"));
        if (!QDir().mkpath(dir) || !f.open(QIODevice::WriteOnly)) {
            qCWarning(lcToolkit) << "cannot write" << f.fileName();
        } else {
            f.write("# Written by meadow-shell for this session (color scheme); the user's own\n"
                    "# ~/.config/Kvantum/kvantum.kvconfig takes precedence.\n[General]\ntheme=");
            f.write(kvantum.toUtf8());
            f.write("\n");
            if (f.commit())
                m_lastKvantum = kvantum;
        }
    }
    qCInfo(lcToolkit) << "toolkit themes:" << gtk << kvantum;
    Q_EMIT applied(gtk, kvantum);
}
