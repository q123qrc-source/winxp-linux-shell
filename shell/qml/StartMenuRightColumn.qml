// SPDX-License-Identifier: GPL-3.0-or-later
// Start menu right column (light blue): My Documents, My Recent Documents ▸,
// My Pictures, My Music, My Computer | Control Panel, Printers… | Help and
// Support, Search, Run…. Items whose program is missing are left out by
// PlacesModel.
import QtQuick
import Meadow.Controls
import Meadow.Shell

FocusScope {
    id: column

    property var host: null
    // The Start menu popup (submenus open beside it).
    property Item startMenu: null
    property int current: -1

    readonly property var tk: Theme.tokens.startMenu.right
    readonly property var places: Shell.places.places
    // Indexes of selectable (non-separator) rows.
    readonly property var selectable: {
        const out = []
        for (let i = 0; i < places.length; ++i)
            if (!places[i].separator)
                out.push(i)
        return out
    }
    property var recentMenu: null

    signal leaveLeft()
    signal leaveDown()

    function activate(i) {
        const p = places[i]
        if (!p || p.separator)
            return
        if (p.submenu) {
            openRecent(true)
            return
        }
        if (host)
            host.closeAll()
        if (p.key === "run")
            ShellActions.runRequested()
        else
            Shell.places.activate(p.key)
    }

    function openRecent(keyboard) {
        if (!host)
            return
        if (recentMenu)
            host.closeFrom(recentMenu)
        const docs = Shell.places.recentDocuments
        const entries = docs.length === 0
            ? [{ label: qsTr("(Empty)"), enabled: false }]
            : docs.map(d => ({ label: d.label, iconName: d.iconName, action: () => Shell.places.openDocument(d.path) }))
        const index = places.findIndex(p => p.submenu)
        const row = rows.itemAt(index)
        recentMenu = host.openChild(recentComponent, { entries: entries, current: keyboard ? 0 : -1, parentMenu: column }, 0, false,
                                    host.besidePlacer(startMenu, row.mapToItem(startMenu, 0, 0).y))
        if (!keyboard)
            column.forceActiveFocus()
    }
    // ShellMenu calls this when Left/Escape leaves the submenu.
    function closeSubmenu() {
        if (recentMenu && host)
            host.closeFrom(recentMenu)
        recentMenu = null
        column.forceActiveFocus()
    }

    function step(delta) {
        const pos = selectable.indexOf(current)
        const n = selectable.length
        if (pos < 0) {
            current = delta > 0 ? selectable[0] : selectable[n - 1]
            return
        }
        if (delta > 0 && pos === n - 1) {
            leaveDown()
            return
        }
        current = selectable[(pos + delta + n) % n]
    }
    function typeAhead(text) {
        const t = text.toLowerCase()
        const n = selectable.length
        const pos = selectable.indexOf(current)
        for (let k = 1; k <= n; ++k) {
            const i = selectable[(pos + k + n) % n]
            if (places[i].label.toLowerCase().startsWith(t)) {
                current = i
                return true
            }
        }
        return false
    }

    Keys.onUpPressed: step(-1)
    Keys.onDownPressed: step(1)
    Keys.onLeftPressed: leaveLeft()
    Keys.onRightPressed: if (places[current] && places[current].submenu) openRecent(true)
    Keys.onReturnPressed: activate(current)
    Keys.onEnterPressed: activate(current)
    Keys.onPressed: (event) => {
        if (event.text.length === 1 && event.text.trim().length === 1 && !(event.modifiers & Qt.ControlModifier))
            event.accepted = typeAhead(event.text)
    }

    Component {
        id: recentComponent
        ShellMenu {}
    }

    Rectangle {
        anchors.fill: parent
        color: column.tk.background
    }
    Rectangle {
        width: Theme.px(1)
        height: parent.height
        color: column.tk.borderLeftColor
    }

    Column {
        y: Theme.px(column.tk.padding)
        width: parent.width

        Repeater {
            id: rows
            model: column.places
            delegate: Item {
                id: row
                required property var modelData
                required property int index
                width: column.width
                height: modelData.separator ? Theme.px(column.tk.padding) * 2 + Theme.px(1) : Theme.px(column.tk.itemHeight)

                Rectangle {
                    visible: row.modelData.separator === true
                    anchors.centerIn: parent
                    width: parent.width - Theme.px(column.tk.padding) * 2
                    height: Theme.px(1)
                    color: column.tk.separatorColor
                }
                StartMenuItem {
                    objectName: "place:" + (row.modelData.key || "")
                    visible: !row.modelData.separator
                    anchors.fill: parent
                    label: row.modelData.label || ""
                    iconName: row.modelData.iconName || ""
                    bold: column.tk.boldItems && row.index < column.places.findIndex(p => p.separator)
                    hasSubmenu: row.modelData.submenu === true
                    iconSize: Theme.px(column.tk.iconSize)
                    padding: Theme.px(column.tk.padding)
                    textColor: column.tk.textColor
                    hoverBackground: column.tk.hoverBackground
                    hoverText: column.tk.hoverText
                    hot: column.current === row.index
                    onHovered: {
                        column.current = row.index
                        column.forceActiveFocus()
                        if (row.modelData.submenu)
                            recentTimer.restart()
                        else if (column.recentMenu)
                            column.closeSubmenu()
                    }
                    onClicked: column.activate(row.index)
                }
            }
        }
    }

    Timer {
        id: recentTimer
        interval: Theme.tokens.startMenu.allPrograms.hoverDelayMs
        onTriggered: column.openRecent(false)
    }
}
