// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Logging.h"

#include <QObject>
#include <QtQml/qqmlregistration.h>

// Plays theme sound events ("logon", "notify", ...) named in theme.json.
// Phase 2 implements playback through QtMultimedia and honors mute settings.
class MEADOW_COMMON_EXPORT SoundPlayer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)

public:
    explicit SoundPlayer(QObject *parent = nullptr);

    bool muted() const { return m_muted; }
    void setMuted(bool muted);

    Q_INVOKABLE void play(const QString &event);

Q_SIGNALS:
    void mutedChanged();

private:
    bool m_muted = false;
};
