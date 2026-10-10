// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

// org.meadow.Shell D-Bus interface used by labwc keybinds via meadow-shellctl.
// Stub: implemented in phase 5.
class ShellIpc : public QObject
{
    Q_OBJECT

public:
    explicit ShellIpc(QObject *parent = nullptr);
};
