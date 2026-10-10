// SPDX-License-Identifier: GPL-3.0-or-later
// org.meadow.Shell end to end: the service registers on a private session bus
// (ctest wraps this in dbus-run-session), and the real meadow-shellctl script
// reaches it for every command.
#include "ipc/ShellIpc.h"

#include <QDBusConnection>
#include <QProcess>
#include <QSignalSpy>
#include <QTest>

class TestShellIpc : public QObject
{
    Q_OBJECT

    ShellIpc *m_ipc = nullptr;

    // Runs meadow-shellctl while the event loop keeps serving D-Bus calls.
    QPair<int, QString> shellctl(const QString &command)
    {
        QProcess p;
        p.start(QStringLiteral(MEADOW_SHELLCTL), {command});
        if (!p.waitForStarted())
            return {-1, QStringLiteral("could not start")};
        while (p.state() != QProcess::NotRunning)
            QTest::qWait(10);
        return {p.exitCode(), QString::fromUtf8(p.readAllStandardOutput() + p.readAllStandardError()).trimmed()};
    }

private Q_SLOTS:
    void initTestCase()
    {
        if (!QDBusConnection::sessionBus().isConnected())
            QSKIP("no session bus (run under dbus-run-session)");
        m_ipc = new ShellIpc(this);
        QVERIFY(m_ipc->registerOnSessionBus());
    }

    void secondInstanceCannotClaimTheName()
    {
        ShellIpc other;
        QVERIFY(!other.registerOnSessionBus());
    }

    void commands_data()
    {
        QTest::addColumn<QString>("command");
        QTest::addColumn<QByteArray>("signal");
        QTest::newRow("toggle-start-menu") << QStringLiteral("toggle-start-menu") << QByteArray(SIGNAL(toggleStartMenuRequested()));
        QTest::newRow("run") << QStringLiteral("run") << QByteArray(SIGNAL(runDialogRequested()));
        QTest::newRow("show-desktop") << QStringLiteral("show-desktop") << QByteArray(SIGNAL(showDesktopRequested()));
        QTest::newRow("lock") << QStringLiteral("lock") << QByteArray(SIGNAL(lockRequested()));
        QTest::newRow("log-off") << QStringLiteral("log-off") << QByteArray(SIGNAL(logOffDialogRequested()));
        QTest::newRow("turn-off") << QStringLiteral("turn-off") << QByteArray(SIGNAL(turnOffDialogRequested()));
        QTest::newRow("next-layout") << QStringLiteral("next-layout") << QByteArray(SIGNAL(nextKeyboardLayoutRequested()));
    }

    void commands()
    {
        QFETCH(QString, command);
        QFETCH(QByteArray, signal);
        QSignalSpy spy(m_ipc, signal.constData());
        const auto [code, output] = shellctl(command);
        QVERIFY2(code == 0, qPrintable(output));
        QCOMPARE(spy.count(), 1);
    }

    void version()
    {
        const auto [code, output] = shellctl(QStringLiteral("version"));
        QCOMPARE(code, 0);
        QCOMPARE(output, QStringLiteral(MEADOW_VERSION));
    }

    void unknownCommandIsUsageError()
    {
        QCOMPARE(shellctl(QStringLiteral("frobnicate")).first, 2);
    }

    void reportsWhenNoShellIsRunning()
    {
        delete m_ipc;
        m_ipc = nullptr;
        QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(ShellIpc::kService));
        const auto [code, output] = shellctl(QStringLiteral("run"));
        QCOMPARE(code, 1);
        QVERIFY2(output.contains(QLatin1String("not running")), qPrintable(output));
    }
};

QTEST_GUILESS_MAIN(TestShellIpc)
#include "test_shellipc.moc"
