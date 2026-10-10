// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/Settings.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestSettings : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString write(const QString &name, const QByteArray &content)
    {
        const QString path = m_dir.filePath(name);
        QFile f(path);
        if (f.open(QIODevice::WriteOnly))
            f.write(content);
        return path;
    }

private Q_SLOTS:
    void builtInDefaultsWithoutFiles()
    {
        Settings s(m_dir.filePath(QStringLiteral("none-user.conf")), m_dir.filePath(QStringLiteral("none-defaults.conf")));
        QVERIFY(s.taskbarLocked());
        QVERIFY(!s.autoHide());
        QVERIFY(s.groupButtons());
        QVERIFY(s.showClock());
        QVERIFY(s.quickLaunch().isEmpty());
    }

    void packagedDefaultsThenUserOverrides()
    {
        const QString defaults = write(QStringLiteral("defaults.conf"),
            "[Taskbar]\nautoHide=true\nquickLaunch=meadow:show-desktop, firefox.desktop\n");
        const QString user = m_dir.filePath(QStringLiteral("user.conf"));
        Settings s(user, defaults);
        QVERIFY(s.autoHide());
        QCOMPARE(s.quickLaunch(), (QStringList{QStringLiteral("meadow:show-desktop"), QStringLiteral("firefox.desktop")}));

        QSignalSpy changed(&s, &Settings::changed);
        s.setAutoHide(false);
        QCOMPARE(changed.count(), 1);
        QVERIFY(!s.autoHide());
        s.setAutoHide(false); // no change, no signal
        QCOMPARE(changed.count(), 1);

        // Persisted in the user file only; the defaults file is untouched.
        Settings reopened(user, defaults);
        QVERIFY(!reopened.autoHide());
        QFile d(defaults);
        QVERIFY(d.open(QIODevice::ReadOnly));
        QVERIFY(d.readAll().contains("autoHide=true"));

        // Reset brings the packaged default back.
        reopened.reset(QStringLiteral("Taskbar/autoHide"));
        QVERIFY(reopened.autoHide());
    }

    void packagedFileParses()
    {
        Settings s(m_dir.filePath(QStringLiteral("u2.conf")), QStringLiteral(MEADOW_SOURCE_DIR "/shell/data/shell.conf"));
        QVERIFY(s.quickLaunch().contains(QStringLiteral("meadow:show-desktop")));
        QVERIFY(s.hideInactiveIcons());
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "test_settings.moc"
