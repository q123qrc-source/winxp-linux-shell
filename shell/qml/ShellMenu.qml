// SPDX-License-Identifier: GPL-3.0-or-later
// XP context menu: white box with a thin grey border, blue hover rows, bold
// default item, check marks, separators and cascading submenus. Opened through
// PopupHost. Entries are plain objects:
//
//   { label, enabled, visible, separator, bold, shortcut,
//     toggleType ("checkmark"/"radio"/""), checked,
//     iconName (theme icon) or iconSource (url), children: [...],
//     action: function() {...} }
//
// Choosing an entry runs its action, emits triggered(entry) and closes all
// menus.
import QtQuick
import Meadow.Controls

FocusScope {
    id: menu

    property var entries: []
    property var host: null
    // Index of the highlighted row (keyboard or mouse), -1 for none.
    property int current: -1
    // Forced highlight for screenshots.
    property int previewHover: -1
    property var submenu: null

    readonly property var tk: Theme.tokens.controls.menu
    readonly property int pad: Theme.px(tk.paddingY)
    readonly property var visibleEntries: entries.filter(e => e.visible !== false)
    readonly property bool hasChecks: visibleEntries.some(e => e.toggleType === "checkmark" || e.toggleType === "radio"
                                                          || e.iconName || e.iconSource)

    signal triggered(var entry)

    // Widest row's natural width (rows fill the menu, so Column cannot tell).
    property real widestRow: 0

    width: Math.max(Theme.px(tk.widthMin), Math.min(Theme.px(tk.widthMax), widestRow)) + Theme.px(1) * 2
    height: column.implicitHeight + pad * 2 + Theme.px(1) * 2
    focus: true

    Accessible.role: Accessible.PopupMenu

    function activate(entry) {
        if (!entry || entry.separator || entry.enabled === false)
            return
        if (entry.children && entry.children.length > 0) {
            openSubmenu(visibleEntries.indexOf(entry))
            return
        }
        if (typeof entry.action === "function")
            entry.action()
        triggered(entry)
        if (host)
            host.closeAll()
    }

    function openSubmenu(index) {
        const entry = visibleEntries[index]
        if (!host || !entry || !entry.children || entry.children.length === 0)
            return
        if (submenu)
            host.closeFrom(submenu)
        const row = rows.itemAt(index)
        // A file cannot instantiate its own type statically; load it by URL.
        const sub = host.openChild(Qt.createComponent(Qt.resolvedUrl("ShellMenu.qml")),
                                   { entries: entry.children }, 0, false,
                                   host.besidePlacer(menu, row.y + pad))
        if (!sub)
            return
        sub.triggered.connect(menu.triggered)
        submenu = sub
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

    Keys.onUpPressed: step(-1)
    Keys.onDownPressed: step(1)
    Keys.onRightPressed: openSubmenu(current)
    Keys.onLeftPressed: if (host && host.layerItem.children[0] !== menu) host.closeFrom(menu)
    Keys.onReturnPressed: activate(visibleEntries[current])
    Keys.onEnterPressed: activate(visibleEntries[current])

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
        color: menu.tk.background
        border.color: menu.tk.borderColor
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
                implicitWidth: (menu.hasChecks ? Theme.px(menu.tk.checkWidth) : Theme.px(menu.tk.paddingX))
                               + label.implicitWidth + Theme.px(menu.tk.paddingX)
                               + (shortcut.text.length > 0 ? Theme.px(menu.tk.shortcutSpacing) + shortcut.implicitWidth : 0)
                               + Theme.px(menu.tk.arrowWidth)
                height: isSeparator ? Theme.px(menu.tk.separatorPadding) * 2 + Theme.px(1) : Theme.px(menu.tk.itemHeight)
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
                    visible: row.hot
                    color: menu.tk.hoverBackground
                }

                // Check mark, radio dot or icon column.
                Item {
                    visible: menu.hasChecks && !row.isSeparator
                    width: Theme.px(menu.tk.checkWidth)
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
                        width: Theme.px(menu.tk.iconSize)
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
                    x: menu.hasChecks ? Theme.px(menu.tk.checkWidth) : Theme.px(menu.tk.paddingX)
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
                            menu.host.closeFrom(menu.submenu)
                            menu.submenu = null
                        }
                    }
                    onClicked: menu.activate(row.modelData)
                }
            }
        }
    }

    Timer {
        id: submenuTimer
        interval: menu.tk.submenuDelayMs
        onTriggered: menu.openSubmenu(menu.current)
    }
}
