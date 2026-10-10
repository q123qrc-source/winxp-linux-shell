// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

// polkit authentication agent with Luna-styled dialogs.
// Stub: implemented in phase 6.
class PolkitAgent : public QObject
{
    Q_OBJECT

public:
    explicit PolkitAgent(QObject *parent = nullptr);
};
