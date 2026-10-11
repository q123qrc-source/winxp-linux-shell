// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <functional>

class SoundPlayer;
class SessionService;
class PowerService;

// The shell's sound events (theme.json "sounds", the meadow sound theme):
//   sessionStarted()   "logon" once the desktop is up (not after a restart
//                      of a crashed shell: MEADOW_RESTARTED=1)
//   logOff()           "logoff", then SessionService::logOff()
//   powerAction(a)     "shutdown" before powerOff/reboot, then PowerService;
//                      suspend and hibernate go at once
//   notify(...)        a notification's sound (see NotificationServer)
//   play(event)        anything else (Run errors, emptying the bin)
// The action after a sound waits for it to end, but never longer than
// sounds.maxWaitMs; with sounds off it happens immediately.
class ShellSounds : public QObject
{
    Q_OBJECT

public:
    ShellSounds(SessionService *session, PowerService *power, QObject *parent = nullptr);
    ~ShellSounds() override;

    Q_INVOKABLE void play(const QString &event);
    Q_INVOKABLE void logOff();
    Q_INVOKABLE void powerAction(const QString &action);
    void sessionStarted();
    // urgency: 0 low (silent), 1 normal, 2 critical. soundName is a freedesktop
    // sound name (looked up in the theme), soundFile a local file.
    void notify(int urgency, const QString &soundName, const QString &soundFile);

    int maxWaitMs() const { return m_maxWait.interval(); }

Q_SIGNALS:
    // For tests: an action ran after its sound.
    void actionRun(const QString &action);

protected:
    // Starts the event's sound; false if nothing plays (muted, missing).
    virtual bool startSound(const QString &event);
    virtual bool startSoundFile(const QString &path);

protected Q_SLOTS:
    void soundFinished();

private:
    void thenRun(const QString &event, const QString &name, std::function<void()> action);

    QPointer<SessionService> m_session;
    QPointer<PowerService> m_power;
    SoundPlayer *m_player;
    QTimer m_maxWait;
    std::function<void()> m_pending;
    QString m_pendingName;
};
