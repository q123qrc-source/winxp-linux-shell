// SPDX-License-Identifier: GPL-3.0-or-later
// The green "start" button: glossy, square on the left and rounded on the
// right, with the Meadow logo and an italic shadowed label. It looks pressed
// while the Start menu is open.
import QtQuick
import Meadow.Controls

Item {
    id: root

    property bool open: false
    // "" or "hover" / "pressed" (screenshots).
    property string previewState: ""

    readonly property var tk: Theme.tokens.startButton
    readonly property bool pressedLook: open || mouse.pressed || previewState === "pressed"
    readonly property bool hoverLook: !pressedLook && (mouse.containsMouse || previewState === "hover")

    signal clicked()

    implicitWidth: Theme.px(tk.width)
    implicitHeight: Theme.px(tk.height)

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Start")
    Accessible.description: qsTr("Open the Start menu")
    Accessible.onPressAction: root.clicked()

    GlossyGradient {
        anchors.fill: parent
        stops: root.pressedLook ? root.tk.gradient.pressed : root.hoverLook ? root.tk.gradient.hover : root.tk.gradient.normal
        glossColor: root.pressedLook ? "transparent" : root.tk.highlightColor
        topLeftRadius: 0
        bottomLeftRadius: 0
        topRightRadius: Theme.px(root.tk.cornerRadiusRight)
        bottomRightRadius: Theme.px(root.tk.cornerRadiusRight)
        borderWidth: Theme.px(1)
        borderColor: root.tk.borderColor
    }

    Logo {
        id: logo
        x: Theme.px(root.tk.logoMarginLeft) + (root.pressedLook ? Theme.px(1) : 0)
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: root.pressedLook ? Theme.px(1) : 0
        width: Theme.px(root.tk.logoSize)
        height: width
    }

    ShadowText {
        anchors.left: logo.right
        anchors.leftMargin: Theme.px(root.tk.labelSpacing)
        anchors.verticalCenter: logo.verticalCenter
        text: root.tk.label
        font.family: Theme.fonts.start.family
        font.pixelSize: Theme.fonts.start.pixelSize
        font.bold: Theme.fonts.start.bold
        font.italic: Theme.fonts.start.italic
        color: root.tk.labelColor
        shadowColor: root.tk.labelShadowColor
        shadowOffset: Theme.px(root.tk.labelShadowOffset)
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.clicked()
    }
}
