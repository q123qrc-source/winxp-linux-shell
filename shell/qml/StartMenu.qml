// SPDX-License-Identifier: GPL-3.0-or-later
// The two-column Start menu, opened through PopupHost above the Start
// button: user header, orange separator, the white program column (pinned,
// frequent, All Programs ▸) and the blue places column, and the Log Off /
// Turn Off footer.
//
// Keyboard: arrows move within a column, Left/Right switch columns (Right on
// All Programs or My Recent Documents opens it), Enter activates, Tab cycles
// columns and the footer, a letter jumps to the next item starting with it,
// Escape closes.
import QtQuick
import Meadow.Controls
import Meadow.Shell

FocusScope {
    id: menu
    objectName: "startMenu"

    property var host: null
    property var allProgramsMenu: null
    // "left", "right" or "footer".
    property string area: "left"

    readonly property var tk: Theme.tokens.startMenu
    readonly property int border: Theme.px(tk.borderWidth)
    readonly property bool animate: !Theme.reducedMotion

    width: Theme.px(tk.width)
    height: Math.max(Theme.px(tk.height),
                     header.height + separator.height + left.contentHeight + footer.height + border * 2)
    focus: true

    Accessible.role: Accessible.PopupMenu
    Accessible.name: qsTr("Start menu")

    // All Programs entries from the menu tree.
    function programEntries(nodes) {
        if (nodes.length === 0)
            return [{ label: qsTr("(Empty)"), enabled: false }]
        return nodes.map(n => n.children
            ? { label: n.label, iconName: n.iconName, highlight: n.isNew, children: programEntries(n.children) }
            : { label: n.label, iconName: n.iconName, highlight: n.isNew, desktopId: n.desktopId,
                action: () => Shell.apps.launch(n.desktopId) })
    }

    function openAllPrograms(keyboard) {
        if (!host)
            return
        if (allProgramsMenu)
            host.closeFrom(allProgramsMenu)
        const button = left.allProgramsItem
        const bottom = button.mapToItem(menu, 0, button.height).y
        allProgramsMenu = host.openChild(allProgramsComponent, {
            entries: programEntries(Shell.appMenu.tree),
            current: keyboard ? 0 : -1,
            parentMenu: menu
        }, 0, false, function(popup) {
            // Out of the left column, bottom-aligned with All Programs.
            popup.x = Math.min(menu.x + left.width, host.width - popup.width)
            popup.y = Math.max(0, Math.min(menu.y + bottom - popup.height, host.height - host.bottomInset - popup.height))
        })
        if (!keyboard)
            left.forceActiveFocus()
    }
    // ShellMenu calls this when Left/Escape leaves All Programs.
    function closeSubmenu() {
        if (allProgramsMenu && host)
            host.closeFrom(allProgramsMenu)
        allProgramsMenu = null
        left.forceActiveFocus()
    }

    function openRecentFromKeyboard() {
        right.current = Shell.places.places.findIndex(p => p.submenu)
        right.openRecent(true)
    }

    // Gives a column the keyboard; `select` also highlights its first item
    // (arrow/Tab navigation; just opening the menu selects nothing, like XP).
    function focusArea(name, select) {
        area = name
        if (name === "left") {
            left.forceActiveFocus()
            if (select !== false && left.current < 0)
                left.current = 0
        } else if (name === "right") {
            right.forceActiveFocus()
            if (select !== false && right.current < 0)
                right.step(1)
        } else {
            footerFocus.forceActiveFocus()
            if (footer.current < 0)
                footer.current = 0
        }
        if (name !== "footer")
            footer.current = -1
    }

    Component.onCompleted: focusArea("left", false)

    // Program context menu inside All Programs.
    function programContextMenu(entry, x, y) {
        if (!entry.desktopId)
            return
        const pinned = Shell.startApps.isPinned(entry.desktopId)
        host.openChild(contextComponent, { entries: [
            { label: qsTr("Open"), bold: true, action: () => { Shell.apps.launch(entry.desktopId); host.closeAll() } },
            { separator: true },
            pinned ? { label: qsTr("Unpin from Start menu"), action: () => Shell.startApps.unpin(entry.desktopId) }
                   : { label: qsTr("Pin to Start menu"), action: () => Shell.startApps.pin(entry.desktopId) },
        ] }, 0, false, function(popup) {
            popup.x = Math.min(x, host.width - popup.width)
            popup.y = Math.min(y, host.height - host.bottomInset - popup.height)
        })
    }

    Keys.onTabPressed: focusArea(area === "left" ? "right" : area === "right" ? "footer" : "left")
    Keys.onBacktabPressed: focusArea(area === "left" ? "footer" : area === "right" ? "left" : "right")

    Component {
        id: allProgramsComponent
        AllProgramsCascade {
            onContextMenuRequested: (entry, x, y) => menu.programContextMenu(entry, x, y)
        }
    }
    Component {
        id: contextComponent
        ShellMenu { keepParent: true }
    }

    // Slide up and fade in (instant with reduced motion).
    transform: Translate { id: slide }
    ParallelAnimation {
        running: menu.animate
        NumberAnimation { target: menu; property: "opacity"; from: 0; to: 1; duration: Theme.tokens.animation.startMenuOpenMs; easing.type: Easing.OutCubic }
        NumberAnimation { target: slide; property: "y"; from: Theme.px(Theme.tokens.animation.startMenuSlide); to: 0; duration: Theme.tokens.animation.startMenuOpenMs; easing.type: Easing.OutCubic }
    }

    // Shadow, then the frame with the rounded top.
    Rectangle {
        x: Theme.px(Theme.tokens.controls.menu.shadowOffset)
        y: Theme.px(Theme.tokens.controls.menu.shadowOffset)
        width: parent.width
        height: parent.height
        radius: Theme.px(menu.tk.radiusTop)
        color: Theme.tokens.controls.menu.shadowColor
    }
    GradientBox {
        anchors.fill: parent
        color: menu.tk.borderColor
        topLeftRadius: Theme.px(menu.tk.radiusTop)
        topRightRadius: Theme.px(menu.tk.radiusTop)
        bottomLeftRadius: 0
        bottomRightRadius: 0
    }

    Item {
        anchors.fill: parent
        anchors.margins: menu.border

        StartMenuUserHeader {
            id: header
            width: parent.width
        }
        GradientBox {
            id: separator
            anchors.top: header.bottom
            width: parent.width
            height: Theme.px(menu.tk.headerSeparator.height)
            orientation: Qt.Horizontal
            stops: menu.tk.headerSeparator.gradient
        }

        StartMenuLeftColumn {
            id: left
            objectName: "startMenuLeft"
            host: menu.host
            anchors.top: separator.bottom
            anchors.bottom: footer.top
            // Rows plus top padding, two separators' padding and the bottom margin.
            readonly property real contentHeight: Theme.px(menu.tk.left.allProgramsHeight)
                + pinned.length * Theme.px(menu.tk.left.itemHeight) + frequent.length * Theme.px(menu.tk.left.smallItemHeight)
                + Theme.px(menu.tk.left.padding) * 5 // theme-ok: count of paddings in the column
            onAllProgramsRequested: (keyboard) => menu.openAllPrograms(keyboard)
            onLeaveRight: menu.focusArea("right")
            onActiveFocusChanged: if (activeFocus) menu.area = "left"
        }
        StartMenuRightColumn {
            id: right
            objectName: "startMenuRight"
            host: menu.host
            startMenu: menu
            anchors.top: separator.bottom
            anchors.bottom: footer.top
            anchors.left: left.right
            anchors.right: parent.right
            onLeaveLeft: menu.focusArea("left")
            onLeaveDown: menu.focusArea("footer")
            onActiveFocusChanged: if (activeFocus) menu.area = "right"
        }

        StartMenuFooter {
            id: footer
            anchors.bottom: parent.bottom
            width: parent.width
            onLogOff: { menu.host.closeAll(); ShellActions.logOffRequested() }
            onTurnOff: { menu.host.closeAll(); ShellActions.turnOffRequested() }

            FocusScope {
                id: footerFocus
                Keys.onLeftPressed: footer.current = 0
                Keys.onRightPressed: footer.current = 1
                Keys.onUpPressed: menu.focusArea("right")
                Keys.onReturnPressed: footer.current === 0 ? footer.logOff() : footer.turnOff()
                Keys.onEnterPressed: footer.current === 0 ? footer.logOff() : footer.turnOff()
            }
        }
    }
}
