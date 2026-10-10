// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Logging.h"

#include <QBrush>
#include <QColor>
#include <QPainterPath>
#include <QQuickPaintedItem>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// A rectangle filled with a theme gradient (a list of {pos, color} stops taken
// straight from theme.json), with optional per-corner radii and a solid or
// dotted border. Qt Quick's Rectangle cannot take a gradient from data or round
// individual corners on Qt 6.5, which the Luna look needs everywhere.
class MEADOW_COMMON_EXPORT GradientBox : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList stops READ stops WRITE setStops NOTIFY appearanceChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY appearanceChanged)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY appearanceChanged)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY appearanceChanged)
    Q_PROPERTY(qreal topLeftRadius READ topLeftRadius WRITE setTopLeftRadius NOTIFY appearanceChanged)
    Q_PROPERTY(qreal topRightRadius READ topRightRadius WRITE setTopRightRadius NOTIFY appearanceChanged)
    Q_PROPERTY(qreal bottomLeftRadius READ bottomLeftRadius WRITE setBottomLeftRadius NOTIFY appearanceChanged)
    Q_PROPERTY(qreal bottomRightRadius READ bottomRightRadius WRITE setBottomRightRadius NOTIFY appearanceChanged)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor NOTIFY appearanceChanged)
    Q_PROPERTY(qreal borderWidth READ borderWidth WRITE setBorderWidth NOTIFY appearanceChanged)
    Q_PROPERTY(bool borderDotted READ borderDotted WRITE setBorderDotted NOTIFY appearanceChanged)
    // Radial mode: stops run from (centerX, centerY) — fractions of the item —
    // out to radiusRatio times the item's larger side.
    Q_PROPERTY(bool radial MEMBER m_radial NOTIFY appearanceChanged)
    Q_PROPERTY(qreal centerX MEMBER m_centerX NOTIFY appearanceChanged)
    Q_PROPERTY(qreal centerY MEMBER m_centerY NOTIFY appearanceChanged)
    Q_PROPERTY(qreal radiusRatio MEMBER m_radiusRatio NOTIFY appearanceChanged)

public:
    explicit GradientBox(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    QVariantList stops() const { return m_stops; }
    void setStops(const QVariantList &stops);
    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    Qt::Orientation orientation() const { return m_orientation; }
    void setOrientation(Qt::Orientation orientation);
    qreal radius() const { return m_radius; }
    void setRadius(qreal radius);
    // A negative corner radius means "use radius".
    qreal topLeftRadius() const { return m_corner[0]; }
    void setTopLeftRadius(qreal r) { setCorner(0, r); }
    qreal topRightRadius() const { return m_corner[1]; }
    void setTopRightRadius(qreal r) { setCorner(1, r); }
    qreal bottomRightRadius() const { return m_corner[2]; }
    void setBottomRightRadius(qreal r) { setCorner(2, r); }
    qreal bottomLeftRadius() const { return m_corner[3]; }
    void setBottomLeftRadius(qreal r) { setCorner(3, r); }
    QColor borderColor() const { return m_borderColor; }
    void setBorderColor(const QColor &color);
    qreal borderWidth() const { return m_borderWidth; }
    void setBorderWidth(qreal width);
    bool borderDotted() const { return m_borderDotted; }
    void setBorderDotted(bool dotted);

    // Shared with other painted items: a rounded rect with per-corner radii
    // (top-left, top-right, bottom-right, bottom-left).
    static QPainterPath roundedPath(const QRectF &rect, const qreal radii[4]);
    // Converts theme stops to a QGradient stop list; invalid entries are skipped.
    static QGradientStops gradientStops(const QVariantList &stops);

Q_SIGNALS:
    void appearanceChanged();

private:
    void setCorner(int index, qreal radius);
    void changed();

    QVariantList m_stops;
    QGradientStops m_parsedStops;
    QColor m_color = Qt::transparent;
    Qt::Orientation m_orientation = Qt::Vertical;
    qreal m_radius = 0;
    qreal m_corner[4] = {-1, -1, -1, -1};
    QColor m_borderColor = Qt::transparent;
    qreal m_borderWidth = 0;
    bool m_borderDotted = false;
    bool m_radial = false;
    qreal m_centerX = 0.5;
    qreal m_centerY = 0.5;
    qreal m_radiusRatio = 0.5;
};
