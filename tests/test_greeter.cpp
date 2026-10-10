// SPDX-License-Identifier: GPL-3.0-or-later
// Greeter component logic with scripted stand-ins for SDDM objects: which
// power buttons appear, what they call, Hibernate on Shift, keyboard layout
// cycling. Full-screen flows are covered by tools/greeter-flow-test.py.
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QTest>

class TestGreeter : public QObject
{
    Q_OBJECT

    QQmlEngine *m_engine = nullptr;

    QObject *create(const QByteArray &qml)
    {
        QQmlComponent component(m_engine);
        component.setData("import QtQuick\nimport Meadow.Controls\nimport \"file://" MEADOW_SOURCE_DIR "/greeter/components\"\n" + qml,
                          QUrl(QStringLiteral("file:///test.qml")));
        QObject *obj = component.create();
        if (!obj)
            qWarning().noquote() << component.errorString();
        return obj;
    }

private Q_SLOTS:
    void init()
    {
        m_engine = new QQmlEngine(this);
        m_engine->addImportPath(QStringLiteral(MEADOW_BUILD_QML_DIR));
    }
    void cleanup()
    {
        delete m_engine;
        m_engine = nullptr;
    }

    void powerButtonsFollowCapabilities()
    {
        QScopedPointer<QObject> root(create(
            "Item {\n"
            "  property var calls: []\n"
            "  property alias dialog: dialog\n"
            "  TurnOffDialog { id: dialog; width: 800; height: 600\n"
            "    greeter: ({ canSuspend: false, canHibernate: true, canPowerOff: true, canReboot: false,\n"
            "                powerOff: () => calls.push('powerOff'), reboot: () => calls.push('reboot'),\n"
            "                suspend: () => calls.push('suspend'), hibernate: () => calls.push('hibernate') }) } }"));
        QVERIFY(root);
        auto *dialog = root->property("dialog").value<QQuickItem *>();
        QVERIFY(dialog);
        auto *standBy = dialog->findChild<QQuickItem *>(QStringLiteral("standByButton"));
        auto *turnOff = dialog->findChild<QQuickItem *>(QStringLiteral("turnOffButton"));
        auto *restart = dialog->findChild<QQuickItem *>(QStringLiteral("restartButton"));
        QVERIFY(standBy && turnOff && restart);
        QMetaObject::invokeMethod(dialog, "open");
        QVERIFY(dialog->isVisible());

        QVERIFY(turnOff->isVisible());
        QVERIFY(!restart->isVisible()); // canReboot is false
        // No suspend, but hibernate works: the yellow button becomes Hibernate.
        QVERIFY(standBy->isVisible());
        QVERIFY(dialog->property("hibernateMode").toBool());
        QCOMPARE(standBy->property("label").toString(), QStringLiteral("Hibernate"));

        QMetaObject::invokeMethod(dialog, "perform", Q_ARG(QVariant, QStringLiteral("powerOff")));
        QMetaObject::invokeMethod(standBy, "triggered");
        QCOMPARE(root->property("calls").toStringList(), (QStringList{QStringLiteral("powerOff"), QStringLiteral("hibernate")}));
        QVERIFY(!dialog->isVisible());
    }

    void shiftTurnsStandByIntoHibernate()
    {
        QScopedPointer<QObject> dialog(create(
            "TurnOffDialog { greeter: ({ canSuspend: true, canHibernate: true, canPowerOff: true, canReboot: true }) }"));
        QVERIFY(dialog);
        QVERIFY(!dialog->property("hibernateMode").toBool());
        dialog->setProperty("shiftHeld", true);
        QVERIFY(dialog->property("hibernateMode").toBool());
    }

    void noCapabilitiesHidesEverything()
    {
        QScopedPointer<QObject> root(create(
            "Item { property alias dialog: dialog\n"
            "  TurnOffDialog { id: dialog; width: 800; height: 600\n"
            "    greeter: ({ canSuspend: false, canHibernate: false, canPowerOff: false, canReboot: false }) } }"));
        QVERIFY(root);
        auto *dialog = root->property("dialog").value<QQuickItem *>();
        QMetaObject::invokeMethod(dialog, "open");
        QVERIFY(dialog->isVisible());
        for (const char *name : {"standByButton", "turnOffButton", "restartButton"})
            QVERIFY2(!dialog->findChild<QQuickItem *>(QLatin1String(name))->isVisible(), name);
        // debugPowerButtons (test mode) shows them all.
        dialog->setProperty("showAll", true);
        for (const char *name : {"standByButton", "turnOffButton", "restartButton"})
            QVERIFY2(dialog->findChild<QQuickItem *>(QLatin1String(name))->isVisible(), name);
    }

    void keyboardLayoutCycles()
    {
        QScopedPointer<QObject> root(create(
            "Item {\n"
            "  // Like SDDM's KeyboardModel: a QObject whose currentLayout notifies.\n"
            "  property QtObject kbd: QtObject { property bool enabled: true; property int currentLayout: 0\n"
            "      property var layouts: [{ shortName: 'us', longName: 'English (US)' }, { shortName: 'ar', longName: 'Arabic' }] }\n"
            "  property alias switcher: switcher\n"
            "  KeyboardLayoutSwitcher { id: switcher; keyboardModel: kbd } }"));
        QVERIFY(root);
        QObject *switcher = root->property("switcher").value<QObject *>();
        QCOMPARE(switcher->property("shortName").toString(), QStringLiteral("us"));
        QObject *kbd = root->property("kbd").value<QObject *>();
        QMetaObject::invokeMethod(switcher, "cycle");
        QCOMPARE(kbd->property("currentLayout").toInt(), 1);
        QCOMPARE(switcher->property("shortName").toString(), QStringLiteral("ar"));
        QMetaObject::invokeMethod(switcher, "cycle");
        QCOMPARE(kbd->property("currentLayout").toInt(), 0);
    }

    void avatarFallsBackForGenericFaces()
    {
        QScopedPointer<QObject> generic(create("Avatar { userName: 'bart'; source: 'file:///usr/share/sddm/faces/.face.icon' }"));
        QVERIFY(generic);
        QVERIFY(generic->property("sourceIsGeneric").toBool());
        QVERIFY(generic->property("fallbackUrl").toString().contains(QLatin1String("avatar-")));
        QScopedPointer<QObject> own(create("Avatar { userName: 'alice'; source: 'file:///home/alice/.face.icon' }"));
        QVERIFY(!own->property("sourceIsGeneric").toBool());
        // The fallback choice is stable for a given user name.
        QScopedPointer<QObject> again(create("Avatar { userName: 'bart' }"));
        QCOMPARE(again->property("fallbackUrl"), generic->property("fallbackUrl"));
    }
};

QTEST_MAIN(TestGreeter)
#include "test_greeter.moc"
