// SPDX-License-Identifier: GPL-3.0-or-later
#include "SoundPlayer.h"
#include "ThemeLoader.h"

#include <QUrl>

#include <algorithm>

#ifdef MEADOW_WITH_MULTIMEDIA
#include <QAudioOutput>
#include <QMediaPlayer>
#endif

SoundPlayer::SoundPlayer(QObject *parent)
    : QObject(parent)
{
    const QByteArray mute = qgetenv("MEADOW_MUTE").toLower();
    m_muted = mute == "1" || mute == "true" || mute == "yes";
}

SoundPlayer::~SoundPlayer() = default;

QStringList SoundPlayer::soundDirs()
{
    QStringList dirs;
    if (const QString dev = qEnvironmentVariable("MEADOW_SOUND_DIR"); !dev.isEmpty())
        dirs << dev;
    dirs << QStringLiteral(MEADOW_SOUND_DIR);
    return dirs;
}

void SoundPlayer::setMuted(bool muted)
{
    if (m_muted == muted)
        return;
    m_muted = muted;
#ifdef MEADOW_WITH_MULTIMEDIA
    if (m_muted && m_player)
        m_player->stop();
#endif
    Q_EMIT mutedChanged();
}

void SoundPlayer::setVolume(qreal volume)
{
    volume = std::clamp(volume, 0.0, 1.0);
    if (qFuzzyCompare(m_volume, volume))
        return;
    m_volume = volume;
#ifdef MEADOW_WITH_MULTIMEDIA
    if (m_output)
        m_output->setVolume(float(m_volume));
#endif
    Q_EMIT volumeChanged();
}

QString SoundPlayer::resolve(const QString &event) const
{
    const QString name = ThemeLoader::instance()->token(QStringLiteral("sounds.") + event).toString();
    if (name.isEmpty())
        return {};
    for (const QString &dir : soundDirs()) {
        const QString url = ThemeLoader::instance()->resolveAsset(name, dir);
        if (!url.isEmpty())
            return url;
    }
    return {};
}

bool SoundPlayer::play(const QString &event)
{
    if (m_muted)
        return false;
    const QString url = resolve(event);
    if (url.isEmpty()) {
        qCDebug(lcSound) << "no sound file for event" << event;
        return false;
    }
#ifdef MEADOW_WITH_MULTIMEDIA
    if (!m_player) {
        m_player = new QMediaPlayer(this);
        m_output = new QAudioOutput(this);
        m_output->setVolume(float(m_volume));
        m_player->setAudioOutput(m_output);
        connect(m_player, &QMediaPlayer::errorOccurred, this, [](QMediaPlayer::Error, const QString &message) {
            qCWarning(lcSound).noquote() << "playback failed:" << message;
        });
    }
    m_player->stop();
    m_player->setSource(QUrl(url));
    m_player->play();
    qCDebug(lcSound) << "playing" << event << url;
    return true;
#else
    qCDebug(lcSound) << "built without QtMultimedia; would play" << event << url;
    return false;
#endif
}
