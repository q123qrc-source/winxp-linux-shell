// SPDX-License-Identifier: GPL-3.0-or-later
// Tray services against stand-ins: mock UPower/NetworkManager on a private
// session bus (tools/mock-system-services.py), a fake wpctl, xkb rules, and
// the notification server. Run under dbus-run-session.
#include "services/BatteryService.h"
#include "services/KeyboardLayoutService.h"
#include "services/NetworkService.h"
#include "models/AppRegistry.h"
#include "services/NotificationServer.h"
#include "services/PowerService.h"
#include "services/RunService.h"
#include "services/SessionService.h"
#include "services/VolumeService.h"
#include "services/PolkitAgent.h"
#include "services/Settings.h"
#include "services/TimeDateService.h"
#include "services/UserAccountsService.h"
#include "wayland/IdleMonitor.h"
#include "wayland/OutputManager.h"
#include "wayland/ToplevelModel.h"

#include <QDBusConnection>
#include <algorithm>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

// Calls a method in this process's own server from another connection
// without blocking the event loop the server needs to answer.
static QDBusMessage call(QDBusInterface &iface, const QString &method, const QVariantList &args = {})
{
    QDBusPendingCall pending = iface.asyncCallWithArgumentList(method, args);
    QDBusPendingCallWatcher watcher(pending);
    QSignalSpy done(&watcher, &QDBusPendingCallWatcher::finished);
    done.wait(5000);
    return pending.reply();
}

class TestServices : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    struct Mock : QProcess {
        ~Mock() override
        {
            terminate();
            waitForFinished();
        }
    };

    // Starts the mock with `args`; returns when its names are on the bus.
    std::unique_ptr<Mock> startMock(const QStringList &args)
    {
        auto p = std::make_unique<Mock>();
        p->start(QStringLiteral("python3"), QStringList{QStringLiteral(MEADOW_SOURCE_DIR "/tools/mock-system-services.py")} + args);
        if (!p->waitForReadyRead(10000) || !p->readAll().contains("ready"))
            return nullptr;
        return p;
    }

private Q_SLOTS:
    void initTestCase()
    {
        if (!QDBusConnection::sessionBus().isConnected())
            QSKIP("no session bus (run under dbus-run-session)");
        if (QProcess::execute(QStringLiteral("python3"), {QStringLiteral("-c"), QStringLiteral("import dbus, gi.repository.GLib")}) != 0)
            QSKIP("python3-dbus / python3-gobject missing");
    }

    void battery()
    {
        auto mock = startMock({QStringLiteral("--battery"), QStringLiteral("25"), QStringLiteral("--network"), QStringLiteral("none")});
        QVERIFY(mock);
        BatteryService b(QDBusConnection::sessionBus());
        QTRY_VERIFY(b.present());
        QCOMPARE(b.percent(), 25);
        QVERIFY(!b.charging());
        QVERIFY(b.onBattery());
        QCOMPARE(b.minutesLeft(), 130);
    }

    void network_data()
    {
        QTest::addColumn<QString>("mode");
        QTest::addColumn<bool>("connected");
        QTest::addColumn<bool>("limited");
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("name");
        QTest::newRow("wired") << "wired" << true << false << "wired" << "Wired connection 1";
        QTest::newRow("wireless") << "wireless" << true << false << "wireless" << "Meadow Wi-Fi";
        QTest::newRow("limited") << "limited" << true << true << "wired" << "Wired connection 1";
        QTest::newRow("offline") << "offline" << false << false << "none" << "";
    }
    void network()
    {
        QFETCH(QString, mode);
        auto mock = startMock({QStringLiteral("--no-battery"), QStringLiteral("--network"), mode});
        QVERIFY(mock);
        NetworkService n(QDBusConnection::sessionBus());
        QTRY_VERIFY(n.available());
        QFETCH(bool, connected);
        QFETCH(bool, limited);
        QFETCH(QString, kind);
        QFETCH(QString, name);
        QCOMPARE(n.connected(), connected);
        QCOMPARE(n.limited(), limited);
        QTRY_COMPARE(n.kind(), kind);
        QTRY_COMPARE(n.connectionName(), name);
    }

    void noServices()
    {
        BatteryService b(QDBusConnection::sessionBus());
        NetworkService n(QDBusConnection::sessionBus());
        QTest::qWait(300);
        QVERIFY(!b.present());
        QVERIFY(!n.available());
    }

    void volumeParse()
    {
        qreal v = 0;
        bool m = true;
        QVERIFY(VolumeService::parse(QStringLiteral("Volume: 0.40\n"), &v, &m));
        QCOMPARE(v, 0.40);
        QVERIFY(!m);
        QVERIFY(VolumeService::parse(QStringLiteral("Volume: 1.00 [MUTED]"), &v, &m));
        QVERIFY(m);
        QVERIFY(!VolumeService::parse(QStringLiteral("garbage"), &v, &m));
    }

    void volumeFakeWpctl()
    {
        qputenv("MEADOW_WPCTL", MEADOW_SOURCE_DIR "/tools/fake-wpctl");
        qputenv("FAKE_WPCTL_STATE", m_dir.filePath(QStringLiteral("wpctl")).toUtf8());
        VolumeService vol;
        QTRY_VERIFY(vol.available());
        QCOMPARE(vol.volume(), 0.40);
        vol.setMuted(true);
        vol.setVolume(0.75);
        const auto logged = [this](const QByteArray &line) {
            QFile log(m_dir.filePath(QStringLiteral("wpctl.log")));
            return log.open(QIODevice::ReadOnly) && log.readAll().contains(line);
        };
        QTRY_VERIFY(logged("set-volume @DEFAULT_AUDIO_SINK@ 0.75"));
        QTRY_VERIFY(logged("set-mute @DEFAULT_AUDIO_SINK@ 1"));
        QTRY_COMPARE(vol.volume(), 0.75);
        QTRY_VERIFY(vol.muted());
        vol.setVolume(2.0);
        QCOMPARE(vol.volume(), 1.0); // never above 100%
    }

    void keyboardLayouts()
    {
        KeyboardLayoutService k(QStringLiteral("us,ara"), QStringLiteral(",digits"), QStringLiteral(MEADOW_SOURCE_DIR "/tests/fixtures/evdev.xml"));
        QCOMPARE(k.count(), 2);
        QCOMPARE(k.currentShortName(), QStringLiteral("EN"));
        QCOMPARE(k.currentName(), QStringLiteral("English (US)"));
        QSignalSpy spy(&k, &KeyboardLayoutService::changed);
        k.next();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(k.currentShortName(), QStringLiteral("AR"));
        QCOMPARE(k.currentName(), QStringLiteral("Arabic (Eastern Arabic numerals)"));
        QVERIFY(k.environmentFileContent().contains(QStringLiteral("XKB_DEFAULT_LAYOUT=ara,us\n")));
        QVERIFY(k.environmentFileContent().contains(QStringLiteral("XKB_DEFAULT_VARIANT=digits,\n")));
        k.select(1);
        QCOMPARE(k.currentShortName(), QStringLiteral("EN"));
    }

    void keyboardWritesOverlay()
    {
        const QString env = m_dir.filePath(QStringLiteral("overlay/environment.d/90-meadow-keyboard.env"));
        qputenv("MEADOW_LABWC_ENV_FILE", env.toUtf8());
        qputenv("XKB_DEFAULT_LAYOUT", "us,de");
        qputenv("XKB_DEFAULT_OPTIONS", "grp:alt_shift_toggle,caps:none");
        // Keep `labwc --reconfigure` away from any real compositor.
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", m_dir.path().toUtf8());
        {
            KeyboardLayoutService k;
            k.next();
        }
        QFile f(env);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray content = f.readAll();
        QVERIFY(content.contains("XKB_DEFAULT_LAYOUT=de,us\n"));
        // xkb's own group switching would fight the language bar.
        QVERIFY(content.contains("XKB_DEFAULT_OPTIONS=caps:none\n"));
        // A restarted shell continues from the order it wrote.
        KeyboardLayoutService again;
        QCOMPARE(again.layouts().value(0).toMap().value(QStringLiteral("shortName")).toString(), QStringLiteral("DE"));
        qunsetenv("MEADOW_LABWC_ENV_FILE");
        qputenv("PATH", path);
    }

    void notificationsMarkup_data()
    {
        QTest::addColumn<QString>("in");
        QTest::addColumn<QString>("out");
        QTest::newRow("plain") << "a < b & c" << "a &lt; b &amp; c";
        QTest::newRow("allowed") << "<b>bold</b> <i>it</i> <u>u</u>" << "<b>bold</b> <i>it</i> <u>u</u>";
        QTest::newRow("link") << "<a href=\"https://x\">site</a>" << "site";
        QTest::newRow("img") << "<img src=\"/x.png\" alt=\"pic\"/>" << "pic";
        QTest::newRow("script") << "<script>alert(1)</script>" << "alert(1)";
        QTest::newRow("style attr") << "<b style=\"color:red\">x</b>" << "<b>x</b>";
        QTest::newRow("entities") << "&amp; &lt; &#65; &nbsp;" << "&amp; &lt; &#65; &amp;nbsp;";
        QTest::newRow("newline") << "a\nb" << "a<br/>b";
    }
    void notificationsMarkup()
    {
        QFETCH(QString, in);
        QFETCH(QString, out);
        QCOMPARE(NotificationServer::sanitizeBody(in), out);
    }

    void notificationsOverDBus()
    {
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("notify-test"));
        NotificationServer server;
        QVERIFY(server.registerOn(QDBusConnection::sessionBus()));
        server.setDefaultTimeout(200);

        QDBusInterface iface(QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
                             QStringLiteral("org.freedesktop.Notifications"), bus);
        QDBusReply<QStringList> caps = call(iface, QStringLiteral("GetCapabilities"));
        QVERIFY2(caps.isValid(), qPrintable(caps.error().message()));
        QVERIFY(caps.value().contains(QStringLiteral("actions")));
        QDBusMessage info = call(iface, QStringLiteral("GetServerInformation"));
        QCOMPARE(info.arguments().value(3).toString(), QStringLiteral("1.2"));

        QSignalSpy closed(&server, &NotificationServer::NotificationClosed);
        QSignalSpy invoked(&server, &NotificationServer::ActionInvoked);
        QDBusReply<uint> id1 = call(iface, QStringLiteral("Notify"), {QStringLiteral("app"), 0u, QString(), QStringLiteral("One"),
                                          QStringLiteral("<b>hi</b>"), QStringList{QStringLiteral("default"), QStringLiteral("Open")},
                                          QVariantMap(), -1});
        QVERIFY(id1.isValid());
        QTRY_COMPARE(server.rowCount(), 1);
        QCOMPARE(server.current().value(QStringLiteral("summary")).toString(), QStringLiteral("One"));

        // Replacing keeps the id and the position.
        QDBusReply<uint> id1b = call(iface, QStringLiteral("Notify"), {QStringLiteral("app"), id1.value(), QString(),
                                           QStringLiteral("One again"), QString(), QStringList(), QVariantMap(), 0});
        QCOMPARE(id1b.value(), id1.value());
        QCOMPARE(server.rowCount(), 1);
        QCOMPARE(server.current().value(QStringLiteral("summary")).toString(), QStringLiteral("One again"));

        // Expiry (default timeout) with reason 1.
        QDBusReply<uint> id2 = call(iface, QStringLiteral("Notify"), {QStringLiteral("app"), 0u, QString(), QStringLiteral("Two"),
                                          QString(), QStringList(), QVariantMap(), -1});
        QTRY_VERIFY(closed.count() >= 1);
        QCOMPARE(closed.first().value(0).toUInt(), id2.value());
        QCOMPARE(closed.first().value(1).toUInt(), uint(NotificationServer::Expired));

        // The first one (timeout 0) stays until an action or close.
        QCOMPARE(server.rowCount(), 1);
        server.invokeAction(id1.value(), QStringLiteral("default"));
        QCOMPARE(invoked.count(), 1);
        QCOMPARE(server.rowCount(), 0);

        const QVariantMap critical{{QStringLiteral("urgency"), QVariant::fromValue(uchar(2))}};
        QDBusReply<uint> id3 = call(iface, QStringLiteral("Notify"), {QStringLiteral("app"), 0u, QString(), QStringLiteral("Three"),
                                                                      QString(), QStringList(), critical, -1});
        QTest::qWait(400);
        QCOMPARE(server.rowCount(), 1); // critical: no expiry
        call(iface, QStringLiteral("CloseNotification"), {id3.value()});
        QTRY_COMPARE(server.rowCount(), 0);
        QCOMPARE(closed.last().value(1).toUInt(), uint(NotificationServer::ClosedByCall));
        QDBusConnection::disconnectFromBus(QStringLiteral("notify-test"));
    }

    QStringList readLines(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }

    void power()
    {
        const QString log = m_dir.filePath(QStringLiteral("logind.log"));
        auto mock = startMock({QStringLiteral("--no-battery"), QStringLiteral("--network"), QStringLiteral("none"), QStringLiteral("--logind"),
                               QStringLiteral("--can"), QStringLiteral("challenge"), QStringLiteral("--no-hibernate"), QStringLiteral("--log"), log});
        QVERIFY(mock);
        PowerService power(QDBusConnection::sessionBus(), false);
        // "challenge" (polkit will ask) counts as available; "na" does not.
        QTRY_VERIFY(power.canPowerOff() && power.canReboot() && power.canSuspend());
        QVERIFY(!power.canHibernate());
        power.powerOff();
        power.reboot();
        power.suspend();
        QTRY_COMPARE(readLines(log), (QStringList{QStringLiteral("PowerOff true"), QStringLiteral("Reboot true"), QStringLiteral("Suspend true")}));
    }

    void powerUnavailable()
    {
        auto mock = startMock({QStringLiteral("--no-battery"), QStringLiteral("--network"), QStringLiteral("none"), QStringLiteral("--logind"),
                               QStringLiteral("--can"), QStringLiteral("no")});
        QVERIFY(mock);
        PowerService power(QDBusConnection::sessionBus(), false);
        QTest::qWait(300);
        QVERIFY(!power.canPowerOff() && !power.canReboot() && !power.canSuspend() && !power.canHibernate());
    }

    void powerSimulated()
    {
        const QString log = m_dir.filePath(QStringLiteral("logind-sim.log"));
        auto mock = startMock({QStringLiteral("--no-battery"), QStringLiteral("--network"), QStringLiteral("none"), QStringLiteral("--logind"),
                               QStringLiteral("--log"), log});
        QVERIFY(mock);
        PowerService power(QDBusConnection::sessionBus(), true);
        QSignalSpy simulated(&power, &PowerService::simulated);
        QVERIFY(power.canHibernate());
        power.powerOff();
        QCOMPARE(simulated.count(), 1);
        QTest::qWait(300);
        QVERIFY(readLines(log).isEmpty()); // nothing reached logind
    }

    void switchUserAndLogOff()
    {
        const QString log = m_dir.filePath(QStringLiteral("dm.log"));
        auto mock = startMock({QStringLiteral("--no-battery"), QStringLiteral("--network"), QStringLiteral("none"), QStringLiteral("--logind"),
                               QStringLiteral("--display-manager"), QStringLiteral("--log"), log});
        QVERIFY(mock);
        qputenv("XDG_SEAT_PATH", "/org/freedesktop/DisplayManager/Seat0");
        qputenv("XDG_SESSION_ID", "c7");
        qunsetenv("LABWC_PID");
        SessionService session(QDBusConnection::sessionBus(), false);
        QTRY_VERIFY(session.canSwitchUser());
        session.switchUser();
        session.logOff(); // outside labwc: logind TerminateSession
        QTRY_COMPARE(readLines(log), (QStringList{QStringLiteral("SwitchToGreeter"), QStringLiteral("TerminateSession c7")}));

        // Inside labwc, log off asks labwc to exit (recorded, not run).
        const QString launches = m_dir.filePath(QStringLiteral("launch-logoff.log"));
        qputenv("MEADOW_LAUNCH_LOG", launches.toUtf8());
        qputenv("LABWC_PID", "12345");
        session.logOff();
        QCOMPARE(readLines(launches), QStringList{QStringLiteral("labwc\t--exit")});
        qunsetenv("LABWC_PID");
        qunsetenv("MEADOW_LAUNCH_LOG");

        // Without XDG_SEAT_PATH there is no Switch User.
        qunsetenv("XDG_SEAT_PATH");
        SessionService noSeat(QDBusConnection::sessionBus(), false);
        QTest::qWait(200);
        QVERIFY(!noSeat.canSwitchUser());
    }

    void runDialog()
    {
        const QString launches = m_dir.filePath(QStringLiteral("launch-run.log"));
        qputenv("MEADOW_LAUNCH_LOG", launches.toUtf8());
        qputenv("XDG_DATA_DIRS", MEADOW_SOURCE_DIR "/tests/fixtures/menu/data");
        qputenv("XDG_DATA_HOME", m_dir.filePath(QStringLiteral("run-data")).toUtf8());
        AppRegistry apps;
        RunService run(&apps, m_dir.filePath(QStringLiteral("run-history")));
        QDir().mkpath(m_dir.filePath(QStringLiteral("a folder")));

        QVERIFY(run.run(QStringLiteral("true --flag 'two words'")).isEmpty());            // PATH program + args
        QVERIFY(run.run(QStringLiteral("https://example.org")).isEmpty());                // URL
        QVERIFY(run.run(QLatin1Char('"') + m_dir.filePath(QStringLiteral("a folder")) + QLatin1Char('"')).isEmpty()); // folder
        QVERIFY(run.run(QStringLiteral("Calculator")).isEmpty());                         // application by name
        QVERIFY(run.run(QStringLiteral("chess")).isEmpty());                              // by desktop id
        const QString err = run.run(QStringLiteral("meadow-no-such-program"));
        QVERIFY(err.contains(QStringLiteral("meadow-no-such-program")));
        QVERIFY(!run.run(QStringLiteral("   ")).isEmpty());

        const QStringList log = readLines(launches);
        QCOMPARE(log.size(), 5);
        QCOMPARE(log.at(0), QStringLiteral("true\t--flag\ttwo words"));
        QCOMPARE(log.at(1), QStringLiteral("xdg-open\thttps://example.org"));
        QVERIFY(log.at(2).endsWith(QLatin1String("\t") + m_dir.filePath(QStringLiteral("a folder"))));
        QCOMPARE(log.at(3), QStringLiteral("calc"));
        QCOMPARE(log.at(4), QStringLiteral("chess"));

        // History: newest first, no duplicates, failures not kept; persisted.
        QCOMPARE(run.history().first(), QStringLiteral("chess"));
        QVERIFY(!run.history().contains(QStringLiteral("meadow-no-such-program")));
        run.run(QStringLiteral("true --flag 'two words'"));
        QCOMPARE(run.history().first(), QStringLiteral("true --flag 'two words'"));
        QCOMPARE(run.history().count(QStringLiteral("true --flag 'two words'")), 1);
        RunService again(&apps, m_dir.filePath(QStringLiteral("run-history")));
        QCOMPARE(again.history(), run.history());

        // Completion: history, then PATH programs, then paths.
        QCOMPARE(run.complete(QStringLiteral("tr")).first(), QStringLiteral("true --flag 'two words'"));
        QVERIFY(run.complete(QStringLiteral("tru")).contains(QStringLiteral("true")));
        QVERIFY(run.complete(m_dir.filePath(QStringLiteral("a fo"))).contains(m_dir.filePath(QStringLiteral("a folder/"))));
        qunsetenv("MEADOW_LAUNCH_LOG");
    }

    void runHistoryBounded()
    {
        qputenv("MEADOW_LAUNCH_LOG", m_dir.filePath(QStringLiteral("launch-bound.log")).toUtf8());
        RunService run(nullptr, m_dir.filePath(QStringLiteral("run-history-bound")));
        for (int i = 0; i < RunService::kMaxHistory + 4; ++i)
            run.run(QStringLiteral("true %1").arg(i));
        QCOMPARE(run.history().size(), RunService::kMaxHistory);
        qunsetenv("MEADOW_LAUNCH_LOG");
    }

    void notificationsQueueBounded()
    {
        NotificationServer server;
        for (int i = 0; i < NotificationServer::kMaxQueued + 5; ++i)
            server.post(QStringLiteral("a"), QStringLiteral("n%1").arg(i), QString(), QString(), 0);
        QCOMPARE(server.rowCount(), NotificationServer::kMaxQueued);
        QCOMPARE(server.current().value(QStringLiteral("summary")).toString(), QStringLiteral("n5"));
    }

    // --- Phase 8 ----------------------------------------------------------

    void logindLockAndSleep()
    {
        const QString log = m_dir.filePath(QStringLiteral("logind-lock.log"));
        auto mock = startMock({QStringLiteral("--no-battery"), QStringLiteral("--network"), QStringLiteral("none"), QStringLiteral("--logind"),
                               QStringLiteral("--log"), log});
        QVERIFY(mock);
        qputenv("XDG_SESSION_ID", "31");
        SessionService session(QDBusConnection::sessionBus(), false);
        QSignalSpy lock(&session, &SessionService::lockRequested);
        QSignalSpy unlock(&session, &SessionService::unlockRequested);
        QSignalSpy sleep(&session, &SessionService::aboutToSleep);
        session.setLockBeforeSleep(true);
        QTRY_VERIFY(readLines(log).contains(QStringLiteral("Inhibit sleep delay")));
        QDBusInterface test(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
                            QStringLiteral("org.meadow.Test"));
        // Wait until the session object is known (GetSession answered).
        QTest::qWait(300);
        call(test, QStringLiteral("Emit"), {QStringLiteral("Lock")});
        QTRY_COMPARE(lock.size(), 1);
        call(test, QStringLiteral("Emit"), {QStringLiteral("Unlock")});
        QTRY_COMPARE(unlock.size(), 1);
        call(test, QStringLiteral("Emit"), {QStringLiteral("PrepareForSleep")});
        QTRY_COMPARE(sleep.size(), 1);
        session.setLockedHint(true);
        QTRY_VERIFY(readLines(log).contains(QStringLiteral("SetLockedHint True")));
        qunsetenv("XDG_SESSION_ID");
    }

    void timeDate()
    {
        const QString log = m_dir.filePath(QStringLiteral("timedate.log"));
        auto mock = startMock({QStringLiteral("--no-battery"), QStringLiteral("--network"), QStringLiteral("none"), QStringLiteral("--timedate"),
                               QStringLiteral("--log"), log});
        QVERIFY(mock);
        TimeDateService td(QDBusConnection::sessionBus(), false);
        QTRY_VERIFY(td.available());
        QCOMPARE(td.timezone(), QStringLiteral("Europe/London"));
        QVERIFY(td.ntp());
        QVERIFY(td.timezones().contains(QStringLiteral("Europe/Paris")));
        td.setTimezone(QStringLiteral("Not/AZone")); // ignored
        td.setTimezone(QStringLiteral("Europe/Paris"));
        QTRY_COMPARE(td.timezone(), QStringLiteral("Europe/Paris"));
        td.setTime(QDateTime::currentDateTime()); // ignored while NTP is on
        td.setNtp(false);
        QTRY_VERIFY(!td.ntp());
        td.setTime(QDateTime::fromSecsSinceEpoch(1700000000));
        QTRY_COMPARE(readLines(log), (QStringList{QStringLiteral("SetTimezone Europe/Paris True"), QStringLiteral("SetNTP False True"),
                                                  QStringLiteral("SetTime 1700000000000000 False True")}));
    }

    void accounts()
    {
        const QString log = m_dir.filePath(QStringLiteral("accounts.log"));
        auto mock = startMock({QStringLiteral("--no-battery"), QStringLiteral("--network"), QStringLiteral("none"), QStringLiteral("--accounts"),
                               QStringLiteral("--log"), log});
        QVERIFY(mock);
        UserAccountsService accounts(QDBusConnection::sessionBus(), false);
        QTRY_COMPARE(accounts.users().size(), 2);
        QVariantMap self;
        for (const QVariant &u : accounts.users()) {
            if (u.toMap().value(QStringLiteral("self")).toBool())
                self = u.toMap();
        }
        QVERIFY(!self.isEmpty());
        QVERIFY(self.value(QStringLiteral("admin")).toBool());
        accounts.setRealName(self.value(QStringLiteral("path")).toString(), QStringLiteral("  New Name "));
        QTRY_VERIFY(readLines(log).size() == 1 && readLines(log).first().endsWith(QStringLiteral("New Name")));
        QTRY_VERIFY(std::any_of(accounts.users().cbegin(), accounts.users().cend(), [](const QVariant &u) {
            return u.toMap().value(QStringLiteral("realName")).toString() == QStringLiteral("New Name");
        }));
    }

    void toplevelMru()
    {
        PreviewToplevelModel m;
        m.addPreviewWindow(QStringLiteral("A"), QStringLiteral("a")); // row 0
        m.addPreviewWindow(QStringLiteral("B"), QStringLiteral("b")); // row 1
        m.addPreviewWindow(QStringLiteral("C"), QStringLiteral("c")); // row 2, active
        QCOMPARE(m.mruRows(), (QList<int>{2, 1, 0}));
        m.activate(0);
        QCOMPARE(m.mruRows(), (QList<int>{0, 2, 1}));
        m.close(2);
        QCOMPARE(m.mruRows().first(), 0);
        QCOMPARE(m.mruRows().size(), 2);
    }

    void outputsSimulated()
    {
        OutputManager out(true);
        QVERIFY(out.isAvailable());
        QCOMPARE(out.outputs().size(), 1);
        const QVariantMap o = out.outputs().first().toMap();
        QCOMPARE(o.value(QStringLiteral("width")).toInt(), 1024);
        QSignalSpy applied(&out, &OutputManager::applied);
        out.apply(o.value(QStringLiteral("name")).toString(), 800, 600, 0, 1.25);
        QCOMPARE(applied.size(), 1);
        QVERIFY(applied.at(0).at(0).toBool());
        QCOMPARE(out.outputs().first().toMap().value(QStringLiteral("width")).toInt(), 800);
        QCOMPARE(out.outputs().first().toMap().value(QStringLiteral("scale")).toDouble(), 1.25);
        QVERIFY(out.canRevert());
        out.revert();
        QCOMPARE(out.outputs().first().toMap().value(QStringLiteral("width")).toInt(), 1024);
        QVERIFY(!out.canRevert());
        out.apply(QStringLiteral("nope"), 800, 600, 0, 1);
        QVERIFY(!applied.last().at(0).toBool());
    }

    void idleSimulated()
    {
        IdleMonitor idle(true);
        QSignalSpy spy(&idle, &IdleMonitor::idleChanged);
        idle.setTimeoutMs(600000);
        QVERIFY(!idle.isIdle());
        idle.setIdle(true);
        QVERIFY(idle.isIdle());
        idle.setIdle(true);
        QCOMPARE(spy.size(), 1);
    }

    void polkitSimulated()
    {
        PolkitAgent agent(true);
        QVERIFY(!agent.registerForSession()); // never registers in preview
        QSignalSpy finished(&agent, &PolkitAgent::finished);
        agent.requestPreview();
        QVERIFY(agent.isActive());
        QCOMPARE(agent.identities().size(), 1);
        agent.submit(QStringLiteral("wrong"));
        QVERIFY(agent.isActive());
        QVERIFY(!agent.feedback().isEmpty());
        agent.submit(QStringLiteral("meadow"));
        QCOMPARE(finished.size(), 1);
        QVERIFY(finished.at(0).at(0).toBool());
        QVERIFY(!agent.isActive());
        agent.requestPreview();
        agent.cancel();
        QCOMPARE(finished.size(), 2);
        QVERIFY(!finished.at(1).at(0).toBool());
    }

    void keyboardAddRemove()
    {
        KeyboardLayoutService k(QStringLiteral("us"), QString(), QStringLiteral(MEADOW_SOURCE_DIR "/tests/fixtures/evdev.xml"));
        const QVariantList available = k.availableLayouts();
        QVERIFY(available.size() >= 3);
        k.addLayout(QStringLiteral("de"));
        QCOMPARE(k.count(), 2);
        k.addLayout(QStringLiteral("de")); // already there
        k.addLayout(QStringLiteral("xx")); // unknown
        QCOMPARE(k.count(), 2);
        k.removeLayout(1);
        QCOMPARE(k.count(), 1);
        k.removeLayout(0); // the last one stays
        QCOMPARE(k.count(), 1);
    }

    void settingsRevision()
    {
        Settings s(m_dir.filePath(QStringLiteral("rev.conf")), QStringLiteral(MEADOW_SOURCE_DIR "/shell/data/shell.conf"));
        const int before = s.revision();
        QCOMPARE(s.stringValue(QStringLiteral("Desktop/wallpaper")), QStringLiteral("meadow-hills.svg"));
        QCOMPARE(s.intSetting(QStringLiteral("ScreenSaver/timeoutMin"), 1), 10);
        QVERIFY(s.boolSetting(QStringLiteral("ScreenSaver/passwordProtect")));
        s.set(QStringLiteral("Desktop/wallpaper"), QString());
        QCOMPARE(s.revision(), before + 1);
        s.set(QStringLiteral("Desktop/wallpaper"), QString()); // unchanged
        QCOMPARE(s.revision(), before + 1);
        QCOMPARE(s.realSetting(QStringLiteral("Mouse/pointerSpeed"), 9), 0.0);
    }
};

QTEST_GUILESS_MAIN(TestServices)
#include "test_services.moc"
