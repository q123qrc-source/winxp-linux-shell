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
    QML_NAMED_ELEMENT(Shell)
    QML_SINGLETON
    QML_UNCREATABLE("Created by meadow-shell")

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
