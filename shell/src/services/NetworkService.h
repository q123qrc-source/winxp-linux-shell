// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

// NetworkManager connection state for the tray icon and menu.
// Stub: implemented in phase 6.
class NetworkService : public QObject
{
    Q_OBJECT

public:
    explicit NetworkService(QObject *parent = nullptr);
};
