// SPDX-License-Identifier: GPL-3.0-or-later
// Non-visual checks for the common library: sound event resolution and muting,
// gradient stop parsing, and that every Meadow.Controls type instantiates
// without QML errors or warnings.
#include "GradientBox.h"
#include "SoundPlayer.h"
#include "ThemeLoader.h"

#include <QDir>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QTest>

class TestControls : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(ThemeLoader::instance()->loadScheme(QStringLiteral("luna-blue")));
    }

    void soundResolution()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        qputenv("MEADOW_SOUND_DIR", dir.path().toUtf8());
        qputenv("XDG_DATA_HOME", dir.filePath(QStringLiteral("home")).toUtf8());
        qunsetenv("MEADOW_MUTE");

        SoundPlayer player;
        QVERIFY(player.resolve(QStringLiteral("logon")).isEmpty()); // no file yet
        QVERIFY(player.resolve(QStringLiteral("no-such-event")).isEmpty());

        QFile bundled(dir.filePath(QStringLiteral("desktop-login.oga")));
        QVERIFY(bundled.open(QIODevice::WriteOnly));
        bundled.close();
        QCOMPARE(player.resolve(QStringLiteral("logon")), QUrl::fromLocalFile(bundled.fileName()).toString());

        // A user override with the same name wins.
        QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("home/meadow/assets-override"))));
        QFile override(dir.filePath(QStringLiteral("home/meadow/assets-override/desktop-login.oga")));
        QVERIFY(override.open(QIODevice::WriteOnly));
        override.close();
        QCOMPARE(player.resolve(QStringLiteral("logon")), QUrl::fromLocalFile(override.fileName()).toString());

        player.setMuted(true);
        QVERIFY(!player.play(QStringLiteral("logon")));
        player.setMuted(false);

        // "[Sounds] enabled=false" mutes every player, live.
        SoundPlayer::setAllMuted(true);
        QVERIFY(!player.play(QStringLiteral("logon")));
        QVERIFY(!player.playFile(override.fileName()));
        SoundPlayer::setAllMuted(false);

        // playFile accepts local files only.
        QVERIFY(!player.playFile(QStringLiteral("https://example.org/a.oga")));
        QVERIFY(!player.playFile(dir.filePath(QStringLiteral("missing.oga"))));

        qunsetenv("MEADOW_SOUND_DIR");
        qunsetenv("XDG_DATA_HOME");
    }

    void muteFromEnvironment()
    {
        qputenv("MEADOW_MUTE", "1");
        SoundPlayer player;
        QVERIFY(player.muted());
        qunsetenv("MEADOW_MUTE");
        SoundPlayer loud;
        QVERIFY(!loud.muted());
        loud.setVolume(3);
        QCOMPARE(loud.volume(), 1.0);
    }

    void gradientStops()
    {
        QVariantList stops;
        stops << QVariantMap{{QStringLiteral("pos"), 0.0}, {QStringLiteral("color"), QStringLiteral("#FF0000")}}
              << QVariantMap{{QStringLiteral("pos"), 1.5}, {QStringLiteral("color"), QStringLiteral("#8000FF00")}}
              << QVariantMap{{QStringLiteral("pos"), 0.5}, {QStringLiteral("color"), QStringLiteral("not-a-color")}};
        const QGradientStops parsed = GradientBox::gradientStops(stops);
        QCOMPARE(parsed.size(), 2);
        QCOMPARE(parsed[0].second, QColor(Qt::red));
        QCOMPARE(parsed[1].first, 1.0); // clamped
        QCOMPARE(parsed[1].second.alpha(), 0x80);
    }

    void controlsInstantiate_data()
    {
        QTest::addColumn<QByteArray>("qml");
        QTest::newRow("LunaButton") << QByteArray("LunaButton { text: 'OK'; isDefault: true }");
        QTest::newRow("LunaTextField") << QByteArray("LunaTextField { placeholderText: 'x' }");
        QTest::newRow("LunaCheckBox") << QByteArray("LunaCheckBox { text: 'x'; checked: true }");
        QTest::newRow("LunaComboBox") << QByteArray("LunaComboBox { model: ['a', 'b'] }");
        QTest::newRow("LunaScrollBar") << QByteArray("LunaScrollBar { size: 0.5 }");
        QTest::newRow("LunaTooltip") << QByteArray("Item { LunaTooltip { text: 'tip' } }");
        QTest::newRow("BalloonTip") << QByteArray("BalloonTip { title: 't'; text: 'x' }");
        QTest::newRow("LunaWindowFrame") << QByteArray("LunaWindowFrame { title: 'w'; width: 200; height: 100 }");
        QTest::newRow("LunaDialog") << QByteArray("LunaDialog { title: 'd'; Item { width: 10; height: 10 } }");
        QTest::newRow("GlossyGradient") << QByteArray("GlossyGradient { glossColor: '#40FFFFFF' }");
        QTest::newRow("ShadowText") << QByteArray("ShadowText { text: 'start'; shadowColor: 'black' }");
    }

    void controlsInstantiate()
    {
        QFETCH(QByteArray, qml);
        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(MEADOW_BUILD_QML_DIR));
        QList<QQmlError> warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&warnings](const QList<QQmlError> &w) { warnings << w; });
        QQmlComponent component(&engine);
        component.setData("import QtQuick\nimport Meadow.Controls\n" + qml, QUrl(QStringLiteral("file:///test.qml")));
        QScopedPointer<QObject> obj(component.create());
        QVERIFY2(obj, qPrintable(component.errorString()));
        QCoreApplication::processEvents();
        QString text;
        for (const QQmlError &e : std::as_const(warnings))
            text += e.toString() + QLatin1Char('\n');
        QVERIFY2(warnings.isEmpty(), qPrintable(text));
    }

    void dialogKeys()
    {
        QQmlEngine engine;
        engine.addImportPath(QStringLiteral(MEADOW_BUILD_QML_DIR));
        QQmlComponent component(&engine);
        component.setData("import QtQuick\nimport Meadow.Controls\n"
                          "LunaDialog { property var log: []\n"
                          "  buttons: ['OK', 'Cancel']\n"
                          "  onButtonClicked: (i) => log.push(i)\n"
                          "  onAccepted: log.push('accepted'); onRejected: log.push('rejected') }",
                          QUrl(QStringLiteral("file:///dialog.qml")));
        QScopedPointer<QObject> dialog(component.create());
        QVERIFY2(dialog, qPrintable(component.errorString()));
        QMetaObject::invokeMethod(dialog.data(), "accept");
        QMetaObject::invokeMethod(dialog.data(), "reject");
        const QVariantList log = dialog->property("log").toList();
        QCOMPARE(log, (QVariantList{0, QStringLiteral("accepted"), 1, QStringLiteral("rejected")}));
    }
};

QTEST_MAIN(TestControls)
#include "test_controls.moc"
