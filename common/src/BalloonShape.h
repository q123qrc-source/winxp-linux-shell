// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Logging.h"

#include <QColor>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

// Body of a balloon tip: a rounded rectangle with a slanted tail on its top
// or bottom edge. The tail occupies tailHeight pixels outside the body, so the
// body rect is the item minus that strip (see bodyRect for layout).
class MEADOW_COMMON_EXPORT BalloonShape : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QColor color MEMBER m_color NOTIFY appearanceChanged)
    Q_PROPERTY(QColor borderColor MEMBER m_borderColor NOTIFY appearanceChanged)
    Q_PROPERTY(qreal radius MEMBER m_radius NOTIFY appearanceChanged)
    Q_PROPERTY(qreal tailWidth MEMBER m_tailWidth NOTIFY appearanceChanged)
    Q_PROPERTY(qreal tailHeight MEMBER m_tailHeight NOTIFY appearanceChanged)
    // Horizontal position of the tail tip, in item coordinates.
    Q_PROPERTY(qreal tailX MEMBER m_tailX NOTIFY appearanceChanged)
    Q_PROPERTY(bool tailOnTop MEMBER m_tailOnTop NOTIFY appearanceChanged)

public:
    explicit BalloonShape(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;

Q_SIGNALS:
    void appearanceChanged();

private:
    QColor m_color = QColor(0xFF, 0xFF, 0xE1);
    QColor m_borderColor = Qt::black;
    qreal m_radius = 7;
    qreal m_tailWidth = 18;
    qreal m_tailHeight = 18;
    qreal m_tailX = 30;
    bool m_tailOnTop = false;
};
