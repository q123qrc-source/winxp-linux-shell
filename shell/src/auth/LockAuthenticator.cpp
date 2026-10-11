// SPDX-License-Identifier: GPL-3.0-or-later
#include "LockAuthenticator.h"

#include <QLoggingCategory>

#include <pwd.h>
#include <unistd.h>

Q_DECLARE_LOGGING_CATEGORY(lcAuth)

namespace {
QString currentUser()
{
    const passwd *pw = getpwuid(getuid());
    return pw && pw->pw_name ? QString::fromLocal8Bit(pw->pw_name) : qEnvironmentVariable("USER");
}
// Simulated authentication answers after this long, like a real PAM round trip.
constexpr int kSimulatedDelayMs = 300;
} // namespace

LockAuthenticator::LockAuthenticator(bool simulate, QObject *parent)
    : QObject(parent)
    , m_simulate(simulate)
    , m_user(simulate && qEnvironmentVariableIsSet("USER") ? qEnvironmentVariable("USER") : currentUser())
{
    m_simulateTimer.setSingleShot(true);
    m_simulateTimer.setInterval(kSimulatedDelayMs);
    connect(&m_simulateTimer, &QTimer::timeout, this, [this] {
        const bool ok = !m_fingerprintAttempt && m_password == QLatin1String("meadow");
        clearPassword();
        finish(ok);
    });

    connect(&m_session, &AuthSession::prompt, this, [this](const QString &, bool) {
        if (m_fingerprintAttempt || m_answered) {
            // A fingerprint attempt that reached the password prompt has
            // failed; a second prompt (e.g. "change your expired password")
            // cannot be answered from a lock screen.
            m_session.cancel();
            return;
        }
        m_answered = true;
        m_session.respond(m_password);
        clearPassword();
    });
    // PAM text is shown to the user, never logged.
    connect(&m_session, &AuthSession::info, this, &LockAuthenticator::informationMessage);
    connect(&m_session, &AuthSession::error, this, &LockAuthenticator::informationMessage);
    connect(&m_session, &AuthSession::finished, this, [this](bool ok) {
        clearPassword();
        finish(ok);
    });
}

void LockAuthenticator::login(const QString &user, const QString &password, int sessionIndex)
{
    Q_UNUSED(sessionIndex);
    if (m_busy)
        return; // like SDDM: one attempt at a time
    if (user != m_user) {
        qCWarning(lcAuth) << "unlock requested for another account; refused";
        Q_EMIT loginFailed();
        return;
    }
    m_password = password;
    m_fingerprintAttempt = password.isEmpty();
    m_answered = false;
    setBusy(true);
    qCInfo(lcAuth) << (m_fingerprintAttempt ? "fingerprint unlock attempt" : "password unlock attempt");
    if (m_simulate) {
        m_simulateTimer.start();
        return;
    }
    if (!m_session.start()) {
        clearPassword();
        Q_EMIT informationMessage(tr("Meadow cannot check your password right now. "
                                     "If this keeps happening, switch to a text console and run "
                                     "\"loginctl unlock-session\"."));
        finish(false);
    }
}

void LockAuthenticator::cancel()
{
    m_simulateTimer.stop();
    clearPassword();
    if (m_session.isRunning())
        m_session.cancel();
    setBusy(false);
}

void LockAuthenticator::finish(bool success)
{
    if (!m_busy)
        return;
    setBusy(false);
    if (success)
        Q_EMIT loginSucceeded();
    else
        Q_EMIT loginFailed();
}

void LockAuthenticator::clearPassword()
{
    m_password.fill(QChar(u'\0'));
    m_password.clear();
}

void LockAuthenticator::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    Q_EMIT busyChanged();
}
