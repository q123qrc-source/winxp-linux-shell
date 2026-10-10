// SPDX-License-Identifier: GPL-3.0-or-later
// Session root (meadow-shell without --preview): the layer-shell surfaces.
//   taskbar   bottom edge, top layer, reserves its height (exclusive zone)
//   popups    full-screen overlay while a menu/calendar/volume popup is open;
//             a click outside the popup closes it
//   tips      full-screen, input-transparent overlay for tooltips
//   balloons  notification balloons above the notification area
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
        onTriggered: taskbar.openPreviewPopup(shell.openAtStart)
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
}
