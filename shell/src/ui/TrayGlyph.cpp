// SPDX-License-Identifier: GPL-3.0-or-later
#include "TrayGlyph.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

// Artwork palette. These are part of the drawings, not theme metrics.
namespace {
const QColor kOutline(0x1c, 0x2a, 0x3c);
const QColor kSilverLight(0xf4, 0xf6, 0xf8);
const QColor kSilverDark(0x8d, 0x99, 0xa6);
const QColor kScreenLight(0x7f, 0xc4, 0xff);
const QColor kScreenDark(0x1d, 0x5f, 0xc8);
const QColor kGreen(0x3c, 0xb8, 0x3a);
const QColor kYellow(0xf6, 0xc9, 0x2e);
const QColor kRed(0xd9, 0x2f, 0x23);
const QColor kWhite(0xff, 0xff, 0xff);

void drawSpeaker(QPainter *p, bool waves)
{
    QLinearGradient body(0, 4, 0, 12);
    body.setColorAt(0, kSilverLight);
    body.setColorAt(1, kSilverDark);
    QPainterPath cone;
    cone.moveTo(1.5, 6);
    cone.lineTo(4.5, 6);
    cone.lineTo(8.5, 2.5);
    cone.lineTo(8.5, 13.5);
    cone.lineTo(4.5, 10);
    cone.lineTo(1.5, 10);
    cone.closeSubpath();
    p->setPen(QPen(kOutline, 1));
    p->setBrush(body);
    p->drawPath(cone);
    if (!waves)
        return;
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(kOutline, 1.2, Qt::SolidLine, Qt::RoundCap));
    p->drawArc(QRectF(7, 5, 4, 6), -50 * 16, 100 * 16);
    p->drawArc(QRectF(6.5, 3, 7, 10), -55 * 16, 110 * 16);
}

void drawMonitor(QPainter *p, const QRectF &r)
{
    p->setPen(QPen(kOutline, 1));
    QLinearGradient frame(0, r.top(), 0, r.bottom());
    frame.setColorAt(0, kSilverLight);
    frame.setColorAt(1, kSilverDark);
    p->setBrush(frame);
    p->drawRoundedRect(r, 1, 1);
    QLinearGradient screen(r.topLeft(), r.bottomRight());
    screen.setColorAt(0, kScreenLight);
    screen.setColorAt(1, kScreenDark);
    p->setPen(Qt::NoPen);
    p->setBrush(screen);
    p->drawRect(r.adjusted(1.5, 1.5, -1.5, -1.5));
    // Stand.
    p->setPen(QPen(kOutline, 1));
    p->setBrush(kSilverDark);
    p->drawRect(QRectF(r.center().x() - 2, r.bottom() + 0.5, 4, 1.5));
}

void drawBadge(QPainter *p, TrayGlyph::Kind kind)
{
    const QRectF badge(8.5, 8.5, 7, 7);
    p->setPen(QPen(kOutline, 0.8));
    if (kind == TrayGlyph::NetworkOffline) {
        p->setBrush(kRed);
        p->drawEllipse(badge);
        p->setPen(QPen(kWhite, 1.4, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(QPointF(10.5, 10.5), QPointF(13.5, 13.5));
        p->drawLine(QPointF(13.5, 10.5), QPointF(10.5, 13.5));
    } else {
        QPainterPath tri;
        tri.moveTo(badge.center().x(), badge.top());
        tri.lineTo(badge.right(), badge.bottom());
        tri.lineTo(badge.left(), badge.bottom());
        tri.closeSubpath();
        p->setBrush(kYellow);
        p->drawPath(tri);
        p->setPen(QPen(kOutline, 1.1, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(QPointF(12, 11), QPointF(12, 13));
        p->drawPoint(QPointF(12, 14.6));
    }
}
} // namespace

TrayGlyph::TrayGlyph(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

void TrayGlyph::setKind(Kind kind)
{
    if (kind == m_kind)
        return;
    m_kind = kind;
    update();
    Q_EMIT appearanceChanged();
}

void TrayGlyph::setLevel(qreal level)
{
    level = qBound(0.0, level, 1.0);
    if (qFuzzyCompare(level, m_level))
        return;
    m_level = level;
    update();
    Q_EMIT appearanceChanged();
}

void TrayGlyph::paint(QPainter *p)
{
    // All drawings are authored on a 16x16 grid.
    const qreal s = qMin(width(), height()) / 16.0;
    p->setRenderHint(QPainter::Antialiasing);
    p->translate((width() - 16 * s) / 2, (height() - 16 * s) / 2);
    p->scale(s, s);

    switch (m_kind) {
    case Volume:
    case VolumeMuted:
        drawSpeaker(p, m_kind == Volume);
        if (m_kind == VolumeMuted) {
            p->setPen(QPen(kRed, 1.8, Qt::SolidLine, Qt::RoundCap));
            p->setBrush(Qt::NoBrush);
            p->drawEllipse(QRectF(9, 5, 6, 6));
            p->drawLine(QPointF(10, 10), QPointF(14, 6));
        }
        break;
    case NetworkWired:
    case NetworkLimited:
    case NetworkOffline:
        drawMonitor(p, QRectF(1.5, 2.5, 11, 8.5));
        // Cable to the wall.
        p->setPen(QPen(kOutline, 1));
        p->drawLine(QPointF(7, 13.5), QPointF(14.5, 13.5));
        if (m_kind == NetworkWired) {
            p->setPen(Qt::NoPen);
            p->setBrush(kGreen);
            p->drawEllipse(QRectF(12.5, 12, 3, 3));
        } else {
            drawBadge(p, m_kind);
        }
        break;
    case NetworkWireless: {
        drawMonitor(p, QRectF(0.5, 4.5, 9, 7));
        // Five rising signal bars, filled up to the level.
        const int lit = qRound(m_level * 5);
        for (int i = 0; i < 5; ++i) {
            const QRectF bar(10 + i * 1.2, 13 - (i + 1) * 2, 1, (i + 1) * 2);
            p->setPen(Qt::NoPen);
            p->setBrush(i < lit ? kGreen : kSilverDark);
            p->drawRect(bar);
        }
        break;
    }
    case Battery:
    case BatteryCharging: {
        const QRectF body(1.5, 4.5, 12, 7);
        p->setPen(QPen(kOutline, 1));
        p->setBrush(kSilverLight);
        p->drawRoundedRect(body, 1, 1);
        p->setBrush(kSilverDark);
        p->drawRect(QRectF(13.5, 6.5, 1.5, 3));
        const QColor fill = m_level > 0.3 ? kGreen : (m_level > 0.1 ? kYellow : kRed);
        p->setPen(Qt::NoPen);
        p->setBrush(fill);
        p->drawRect(QRectF(2.5, 5.5, 10 * m_level, 5));
        if (m_kind == BatteryCharging) {
            QPainterPath bolt;
            bolt.moveTo(8.5, 2.5);
            bolt.lineTo(5, 8.5);
            bolt.lineTo(7.5, 8.5);
            bolt.lineTo(6.5, 13.5);
            bolt.lineTo(10.5, 7);
            bolt.lineTo(8, 7);
            bolt.closeSubpath();
            p->setPen(QPen(kOutline, 0.8));
            p->setBrush(kYellow);
            p->drawPath(bolt);
        }
        break;
    }
    case ShowDesktop: {
        // A desk blotter with a small hill: "the desktop".
        const QRectF desk(1.5, 2.5, 13, 11);
        p->setPen(QPen(kOutline, 1));
        QLinearGradient sky(0, desk.top(), 0, desk.bottom());
        sky.setColorAt(0, kScreenLight);
        sky.setColorAt(1, kScreenDark);
        p->setBrush(sky);
        p->drawRect(desk);
        QPainterPath hill;
        hill.moveTo(2, 13);
        hill.quadTo(7, 6.5, 14, 11);
        hill.lineTo(14, 13);
        hill.closeSubpath();
        p->setPen(Qt::NoPen);
        p->setBrush(kGreen);
        p->drawPath(hill);
        p->setBrush(kWhite);
        p->drawRect(QRectF(3.5, 4.5, 4, 3));
        break;
    }
    }
}
