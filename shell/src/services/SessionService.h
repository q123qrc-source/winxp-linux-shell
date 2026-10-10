// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

// Log off, switch user (DisplayManager SwitchToGreeter) and lock.
// Stub: implemented in phase 7.
class SessionService : public QObject
{
    Q_OBJECT

public:
    explicit SessionService(QObject *parent = nullptr);
};
