// SPDX-License-Identifier: GPL-3.0-or-later
// Balloon tip: pale-yellow rounded box with a tail pointing at its anchor,
// an icon, a bold title, wrapped text and a close button. Used for tray
// notifications in the shell and for hints (wrong password, Caps Lock) in the
// greeter. Position it so the tail tip lands on the target; see tailX.
import QtQuick
import Meadow.Controls

Item {
    id: root

    property string title
    property string text
    // LunaGlyph.Info, Warning, Error, Question or None.
    property int icon: LunaGlyph.Info
    property bool tailOnTop: false
    // Tail tip position from the left edge; defaults near the right end.
    property real tailX: width - Theme.px(tk.tailWidth) * 2
    property bool closable: true
    // Hide automatically after this many ms; 0 keeps it until closed.
    property int timeout: tk.timeoutMs
    // "" or "closeHover" (gallery and screenshot tests only).
    property string previewState: ""

    readonly property var tk: Theme.tokens.controls.balloon
    readonly property int pad: Theme.px(tk.padding)
    readonly property int tail: Theme.px(tk.tailHeight)

    signal clicked()
    signal closed()

    function show() {
        visible = true
        opacity = 1
        if (timeout > 0)
            hideTimer.restart()
    }
    function close() {
        hideTimer.stop()
        opacity = 0
    }

    implicitWidth: Math.min(Theme.px(tk.maxWidth), Math.max(header.implicitWidth, body.implicitWidth) + pad * 2)
    implicitHeight: header.implicitHeight + (root.text.length > 0 ? Theme.px(tk.spacing) + body.implicitHeight : 0) + pad * 2 + tail

    Accessible.role: Accessible.AlertMessage
    Accessible.name: title
    Accessible.description: text

    Behavior on opacity {
        NumberAnimation { duration: Theme.duration("fadeMs") }
    }
    onOpacityChanged: if (opacity === 0 && visible) { visible = false; closed() }

    Timer {
        id: hideTimer
        interval: root.timeout
        onTriggered: root.close()
    }

    BalloonShape {
        anchors.fill: parent
        color: root.tk.background
        borderColor: root.tk.borderColor
        radius: Theme.px(root.tk.radius)
        tailWidth: Theme.px(root.tk.tailWidth)
        tailHeight: root.tail
        tailX: root.tailX
        tailOnTop: root.tailOnTop
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }

    Item {
        id: content
        x: root.pad
        y: root.pad + (root.tailOnTop ? root.tail : 0)
        width: root.width - root.pad * 2
        height: root.height - root.pad * 2 - root.tail

        Item {
            id: header
            readonly property real iconWidth: headerIcon.visible ? headerIcon.width + Theme.px(root.tk.spacing) : 0
            width: parent.width
            implicitWidth: iconWidth + titleText.implicitWidth + (closeButton.visible ? closeButton.width + Theme.px(root.tk.spacing) : 0)
            implicitHeight: Math.max(headerIcon.visible ? headerIcon.height : 0, titleText.height)
            height: implicitHeight

            LunaGlyph {
                id: headerIcon
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.px(root.tk.iconSize)
                height: width
                visible: root.icon !== LunaGlyph.None
                glyph: root.icon
            }
            Text {
                id: titleText
                anchors.left: parent.left
                anchors.leftMargin: header.iconWidth
                anchors.right: parent.right
                anchors.rightMargin: closeButton.visible ? closeButton.width + Theme.px(root.tk.spacing) : 0
                anchors.verticalCenter: parent.verticalCenter
                text: root.title
                font.family: Theme.fonts.ui.family
                font.pixelSize: Theme.fonts.ui.pixelSize
                font.bold: root.tk.titleBold
                color: root.tk.textColor
                wrapMode: Text.Wrap
            }
        }

        Text {
            id: body
            y: header.height + Theme.px(root.tk.spacing)
            width: parent.width
            visible: root.text.length > 0
            text: root.text
            font: Theme.fonts.ui
            color: root.tk.textColor
            wrapMode: Text.Wrap
        }

        GradientBox {
            id: closeButton
            anchors.right: parent.right
            anchors.top: parent.top
            width: Theme.px(root.tk.closeButtonSize)
            height: width
            visible: root.closable
            radius: Theme.px(2)
            readonly property bool hot: closeArea.containsMouse || root.previewState === "closeHover"
            color: hot ? Theme.tokens.controls.balloon.background : "transparent"
            borderWidth: hot ? Theme.px(1) : 0
            borderColor: root.tk.borderColor

            LunaGlyph {
                anchors.fill: parent
                anchors.margins: Theme.px(2)
                glyph: LunaGlyph.Close
                color: root.tk.textColor
                lineWidth: Theme.px(1)
            }
            MouseArea {
                id: closeArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.close()
            }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Close")
        }
    }
}
