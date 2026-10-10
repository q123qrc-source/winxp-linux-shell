// SPDX-License-Identifier: GPL-3.0-or-later
#include "SoundPlayer.h"

SoundPlayer::SoundPlayer(QObject *parent)
    : QObject(parent)
{
}

void SoundPlayer::setMuted(bool muted)
{
    if (m_muted == muted)
        return;
    m_muted = muted;
    Q_EMIT mutedChanged();
}

void SoundPlayer::play(const QString &event)
{
    if (m_muted)
        return;
    qCDebug(lcSound) << "sound event (playback lands in phase 2):" << event;
}
