// SPDX-License-Identifier: GPL-3.0-or-later
// LockAuthenticator with the real meadow-auth-helper under pam_wrapper:
// right and wrong password, a fingerprint attempt (empty password) that
// reaches the password prompt fails without sending anything, one attempt
// at a time, another account is refused, and the simulated (preview) mode.
#include "auth/LockAuthenticator.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <pwd.h>
#include <unistd.h>

class TestLockAuth : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_user;
    bool m_havePamWrapper = false;
    const QString m_password = QStringLiteral("hunter2 ✓");

private Q_SLOTS:
    void initTestCase()
    {
        const passwd *pw = getpwuid(getuid());
        m_user = QString::fromLocal8Bit(pw->pw_name);
        QString matrix;
        for (const char *d : {"/usr/lib64/pam_wrapper", "/usr/lib/pam_wrapper", "/usr/lib/x86_64-linux-gnu/pam_wrapper"}) {
            if (QFile::exists(QLatin1String(d) + QStringLiteral("/pam_matrix.so")))
                matrix = QLatin1String(d) + QStringLiteral("/pam_matrix.so");
        }
        m_havePamWrapper = !matrix.isEmpty();
        if (!m_havePamWrapper)
            return;
        const QString passdb = m_dir.filePath(QStringLiteral("passdb"));
        QFile db(passdb);
        QVERIFY(db.open(QIODevice::WriteOnly));
        db.write(QStringLiteral("%1:%2:lock-test\n").arg(m_user, m_password).toUtf8());
        db.close();
        QDir().mkpath(m_dir.filePath(QStringLiteral("pam")));
        QFile svc(m_dir.filePath(QStringLiteral("pam/lock-test")));
        QVERIFY(svc.open(QIODevice::WriteOnly));
        svc.write(QStringLiteral("auth required %1 passdb=%2\naccount required %1 passdb=%2\n").arg(matrix, passdb).toUtf8());
        svc.close();
        // A wrapper that runs the helper with pam_wrapper preloaded.
        QFile wrapper(m_dir.filePath(QStringLiteral("helper")));
        QVERIFY(wrapper.open(QIODevice::WriteOnly));
        wrapper.write(QStringLiteral("#!/bin/sh\nLD_PRELOAD=libpam_wrapper.so PAM_WRAPPER=1 PAM_WRAPPER_SERVICE_DIR=%1 "
                                     "MEADOW_PAM_SERVICE=lock-test exec %2\n")
                          .arg(m_dir.filePath(QStringLiteral("pam")), QStringLiteral(MEADOW_AUTH_HELPER))
                          .toUtf8());
        wrapper.close();
        wrapper.setPermissions(wrapper.permissions() | QFileDevice::ExeOwner);
        qputenv("MEADOW_AUTH_HELPER", m_dir.filePath(QStringLiteral("helper")).toUtf8());
    }

    void rightPasswordUnlocks()
    {
        if (!m_havePamWrapper)
            QSKIP("pam_wrapper not installed");
        LockAuthenticator auth(false);
        QSignalSpy ok(&auth, &LockAuthenticator::loginSucceeded);
        QSignalSpy failed(&auth, &LockAuthenticator::loginFailed);
        auth.login(m_user, m_password);
        QVERIFY(auth.isBusy());
        QTRY_COMPARE(ok.size(), 1);
        QCOMPARE(failed.size(), 0);
        QVERIFY(!auth.isBusy());
    }

    void wrongPasswordFails()
    {
        if (!m_havePamWrapper)
            QSKIP("pam_wrapper not installed");
        LockAuthenticator auth(false);
        QSignalSpy ok(&auth, &LockAuthenticator::loginSucceeded);
        QSignalSpy failed(&auth, &LockAuthenticator::loginFailed);
        auth.login(m_user, QStringLiteral("guess"));
        QTRY_COMPARE(failed.size(), 1);
        QCOMPARE(ok.size(), 0);
    }

    void fingerprintAttemptReachingPasswordPromptFails()
    {
        if (!m_havePamWrapper)
            QSKIP("pam_wrapper not installed");
        // No fingerprint module in this stack: PAM asks for a password at
        // once, which a fingerprint attempt must not answer.
        LockAuthenticator auth(false);
        QSignalSpy failed(&auth, &LockAuthenticator::loginFailed);
        auth.login(m_user, QString());
        QTRY_COMPARE(failed.size(), 1);
    }

    void oneAttemptAtATime()
    {
        if (!m_havePamWrapper)
            QSKIP("pam_wrapper not installed");
        LockAuthenticator auth(false);
        QSignalSpy ok(&auth, &LockAuthenticator::loginSucceeded);
        QSignalSpy failed(&auth, &LockAuthenticator::loginFailed);
        auth.login(m_user, m_password);
        auth.login(m_user, QStringLiteral("ignored")); // dropped, like SDDM
        QTRY_COMPARE(ok.size() + failed.size(), 1);
        QTest::qWait(200);
        QCOMPARE(ok.size(), 1);
        QCOMPARE(failed.size(), 0);
    }

    void otherAccountRefused()
    {
        LockAuthenticator auth(false);
        QSignalSpy failed(&auth, &LockAuthenticator::loginFailed);
        auth.login(QStringLiteral("root-or-anyone-else-") + m_user, m_password);
        QCOMPARE(failed.size(), 1);
        QVERIFY(!auth.isBusy());
    }

    void missingHelperFailsWithAMessage()
    {
        const QByteArray saved = qgetenv("MEADOW_AUTH_HELPER");
        qputenv("MEADOW_AUTH_HELPER", "/nonexistent/meadow-auth-helper");
        LockAuthenticator auth(false);
        QSignalSpy failed(&auth, &LockAuthenticator::loginFailed);
        QSignalSpy info(&auth, &LockAuthenticator::informationMessage);
        auth.login(m_user, m_password);
        QCOMPARE(failed.size(), 1);
        QCOMPARE(info.size(), 1);
        QVERIFY(info.at(0).at(0).toString().contains(QStringLiteral("loginctl unlock-session")));
        qputenv("MEADOW_AUTH_HELPER", saved);
    }

    void simulatedAcceptsMeadow()
    {
        qputenv("USER", "preview-user");
        LockAuthenticator auth(true);
        QCOMPARE(auth.userName(), QStringLiteral("preview-user"));
        QSignalSpy ok(&auth, &LockAuthenticator::loginSucceeded);
        QSignalSpy failed(&auth, &LockAuthenticator::loginFailed);
        auth.login(QStringLiteral("preview-user"), QStringLiteral("wrong"));
        QTRY_COMPARE(failed.size(), 1);
        auth.login(QStringLiteral("preview-user"), QStringLiteral("meadow"));
        QTRY_COMPARE(ok.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestLockAuth)
#include "test_lockauth.moc"
