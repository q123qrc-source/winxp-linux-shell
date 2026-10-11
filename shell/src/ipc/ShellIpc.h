// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

// org.meadow.Shell on the session bus (shell/dbus/org.meadow.Shell.xml).
// Each D-Bus method re-emits as a Qt signal for the QML that implements it;
// signals without a receiver yet (phases 7-8) are only logged.
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
    void NextKeyboardLayout();
    void SwitchWindow(int direction);
    void ShowControlPanel(const QString &page);
    void ShowDisplayProperties(const QString &tab);
    bool IsLocked() const { return m_locked; }
    QString Version() const;

    // Set by the shell when the lock state changes; emits LockedChanged.
    void setLocked(bool locked);

Q_SIGNALS:
    void toggleStartMenuRequested();
    void runDialogRequested();
    void showDesktopRequested();
    void lockRequested();
    void logOffDialogRequested();
    void turnOffDialogRequested();
    void nextKeyboardLayoutRequested();
    void switchWindowRequested(int direction);
    void controlPanelRequested(const QString &page);
    void displayPropertiesRequested(const QString &tab);
    // D-Bus signal (relayed by the adaptor).
    void LockedChanged(bool locked);

private:
    bool m_locked = false;
};
