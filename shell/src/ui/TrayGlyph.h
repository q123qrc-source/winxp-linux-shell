// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

// Meadow's own small status icons (volume, network, battery, show desktop),
// drawn procedurally at any size so no icon theme is needed. The drawings
// are original; they borrow only the general idea of each XP icon.
class TrayGlyph : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Kind kind READ kind WRITE setKind NOTIFY appearanceChanged)
    // Battery charge or wireless signal, 0..1.
    Q_PROPERTY(qreal level READ level WRITE setLevel NOTIFY appearanceChanged)

public:
    enum Kind {
        Volume,
        VolumeMuted,
        NetworkWired,
        NetworkWireless,
        NetworkLimited,
        NetworkOffline,
        Battery,
        BatteryCharging,
        ShowDesktop,
    };
    Q_ENUM(Kind)

    explicit TrayGlyph(QQuickItem *parent = nullptr);

    Kind kind() const { return m_kind; }
    void setKind(Kind kind);
    qreal level() const { return m_level; }
    void setLevel(qreal level);

    void paint(QPainter *painter) override;

Q_SIGNALS:
    void appearanceChanged();

private:
    Kind m_kind = Volume;
    qreal m_level = 1.0;
};
