// SPDX-License-Identifier: GPL-3.0-or-later
// A sunken white list with single selection (wallpaper list, time zones,
// user accounts, input languages). Rows: an optional 16 px icon and a label.
// `model` entries are strings or objects with `label` and `iconName`.
import QtQuick
import Meadow.Controls

FocusScope {
    id: list

    property var model: []
    property alias currentIndex: view.currentIndex
    readonly property alias count: view.count
    readonly property alias view: view

    signal activated(int index)

    readonly property var tk: Theme.tokens.controls.list

    implicitWidth: Theme.px(tk.defaultWidth)
    implicitHeight: Theme.px(tk.itemHeight) * tk.defaultRows + Theme.px(tk.padding) * 2

    Accessible.role: Accessible.List

    function labelOf(entry) { return typeof entry === "string" ? entry : entry.label }
    function iconOf(entry) { return typeof entry === "string" ? "" : (entry.iconName || "") }

    Rectangle {
        anchors.fill: parent
        color: list.enabled ? list.tk.background : Theme.tokens.controls.textField.disabledBackground
        border.color: list.tk.borderColor
        border.width: Theme.px(1)
    }

    ListView {
        id: view
        anchors.fill: parent
        anchors.margins: Theme.px(list.tk.padding)
        clip: true
        focus: true
        model: list.model
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationEnabled: true
        highlightMoveDuration: 0
        Keys.onReturnPressed: list.activated(currentIndex)
        Keys.onEnterPressed: list.activated(currentIndex)

        LunaScrollBar.vertical: LunaScrollBar { policy: LunaScrollBar.AsNeeded }

        delegate: Item {
            id: row
            required property var modelData
            required property int index
            readonly property bool isCurrent: ListView.isCurrentItem
            width: ListView.view.width
            height: Theme.px(list.tk.itemHeight)

            Accessible.role: Accessible.ListItem
            Accessible.name: list.labelOf(modelData)
            Accessible.selected: isCurrent

            Rectangle {
                anchors.fill: label
                anchors.leftMargin: -Theme.px(list.tk.textPadding)
                anchors.rightMargin: -Theme.px(list.tk.textPadding)
                visible: row.isCurrent
                color: list.activeFocus ? list.tk.selectionColor : list.tk.inactiveSelectionColor
            }
            Image {
                id: rowIcon
                x: Theme.px(list.tk.textPadding)
                anchors.verticalCenter: parent.verticalCenter
                visible: list.iconOf(row.modelData).length > 0
                width: visible ? Theme.px(list.tk.iconSize) : 0
                height: width
                sourceSize: Qt.size(width, height)
                source: visible ? "image://icon/" + list.iconOf(row.modelData) : ""
            }
            Text {
                id: label
                x: rowIcon.x + rowIcon.width + Theme.px(list.tk.textPadding) * 2
                width: Math.min(implicitWidth, parent.width - x - Theme.px(list.tk.textPadding))
                anchors.verticalCenter: parent.verticalCenter
                text: list.labelOf(row.modelData)
                font: Theme.fonts.ui
                elide: Text.ElideRight
                color: row.isCurrent && list.activeFocus ? list.tk.selectionTextColor : Theme.tokens.colors.windowText
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    // Focus first: users of the list tell user changes from
                    // model resets by activeFocus.
                    list.forceActiveFocus()
                    view.currentIndex = row.index
                }
                onDoubleClicked: list.activated(row.index)
            }
        }
    }
}
