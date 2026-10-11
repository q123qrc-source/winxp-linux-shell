// SPDX-License-Identifier: GPL-3.0-or-later
// Group box: a thin rounded frame with its title set into the top edge, in
// the dialog's accent blue.
import QtQuick
import Meadow.Controls

Item {
    id: root

    property string title
    default property alias content: body.data
    // Behind the title; set it to the surface the box sits on (tab panes).
    property color titleBackground: Theme.tokens.dialogs.background

    readonly property var tk: Theme.tokens.controls.groupBox
    readonly property int pad: Theme.px(tk.padding)

    implicitWidth: Math.max(body.childrenRect.width, titleText.implicitWidth + Theme.px(tk.titleInset) * 2) + pad * 2
    implicitHeight: titleText.implicitHeight + body.childrenRect.height + pad * 2

    Accessible.role: Accessible.Grouping
    Accessible.name: title

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: titleText.implicitHeight / 2
        color: "transparent"
        radius: Theme.px(root.tk.radius)
        border.width: Theme.px(1)
        border.color: root.tk.borderColor
    }

    // The title interrupts the frame line.
    Rectangle {
        x: Theme.px(root.tk.titleInset)
        width: titleText.implicitWidth + Theme.px(2) * 2
        height: titleText.implicitHeight
        color: root.titleBackground
        Text {
            id: titleText
            anchors.centerIn: parent
            text: root.title
            font: Theme.fonts.ui
            color: root.tk.titleColor
        }
    }

    Item {
        id: body
        x: root.pad
        y: titleText.implicitHeight + root.pad / 2
        width: root.width - root.pad * 2
        height: childrenRect.height
    }
}
