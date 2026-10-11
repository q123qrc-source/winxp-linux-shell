// SPDX-License-Identifier: GPL-3.0-or-later
// Runs the screen saver from shell.conf [ScreenSaver]: after `timeoutMin`
// idle minutes (Shell.idle) it becomes `active` with the chosen `mode`; the
// first input ends it, and with "On resume, password protect" the session
// locks first, so ending the saver reveals the lock screen rather than the
// desktop. The Display Properties preview runs it without locking.
// The view (ScreenSaver.qml on an overlay surface, or over the preview
// window) shows while `active`.
import QtQuick
import Meadow.Controls
import Meadow.Shell

QtObject {
    id: controller

    property bool active: false
    property string mode: "logo"
    // A preview (Display Properties) never locks.
    property bool previewing: false

    readonly property string saver: { Shell.settings.revision; return Shell.settings.stringValue("ScreenSaver/saver", "logo") }
    readonly property int minutes: { Shell.settings.revision; return Shell.settings.intSetting("ScreenSaver/timeoutMin", Theme.tokens.screenSaver.defaultMinutes) }
    readonly property bool passwordProtect: { Shell.settings.revision; return Shell.settings.boolSetting("ScreenSaver/passwordProtect", true) }

    property LoggingCategory log: LoggingCategory {
        name: "meadow.shell.idle"
        defaultLogLevel: LoggingCategory.Info
    }

    // Idle notifications only while a screen saver is chosen.
    property Binding timeout: Binding {
        target: Shell.idle
        property: "timeoutMs"
        value: controller.saver === "none" ? 0 : controller.minutes * 60000 // theme-ok: minutes to milliseconds
    }

    function start(m, preview) {
        if (Shell.lock.active)
            return
        mode = m
        previewing = preview
        active = true
        console.info(log, "screen saver started", preview ? "(preview)" : "")
    }
    function stop() {
        if (!active)
            return
        if (!previewing && passwordProtect)
            Shell.lock.lock() // first, so ending the saver shows the lock screen
        active = false
        previewing = false
        console.info(log, "screen saver ended")
    }

    property Connections idle: Connections {
        target: Shell.idle
        function onIdleChanged() {
            if (Shell.idle.idle && controller.saver !== "none")
                controller.start(controller.saver, false)
            else if (!Shell.idle.idle && controller.active && !controller.previewing)
                controller.stop()
        }
    }
    property Connections actions: Connections {
        target: ShellActions
        function onScreenSaverPreviewRequested(m) { controller.start(m, true) }
    }
}
