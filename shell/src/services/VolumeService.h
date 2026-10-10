// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>

// PipeWire/WirePlumber default sink volume and mute.
// Stub: implemented in phase 6.
class VolumeService : public QObject
{
    Q_OBJECT

public:
    explicit VolumeService(QObject *parent = nullptr);
};
