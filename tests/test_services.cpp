// SPDX-License-Identifier: GPL-3.0-or-later
// Tray services against stand-ins: mock UPower/NetworkManager on a private
// session bus (tools/mock-system-services.py), a fake wpctl, xkb rules, and
// the notification server. Run under dbus-run-session.
#include "services/BatteryService.h"
#include "services/KeyboardLayoutService.h"
#include "services/NetworkService.h"
#include "services/NotificationServer.h"
#include "services/VolumeService.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
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

    void notificationsQueueBounded()
    {
        NotificationServer server;
        for (int i = 0; i < NotificationServer::kMaxQueued + 5; ++i)
            server.post(QStringLiteral("a"), QStringLiteral("n%1").arg(i), QString(), QString(), 0);
        QCOMPARE(server.rowCount(), NotificationServer::kMaxQueued);
        QCOMPARE(server.current().value(QStringLiteral("summary")).toString(), QStringLiteral("n5"));
    }
};

QTEST_GUILESS_MAIN(TestServices)
#include "test_services.moc"
