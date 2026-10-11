// SPDX-License-Identifier: GPL-3.0-or-later
// The taskbar: Start button, Quick Launch, window buttons, language bar and
// notification area on the blue Luna gradient. The same item is the content
// of the bottom layer-shell surface in a session and sits at the bottom of
// the window in --preview.
import QtQuick
import Meadow.Controls
import Meadow.Shell

Item {
    id: taskbar

    property PopupHost popups: null
    // Kept revealed regardless of auto-hide (e.g. while a balloon shows).
    property bool keepRevealed: false

    readonly property var tk: Theme.tokens.taskbar
    readonly property int fullHeight: Theme.px(tk.height)
    readonly property bool wantsHidden: Shell.settings.autoHide && !hover.hovered && !keepRevealed
                                        && !(popups && popups.active) && !Shell.startMenuOpen
    property bool autoHidden: false
    // Height that is on screen: the full bar, or the auto-hide reveal strip.
    readonly property int visibleHeight: autoHidden ? Theme.px(tk.autoHideRevealHeight) : fullHeight
    // Window x the notification balloons point at (the bar starts at x 0).
    readonly property real balloonTargetX: tray.x + tray.balloonTargetLocalX
    readonly property int spacing: Theme.px(tk.spacing)

    signal propertiesRequested()

    height: fullHeight

    Accessible.role: Accessible.ToolBar
    Accessible.name: qsTr("Taskbar")

    onWantsHiddenChanged: {
        if (wantsHidden) {
            hideTimer.restart()
        } else {
            hideTimer.stop()
            autoHidden = false
        }
    }
    Timer {
        id: hideTimer
        interval: taskbar.tk.autoHideDelayMs
        onTriggered: taskbar.autoHidden = taskbar.wantsHidden
    }

    HoverHandler {
        id: hover
    }

    Component {
        id: menuComponent
        ShellMenu {}
    }

    function openTaskbarMenu(x) {
        popups.open(menuComponent, { entries: [
            { label: qsTr("Show the Desktop"), action: () => Shell.windows.toggleShowDesktop() },
            { separator: true },
            { label: qsTr("Lock the Taskbar"), toggleType: "checkmark", checked: Shell.settings.taskbarLocked,
              action: () => Shell.settings.taskbarLocked = !Shell.settings.taskbarLocked },
            { label: qsTr("Properties"), action: () => taskbar.propertiesRequested() },
        ] }, x, false)
    }

    // Opens one popup by name (--open, screenshot and QML tests).
    function openPreviewPopup(name) {
        switch (name) {
        case "calendar": tray.openCalendar(); break
        case "volume": tray.openVolume(); break
        case "network": tray.openNetwork(); break
        case "tray-menu": tray.openFirstItemMenu(); break
        case "language": language.openMenu(); break
        case "window-menu": tasks.openWindowMenu(0); break
        case "group": tasks.openFirstGroup(); break
        case "taskbar-menu": openTaskbarMenu(taskbar.width / 2); break
        case "properties": propertiesRequested(); break
        case "start": Shell.startMenuOpen = true; break
        case "all-programs":
            Shell.startMenuOpen = true
            if (startMenu)
                startMenu.openAllPrograms(true)
            break
        case "recent":
            Shell.startMenuOpen = true
            if (startMenu) {
                startMenu.focusArea("right")
                startMenu.openRecentFromKeyboard()
            }
            break
        case "run": ShellActions.runRequested(); break
        case "logoff": ShellActions.logOffRequested(); break
        case "turnoff": ShellActions.turnOffRequested(); break
        case "lock": Shell.lock.lock(); break
        case "alttab": Shell.ipc.switchWindowRequested(1); break
        case "balloon":
            Shell.notifications.post(qsTr("Updates are ready"),
                                     qsTr("Updates are ready to install"),
                                     qsTr("Click here to review <b>3</b> updates for your computer."))
            break
        default: console.warn("unknown popup", name)
        }
    }

    // The Start menu popup follows Shell.startMenuOpen (Start button, Win
    // key, Ctrl+Esc); closing it any other way clears the flag.
    property var startMenu: null
    property bool openingStartMenu: false
    Component {
        id: startMenuComponent
        StartMenu {}
    }
    Connections {
        target: Shell
        function onStartMenuOpenChanged() {
            if (Shell.startMenuOpen && !taskbar.startMenu && taskbar.popups) {
                taskbar.openingStartMenu = true
                taskbar.startMenu = taskbar.popups.open(startMenuComponent, {}, 0, false)
                taskbar.openingStartMenu = false
            } else if (!Shell.startMenuOpen && taskbar.startMenu) {
                taskbar.popups.closeAll()
            }
        }
    }
    Connections {
        target: taskbar.popups
        function onClosed() {
            if (taskbar.openingStartMenu)
                return
            taskbar.startMenu = null
            Shell.startMenuOpen = false
        }
    }

    // Right-click on empty taskbar space.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: (event) => taskbar.openTaskbarMenu(taskbar.mapToItem(null, event.x, 0).x)
    }

    GradientBox {
        anchors.fill: parent
        stops: taskbar.tk.gradient
    }
    Rectangle {
        width: parent.width
        height: Theme.px(taskbar.tk.topEdgeHeight)
        color: taskbar.tk.topEdgeColor
    }

    StartButton {
        id: start
        objectName: "startButton"
        height: parent.height
        open: Shell.startMenuOpen
        onClicked: Shell.startMenuOpen = !Shell.startMenuOpen
    }

    Grip {
        id: quickGrip
        visible: !Shell.settings.taskbarLocked && quickLaunch.visible
        anchors.left: start.right
        anchors.leftMargin: taskbar.spacing
        height: parent.height
        width: visible ? implicitWidth : 0
    }

    QuickLaunch {
        id: quickLaunch
        visible: Shell.settings.showQuickLaunch && entries.length > 0
        anchors.left: quickGrip.right
        anchors.leftMargin: taskbar.spacing
        height: parent.height
        width: visible ? implicitWidth : 0
    }

    Grip {
        id: taskGrip
        visible: !Shell.settings.taskbarLocked
        anchors.left: quickLaunch.right
        anchors.leftMargin: taskbar.spacing
        height: parent.height
        width: visible ? implicitWidth : 0
    }

    TaskList {
        id: tasks
        popups: taskbar.popups
        anchors.left: taskGrip.right
        anchors.leftMargin: taskbar.spacing
        anchors.right: language.left
        anchors.rightMargin: taskbar.spacing
        height: parent.height
    }

    LanguageBar {
        id: language
        objectName: "languageBar"
        popups: taskbar.popups
        visible: Shell.keyboard.count > 1
        anchors.right: tray.left
        anchors.rightMargin: visible ? taskbar.spacing : 0
        height: parent.height
        width: visible ? implicitWidth : 0
    }

    NotificationArea {
        id: tray
        objectName: "notificationArea"
        popups: taskbar.popups
        anchors.right: parent.right
        height: parent.height
        width: implicitWidth
    }
}
