// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Meadow.Controls

Column {
    id: section
    property string title
    default property alias items: row.data

    spacing: Theme.px(6)
    width: parent ? parent.width : implicitWidth

    Text {
        text: section.title
        font: Theme.fonts.menuItemBold
        color: Theme.tokens.colors.accentDark
    }
    GradientBox {
        width: parent.width
        height: Theme.px(1)
        color: Theme.tokens.controls.menu.separatorColor
    }
    Flow {
        id: row
        width: parent.width
        spacing: Theme.px(12)
    }
}
