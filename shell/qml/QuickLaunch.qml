// SPDX-License-Identifier: GPL-3.0-or-later
// Quick Launch: small launcher icons next to the Start button. Entries come
// from shell.conf ([Taskbar] quickLaunch): desktop file ids, or
// "meadow:show-desktop" for Show Desktop. Missing applications are skipped.
import QtQuick
import Meadow.Controls
import Meadow.Shell

Row {
    id: root

    readonly property var tk: Theme.tokens.quickLaunch
    readonly property var entries: Shell.settings.quickLaunch.filter(
        id => id === "meadow:show-desktop" || Shell.apps.hasDesktopId(id))

    spacing: 0

    Accessible.role: Accessible.ToolBar
    Accessible.name: qsTr("Quick Launch")

    Repeater {
        model: root.entries

        delegate: Item {
            id: button
            required property string modelData
            objectName: "quickLaunch:" + modelData
            readonly property bool isShowDesktop: modelData === "meadow:show-desktop"
            readonly property string label: isShowDesktop ? qsTr("Show Desktop") : Shell.apps.nameForDesktopId(modelData)

            width: Theme.px(root.tk.iconSize) + Theme.px(root.tk.itemPadding) * 2
            height: root.height

            Accessible.role: Accessible.Button
            Accessible.name: label
            Accessible.onPressAction: activate()

            function activate() {
                if (isShowDesktop)
                    Shell.windows.toggleShowDesktop()
                else
                    Shell.apps.launch(modelData)
            }

            // XP draws a thin raised (hover) or sunken (pressed) frame.
            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: Theme.px(root.tk.iconSize) + Theme.px(root.tk.itemPadding) * 2
                visible: mouse.containsMouse
                color: "transparent"
                border.width: Theme.px(1)
                border.color: mouse.pressed ? Theme.tokens.taskButton.activeBorderColor : root.tk.gripDotHighlight
            }

            TrayGlyph {
                anchors.centerIn: parent
                width: Theme.px(root.tk.iconSize)
                height: width
                visible: button.isShowDesktop
                kind: TrayGlyph.ShowDesktop
            }
            Image {
                anchors.centerIn: parent
                width: Theme.px(root.tk.iconSize)
                height: width
                sourceSize: Qt.size(width, height)
                visible: !button.isShowDesktop
                source: button.isShowDesktop ? "" : "image://icon/" + Shell.apps.iconForDesktopId(button.modelData)
            }

            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: button.activate()
                onContainsMouseChanged: containsMouse ? Tips.show(button, button.label) : Tips.hide(button)
            }
        }
    }
}
