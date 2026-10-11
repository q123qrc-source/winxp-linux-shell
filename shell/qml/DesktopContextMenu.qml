// SPDX-License-Identifier: GPL-3.0-or-later
// The desktop's context menus, opened at the pointer through the PopupHost:
//   desktop  Arrange Icons By > (Name, Size, Type, Modified | Auto Arrange,
//            Align to Grid), Refresh | New > (Folder, Text Document) |
//            Properties (Display Properties, Desktop tab)
//   icon     Open (bold) | Rename, Delete | Properties; the Recycle Bin has
//            Open, Empty Recycle Bin and Properties
import QtQuick
import Meadow.Shell

Item {
    id: menus

    property Item desktop
    property PopupHost popups

    Component {
        id: menuComponent
        ShellMenu {}
    }

    // Opens `entries` with its top-left corner at (x, y) in desktop
    // coordinates, flipped left/up where it would leave the screen.
    function openAt(entries, x, y) {
        if (!popups)
            return null
        popups.closeAll()
        const origin = desktop.mapToItem(popups, x, y)
        return popups.openChild(menuComponent, { entries: entries }, 0, false, function(popup) {
            popup.x = origin.x + popup.width <= popups.width ? origin.x : Math.max(0, origin.x - popup.width)
            const bottom = popups.height - popups.bottomInset
            popup.y = origin.y + popup.height <= bottom ? origin.y : Math.max(0, origin.y - popup.height)
        })
    }

    function sortEntry(label, key) {
        return { label: label, toggleType: "radio", checked: Shell.desktop.sortBy === key,
                 action: () => Shell.desktop.arrange(key) }
    }

    function openDesktopMenu(x, y) {
        openAt([
            { label: qsTr("Arrange Icons By"), children: [
                sortEntry(qsTr("Name"), "name"),
                sortEntry(qsTr("Size"), "size"),
                sortEntry(qsTr("Type"), "type"),
                sortEntry(qsTr("Modified"), "modified"),
                { separator: true },
                { label: qsTr("Auto Arrange"), toggleType: "checkmark", checked: Shell.desktop.autoArrange,
                  action: () => Shell.desktop.autoArrange = !Shell.desktop.autoArrange },
                { label: qsTr("Align to Grid"), toggleType: "checkmark", checked: Shell.desktop.alignToGrid,
                  action: () => Shell.desktop.alignToGrid = !Shell.desktop.alignToGrid },
            ] },
            { label: qsTr("Refresh"), action: () => Shell.desktop.refresh() },
            { separator: true },
            { label: qsTr("New"), children: [
                { label: qsTr("Folder"), iconName: "folder",
                  action: () => desktop.renameWhenReady(Shell.desktop.newFolder()) },
                { separator: true },
                { label: qsTr("Text Document"), iconName: "text-plain",
                  action: () => desktop.renameWhenReady(Shell.desktop.newTextDocument()) },
            ] },
            { separator: true },
            { label: qsTr("Properties"), action: () => ShellActions.displayPropertiesRequested("desktop") },
        ], x, y)
    }

    function openItemMenu(id, x, y) {
        const item = Shell.desktop.get(id)
        const multiple = desktop.selectedIds().length > 1
        let entries
        if (item.kind === "trash") {
            entries = [
                { label: qsTr("Open"), bold: true, action: () => Shell.desktop.open(id) },
                { label: qsTr("Empty Recycle Bin"), enabled: Shell.desktop.trashFull,
                  action: () => ShellActions.emptyTrashRequested() },
                { separator: true },
                { label: qsTr("Properties"), action: () => ShellActions.itemPropertiesRequested(Shell.desktop.details(id)) },
            ]
        } else if (item.kind === "computer" || item.kind === "documents") {
            entries = [
                { label: qsTr("Open"), bold: true, action: () => Shell.desktop.open(id) },
                { separator: true },
                { label: qsTr("Properties"), action: () => ShellActions.itemPropertiesRequested(Shell.desktop.details(id)) },
            ]
        } else {
            entries = [
                { label: qsTr("Open"), bold: true, action: () => desktop.openSelected() },
                { separator: true },
                { label: qsTr("Delete"), action: () => desktop.trashSelected() },
                { label: qsTr("Rename"), enabled: !multiple && Shell.desktop.canRename(id),
                  action: () => desktop.renameCurrent() },
                { separator: true },
                { label: qsTr("Properties"), enabled: !multiple,
                  action: () => ShellActions.itemPropertiesRequested(Shell.desktop.details(id)) },
            ]
        }
        openAt(entries, x, y)
    }
}
