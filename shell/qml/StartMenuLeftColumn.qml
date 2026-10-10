// SPDX-License-Identifier: GPL-3.0-or-later
// Start menu left column (white): pinned items with bold names (Internet and
// E-mail show the program as a gray subtitle), a separator, the most
// frequently used programs, and "All Programs" at the bottom.
//
// Pinning: right-click "Pin to Start menu" / "Unpin from Start menu", or drag
// a program from All Programs (or a pinned item, to reorder) onto the
// pinned area. "Remove from This List" hides a frequent program until it is
// used again.
import QtQuick
import Meadow.Controls
import Meadow.Shell

FocusScope {
    id: column

    property var host: null
    // Index into `rows` of the keyboard/hover selection, -1 for none.
    property int current: -1

    readonly property var tk: Theme.tokens.startMenu.left
    readonly property var pinned: Shell.startApps.pinned
    readonly property var frequent: Shell.startApps.frequent
    // Selectable rows in order: pinned, frequent, then All Programs.
    readonly property int allProgramsIndex: pinned.length + frequent.length
    readonly property int rowCount: allProgramsIndex + 1
    // Where a drop would insert into the pinned list, -1 when not dragging over.
    property int dropIndex: -1

    readonly property alias allProgramsItem: allPrograms

    signal allProgramsRequested(bool keyboard)
    signal leaveRight()

    function entryAt(i) {
        return i < pinned.length ? pinned[i] : (i < allProgramsIndex ? frequent[i - pinned.length] : null)
    }
    function rowItem(i) {
        return i < pinned.length ? pinnedRows.itemAt(i)
             : (i < allProgramsIndex ? frequentRows.itemAt(i - pinned.length) : allPrograms)
    }
    function activate(i) {
        if (i === allProgramsIndex) {
            allProgramsRequested(true)
            return
        }
        const e = entryAt(i)
        if (!e)
            return
        Shell.startApps.activate(e.id)
        if (host)
            host.closeAll()
    }
    function step(delta) {
        current = current < 0 ? (delta > 0 ? 0 : rowCount - 1) : (current + delta + rowCount) % rowCount
    }
    function typeAhead(text) {
        const t = text.toLowerCase()
        for (let k = 1; k <= rowCount; ++k) {
            const i = ((current < 0 ? -1 : current) + k + rowCount) % rowCount
            const label = i === allProgramsIndex ? qsTr("All Programs") : entryAt(i).label
            if (label.toLowerCase().startsWith(t)) {
                current = i
                return true
            }
        }
        return false
    }

    // Context menu for a program row, kept inside the Start menu.
    function openContextMenu(entry, x, y, isFrequent) {
        if (!host)
            return
        const pinnedNow = Shell.startApps.isPinned(entry.id)
        const items = [
            { label: qsTr("Open"), bold: true, action: () => column.activateEntry(entry) },
            { separator: true },
            pinnedNow ? { label: qsTr("Unpin from Start menu"), action: () => Shell.startApps.unpin(entry.id) }
                      : { label: qsTr("Pin to Start menu"), action: () => Shell.startApps.pin(entry.id) },
        ]
        if (isFrequent)
            items.push({ label: qsTr("Remove from This List"), action: () => Shell.startApps.removeFromFrequent(entry.id) })
        host.openChild(contextComponent, { entries: items }, 0, false, function(popup) {
            popup.x = Math.min(x, host.width - popup.width)
            popup.y = Math.min(y, host.height - host.bottomInset - popup.height)
        })
    }
    function activateEntry(entry) {
        Shell.startApps.activate(entry.id)
        if (host)
            host.closeAll()
    }

    width: Theme.px(tk.width)

    Keys.onUpPressed: step(-1)
    Keys.onDownPressed: step(1)
    Keys.onRightPressed: (event) => {
        if (current === allProgramsIndex)
            allProgramsRequested(true)
        else
            leaveRight()
    }
    Keys.onReturnPressed: activate(current)
    Keys.onEnterPressed: activate(current)
    Keys.onPressed: (event) => {
        if (event.text.length === 1 && event.text.trim().length === 1 && !(event.modifiers & Qt.ControlModifier))
            event.accepted = typeAhead(event.text)
    }

    Component {
        id: contextComponent
        // Choosing an item closes only this context menu, not the Start menu.
        ShellMenu { keepParent: true }
    }

    Rectangle {
        anchors.fill: parent
        color: column.tk.background
    }

    Column {
        id: programColumn
        y: Theme.px(column.tk.padding)
        width: parent.width

        Item {
            id: pinnedArea
            objectName: "pinnedArea"
            width: parent.width
            height: pinnedColumn.height

            Column {
                id: pinnedColumn
                width: parent.width
                Repeater {
                    id: pinnedRows
                    model: column.pinned
                    delegate: StartMenuItem {
                        id: pinnedRow
                        required property var modelData
                        required property int index
                        objectName: "pinned:" + modelData.id
                        width: pinnedColumn.width
                        height: Theme.px(column.tk.itemHeight)
                        label: modelData.label
                        subtitle: modelData.subtitle
                        iconName: modelData.iconName
                        bold: true
                        hot: column.current === index
                        onHovered: { column.current = index; column.forceActiveFocus() }
                        onClicked: column.activate(index)
                        onRightClicked: (x, y) => column.openContextMenu(modelData, x, y, false)

                        // Drag a pinned item to reorder.
                        DragHandler {
                            target: null
                            enabled: !!column.host
                            onActiveChanged: active ? column.host.beginDrag(
                                                          Object.assign({ pinId: pinnedRow.modelData.id }, pinnedRow.modelData),
                                                          centroid.scenePosition)
                                                    : column.host.endDrag()
                            onCentroidChanged: if (active) column.host.moveDrag(centroid.scenePosition)
                        }
                    }
                }
            }

            // Insertion line while dragging over the pinned area.
            Rectangle {
                visible: column.dropIndex >= 0
                x: Theme.px(column.tk.padding)
                width: parent.width - x * 2
                height: Theme.px(Theme.tokens.startMenu.dropIndicatorHeight)
                y: column.dropIndex * Theme.px(column.tk.itemHeight) - height / 2
                color: Theme.tokens.startMenu.dropIndicatorColor
            }

            DropArea {
                // Also accept drops just below the last pinned item.
                anchors.fill: parent
                anchors.bottomMargin: -Theme.px(column.tk.itemHeight)
                keys: ["meadow/app"]
                function indexAt(y) {
                    return Math.max(0, Math.min(column.pinned.length, Math.round(y / Theme.px(column.tk.itemHeight))))
                }
                onPositionChanged: (drag) => column.dropIndex = indexAt(drag.y)
                onEntered: (drag) => column.dropIndex = indexAt(drag.y)
                onExited: column.dropIndex = -1
                onDropped: (drop) => {
                    const data = column.host ? column.host.dragData : null
                    const id = data ? (data.pinId || data.desktopId) : ""
                    if (id)
                        Shell.startApps.pin(id, indexAt(drop.y))
                    column.dropIndex = -1
                }
            }
        }

        Rectangle {
            visible: column.pinned.length > 0 && column.frequent.length > 0
            x: Theme.px(column.tk.padding)
            width: parent.width - x * 2
            height: Theme.px(1) + Theme.px(column.tk.padding) * 2
            color: "transparent"
            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: Theme.px(1)
                color: column.tk.separatorColor
            }
        }

        Repeater {
            id: frequentRows
            model: column.frequent
            delegate: StartMenuItem {
                required property var modelData
                required property int index
                objectName: "frequent:" + modelData.id
                width: programColumn.width
                height: Theme.px(column.tk.smallItemHeight)
                label: modelData.label
                iconName: modelData.iconName
                hot: column.current === column.pinned.length + index
                onHovered: { column.current = column.pinned.length + index; column.forceActiveFocus() }
                onClicked: column.activate(column.pinned.length + index)
                onRightClicked: (x, y) => column.openContextMenu(modelData, x, y, true)
            }
        }
    }

    // All Programs at the bottom, under its own separator.
    Rectangle {
        anchors.bottom: allPrograms.top
        x: Theme.px(column.tk.padding)
        width: parent.width - x * 2
        height: Theme.px(1)
        color: column.tk.separatorColor
    }
    Item {
        id: allPrograms
        objectName: "allPrograms"
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.px(column.tk.padding)
        width: parent.width
        height: Theme.px(column.tk.allProgramsHeight)
        readonly property bool hot: column.current === column.allProgramsIndex

        Accessible.role: Accessible.MenuItem
        Accessible.name: qsTr("All Programs")
        Accessible.onPressAction: column.allProgramsRequested(false)

        Rectangle {
            anchors.fill: parent
            visible: allPrograms.hot
            color: column.tk.hoverBackground
        }
        Row {
            anchors.centerIn: parent
            spacing: Theme.px(column.tk.padding)
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("All Programs")
                font: Theme.fonts.menuItemBold
                color: allPrograms.hot ? column.tk.hoverText : Theme.tokens.colors.windowText
            }
            GradientBox {
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.px(column.tk.allProgramsArrowSize)
                height: width
                radius: Theme.px(2)
                color: column.tk.allProgramsArrowColor
                LunaGlyph {
                    anchors.fill: parent
                    anchors.margins: Theme.px(2)
                    glyph: LunaGlyph.ArrowRight
                    color: Theme.tokens.colors.white
                }
            }
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onEntered: {
                column.current = column.allProgramsIndex
                column.forceActiveFocus()
                openTimer.restart()
            }
            onExited: openTimer.stop()
            onClicked: column.allProgramsRequested(false)
        }
        Timer {
            id: openTimer
            interval: Theme.tokens.startMenu.allPrograms.hoverDelayMs
            onTriggered: column.allProgramsRequested(false)
        }
    }
}
