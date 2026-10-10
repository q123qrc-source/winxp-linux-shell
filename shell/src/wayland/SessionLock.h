// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

// Lock screen surfaces via ext-session-lock-v1.
// Stub: implemented in phase 8.
class SessionLock : public QObject
{
    Q_OBJECT

public:
    explicit SessionLock(QObject *parent = nullptr);
};
