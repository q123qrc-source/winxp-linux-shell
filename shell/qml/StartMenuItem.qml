// SPDX-License-Identifier: GPL-3.0-or-later
// One row of the Start menu: icon, label (optionally bold, with a gray
// subtitle under it) and an optional submenu arrow. Highlighted in selection
// blue when hot (hover or keyboard).
import QtQuick
import Meadow.Controls

Item {
    id: row

    property string label
    property string subtitle
    property string iconName
    property int glyph: LunaGlyph.None
    property bool bold: false
    property bool hot: false
    property bool hasSubmenu: false
    property int iconSize: Theme.px(Theme.tokens.startMenu.left.iconSize)
    property int padding: Theme.px(Theme.tokens.startMenu.left.padding)
    property color textColor: Theme.tokens.colors.windowText
    property color hoverBackground: Theme.tokens.startMenu.left.hoverBackground
    property color hoverText: Theme.tokens.startMenu.left.hoverText
    property color subtitleColor: Theme.tokens.startMenu.left.subtitleColor
    property color arrowColor: textColor

    signal clicked()
    signal rightClicked(real x, real y)
    signal hovered()

    implicitWidth: padding * 3 + iconSize // theme-ok: left, middle and right padding + Math.max(labelText.implicitWidth, subtitleText.implicitWidth)
                   + (hasSubmenu ? Theme.px(Theme.tokens.startMenu.right.submenuArrowSize) + padding : 0)

    Accessible.role: hasSubmenu ? Accessible.MenuItem : Accessible.Button
    Accessible.name: label
    Accessible.description: subtitle
    Accessible.onPressAction: row.clicked()

    Rectangle {
        anchors.fill: parent
        visible: row.hot
        color: row.hoverBackground
    }

    Image {
        id: icon
        x: row.padding
        anchors.verticalCenter: parent.verticalCenter
        width: row.iconSize
        height: width
        sourceSize: Qt.size(width, height)
        visible: row.glyph === LunaGlyph.None
        source: row.iconName.length > 0 ? "image://icon/" + row.iconName : ""
    }
    LunaGlyph {
        anchors.fill: icon
        visible: row.glyph !== LunaGlyph.None
        glyph: row.glyph
        color: row.hot ? row.hoverText : row.textColor
    }

    Column {
        anchors.left: icon.right
        anchors.leftMargin: row.padding
        anchors.right: arrow.visible ? arrow.left : parent.right
        anchors.rightMargin: row.padding
        anchors.verticalCenter: parent.verticalCenter

        Text {
            id: labelText
            width: parent.width
            text: row.label
            elide: Text.ElideRight
            font: row.bold ? Theme.fonts.menuItemBold : Theme.fonts.ui
            color: row.hot ? row.hoverText : row.textColor
        }
        Text {
            id: subtitleText
            width: parent.width
            visible: row.subtitle.length > 0
            text: row.subtitle
            elide: Text.ElideRight
            font: Theme.fonts.ui
            color: row.hot ? row.hoverText : row.subtitleColor
        }
    }

    LunaGlyph {
        id: arrow
        visible: row.hasSubmenu
        anchors.right: parent.right
        anchors.rightMargin: row.padding
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.px(Theme.tokens.startMenu.right.submenuArrowSize)
        height: width
        glyph: LunaGlyph.ArrowRight
        color: row.hot ? row.hoverText : row.arrowColor
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onEntered: row.hovered()
        onClicked: (event) => {
            if (event.button === Qt.RightButton) {
                const p = row.mapToItem(null, event.x, event.y)
                row.rightClicked(p.x, p.y)
            } else {
                row.clicked()
            }
        }
    }
}
