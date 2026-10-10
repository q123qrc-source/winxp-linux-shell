// SPDX-License-Identifier: GPL-3.0-or-later
// Taskbar clock: time in clock.format, the full date as its tooltip, and a
// click opens the calendar.
import QtQuick
import Meadow.Controls
import Meadow.Shell

Item {
    id: root

    readonly property var tk: Theme.tokens.clock

    signal clicked()

    implicitWidth: Theme.px(tk.paddingLeft) + label.implicitWidth

    Accessible.role: Accessible.Clock
    Accessible.name: label.text
    Accessible.description: Qt.locale().toString(Shell.now, tk.tooltipFormat)
    Accessible.onPressAction: root.clicked()

    Text {
        id: label
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        text: Qt.locale().toString(Shell.now, root.tk.format)
        font: Theme.fonts.ui
        color: root.tk.textColor
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.clicked()
        onContainsMouseChanged: containsMouse ? Tips.show(root, root.Accessible.description) : Tips.hide(root)
    }
}
