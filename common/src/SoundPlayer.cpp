// SPDX-License-Identifier: GPL-3.0-or-later
#include "SoundPlayer.h"
#include "ThemeLoader.h"

#include <QFile>
#include <QUrl>

#include <algorithm>
#include <atomic>

#ifdef MEADOW_WITH_MULTIMEDIA
#include <QAudioOutput>
#include <QMediaPlayer>
#endif

namespace {
std::atomic_bool g_allMuted{false};
}

void SoundPlayer::setAllMuted(bool muted)
{
    g_allMuted = muted;
}

bool SoundPlayer::allMuted()
{
    return g_allMuted;
}

double SoundPlayer::secondsSinceBoot()
{
    QFile f(QStringLiteral("/proc/uptime"));
    if (!f.open(QIODevice::ReadOnly))
        return -1;
    bool ok = false;
    const double up = QString::fromLatin1(f.readLine()).section(QLatin1Char(' '), 0, 0).toDouble(&ok);
    return ok ? up : -1;
}

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
    if (m_muted || g_allMuted)
        return false;
    const QString url = resolve(event);
    if (url.isEmpty()) {
        qCDebug(lcSound) << "no sound file for event" << event;
        return false;
    }
    return startPlayback(url, event);
}

bool SoundPlayer::playFile(const QString &pathOrUrl)
{
    if (m_muted || g_allMuted || pathOrUrl.isEmpty())
        return false;
    const QUrl url = pathOrUrl.startsWith(QLatin1Char('/')) ? QUrl::fromLocalFile(pathOrUrl) : QUrl(pathOrUrl);
    if (!url.isLocalFile() || !QFile::exists(url.toLocalFile())) {
        qCDebug(lcSound) << "not a local sound file:" << pathOrUrl;
        return false;
    }
    return startPlayback(url.toString(), QStringLiteral("file"));
}

void SoundPlayer::stop()
{
#ifdef MEADOW_WITH_MULTIMEDIA
    if (m_player)
        m_player->stop();
#endif
}

bool SoundPlayer::startPlayback(const QString &url, const QString &what)
{
#ifdef MEADOW_WITH_MULTIMEDIA
    if (!m_player) {
        m_player = new QMediaPlayer(this);
        m_output = new QAudioOutput(this);
        m_output->setVolume(float(m_volume));
        m_player->setAudioOutput(m_output);
        connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &message) {
            qCWarning(lcSound).noquote() << "playback failed:" << message;
            Q_EMIT finished();
        });
        connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::EndOfMedia || status == QMediaPlayer::InvalidMedia)
                Q_EMIT finished();
        });
    }
    m_player->stop();
    m_player->setSource(QUrl(url));
    m_player->play();
    qCDebug(lcSound) << "playing" << what << url;
    return true;
#else
    qCDebug(lcSound) << "built without QtMultimedia; would play" << what << url;
    return false;
#endif
}
