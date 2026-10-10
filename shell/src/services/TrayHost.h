// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

// StatusNotifierWatcher + StatusNotifierHost with DBusMenu support.
// Stub: implemented in phase 6.
class TrayHost : public QObject
{
    Q_OBJECT

public:
    explicit TrayHost(QObject *parent = nullptr);
};
