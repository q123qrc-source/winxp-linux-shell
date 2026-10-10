// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

// org.meadow.Shell on the session bus (shell/dbus/org.meadow.Shell.xml).
// Each D-Bus method re-emits as a Qt signal for the QML that implements it;
// until those parts exist (phases 6-8) calls are logged.
class ShellIpc : public QObject
{
    Q_OBJECT

public:
    static constexpr const char *kService = "org.meadow.Shell";
    static constexpr const char *kPath = "/org/meadow/Shell";

    explicit ShellIpc(QObject *parent = nullptr);

    // Claims the bus name and exports the object. Returns false (and logs)
    // if the bus is unavailable or another shell already owns the name.
    bool registerOnSessionBus();

    // D-Bus methods (called by the generated adaptor).
    void ToggleStartMenu();
    void ShowRunDialog();
    void ShowDesktop();
    void Lock();
    void ShowLogOffDialog();
    void ShowTurnOffDialog();
    QString Version() const;

Q_SIGNALS:
    void toggleStartMenuRequested();
    void runDialogRequested();
    void showDesktopRequested();
    void lockRequested();
    void logOffDialogRequested();
    void turnOffDialogRequested();
};
