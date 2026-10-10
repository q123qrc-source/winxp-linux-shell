// SPDX-License-Identifier: GPL-3.0-or-later
// The taskbar QML in preview mode, driven with real mouse clicks: task button
// semantics, window menu, grouping, attention, show desktop, Start button,
// notification balloons, the language bar, auto-hide and Taskbar Properties.
// Window requests are checked through PreviewToplevelModel's log.
#include "ThemeLoader.h"
#include "ipc/ShellIpc.h"
#include "models/AppRegistry.h"
#include "models/IconProvider.h"
#include "services/BatteryService.h"
#include "services/KeyboardLayoutService.h"
#include "services/NetworkService.h"
#include "services/NotificationServer.h"
#include "services/Settings.h"
#include "services/TrayModel.h"
#include "services/VolumeService.h"
#include "ui/ShellContext.h"
#include "wayland/ToplevelModel.h"

#include <QDBusConnection>
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

    void start(int windows, int width = 1024)
    {
        m_windows = std::make_unique<PreviewToplevelModel>();
        for (int i = 0; i < windows; ++i)
            m_windows->addPreviewWindow(QStringLiteral("Window %1").arg(i + 1), i % 3 == 0 ? QStringLiteral("firefox") : QStringLiteral("app%1").arg(i));
        m_settings = std::make_unique<Settings>(m_dir.filePath(QStringLiteral("user-%1.conf").arg(QTest::currentTestFunction())),
                                                QStringLiteral(MEADOW_SOURCE_DIR "/shell/data/shell.conf"));
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
        m_shell = std::make_unique<ShellContext>(s, true);

        m_engine = std::make_unique<QQmlApplicationEngine>();
        m_engine->addImportPath(QStringLiteral(MEADOW_BUILD_QML_DIR));
        m_engine->addImageProvider(QStringLiteral("icon"), new IconProvider);
        m_engine->setInitialProperties({{QStringLiteral("width"), width}, {QStringLiteral("height"), 480}});
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
        QVERIFY(ThemeLoader::instance()->loadScheme(QStringLiteral("luna-blue")));
        ThemeLoader::instance()->setReducedMotion(true);
    }

    void cleanup()
    {
        m_engine.reset();
        m_shell.reset();
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
};

QTEST_MAIN(TestShellQml)
#include "test_shell_qml.moc"
