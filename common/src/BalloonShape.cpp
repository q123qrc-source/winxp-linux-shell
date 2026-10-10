// SPDX-License-Identifier: GPL-3.0-or-later
#include "BalloonShape.h"
#include "GradientBox.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

BalloonShape::BalloonShape(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &BalloonShape::appearanceChanged, this, [this] { update(); });
}

void BalloonShape::paint(QPainter *p)
{
    const QRectF all = boundingRect().adjusted(0.5, 0.5, -0.5, -0.5);
    if (all.height() <= m_tailHeight || all.width() <= m_tailWidth)
        return;
    const QRectF body = m_tailOnTop ? all.adjusted(0, m_tailHeight, 0, 0) : all.adjusted(0, 0, 0, -m_tailHeight);
    const qreal radii[4] = {m_radius, m_radius, m_radius, m_radius};
    QPainterPath path = GradientBox::roundedPath(body, radii);

    // Slanted tail: one edge vertical at the tip, the other leaning back, like
    // a speech balloon pointing at a tray icon.
    const qreal tipX = std::clamp(m_tailX, all.left() + m_radius, all.right() - m_radius);
    const bool leanRight = tipX < all.center().x();
    const qreal baseA = tipX;
    const qreal baseB = leanRight ? std::min(tipX + m_tailWidth, body.right() - m_radius) : std::max(tipX - m_tailWidth, body.left() + m_radius);
    const qreal edgeY = m_tailOnTop ? body.top() : body.bottom();
    // Overlap the body by 1px so the union leaves no seam.
    const qreal inside = m_tailOnTop ? 1 : -1;
    QPainterPath tail;
    tail.moveTo(baseA, edgeY + inside);
    tail.lineTo(tipX, m_tailOnTop ? all.top() : all.bottom());
    tail.lineTo(baseB, edgeY + inside);
    tail.closeSubpath();
    path = path.united(tail).simplified();

    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(QPen(m_borderColor, 1));
    p->setBrush(m_color);
    p->drawPath(path);
}
