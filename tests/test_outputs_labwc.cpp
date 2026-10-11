// SPDX-License-Identifier: GPL-3.0-or-later
// OutputManager and IdleMonitor against a real compositor (headless labwc,
// run by tools/with-headless-labwc.sh):
//   - the headless output is listed; a new (custom) mode and scale are
//     tested and applied, then reverted;
//   - an idle timeout fires without input and resumes on a key press.
#include "wayland/IdleMonitor.h"
#include "wayland/OutputManager.h"

#include <QGuiApplication>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class TestOutputsLabwc : public QObject
{
    Q_OBJECT

    static QVariantMap first(const OutputManager &m)
    {
        return m.outputs().isEmpty() ? QVariantMap() : m.outputs().first().toMap();
    }

private Q_SLOTS:
    void initTestCase()
    {
        if (QGuiApplication::platformName() != QLatin1String("wayland"))
            QSKIP("needs a Wayland compositor (run through tools/with-headless-labwc.sh)");
    }

    void changeAndRevertMode()
    {
        OutputManager m(false);
        QTRY_VERIFY_WITH_TIMEOUT(m.isAvailable(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!m.outputs().isEmpty(), 5000);
        const QVariantMap before = first(m);
        const QString name = before.value(QStringLiteral("name")).toString();
        QVERIFY(!name.isEmpty());
        QVERIFY(before.value(QStringLiteral("width")).toInt() > 0);

        QSignalSpy applied(&m, &OutputManager::applied);
        m.apply(name, 800, 600, 0, 1.0);
        QTRY_COMPARE_WITH_TIMEOUT(applied.size(), 1, 5000);
        QVERIFY2(applied.at(0).at(0).toBool(), qPrintable(applied.at(0).at(1).toString()));
        QTRY_COMPARE_WITH_TIMEOUT(first(m).value(QStringLiteral("width")).toInt(), 800, 5000);
        QCOMPARE(first(m).value(QStringLiteral("height")).toInt(), 600);
        QVERIFY(m.canRevert());

        // Scale only.
        m.apply(name, 800, 600, 0, 2.0);
        QTRY_COMPARE_WITH_TIMEOUT(applied.size(), 2, 5000);
        QVERIFY(applied.at(1).at(0).toBool());
        QTRY_COMPARE_WITH_TIMEOUT(first(m).value(QStringLiteral("scale")).toDouble(), 2.0, 5000);

        // Revert restores the configuration before the last apply.
        m.revert();
        QTRY_COMPARE_WITH_TIMEOUT(applied.size(), 3, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(first(m).value(QStringLiteral("scale")).toDouble(), 1.0, 5000);
        QCOMPARE(first(m).value(QStringLiteral("width")).toInt(), 800);
        QVERIFY(!m.canRevert());

        // Back to where the compositor started.
        m.apply(name, before.value(QStringLiteral("width")).toInt(), before.value(QStringLiteral("height")).toInt(), 0, 1.0);
        QTRY_COMPARE_WITH_TIMEOUT(first(m).value(QStringLiteral("width")).toInt(), before.value(QStringLiteral("width")).toInt(), 5000);
    }

    void idleNotification()
    {
        IdleMonitor idle(false);
        QTRY_VERIFY_WITH_TIMEOUT(idle.isAvailable(), 5000);
        QSignalSpy changed(&idle, &IdleMonitor::idleChanged);
        idle.setTimeoutMs(500);
        QTRY_VERIFY_WITH_TIMEOUT(idle.isIdle(), 5000);
        const QString wtype = QStandardPaths::findExecutable(QStringLiteral("wtype"));
        if (wtype.isEmpty())
            QSKIP("wtype not installed: cannot check resuming");
        QProcess::execute(wtype, {QStringLiteral("-k"), QStringLiteral("Shift_L")});
        QTRY_VERIFY_WITH_TIMEOUT(!idle.isIdle(), 5000);
        // And idle again after another quiet period.
        QTRY_VERIFY_WITH_TIMEOUT(idle.isIdle(), 5000);
        QVERIFY(changed.size() >= 3);
        // Timeout 0 turns notifications off.
        idle.setTimeoutMs(0);
        QVERIFY(!idle.isIdle());
    }
};

QTEST_MAIN(TestOutputsLabwc)
#include "test_outputs_labwc.moc"
