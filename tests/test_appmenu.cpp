// SPDX-License-Identifier: GPL-3.0-or-later
// Start menu data: XDG menu evaluation (Meadow's shipped menu and a fixture
// exercising the spec's rules), filtering (NoDisplay, OnlyShowIn, TryExec),
// localized names, live reload, newly installed programs, pinned and
// frequent programs, default browser/mail resolution, places and recent
// documents. Launches are recorded through MEADOW_LAUNCH_LOG, never run.
#include "models/AppMenuModel.h"
#include "models/AppRegistry.h"
#include "models/MenuParser.h"
#include "models/PlacesModel.h"
#include "models/RecentAppsModel.h"
#include "models/UsageStore.h"
#include "services/Settings.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#define FIXTURE MEADOW_SOURCE_DIR "/tests/fixtures/menu"

namespace {
QStringList labels(const QVariantList &entries)
{
    QStringList out;
    for (const QVariant &v : entries)
        out << v.toMap().value(QStringLiteral("label")).toString();
    return out;
}
QVariantMap child(const QVariantList &entries, const QString &label)
{
    for (const QVariant &v : entries) {
        if (v.toMap().value(QStringLiteral("label")).toString() == label)
            return v.toMap();
    }
    return {};
}
QVariantList submenu(const QVariantList &entries, const QString &label)
{
    return child(entries, label).value(QStringLiteral("children")).toList();
}
} // namespace

class TestAppMenu : public QObject
{
    Q_OBJECT

    QTemporaryDir m_tmp;

    QString launchLog() const { return m_tmp.filePath(QStringLiteral("launches.log")); }
    QStringList launches() const
    {
        QFile f(launchLog());
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }
    std::unique_ptr<Settings> settings(const QString &extra = {})
    {
        static int n = 0;
        const QString defaults = m_tmp.filePath(QStringLiteral("defaults-%1.conf").arg(++n));
        QFile::copy(QStringLiteral(MEADOW_SOURCE_DIR "/shell/data/shell.conf"), defaults);
        QFile::setPermissions(defaults, QFile::ReadOwner | QFile::WriteOwner);
        if (!extra.isEmpty()) {
            QFile f(defaults);
            if (f.open(QIODevice::Append))
            f.write(extra.toUtf8());
        }
        return std::make_unique<Settings>(m_tmp.filePath(QStringLiteral("user-%1.conf").arg(n)), defaults);
    }

private Q_SLOTS:
    void initTestCase()
    {
        qputenv("XDG_DATA_HOME", m_tmp.filePath(QStringLiteral("home-data")).toUtf8());
        qputenv("XDG_DATA_DIRS", FIXTURE "/data");
        // Qt built without its own MIME database (Arch) reads the system one
        // from the data directories, which the fixtures replace.
        QDir().mkpath(m_tmp.filePath(QStringLiteral("home-data")));
        QFile::link(QStringLiteral("/usr/share/mime"), m_tmp.filePath(QStringLiteral("home-data/mime")));
        qputenv("XDG_CONFIG_HOME", m_tmp.filePath(QStringLiteral("home-config")).toUtf8());
        qputenv("XDG_CONFIG_DIRS", FIXTURE "/config");
        qputenv("XDG_CURRENT_DESKTOP", "Meadow");
        qputenv("MEADOW_MENU_DIR", MEADOW_SOURCE_DIR "/session/menus");
        qputenv("MEADOW_LAUNCH_LOG", launchLog().toUtf8());
        QLocale::setDefault(QLocale(QStringLiteral("en_US")));
    }

    void shippedMenu()
    {
        AppRegistry apps;
        UsageStore usage(m_tmp.filePath(QStringLiteral("usage-shipped.json")));
        AppMenuModel menu(&apps, &usage);
        QVERIFY(menu.effectiveMenuFile().endsWith(QLatin1String("session/menus/meadow-applications.menu")));
        const QVariantList tree = menu.tree();
        // Folders first (sorted), then unallocated programs.
        QCOMPARE(labels(tree), (QStringList{QStringLiteral("Accessories"), QStringLiteral("Games"), QStringLiteral("Internet"),
                                            QStringLiteral("Office"), QStringLiteral("Programming"), QStringLiteral("Misc Tool")}));
        const QVariantList accessories = submenu(tree, QStringLiteral("Accessories"));
        QCOMPARE(labels(accessories), (QStringList{QStringLiteral("Accessibility"), QStringLiteral("System Tools"),
                                                   QStringLiteral("Calculator"), QStringLiteral("Text Editor")}));
        QCOMPARE(labels(submenu(accessories, QStringLiteral("System Tools"))), QStringList{QStringLiteral("Settings")});
        QCOMPARE(child(tree, QStringLiteral("Games")).value(QStringLiteral("iconName")).toString(), QStringLiteral("applications-games"));
        // Subdirectory ids use '-' (vendor/tool.desktop -> vendor-tool.desktop).
        QCOMPARE(child(submenu(tree, QStringLiteral("Programming")), QStringLiteral("Vendor Tool")).value(QStringLiteral("desktopId")).toString(),
                 QStringLiteral("vendor-tool.desktop"));
        // NoDisplay, OnlyShowIn=KDE and a missing TryExec never appear.
        QStringList flat;
        std::function<void(const QVariantList &)> walk = [&](const QVariantList &l) {
            for (const QVariant &v : l) {
                flat << v.toMap().value(QStringLiteral("label")).toString();
                walk(v.toMap().value(QStringLiteral("children")).toList());
            }
        };
        walk(tree);
        QVERIFY(!flat.contains(QStringLiteral("Hidden Helper")));
        QVERIFY(!flat.contains(QStringLiteral("KDE Only")));
        QVERIFY(!flat.contains(QStringLiteral("Not Installed")));
    }

    void specRules()
    {
        AppRegistry apps;
        UsageStore usage(m_tmp.filePath(QStringLiteral("usage-rules.json")));
        AppMenuModel menu(&apps, &usage);
        menu.setMenuFile(QStringLiteral(FIXTURE "/config/menus/custom.menu"));
        const QVariantList tree = menu.tree();
        // "Tools" (two same-name menus merged, .directory name used): Utility
        // minus calc (And/Not/Filename), minus TextEditor (Exclude), plus
        // vendor-tool from the second "Tools"; the a11y app is Utility too.
        QCOMPARE(labels(submenu(tree, QStringLiteral("Handy Tools"))),
                 (QStringList{QStringLiteral("Magnifier"), QStringLiteral("Vendor Tool")}));
        // MergeFile brought "Net"; "Gone" is Deleted, so Chess is unallocated.
        QCOMPARE(labels(submenu(tree, QStringLiteral("Net"))), (QStringList{QStringLiteral("Mail"), QStringLiteral("Web Browser")}));
        QVERIFY(labels(tree).contains(QStringLiteral("Chess")));
        QVERIFY(labels(tree).contains(QStringLiteral("Calculator")));
        QVERIFY(!labels(tree).contains(QStringLiteral("Gone")));
    }

    void brokenMenuFallsBackToFlatList()
    {
        const QString broken = m_tmp.filePath(QStringLiteral("broken.menu"));
        QFile f(broken);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("<Menu><Name>x</Name>");
        f.close();
        AppRegistry apps;
        UsageStore usage(m_tmp.filePath(QStringLiteral("usage-broken.json")));
        AppMenuModel menu(&apps, &usage);
        menu.setMenuFile(broken);
        QVERIFY(menu.effectiveMenuFile().isEmpty());
        QVERIFY(labels(menu.tree()).contains(QStringLiteral("Calculator")));
    }

    void localizedNames()
    {
        QLocale::setDefault(QLocale(QStringLiteral("de_DE")));
        AppRegistry apps;
        UsageStore usage(m_tmp.filePath(QStringLiteral("usage-de.json")));
        AppMenuModel menu(&apps, &usage);
        menu.setMenuFile(QStringLiteral(FIXTURE "/config/menus/custom.menu"));
        QVERIFY(labels(menu.tree()).contains(QStringLiteral("Werkzeuge")));
        QVERIFY(labels(menu.tree()).contains(QStringLiteral("Rechner")));
        QLocale::setDefault(QLocale(QStringLiteral("en_US")));
    }

    void liveReloadAndNewPrograms()
    {
        // A writable app dir: XDG_DATA_HOME/applications.
        const QString appDir = m_tmp.filePath(QStringLiteral("home-data/applications"));
        QDir().mkpath(appDir);
        AppRegistry apps;
        UsageStore usage(m_tmp.filePath(QStringLiteral("usage-live.json")));
        AppMenuModel menu(&apps, &usage);
        QSignalSpy spy(&menu, &AppMenuModel::changed);
        // Nothing is new on the first run.
        QVERIFY(!child(submenu(menu.tree(), QStringLiteral("Games")), QStringLiteral("Chess")).value(QStringLiteral("isNew")).toBool());

        QFile f(appDir + QLatin1String("/solitaire.desktop"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[Desktop Entry]\nType=Application\nName=Solitaire\nExec=sol\nCategories=Game;\n");
        f.close();
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() > 0, 5000);
        const QVariantMap games = child(menu.tree(), QStringLiteral("Games"));
        QVERIFY(games.value(QStringLiteral("isNew")).toBool()); // the folder too
        QVERIFY(child(games.value(QStringLiteral("children")).toList(), QStringLiteral("Solitaire")).value(QStringLiteral("isNew")).toBool());

        // Launching it clears the highlight; so does time passing.
        apps.launch(QStringLiteral("solitaire.desktop"));
        usage.recordLaunch(QStringLiteral("solitaire.desktop"));
        QVERIFY(!child(submenu(menu.tree(), QStringLiteral("Games")), QStringLiteral("Solitaire")).value(QStringLiteral("isNew")).toBool());

        QFile::remove(appDir + QLatin1String("/solitaire.desktop"));
        spy.clear();
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() > 0, 5000);
        QVERIFY(!labels(submenu(menu.tree(), QStringLiteral("Games"))).contains(QStringLiteral("Solitaire")));
    }

    void newProgramExpires()
    {
        UsageStore usage(m_tmp.filePath(QStringLiteral("usage-expire.json")));
        usage.noteInstalled({QStringLiteral("a.desktop")}); // first run: old
        usage.noteInstalled({QStringLiteral("b.desktop")});
        QVERIFY(!usage.isNew(QStringLiteral("a.desktop"), 7));
        QVERIFY(usage.isNew(QStringLiteral("b.desktop"), 7));
        usage.setNow(QDateTime::currentDateTimeUtc().addDays(8));
        QVERIFY(!usage.isNew(QStringLiteral("b.desktop"), 7));
    }

    void frequencyDecays()
    {
        UsageStore usage(m_tmp.filePath(QStringLiteral("usage-decay.json")));
        const QDateTime t0 = QDateTime::currentDateTimeUtc();
        usage.setNow(t0.addDays(-14));
        for (int i = 0; i < 4; ++i)
            usage.recordLaunch(QStringLiteral("old.desktop"));
        usage.setNow(t0);
        usage.recordLaunch(QStringLiteral("recent.desktop"));
        usage.recordLaunch(QStringLiteral("recent.desktop"));
        // 4 launches two half-lives ago (= 1.0) rank below 2 launches today.
        QCOMPARE(usage.byScore(), (QStringList{QStringLiteral("recent.desktop"), QStringLiteral("old.desktop")}));
        QVERIFY(qAbs(usage.score(QStringLiteral("old.desktop")) - 1.0) < 0.01);
        // Persisted.
        UsageStore again(m_tmp.filePath(QStringLiteral("usage-decay.json")));
        again.setNow(t0);
        QCOMPARE(again.byScore(), usage.byScore());
    }

    void pinnedAndFrequent()
    {
        AppRegistry apps;
        UsageStore usage(m_tmp.filePath(QStringLiteral("usage-pins.json")));
        auto s = settings(QStringLiteral("\n[StartMenu]\npinned=meadow:internet, meadow:email\ndefaultFrequent=calc.desktop, writer.desktop, not-installed.desktop\n"));
        RecentAppsModel model(&apps, &usage, s.get());
        // Internet comes from mimeapps.list; E-mail from MimeType= as fallback.
        QCOMPARE(labels(model.pinned()), (QStringList{QStringLiteral("Internet"), QStringLiteral("E-mail")}));
        QCOMPARE(model.pinned().at(0).toMap().value(QStringLiteral("subtitle")).toString(), QStringLiteral("Web Browser"));
        QCOMPARE(model.pinned().at(1).toMap().value(QStringLiteral("desktopId")).toString(), QStringLiteral("mail.desktop"));
        QCOMPARE(labels(model.frequent()), (QStringList{QStringLiteral("Calculator"), QStringLiteral("Writer")}));

        // Launching ranks a program first; pinned programs never repeat below.
        model.activate(QStringLiteral("chess.desktop"));
        model.activate(QStringLiteral("chess.desktop"));
        model.activate(QStringLiteral("browser.desktop"));
        QCOMPARE(labels(model.frequent()).first(), QStringLiteral("Chess"));
        QVERIFY(!labels(model.frequent()).contains(QStringLiteral("Web Browser")));
        QVERIFY(launches().contains(QStringLiteral("chess")));

        // Pin, reorder by pinning at an index (drag and drop), unpin.
        model.pin(QStringLiteral("chess.desktop"));
        QCOMPARE(labels(model.pinned()), (QStringList{QStringLiteral("Internet"), QStringLiteral("E-mail"), QStringLiteral("Chess")}));
        QVERIFY(!labels(model.frequent()).contains(QStringLiteral("Chess")));
        model.pin(QStringLiteral("chess.desktop"), 0);
        QCOMPARE(labels(model.pinned()).first(), QStringLiteral("Chess"));
        model.unpin(QStringLiteral("meadow:email"));
        QCOMPARE(labels(model.pinned()), (QStringList{QStringLiteral("Chess"), QStringLiteral("Internet")}));
        QVERIFY(model.isPinned(QStringLiteral("browser.desktop"))); // through meadow:internet

        // Remove from this list; it comes back only when used again.
        model.removeFromFrequent(QStringLiteral("calc.desktop"));
        QVERIFY(!labels(model.frequent()).contains(QStringLiteral("Calculator")));
        model.activate(QStringLiteral("calc.desktop"));
        QVERIFY(labels(model.frequent()).contains(QStringLiteral("Calculator")));

        // The user's pins survive a restart.
        UsageStore usage2(m_tmp.filePath(QStringLiteral("usage-pins.json")));
        RecentAppsModel again(&apps, &usage2, s.get());
        QCOMPARE(labels(again.pinned()), (QStringList{QStringLiteral("Chess"), QStringLiteral("Internet")}));
    }

    void frequentCount()
    {
        AppRegistry apps;
        UsageStore usage(m_tmp.filePath(QStringLiteral("usage-count.json")));
        auto s = settings(QStringLiteral("\n[StartMenu]\npinned=\ndefaultFrequent=\n"));
        RecentAppsModel model(&apps, &usage, s.get());
        model.setFrequentCount(2);
        for (const char *id : {"a11y.desktop", "calc.desktop", "chess.desktop", "writer.desktop"})
            model.activate(QString::fromLatin1(id));
        QCOMPARE(model.frequent().size(), 2);
    }

    void places()
    {
        auto s = settings(QStringLiteral("\n[StartMenu]\nfileManager=true\ncontrolPanel=meadow-no-such-control-panel\n"
                                         "printers=\nhelp=true --help-browser\nsearch=true --search\n"));
        PlacesModel model(s.get());
        QStringList keys;
        for (const QVariant &v : model.places())
            keys << v.toMap().value(QStringLiteral("key")).toString();
        // Tools whose program is missing (or unset) are hidden.
        QCOMPARE(keys, (QStringList{QStringLiteral("documents"), QStringLiteral("recent"), QStringLiteral("pictures"),
                                    QStringLiteral("music"), QStringLiteral("computer"), QString(), QStringLiteral("help"),
                                    QStringLiteral("search"), QStringLiteral("run")}));
        QFile::remove(launchLog());
        QVERIFY(model.activate(QStringLiteral("documents")));
        QVERIFY(model.activate(QStringLiteral("computer")));
        QVERIFY(model.activate(QStringLiteral("search")));
        QSignalSpy run(&model, &PlacesModel::runRequested);
        QVERIFY(model.activate(QStringLiteral("run")));
        QCOMPARE(run.count(), 1);
        const QStringList log = launches();
        QCOMPARE(log.size(), 3);
        QVERIFY(log.at(0).startsWith(QLatin1String("true\t")));
        QVERIFY(log.at(0).endsWith(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)));
        QCOMPARE(log.at(1), QStringLiteral("true\t/")); // not pcmanfm: no computer:///
        QCOMPARE(log.at(2), QStringLiteral("true\t--search"));
    }

    void recentDocuments()
    {
        const QString dir = m_tmp.filePath(QStringLiteral("recent"));
        QDir().mkpath(dir);
        for (const char *name : {"older.txt", "newer doc.odt"}) {
            QFile f(dir + QLatin1Char('/') + QLatin1String(name));
            QVERIFY(f.open(QIODevice::WriteOnly));
            // Not empty: newer MIME databases call empty files application/x-zerosize.
            f.write("text\n");
        }
        QFile in(QStringLiteral(FIXTURE "/recently-used.xbel"));
        QVERIFY(in.open(QIODevice::ReadOnly));
        const QString xbel = m_tmp.filePath(QStringLiteral("recently-used.xbel"));
        QFile out(xbel);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(QString::fromUtf8(in.readAll()).replace(QLatin1String("file:///RECENT"), QLatin1String("file://") + dir).toUtf8());
        out.close();
        const QVariantList docs = PlacesModel::parseRecent(xbel, 15);
        // Newest first; deleted files and remote URLs are skipped.
        QCOMPARE(labels(docs), (QStringList{QStringLiteral("newer doc.odt"), QStringLiteral("older.txt")}));
        QCOMPARE(docs.at(1).toMap().value(QStringLiteral("iconName")).toString(), QStringLiteral("text-plain"));
        QCOMPARE(PlacesModel::parseRecent(xbel, 1).size(), 1);
        QVERIFY(PlacesModel::parseRecent(m_tmp.filePath(QStringLiteral("none.xbel")), 15).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestAppMenu)
#include "test_appmenu.moc"
