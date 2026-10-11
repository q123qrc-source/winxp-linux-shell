// SPDX-License-Identifier: GPL-3.0-or-later
// DesktopModel: items, first-free-cell placement, the Recycle Bin corner,
// snapping moves, arranging, persistence, new/rename/trash, launcher trust,
// and the wallpaper choices. Every directory is temporary.
#include "models/AppRegistry.h"
#include "models/DesktopModel.h"
#include "models/PlacesModel.h"
#include "services/Settings.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestDesktop : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    int m_case = 0;
    QString m_desk;
    QString m_trash;
    QString m_layout;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<AppRegistry> m_apps;
    std::unique_ptr<PlacesModel> m_places;

    void touch(const QString &name, bool executable = false, const QByteArray &content = {})
    {
        QFile f(m_desk + QLatin1Char('/') + name);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
        f.close();
        if (executable)
            f.setPermissions(f.permissions() | QFileDevice::ExeOwner);
    }
    std::unique_ptr<DesktopModel> model(int cols = 4, int rows = 3)
    {
        auto m = std::make_unique<DesktopModel>(m_settings.get(), m_places.get(), m_apps.get(), m_desk, m_trash, m_layout);
        m->setGridSize(cols, rows);
        return m;
    }
    QPointF cell(const DesktopModel &m, const QString &id)
    {
        const QVariantMap v = m.get(id);
        return {v.value(QStringLiteral("cellX")).toReal(), v.value(QStringLiteral("cellY")).toReal()};
    }

private Q_SLOTS:
    void initTestCase()
    {
        qputenv("XDG_DATA_HOME", m_dir.filePath(QStringLiteral("data")).toUtf8());
        qputenv("XDG_DATA_DIRS", m_dir.filePath(QStringLiteral("nodata")).toUtf8());
        // Qt built without its own MIME database (Arch) reads the system one
        // from the data directories, which the fixtures replace.
        QDir().mkpath(m_dir.filePath(QStringLiteral("data")));
        QFile::link(QStringLiteral("/usr/share/mime"), m_dir.filePath(QStringLiteral("data/mime")));
        qputenv("MEADOW_LAUNCH_LOG", m_dir.filePath(QStringLiteral("launches.log")).toUtf8());
    }
    void init()
    {
        ++m_case;
        m_desk = m_dir.filePath(QStringLiteral("Desktop%1").arg(m_case));
        m_trash = m_dir.filePath(QStringLiteral("Trash%1/files").arg(m_case));
        m_layout = m_dir.filePath(QStringLiteral("layout%1.json").arg(m_case));
        QDir().mkpath(m_desk);
        m_settings = std::make_unique<Settings>(m_dir.filePath(QStringLiteral("user%1.conf").arg(m_case)),
                                                QStringLiteral(MEADOW_SOURCE_DIR "/shell/data/shell.conf"));
        m_apps = std::make_unique<AppRegistry>();
        m_places = std::make_unique<PlacesModel>(m_settings.get());
    }

    void specialsAndFiles()
    {
        // Not empty: newer MIME databases call empty files application/x-zerosize.
        touch(QStringLiteral("b.txt"), false, "text\n");
        QDir(m_desk).mkdir(QStringLiteral("a folder"));
        auto m = model();
        QCOMPARE(m->rowCount(), 5);
        QCOMPARE(m->get(QStringLiteral("meadow:documents")).value(QStringLiteral("name")).toString(), QStringLiteral("My Documents"));
        QCOMPARE(m->get(QStringLiteral("file:a folder")).value(QStringLiteral("kind")).toString(), QStringLiteral("folder"));
        QCOMPARE(m->get(QStringLiteral("file:b.txt")).value(QStringLiteral("iconName")).toString(), QStringLiteral("text-plain"));
        // XP's default spots: My Documents first, the Recycle Bin bottom right.
        QCOMPARE(cell(*m, QStringLiteral("meadow:documents")), QPointF(0, 0));
        QCOMPARE(cell(*m, QStringLiteral("meadow:computer")), QPointF(0, 1));
        QCOMPARE(cell(*m, QStringLiteral("meadow:trash")), QPointF(3, 2));
        // Then files fill the first column top to bottom, then the next.
        QCOMPARE(cell(*m, QStringLiteral("file:a folder")), QPointF(0, 2));
        QCOMPARE(cell(*m, QStringLiteral("file:b.txt")), QPointF(1, 0));
    }

    void hiddenSpecials()
    {
        m_settings->set(QStringLiteral("Desktop/showMyComputer"), false);
        m_settings->set(QStringLiteral("Desktop/showRecycleBin"), false);
        auto m = model();
        QCOMPARE(m->rowCount(), 1);
        QCOMPARE(m->indexOf(QStringLiteral("meadow:computer")), -1);
    }

    void moveSnapsAndAvoidsOthers()
    {
        auto m = model();
        // Onto My Computer's cell: the nearest free one instead.
        m->moveItems({QStringLiteral("meadow:documents")}, 0.2, 1.1);
        const QPointF p = cell(*m, QStringLiteral("meadow:documents"));
        QVERIFY(p != QPointF(0, 1));
        QCOMPARE(p.x(), std::round(p.x()));
        QCOMPARE(p.y(), std::round(p.y()));
        // Off the grid: kept inside.
        m->moveItems({QStringLiteral("meadow:computer")}, 50, 50);
        const QPointF c = cell(*m, QStringLiteral("meadow:computer"));
        QVERIFY(c.x() <= 3 && c.y() <= 2);
    }

    void freePlacementWithoutAlignToGrid()
    {
        m_settings->set(QStringLiteral("Desktop/alignToGrid"), false);
        auto m = model();
        m->moveItems({QStringLiteral("meadow:documents")}, 1.4, 0.3);
        QCOMPARE(cell(*m, QStringLiteral("meadow:documents")), QPointF(1.4, 0.3));
        // Turning it back on snaps.
        m->setAlignToGrid(true);
        const QPointF p = cell(*m, QStringLiteral("meadow:documents"));
        QCOMPARE(p, QPointF(std::round(p.x()), std::round(p.y())));
    }

    void layoutIsRemembered()
    {
        touch(QStringLiteral("notes.txt"));
        {
            auto m = model();
            QCOMPARE(cell(*m, QStringLiteral("file:notes.txt")), QPointF(0, 2));
            m->moveItems({QStringLiteral("file:notes.txt")}, 3, -1);
            QCOMPARE(cell(*m, QStringLiteral("file:notes.txt")), QPointF(3, 1));
        } // the destructor saves
        auto again = model();
        QCOMPARE(cell(*again, QStringLiteral("file:notes.txt")), QPointF(3, 1));
    }

    void arrangeByName()
    {
        touch(QStringLiteral("zebra.txt"));
        touch(QStringLiteral("Apple.txt"));
        QDir(m_desk).mkdir(QStringLiteral("Mango"));
        auto m = model(2, 3);
        m->arrange(QStringLiteral("name"));
        // Specials first, then folders, then files by name (case-insensitive).
        QCOMPARE(cell(*m, QStringLiteral("meadow:documents")), QPointF(0, 0));
        QCOMPARE(cell(*m, QStringLiteral("meadow:computer")), QPointF(0, 1));
        QCOMPARE(cell(*m, QStringLiteral("meadow:trash")), QPointF(0, 2));
        QCOMPARE(cell(*m, QStringLiteral("file:Mango")), QPointF(1, 0));
        QCOMPARE(cell(*m, QStringLiteral("file:Apple.txt")), QPointF(1, 1));
        QCOMPARE(cell(*m, QStringLiteral("file:zebra.txt")), QPointF(1, 2));
        QCOMPARE(m->sortBy(), QStringLiteral("name"));
    }

    void autoArrangeIgnoresDrags()
    {
        auto m = model();
        m->setAutoArrange(true);
        const QPointF before = cell(*m, QStringLiteral("meadow:trash"));
        m->moveItems({QStringLiteral("meadow:trash")}, 2, 0);
        QCOMPARE(cell(*m, QStringLiteral("meadow:trash")), before);
    }

    void newRenameAndTrash()
    {
        auto m = model();
        const QString folder = m->newFolder();
        QCOMPARE(folder, QStringLiteral("file:New Folder"));
        QCOMPARE(m->newFolder(), QStringLiteral("file:New Folder (2)"));
        const QString doc = m->newTextDocument();
        QCOMPARE(doc, QStringLiteral("file:New Text Document.txt"));
        QVERIFY(QFile::exists(m_desk + QStringLiteral("/New Text Document.txt")));

        const QPointF pos = cell(*m, doc);
        QVERIFY(m->rename(doc, QStringLiteral("Plan.txt")));
        QCOMPARE(cell(*m, QStringLiteral("file:Plan.txt")), pos); // stays put
        QVERIFY(!m->rename(QStringLiteral("file:Plan.txt"), QStringLiteral("New Folder"))); // taken
        QVERIFY(!m->rename(QStringLiteral("file:Plan.txt"), QStringLiteral("a/b")));
        QVERIFY(!m->rename(QStringLiteral("meadow:computer"), QStringLiteral("PC"))); // specials keep names
        QVERIFY(!m->canRename(QStringLiteral("meadow:trash")));
    }

    void untrustedLaunchersDoNotRun()
    {
        const QByteArray entry = "[Desktop Entry]\nType=Application\nName=Totally A Document\nExec=true --ran\nIcon=x-office-document\n";
        touch(QStringLiteral("evil.desktop"), false, entry);
        touch(QStringLiteral("good.desktop"), true, entry);
        auto m = model();
        const QVariantMap evil = m->get(QStringLiteral("file:evil.desktop"));
        QCOMPARE(evil.value(QStringLiteral("name")).toString(), QStringLiteral("evil.desktop")); // no disguise
        QCOMPARE(evil.value(QStringLiteral("trusted")).toBool(), false);
        QSignalSpy refused(m.get(), &DesktopModel::untrustedLauncher);
        QVERIFY(!m->open(QStringLiteral("file:evil.desktop")));
        QCOMPARE(refused.size(), 1);
        QCOMPARE(m->get(QStringLiteral("file:good.desktop")).value(QStringLiteral("name")).toString(), QStringLiteral("Totally A Document"));
        QVERIFY(m->open(QStringLiteral("file:good.desktop")));
        QFile log(m_dir.filePath(QStringLiteral("launches.log")));
        QVERIFY(log.open(QIODevice::ReadOnly));
        const QString launched = QString::fromUtf8(log.readAll());
        QCOMPARE(launched.count(QStringLiteral("--ran")), 1);
    }

    void wallpaperChoices()
    {
        auto m = model();
        const QVariantList choices = m->wallpaperChoices();
        QVERIFY(choices.size() >= 2);
        QCOMPARE(choices.at(0).toMap().value(QStringLiteral("value")).toString(), QString());
        QCOMPARE(choices.at(1).toMap().value(QStringLiteral("value")).toString(), QStringLiteral("meadow-hills.svg"));
        QVERIFY(DesktopModel::isImage(QStringLiteral(MEADOW_SOURCE_DIR "/theme/assets/meadow-hills.svg")));
        QVERIFY(!DesktopModel::isImage(QStringLiteral(MEADOW_SOURCE_DIR "/README.md")));
    }

    void gridShrinkRelocatesItems()
    {
        auto m = model(6, 6);
        m->moveItems({QStringLiteral("meadow:computer")}, 5, 4);
        m->setGridSize(3, 3);
        const QPointF c = cell(*m, QStringLiteral("meadow:computer"));
        QVERIFY(c.x() < 3 && c.y() < 3);
    }
};

QTEST_GUILESS_MAIN(TestDesktop)
#include "test_desktop.moc"
