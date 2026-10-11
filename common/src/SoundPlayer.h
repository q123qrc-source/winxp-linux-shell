// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Logging.h"

#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class QMediaPlayer;
class QAudioOutput;

// Plays theme sound events ("logon", "notify", ...). The file name for an
// event comes from tokens.sounds; the file is looked up in the asset override
// folders first, then in the installed sound theme.
//
// Muted when: muted is set, all players are muted (setAllMuted: the
// shell's "[Sounds] enabled=false"), MEADOW_MUTE=1, or Meadow was built
// without QtMultimedia (events are then only logged).
class MEADOW_COMMON_EXPORT SoundPlayer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)

public:
    explicit SoundPlayer(QObject *parent = nullptr);
    ~SoundPlayer() override;

    // Directories holding the bundled sound files, highest priority first.
    static QStringList soundDirs();

    bool muted() const { return m_muted; }
    void setMuted(bool muted);
    qreal volume() const { return m_volume; }
    void setVolume(qreal volume);

    // File URL that play(event) would use; empty if the event or file is unknown.
    Q_INVOKABLE QString resolve(const QString &event) const;
    // Returns true if playback was started; finished() follows.
    Q_INVOKABLE bool play(const QString &event);
    // A sound file (absolute path or file URL) outside the theme, e.g. a
    // notification's sound-file hint.
    Q_INVOKABLE bool playFile(const QString &pathOrUrl);
    Q_INVOKABLE void stop();

    // Mutes every SoundPlayer in the process at once, live.
    static void setAllMuted(bool muted);
    static bool allMuted();

    // Seconds since the system booted (/proc/uptime), or -1 if unknown. The
    // greeter plays the startup sound only shortly after boot.
    Q_INVOKABLE static double secondsSinceBoot();

Q_SIGNALS:
    void mutedChanged();
    void volumeChanged();
    // Playback that play() started has ended (or failed).
    void finished();

private:
    bool startPlayback(const QString &url, const QString &what);

    bool m_muted = false;
    qreal m_volume = 1.0;
    // Declared unconditionally: the class layout must not depend on a define
    // that only the library sees (users would disagree on sizeof).
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_output = nullptr;
};
