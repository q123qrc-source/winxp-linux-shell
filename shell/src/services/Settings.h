// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

// ~/.config/meadow/shell.conf with defaults from /usr/share/meadow.
// Stub: implemented in phase 6.
class Settings : public QObject
{
    Q_OBJECT

public:
    explicit Settings(QObject *parent = nullptr);
};
