// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

// logind PowerOff/Reboot/Suspend/Hibernate and their CanXxx checks.
// Stub: implemented in phase 7.
class PowerService : public QObject
{
    Q_OBJECT

public:
    explicit PowerService(QObject *parent = nullptr);
};
