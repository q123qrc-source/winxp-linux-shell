// SPDX-License-Identifier: GPL-3.0-or-later
// The little monitor in Display Properties: a beige bezel with a power
// light on a stand, and a "screen" that shows whatever is placed inside
// (wallpaper preview, screen saver preview, a sample window).
//
//   MonitorPreview { Image { anchors.fill: parent; source: ... } }
import QtQuick
import Meadow.Controls

Item {
    id: monitor

    default property alias screenContent: screen.data
    readonly property alias screen: screen
    readonly property var tk: Theme.tokens.controls.monitor

    implicitWidth: Theme.px(tk.width)
    implicitHeight: Theme.px(tk.height)

    Accessible.role: Accessible.Graphic
    Accessible.name: qsTr("Preview")

    // Stand: a neck and a foot.
    GradientBox {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: foot.top
        width: Theme.px(monitor.tk.neckWidth)
        height: Theme.px(monitor.tk.standHeight)
        stops: monitor.tk.standGradient
        orientation: Qt.Horizontal
    }
    GradientBox {
        id: foot
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: Theme.px(monitor.tk.footWidth)
        height: Theme.px(monitor.tk.footHeight)
        radius: Theme.px(monitor.tk.footHeight) / 2
        borderWidth: Theme.px(1)
        borderColor: monitor.tk.borderColor
        stops: monitor.tk.bezelGradient
    }

    GradientBox {
        id: bezel
        width: parent.width
        height: parent.height - Theme.px(monitor.tk.standHeight) - Theme.px(monitor.tk.footHeight)
        radius: Theme.px(monitor.tk.radius)
        borderWidth: Theme.px(1)
        borderColor: monitor.tk.borderColor
        stops: monitor.tk.bezelGradient

        Rectangle {
            id: screenFrame
            x: Theme.px(monitor.tk.bezel)
            y: Theme.px(monitor.tk.bezel)
            width: parent.width - x * 2
            height: parent.height - y - Theme.px(monitor.tk.bezelBottom)
            color: monitor.tk.screenBorderColor

            Item {
                id: screen
                anchors.fill: parent
                anchors.margins: Theme.px(1)
                clip: true
            }
        }
        // Power light.
        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: Theme.px(monitor.tk.bezel) * 2
            anchors.bottom: parent.bottom
            anchors.bottomMargin: (Theme.px(monitor.tk.bezelBottom) - height) / 2
            width: Theme.px(monitor.tk.ledSize)
            height: width
            radius: width / 2
            color: monitor.tk.ledColor
        }
    }
}
