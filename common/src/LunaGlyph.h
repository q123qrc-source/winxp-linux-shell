// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Logging.h"

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

// Small vector glyphs drawn with QPainter so controls never depend on icon
// fonts or bitmaps: check marks, arrows, window buttons, balloon icons.
// The glyph is fitted into the item's square center.
class MEADOW_COMMON_EXPORT LunaGlyph : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Glyph glyph READ glyph WRITE setGlyph NOTIFY appearanceChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY appearanceChanged)
    // Stroke width in item pixels; 0 picks one proportional to the size.
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth NOTIFY appearanceChanged)

public:
    enum Glyph {
        None,
        Check,
        ArrowUp,
        ArrowDown,
        ArrowLeft,
        ArrowRight,
        Close,
        Minimize,
        Maximize,
        Restore,
        ChevronLeft,
        ChevronRight,
        Info,
        Warning,
        Error,
        Question,
        Power,
        Restart,
        Keyboard,
        Contrast,
        Fingerprint,
    };
    Q_ENUM(Glyph)

    explicit LunaGlyph(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    Glyph glyph() const { return m_glyph; }
    void setGlyph(Glyph glyph);
    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    qreal lineWidth() const { return m_lineWidth; }
    void setLineWidth(qreal width);

Q_SIGNALS:
    void appearanceChanged();

private:
    Glyph m_glyph = None;
    QColor m_color = Qt::black;
    qreal m_lineWidth = 0;
};
