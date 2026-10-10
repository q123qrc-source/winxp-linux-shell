// SPDX-License-Identifier: GPL-3.0-or-later
#include "ThemeLoader.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

static bool touch(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::WriteOnly);
}

class TestThemeLoader : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY2(qEnvironmentVariableIsSet("MEADOW_THEME_DIR"), "ctest must set MEADOW_THEME_DIR");
    }

    void loadsEveryScheme_data()
    {
        QTest::addColumn<QString>("scheme");
        QTest::newRow("blue") << QStringLiteral("luna-blue");
        QTest::newRow("olive") << QStringLiteral("luna-olive");
        QTest::newRow("silver") << QStringLiteral("luna-silver");
    }

    void loadsEveryScheme()
    {
        QFETCH(QString, scheme);
        ThemeLoader loader(nullptr);
        QSignalSpy changed(&loader, &ThemeLoader::themeChanged);
        QVERIFY2(loader.loadScheme(scheme), qPrintable(loader.errorString()));
        QCOMPARE(loader.schemeId(), scheme);
        QCOMPARE(changed.count(), 1);
        QVERIFY(loader.token(QStringLiteral("taskbar.height")).toInt() > 0);
    }

    void tokenPaths()
    {
        ThemeLoader loader(nullptr);
        QVERIFY(loader.loadScheme(QStringLiteral("luna-blue")));
        QCOMPARE(loader.token(QStringLiteral("taskbar.height")).toInt(), 30);
        QCOMPARE(loader.token(QStringLiteral("startButton.label")).toString(), QStringLiteral("start"));
        QVERIFY(!loader.token(QStringLiteral("taskbar.doesNotExist")).isValid());
        QVERIFY(!loader.token(QStringLiteral("nope.nope.nope")).isValid());
    }

    void unknownSchemeFails()
    {
        ThemeLoader loader(nullptr);
        QVERIFY(!loader.loadScheme(QStringLiteral("no-such-scheme")));
        QVERIFY(!loader.errorString().isEmpty());
        QVERIFY(loader.tokens().isEmpty());
    }

    void malformedJsonFails()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("theme.json"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ \"meta\": ");
        f.close();
        ThemeLoader loader(nullptr);
        QVERIFY(!loader.loadFile(path));
        QVERIFY(!loader.errorString().isEmpty());
    }

    void overrideAssetWins()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        qputenv("XDG_DATA_HOME", home.path().toUtf8());
        QDir().mkpath(home.filePath(QStringLiteral("meadow/assets-override")));
        QTemporaryDir bundled;
        QVERIFY(touch(bundled.filePath(QStringLiteral("wall.svg"))));

        ThemeLoader loader(nullptr);
        QCOMPARE(loader.resolveAsset(QStringLiteral("wall.svg"), bundled.path()),
                 QUrl::fromLocalFile(bundled.filePath(QStringLiteral("wall.svg"))).toString());

        QVERIFY(touch(home.filePath(QStringLiteral("meadow/assets-override/wall.svg"))));
        QCOMPARE(loader.resolveAsset(QStringLiteral("wall.svg"), bundled.path()),
                 QUrl::fromLocalFile(home.filePath(QStringLiteral("meadow/assets-override/wall.svg"))).toString());

        QVERIFY(loader.resolveAsset(QStringLiteral("../etc/passwd"), bundled.path()).isEmpty());
        QVERIFY(loader.resolveAsset(QStringLiteral("/etc/passwd"), bundled.path()).isEmpty());
        QVERIFY(loader.resolveAsset(QStringLiteral("missing.svg"), bundled.path()).isEmpty());
        qunsetenv("XDG_DATA_HOME");
    }
};

QTEST_GUILESS_MAIN(TestThemeLoader)
#include "test_themeloader.moc"
