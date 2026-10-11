// SPDX-License-Identifier: GPL-3.0-or-later
#include "ShellSounds.h"

#include "PowerService.h"
#include "SessionService.h"
#include "SoundPlayer.h"
#include "ThemeLoader.h"

#include <QFileInfo>
#include <QLoggingCategory>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(lcShellSound, "meadow.shell.sound")

ShellSounds::ShellSounds(SessionService *session, PowerService *power, QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_power(power)
    , m_player(new SoundPlayer(this))
{
    m_maxWait.setSingleShot(true);
    m_maxWait.setInterval(ThemeLoader::instance()->token(QStringLiteral("sounds.maxWaitMs")).toInt());
    connect(&m_maxWait, &QTimer::timeout, this, &ShellSounds::soundFinished);
    connect(m_player, &SoundPlayer::finished, this, &ShellSounds::soundFinished);
}

ShellSounds::~ShellSounds() = default;

bool ShellSounds::startSound(const QString &event)
{
    return m_player->play(event);
}

bool ShellSounds::startSoundFile(const QString &path)
{
    return m_player->playFile(path);
}

void ShellSounds::play(const QString &event)
{
    if (m_pending)
        return; // a log-off or shutdown sound is playing; let it finish
    startSound(event);
}

void ShellSounds::sessionStarted()
{
    if (qEnvironmentVariableIntValue("MEADOW_RESTARTED") == 1) {
        qCInfo(lcShellSound) << "shell restarted; no logon sound";
        return;
    }
    play(QStringLiteral("logon"));
}

void ShellSounds::thenRun(const QString &event, const QString &name, std::function<void()> action)
{
    if (m_pending)
        return; // already on the way out
    m_pending = std::move(action);
    m_pendingName = name;
    if (startSound(event))
        m_maxWait.start();
    else
        soundFinished();
}

void ShellSounds::soundFinished()
{
    m_maxWait.stop();
    if (!m_pending)
        return;
    auto action = std::move(m_pending);
    m_pending = nullptr;
    qCInfo(lcShellSound) << "running" << m_pendingName;
    action();
    Q_EMIT actionRun(m_pendingName);
}

void ShellSounds::logOff()
{
    thenRun(QStringLiteral("logoff"), QStringLiteral("logOff"), [this] {
        if (m_session)
            m_session->logOff();
    });
}

void ShellSounds::powerAction(const QString &action)
{
    static const QStringList known = {QStringLiteral("powerOff"), QStringLiteral("reboot"),
                                      QStringLiteral("suspend"), QStringLiteral("hibernate")};
    if (!known.contains(action)) {
        qCWarning(lcShellSound) << "unknown power action" << action;
        return;
    }
    auto run = [this, action] {
        if (!m_power)
            return;
        if (action == QLatin1String("powerOff"))
            m_power->powerOff();
        else if (action == QLatin1String("reboot"))
            m_power->reboot();
        else if (action == QLatin1String("suspend"))
            m_power->suspend();
        else
            m_power->hibernate();
    };
    if (action == QLatin1String("powerOff") || action == QLatin1String("reboot"))
        thenRun(QStringLiteral("shutdown"), action, run);
    else
        thenRun(QString(), action, run); // no sound; runs at once
}

void ShellSounds::notify(int urgency, const QString &soundName, const QString &soundFile)
{
    if (m_pending)
        return;
    if (!soundFile.isEmpty() && QFileInfo(soundFile).isAbsolute() && startSoundFile(soundFile))
        return;
    if (!soundName.isEmpty()) {
        // A freedesktop sound name: our theme's file if it has one.
        static const QRegularExpression valid(QStringLiteral("^[a-z0-9-]+$"));
        if (valid.match(soundName).hasMatch()) {
            for (const QString &dir : SoundPlayer::soundDirs()) {
                const QString file = dir + QLatin1Char('/') + soundName + QStringLiteral(".oga");
                if (QFileInfo::exists(file) && startSoundFile(file))
                    return;
            }
        }
    }
    if (urgency >= 2)
        play(QStringLiteral("exclamation"));
    else if (urgency == 1)
        play(QStringLiteral("balloon"));
    // Low urgency: silent unless the notification named a sound.
}
