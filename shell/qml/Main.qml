// SPDX-License-Identifier: GPL-3.0-or-later
// Preview root used by `meadow-shell --preview`: a plain window that shows the
// shell components without layer-shell. Real surfaces are created in phase 6.
import QtQuick
import QtQuick.Window
import Meadow.Controls

Window {
    id: root
    width: 1024
    height: 768
    visible: true
    title: qsTr("Meadow Shell Preview")
    color: Theme.tokens.desktop.background

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Theme.tokens.taskbar.height
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.tokens.taskbar.gradient[0].color }
            GradientStop { position: 1.0; color: Theme.tokens.taskbar.gradient[Theme.tokens.taskbar.gradient.length - 1].color }
        }
        Accessible.role: Accessible.ToolBar
        Accessible.name: qsTr("Taskbar")
    }
}
