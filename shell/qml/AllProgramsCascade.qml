// SPDX-License-Identifier: GPL-3.0-or-later
// All Programs: a ShellMenu with the Start menu's cascade metrics. Folders
// open after a short hover; newly installed programs (and their folders)
// are highlighted; programs can be dragged onto the pinned list.
import QtQuick
import Meadow.Controls

ShellMenu {
    id: cascade
    objectName: "allProgramsMenu"

    readonly property var ak: Theme.tokens.startMenu.allPrograms

    itemHeight: Theme.px(ak.itemHeight)
    iconSize: Theme.px(ak.iconSize)
    background: ak.background
    borderColor: ak.borderColor
    highlightColor: ak.newProgramHighlight
    widthMin: Theme.px(ak.minWidth)
    widthMax: Theme.px(ak.maxWidth)
    dragEnabled: true
    submenuProperties: ({
        itemHeight: itemHeight, iconSize: iconSize, background: background, borderColor: borderColor,
        highlightColor: highlightColor, widthMin: widthMin, widthMax: widthMax
    })
}
