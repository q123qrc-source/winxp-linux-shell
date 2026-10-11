// SPDX-License-Identifier: GPL-3.0-or-later
// The Alt+Tab window switcher, XP style: a small dialog-colored panel with a
// grid of window icons (most recently used first, at most altTab.columns per
// row) and the selected window's title in a sunken box underneath.
//
// Alt+Tab (labwc keybind -> meadow-shellctl switch-window next) opens it with
// the previous window selected. While Alt is held, Tab / Shift+Tab and the
// arrow keys move the selection and Escape cancels; releasing Alt switches.
// A quick Alt+Tab (Alt already released when the switcher gets the keyboard)
// switches straight to the previous window without showing anything. A click
// on an icon switches to it.
import QtQuick
import Meadow.Controls
import Meadow.Shell

FocusScope {
    id: switcher

    // Rows of Shell.windows, most recently used first.
    property var rows: []
    property int selected: 0
    // True from open() until it switches or is cancelled.
    property bool active: false
    // Shown once the keyboard confirms Alt is still held (or in preview).
    property bool shown: false
    // Preview/tests: never commit on its own (no real Alt key).
    property bool holdOpen: Shell.preview

    readonly property var tk: Theme.tokens.altTab
    readonly property int columns: Math.max(1, Math.min(tk.columns, rows.length))
    readonly property int cell: Theme.px(tk.cellSize)
    readonly property int pad: Theme.px(tk.padding)
    readonly property bool rtl: Qt.application.layoutDirection === Qt.RightToLeft

    signal finished()

    implicitWidth: columns * cell + pad * 2
    implicitHeight: Math.ceil(rows.length / columns) * cell + pad * 2
                    + Theme.px(tk.titleSpacing) + Theme.px(tk.titleHeight)
    visible: shown

    Accessible.role: Accessible.List
    Accessible.name: qsTr("Switch between windows")

    LoggingCategory {
        id: lcAltTab
        name: "meadow.shell.alttab"
        defaultLogLevel: LoggingCategory.Info
    }

    function open(direction) {
        if (active) {
            step(direction)
            return
        }
        const r = Shell.windows.mruRows()
        if (r.length === 0)
            return
        rows = r
        // The previous window (next), or the least recently used (previous).
        selected = r.length > 1 ? (direction < 0 ? r.length - 1 : 1) : 0
        active = true
        shown = holdOpen
        console.info(lcAltTab, "opened with", r.length, "windows")
        focusTimer.restart()
    }

    function step(delta) {
        if (!active || rows.length === 0)
            return
        selected = (selected + delta + rows.length) % rows.length
    }
    function moveRow(delta) {
        if (!active)
            return
        const next = selected + delta * columns
        if (next >= 0 && next < rows.length)
            selected = next
    }

    function commit() {
        if (!active)
            return
        const row = rows[selected]
        close()
        // The app id only: titles can name private documents.
        console.info(lcAltTab, "switching to", Shell.windows.get(row).appId)
        Shell.windows.activate(row)
    }
    function close() {
        focusTimer.stop()
        active = false
        shown = false
        finished()
    }

    // Called when the switcher's surface gets the keyboard. The compositor
    // sends the held modifiers right after the keyboard focus, so look one
    // event-loop turn later.
    function keyboardArrived() {
        if (!active || holdOpen)
            return
        focusTimer.stop()
        Qt.callLater(checkAlt)
    }
    function checkAlt() {
        if (!active)
            return
        if (Shell.keyboardModifiers() & Qt.AltModifier)
            shown = true
        else
            commit() // Alt was already released: a quick Alt+Tab
    }
    // If the keyboard never arrives (compositor refused), switch anyway.
    Timer {
        id: focusTimer
        interval: switcher.tk.focusTimeoutMs
        onTriggered: if (switcher.active && !switcher.holdOpen) switcher.commit()
    }

    Keys.onPressed: (event) => {
        if (!active)
            return
        const back = event.modifiers & Qt.ShiftModifier
        switch (event.key) {
        case Qt.Key_Tab: step(1); break
        case Qt.Key_Backtab: step(-1); break
        case Qt.Key_Right: step(rtl ? -1 : 1); break
        case Qt.Key_Left: step(rtl ? 1 : -1); break
        case Qt.Key_Down: moveRow(1); break
        case Qt.Key_Up: moveRow(-1); break
        case Qt.Key_Return:
        case Qt.Key_Enter: commit(); break
        case Qt.Key_Escape: close(); break
        default: return
        }
        event.accepted = true
    }
    Keys.onReleased: (event) => {
        if (active && !holdOpen && (event.key === Qt.Key_Alt || event.key === Qt.Key_Meta)) {
            event.accepted = true
            commit()
        }
    }

    // --- Look -------------------------------------------------------------

    Rectangle {
        x: Theme.px(switcher.tk.shadowOffset)
        y: Theme.px(switcher.tk.shadowOffset)
        width: parent.width
        height: parent.height
        radius: Theme.px(switcher.tk.radius)
        color: switcher.tk.shadowColor
    }
    Rectangle {
        anchors.fill: parent
        radius: Theme.px(switcher.tk.radius)
        color: switcher.tk.background
        border.color: switcher.tk.borderColor
        border.width: Theme.px(switcher.tk.borderWidth)
    }

    Grid {
        id: grid
        x: switcher.pad
        y: switcher.pad
        columns: switcher.columns
        layoutDirection: switcher.rtl ? Qt.RightToLeft : Qt.LeftToRight

        Repeater {
            model: switcher.rows
            delegate: Item {
                id: cellItem
                required property int modelData
                required property int index
                readonly property var win: Shell.windows.get(modelData)
                readonly property bool isSelected: index === switcher.selected
                width: switcher.cell
                height: switcher.cell

                Accessible.role: Accessible.ListItem
                Accessible.name: win.title
                Accessible.selected: isSelected

                Rectangle {
                    anchors.fill: parent
                    visible: cellItem.isSelected
                    color: "transparent"
                    radius: Theme.px(switcher.tk.selectionRadius)
                    border.color: switcher.tk.selectionColor
                    border.width: Theme.px(switcher.tk.selectionBorderWidth)
                }
                Image {
                    anchors.centerIn: parent
                    width: Theme.px(switcher.tk.iconSize)
                    height: width
                    sourceSize: Qt.size(width, height)
                    source: "image://icon/" + (Shell.apps.iconForApp(cellItem.win.appId) || "application-x-executable")
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        switcher.selected = cellItem.index
                        switcher.commit()
                    }
                }
            }
        }
    }

    // Title of the selected window, in a sunken white box.
    Rectangle {
        x: switcher.pad
        y: grid.y + grid.height + Theme.px(switcher.tk.titleSpacing)
        width: parent.width - switcher.pad * 2
        height: Theme.px(switcher.tk.titleHeight)
        color: switcher.tk.titleBackground
        border.color: switcher.tk.titleBorderColor
        border.width: Theme.px(1)

        Text {
            anchors.fill: parent
            anchors.leftMargin: Theme.px(switcher.tk.titlePadding)
            anchors.rightMargin: Theme.px(switcher.tk.titlePadding)
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            font: Theme.fonts.ui
            color: switcher.tk.titleTextColor
            text: switcher.active && switcher.selected < switcher.rows.length
                  ? Shell.windows.get(switcher.rows[switcher.selected]).title : ""
        }
    }
}
