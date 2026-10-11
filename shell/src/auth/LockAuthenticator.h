// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "AuthSession.h"

#include <QObject>
#include <QString>
#include <QTimer>

// The lock screen's "greeter": the same contract as SDDM's greeter proxy so
// Meadow.Welcome's WelcomeLayout drives it unchanged (login(), loginFailed(),
// loginSucceeded(), informationMessage()).
//
// login() with a password answers the first PAM prompt with it. login() with
// an empty password is a fingerprint attempt: pam_fprintd (first in the
// meadow-lock stack) reads the finger; if PAM gets as far as asking for a
// password, the attempt has failed and is cancelled, and WelcomeLayout then
// submits any password typed meanwhile.
//
// Like SDDM, a login() while one is running is ignored. Only the locked
// session's user can be authenticated; the helper enforces that too.
// In simulate mode (preview, tests) the password "meadow" is accepted.
class LockAuthenticator : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString userName READ userName CONSTANT)

public:
    explicit LockAuthenticator(bool simulate, QObject *parent = nullptr);

    bool isBusy() const { return m_busy; }
    QString userName() const { return m_user; }

    Q_INVOKABLE void login(const QString &user, const QString &password, int sessionIndex = 0);
    // Abandons a running attempt (e.g. the lock ended some other way).
    Q_INVOKABLE void cancel();

Q_SIGNALS:
    void loginSucceeded();
    void loginFailed();
    void informationMessage(const QString &message);
    void busyChanged();

private:
    void setBusy(bool busy);
    void finish(bool success);
    void clearPassword();

    bool m_simulate;
    QString m_user;
    QString m_password;
    bool m_fingerprintAttempt = false;
    bool m_answered = false;
    bool m_busy = false;
    AuthSession m_session;
    QTimer m_simulateTimer;
};
