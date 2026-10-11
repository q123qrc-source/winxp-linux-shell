// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QObject>
#include <QtQml/qqmlregistration.h>

Q_MOC_INCLUDE("ipc/ShellIpc.h")
Q_MOC_INCLUDE("models/AppRegistry.h")
Q_MOC_INCLUDE("services/BatteryService.h")
Q_MOC_INCLUDE("services/KeyboardLayoutService.h")
Q_MOC_INCLUDE("services/NetworkService.h")
Q_MOC_INCLUDE("services/NotificationServer.h")
Q_MOC_INCLUDE("services/Settings.h")
Q_MOC_INCLUDE("services/TrayModel.h")
Q_MOC_INCLUDE("services/VolumeService.h")
Q_MOC_INCLUDE("wayland/ToplevelModel.h")
Q_MOC_INCLUDE("models/AppMenuModel.h")
Q_MOC_INCLUDE("models/RecentAppsModel.h")
Q_MOC_INCLUDE("models/PlacesModel.h")
Q_MOC_INCLUDE("services/PowerService.h")
Q_MOC_INCLUDE("services/SessionService.h")
Q_MOC_INCLUDE("services/RunService.h")
Q_MOC_INCLUDE("ui/ScreenGrabber.h")
Q_MOC_INCLUDE("services/UserAccountsService.h")
Q_MOC_INCLUDE("services/TimeDateService.h")
Q_MOC_INCLUDE("services/PolkitAgent.h")
Q_MOC_INCLUDE("wayland/IdleMonitor.h")
Q_MOC_INCLUDE("wayland/OutputManager.h")
Q_MOC_INCLUDE("services/LabwcConfig.h")
Q_MOC_INCLUDE("models/DesktopModel.h")
Q_MOC_INCLUDE("auth/LockAuthenticator.h")
Q_MOC_INCLUDE("wayland/SessionLock.h")
Q_MOC_INCLUDE("services/ShellSounds.h")

class AppMenuModel;
class RecentAppsModel;
class PlacesModel;
class PowerService;
class SessionService;
class RunService;
class ScreenGrabber;
class UserAccountsService;
class TimeDateService;
class PolkitAgent;
class IdleMonitor;
class OutputManager;
class LabwcConfig;
class DesktopModel;
class LockAuthenticator;
class SessionLock;
class ShellSounds;

class AppRegistry;
class BatteryService;
class KeyboardLayoutService;
class NetworkService;
class NotificationServer;
class QQmlEngine;
class QJSEngine;
class QTimer;
class Settings;
class ShellIpc;
class ToplevelModel;
class TrayModel;
class VolumeService;

// Everything the shell's QML talks to, as the `Shell` singleton. main.cpp
// creates one instance (real compositor-backed services, or the in-memory
// preview window list) before loading QML.
class ShellContext : public QObject
{
    Q_OBJECT
    // No QML_UNCREATABLE: since Qt 6.12 it also stops create() from
    // providing the singleton.
    QML_NAMED_ELEMENT(Shell)
    QML_SINGLETON

    Q_PROPERTY(bool preview READ preview CONSTANT)
    Q_PROPERTY(ToplevelModel *windows READ windows CONSTANT)
    Q_PROPERTY(Settings *settings READ settings CONSTANT)
    Q_PROPERTY(AppRegistry *apps READ apps CONSTANT)
    Q_PROPERTY(TrayModel *tray READ tray CONSTANT)
    Q_PROPERTY(NotificationServer *notifications READ notifications CONSTANT)
    Q_PROPERTY(VolumeService *volume READ volume CONSTANT)
    Q_PROPERTY(NetworkService *network READ network CONSTANT)
    Q_PROPERTY(BatteryService *battery READ battery CONSTANT)
    Q_PROPERTY(KeyboardLayoutService *keyboard READ keyboard CONSTANT)
    Q_PROPERTY(ShellIpc *ipc READ ipc CONSTANT)
    Q_PROPERTY(AppMenuModel *appMenu READ appMenu CONSTANT)
    Q_PROPERTY(RecentAppsModel *startApps READ startApps CONSTANT)
    Q_PROPERTY(PlacesModel *places READ places CONSTANT)
    Q_PROPERTY(PowerService *power READ power CONSTANT)
    Q_PROPERTY(SessionService *session READ session CONSTANT)
    Q_PROPERTY(RunService *run READ run CONSTANT)
    Q_PROPERTY(ScreenGrabber *screen READ screen CONSTANT)
    Q_PROPERTY(UserAccountsService *accounts READ accounts CONSTANT)
    Q_PROPERTY(TimeDateService *timeDate READ timeDate CONSTANT)
    Q_PROPERTY(PolkitAgent *polkit READ polkit CONSTANT)
    Q_PROPERTY(IdleMonitor *idle READ idle CONSTANT)
    Q_PROPERTY(OutputManager *outputs READ outputs CONSTANT)
    Q_PROPERTY(LabwcConfig *labwc READ labwc CONSTANT)
    Q_PROPERTY(DesktopModel *desktop READ desktop CONSTANT)
    Q_PROPERTY(LockAuthenticator *lockAuth READ lockAuth CONSTANT)
    Q_PROPERTY(SessionLock *lock READ lock CONSTANT)
    Q_PROPERTY(ShellSounds *sounds READ sounds CONSTANT)
    // Login name, for the Start menu header.
    Q_PROPERTY(QString userName READ userName CONSTANT)
    // Wall clock, updated at each minute boundary; fixed when
    // MEADOW_FAKE_TIME (ISO 8601) is set, for screenshots.
    Q_PROPERTY(QDateTime now READ now NOTIFY nowChanged)
    // The Start menu itself arrives in phase 7; the button already toggles this.
    Q_PROPERTY(bool startMenuOpen READ startMenuOpen WRITE setStartMenuOpen NOTIFY startMenuOpenChanged)

public:
    struct Services {
        ToplevelModel *windows = nullptr;
        Settings *settings = nullptr;
        AppRegistry *apps = nullptr;
        TrayModel *tray = nullptr;
        NotificationServer *notifications = nullptr;
        VolumeService *volume = nullptr;
        NetworkService *network = nullptr;
        BatteryService *battery = nullptr;
        KeyboardLayoutService *keyboard = nullptr;
        ShellIpc *ipc = nullptr;
        AppMenuModel *appMenu = nullptr;
        RecentAppsModel *startApps = nullptr;
        PlacesModel *places = nullptr;
        PowerService *power = nullptr;
        SessionService *session = nullptr;
        RunService *run = nullptr;
        ScreenGrabber *screen = nullptr;
        UserAccountsService *accounts = nullptr;
        TimeDateService *timeDate = nullptr;
        PolkitAgent *polkit = nullptr;
        IdleMonitor *idle = nullptr;
        OutputManager *outputs = nullptr;
        LabwcConfig *labwc = nullptr;
        DesktopModel *desktop = nullptr;
        LockAuthenticator *lockAuth = nullptr;
        SessionLock *lock = nullptr;
        ShellSounds *sounds = nullptr;
    };

    ShellContext(const Services &services, bool preview, QObject *parent = nullptr);
    ~ShellContext() override;

    static ShellContext *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static ShellContext *instance() { return s_instance; }

    bool preview() const { return m_preview; }
    ToplevelModel *windows() const { return m_s.windows; }
    Settings *settings() const { return m_s.settings; }
    AppRegistry *apps() const { return m_s.apps; }
    TrayModel *tray() const { return m_s.tray; }
    NotificationServer *notifications() const { return m_s.notifications; }
    VolumeService *volume() const { return m_s.volume; }
    NetworkService *network() const { return m_s.network; }
    BatteryService *battery() const { return m_s.battery; }
    KeyboardLayoutService *keyboard() const { return m_s.keyboard; }
    ShellIpc *ipc() const { return m_s.ipc; }
    AppMenuModel *appMenu() const { return m_s.appMenu; }
    RecentAppsModel *startApps() const { return m_s.startApps; }
    PlacesModel *places() const { return m_s.places; }
    PowerService *power() const { return m_s.power; }
    SessionService *session() const { return m_s.session; }
    RunService *run() const { return m_s.run; }
    ScreenGrabber *screen() const { return m_s.screen; }
    UserAccountsService *accounts() const { return m_s.accounts; }
    TimeDateService *timeDate() const { return m_s.timeDate; }
    PolkitAgent *polkit() const { return m_s.polkit; }
    IdleMonitor *idle() const { return m_s.idle; }
    OutputManager *outputs() const { return m_s.outputs; }
    LabwcConfig *labwc() const { return m_s.labwc; }
    DesktopModel *desktop() const { return m_s.desktop; }
    LockAuthenticator *lockAuth() const { return m_s.lockAuth; }
    SessionLock *lock() const { return m_s.lock; }
    ShellSounds *sounds() const { return m_s.sounds; }
    static QString userName();

    // Modifiers held right now (Alt+Tab checks whether Alt is still down
    // when its surface gets keyboard focus).
    Q_INVOKABLE int keyboardModifiers() const;

    QDateTime now() const { return m_now; }
    bool startMenuOpen() const { return m_startMenuOpen; }
    void setStartMenuOpen(bool open);

Q_SIGNALS:
    void nowChanged();
    void startMenuOpenChanged();

private:
    void tick();

    static ShellContext *s_instance;
    Services m_s;
    bool m_preview;
    bool m_fakeTime = false;
    bool m_startMenuOpen = false;
    QDateTime m_now;
    QTimer *m_clock;
};
