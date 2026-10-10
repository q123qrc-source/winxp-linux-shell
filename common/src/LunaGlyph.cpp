// SPDX-License-Identifier: GPL-3.0-or-later
#include "LunaGlyph.h"

#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>

LunaGlyph::LunaGlyph(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

namespace {
// Draws a filled circular badge (balloon icons) with a light rim and a glossy
// top-left highlight, then returns the inner rect for the symbol.
QRectF badge(QPainter *p, const QRectF &r, const QColor &base)
{
    QRadialGradient g(r.left() + r.width() * 0.35, r.top() + r.height() * 0.3, r.width() * 0.75);
    g.setColorAt(0, base.lighter(150));
    g.setColorAt(1, base.darker(115));
    p->setPen(QPen(base.darker(140), std::max(1.0, r.width() / 16)));
    p->setBrush(g);
    p->drawEllipse(r.adjusted(0.5, 0.5, -0.5, -0.5));
    return r.adjusted(r.width() * 0.28, r.height() * 0.2, -r.width() * 0.28, -r.height() * 0.2);
}
} // namespace

void LunaGlyph::paint(QPainter *p)
{
    if (m_glyph == None)
        return;
    const qreal side = std::min(width(), height());
    if (side <= 0)
        return;
    const QRectF r((width() - side) / 2, (height() - side) / 2, side, side);
    const qreal lw = m_lineWidth > 0 ? m_lineWidth : std::max(1.0, std::round(side / 7));
    p->setRenderHint(QPainter::Antialiasing, true);

    QPen pen(m_color, lw, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    const auto at = [&r](qreal fx, qreal fy) { return QPointF(r.left() + fx * r.width(), r.top() + fy * r.height()); };
    const auto triangle = [&](QPointF a, QPointF b, QPointF c) {
        QPainterPath path;
        path.moveTo(a);
        path.lineTo(b);
        path.lineTo(c);
        path.closeSubpath();
        p->fillPath(path, m_color);
    };

    switch (m_glyph) {
    case Check: {
        pen.setCapStyle(Qt::FlatCap);
        p->setPen(pen);
        QPainterPath path;
        path.moveTo(at(0.18, 0.5));
        path.lineTo(at(0.42, 0.74));
        path.lineTo(at(0.84, 0.28));
        p->drawPath(path);
        break;
    }
    case ArrowUp:
        triangle(at(0.2, 0.68), at(0.8, 0.68), at(0.5, 0.32));
        break;
    case ArrowDown:
        triangle(at(0.2, 0.36), at(0.8, 0.36), at(0.5, 0.72));
        break;
    case ArrowLeft:
        triangle(at(0.68, 0.2), at(0.68, 0.8), at(0.32, 0.5));
        break;
    case ArrowRight:
        triangle(at(0.36, 0.2), at(0.36, 0.8), at(0.72, 0.5));
        break;
    case ChevronLeft:
    case ChevronRight: {
        pen.setCapStyle(Qt::FlatCap);
        p->setPen(pen);
        const bool left = m_glyph == ChevronLeft;
        for (qreal dx : {0.0, 0.26}) {
            QPainterPath path;
            const qreal x0 = left ? 0.52 - dx : 0.24 + dx;
            const qreal x1 = left ? 0.26 - dx : 0.50 + dx;
            path.moveTo(at(x0, 0.24));
            path.lineTo(at(x1, 0.5));
            path.lineTo(at(x0, 0.76));
            p->drawPath(path);
        }
        break;
    }
    case Close:
        pen.setWidthF(lw * 1.15);
        p->setPen(pen);
        p->drawLine(at(0.27, 0.27), at(0.73, 0.73));
        p->drawLine(at(0.73, 0.27), at(0.27, 0.73));
        break;
    case Minimize:
        p->fillRect(QRectF(at(0.25, 0.66), at(0.6, 0.66 + lw * 1.4 / r.height())), m_color);
        break;
    case Maximize:
        p->setRenderHint(QPainter::Antialiasing, false);
        p->setPen(QPen(m_color, std::max(1.0, std::round(lw / 2))));
        p->setBrush(Qt::NoBrush);
        p->fillRect(QRectF(at(0.24, 0.24), at(0.76, 0.24 + lw * 1.6 / r.height())), m_color);
        p->drawRect(QRectF(at(0.24, 0.24), at(0.76, 0.76)));
        break;
    case Restore:
        p->setRenderHint(QPainter::Antialiasing, false);
        p->setPen(QPen(m_color, std::max(1.0, std::round(lw / 2))));
        p->setBrush(Qt::NoBrush);
        for (const auto &[x, y] : {std::pair{0.38, 0.2}, std::pair{0.22, 0.38}}) {
            const QRectF box(at(x, y), at(x + 0.4, y + 0.4));
            p->fillRect(QRectF(box.topLeft(), QPointF(box.right(), box.top() + lw * 1.2)), m_color);
            p->drawRect(box);
        }
        break;
    case Info: {
        const QRectF in = badge(p, r, QColor(0x2B, 0x6C, 0xD9));
        p->setPen(Qt::NoPen);
        p->setBrush(Qt::white);
        const qreal dot = in.width() * 0.32;
        p->drawEllipse(QRectF(in.center().x() - dot / 2, in.top(), dot, dot));
        p->drawRect(QRectF(in.center().x() - dot / 2, in.top() + dot * 1.5, dot, in.height() - dot * 1.5));
        break;
    }
    case Error: {
        const QRectF in = badge(p, r, QColor(0xD8, 0x33, 0x1F));
        QPen x(Qt::white, side / 8, Qt::SolidLine, Qt::RoundCap);
        p->setPen(x);
        p->drawLine(in.topLeft() + QPointF(0, in.height() * 0.1), in.bottomRight() - QPointF(0, in.height() * 0.1));
        p->drawLine(in.topRight() + QPointF(0, in.height() * 0.1), in.bottomLeft() - QPointF(0, in.height() * 0.1));
        break;
    }
    case Question: {
        const QRectF in = badge(p, r, QColor(0x2B, 0x6C, 0xD9));
        QPen q(Qt::white, side / 9, Qt::SolidLine, Qt::RoundCap);
        p->setPen(q);
        p->setBrush(Qt::NoBrush);
        QPainterPath path;
        const QRectF arc(in.left(), in.top(), in.width(), in.height() * 0.55);
        path.arcMoveTo(arc, 160);
        path.arcTo(arc, 160, -230);
        path.lineTo(in.center().x(), in.top() + in.height() * 0.72);
        p->drawPath(path);
        p->drawPoint(QPointF(in.center().x(), in.bottom()));
        break;
    }
    case Warning: {
        QPainterPath tri;
        tri.moveTo(at(0.5, 0.06));
        tri.lineTo(at(0.96, 0.9));
        tri.lineTo(at(0.04, 0.9));
        tri.closeSubpath();
        QLinearGradient g(r.topLeft(), r.bottomLeft());
        g.setColorAt(0, QColor(0xFF, 0xE8, 0x7A));
        g.setColorAt(1, QColor(0xF0, 0xB4, 0x00));
        p->setPen(QPen(QColor(0xA0, 0x70, 0x00), std::max(1.0, side / 16), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(g);
        p->drawPath(tri);
        p->setPen(QPen(Qt::black, side / 9, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(at(0.5, 0.36), at(0.5, 0.6));
        p->drawPoint(at(0.5, 0.75));
        break;
    }
    case Power: {
        pen.setCapStyle(Qt::RoundCap);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        const QRectF ring = r.adjusted(r.width() * 0.2, r.height() * 0.22, -r.width() * 0.2, -r.height() * 0.18);
        p->drawArc(ring, 120 * 16, -240 * 16);
        p->drawLine(at(0.5, 0.14), at(0.5, 0.5));
        break;
    }
    case Restart: {
        pen.setCapStyle(Qt::RoundCap);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        const QRectF ring = r.adjusted(r.width() * 0.2, r.height() * 0.2, -r.width() * 0.2, -r.height() * 0.2);
        p->drawArc(ring, 70 * 16, -300 * 16);
        // Arrow head at the arc's end (top, pointing right).
        triangle(at(0.5, 0.06), at(0.5, 0.34), at(0.72, 0.2));
        break;
    }
    case Keyboard: {
        p->setPen(QPen(m_color, std::max(1.0, lw / 2)));
        p->setBrush(Qt::NoBrush);
        const QRectF body(at(0.08, 0.26), at(0.92, 0.76));
        p->drawRoundedRect(body, side * 0.06, side * 0.06);
        const qreal key = side * 0.08;
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 5; ++col)
                p->fillRect(QRectF(body.left() + side * 0.08 + col * side * 0.15, body.top() + side * 0.08 + row * side * 0.13, key, key), m_color);
        p->fillRect(QRectF(at(0.3, 0.62), at(0.7, 0.62 + 0.07)), m_color);
        break;
    }
    case Contrast: {
        p->setPen(QPen(m_color, std::max(1.0, lw / 2)));
        p->setBrush(Qt::NoBrush);
        const QRectF disc = r.adjusted(r.width() * 0.14, r.height() * 0.14, -r.width() * 0.14, -r.height() * 0.14);
        p->drawEllipse(disc);
        QPainterPath half;
        half.moveTo(disc.center());
        half.arcTo(disc, 90, 180);
        half.closeSubpath();
        p->fillPath(half, m_color);
        break;
    }
    case None:
        break;
    }
}

void LunaGlyph::setGlyph(Glyph glyph)
{
    if (m_glyph == glyph)
        return;
    m_glyph = glyph;
    update();
    Q_EMIT appearanceChanged();
}

void LunaGlyph::setColor(const QColor &color)
{
    if (m_color == color)
        return;
    m_color = color;
    update();
    Q_EMIT appearanceChanged();
}

void LunaGlyph::setLineWidth(qreal width)
{
    if (qFuzzyCompare(m_lineWidth, width))
        return;
    m_lineWidth = width;
    update();
    Q_EMIT appearanceChanged();
}
