// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>

// One PAM conversation, run by meadow-auth-helper in its own process (see
// shell/auth-helper/meadow-auth-helper.cpp for the protocol). Used by the
// lock screen and the polkit agent's fallback.
//
// Secrets passed to respond() are written to the helper and wiped; neither
// they nor PAM messages are logged.
class AuthSession : public QObject
{
    Q_OBJECT

public:
    explicit AuthSession(QObject *parent = nullptr);
    ~AuthSession() override;

    // The helper binary: $MEADOW_AUTH_HELPER, else the installed one.
    static QString helperPath();

    bool isRunning() const { return m_process.state() != QProcess::NotRunning; }
    // Starts a conversation; false if one is running or the helper is missing.
    bool start();
    // Answers the pending prompt.
    void respond(QString secret);
    // Ends the conversation; finished(false) follows.
    void cancel();

Q_SIGNALS:
    void prompt(const QString &message, bool echo);
    void info(const QString &message);
    void error(const QString &message);
    void finished(bool success);

private:
    void readFrames();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void wipeBuffer();

    QProcess m_process;
    QByteArray m_buffer;
    bool m_resultSeen = false;
    bool m_result = false;
};
