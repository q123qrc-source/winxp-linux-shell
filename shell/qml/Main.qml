// SPDX-License-Identifier: GPL-3.0-or-later
// Preview root used by `meadow-shell --preview`: one normal window showing the
// desktop color, the taskbar at the bottom and its popups, balloons and
// tooltips, driven by an in-memory window list. Runs on any compositor and on
// the offscreen platform (screenshot tests).
import QtQuick
import QtQuick.Window
import Meadow.Controls
import Meadow.Shell

Window {
    id: root

    // Popup to open at start (screenshots): see main.cpp --open.
    property string openAtStart

    visible: true
    title: qsTr("Meadow Shell Preview")
    color: Theme.tokens.desktop.background

    Desktop {
        id: desktop
        objectName: "desktop"
        anchors.fill: parent
        bottomInset: taskbar.visibleHeight
        popups: popupHost
        focus: true
    }

    Taskbar {
        id: taskbar
        objectName: "taskbar"
        width: parent.width
        y: root.height - visibleHeight
        popups: popupHost
        keepRevealed: balloons.showing
        onPropertiesRequested: properties.show()
    }

    BalloonHost {
        id: balloons
        anchors.fill: parent
        bottomInset: taskbar.visibleHeight
        targetX: taskbar.balloonTargetX
    }

    TipHost {
        anchors.fill: parent
        bottomInset: taskbar.visibleHeight
    }

    // No window manager draws this window's frame in preview, so the dialog
    // gets a drawn Luna frame.
    LunaWindowFrame {
        id: properties
        objectName: "taskbarProperties"
        visible: false
        anchors.centerIn: parent
        title: qsTr("Taskbar Properties")
        showMinimize: false
        showMaximize: false
        onCloseClicked: visible = false

        function show() {
            dialog.load()
            visible = true
            dialog.forceActiveFocus()
        }

        TaskbarProperties {
            id: dialog
            onDone: properties.visible = false
        }
    }

    LunaWindowFrame {
        id: runFrame
        objectName: "runFrame"
        visible: false
        anchors.left: parent.left
        anchors.bottom: taskbar.top
        anchors.margins: Theme.px(Theme.tokens.dialogs.padding)
        title: qsTr("Run")
        showMinimize: false
        showMaximize: false
        onCloseClicked: visible = false

        function show() {
            visible = true
            runDialog.open()
        }

        RunDialog {
            id: runDialog
            width: implicitWidth
            height: implicitHeight
            onDone: runFrame.visible = false
        }
    }

    PopupHost {
        id: popupHost
        objectName: "popupHost"
        anchors.fill: parent
        bottomInset: taskbar.visibleHeight
    }

    SystemDialogHost {
        id: systemDialogs
        objectName: "systemDialogs"
        anchors.fill: parent
        previewWindow: root
        visible: active
    }

    AltTabSwitcher {
        id: altTab
        objectName: "altTab"
        anchors.centerIn: parent
        width: implicitWidth
        height: implicitHeight
        focus: active
    }

    // Display Properties, Control Panel, applets: drawn frames in here.
    Item {
        id: windowLayer
        anchors.fill: parent
        anchors.bottomMargin: taskbar.visibleHeight
    }
    ShellWindows {
        id: shellWindows
        previewHost: windowLayer
    }

    ScreenSaverController { id: saverController }
    ScreenSaver {
        objectName: "screenSaver"
        anchors.fill: parent
        visible: saverController.active
        focus: visible
        mode: saverController.mode
        onDismissed: saverController.stop()
    }

    AuthenticateDialog {
        anchors.fill: parent
        focus: visible
    }

    // Simulated lock (SessionLock in preview): covers the preview window.
    LockScreen {
        id: lockScreen
        objectName: "lockScreen"
        anchors.fill: parent
        visible: Shell.lock.active
        focus: visible
    }

    Connections {
        target: ShellActions
        function onRunRequested() { runFrame.show() }
        function onLogOffRequested() { systemDialogs.openLogOff() }
        function onTurnOffRequested() { systemDialogs.openTurnOff() }
    }
    Connections {
        target: Shell.ipc
        function onRunDialogRequested() { ShellActions.runRequested() }
        function onLogOffDialogRequested() { ShellActions.logOffRequested() }
        function onTurnOffDialogRequested() { ShellActions.turnOffRequested() }
        function onSwitchWindowRequested(direction) { altTab.open(direction) }
    }

    // Give the first layout pass (and D-Bus replies) a moment before opening.
    Timer {
        running: root.openAtStart.length > 0
        interval: Theme.tokens.animation.slow
        onTriggered: {
            if (!ShellPreview.open(root.openAtStart, desktop))
                taskbar.openPreviewPopup(root.openAtStart)
        }
    }
}
