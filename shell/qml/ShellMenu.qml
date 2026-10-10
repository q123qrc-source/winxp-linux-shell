// SPDX-License-Identifier: GPL-3.0-or-later
// XP menu: white box with a thin grey border, blue hover rows, bold default
// item, check marks, separators and cascading submenus. Used for context
// menus and, with All Programs metrics, for the Start menu's cascades.
// Opened through PopupHost. Entries are plain objects:
//
//   { label, enabled, visible, separator, bold, shortcut,
//     toggleType ("checkmark"/"radio"/""), checked,
//     iconName (theme icon) or iconSource (url), children: [...],
//     highlight (e.g. newly installed), desktopId (draggable to pin),
//     action: function() {...} }
//
// Choosing an entry runs its action, emits triggered(entry) and closes all
// menus. Keyboard: arrows, Enter, Right/Left into and out of submenus,
// Escape, and type-ahead (a letter selects the next item starting with it).
import QtQuick
import Meadow.Controls

FocusScope {
    id: menu

    property var entries: []
    property var host: null
    // The menu this one cascades from (Left returns to it).
    property var parentMenu: null
    // Index of the highlighted row (keyboard or mouse), -1 for none.
    property int current: -1
    // Forced highlight for screenshots.
    property int previewHover: -1
    property var submenu: null

    readonly property var tk: Theme.tokens.controls.menu
    // Metrics and colors; the Start menu's cascades override them.
    property int itemHeight: Theme.px(tk.itemHeight)
    property int iconSize: Theme.px(tk.iconSize)
    property color background: tk.background
    property color borderColor: tk.borderColor
    property color highlightColor: Theme.tokens.startMenu.allPrograms.newProgramHighlight
    property int widthMin: Theme.px(tk.widthMin)
    property int widthMax: Theme.px(tk.widthMax)
    // Rows with a desktopId can be dragged (to pin them in the Start menu).
    property bool dragEnabled: false
    // Close only this menu (and its submenus) after a choice, e.g. a context
    // menu over the Start menu.
    property bool keepParent: false
    // Submenus are created like this one (same metrics).
    property var submenuProperties: ({})

    readonly property int pad: Theme.px(tk.paddingY)
    readonly property var visibleEntries: entries.filter(e => e.visible !== false)
    readonly property bool hasIcons: visibleEntries.some(e => e.iconName || e.iconSource)
    readonly property bool hasChecks: hasIcons || visibleEntries.some(e => e.toggleType === "checkmark" || e.toggleType === "radio")
    // Width of the check/icon column.
    readonly property int leadWidth: hasIcons ? Math.max(Theme.px(tk.checkWidth), iconSize + Theme.px(tk.paddingX))
                                              : Theme.px(tk.checkWidth)

    signal triggered(var entry)
    // Right-click on a row; x/y in window coordinates.
    signal contextMenuRequested(var entry, real x, real y)

    // Widest row's natural width (rows fill the menu, so Column cannot tell).
    property real widestRow: 0

    width: Math.max(widthMin, Math.min(widthMax, widestRow)) + Theme.px(1) * 2
    height: column.implicitHeight + pad * 2 + Theme.px(1) * 2
    focus: true

    Accessible.role: Accessible.PopupMenu

    function activate(entry) {
        if (!entry || entry.separator || entry.enabled === false)
            return
        if (entry.children && entry.children.length > 0) {
            openSubmenu(visibleEntries.indexOf(entry), true)
            return
        }
        if (typeof entry.action === "function")
            entry.action()
        triggered(entry)
        if (host) {
            if (keepParent)
                host.closeFrom(menu)
            else
                host.closeAll()
        }
    }

    function openSubmenu(index, focusFirst) {
        const entry = visibleEntries[index]
        if (!host || !entry || !entry.children || entry.children.length === 0)
            return
        if (submenu)
            host.closeFrom(submenu)
        const row = rows.itemAt(index)
        // A file cannot instantiate its own type statically; load it by URL.
        const props = Object.assign({}, submenuProperties, {
            entries: entry.children, parentMenu: menu, submenuProperties: submenuProperties,
            dragEnabled: dragEnabled, current: focusFirst ? 0 : -1
        })
        const sub = host.openChild(Qt.createComponent(Qt.resolvedUrl("ShellMenu.qml")), props, 0, false,
                                   host.besidePlacer(menu, row.y + pad))
        if (!sub)
            return
        sub.triggered.connect(menu.triggered)
        sub.contextMenuRequested.connect(menu.contextMenuRequested)
        submenu = sub
        if (!focusFirst)
            menu.forceActiveFocus()
    }

    function closeSubmenu() {
        if (submenu && host) {
            host.closeFrom(submenu)
            submenu = null
        }
        forceActiveFocus()
    }

    function step(delta) {
        const n = visibleEntries.length
        let idx = current
        for (let i = 0; i < n; ++i) {
            idx = idx < 0 ? (delta > 0 ? 0 : n - 1) : (idx + delta + n) % n
            const e = visibleEntries[idx]
            if (!e.separator && e.enabled !== false) {
                current = idx
                return
            }
        }
    }

    // Type-ahead: the next selectable item whose label starts with `text`.
    function typeAhead(text) {
        const n = visibleEntries.length
        const t = text.toLowerCase()
        for (let i = 1; i <= n; ++i) {
            const idx = ((current < 0 ? -1 : current) + i + n) % n
            const e = visibleEntries[idx]
            if (!e.separator && e.enabled !== false && (e.label || "").toLowerCase().startsWith(t)) {
                current = idx
                return true
            }
        }
        return false
    }

    Keys.onUpPressed: step(-1)
    Keys.onDownPressed: step(1)
    Keys.onRightPressed: openSubmenu(current, true)
    Keys.onLeftPressed: {
        if (parentMenu) {
            parentMenu.closeSubmenu()
        } else if (host && host.layerItem.children[0] !== menu) {
            host.closeFrom(menu)
        }
    }
    Keys.onReturnPressed: activate(visibleEntries[current])
    Keys.onEnterPressed: activate(visibleEntries[current])
    Keys.onEscapePressed: (event) => {
        // Escape closes just this cascade level, like XP.
        if (parentMenu) {
            parentMenu.closeSubmenu()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }
    Keys.onPressed: (event) => {
        if (event.text.length === 1 && event.text.trim().length === 1 && !(event.modifiers & Qt.ControlModifier))
            event.accepted = typeAhead(event.text)
    }

    // Soft drop shadow (bottom-right), then the box.
    Rectangle {
        x: Theme.px(menu.tk.shadowOffset)
        y: Theme.px(menu.tk.shadowOffset)
        width: parent.width
        height: parent.height
        color: menu.tk.shadowColor
    }
    Rectangle {
        anchors.fill: parent
        color: menu.background
        border.color: menu.borderColor
        border.width: Theme.px(1)
    }

    Column {
        id: column
        x: Theme.px(1)
        y: Theme.px(1) + menu.pad
        width: menu.width - Theme.px(1) * 2

        Repeater {
            id: rows
            model: menu.visibleEntries

            delegate: Item {
                id: row
                objectName: "menuItem:" + (modelData.label || "")
                required property var modelData
                required property int index
                readonly property bool isSeparator: modelData.separator === true
                readonly property bool enabledRow: !isSeparator && modelData.enabled !== false
                readonly property bool hot: enabledRow && (menu.current === index || menu.previewHover === index)
                readonly property bool hasChildren: !!modelData.children && modelData.children.length > 0

                width: column.width
                implicitWidth: (menu.hasChecks ? menu.leadWidth : Theme.px(menu.tk.paddingX))
                               + label.implicitWidth + Theme.px(menu.tk.paddingX)
                               + (shortcut.text.length > 0 ? Theme.px(menu.tk.shortcutSpacing) + shortcut.implicitWidth : 0)
                               + Theme.px(menu.tk.arrowWidth)
                height: isSeparator ? Theme.px(menu.tk.separatorPadding) * 2 + Theme.px(1) : menu.itemHeight
                onImplicitWidthChanged: menu.widestRow = Math.max(menu.widestRow, implicitWidth)
                Component.onCompleted: menu.widestRow = Math.max(menu.widestRow, implicitWidth)

                Accessible.role: isSeparator ? Accessible.Separator : Accessible.MenuItem
                Accessible.name: modelData.label || ""
                Accessible.checkable: modelData.toggleType === "checkmark" || modelData.toggleType === "radio"
                Accessible.checked: modelData.checked === true

                Rectangle {
                    visible: row.isSeparator
                    anchors.verticalCenter: parent.verticalCenter
                    x: Theme.px(1)
                    width: parent.width - Theme.px(1) * 2
                    height: Theme.px(1)
                    color: menu.tk.separatorColor
                }

                Rectangle {
                    anchors.fill: parent
                    visible: !row.hot && row.modelData.highlight === true
                    color: menu.highlightColor
                }
                Rectangle {
                    anchors.fill: parent
                    visible: row.hot
                    color: menu.tk.hoverBackground
                }

                // Check mark, radio dot or icon column.
                Item {
                    id: iconColumn
                    visible: menu.hasChecks && !row.isSeparator
                    width: menu.leadWidth
                    height: parent.height

                    LunaGlyph {
                        anchors.centerIn: parent
                        width: Theme.px(menu.tk.glyphSize)
                        height: width
                        visible: row.modelData.toggleType === "checkmark" && row.modelData.checked === true
                        glyph: LunaGlyph.Check
                        color: label.color
                        lineWidth: Theme.px(1.5) // theme-ok: stroke weight of the check glyph
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: Theme.px(menu.tk.radioSize)
                        height: width
                        radius: width / 2
                        visible: row.modelData.toggleType === "radio" && row.modelData.checked === true
                        color: label.color
                    }
                    Image {
                        anchors.centerIn: parent
                        width: menu.iconSize
                        height: width
                        sourceSize: Qt.size(width, height)
                        visible: !row.modelData.toggleType && source.toString().length > 0
                        source: row.modelData.iconSource ? row.modelData.iconSource
                              : (row.modelData.iconName ? "image://icon/" + row.modelData.iconName : "")
                    }
                }

                Text {
                    id: label
                    visible: !row.isSeparator
                    x: menu.hasChecks ? iconColumn.width : Theme.px(menu.tk.paddingX)
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - x - Theme.px(menu.tk.arrowWidth)
                           - (shortcut.text.length > 0 ? shortcut.implicitWidth + Theme.px(menu.tk.shortcutSpacing) : 0)
                    text: row.modelData.label || ""
                    elide: Text.ElideRight
                    font.family: Theme.fonts.ui.family
                    font.pixelSize: Theme.fonts.ui.pixelSize
                    font.bold: row.modelData.bold === true
                    color: !row.enabledRow ? Theme.tokens.colors.disabledText
                         : row.hot ? menu.tk.hoverText : Theme.tokens.colors.windowText
                }
                Text {
                    id: shortcut
                    visible: !row.isSeparator && text.length > 0
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.px(menu.tk.arrowWidth)
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.shortcut || ""
                    font: Theme.fonts.ui
                    color: label.color
                }
                LunaGlyph {
                    visible: row.hasChildren
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.px(menu.tk.arrowWidth)
                    height: Theme.px(menu.tk.glyphSize)
                    glyph: LunaGlyph.ArrowRight
                    color: label.color
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: !row.isSeparator
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onEntered: {
                        menu.current = row.index
                        if (row.hasChildren)
                            submenuTimer.restart()
                        else if (menu.submenu && menu.host) {
                            submenuTimer.stop()
                            menu.host.closeFrom(menu.submenu)
                            menu.submenu = null
                        }
                    }
                    onClicked: (event) => {
                        if (event.button === Qt.RightButton) {
                            const p = row.mapToItem(null, event.x, event.y)
                            menu.contextMenuRequested(row.modelData, p.x, p.y)
                        } else {
                            menu.activate(row.modelData)
                        }
                    }
                }

                DragHandler {
                    target: null
                    enabled: menu.dragEnabled && !!row.modelData.desktopId && !!menu.host
                    onActiveChanged: active ? menu.host.beginDrag(row.modelData, centroid.scenePosition)
                                            : menu.host.endDrag()
                    onCentroidChanged: if (active) menu.host.moveDrag(centroid.scenePosition)
                }
            }
        }
    }

    Timer {
        id: submenuTimer
        interval: menu.tk.submenuDelayMs
        onTriggered: menu.openSubmenu(menu.current, false)
    }
}
