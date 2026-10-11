// SPDX-License-Identifier: GPL-3.0-or-later
// Session root (meadow-shell without --preview): the layer-shell surfaces.
//   desktop   background layer on every screen: wallpaper, plus the icons
//             on the taskbar's screen
//   taskbar   bottom edge, top layer, reserves its height (exclusive zone)
//   popups    full-screen overlay while a menu/calendar/volume popup is open;
//             a click outside the popup closes it
//   tips      full-screen, input-transparent overlay for tooltips
//   balloons  notification balloons above the notification area
//   lock      one ext-session-lock surface per screen while locked
//             (SessionLock shows them; the compositor shows nothing else)
// Taskbar Properties is an ordinary window that labwc decorates.
import QtQuick
import QtQuick.Window
import Meadow.Controls
import Meadow.Shell

QtObject {
    id: shell

    // Popup to open at start (session screenshots): see main.cpp --open.
    property string openAtStart

    property Timer openTimer: Timer {
        running: shell.openAtStart.length > 0
        interval: Theme.tokens.animation.slow
        onTriggered: shell.openPreview(shell.openAtStart)
    }
    // --open targets added in phase 8, then the taskbar's own.
    function openPreview(name) {
        if (!ShellPreview.open(name))
            taskbar.openPreviewPopup(name)
    }

    // Display Properties, Control Panel and its applets, Properties sheets.
    property ShellWindows shellWindows: ShellWindows {}

    property ScreenSaverController saverController: ScreenSaverController {}

    // The screen saver: full screen above everything but the lock, taking
    // the keyboard so the first key ends it.
    property Window saverWindow: Window {
        id: saverWindow
        title: "meadow-screensaver"
        color: Theme.tokens.screenSaver.background
        visible: shell.saverController.active

        ShellSurface {
            window: saverWindow
            scope: "meadow-screensaver"
            layer: ShellSurface.LayerOverlay
            edges: ShellSurface.AnchorTop | ShellSurface.AnchorBottom | ShellSurface.AnchorLeft | ShellSurface.AnchorRight
            exclusiveZone: -1
            keyboard: ShellSurface.KeyboardExclusive
        }

        ScreenSaver {
            anchors.fill: parent
            focus: true
            mode: shell.saverController.mode
            running: saverWindow.visible
            onDismissed: shell.saverController.stop()
        }
    }

    // polkit's Authenticate dialog over a dimmed screen.
    property Window authWindow: Window {
        id: authWindow
        title: "meadow-authenticate"
        color: "transparent"
        visible: Shell.polkit.active

        ShellSurface {
            window: authWindow
            scope: "meadow-authenticate"
            layer: ShellSurface.LayerOverlay
            edges: ShellSurface.AnchorTop | ShellSurface.AnchorBottom | ShellSurface.AnchorLeft | ShellSurface.AnchorRight
            exclusiveZone: -1
            keyboard: ShellSurface.KeyboardExclusive
        }

        AuthenticateDialog {
            anchors.fill: parent
            focus: true
        }
    }

    property Window taskbarWindow: Window {
        id: barWindow
        title: "meadow-taskbar"
        color: "transparent"
        height: taskbar.visibleHeight
        visible: true

        ShellSurface {
            window: barWindow
            scope: "meadow-taskbar"
            layer: ShellSurface.LayerTop
            edges: ShellSurface.AnchorBottom | ShellSurface.AnchorLeft | ShellSurface.AnchorRight
            exclusiveZone: Shell.settings.autoHide ? 0 : taskbar.fullHeight
        }

        Taskbar {
            id: taskbar
            width: parent.width
            popups: popupHost
            keepRevealed: balloons.showing
            onPropertiesRequested: shell.propertiesWindow.open()
        }
    }

    property Window popupWindow: Window {
        id: popupWindow
        title: "meadow-popups"
        color: "transparent"
        visible: popupHost.active

        ShellSurface {
            window: popupWindow
            scope: "meadow-popups"
            layer: ShellSurface.LayerOverlay
            edges: ShellSurface.AnchorTop | ShellSurface.AnchorBottom | ShellSurface.AnchorLeft | ShellSurface.AnchorRight
            exclusiveZone: -1
            keyboard: ShellSurface.KeyboardExclusive
        }

        PopupHost {
            id: popupHost
            anchors.fill: parent
            bottomInset: taskbar.visibleHeight
            focus: true
        }
    }

    property Window tipWindow: Window {
        id: tipWindow
        title: "meadow-tips"
        color: "transparent"
        flags: Qt.WindowTransparentForInput
        visible: tipHost.shown

        ShellSurface {
            window: tipWindow
            scope: "meadow-tips"
            layer: ShellSurface.LayerOverlay
            edges: ShellSurface.AnchorTop | ShellSurface.AnchorBottom | ShellSurface.AnchorLeft | ShellSurface.AnchorRight
            exclusiveZone: -1
        }

        TipHost {
            id: tipHost
            anchors.fill: parent
            bottomInset: taskbar.visibleHeight
        }
    }

    property Window balloonWindow: Window {
        id: balloonWindow
        title: "meadow-balloons"
        color: "transparent"
        width: Math.max(1, balloons.balloonWidth)
        height: Math.max(1, balloons.balloonHeight)
        visible: balloons.showing

        ShellSurface {
            window: balloonWindow
            scope: "meadow-balloons"
            layer: ShellSurface.LayerOverlay
            edges: ShellSurface.AnchorBottom | ShellSurface.AnchorRight
            marginBottom: taskbar.visibleHeight
        }

        BalloonHost {
            id: balloons
            anchors.fill: parent
            // The window hugs the right screen edge; translate the target.
            targetX: taskbar.balloonTargetX - (barWindow.width - balloonWindow.width)
        }
    }

    property Window propertiesWindow: Window {
        id: propertiesWindow
        title: qsTr("Taskbar Properties")
        width: dialog.implicitWidth
        height: dialog.implicitHeight
        // Fixed size, like XP property sheets.
        minimumWidth: dialog.implicitWidth
        maximumWidth: dialog.implicitWidth
        minimumHeight: dialog.implicitHeight
        maximumHeight: dialog.implicitHeight
        color: Theme.tokens.dialogs.background
        visible: false

        function open() {
            dialog.load()
            visible = true
            requestActivate()
        }

        TaskbarProperties {
            id: dialog
            anchors.fill: parent
            focus: true
            onDone: propertiesWindow.visible = false
        }
    }

    property Window systemDialogWindow: Window {
        id: systemDialogWindow
        title: "meadow-system-dialogs"
        color: "transparent"
        visible: systemDialogs.active

        ShellSurface {
            window: systemDialogWindow
            scope: "meadow-system-dialogs"
            layer: ShellSurface.LayerOverlay
            edges: ShellSurface.AnchorTop | ShellSurface.AnchorBottom | ShellSurface.AnchorLeft | ShellSurface.AnchorRight
            exclusiveZone: -1
            keyboard: ShellSurface.KeyboardExclusive
        }

        SystemDialogHost {
            id: systemDialogs
            anchors.fill: parent
            outputName: barWindow.screen ? barWindow.screen.name : ""
            focus: true
        }
    }

    property Window runWindow: Window {
        id: runWindow
        title: qsTr("Run")
        width: run.implicitWidth
        height: run.implicitHeight
        minimumWidth: run.implicitWidth
        maximumWidth: run.implicitWidth
        minimumHeight: run.implicitHeight
        maximumHeight: run.implicitHeight
        color: Theme.tokens.dialogs.background
        visible: false

        function open() {
            visible = true
            requestActivate()
            run.open()
        }

        RunDialog {
            id: run
            anchors.fill: parent
            onDone: runWindow.visible = false
        }
    }

    // Alt+Tab: a centered overlay that takes the keyboard while it is active
    // (it must see Alt being released).
    property Window altTabWindow: Window {
        id: altTabWindow
        title: "meadow-alttab"
        color: "transparent"
        width: Math.max(1, altTab.implicitWidth + Theme.px(Theme.tokens.altTab.shadowOffset))
        height: Math.max(1, altTab.implicitHeight + Theme.px(Theme.tokens.altTab.shadowOffset))
        visible: altTab.active

        ShellSurface {
            window: altTabWindow
            scope: "meadow-alttab"
            layer: ShellSurface.LayerOverlay
            edges: 0 // centered
            keyboard: ShellSurface.KeyboardExclusive
        }

        AltTabSwitcher {
            id: altTab
            width: implicitWidth
            height: implicitHeight
            focus: true
        }
        onActiveChanged: if (active) altTab.keyboardArrived()
    }

    // The desktop on every screen (background layer, below all windows).
    // Exclusive zone 0: the compositor keeps it clear of the taskbar.
    property Instantiator desktopWindows: Instantiator {
        model: Qt.application.screens
        delegate: Window {
            id: desktopWindow
            required property var modelData
            readonly property bool primary: !!barWindow.screen && modelData.name === barWindow.screen.name
            title: "meadow-desktop"
            screen: modelData
            color: Theme.tokens.desktop.background
            visible: true

            ShellSurface {
                window: desktopWindow
                scope: "meadow-desktop"
                layer: ShellSurface.LayerBackground
                edges: ShellSurface.AnchorTop | ShellSurface.AnchorBottom | ShellSurface.AnchorLeft | ShellSurface.AnchorRight
                exclusiveZone: 0
                keyboard: ShellSurface.KeyboardOnDemand
            }

            Desktop {
                anchors.fill: parent
                focus: true
                showIcons: desktopWindow.primary
                popups: popupHost
            }
        }
    }

    // Lock windows: created hidden for every screen and handed to
    // Shell.lock, which turns them into lock surfaces when the session locks.
    property Instantiator lockWindows: Instantiator {
        model: Qt.application.screens
        delegate: Window {
            id: lockWindow
            required property var modelData
            title: "meadow-lock"
            screen: modelData
            color: Theme.tokens.welcome.background
            visible: false

            LockScreen {
                anchors.fill: parent
                focus: true
                // Screen objects from Qt.application.screens are wrappers; compare names.
                primary: !!barWindow.screen && lockWindow.modelData.name === barWindow.screen.name
            }

            Component.onCompleted: Shell.lock.addSurface(lockWindow)
            Component.onDestruction: Shell.lock.removeSurface(lockWindow)
        }
    }

    property Connections actions: Connections {
        target: ShellActions
        function onRunRequested() { runWindow.open() }
        function onLogOffRequested() { systemDialogs.openLogOff() }
        function onTurnOffRequested() { systemDialogs.openTurnOff() }
    }
    property Connections ipc: Connections {
        target: Shell.ipc
        function onRunDialogRequested() { ShellActions.runRequested() }
        function onLogOffDialogRequested() { ShellActions.logOffRequested() }
        function onTurnOffDialogRequested() { ShellActions.turnOffRequested() }
        function onSwitchWindowRequested(direction) { altTab.open(direction) }
    }
}
