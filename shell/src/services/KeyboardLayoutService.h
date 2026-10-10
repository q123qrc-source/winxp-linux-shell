// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

// Keyboard layout list and switching for the language bar.
// Stub: implemented in phase 6.
class KeyboardLayoutService : public QObject
{
    Q_OBJECT

public:
    explicit KeyboardLayoutService(QObject *parent = nullptr);
};
