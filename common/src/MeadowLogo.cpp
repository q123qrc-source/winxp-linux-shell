// SPDX-License-Identifier: GPL-3.0-or-later
#include "MeadowLogo.h"
#include "GradientBox.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

MeadowLogo::MeadowLogo(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &MeadowLogo::appearanceChanged, this, [this] { update(); });
}

void MeadowLogo::paint(QPainter *p)
{
    const qreal side = std::min(width(), height());
    if (side <= 0)
        return;
    p->setRenderHint(QPainter::Antialiasing, true);

    // A 2x2 grid of side g, rotated 45 degrees, must fit the item: its
    // diagonal g*sqrt(2) equals the side.
    const qreal grid = side / std::sqrt(2.0);
    const qreal gap = grid * m_gapRatio;
    const qreal tile = (grid - gap) / 2;
    const qreal r = tile * m_radiusRatio;
    const qreal radii[4] = {r, r, r, r};

    p->translate(width() / 2, height() / 2);
    p->rotate(45);

    // Grid order (row-major) after the rotation: top, right, left, bottom.
    static constexpr int colorIndexForCell[4] = {0, 1, 3, 2};
    const QColor fallback[4] = {QColor(0xF5, 0xC2, 0x2B), QColor(0x2E, 0x8B, 0xEA), QColor(0x4C, 0xAE, 0x3A), QColor(0x7C, 0xC4, 0x3E)};

    for (int cell = 0; cell < 4; ++cell) {
        const int row = cell / 2;
        const int col = cell % 2;
        const QRectF rect(-grid / 2 + col * (tile + gap), -grid / 2 + row * (tile + gap), tile, tile);
        const QPainterPath path = GradientBox::roundedPath(rect, radii);
        const int ci = colorIndexForCell[cell];
        QColor base = m_colors.size() == 4 ? QColor(m_colors.at(ci).toString()) : fallback[ci];
        if (!base.isValid())
            base = fallback[ci];

        if (m_monochrome.isValid() && m_monochrome.alpha() > 0) {
            p->fillPath(path, m_monochrome);
            continue;
        }

        // Light from the top-left of the unrotated mark, i.e. from the top
        // after rotation: lighter toward the upper corner of each pane.
        QLinearGradient body(rect.topLeft(), rect.bottomRight());
        body.setColorAt(0, base.lighter(135));
        body.setColorAt(0.55, base);
        body.setColorAt(1, base.darker(125));
        p->fillPath(path, body);

        // Glossy highlight on the upper-left half of the pane.
        QPainterPath gloss;
        gloss.moveTo(rect.topLeft() + QPointF(r * 0.6, tile * 0.08));
        gloss.lineTo(rect.topRight() - QPointF(tile * 0.18, -tile * 0.08));
        gloss.quadTo(rect.center(), rect.bottomLeft() + QPointF(tile * 0.08, -tile * 0.18));
        gloss.lineTo(rect.topLeft() + QPointF(tile * 0.08, r * 0.6));
        gloss.closeSubpath();
        QColor white(Qt::white);
        white.setAlphaF(m_glossAlpha);
        QLinearGradient glossFill(rect.topLeft(), rect.center());
        glossFill.setColorAt(0, white);
        white.setAlphaF(0);
        glossFill.setColorAt(1, white);
        p->fillPath(gloss.intersected(path), glossFill);

        p->setPen(QPen(base.darker(150), std::max(0.75, side / 96)));
        p->setBrush(Qt::NoBrush);
        p->drawPath(path);
    }
}
