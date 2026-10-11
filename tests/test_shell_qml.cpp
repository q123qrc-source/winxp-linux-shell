// SPDX-License-Identifier: GPL-3.0-or-later
// The taskbar QML in preview mode, driven with real mouse clicks: task button
// semantics, window menu, grouping, attention, show desktop, Start button,
// notification balloons, the language bar, auto-hide and Taskbar Properties.
// Window requests are checked through PreviewToplevelModel's log.
#include "ThemeLoader.h"
#include "ipc/ShellIpc.h"
#include "models/AppMenuModel.h"
#include "models/AppRegistry.h"
#include "models/PlacesModel.h"
#include "models/RecentAppsModel.h"
#include "models/UsageStore.h"
#include "models/IconProvider.h"
#include "services/BatteryService.h"
#include "services/KeyboardLayoutService.h"
#include "services/NetworkService.h"
#include "services/NotificationServer.h"
#include "services/PowerService.h"
#include "services/RunService.h"
#include "services/SessionService.h"
#include "services/ShellSounds.h"
#include "services/Settings.h"
#include "services/TrayModel.h"
#include "services/VolumeService.h"
#include "auth/LockAuthenticator.h"
#include "models/DesktopModel.h"
#include "services/LabwcConfig.h"
#include "services/PolkitAgent.h"
#include "services/TimeDateService.h"
#include "services/UserAccountsService.h"
#include "ui/ScreenGrabber.h"
#include "wayland/IdleMonitor.h"
#include "wayland/OutputManager.h"
#include "wayland/SessionLock.h"
#include "ui/ShellContext.h"
#include "wayland/ToplevelModel.h"

#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(Meadow_ShellPlugin)

namespace {
QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (!root)
        return nullptr;
    if (root->objectName() == name && root->isVisible())
        return root;
    const auto children = root->childItems();
    for (QQuickItem *child : children) {
        if (QQuickItem *found = findItem(child, name))
            return found;
    }
    return nullptr;
}

int countVisible(QQuickItem *root, const QString &prefix)
{
    int n = 0;
    if (root->objectName().startsWith(prefix) && root->isVisible() && root->width() > 0)
        ++n;
    const auto children = root->childItems();
    for (QQuickItem *child : children)
        n += countVisible(child, prefix);
    return n;
}
} // namespace

class TestShellQml : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    std::unique_ptr<PreviewToplevelModel> m_windows;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<AppRegistry> m_apps;
    std::unique_ptr<TrayModel> m_tray;
    std::unique_ptr<NotificationServer> m_notifications;
    std::unique_ptr<VolumeService> m_volume;
    std::unique_ptr<NetworkService> m_network;
    std::unique_ptr<BatteryService> m_battery;
    std::unique_ptr<KeyboardLayoutService> m_keyboard;
    std::unique_ptr<ShellIpc> m_ipc;
    std::unique_ptr<UsageStore> m_usage;
    std::unique_ptr<AppMenuModel> m_appMenu;
    std::unique_ptr<RecentAppsModel> m_startApps;
    std::unique_ptr<PlacesModel> m_places;
    std::unique_ptr<PowerService> m_power;
    std::unique_ptr<SessionService> m_session;
    std::unique_ptr<RunService> m_run;
    std::unique_ptr<ScreenGrabber> m_screen;
    std::unique_ptr<SessionLock> m_lock;
    std::unique_ptr<LockAuthenticator> m_lockAuth;
    std::unique_ptr<ShellSounds> m_sounds;
    std::unique_ptr<DesktopModel> m_desktop;
    std::unique_ptr<LabwcConfig> m_labwc;
    std::unique_ptr<OutputManager> m_outputs;
    std::unique_ptr<IdleMonitor> m_idle;
    std::unique_ptr<PolkitAgent> m_polkit;
    std::unique_ptr<TimeDateService> m_timeDate;
    std::unique_ptr<UserAccountsService> m_accounts;
    std::unique_ptr<ShellContext> m_shell;
    std::unique_ptr<QQmlApplicationEngine> m_engine;
    QQuickWindow *m_window = nullptr;

    QQuickItem *item(const QString &name) { return findItem(m_window->contentItem(), name); }

    void click(QQuickItem *target, Qt::MouseButton button = Qt::LeftButton, QPointF at = {-1, -1})
    {
        QVERIFY2(target, "item not found");
        if (at.x() < 0)
            at = QPointF(target->width() / 2, target->height() / 2);
        QTest::mouseClick(m_window, button, {}, target->mapToScene(at).toPoint());
    }

    QStringList launches() const
    {
        QFile f(m_dir.filePath(QStringLiteral("launches.log")));
        if (!f.open(QIODevice::ReadOnly))
            return {};
        return QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }
    void typeText(const QString &text)
    {
        // QTest::keyClicks only exists for widgets.
        for (const QChar c : text)
            QTest::keyClick(m_window, c.toLatin1());
    }
    void clearLaunches() { QFile::remove(m_dir.filePath(QStringLiteral("launches.log"))); }

    void start(int windows, int width = 1024, int height = 480)
    {
        m_windows = std::make_unique<PreviewToplevelModel>();
        for (int i = 0; i < windows; ++i)
            m_windows->addPreviewWindow(QStringLiteral("Window %1").arg(i + 1), i % 3 == 0 ? QStringLiteral("firefox") : QStringLiteral("app%1").arg(i));
        // Packaged defaults, with every right-column tool pointing at `true`
        // so all items show and their launches can be checked.
        const QString defaults = m_dir.filePath(QStringLiteral("defaults.conf"));
        if (!QFile::exists(defaults)) {
            QFile::copy(QStringLiteral(MEADOW_SOURCE_DIR "/shell/data/shell.conf"), defaults);
            QFile::setPermissions(defaults, QFile::ReadOwner | QFile::WriteOwner);
            QSettings ini(defaults, QSettings::IniFormat);
            ini.setValue(QStringLiteral("StartMenu/fileManager"), QStringLiteral("true --files"));
            for (const char *key : {"controlPanel", "defaultPrograms", "printers", "help", "search"})
                ini.setValue(QStringLiteral("StartMenu/") + QLatin1String(key), QStringLiteral("true --") + QLatin1String(key));
        }
        m_settings = std::make_unique<Settings>(m_dir.filePath(QStringLiteral("user-%1.conf").arg(QTest::currentTestFunction())), defaults);
        m_apps = std::make_unique<AppRegistry>();
        m_tray = std::make_unique<TrayModel>();
        m_notifications = std::make_unique<NotificationServer>();
        m_notifications->setDefaultTimeout(60000);
        m_volume = std::make_unique<VolumeService>();
        m_network = std::make_unique<NetworkService>(QDBusConnection::sessionBus());
        m_battery = std::make_unique<BatteryService>(QDBusConnection::sessionBus());
        m_keyboard = std::make_unique<KeyboardLayoutService>(QStringLiteral("us,de"), QString(),
                                                             QStringLiteral(MEADOW_SOURCE_DIR "/tests/fixtures/evdev.xml"));
        m_ipc = std::make_unique<ShellIpc>();
        m_usage = std::make_unique<UsageStore>(m_dir.filePath(QStringLiteral("usage-%1.json").arg(QTest::currentTestFunction())));
        m_appMenu = std::make_unique<AppMenuModel>(m_apps.get(), m_usage.get());
        m_startApps = std::make_unique<RecentAppsModel>(m_apps.get(), m_usage.get(), m_settings.get());
        m_places = std::make_unique<PlacesModel>(m_settings.get());
        m_power = std::make_unique<PowerService>(QDBusConnection::sessionBus(), true);
        m_session = std::make_unique<SessionService>(QDBusConnection::sessionBus(), true);
        m_run = std::make_unique<RunService>(m_apps.get(), m_dir.filePath(QStringLiteral("run-%1").arg(QTest::currentTestFunction())));
        m_screen = std::make_unique<ScreenGrabber>();
        m_lock = std::make_unique<SessionLock>(true);
        m_lockAuth = std::make_unique<LockAuthenticator>(true);
        m_sounds = std::make_unique<ShellSounds>(m_session.get(), m_power.get());
        // A desktop folder per test with a couple of files.
        const QString desk = m_dir.filePath(QStringLiteral("desktop-%1").arg(QTest::currentTestFunction()));
        QDir().mkpath(desk + QStringLiteral("/Holiday photos"));
        QFile(desk + QStringLiteral("/Shopping list.txt")).open(QIODevice::WriteOnly);
        m_desktop = std::make_unique<DesktopModel>(m_settings.get(), m_places.get(), m_apps.get(), desk,
                                                   m_dir.filePath(QStringLiteral("trash-%1/files").arg(QTest::currentTestFunction())),
                                                   m_dir.filePath(QStringLiteral("layout-%1.json").arg(QTest::currentTestFunction())));
        m_labwc = std::make_unique<LabwcConfig>(m_settings.get(), QString(), QString(), false);
        m_outputs = std::make_unique<OutputManager>(true);
        m_idle = std::make_unique<IdleMonitor>(true);
        m_polkit = std::make_unique<PolkitAgent>(true);
        m_timeDate = std::make_unique<TimeDateService>(QDBusConnection::sessionBus(), true);
        m_accounts = std::make_unique<UserAccountsService>(QDBusConnection::sessionBus(), true);
        ShellContext::Services s;
        s.windows = m_windows.get();
        s.settings = m_settings.get();
        s.apps = m_apps.get();
        s.tray = m_tray.get();
        s.notifications = m_notifications.get();
        s.volume = m_volume.get();
        s.network = m_network.get();
        s.battery = m_battery.get();
        s.keyboard = m_keyboard.get();
        s.ipc = m_ipc.get();
        s.appMenu = m_appMenu.get();
        s.startApps = m_startApps.get();
        s.places = m_places.get();
        s.power = m_power.get();
        s.session = m_session.get();
        s.run = m_run.get();
        s.screen = m_screen.get();
        s.lock = m_lock.get();
        s.lockAuth = m_lockAuth.get();
        s.desktop = m_desktop.get();
        s.labwc = m_labwc.get();
        s.outputs = m_outputs.get();
        s.idle = m_idle.get();
        s.polkit = m_polkit.get();
        s.timeDate = m_timeDate.get();
        s.accounts = m_accounts.get();
        s.sounds = m_sounds.get();
        m_shell = std::make_unique<ShellContext>(s, true);

        m_engine = std::make_unique<QQmlApplicationEngine>();
        m_engine->addImportPath(QStringLiteral(MEADOW_BUILD_QML_DIR));
        m_engine->addImageProvider(QStringLiteral("icon"), new IconProvider);
        m_engine->addImageProvider(QStringLiteral("screen"), new ScreenImageProvider(m_screen.get()));
        m_engine->setInitialProperties({{QStringLiteral("width"), width}, {QStringLiteral("height"), height}});
        m_engine->loadFromModule("Meadow.Shell", "Main");
        QVERIFY(!m_engine->rootObjects().isEmpty());
        m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().constFirst());
        QVERIFY(m_window);
        QVERIFY(QTest::qWaitForWindowExposed(m_window));
        QTRY_VERIFY(item(QStringLiteral("task-0")));
    }

private Q_SLOTS:
    void initTestCase()
    {
        qputenv("XDG_DATA_DIRS", MEADOW_SOURCE_DIR "/tests/fixtures");
        qputenv("XDG_DATA_HOME", m_dir.filePath(QStringLiteral("data")).toUtf8());
        qputenv("MEADOW_ICONS_GENERIC", "1");
        qputenv("MEADOW_WPCTL", "/bin/false");
        // Muted: actions after a sound (Log Off, Turn Off) then run at once.
        qputenv("MEADOW_MUTE", "1");
        qputenv("MEADOW_MENU_DIR", MEADOW_SOURCE_DIR "/session/menus");
        qputenv("MEADOW_LAUNCH_LOG", m_dir.filePath(QStringLiteral("launches.log")).toUtf8());
        qputenv("XDG_CURRENT_DESKTOP", "Meadow");
        QVERIFY(ThemeLoader::instance()->loadScheme(QStringLiteral("luna-blue")));
        ThemeLoader::instance()->setReducedMotion(true);
    }

    void cleanup()
    {
        m_engine.reset();
        m_shell.reset();
        m_startApps.reset();
        m_appMenu.reset();
        m_usage.reset();
        clearLaunches();
        m_ipc.reset();
        m_windows.reset();
        m_notifications.reset();
        m_tray.reset();
        m_window = nullptr;
    }

    void taskButtonClicks()
    {
        start(3);
        // The newest window (3) is active: clicking it minimizes.
        click(item(QStringLiteral("task-2")));
        QCOMPARE(m_windows->log().last(), QStringLiteral("minimize 3"));
        // Clicking another one activates it; clicking a minimized one restores it.
        click(item(QStringLiteral("task-0")));
        QCOMPARE(m_windows->log().last(), QStringLiteral("activate 1"));
        click(item(QStringLiteral("task-2")));
        QVERIFY(m_windows->log().contains(QStringLiteral("unminimize 3")));
        QCOMPARE(m_windows->log().last(), QStringLiteral("activate 3"));
        QVERIFY(item(QStringLiteral("task-2"))->property("active").toBool());
    }

    void middleClickCloses()
    {
        start(3);
        click(item(QStringLiteral("task-1")), Qt::MiddleButton);
        QCOMPARE(m_windows->log().last(), QStringLiteral("close 2"));
        QCOMPARE(m_windows->count(), 2);
        QTRY_COMPARE(countVisible(m_window->contentItem(), QStringLiteral("task-")), 2);
    }

    void windowMenu()
    {
        start(3);
        click(item(QStringLiteral("task-0")), Qt::RightButton);
        QQuickItem *restore = item(QStringLiteral("menuItem:Restore"));
        QVERIFY(restore);
        click(item(QStringLiteral("menuItem:Maximize")));
        QCOMPARE(m_windows->log().last(), QStringLiteral("maximize 1"));
        QTRY_VERIFY(!item(QStringLiteral("menuItem:Maximize"))); // menu closed

        click(item(QStringLiteral("task-0")), Qt::RightButton);
        click(item(QStringLiteral("menuItem:Close")));
        QCOMPARE(m_windows->log().last(), QStringLiteral("close 1"));
    }

    void clickOutsideClosesMenu()
    {
        start(2);
        click(item(QStringLiteral("task-0")), Qt::RightButton);
        QVERIFY(item(QStringLiteral("menuItem:Close")));
        QTest::mouseClick(m_window, Qt::LeftButton, {}, QPoint(600, 100));
        QTRY_VERIFY(!item(QStringLiteral("menuItem:Close")));
        QCOMPARE(m_windows->count(), 2);
    }

    void escapeClosesMenu()
    {
        start(2);
        click(item(QStringLiteral("task-0")), Qt::RightButton);
        QVERIFY(item(QStringLiteral("menuItem:Close")));
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(!item(QStringLiteral("menuItem:Close")));
    }

    void grouping()
    {
        // 10 windows, 4 of them "firefox", on a narrow bar.
        start(10, 640);
        QTRY_VERIFY(countVisible(m_window->contentItem(), QStringLiteral("task-")) < 10);
        QQuickItem *leader = item(QStringLiteral("task-0"));
        QCOMPARE(leader->property("groupCount").toInt(), 4);
        QCOMPARE(leader->property("title").toString(), QStringLiteral("Web Browser"));
        click(leader);
        QVERIFY(item(QStringLiteral("menuItem:Window 4")));
        click(item(QStringLiteral("menuItem:Window 4")));
        QCOMPARE(m_windows->log().last(), QStringLiteral("activate 4"));
        // Grouping off: every window has its own button again.
        m_settings->setGroupButtons(false);
        QTRY_COMPARE(countVisible(m_window->contentItem(), QStringLiteral("task-")), 10);
    }

    void groupCloseFromMenu()
    {
        start(10, 640);
        click(item(QStringLiteral("task-0")), Qt::RightButton);
        click(item(QStringLiteral("menuItem:Close Group")));
        QCOMPARE(m_windows->count(), 6);
    }

    void attention()
    {
        start(3);
        m_windows->setNeedsAttention(0, true);
        QQuickItem *button = item(QStringLiteral("task-0"));
        // Reduced motion: no flashing, the button turns orange at once.
        QTRY_VERIFY(button->property("flashOn").toBool());
        click(button);
        QTRY_VERIFY(!button->property("flashOn").toBool());
    }

    void showDesktop()
    {
        start(3);
        m_windows->toggleShowDesktop(); // Win+D (meadow-shellctl show-desktop)
        QVERIFY(m_windows->showingDesktop());
        for (int i = 0; i < 3; ++i)
            QVERIFY(m_windows->window(i).minimized);
        m_windows->toggleShowDesktop();
        QCOMPARE(m_windows->log().last(), QStringLiteral("activate 3")); // the active one comes back last
        // Quick Launch "Show Desktop" does the same.
        QQuickItem *quick = item(QStringLiteral("quickLaunch:meadow:show-desktop"));
        click(quick);
        QVERIFY(m_windows->showingDesktop());
    }

    void startButton()
    {
        start(1);
        click(item(QStringLiteral("startButton")));
        QVERIFY(m_shell->startMenuOpen());
        QVERIFY(item(QStringLiteral("startButton"))->property("pressedLook").toBool());
        click(item(QStringLiteral("startButton")));
        QVERIFY(!m_shell->startMenuOpen());
    }

    void balloon()
    {
        start(1);
        QVERIFY(!item(QStringLiteral("balloon")));
        const uint id = m_notifications->notify(QStringLiteral("app"), 0, QString(), QStringLiteral("Hello"),
                                                QStringLiteral("World"), {QStringLiteral("default"), QStringLiteral("Open")}, {}, -1);
        QTRY_VERIFY(item(QStringLiteral("balloon")));
        QSignalSpy invoked(m_notifications.get(), &NotificationServer::ActionInvoked);
        // Clicking the balloon body runs the default action.
        click(item(QStringLiteral("balloon")), Qt::LeftButton, QPointF(20, 30));
        QCOMPARE(invoked.count(), 1);
        QCOMPARE(invoked.first().value(0).toUInt(), id);
        QTRY_VERIFY(!item(QStringLiteral("balloon")));
    }

    void languageBar()
    {
        start(1);
        QQuickItem *bar = item(QStringLiteral("languageBar"));
        QVERIFY(bar);
        click(bar);
        click(item(QStringLiteral("menuItem:DE  German")));
        QCOMPARE(m_keyboard->currentShortName(), QStringLiteral("DE"));
        m_keyboard->next();
        QCOMPARE(m_keyboard->currentShortName(), QStringLiteral("EN"));
    }

    void clockAndCalendar()
    {
        start(1);
        click(item(QStringLiteral("clock")));
        QTRY_VERIFY(item(QStringLiteral("calendar")));
        m_settings->setShowClock(false);
        QTRY_VERIFY(!item(QStringLiteral("clock")));
    }

    void autoHide()
    {
        start(1);
        QQuickItem *taskbar = item(QStringLiteral("taskbar"));
        const int full = taskbar->property("visibleHeight").toInt();
        m_settings->setAutoHide(true);
        QTest::mouseMove(m_window, QPoint(500, 100));
        QTRY_VERIFY(taskbar->property("visibleHeight").toInt() < full);
        QTest::mouseMove(m_window, QPoint(500, 479));
        QTRY_COMPARE(taskbar->property("visibleHeight").toInt(), full);
    }

    void taskbarProperties()
    {
        start(1);
        QQuickItem *taskbar = item(QStringLiteral("taskbar"));
        // Right-click empty taskbar space, then Properties.
        click(taskbar, Qt::RightButton, QPointF(taskbar->width() * 0.6, taskbar->height() / 2));
        click(item(QStringLiteral("menuItem:Properties")));
        QTRY_VERIFY(item(QStringLiteral("taskbarProperties")));
        QVERIFY(m_settings->showQuickLaunch());
        click(item(QStringLiteral("option:showQuickLaunch")));
        QVERIFY(m_settings->showQuickLaunch()); // not before OK/Apply
        click(item(QStringLiteral("dialogButton:OK")));
        QVERIFY(!m_settings->showQuickLaunch());
        QTRY_VERIFY(!item(QStringLiteral("taskbarProperties")));
    }

    void lockFromTaskbarMenu()
    {
        start(1);
        QVERIFY(m_settings->taskbarLocked());
        QQuickItem *taskbar = item(QStringLiteral("taskbar"));
        click(taskbar, Qt::RightButton, QPointF(taskbar->width() * 0.6, taskbar->height() / 2));
        click(item(QStringLiteral("menuItem:Lock the Taskbar")));
        QVERIFY(!m_settings->taskbarLocked());
    }

    // --- Start menu (phase 7) -------------------------------------------

    QQuickItem *openStartMenu()
    {
        click(item(QStringLiteral("startButton")));
        QQuickItem *menu = nullptr;
        [&] { QTRY_VERIFY((menu = item(QStringLiteral("startMenu")))); }();
        return menu;
    }

    void startMenuOpensAndCloses()
    {
        start(1, 1024, 640);
        openStartMenu();
        QVERIFY(m_shell->startMenuOpen());
        // Start button again: the click lands on the popup overlay and closes it.
        click(item(QStringLiteral("startButton")));
        QTRY_VERIFY(!item(QStringLiteral("startMenu")));
        QVERIFY(!m_shell->startMenuOpen());
        // Windows key (meadow-shellctl toggle-start-menu) opens and closes it.
        m_shell->setStartMenuOpen(true);
        QTRY_VERIFY(item(QStringLiteral("startMenu")));
        m_shell->setStartMenuOpen(false);
        QTRY_VERIFY(!item(QStringLiteral("startMenu")));
        // Escape closes it.
        openStartMenu();
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(!item(QStringLiteral("startMenu")));
        QVERIFY(!m_shell->startMenuOpen());
    }

    void everyItemLaunches()
    {
        start(1, 1024, 640);
        // Pinned and frequent programs.
        QStringList expected;
        const QVariantList left = m_startApps->pinned() + m_startApps->frequent();
        QVERIFY(left.size() >= 4);
        for (const QVariant &v : left) {
            const QVariantMap e = v.toMap();
            openStartMenu();
            const QString name = (e.value(QStringLiteral("pinned")).toBool() ? QStringLiteral("pinned:") : QStringLiteral("frequent:"))
                + e.value(QStringLiteral("id")).toString();
            click(item(name));
            QTRY_VERIFY(!item(QStringLiteral("startMenu")));
            expected << QStringLiteral("true"); // fixture apps all Exec=true
        }
        QCOMPARE(launches().size(), left.size());

        // Right column: every place and tool.
        clearLaunches();
        const QStringList keys{QStringLiteral("documents"), QStringLiteral("pictures"), QStringLiteral("music"), QStringLiteral("computer"),
                               QStringLiteral("controlPanel"), QStringLiteral("defaultPrograms"), QStringLiteral("printers"),
                               QStringLiteral("help"), QStringLiteral("search")};
        for (const QString &key : keys) {
            openStartMenu();
            click(item(QStringLiteral("place:") + key));
            QTRY_VERIFY(!item(QStringLiteral("startMenu")));
        }
        const QStringList log = launches();
        QCOMPARE(log.size(), keys.size());
        QVERIFY(log.at(0).startsWith(QLatin1String("true\t--files\t")));
        QCOMPARE(log.at(3), QStringLiteral("true\t--files\t/")); // My Computer, not pcmanfm
        QCOMPARE(log.at(4), QStringLiteral("true\t--controlPanel"));
        QCOMPARE(log.at(8), QStringLiteral("true\t--search"));

        // Run… opens the Run dialog.
        openStartMenu();
        click(item(QStringLiteral("place:run")));
        QTRY_VERIFY(item(QStringLiteral("runDialog")));
    }

    void everyAllProgramsEntryLaunches()
    {
        start(1, 1024, 720);
        // Walk the tree: open each folder chain from the Start menu and click
        // every program; each must reach the launcher with its Exec line.
        QList<QStringList> paths;
        std::function<void(const QVariantList &, QStringList)> collect = [&](const QVariantList &nodes, QStringList prefix) {
            for (const QVariant &v : nodes) {
                const QVariantMap n = v.toMap();
                QStringList path = prefix;
                path << n.value(QStringLiteral("label")).toString();
                if (n.contains(QStringLiteral("children")))
                    collect(n.value(QStringLiteral("children")).toList(), path);
                else
                    paths << path;
            }
        };
        collect(m_appMenu->tree(), {});
        QVERIFY(paths.size() >= 10);
        for (const QStringList &path : std::as_const(paths)) {
            openStartMenu();
            click(item(QStringLiteral("allPrograms")));
            for (const QString &label : path) {
                QQuickItem *row = nullptr;
                QTRY_VERIFY2((row = item(QStringLiteral("menuItem:") + label)), qPrintable(path.join(QLatin1String(" > "))));
                click(row);
            }
            QTRY_VERIFY(!item(QStringLiteral("startMenu")));
        }
        QCOMPARE(launches().size(), paths.size());
    }

    void keyboardNavigation()
    {
        start(1, 1024, 640);
        m_shell->setStartMenuOpen(true); // Windows key
        QTRY_VERIFY(item(QStringLiteral("startMenu")));
        // Down selects the first pinned item; Enter launches it.
        QTest::keyClick(m_window, Qt::Key_Down);
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_COMPARE(launches().size(), 1);

        // Type-ahead jumps to the next item starting with a letter.
        m_shell->setStartMenuOpen(true);
        QTRY_VERIFY(item(QStringLiteral("startMenu")));
        QQuickItem *left = item(QStringLiteral("startMenuLeft"));
        QTest::keyClick(m_window, Qt::Key_A); // "All Programs"
        QCOMPARE(left->property("current").toInt(), left->property("allProgramsIndex").toInt());
        // Right opens All Programs with its first entry selected.
        QTest::keyClick(m_window, Qt::Key_Right);
        QQuickItem *cascade = nullptr;
        QTRY_VERIFY((cascade = item(QStringLiteral("allProgramsMenu"))));
        QCOMPARE(cascade->property("current").toInt(), 0);
        QTRY_VERIFY(cascade->hasActiveFocus());
        // Type-ahead inside: "g" -> Games, Right opens it, Enter launches Mines.
        QTest::keyClick(m_window, Qt::Key_G);
        QTest::keyClick(m_window, Qt::Key_Right);
        QTRY_VERIFY(item(QStringLiteral("menuItem:Mines")));
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(!item(QStringLiteral("startMenu")));
        QCOMPARE(launches().size(), 2);

        // Escape leaves one cascade level at a time, then the menu.
        m_shell->setStartMenuOpen(true);
        QTRY_VERIFY(item(QStringLiteral("startMenu")));
        QTest::keyClick(m_window, Qt::Key_Up); // All Programs (bottom)
        QTest::keyClick(m_window, Qt::Key_Right);
        QTRY_VERIFY(item(QStringLiteral("allProgramsMenu")));
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(!item(QStringLiteral("allProgramsMenu")));
        QVERIFY(item(QStringLiteral("startMenu")));
        // Right column and Tab.
        QTest::keyClick(m_window, Qt::Key_Tab);
        QQuickItem *right = item(QStringLiteral("startMenuRight"));
        QVERIFY(right->hasActiveFocus());
        QCOMPARE(right->property("current").toInt(), 0); // My Documents
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(!item(QStringLiteral("startMenu")));
        QVERIFY(launches().last().startsWith(QLatin1String("true\t--files\t")));
    }

    void pinFromAllPrograms()
    {
        start(1, 1024, 720);
        openStartMenu();
        click(item(QStringLiteral("allPrograms")));
        QTRY_VERIFY(item(QStringLiteral("menuItem:Games")));
        click(item(QStringLiteral("menuItem:Games")));
        QTRY_VERIFY(item(QStringLiteral("menuItem:Mines")));
        click(item(QStringLiteral("menuItem:Mines")), Qt::RightButton);
        QTRY_VERIFY(item(QStringLiteral("menuItem:Pin to Start menu")));
        click(item(QStringLiteral("menuItem:Pin to Start menu")));
        QTRY_VERIFY(item(QStringLiteral("pinned:org.example.Mines.desktop")));
        QVERIFY(item(QStringLiteral("startMenu"))); // still open
        // Unpin from the pinned list's context menu.
        click(item(QStringLiteral("pinned:org.example.Mines.desktop")), Qt::RightButton);
        click(item(QStringLiteral("menuItem:Unpin from Start menu")));
        QTRY_VERIFY(!item(QStringLiteral("pinned:org.example.Mines.desktop")));
    }

    void dragToPin()
    {
        start(1, 1024, 720);
        openStartMenu();
        click(item(QStringLiteral("allPrograms")));
        QTRY_VERIFY(item(QStringLiteral("menuItem:Games")));
        click(item(QStringLiteral("menuItem:Games")));
        QQuickItem *mines = nullptr;
        QTRY_VERIFY((mines = item(QStringLiteral("menuItem:Mines"))));
        QQuickItem *pinnedArea = item(QStringLiteral("pinnedArea"));
        const QPoint from = mines->mapToScene(QPointF(mines->width() / 2, mines->height() / 2)).toPoint();
        // Drop between the two pinned items.
        const QPoint to = pinnedArea->mapToScene(QPointF(pinnedArea->width() / 2, pinnedArea->height() / 2)).toPoint();
        QTest::mousePress(m_window, Qt::LeftButton, {}, from);
        for (int i = 1; i <= 20; ++i) {
            QTest::mouseMove(m_window, from + (to - from) * i / 20);
            QTest::qWait(5);
        }
        QTest::mouseRelease(m_window, Qt::LeftButton, {}, to);
        QTRY_COMPARE(m_startApps->pinned().size(), 3);
        QCOMPARE(m_startApps->pinned().at(1).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("org.example.Mines.desktop"));
    }

    void removeFromFrequent()
    {
        start(1, 1024, 640);
        const QString id = m_startApps->frequent().first().toMap().value(QStringLiteral("id")).toString();
        openStartMenu();
        click(item(QStringLiteral("frequent:") + id), Qt::RightButton);
        click(item(QStringLiteral("menuItem:Remove from This List")));
        QTRY_VERIFY(!item(QStringLiteral("frequent:") + id));
    }

    void logOffDialog()
    {
        start(2, 1024, 640);
        QSignalSpy simulated(m_session.get(), &SessionService::simulated);
        openStartMenu();
        click(item(QStringLiteral("startMenuLogOff")));
        QTRY_VERIFY(item(QStringLiteral("logOffDialog")));
        QVERIFY(m_screen->ready()); // the gray backdrop has a capture
        click(item(QStringLiteral("logOffButton")));
        QCOMPARE(simulated.count(), 1);
        QCOMPARE(simulated.first().value(0).toString(), QStringLiteral("LogOff"));
        QTRY_VERIFY(!item(QStringLiteral("logOffDialog")));
        QVERIFY(!m_screen->ready()); // capture dropped

        // Ctrl+Alt+Delete path (IPC) and Switch User.
        m_ipc->ShowLogOffDialog();
        QTRY_VERIFY(item(QStringLiteral("logOffDialog")));
        click(item(QStringLiteral("switchUserButton")));
        QCOMPARE(simulated.last().value(0).toString(), QStringLiteral("SwitchToGreeter"));

        // Cancel and Escape do nothing.
        m_ipc->ShowLogOffDialog();
        QTRY_VERIFY(item(QStringLiteral("logOffDialog")));
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(!item(QStringLiteral("logOffDialog")));
        QCOMPARE(simulated.count(), 2);
    }

    void turnOffDialog()
    {
        start(1, 1024, 640);
        QSignalSpy simulated(m_power.get(), &PowerService::simulated);
        openStartMenu();
        click(item(QStringLiteral("startMenuTurnOff")));
        QTRY_VERIFY(item(QStringLiteral("turnOffDialog")));
        click(item(QStringLiteral("restartButton")));
        QCOMPARE(simulated.last().value(0).toString(), QStringLiteral("Reboot"));
        m_ipc->ShowTurnOffDialog();
        QTRY_VERIFY(item(QStringLiteral("turnOffDialog")));
        click(item(QStringLiteral("turnOffButton")));
        QCOMPARE(simulated.last().value(0).toString(), QStringLiteral("PowerOff"));
        m_ipc->ShowTurnOffDialog();
        QTRY_VERIFY(item(QStringLiteral("standByButton")));
        click(item(QStringLiteral("standByButton")));
        QCOMPARE(simulated.last().value(0).toString(), QStringLiteral("Suspend"));
        m_ipc->ShowTurnOffDialog();
        QTRY_VERIFY(item(QStringLiteral("cancelButton")));
        click(item(QStringLiteral("cancelButton")));
        QTRY_VERIFY(!item(QStringLiteral("turnOffDialog")));
        QCOMPARE(simulated.count(), 3);
    }

    void runDialog()
    {
        start(1, 1024, 640);
        m_ipc->ShowRunDialog(); // Win+R
        QQuickItem *field = nullptr;
        QTRY_VERIFY((field = item(QStringLiteral("runField"))));
        QTRY_VERIFY(field->hasActiveFocus());
        typeText(QStringLiteral("meadow-no-such-program"));
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(item(QStringLiteral("runError")));
        QVERIFY(item(QStringLiteral("runDialog"))); // stays open
        field->setProperty("text", QString());
        typeText(QStringLiteral("true --from-run"));
        click(item(QStringLiteral("runOk")));
        QTRY_VERIFY(!item(QStringLiteral("runDialog")));
        QCOMPARE(launches().last(), QStringLiteral("true\t--from-run"));
        // Reopened, the last command is preselected; Up/Down walk the history.
        m_ipc->ShowRunDialog();
        QTRY_VERIFY((field = item(QStringLiteral("runField"))));
        QCOMPARE(field->property("text").toString(), QStringLiteral("true --from-run"));
        click(item(QStringLiteral("runCancel")));
        QTRY_VERIFY(!item(QStringLiteral("runDialog")));
    }
    // --- Phase 8 ----------------------------------------------------------

    void desktopSelectionAndMenus()
    {
        start(1, 1024, 640);
        // Placed once the desktop knows its grid size (which resets the model).
        QQuickItem *computer = nullptr;
        QTRY_VERIFY((computer = item(QStringLiteral("desktopIcon:meadow:computer"))) && computer->x() >= 0);
        QQuickItem *list = item(QStringLiteral("desktopIcon:file:Shopping list.txt"));
        QVERIFY(list);
        click(computer);
        QVERIFY(computer->property("selected").toBool());
        // Ctrl+click adds to the selection.
        QTest::mouseClick(m_window, Qt::LeftButton, Qt::ControlModifier, list->mapToScene(QPointF(list->width() / 2, list->height() / 2)).toPoint());
        QTRY_VERIFY(list->property("selected").toBool());
        QVERIFY(computer->property("selected").toBool());
        // Empty space clears it.
        QTest::mouseClick(m_window, Qt::LeftButton, {}, QPoint(600, 300));
        QTRY_VERIFY(!computer->property("selected").toBool());
        QVERIFY(!list->property("selected").toBool());

        // Rubber band over the first column selects its icons.
        const QPoint from(2, 2);
        const QPoint to = computer->mapToScene(QPointF(computer->width() - 4, computer->height() - 10)).toPoint();
        QTest::mousePress(m_window, Qt::LeftButton, {}, QPoint(300, 2));
        QTest::mouseMove(m_window, QPoint(200, 40));
        QTest::mouseMove(m_window, to);
        QTRY_VERIFY(item(QStringLiteral("rubberBand")));
        QTest::mouseRelease(m_window, Qt::LeftButton, {}, to);
        Q_UNUSED(from);
        QVERIFY(computer->property("selected").toBool());
        QVERIFY(item(QStringLiteral("desktopIcon:meadow:documents"))->property("selected").toBool());

        // Desktop menu > New > Folder creates one and starts renaming it.
        QTest::mouseClick(m_window, Qt::RightButton, {}, QPoint(600, 300));
        QQuickItem *newEntry = nullptr;
        QTRY_VERIFY((newEntry = item(QStringLiteral("menuItem:New"))));
        click(newEntry);
        QQuickItem *folder = nullptr;
        QTRY_VERIFY((folder = item(QStringLiteral("menuItem:Folder"))));
        click(folder);
        QQuickItem *newIcon = nullptr;
        QTRY_VERIFY((newIcon = item(QStringLiteral("desktopIcon:file:New Folder"))));
        QTRY_VERIFY(newIcon->property("renaming").toBool());
        typeText(QStringLiteral("Taxes"));
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(item(QStringLiteral("desktopIcon:file:Taxes")));

        // Arrange Icons By > Name.
        QTest::mouseClick(m_window, Qt::RightButton, {}, QPoint(600, 300));
        QTRY_VERIFY(item(QStringLiteral("menuItem:Arrange Icons By")));
        click(item(QStringLiteral("menuItem:Arrange Icons By")));
        QTRY_VERIFY(item(QStringLiteral("menuItem:Name")));
        click(item(QStringLiteral("menuItem:Name")));
        QTRY_COMPARE(m_settings->stringValue(QStringLiteral("Desktop/sortBy")), QStringLiteral("name"));

        // Properties opens Display Properties on the Desktop tab.
        QTest::mouseClick(m_window, Qt::RightButton, {}, QPoint(600, 300));
        QTRY_VERIFY(item(QStringLiteral("menuItem:Properties")));
        click(item(QStringLiteral("menuItem:Properties")));
        QTRY_VERIFY(item(QStringLiteral("window:display")));
        QTRY_VERIFY(item(QStringLiteral("page:desktop")));
    }

    void desktopKeyboardAndDrag()
    {
        start(1, 1024, 640);
        QQuickItem *docs = nullptr;
        QTRY_VERIFY((docs = item(QStringLiteral("desktopIcon:meadow:documents"))) && docs->x() >= 0);
        click(docs);
        QTest::keyClick(m_window, Qt::Key_Down);
        QTRY_VERIFY(item(QStringLiteral("desktopIcon:meadow:computer"))->property("selected").toBool());
        QTest::keyClick(m_window, Qt::Key_Return); // opens My Computer
        QTRY_VERIFY(launches().join(QLatin1Char('\n')).contains(QStringLiteral("--files")));
        // Type-ahead.
        QTest::keyClick(m_window, Qt::Key_S);
        QTRY_VERIFY(item(QStringLiteral("desktopIcon:file:Shopping list.txt"))->property("selected").toBool());

        // Drag My Documents three cells to the right.
        const QPointF before(docs->x(), docs->y());
        const QPoint p0 = docs->mapToScene(QPointF(docs->width() / 2, docs->height() / 2)).toPoint();
        const int cell = int(docs->width());
        QTest::mousePress(m_window, Qt::LeftButton, {}, p0);
        QTest::mouseMove(m_window, p0 + QPoint(10, 0));
        QTest::mouseMove(m_window, p0 + QPoint(cell * 3, 0));
        QTest::mouseRelease(m_window, Qt::LeftButton, {}, p0 + QPoint(cell * 3, 0));
        QTRY_COMPARE(item(QStringLiteral("desktopIcon:meadow:documents"))->x(), before.x() + cell * 3);
        QCOMPARE(item(QStringLiteral("desktopIcon:meadow:documents"))->y(), before.y());
    }

    void altTabKeys()
    {
        start(4, 1024, 640);
        QQuickItem *altTab = nullptr;
        m_ipc->SwitchWindow(1);
        QTRY_VERIFY((altTab = item(QStringLiteral("altTab"))));
        QCOMPARE(altTab->property("selected").toInt(), 1);
        QTest::keyClick(m_window, Qt::Key_Tab, Qt::AltModifier);
        QCOMPARE(altTab->property("selected").toInt(), 2);
        QTest::keyClick(m_window, Qt::Key_Backtab, Qt::AltModifier | Qt::ShiftModifier);
        QCOMPARE(altTab->property("selected").toInt(), 1);
        m_ipc->SwitchWindow(1); // the keybind again while open: next
        QCOMPARE(altTab->property("selected").toInt(), 2);
        QTest::keyClick(m_window, Qt::Key_Escape);
        QTRY_VERIFY(!item(QStringLiteral("altTab")));
        QVERIFY(!m_windows->log().join(QLatin1Char(' ')).contains(QStringLiteral("activate")));
        // Enter switches (preview stands in for releasing Alt).
        m_ipc->SwitchWindow(-1); // previous: the least recently used
        QTRY_VERIFY((altTab = item(QStringLiteral("altTab"))));
        QCOMPARE(altTab->property("selected").toInt(), 3);
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(!item(QStringLiteral("altTab")));
        QVERIFY(m_windows->log().last().startsWith(QStringLiteral("activate")));
    }

    void displayPropertiesApplies()
    {
        start(1, 1024, 700);
        m_ipc->ShowDisplayProperties(QStringLiteral("appearance"));
        QTRY_VERIFY(item(QStringLiteral("page:appearance")));
        QQuickItem *apply = item(QStringLiteral("dialogButton:Apply"));
        QVERIFY(apply);
        QVERIFY(!apply->isEnabled());
        // Pick Silver in the color scheme combo.
        QQuickItem *combo = item(QStringLiteral("schemeCombo"));
        QVERIFY(combo);
        QMetaObject::invokeMethod(combo, "activated", Q_ARG(int, 2));
        QTRY_VERIFY(apply->isEnabled());
        click(apply);
        QTRY_COMPARE(ThemeLoader::instance()->schemeId(), QStringLiteral("luna-silver"));
        QCOMPARE(m_settings->stringValue(QStringLiteral("Appearance/scheme")), QStringLiteral("luna-silver"));
        QVERIFY(!apply->isEnabled());

        // Desktop tab: choose (None), then OK closes.
        click(item(QStringLiteral("tab:Desktop")));
        QQuickItem *list = nullptr;
        QTRY_VERIFY((list = item(QStringLiteral("wallpaperList"))));
        QTRY_COMPARE(list->property("currentIndex").toInt(), 1); // Meadow Hills
        list->forceActiveFocus();
        QTest::keyClick(m_window, Qt::Key_Up);
        QTRY_VERIFY(apply->isEnabled());
        click(item(QStringLiteral("dialogButton:OK")));
        QTRY_VERIFY(!item(QStringLiteral("window:display")));
        QCOMPARE(m_settings->stringValue(QStringLiteral("Desktop/wallpaper"), QStringLiteral("x")), QString());
        QVERIFY(!item(QStringLiteral("wallpaper"))); // the desktop shows the plain color

        // Settings tab: a new resolution asks to keep it; No reverts.
        m_ipc->ShowDisplayProperties(QStringLiteral("settings"));
        QQuickItem *slider = nullptr;
        QTRY_VERIFY((slider = item(QStringLiteral("resolutionSlider"))));
        slider->setProperty("value", 0);
        QMetaObject::invokeMethod(slider, "moved");
        QTRY_VERIFY(item(QStringLiteral("dialogButton:Apply"))->isEnabled());
        click(item(QStringLiteral("dialogButton:Apply")));
        QTRY_VERIFY(item(QStringLiteral("keepSettings")));
        QCOMPARE(m_outputs->outputs().first().toMap().value(QStringLiteral("width")).toInt(), 800);
        click(item(QStringLiteral("dialogButton:No")));
        QTRY_VERIFY(!item(QStringLiteral("keepSettings")));
        QCOMPARE(m_outputs->outputs().first().toMap().value(QStringLiteral("width")).toInt(), 1024);
        ThemeLoader::instance()->loadScheme(QStringLiteral("luna-blue"));
    }

    void controlPanelOpensApplets()
    {
        start(1, 1100, 720);
        m_ipc->ShowControlPanel(QString());
        QQuickItem *category = nullptr;
        QTRY_VERIFY((category = item(QStringLiteral("category:hardware"))));
        click(category);
        QQuickItem *mouse = nullptr;
        QTRY_VERIFY((mouse = item(QStringLiteral("applet:mouse"))));
        click(mouse);
        QQuickItem *swap = nullptr;
        QTRY_VERIFY((swap = item(QStringLiteral("option:swapButtons"))));
        click(swap);
        click(findItem(item(QStringLiteral("window:mouse")), QStringLiteral("dialogButton:OK")));
        QTRY_VERIFY(!item(QStringLiteral("window:mouse")));
        QVERIFY(m_settings->boolSetting(QStringLiteral("Mouse/swapButtons")));

        // Classic view lists every applet.
        click(item(QStringLiteral("switchView")));
        QTRY_VERIFY(item(QStringLiteral("classicView")));
        QCOMPARE(countVisible(m_window->contentItem(), QStringLiteral("applet:")), 7);

        // meadow-control-panel datetime: straight to the applet.
        m_ipc->ShowControlPanel(QStringLiteral("datetime"));
        QTRY_VERIFY(item(QStringLiteral("analogClock")));
    }

    void lockAndUnlockSimulated()
    {
        start(1, 1024, 640);
        m_lock->lock(); // main.cpp connects Win+L (ShellIpc::Lock) to this
        QQuickItem *lock = nullptr;
        QTRY_VERIFY((lock = item(QStringLiteral("lockScreen"))));
        QVERIFY(m_lock->isLocked());
        // The password field has the focus; a wrong password keeps it locked.
        QTest::qWait(100);
        typeText(QStringLiteral("nope"));
        QTest::keyClick(m_window, Qt::Key_Return);
        QTest::qWait(600);
        QVERIFY(m_lock->isLocked());
        typeText(QStringLiteral("meadow"));
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(!m_lock->isLocked());
        QTRY_VERIFY(!item(QStringLiteral("lockScreen")));
    }

    void screenSaverPreviewAndIdle()
    {
        start(1, 1024, 640);
        m_settings->set(QStringLiteral("ScreenSaver/passwordProtect"), true);
        // Idle starts it; input ends it and, password-protected, locks.
        m_idle->setIdle(true);
        QTRY_VERIFY(item(QStringLiteral("screenSaver")));
        QTest::keyClick(m_window, Qt::Key_Space); // too soon: ignored
        QVERIFY(item(QStringLiteral("screenSaver")));
        QTest::qWait(ThemeLoader::instance()->token(QStringLiteral("screenSaver.graceMs")).toInt() + 100);
        QTest::keyClick(m_window, Qt::Key_Space);
        QTRY_VERIFY(!item(QStringLiteral("screenSaver")));
        QVERIFY(m_lock->isLocked());
        m_lock->unlock();
        m_idle->setIdle(false);
        // The Display Properties preview never locks.
        m_ipc->ShowDisplayProperties(QStringLiteral("screensaver"));
        QQuickItem *preview = nullptr;
        QTRY_VERIFY((preview = item(QStringLiteral("saverPreview"))));
        click(preview);
        QTRY_VERIFY(item(QStringLiteral("screenSaver")));
        QTest::qWait(ThemeLoader::instance()->token(QStringLiteral("screenSaver.graceMs")).toInt() + 100);
        QTest::keyClick(m_window, Qt::Key_Space);
        QTRY_VERIFY(!item(QStringLiteral("screenSaver")));
        QVERIFY(!m_lock->isLocked());
    }

    void authenticateDialog()
    {
        start(1, 1024, 640);
        m_polkit->requestPreview();
        QQuickItem *field = nullptr;
        QTRY_VERIFY((field = item(QStringLiteral("authPassword"))));
        QTRY_VERIFY(field->hasActiveFocus());
        typeText(QStringLiteral("bad"));
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_VERIFY(item(QStringLiteral("authFeedback")));
        QCOMPARE(field->property("text").toString(), QString()); // cleared at once
        typeText(QStringLiteral("meadow"));
        QSignalSpy finished(m_polkit.get(), &PolkitAgent::finished);
        QTest::keyClick(m_window, Qt::Key_Return);
        QTRY_COMPARE(finished.size(), 1);
        QVERIFY(finished.at(0).at(0).toBool());
        QTRY_VERIFY(!item(QStringLiteral("authenticateDialog")));
    }

};

QTEST_MAIN(TestShellQml)
#include "test_shell_qml.moc"
