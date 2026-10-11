// SPDX-License-Identifier: GPL-3.0-or-later
// The desktop: wallpaper (shell.conf [Desktop]: an asset name or a path,
// Center/Tile/Stretch, over a solid color) and the icons of Shell.desktop on
// a grid of desktop.iconGrid* cells.
//
// Mouse: click selects (Ctrl toggles), drag moves the selection (snapped to
// the grid unless Align to Grid is off), drag on empty space draws a rubber
// band, double-click opens, right-click opens the icon or desktop menu.
// Keyboard: arrows move between icons, Enter opens, Delete moves to the
// Recycle Bin, F2 renames, Ctrl+A selects all, letters jump by name.
//
// In a session this is the content of a background layer surface (one per
// screen; icons only on the primary one); in --preview it sits behind the
// taskbar in the preview window.
import QtQuick
import Meadow.Controls
import Meadow.Shell

FocusScope {
    id: desktop

    property PopupHost popups: null
    property bool showIcons: true
    // Space kept free at the bottom (the taskbar in --preview).
    property int bottomInset: 0

    readonly property var tk: Theme.tokens.desktop
    readonly property int cellWidth: Theme.px(tk.iconGridWidth)
    readonly property int cellHeight: Theme.px(tk.iconGridHeight)
    readonly property int margin: Theme.px(tk.gridMargin)
    readonly property bool rtl: Qt.application.layoutDirection === Qt.RightToLeft

    // Selection: item id -> true.
    property var selection: ({})
    property string currentId: ""
    property bool showFocus: false
    // Icon drag in progress: offset of the dragged selection in pixels.
    property point dragOffset: Qt.point(0, 0)
    property bool dragging: false

    Accessible.role: Accessible.List
    Accessible.name: qsTr("Desktop")

    // --- Wallpaper --------------------------------------------------------

    readonly property string wallpaperSetting: { Shell.settings.revision; return Shell.settings.stringValue("Desktop/wallpaper", tk.wallpaper) }
    readonly property string wallpaperMode: { Shell.settings.revision; return Shell.settings.stringValue("Desktop/wallpaperMode", tk.wallpaperMode) }
    readonly property string colorSetting: { Shell.settings.revision; return Shell.settings.stringValue("Desktop/color", "") }
    readonly property string wallpaperUrl: {
        const w = wallpaperSetting
        if (w.length === 0)
            return ""
        return w.startsWith("/") ? "file://" + w : Theme.asset(w)
    }

    Rectangle {
        anchors.fill: parent
        color: desktop.colorSetting.length > 0 ? desktop.colorSetting : desktop.tk.background
    }
    Image {
        id: wallpaper
        objectName: "wallpaper"
        anchors.fill: parent
        visible: desktop.wallpaperUrl.length > 0
        source: desktop.wallpaperUrl
        asynchronous: !Shell.preview
        cache: false
        // Stretch fills the screen without distorting the picture.
        fillMode: desktop.wallpaperMode === "tile" ? Image.Tile
                : desktop.wallpaperMode === "center" ? Image.Pad
                : Image.PreserveAspectCrop
        horizontalAlignment: Image.AlignHCenter
        verticalAlignment: Image.AlignVCenter
        // Vector art is rendered at the screen size; pictures at their own.
        sourceSize: desktop.wallpaperUrl.endsWith(".svg") && desktop.wallpaperMode !== "tile"
                    ? Qt.size(Math.max(width, height * 4 / 3), Math.max(height, width * 3 / 4)) // theme-ok: the art's 4:3 aspect
                    : undefined
    }

    // --- Grid -------------------------------------------------------------

    function updateGrid() {
        if (!showIcons || width <= 0 || height <= 0)
            return
        Shell.desktop.setGridSize(Math.floor((width - margin * 2) / cellWidth),
                                  Math.floor((height - bottomInset - margin * 2) / cellHeight))
    }
    onWidthChanged: Qt.callLater(updateGrid)
    onHeightChanged: Qt.callLater(updateGrid)
    onBottomInsetChanged: Qt.callLater(updateGrid)
    Component.onCompleted: Qt.callLater(updateGrid)

    function cellToX(cx) {
        const x = margin + cx * cellWidth
        return rtl ? width - x - cellWidth : x
    }
    function cellToY(cy) { return margin + cy * cellHeight }

    // --- Selection --------------------------------------------------------

    function selectedIds() {
        return Object.keys(selection).filter(k => selection[k])
    }
    function select(id, mode) { // mode: "only", "toggle", "add"
        const s = mode === "only" ? {} : Object.assign({}, selection)
        if (mode === "toggle")
            s[id] = !s[id]
        else
            s[id] = true
        selection = s
        currentId = id
    }
    function clearSelection() {
        selection = {}
    }
    function selectAll() {
        const s = {}
        for (let i = 0; i < icons.count; ++i)
            s[icons.itemAt(i).itemId] = true
        selection = s
    }
    function iconFor(id) {
        for (let i = 0; i < icons.count; ++i) {
            if (icons.itemAt(i).itemId === id)
                return icons.itemAt(i)
        }
        return null
    }

    function openSelected() {
        const ids = selectedIds()
        for (const id of ids)
            Shell.desktop.open(id)
    }
    function trashSelected() {
        Shell.desktop.moveToTrash(selectedIds())
        clearSelection()
    }
    function renameCurrent() {
        const icon = iconFor(currentId)
        if (icon && Shell.desktop.canRename(currentId))
            icon.startRename()
    }

    // Arrow keys: the nearest icon in that direction (by cell distance).
    function moveCurrent(dx, dy, extend) {
        if (rtl)
            dx = -dx
        const from = iconFor(currentId)
        if (!from) {
            if (icons.count > 0)
                select(icons.itemAt(0).itemId, "only")
            return
        }
        let best = null, bestScore = 1e9 // theme-ok: "infinity" for the search
        for (let i = 0; i < icons.count; ++i) {
            const it = icons.itemAt(i)
            const ddx = it.cellX - from.cellX, ddy = it.cellY - from.cellY
            const along = dx !== 0 ? ddx * dx : ddy * dy
            if (along <= 0)
                continue
            const across = Math.abs(dx !== 0 ? ddy : ddx)
            const score = along + across * 4 // theme-ok: prefer the same row/column
            if (score < bestScore) {
                bestScore = score
                best = it
            }
        }
        if (best)
            select(best.itemId, extend ? "add" : "only")
    }

    function typeAhead(text) {
        const t = text.toLowerCase()
        const n = icons.count
        let start = 0
        for (let i = 0; i < n; ++i) {
            if (icons.itemAt(i).itemId === currentId)
                start = i + 1
        }
        for (let k = 0; k < n; ++k) {
            const it = icons.itemAt((start + k) % n)
            if (it.name.toLowerCase().startsWith(t)) {
                select(it.itemId, "only")
                return true
            }
        }
        return false
    }

    Keys.onPressed: (event) => {
        showFocus = true
        const extend = event.modifiers & Qt.ShiftModifier
        switch (event.key) {
        case Qt.Key_Left: moveCurrent(-1, 0, extend); break
        case Qt.Key_Right: moveCurrent(1, 0, extend); break
        case Qt.Key_Up: moveCurrent(0, -1, extend); break
        case Qt.Key_Down: moveCurrent(0, 1, extend); break
        case Qt.Key_Return:
        case Qt.Key_Enter: openSelected(); break
        case Qt.Key_Delete: trashSelected(); break
        case Qt.Key_F2: renameCurrent(); break
        case Qt.Key_F5: Shell.desktop.refresh(); break
        case Qt.Key_Menu: openMenuForSelection(); break
        case Qt.Key_A:
            if (event.modifiers & Qt.ControlModifier) {
                selectAll()
                break
            }
            event.accepted = typeAhead(event.text)
            return
        default:
            if (event.text.length === 1 && event.text.trim().length === 1 && !(event.modifiers & Qt.ControlModifier)) {
                event.accepted = typeAhead(event.text)
                return
            }
            event.accepted = false
            return
        }
        event.accepted = true
    }

    // --- Menus ------------------------------------------------------------

    DesktopContextMenu {
        id: menus
        desktop: desktop
        popups: desktop.popups
    }
    function openMenuForSelection() {
        const icon = iconFor(currentId)
        if (icon && selection[currentId]) {
            const p = icon.mapToItem(desktop, icon.width / 2, icon.height / 2)
            menus.openItemMenu(currentId, p.x, p.y)
        } else {
            menus.openDesktopMenu(width / 2, height / 2)
        }
    }

    // --- Background mouse: rubber band and the desktop menu ---------------

    MouseArea {
        id: background
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        property point start
        property bool banding: false
        property var baseSelection: ({})

        onPressed: (mouse) => {
            desktop.forceActiveFocus()
            desktop.showFocus = false
            if (mouse.button === Qt.RightButton)
                return
            start = Qt.point(mouse.x, mouse.y)
            baseSelection = (mouse.modifiers & Qt.ControlModifier) ? Object.assign({}, desktop.selection) : {}
            if (!(mouse.modifiers & Qt.ControlModifier))
                desktop.clearSelection()
        }
        onPositionChanged: (mouse) => {
            if (!(mouse.buttons & Qt.LeftButton))
                return
            const dx = mouse.x - start.x, dy = mouse.y - start.y
            if (!banding && Math.abs(dx) + Math.abs(dy) < Theme.px(desktop.tk.dragThreshold))
                return
            banding = true
            band.x = Math.min(start.x, mouse.x)
            band.y = Math.min(start.y, mouse.y)
            band.width = Math.abs(dx)
            band.height = Math.abs(dy)
            const s = Object.assign({}, baseSelection)
            for (let i = 0; i < icons.count; ++i) {
                const it = icons.itemAt(i)
                const r = it.hitRect
                const tl = it.mapToItem(desktop, r.x, r.y)
                const hit = tl.x < band.x + band.width && tl.x + r.width > band.x
                         && tl.y < band.y + band.height && tl.y + r.height > band.y
                if (hit)
                    s[it.itemId] = true
            }
            desktop.selection = s
        }
        onReleased: (mouse) => {
            banding = false
        }
        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton)
                menus.openDesktopMenu(mouse.x, mouse.y)
        }
    }

    Rectangle {
        id: band
        objectName: "rubberBand"
        visible: background.banding
        color: desktop.tk.rubberBandFill
        border.color: desktop.tk.rubberBandBorder
        border.width: Theme.px(1)
    }

    // --- Icons ------------------------------------------------------------

    Repeater {
        id: icons
        model: desktop.showIcons ? Shell.desktop : null

        delegate: DesktopIcon {
            id: iconItem
            required property int index
            required property real cellX
            required property real cellY
            required property string kind
            objectName: "desktopIcon:" + itemId

            readonly property bool isSelected: !!desktop.selection[itemId]
            readonly property bool moving: desktop.dragging && isSelected

            x: desktop.cellToX(cellX) + (moving ? desktop.dragOffset.x : 0)
            y: desktop.cellToY(cellY) + (moving ? desktop.dragOffset.y : 0)
            z: moving ? 1 : 0
            width: desktop.cellWidth
            height: desktop.cellHeight
            opacity: moving ? desktop.tk.dragOpacity : 1
            selected: isSelected
            current: desktop.currentId === itemId
            showFocus: desktop.showFocus && desktop.activeFocus

            onRenameFinished: (newName, accepted) => {
                if (accepted)
                    Shell.desktop.rename(itemId, newName)
                desktop.forceActiveFocus()
            }

            MouseArea {
                x: iconItem.hitRect.x
                y: iconItem.hitRect.y
                width: iconItem.hitRect.width
                height: iconItem.hitRect.height
                enabled: !iconItem.renaming
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                property point start
                property bool moved: false

                onPressed: (mouse) => {
                    desktop.forceActiveFocus()
                    desktop.showFocus = false
                    start = mapToItem(desktop, mouse.x, mouse.y)
                    moved = false
                    if (mouse.modifiers & Qt.ControlModifier)
                        desktop.select(iconItem.itemId, "toggle")
                    else if (!iconItem.isSelected)
                        desktop.select(iconItem.itemId, "only")
                    else
                        desktop.currentId = iconItem.itemId
                }
                onPositionChanged: (mouse) => {
                    if (!(mouse.buttons & Qt.LeftButton))
                        return
                    const p = mapToItem(desktop, mouse.x, mouse.y)
                    const dx = p.x - start.x, dy = p.y - start.y
                    if (!moved && Math.abs(dx) + Math.abs(dy) < Theme.px(desktop.tk.dragThreshold))
                        return
                    moved = true
                    desktop.dragging = true
                    desktop.dragOffset = Qt.point(dx, dy)
                }
                onReleased: (mouse) => {
                    if (!moved)
                        return
                    // Copy the numbers: a point read from a property is a live
                    // reference, and dragOffset is reset below.
                    const dx = desktop.dragOffset.x
                    const dy = desktop.dragOffset.y
                    desktop.dragging = false
                    desktop.dragOffset = Qt.point(0, 0)
                    Shell.desktop.moveItems(desktop.selectedIds(),
                                            (desktop.rtl ? -dx : dx) / desktop.cellWidth, dy / desktop.cellHeight)
                }
                onClicked: (mouse) => {
                    if (mouse.button !== Qt.RightButton || moved)
                        return
                    if (!iconItem.isSelected)
                        desktop.select(iconItem.itemId, "only")
                    const p = mapToItem(desktop, mouse.x, mouse.y)
                    menus.openItemMenu(iconItem.itemId, p.x, p.y)
                }
                onDoubleClicked: (mouse) => {
                    if (mouse.button === Qt.LeftButton)
                        Shell.desktop.open(iconItem.itemId)
                }
            }
        }
    }

    Connections {
        target: Shell.desktop
        function onUntrustedLauncher(name) {
            Shell.notifications.post(qsTr("Desktop"), qsTr("This shortcut is not trusted"),
                qsTr("\"%1\" is a launcher that is not marked as executable, so Meadow will not run it. "
                     + "If you trust it, make the file executable.").arg(name))
        }
    }

    // --open desktop-menu / item-menu (screenshots): the desktop menu at
    // the middle, or the first file's menu.
    function openPreviewMenu(onItem) {
        if (!onItem) {
            menus.openDesktopMenu(width / 3, height / 3) // theme-ok: a third in, for screenshots
            return
        }
        for (let i = 0; i < icons.count; ++i) {
            const it = icons.itemAt(i)
            if (it.kind === "file") {
                select(it.itemId, "only")
                menus.openItemMenu(it.itemId, it.x + it.width / 2, it.y + it.height / 2)
                return
            }
        }
    }

    // Starts renaming a new item once the model has it (New > Folder).
    function renameWhenReady(id) {
        if (!id)
            return
        select(id, "only")
        Qt.callLater(() => { const icon = iconFor(id); if (icon) icon.startRename() })
    }
}
