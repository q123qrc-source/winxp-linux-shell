// SPDX-License-Identifier: GPL-3.0-or-later
#include "IconProvider.h"

#include <QFileInfo>
#include <QIcon>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap IconProvider::genericIcon(int s)
{
    // A small window: blue title bar over a light client area.
    QPixmap pm(s, s);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r(s * 0.08, s * 0.14, s * 0.84, s * 0.72);
    QPainterPath frame;
    frame.addRoundedRect(r, s * 0.08, s * 0.08);
    p.fillPath(frame, QColor(0x1D, 0x4F, 0xD3));
    p.fillRect(QRectF(r.left() + s * 0.06, r.top() + s * 0.2, r.width() - s * 0.12, r.height() - s * 0.26), QColor(0xF4, 0xF3, 0xEE));
    QLinearGradient g(r.topLeft(), QPointF(r.left(), r.top() + s * 0.2));
    g.setColorAt(0, QColor(0x5B, 0x8E, 0xF3));
    g.setColorAt(1, QColor(0x1D, 0x4F, 0xD3));
    p.fillRect(QRectF(r.left() + s * 0.04, r.top() + s * 0.03, r.width() - s * 0.08, s * 0.15), g);
    return pm;
}

QPixmap IconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    const int s = requestedSize.isValid() && requestedSize.width() > 0 ? requestedSize.width() : 32;
    if (size)
        *size = QSize(s, s);
    // "name?tint=AARRGGBB": the icon blended with a color over its opaque
    // pixels (selected desktop icons). Done here rather than with a shader so
    // it also works with the software renderer.
    QString name = QUrl::fromPercentEncoding(id.toUtf8());
    QColor tint;
    const qsizetype query = name.indexOf(QLatin1String("?tint="));
    if (query >= 0) {
        tint = QColor(QLatin1Char('#') + name.mid(query + 6));
        name.truncate(query);
    }
    QPixmap pm = iconPixmap(name, s);
    if (tint.isValid() && tint.alpha() > 0) {
        QPainter p(&pm);
        p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        p.fillRect(pm.rect(), tint);
    }
    return pm;
}

QPixmap IconProvider::iconPixmap(const QString &name, int s)
{
    // Screenshot tests: the same drawn icon everywhere, whatever is installed.
    static const bool genericOnly = qEnvironmentVariableIntValue("MEADOW_ICONS_GENERIC") == 1;
    if (genericOnly)
        return genericIcon(s);
    QIcon icon;
    if (QFileInfo(name).isAbsolute() && QFileInfo::exists(name))
        icon = QIcon(name);
    else if (!name.isEmpty())
        icon = QIcon::fromTheme(name);
    if (icon.isNull())
        return genericIcon(s);
    return icon.pixmap(QSize(s, s));
}
