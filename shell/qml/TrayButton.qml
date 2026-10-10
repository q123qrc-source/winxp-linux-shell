// SPDX-License-Identifier: GPL-3.0-or-later
// A 16px notification-area icon slot: hover tooltip, clicks and the wheel.
import QtQuick
import Meadow.Controls

Item {
    id: root

    property string tipText
    default property alias content: holder.data

    signal clicked(int button)
    signal doubleClicked()
    signal wheel(int delta, bool horizontal)

    implicitWidth: Theme.px(Theme.tokens.tray.iconSize)
    implicitHeight: implicitWidth

    Accessible.role: Accessible.Button
    Accessible.name: tipText
    Accessible.onPressAction: root.clicked(Qt.LeftButton)

    Item {
        id: holder
        anchors.fill: parent
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        onClicked: (event) => root.clicked(event.button)
        onDoubleClicked: (event) => { if (event.button === Qt.LeftButton) root.doubleClicked() }
        onWheel: (event) => {
            const horizontal = event.angleDelta.x !== 0 && event.angleDelta.y === 0
            root.wheel(horizontal ? event.angleDelta.x : event.angleDelta.y, horizontal)
        }
        onContainsMouseChanged: containsMouse && root.tipText.length > 0 ? Tips.show(root, root.tipText) : Tips.hide(root)
    }
}
