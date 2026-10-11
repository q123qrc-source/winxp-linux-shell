// SPDX-License-Identifier: GPL-3.0-or-later
// `meadow-shell --open NAME` targets added in phase 8 (screenshots, tests):
//   display[:TAB]  control-panel[:PAGE]  applet:NAME[:TAB]  authenticate
//   screensaver  desktop-menu  item-menu  alttab  lock
// Returns false for names the taskbar handles (Taskbar.openPreviewPopup).
pragma Singleton
import QtQuick
import Meadow.Shell

QtObject {
    function open(name, desktop) {
        const parts = name.split(":")
        switch (parts[0]) {
        case "display": ShellActions.displayPropertiesRequested(parts[1] || ""); return true
        case "control-panel": ShellActions.controlPanelRequested(parts[1] || ""); return true
        case "applet": ShellActions.appletRequested(parts[1] || "", parts[2] || ""); return true
        case "authenticate": Shell.polkit.requestPreview(); return true
        case "screensaver": ShellActions.screenSaverPreviewRequested(parts[1] || "logo"); return true
        case "desktop-menu":
            if (desktop)
                desktop.openPreviewMenu(false)
            return true
        case "item-menu":
            if (desktop)
                desktop.openPreviewMenu(true)
            return true
        default: return false
        }
    }
}
