// SPDX-License-Identifier: GPL-3.0-or-later
// LabwcConfig: the session rc.xml is the real one with the scheme's theme,
// key repeat, double-click time and libinput settings; it replaces the
// overlay's symlink (never the real file); without an overlay nothing is
// written.
#include "services/LabwcConfig.h"
#include "services/Settings.h"

#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

class TestLabwcConfig : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    static QDomElement path(const QDomDocument &doc, const QStringList &names)
    {
        QDomElement e = doc.documentElement();
        for (const QString &n : names)
            e = e.firstChildElement(n);
        return e;
    }

private Q_SLOTS:
    void themeNames()
    {
        QCOMPARE(LabwcConfig::themeForScheme(QStringLiteral("luna-blue")), QStringLiteral("Meadow-Luna"));
        QCOMPARE(LabwcConfig::themeForScheme(QStringLiteral("luna-olive")), QStringLiteral("Meadow-Luna-Olive"));
        QCOMPARE(LabwcConfig::themeForScheme(QStringLiteral("luna-silver")), QStringLiteral("Meadow-Luna-Silver"));
        QCOMPARE(LabwcConfig::themeForScheme(QStringLiteral("unknown")), QStringLiteral("Meadow-Luna"));
    }

    void rendersSettingsIntoRealRcXml()
    {
        Settings settings(m_dir.filePath(QStringLiteral("user.conf")), QStringLiteral(MEADOW_SOURCE_DIR "/shell/data/shell.conf"));
        settings.set(QStringLiteral("Appearance/scheme"), QStringLiteral("luna-silver"));
        settings.set(QStringLiteral("Keyboard/repeatRate"), 40);
        settings.set(QStringLiteral("Keyboard/repeatDelay"), 300);
        settings.set(QStringLiteral("Mouse/doubleClickMs"), 350);
        settings.set(QStringLiteral("Mouse/pointerSpeed"), 0.5);
        settings.set(QStringLiteral("Mouse/swapButtons"), true);
        LabwcConfig cfg(&settings, QString(), QStringLiteral(MEADOW_SOURCE_DIR "/session/labwc"), false);
        QDomDocument doc;
        QVERIFY(doc.setContent(cfg.renderRcXml()));
        QCOMPARE(path(doc, {QStringLiteral("theme"), QStringLiteral("name")}).text(), QStringLiteral("Meadow-Luna-Silver"));
        QCOMPARE(path(doc, {QStringLiteral("keyboard"), QStringLiteral("repeatRate")}).text(), QStringLiteral("40"));
        QCOMPARE(path(doc, {QStringLiteral("keyboard"), QStringLiteral("repeatDelay")}).text(), QStringLiteral("300"));
        QCOMPARE(path(doc, {QStringLiteral("mouse"), QStringLiteral("doubleClickTime")}).text(), QStringLiteral("350"));
        const QDomElement device = path(doc, {QStringLiteral("libinput"), QStringLiteral("device")});
        QCOMPARE(device.attribute(QStringLiteral("category")), QStringLiteral("default"));
        QCOMPARE(device.firstChildElement(QStringLiteral("pointerSpeed")).text(), QStringLiteral("0.50"));
        QCOMPARE(device.firstChildElement(QStringLiteral("leftHanded")).text(), QStringLiteral("yes"));
        // Everything else (keybinds) is still there, exactly once.
        QCOMPARE(doc.elementsByTagName(QStringLiteral("keybind")).size(),
                 [] {
                     QDomDocument orig;
                     QFile f(QStringLiteral(MEADOW_SOURCE_DIR "/session/labwc/rc.xml"));
                     f.open(QIODevice::ReadOnly);
                     orig.setContent(&f);
                     return orig.elementsByTagName(QStringLiteral("keybind")).size();
                 }());
        QCOMPARE(doc.elementsByTagName(QStringLiteral("theme")).size(), 1);
    }

    void outOfRangeValuesAreClamped()
    {
        Settings settings(m_dir.filePath(QStringLiteral("clamp.conf")), QStringLiteral(MEADOW_SOURCE_DIR "/shell/data/shell.conf"));
        settings.set(QStringLiteral("Keyboard/repeatRate"), 100000);
        settings.set(QStringLiteral("Mouse/pointerSpeed"), -7);
        LabwcConfig cfg(&settings, QString(), QStringLiteral(MEADOW_SOURCE_DIR "/session/labwc"), false);
        QDomDocument doc;
        QVERIFY(doc.setContent(cfg.renderRcXml()));
        QCOMPARE(path(doc, {QStringLiteral("keyboard"), QStringLiteral("repeatRate")}).text(), QStringLiteral("100"));
        QCOMPARE(path(doc, {QStringLiteral("libinput"), QStringLiteral("device"), QStringLiteral("pointerSpeed")}).text(),
                 QStringLiteral("-1.00"));
    }

    void replacesTheOverlaySymlinkNotTheRealFile()
    {
        // The "real" config (as the user's ~/.config/meadow/labwc would be).
        const QString source = m_dir.filePath(QStringLiteral("source"));
        const QString overlay = m_dir.filePath(QStringLiteral("overlay"));
        QDir().mkpath(source);
        QDir().mkpath(overlay);
        QVERIFY(QFile::copy(QStringLiteral(MEADOW_SOURCE_DIR "/session/labwc/rc.xml"), source + QStringLiteral("/rc.xml")));
        QFile::setPermissions(source + QStringLiteral("/rc.xml"), QFile::ReadOwner | QFile::WriteOwner);
        QFile realFile(source + QStringLiteral("/rc.xml"));
        QVERIFY(realFile.open(QIODevice::ReadOnly));
        const QByteArray original = realFile.readAll();
        realFile.close();
        QVERIFY(QFile::link(source + QStringLiteral("/rc.xml"), overlay + QStringLiteral("/rc.xml")));

        Settings settings(m_dir.filePath(QStringLiteral("overlay.conf")), QStringLiteral(MEADOW_SOURCE_DIR "/shell/data/shell.conf"));
        settings.set(QStringLiteral("Appearance/scheme"), QStringLiteral("luna-olive"));
        LabwcConfig cfg(&settings, overlay, source, false);
        QVERIFY(cfg.apply());

        const QFileInfo written(overlay + QStringLiteral("/rc.xml"));
        QVERIFY(!written.isSymLink());
        QFile out(written.filePath());
        QVERIFY(out.open(QIODevice::ReadOnly));
        QVERIFY(out.readAll().contains("Meadow-Luna-Olive"));
        QVERIFY(realFile.open(QIODevice::ReadOnly));
        QCOMPARE(realFile.readAll(), original);
        QVERIFY(!QFileInfo::exists(overlay + QStringLiteral("/.rc.xml.meadow-tmp")));
    }

    void noOverlayWritesNothing()
    {
        Settings settings(m_dir.filePath(QStringLiteral("none.conf")), QStringLiteral(MEADOW_SOURCE_DIR "/shell/data/shell.conf"));
        LabwcConfig cfg(&settings, QString(), QStringLiteral(MEADOW_SOURCE_DIR "/session/labwc"), false);
        QVERIFY(!cfg.hasOverlay());
        QVERIFY(!cfg.apply());
    }
};

QTEST_GUILESS_MAIN(TestLabwcConfig)
#include "test_labwcconfig.moc"
