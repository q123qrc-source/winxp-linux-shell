// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Logging.h"

#include <QColor>
#include <QQuickPaintedItem>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Meadow's original mark: four glossy rounded panes arranged as a diamond
// (a 2x2 grid turned 45 degrees): sun on top, sky on the right, grass at the
// bottom, leaves on the left. Colors come from tokens.logo; a monochrome
// variant is used on small or busy backgrounds.
class MEADOW_COMMON_EXPORT MeadowLogo : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    // Four colors: top, right, bottom, left.
    Q_PROPERTY(QVariantList colors MEMBER m_colors NOTIFY appearanceChanged)
    // When opaque, every pane is drawn in this color (no gloss); transparent
    // (the default) means full color.
    Q_PROPERTY(QColor monochrome MEMBER m_monochrome NOTIFY appearanceChanged)
    Q_PROPERTY(qreal gapRatio MEMBER m_gapRatio NOTIFY appearanceChanged)
    Q_PROPERTY(qreal radiusRatio MEMBER m_radiusRatio NOTIFY appearanceChanged)
    Q_PROPERTY(qreal glossAlpha MEMBER m_glossAlpha NOTIFY appearanceChanged)

public:
    explicit MeadowLogo(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

Q_SIGNALS:
    void appearanceChanged();

private:
    QVariantList m_colors;
    QColor m_monochrome;
    qreal m_gapRatio = 0.07;
    qreal m_radiusRatio = 0.3;
    qreal m_glossAlpha = 0.45;
};
