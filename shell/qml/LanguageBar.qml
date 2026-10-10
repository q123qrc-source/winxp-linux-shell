// SPDX-License-Identifier: GPL-3.0-or-later
// Language bar: a small box with the active layout's language code ("EN").
// Clicking lists the layouts; Alt+Shift cycles them (meadow-shellctl
// next-layout). Shown only when more than one layout is configured.
import QtQuick
import Meadow.Controls
import Meadow.Shell

Item {
    id: root

    property PopupHost popups: null
    readonly property var tk: Theme.tokens.languageBar

    implicitWidth: Theme.px(tk.width)

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Input language: %1").arg(Shell.keyboard.currentName)
    Accessible.onPressAction: openMenu()

    function openMenu() {
        const entries = Shell.keyboard.layouts.map((l, i) => ({
            label: l.shortName + "  " + l.name, toggleType: "radio", checked: i === 0,
            action: () => Shell.keyboard.select(i)
        }))
        popups.open(menuComponent, { entries: entries }, root.mapToItem(null, root.width, 0).x, true)
    }

    Component {
        id: menuComponent
        ShellMenu {}
    }

    Rectangle {
        anchors.centerIn: parent
        width: parent.width
        height: Theme.px(root.tk.height)
        color: mouse.pressed ? root.tk.border : root.tk.background
        border.width: Theme.px(1)
        border.color: root.tk.border

        Text {
            anchors.centerIn: parent
            text: Shell.keyboard.currentShortName
            font: Theme.fonts.small
            color: root.tk.textColor
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.openMenu()
        onContainsMouseChanged: containsMouse ? Tips.show(root, Shell.keyboard.currentName) : Tips.hide(root)
    }
}
