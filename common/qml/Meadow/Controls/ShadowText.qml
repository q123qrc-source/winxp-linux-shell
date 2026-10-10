// SPDX-License-Identifier: GPL-3.0-or-later
// Text with a hard drop shadow (offset copy), as used for the start label,
// title bars and the Start menu user name.
import QtQuick

Item {
    id: root

    property alias text: label.text
    property alias font: label.font
    property alias color: label.color
    property alias elide: label.elide
    property alias horizontalAlignment: label.horizontalAlignment
    property color shadowColor: "transparent"
    property real shadowOffset: 1

    implicitWidth: label.implicitWidth + shadowOffset
    implicitHeight: label.implicitHeight + shadowOffset

    Text {
        x: root.shadowOffset
        y: root.shadowOffset
        width: label.width
        height: label.height
        visible: root.shadowColor.a > 0
        text: label.text
        font: label.font
        color: root.shadowColor
        elide: label.elide
        horizontalAlignment: label.horizontalAlignment
        verticalAlignment: label.verticalAlignment
    }

    Text {
        id: label
        width: root.width - root.shadowOffset
        height: root.height - root.shadowOffset
        verticalAlignment: Text.AlignVCenter
    }
}
