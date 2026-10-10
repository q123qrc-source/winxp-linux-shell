// SPDX-License-Identifier: GPL-3.0-or-later
// "Taskbar Properties": the taskbar and notification-area options from
// shell.conf. OK applies and closes, Apply applies, Cancel discards.
// It is the body of a normal (labwc-decorated) window in a session and of a
// drawn LunaWindowFrame in --preview.
import QtQuick
import Meadow.Controls
import Meadow.Shell

Item {
    id: root

    readonly property int pad: Theme.px(Theme.tokens.dialogs.padding)
    readonly property var tk: Theme.tokens.dialogs.taskbarProperties

    // Pending values, loaded from the settings whenever the dialog opens.
    property bool locked
    property bool autoHide
    property bool group
    property bool quickLaunch
    property bool clock
    property bool hideInactive

    signal done()

    implicitWidth: Theme.px(tk.width)
    implicitHeight: column.implicitHeight + pad * 2

    function load() {
        locked = Shell.settings.taskbarLocked
        autoHide = Shell.settings.autoHide
        group = Shell.settings.groupButtons
        quickLaunch = Shell.settings.showQuickLaunch
        clock = Shell.settings.showClock
        hideInactive = Shell.settings.hideInactiveIcons
    }
    function apply() {
        Shell.settings.taskbarLocked = locked
        Shell.settings.autoHide = autoHide
        Shell.settings.groupButtons = group
        Shell.settings.showQuickLaunch = quickLaunch
        Shell.settings.showClock = clock
        Shell.settings.hideInactiveIcons = hideInactive
    }
    readonly property bool dirty: locked !== Shell.settings.taskbarLocked || autoHide !== Shell.settings.autoHide
        || group !== Shell.settings.groupButtons || quickLaunch !== Shell.settings.showQuickLaunch
        || clock !== Shell.settings.showClock || hideInactive !== Shell.settings.hideInactiveIcons

    Component.onCompleted: load()

    Keys.onReturnPressed: { apply(); done() }
    Keys.onEscapePressed: done()

    Rectangle {
        anchors.fill: parent
        color: Theme.tokens.dialogs.background
    }

    Column {
        id: column
        x: root.pad
        y: root.pad
        width: root.width - root.pad * 2
        spacing: root.pad

        LunaGroupBox {
            width: parent.width
            title: qsTr("Taskbar appearance")
            Column {
                spacing: Theme.px(root.tk.optionSpacing)
                LunaCheckBox { objectName: "option:taskbarLocked"; text: qsTr("Lock the taskbar"); checked: root.locked; onToggled: root.locked = checked }
                LunaCheckBox { objectName: "option:autoHide"; text: qsTr("Auto-hide the taskbar"); checked: root.autoHide; onToggled: root.autoHide = checked }
                LunaCheckBox { objectName: "option:groupButtons"; text: qsTr("Group similar taskbar buttons"); checked: root.group; onToggled: root.group = checked }
                LunaCheckBox { objectName: "option:showQuickLaunch"; text: qsTr("Show Quick Launch"); checked: root.quickLaunch; onToggled: root.quickLaunch = checked }
            }
        }

        LunaGroupBox {
            width: parent.width
            title: qsTr("Notification area")
            Column {
                spacing: Theme.px(root.tk.optionSpacing)
                LunaCheckBox { objectName: "option:showClock"; text: qsTr("Show the clock"); checked: root.clock; onToggled: root.clock = checked }
                LunaCheckBox { objectName: "option:hideInactiveIcons"; text: qsTr("Hide inactive icons"); checked: root.hideInactive; onToggled: root.hideInactive = checked }
            }
        }

        Row {
            anchors.right: parent.right
            spacing: Theme.px(Theme.tokens.dialogs.buttonSpacing)
            LunaButton { objectName: "dialogButton:OK"; text: qsTr("OK"); isDefault: true; onClicked: { root.apply(); root.done() } }
            LunaButton { objectName: "dialogButton:Cancel"; text: qsTr("Cancel"); onClicked: root.done() }
            LunaButton { objectName: "dialogButton:Apply"; text: qsTr("Apply"); enabled: root.dirty; onClicked: root.apply() }
        }
    }
}
