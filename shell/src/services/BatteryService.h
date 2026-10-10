// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

// UPower battery state.
// Stub: implemented in phase 6.
class BatteryService : public QObject
{
    Q_OBJECT

public:
    explicit BatteryService(QObject *parent = nullptr);
};
