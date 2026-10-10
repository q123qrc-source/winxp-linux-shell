// SPDX-License-Identifier: GPL-3.0-or-later
#include "ThemeLoader.h"

#include <QDir>
#include <QFontDatabase>
#include <QJSEngine>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

static bool touch(const QString &path, const QByteArray &content = {})
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    return f.write(content) == content.size();
}

static QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

class TestThemeLoader : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY2(qEnvironmentVariableIsSet("MEADOW_THEME_DIR"), "ctest must set MEADOW_THEME_DIR");
        qunsetenv("MEADOW_SCALE");
        qunsetenv("MEADOW_REDUCED_MOTION");
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

    void availableSchemes()
    {
        const QStringList schemes = ThemeLoader::availableSchemes();
        QVERIFY(schemes.contains(QStringLiteral("luna-blue")));
        QVERIFY(schemes.contains(QStringLiteral("luna-olive")));
        QVERIFY(schemes.contains(QStringLiteral("luna-silver")));
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

    void rejectsBadSchemes()
    {
        ThemeLoader loader(nullptr);
        QVERIFY(!loader.loadScheme(QStringLiteral("no-such-scheme")));
        QVERIFY(!loader.errorString().isEmpty());
        QVERIFY(loader.tokenMap().isEmpty());
        QVERIFY(!loader.loadScheme(QStringLiteral("../luna-blue")));
        QVERIFY(!loader.loadScheme(QStringLiteral(".hidden")));
        QVERIFY(!loader.loadScheme(QString()));
    }

    void malformedJsonKeepsPreviousTheme()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("theme.json"));
        QVERIFY(touch(path, "{ \"meta\": "));
        ThemeLoader loader(nullptr);
        QVERIFY(loader.loadScheme(QStringLiteral("luna-blue")));
        QVERIFY(!loader.loadFile(path));
        QVERIFY(!loader.errorString().isEmpty());
        QCOMPARE(loader.schemeId(), QStringLiteral("luna-blue"));
    }

    void scaleAndPx()
    {
        ThemeLoader loader(nullptr);
        QVERIFY(loader.loadScheme(QStringLiteral("luna-blue")));
        QCOMPARE(loader.scale(), 1.0);
        QCOMPARE(loader.px(30), 30);

        QSignalSpy changed(&loader, &ThemeLoader::themeChanged);
        loader.setScale(1.25);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(loader.px(30), 38); // 37.5 rounds half away from zero
        QCOMPARE(loader.px(1), 1);
        loader.setScale(2.0);
        QCOMPARE(loader.px(30), 60);
        loader.setScale(100);
        QCOMPARE(loader.scale(), 4.0);
        loader.setScale(0.01);
        QCOMPARE(loader.scale(), 0.5);

        // A user-chosen scale survives a scheme switch.
        loader.setScale(1.5);
        QVERIFY(loader.loadScheme(QStringLiteral("luna-olive")));
        QCOMPARE(loader.scale(), 1.5);
    }

    void scaleFromEnvironment()
    {
        qputenv("MEADOW_SCALE", "2");
        ThemeLoader loader(nullptr);
        QVERIFY(loader.loadScheme(QStringLiteral("luna-blue")));
        QCOMPARE(loader.scale(), 2.0);
        qunsetenv("MEADOW_SCALE");
    }

    void fontFallback()
    {
        ThemeLoader loader(nullptr);
        QVERIFY(loader.loadScheme(QStringLiteral("luna-blue")));
        const QStringList wanted = loader.token(QStringLiteral("fonts.ui.families")).toStringList();
        const QString family = loader.fontFamily(QStringLiteral("ui"));
        QVERIFY(wanted.contains(family));
        // The pick is the first installed family of the list, else the generic last entry.
        const QStringList installed = QFontDatabase::families();
        QString expected = wanted.constLast();
        for (const QString &f : wanted) {
            if (installed.contains(f, Qt::CaseInsensitive)) {
                expected = f;
                break;
            }
        }
        QCOMPARE(family, expected);

        const QFont start = loader.font(QStringLiteral("start"));
        QCOMPARE(start.pixelSize(), 18);
        QVERIFY(start.bold());
        QVERIFY(start.italic());
        loader.setScale(2.0);
        QCOMPARE(loader.font(QStringLiteral("start")).pixelSize(), 36);
        QVERIFY(loader.fonts().contains(QStringLiteral("title")));
        QCOMPARE(loader.fonts().value(QStringLiteral("ui")).value<QFont>().pixelSize(), 22);
    }

    void durationsAndReducedMotion()
    {
        ThemeLoader loader(nullptr);
        QVERIFY(loader.loadScheme(QStringLiteral("luna-blue")));
        QVERIFY(!loader.reducedMotion());
        QCOMPARE(loader.duration(QStringLiteral("fast")), 100);
        QCOMPARE(loader.duration(QStringLiteral("startMenuOpenMs")), 150);
        loader.setReducedMotion(true);
        QCOMPARE(loader.duration(QStringLiteral("fast")), 0);
        loader.setReducedMotion(false);

        qputenv("MEADOW_REDUCED_MOTION", "1");
        ThemeLoader fromEnv(nullptr);
        QVERIFY(fromEnv.loadScheme(QStringLiteral("luna-blue")));
        QVERIFY(fromEnv.reducedMotion());
        QCOMPARE(fromEnv.duration(QStringLiteral("slow")), 0);
        qunsetenv("MEADOW_REDUCED_MOTION");
    }

    void liveReload()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("theme.json"));
        const QByteArray original = readFile(QString::fromLocal8Bit(qgetenv("MEADOW_THEME_DIR")) + QLatin1String("/luna-blue/theme.json"));
        QVERIFY(!original.isEmpty());
        QVERIFY(touch(path, original));

        ThemeLoader loader(nullptr);
        QVERIFY(loader.loadFile(path));
        loader.setLiveReload(true);
        QVERIFY(loader.liveReload());
        QSignalSpy changed(&loader, &ThemeLoader::themeChanged);

        QByteArray edited = original;
        edited.replace("\"height\": 30,", "\"height\": 44,");
        QVERIFY(edited != original);
        // Replace the file the way editors do: write elsewhere, rename over it.
        const QString tmp = dir.filePath(QStringLiteral("theme.json.new"));
        QVERIFY(touch(tmp, edited));
        QVERIFY(QFile::remove(path));
        QVERIFY(QFile::rename(tmp, path));

        QTRY_VERIFY_WITH_TIMEOUT(changed.count() >= 1, 5000);
        QCOMPARE(loader.token(QStringLiteral("taskbar.height")).toInt(), 44);

        // A second edit is still seen after the rename dropped the original watch.
        edited.replace("\"height\": 44,", "\"height\": 50,");
        QVERIFY(touch(path, edited));
        QTRY_COMPARE_WITH_TIMEOUT(loader.token(QStringLiteral("taskbar.height")).toInt(), 50, 5000);

        // A broken edit keeps the last good theme.
        QVERIFY(touch(path, "{ broken"));
        QTest::qWait(400);
        QCOMPARE(loader.token(QStringLiteral("taskbar.height")).toInt(), 50);
    }

    void qmlSingletonSharesInstance()
    {
        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(MEADOW_BUILD_QML_DIR));
        QQmlComponent component(&engine);
        component.setData("import QtQuick\nimport Meadow.Controls\n"
                          "QtObject { property int h: Theme.tokens.taskbar.height\n"
                          "           property string id: Theme.schemeId\n"
                          "           property int scaled: Theme.px(Theme.tokens.taskbar.height) }",
                          QUrl());
        QScopedPointer<QObject> obj(component.create());
        QVERIFY2(obj, qPrintable(component.errorString()));
        QCOMPARE(obj->property("id").toString(), ThemeLoader::instance()->schemeId());
        QCOMPARE(obj->property("h").toInt(), 30);

        // Bindings follow scale changes on the shared instance.
        ThemeLoader::instance()->setScale(2.0);
        QCOMPARE(obj->property("scaled").toInt(), 60);
        ThemeLoader::instance()->setScale(1.0);
        QCOMPARE(obj->property("scaled").toInt(), 30);
    }

    void everyEngineGetsItsOwnTheme()
    {
        // SDDM creates one QML engine per screen; tokens are engine-bound JS.
        const QByteArray qml = "import QtQuick\nimport Meadow.Controls\n"
                               "QtObject { property int h: Theme.tokens.taskbar.height }";
        QQmlEngine first;
        first.addImportPath(QStringLiteral(MEADOW_BUILD_QML_DIR));
        QQmlEngine second;
        second.addImportPath(QStringLiteral(MEADOW_BUILD_QML_DIR));
        QQmlComponent a(&first), b(&second);
        a.setData(qml, QUrl());
        b.setData(qml, QUrl());
        QScopedPointer<QObject> oa(a.create()), ob(b.create());
        QVERIFY2(oa && ob, qPrintable(a.errorString() + b.errorString()));
        QCOMPARE(oa->property("h").toInt(), 30);
        QCOMPARE(ob->property("h").toInt(), 30);
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

QTEST_MAIN(TestThemeLoader)
#include "test_themeloader.moc"
