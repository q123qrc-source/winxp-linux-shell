// SPDX-License-Identifier: GPL-3.0-or-later
// Requests for the shell's system dialogs and windows, from the Start menu,
// the desktop, the IPC (meadow-shellctl) and tests. The session and preview
// roots open them.
pragma Singleton
import QtQuick

QtObject {
    signal runRequested()
    signal logOffRequested()
    signal turnOffRequested()
    // tab: themes, desktop, screensaver, appearance, settings ("" = first).
    signal displayPropertiesRequested(string tab)
    // page: "" (categories) or display, sounds, mouse, keyboard, datetime,
    // users, network.
    signal controlPanelRequested(string page)
    // Properties sheet of a desktop item ({name, iconName, type, ...}).
    signal itemPropertiesRequested(var details)
    // Ask before emptying the Recycle Bin.
    signal emptyTrashRequested()
    // A Control Panel applet: sounds, mouse, keyboard, datetime, users,
    // network (display goes through displayPropertiesRequested). tab: the
    // applet's tab key, "" for the first.
    signal appletRequested(string name, string tab)
    // Display Properties' Preview button: show the screen saver now.
    signal screenSaverPreviewRequested(string mode)
}
