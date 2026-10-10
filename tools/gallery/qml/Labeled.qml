// SPDX-License-Identifier: GPL-3.0-or-later
// A gallery cell: the control on top and its state name below.
import QtQuick
import Meadow.Controls

Column {
    id: cell
    property string label
    default property alias control: holder.data

    spacing: Theme.px(3)

    Item {
        id: holder
        width: childrenRect.width
        height: childrenRect.height
    }
    Text {
        text: cell.label
        font: Theme.fonts.small
        color: Theme.tokens.colors.disabledText
    }
}
