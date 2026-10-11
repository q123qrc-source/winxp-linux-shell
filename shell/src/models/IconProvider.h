// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QQuickImageProvider>

// image://icon/<name or absolute path>: theme icons for QML at the requested
// size. Unknown icons get a drawn generic application icon so screenshots do
// not depend on which icon themes are installed.
class IconProvider : public QQuickImageProvider
{
public:
    IconProvider();
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

    // Selects the scheme's icon theme (theme.json assets.iconTheme) for the
    // shell, whatever the platform theme would pick. MEADOW_ICON_DIRS
    // (colon-separated) adds uninstalled icon directories in front, for
    // previews and screenshot tests.
    static void useIconTheme(const QString &name);

    static QPixmap genericIcon(int size);
    static QPixmap iconPixmap(const QString &name, int size);
};
