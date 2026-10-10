// SPDX-License-Identifier: GPL-3.0-or-later
#include "GradientBox.h"

#include <QLinearGradient>
#include <QRadialGradient>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

GradientBox::GradientBox(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &GradientBox::appearanceChanged, this, [this] { update(); });
}

QGradientStops GradientBox::gradientStops(const QVariantList &stops)
{
    QGradientStops out;
    out.reserve(stops.size());
    for (const QVariant &v : stops) {
        const QVariantMap stop = v.toMap();
        const QColor color(stop.value(QStringLiteral("color")).toString());
        if (!color.isValid())
            continue;
        out.append({std::clamp(stop.value(QStringLiteral("pos")).toReal(), 0.0, 1.0), color});
    }
    return out;
}

QPainterPath GradientBox::roundedPath(const QRectF &r, const qreal radii[4])
{
    const qreal maxR = std::min(r.width(), r.height()) / 2;
    const qreal tl = std::clamp(radii[0], 0.0, maxR);
    const qreal tr = std::clamp(radii[1], 0.0, maxR);
    const qreal br = std::clamp(radii[2], 0.0, maxR);
    const qreal bl = std::clamp(radii[3], 0.0, maxR);

    QPainterPath path;
    path.moveTo(r.left() + tl, r.top());
    path.lineTo(r.right() - tr, r.top());
    if (tr > 0)
        path.arcTo(QRectF(r.right() - 2 * tr, r.top(), 2 * tr, 2 * tr), 90, -90);
    path.lineTo(r.right(), r.bottom() - br);
    if (br > 0)
        path.arcTo(QRectF(r.right() - 2 * br, r.bottom() - 2 * br, 2 * br, 2 * br), 0, -90);
    path.lineTo(r.left() + bl, r.bottom());
    if (bl > 0)
        path.arcTo(QRectF(r.left(), r.bottom() - 2 * bl, 2 * bl, 2 * bl), 270, -90);
    path.lineTo(r.left(), r.top() + tl);
    if (tl > 0)
        path.arcTo(QRectF(r.left(), r.top(), 2 * tl, 2 * tl), 180, -90);
    path.closeSubpath();
    return path;
}

void GradientBox::paint(QPainter *painter)
{
    const QRectF bounds = boundingRect();
    if (bounds.isEmpty())
        return;
    painter->setRenderHint(QPainter::Antialiasing, true);

    qreal radii[4];
    for (int i = 0; i < 4; ++i)
        radii[i] = m_corner[i] >= 0 ? m_corner[i] : m_radius;

    const bool hasBorder = m_borderWidth > 0 && m_borderColor.alpha() > 0;
    // Stroke centered on a half-inset rect so the border stays inside the item.
    const qreal inset = hasBorder ? m_borderWidth / 2 : 0;
    const QRectF rect = bounds.adjusted(inset, inset, -inset, -inset);
    const QPainterPath path = roundedPath(rect, radii);

    if (!m_parsedStops.isEmpty() && m_radial) {
        QRadialGradient gradient(QPointF(bounds.width() * m_centerX, bounds.height() * m_centerY),
                                 std::max(bounds.width(), bounds.height()) * m_radiusRatio);
        gradient.setStops(m_parsedStops);
        painter->fillPath(path, gradient);
    } else if (!m_parsedStops.isEmpty()) {
        QLinearGradient gradient(bounds.topLeft(), m_orientation == Qt::Vertical ? bounds.bottomLeft() : bounds.topRight());
        gradient.setStops(m_parsedStops);
        painter->fillPath(path, gradient);
    } else if (m_color.alpha() > 0) {
        painter->fillPath(path, m_color);
    }

    if (hasBorder) {
        QPen pen(m_borderColor, m_borderWidth);
        if (m_borderDotted) {
            pen.setStyle(Qt::CustomDashLine);
            pen.setDashPattern({1, 1});
            pen.setCapStyle(Qt::FlatCap);
            // Dots look smeared when antialiased across pixel boundaries.
            painter->setRenderHint(QPainter::Antialiasing, false);
        }
        painter->strokePath(path, pen);
    }
}

void GradientBox::changed()
{
    update();
    Q_EMIT appearanceChanged();
}

void GradientBox::setStops(const QVariantList &stops)
{
    if (m_stops == stops)
        return;
    m_stops = stops;
    m_parsedStops = gradientStops(stops);
    changed();
}

void GradientBox::setColor(const QColor &color)
{
    if (m_color == color)
        return;
    m_color = color;
    changed();
}

void GradientBox::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    changed();
}

void GradientBox::setRadius(qreal radius)
{
    if (qFuzzyCompare(m_radius, radius))
        return;
    m_radius = radius;
    changed();
}

void GradientBox::setCorner(int index, qreal radius)
{
    if (qFuzzyCompare(m_corner[index], radius))
        return;
    m_corner[index] = radius;
    changed();
}

void GradientBox::setBorderColor(const QColor &color)
{
    if (m_borderColor == color)
        return;
    m_borderColor = color;
    changed();
}

void GradientBox::setBorderWidth(qreal width)
{
    if (qFuzzyCompare(m_borderWidth, width))
        return;
    m_borderWidth = width;
    changed();
}

void GradientBox::setBorderDotted(bool dotted)
{
    if (m_borderDotted == dotted)
        return;
    m_borderDotted = dotted;
    changed();
}
