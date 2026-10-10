// SPDX-License-Identifier: GPL-3.0-or-later
#include "ThemeLoader.h"
#include "ipc/ShellIpc.h"
#include "models/AppRegistry.h"
#include "models/AppMenuModel.h"
#include "models/IconProvider.h"
#include "models/PlacesModel.h"
#include "models/RecentAppsModel.h"
#include "models/UsageStore.h"
#include "services/BatteryService.h"
#include "services/KeyboardLayoutService.h"
#include "services/NetworkService.h"
#include "services/NotificationServer.h"
#include "services/PowerService.h"
#include "services/RunService.h"
#include "services/SessionService.h"
#include "services/Settings.h"
#include "services/TrayModel.h"
#include "services/VolumeService.h"
#include "ui/ScreenGrabber.h"
#include "ui/ShellContext.h"
#include "wayland/ForeignToplevelModel.h"
#include "wayland/ToplevelModel.h"

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QStyleHints>
#include <QTimer>
#include <QtQml/qqmlextensionplugin.h>

#include <memory>

Q_IMPORT_QML_PLUGIN(Meadow_ShellPlugin)

Q_LOGGING_CATEGORY(lcShell, "meadow.shell")

namespace {
// Sample windows for --preview: enough apps to show icons, grouping and
// every button state.
void addPreviewWindows(PreviewToplevelModel *model, int count)
{
    static const QList<QPair<const char *, const char *>> samples = {
        {"My Documents", "pcmanfm-qt"},
        {"Untitled - Notepad", "org.kde.kwrite"},
        {"Meadow Help - Web Browser", "firefox"},
        {"Terminal", "xterm"},
        {"Inbox - Mail", "thunderbird"},
        {"Calculator", "org.kde.kcalc"},
        {"Downloads", "pcmanfm-qt"},
        {"Release notes - Web Browser", "firefox"},
        {"Holiday photos", "pcmanfm-qt"},
        {"Notes.txt - Notepad", "org.kde.kwrite"},
    };
    for (int i = 0; i < count; ++i) {
        const auto &s = samples.at(i % samples.size());
        const QString title = i < samples.size() ? QString::fromUtf8(s.first)
                                                 : QStringLiteral("%1 (%2)").arg(QString::fromUtf8(s.first)).arg(i / samples.size() + 1);
        model->addPreviewWindow(title, QString::fromLatin1(s.second), i == 1);
    }
}
} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("meadow-shell"));
    QGuiApplication::setOrganizationName(QStringLiteral("meadow"));
    QGuiApplication::setApplicationVersion(QStringLiteral(MEADOW_VERSION));
    // The shell is the desktop; its windows closing must not end it.
    QGuiApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Meadow desktop shell"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption previewOpt(QStringLiteral("preview"),
        QStringLiteral("Show the shell inside one normal window with sample windows (any compositor, or offscreen)."));
    const QCommandLineOption windowsOpt(QStringLiteral("preview-windows"),
        QStringLiteral("Number of sample windows in --preview (default 5)."), QStringLiteral("n"), QStringLiteral("5"));
    const QCommandLineOption sizeOpt(QStringLiteral("size"),
        QStringLiteral("--preview window size WxH (default 1024x768)."), QStringLiteral("WxH"), QStringLiteral("1024x768"));
    const QCommandLineOption openOpt(QStringLiteral("open"),
        QStringLiteral("Open <popup> at start (calendar, volume, network, language, window-menu, group, "
                       "taskbar-menu, properties, balloon)."), QStringLiteral("popup"));
    const QCommandLineOption themeOpt(QStringLiteral("theme"),
        QStringLiteral("Load the theme.json at <path> instead of the configured scheme."), QStringLiteral("path"));
    const QCommandLineOption grabOpt(QStringLiteral("grab"),
        QStringLiteral("--preview: save a screenshot to <file> after --delay ms and exit."), QStringLiteral("file"));
    const QCommandLineOption delayOpt(QStringLiteral("delay"),
        QStringLiteral("Milliseconds before --grab (default 600)."), QStringLiteral("ms"), QStringLiteral("600"));
    const QCommandLineOption exitAfterOpt(QStringLiteral("exit-after-ms"),
        QStringLiteral("Quit after <ms> milliseconds (used by smoke and screenshot tests)."), QStringLiteral("ms"));
    parser.addOptions({previewOpt, windowsOpt, sizeOpt, openOpt, themeOpt, grabOpt, delayOpt, exitAfterOpt});
    parser.process(app);

    const bool preview = parser.isSet(previewOpt);
    ThemeLoader *theme = ThemeLoader::instance();
    if (parser.isSet(themeOpt) && !theme->loadFile(parser.value(themeOpt)))
        return 1;
    if (theme->tokenMap().isEmpty()) {
        qCCritical(lcShell).noquote() << "no theme loaded:" << theme->errorString();
        return 1;
    }
    if (parser.isSet(grabOpt)) {
        theme->setReducedMotion(true);
        QGuiApplication::styleHints()->setCursorFlashTime(0);
    }

    // Keybind actions arrive over D-Bus (meadow-shellctl). A preview may run
    // next to a real shell, so failing to claim a name is never fatal.
    ShellIpc ipc;
    ipc.registerOnSessionBus();

    std::unique_ptr<ToplevelModel> windows;
    if (preview) {
        auto *model = new PreviewToplevelModel;
        addPreviewWindows(model, parser.value(windowsOpt).toInt());
        windows.reset(model);
    } else {
        windows = std::make_unique<ForeignToplevelModel>();
    }

    Settings settings;
    AppRegistry apps;
    TrayModel tray;
    NotificationServer notifications;
    notifications.setDefaultTimeout(theme->token(QStringLiteral("controls.balloon.timeoutMs")).toInt());
    notifications.registerOn(QDBusConnection::sessionBus());
    VolumeService volume;
    // Test hook: tools/mock-system-services.py serves UPower and
    // NetworkManager on the session bus.
    const QDBusConnection systemServices = qEnvironmentVariableIntValue("MEADOW_SYSTEM_SERVICES_ON_SESSION_BUS") == 1
        ? QDBusConnection::sessionBus() : QDBusConnection::systemBus();
    NetworkService network(systemServices);
    BatteryService battery(systemServices);
    KeyboardLayoutService keyboard;
    QObject::connect(&ipc, &ShellIpc::showDesktopRequested, windows.get(), &ToplevelModel::toggleShowDesktop);
    QObject::connect(&ipc, &ShellIpc::nextKeyboardLayoutRequested, &keyboard, &KeyboardLayoutService::next);

    // Start menu. Power and session actions are simulated in --preview so a
    // preview can never log out or power off the machine it runs on.
    UsageStore usage;
    AppMenuModel appMenu(&apps, &usage);
    appMenu.setNewProgramDays(theme->token(QStringLiteral("startMenu.allPrograms.newProgramDays")).toInt());
    appMenu.setMenuFile(settings.stringValue(QStringLiteral("StartMenu/menuFile")));
    RecentAppsModel startApps(&apps, &usage, &settings);
    startApps.setFrequentCount(theme->token(QStringLiteral("startMenu.left.mfuCount")).toInt());
    PlacesModel places(&settings);
    PowerService power(systemServices, preview);
    SessionService session(systemServices, preview);
    RunService run(&apps);
    ScreenGrabber screen;

    ShellContext::Services services;
    services.windows = windows.get();
    services.settings = &settings;
    services.apps = &apps;
    services.tray = &tray;
    services.notifications = &notifications;
    services.volume = &volume;
    services.network = &network;
    services.battery = &battery;
    services.keyboard = &keyboard;
    services.ipc = &ipc;
    services.appMenu = &appMenu;
    services.startApps = &startApps;
    services.places = &places;
    services.power = &power;
    services.session = &session;
    services.run = &run;
    services.screen = &screen;
    ShellContext shell(services, preview);
    QObject::connect(&ipc, &ShellIpc::toggleStartMenuRequested, &shell,
                     [&shell] { shell.setStartMenuOpen(!shell.startMenuOpen()); });

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    engine.addImageProvider(QStringLiteral("tray"), new TrayImageProvider(&tray));
    engine.addImageProvider(QStringLiteral("notification"), new NotificationImageProvider(&notifications));
    engine.addImageProvider(QStringLiteral("screen"), new ScreenImageProvider(&screen));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    if (preview) {
        const QStringList size = parser.value(sizeOpt).split(QLatin1Char('x'));
        engine.setInitialProperties({{QStringLiteral("width"), size.value(0).toInt()},
                                     {QStringLiteral("height"), size.value(1).toInt()},
                                     {QStringLiteral("openAtStart"), parser.value(openOpt)}});
        engine.loadFromModule("Meadow.Shell", "Main");
    } else {
        engine.setInitialProperties({{QStringLiteral("openAtStart"), parser.value(openOpt)}});
        engine.loadFromModule("Meadow.Shell", "Session");
    }
    if (engine.rootObjects().isEmpty())
        return 1;

    if (parser.isSet(grabOpt)) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        const QString path = parser.value(grabOpt);
        QTimer::singleShot(parser.value(delayOpt).toInt(), &app, [window, path] {
            const QImage image = window ? window->grabWindow() : QImage();
            QCoreApplication::exit(!image.isNull() && image.save(path) ? 0 : 1);
        });
    } else if (parser.isSet(exitAfterOpt)) {
        QTimer::singleShot(parser.value(exitAfterOpt).toInt(), &app, &QCoreApplication::quit);
    }

    return app.exec();
}
