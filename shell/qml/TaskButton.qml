// SPDX-License-Identifier: GPL-3.0-or-later
// One taskbar button: app icon and elided title on a glossy blue gradient.
// The focused window's button looks pressed; a window asking for attention
// flashes orange a few times and then stays orange. A group button (several
// windows of one app) shows the window count and the application name.
import QtQuick
import Meadow.Controls
import Meadow.Shell

Item {
    id: root

    property string title
    property string iconSource
    property bool active: false
    property bool needsAttention: false
    // > 1 for a group button.
    property int groupCount: 0
    // "" or "hover" / "pressed" (screenshots).
    property string previewState: ""

    readonly property var tk: Theme.tokens.taskButton
    readonly property bool pressedLook: active || mouse.pressed || previewState === "pressed"
    readonly property bool hoverLook: !pressedLook && (mouse.containsMouse || previewState === "hover")
    property bool flashOn: false

    signal clicked()
    signal middleClicked()
    signal contextMenuRequested(real x)

    implicitHeight: Theme.px(tk.height)

    Accessible.role: Accessible.Button
    Accessible.name: groupCount > 1 ? qsTr("%1 (%n windows)", "", groupCount).arg(title) : title
    Accessible.checkable: true
    Accessible.checked: active
    Accessible.onPressAction: root.clicked()

    // Attention: flash attentionPulseCount times, then stay highlighted.
    Timer {
        id: flash
        property int remaining: 0
        interval: root.tk.attentionPulseMs
        repeat: true
        onTriggered: {
            root.flashOn = !root.flashOn
            if (--remaining <= 0) {
                stop()
                root.flashOn = true
            }
        }
    }
    onNeedsAttentionChanged: {
        if (needsAttention && !Theme.reducedMotion) {
            flash.remaining = root.tk.attentionPulseCount * 2
            flashOn = true
            flash.restart()
        } else {
            flash.stop()
            flashOn = needsAttention
        }
    }
    Component.onCompleted: if (needsAttention) flashOn = true

    GlossyGradient {
        anchors.fill: parent
        stops: root.flashOn && !root.active ? root.tk.gradient.attention
             : root.pressedLook ? root.tk.gradient.active
             : root.hoverLook ? root.tk.gradient.hover : root.tk.gradient.normal
        radius: Theme.px(root.tk.radius)
        borderWidth: Theme.px(1)
        borderColor: root.pressedLook ? root.tk.activeBorderColor : root.tk.borderColor
    }

    Image {
        id: icon
        x: Theme.px(root.tk.padding) + (root.pressedLook ? Theme.px(1) : 0)
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: root.pressedLook ? Theme.px(1) : 0
        width: Theme.px(root.tk.iconSize)
        height: width
        sourceSize: Qt.size(width, height)
        source: root.iconSource
    }

    Text {
        anchors.left: icon.right
        anchors.leftMargin: Theme.px(root.tk.padding)
        anchors.right: groupArrow.visible ? groupArrow.left : parent.right
        anchors.rightMargin: Theme.px(root.tk.padding)
        anchors.verticalCenter: icon.verticalCenter
        visible: width > 0
        text: root.groupCount > 1 ? root.groupCount + " " + root.title : root.title
        elide: Text.ElideRight
        font: Theme.fonts.ui
        color: root.tk.textColor
    }

    LunaGlyph {
        id: groupArrow
        visible: root.groupCount > 1
        anchors.right: parent.right
        anchors.rightMargin: Theme.px(root.tk.padding)
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.px(root.tk.groupArrowSize)
        height: width
        glyph: LunaGlyph.ArrowDown
        color: root.tk.textColor
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        onClicked: (event) => {
            if (event.button === Qt.MiddleButton)
                root.middleClicked()
            else if (event.button === Qt.RightButton)
                root.contextMenuRequested(event.x)
            else
                root.clicked()
        }
        onContainsMouseChanged: containsMouse ? Tips.show(root, root.title) : Tips.hide(root)
    }
}
