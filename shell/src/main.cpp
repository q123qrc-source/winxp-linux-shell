// SPDX-License-Identifier: GPL-3.0-or-later
#include "Perf.h"
#include "SoundPlayer.h"
#include "ThemeLoader.h"
#include "auth/LockAuthenticator.h"
#include "ipc/ShellIpc.h"
#include "models/AppRegistry.h"
#include "models/AppMenuModel.h"
#include "models/DesktopModel.h"
#include "models/IconProvider.h"
#include "models/PlacesModel.h"
#include "models/RecentAppsModel.h"
#include "models/UsageStore.h"
#include "services/BatteryService.h"
#include "services/KeyboardLayoutService.h"
#include "services/LabwcConfig.h"
#include "services/PolkitAgent.h"
#include "services/TimeDateService.h"
#include "services/UserAccountsService.h"
#include "services/NetworkService.h"
#include "services/NotificationServer.h"
#include "services/PowerService.h"
#include "services/RunService.h"
#include "services/SessionService.h"
#include "services/ShellSounds.h"
#include "services/ToolkitThemeSync.h"
#include "services/Settings.h"
#include "services/TrayModel.h"
#include "services/VolumeService.h"
#include "ui/ScreenGrabber.h"
#include "ui/ShellContext.h"
#include "wayland/ForeignToplevelModel.h"
#include "wayland/IdleMonitor.h"
#include "wayland/OutputManager.h"
#include "wayland/SessionLock.h"
#include "wayland/ToplevelModel.h"

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
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
    IconProvider::useIconTheme(theme->token(QStringLiteral("assets.iconTheme")).toString());
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

    // The shell's 2D surfaces render in software unless configured otherwise
    // ([Performance] renderer): an OpenGL context per surface would cost more
    // than the whole memory budget (docs/PERFORMANCE.md). Applications are
    // not affected.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND")) {
        const QString renderer = settings.stringValue(QStringLiteral("Performance/renderer"), QStringLiteral("software"));
        if (renderer == QLatin1String("software"))
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
        else if (renderer != QLatin1String("opengl"))
            qCWarning(lcShell) << "unknown [Performance] renderer" << renderer << "- using the default";
    }

    // Appearance: the saved color scheme and font size, unless a scheme was
    // forced (--theme, or MEADOW_THEME as the tests do).
    if (!parser.isSet(themeOpt) && qEnvironmentVariableIsEmpty("MEADOW_THEME")) {
        const QString scheme = settings.stringValue(QStringLiteral("Appearance/scheme"), QStringLiteral("luna-blue"));
        if (scheme != theme->schemeId() && !theme->loadScheme(scheme))
            qCWarning(lcShell) << "unknown color scheme" << scheme;
    }
    auto applyFontSize = [&] {
        const bool large = settings.stringValue(QStringLiteral("Appearance/fontSize")) == QLatin1String("large");
        theme->setScale(large ? theme->token(QStringLiteral("scale.large")).toReal() : 1.0);
    };
    if (!parser.isSet(grabOpt))
        applyFontSize();
    // Sounds on or off ([Sounds] enabled), for every sound player at once.
    auto applySounds = [&] {
        SoundPlayer::setAllMuted(!settings.boolSetting(QStringLiteral("Sounds/enabled"), true));
    };
    if (!parser.isSet(grabOpt))
        applySounds();

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
    AppMenuModel appMenu(&apps, &usage, theme->token(QStringLiteral("startMenu.allPrograms.newProgramDays")).toInt(),
                         settings.stringValue(QStringLiteral("StartMenu/menuFile")));
    RecentAppsModel startApps(&apps, &usage, &settings);
    startApps.setFrequentCount(theme->token(QStringLiteral("startMenu.left.mfuCount")).toInt());
    PlacesModel places(&settings);
    DesktopModel desktop(&settings, &places, &apps);
    PowerService power(systemServices, preview);
    SessionService session(systemServices, preview);
    RunService run(&apps);
    ShellSounds sounds(&session, &power);
    QObject::connect(&notifications, &NotificationServer::soundRequested, &sounds, &ShellSounds::notify);
    ScreenGrabber screen;

    // Compositor-side settings (decoration theme, mouse, keyboard) through
    // the session's labwc overlay; written once at startup so the saved
    // color scheme's decorations come back.
    LabwcConfig labwc(&settings);
    // GTK and Kvantum themes for the applications the session starts (not
    // in a preview, which must not touch the real session).
    std::unique_ptr<ToolkitThemeSync> toolkitThemes;
    if (!preview) {
        labwc.apply();
        toolkitThemes = std::make_unique<ToolkitThemeSync>();
        toolkitThemes->apply();
    }
    QObject::connect(&settings, &Settings::changed, &app, [&] {
        if (!parser.isSet(grabOpt)) {
            applyFontSize();
            applySounds();
        }
    });
    OutputManager outputs(preview);
    IdleMonitor idle(preview);
    PolkitAgent polkit(preview);
    TimeDateService timeDate(systemServices, preview);
    UserAccountsService accounts(systemServices, preview);
    if (!preview)
        polkit.registerForSession();

    // Lock screen. Locking is simulated in --preview (the lock screen then
    // shows inside the preview window and accepts the password "meadow").
    SessionLock lock(preview);
    LockAuthenticator lockAuth(preview);
    QObject::connect(&ipc, &ShellIpc::lockRequested, &lock, &SessionLock::lock);
    QObject::connect(&session, &SessionService::lockRequested, &lock, &SessionLock::lock);
    // `loginctl unlock-session`: logind accepts it only from the session's own
    // user or an administrator (polkit), so it is the recovery path from a
    // text console when the lock screen cannot unlock (as GNOME and KDE do).
    QObject::connect(&session, &SessionService::unlockRequested, &lock, [&lock] {
        if (!lock.isActive())
            return;
        qCInfo(lcShell) << "logind Unlock: unlocking";
        lock.unlock();
    });
    session.setLockBeforeSleep(settings.boolSetting(QStringLiteral("ScreenSaver/lockOnSuspend"), true));
    QObject::connect(&settings, &Settings::changed, &session, [&] {
        session.setLockBeforeSleep(settings.boolSetting(QStringLiteral("ScreenSaver/lockOnSuspend"), true));
    });
    QObject::connect(&session, &SessionService::aboutToSleep, &lock, [&] {
        if (settings.boolSetting(QStringLiteral("ScreenSaver/lockOnSuspend"), true) && !lock.lock())
            session.sleepReady();
        else if (lock.isLocked())
            session.sleepReady();
    });
    QObject::connect(&lock, &SessionLock::lockFailed, &session, &SessionService::sleepReady);
    QObject::connect(&lock, &SessionLock::stateChanged, &app, [&] {
        ipc.setLocked(lock.isLocked());
        session.setLockedHint(lock.isLocked());
        if (lock.isLocked())
            session.sleepReady();
        if (!lock.isActive())
            lockAuth.cancel();
    });

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
    services.lock = &lock;
    services.labwc = &labwc;
    services.outputs = &outputs;
    services.idle = &idle;
    services.polkit = &polkit;
    services.timeDate = &timeDate;
    services.accounts = &accounts;
    services.desktop = &desktop;
    services.lockAuth = &lockAuth;
    services.sounds = &sounds;
    ShellContext shell(services, preview);
    Perf::mark("shell", QStringLiteral("services-ready"));
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
    if (Perf::enabled()) {
        Perf::mark("shell", QStringLiteral("qml-loaded"));
        const QString first = preview ? QString() : QStringLiteral("meadow-taskbar");
        for (QWindow *w : QGuiApplication::topLevelWindows()) {
            if (auto *qw = qobject_cast<QQuickWindow *>(w); qw && (first.isEmpty() || w->title() == first)) {
                Perf::markNextFrame(qw, "shell", QStringLiteral("first-frame"));
                break;
            }
        }
    }
    // The lock windows exist now; lock again if a crashed shell left the
    // session locked.
    if (!preview) {
        lock.relockIfNeeded();
        sounds.sessionStarted();
    }

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
