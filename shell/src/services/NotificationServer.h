// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

// org.freedesktop.Notifications server rendered as tray balloons.
// Stub: implemented in phase 6.
class NotificationServer : public QObject
{
    Q_OBJECT

public:
    explicit NotificationServer(QObject *parent = nullptr);
};
