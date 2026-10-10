// SPDX-License-Identifier: GPL-3.0-or-later
// .desktop parsing (Desktop Entry Specification) and the app_id -> desktop
// entry matching the taskbar uses for icons and group names.
#include "models/AppRegistry.h"
#include "models/DesktopEntry.h"

#include <QTest>

#define FIXTURES MEADOW_SOURCE_DIR "/tests/fixtures"

class TestApps : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        qputenv("XDG_DATA_HOME", FIXTURES "/apps-extra");
        qputenv("XDG_DATA_DIRS", FIXTURES);
    }

    void parse()
    {
        const DesktopEntry e = DesktopEntry::parse(QStringLiteral(FIXTURES "/apps-extra/applications/org.example.Viewer.desktop"),
                                                   QStringLiteral("org.example.Viewer.desktop"), QStringLiteral("de_DE"));
        QVERIFY(e.isValid());
        QCOMPARE(e.name, QStringLiteral("Bildbetrachter"));
        QCOMPARE(e.genericName, QStringLiteral("Image Viewer"));
        // "\;" escapes a semicolon inside a list item.
        QCOMPARE(e.categories, (QStringList{QStringLiteral("Graphics"), QStringLiteral("Viewer;Photo")}));
        QCOMPARE(e.startupWMClass, QStringLiteral("ExampleViewer"));
        const DesktopEntry en = DesktopEntry::parse(e.path, e.id, QStringLiteral("en_US"));
        QCOMPARE(en.name, QStringLiteral("Picture Viewer"));
    }

    void execArguments()
    {
        const DesktopEntry e = DesktopEntry::parse(QStringLiteral(FIXTURES "/apps-extra/applications/org.example.Viewer.desktop"),
                                                   QStringLiteral("org.example.Viewer.desktop"), QStringLiteral("C"));
        QCOMPARE(e.execArguments({QStringLiteral("/tmp/a b.png"), QStringLiteral("/tmp/c.png")}),
                 (QStringList{QStringLiteral("viewer"), QStringLiteral("--title"), QStringLiteral("My \"Viewer\""),
                              QStringLiteral("/tmp/a b.png"), QStringLiteral("/tmp/c.png"), QStringLiteral("--name=Picture Viewer")}));
        QCOMPARE(e.execArguments(), (QStringList{QStringLiteral("viewer"), QStringLiteral("--title"), QStringLiteral("My \"Viewer\""),
                                                 QStringLiteral("--name=Picture Viewer")}));
    }

    void shownIn()
    {
        const DesktopEntry e = DesktopEntry::parse(QStringLiteral(FIXTURES "/apps-extra/applications/hidden-tool.desktop"),
                                                   QStringLiteral("hidden-tool.desktop"));
        QVERIFY(e.isValid());
        QVERIFY(!e.shownIn(QStringLiteral("Meadow")));
    }

    void matchAppId_data()
    {
        QTest::addColumn<QString>("appId");
        QTest::addColumn<QString>("desktopId");
        QTest::newRow("exact") << "firefox" << "firefox.desktop";
        QTest::newRow("case") << "Firefox" << "firefox.desktop";
        QTest::newRow("reverse dns") << "org.example.Viewer" << "org.example.Viewer.desktop";
        QTest::newRow("wm class") << "ExampleViewer" << "org.example.Viewer.desktop";
        QTest::newRow("last component") << "org.mozilla.xterm" << "xterm.desktop";
        QTest::newRow("unknown") << "nothing-here" << "";
    }
    void matchAppId()
    {
        QFETCH(QString, appId);
        QFETCH(QString, desktopId);
        AppRegistry apps;
        QCOMPARE(apps.desktopIdForApp(appId), desktopId);
    }

    void namesAndIcons()
    {
        AppRegistry apps;
        QCOMPARE(apps.nameForApp(QStringLiteral("firefox")), QStringLiteral("Web Browser"));
        QCOMPARE(apps.iconForApp(QStringLiteral("ExampleViewer")), QStringLiteral("org.example.Viewer"));
        QCOMPARE(apps.iconForApp(QStringLiteral("nothing-here")), QString());
        // XDG_DATA_HOME wins over XDG_DATA_DIRS.
        QCOMPARE(AppRegistry::applicationDirs().first(), QStringLiteral(FIXTURES "/apps-extra/applications"));
    }
};

QTEST_GUILESS_MAIN(TestApps)
#include "test_apps.moc"
