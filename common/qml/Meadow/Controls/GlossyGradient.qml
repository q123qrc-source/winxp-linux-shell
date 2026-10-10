// SPDX-License-Identifier: GPL-3.0-or-later
// A theme gradient with an optional glossy highlight over its top part, used
// for the start button, task buttons and title bars.
import QtQuick
import Meadow.Controls

GradientBox {
    id: root

    // Color of the highlight band; transparent disables it.
    property color glossColor: "transparent"
    // Fraction of the height covered by the highlight.
    property real glossFraction: 0.5

    GradientBox {
        visible: root.glossColor.a > 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: root.borderWidth
        height: (root.height - root.borderWidth * 2) * root.glossFraction
        color: root.glossColor
        topLeftRadius: Math.max(0, (root.topLeftRadius >= 0 ? root.topLeftRadius : root.radius) - root.borderWidth)
        topRightRadius: Math.max(0, (root.topRightRadius >= 0 ? root.topRightRadius : root.radius) - root.borderWidth)
        bottomLeftRadius: 0
        bottomRightRadius: 0
    }
}
