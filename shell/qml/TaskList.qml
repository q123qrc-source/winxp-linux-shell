// SPDX-License-Identifier: GPL-3.0-or-later
// The window buttons. Buttons share the width evenly between
// taskButton.minWidth and maxWidth. When they would get narrower than
// taskButton.groupThreshold (and grouping is on), the windows of the app with
// the most windows collapse into one group button, then the next app, until
// the rest fit — like XP's "Group similar taskbar buttons".
import QtQuick
import Meadow.Controls
import Meadow.Shell

Item {
    id: list

    property PopupHost popups: null

    readonly property var tk: Theme.tokens.taskButton
    readonly property int spacing: Theme.px(Theme.tokens.taskbar.spacing)
    // Bumped on every model change so the grouping bindings re-run.
    property int revision: 0
    readonly property var groups: computeGroups(revision, width, Shell.settings.groupButtons)
    readonly property int entryCount: {
        let n = Shell.windows.count
        for (const app in groups)
            n -= groups[app].length - 1
        return n
    }
    readonly property real buttonWidth: entryCount === 0 ? 0
        : Math.max(Theme.px(tk.minWidth),
                   Math.min(Theme.px(tk.maxWidth), Math.floor((width - spacing * (entryCount - 1)) / entryCount)))

    Accessible.role: Accessible.PageTabList
    Accessible.name: qsTr("Running applications")

    function computeGroups(rev, availableWidth, enabled) {
        const n = Shell.windows.count
        const result = {}
        if (!enabled || n === 0)
            return result
        const minWidth = Theme.px(tk.groupThreshold)
        const fits = k => k * minWidth + (k - 1) * spacing <= availableWidth
        const byApp = {}
        for (let r = 0; r < n; ++r) {
            const appId = Shell.windows.get(r).appId
            if (appId)
                (byApp[appId] = byApp[appId] || []).push(r)
        }
        const candidates = Object.keys(byApp).filter(a => byApp[a].length > 1)
                                             .sort((a, b) => byApp[b].length - byApp[a].length)
        let entries = n
        for (const app of candidates) {
            if (fits(entries))
                break
            result[app] = byApp[app]
            entries -= byApp[app].length - 1
        }
        return result
    }

    function iconFor(appId) {
        const name = Shell.apps.iconForApp(appId)
        return "image://icon/" + (name.length > 0 ? name : (appId.length > 0 ? appId : "application-x-executable"))
    }

    function windowMenu(row) {
        const w = Shell.windows.get(row)
        return [
            { label: qsTr("Restore"), enabled: w.minimized || w.maximized,
              action: () => w.minimized ? Shell.windows.activate(row) : Shell.windows.toggleMaximize(row) },
            { label: qsTr("Minimize"), enabled: !w.minimized, action: () => Shell.windows.minimize(row) },
            { label: qsTr("Maximize"), enabled: !w.maximized, action: () => Shell.windows.toggleMaximize(row) },
            { separator: true },
            { label: qsTr("Close"), bold: true, shortcut: qsTr("Alt+F4"), action: () => Shell.windows.close(row) },
        ]
    }

    function groupMenu(rows) {
        return [
            { label: qsTr("Minimize Group"), action: () => rows.forEach(r => Shell.windows.minimize(r)) },
            { separator: true },
            { label: qsTr("Close Group"), bold: true,
              action: () => rows.slice().reverse().forEach(r => Shell.windows.close(r)) },
        ]
    }

    function openWindowMenu(row) {
        const button = buttons.itemAt(row)
        if (button && button.visible)
            button.contextMenuRequested(0)
    }
    function openFirstGroup() {
        for (let i = 0; i < buttons.count; ++i) {
            const button = buttons.itemAt(i)
            if (button && button.leader) {
                button.clicked()
                return
            }
        }
    }

    Connections {
        target: Shell.windows
        function onRowsInserted() { list.revision++ }
        function onRowsRemoved() { list.revision++ }
        function onModelReset() { list.revision++ }
        function onDataChanged() { list.revision++ }
    }

    Component {
        id: menuComponent
        ShellMenu {}
    }
    Component {
        id: groupComponent
        TaskGroupMenu {}
    }

    Row {
        spacing: list.spacing
        height: parent.height

        Repeater {
            id: buttons
            model: Shell.windows

            delegate: TaskButton {
                id: button
                objectName: "task-" + index
                required property int index
                required property var model

                readonly property string appId: model.appId
                readonly property var group: list.groups[appId]
                readonly property bool grouped: group !== undefined
                readonly property bool leader: grouped && group[0] === index
                readonly property bool groupActive: {
                    list.revision
                    return grouped && group.some(r => Shell.windows.get(r).active)
                }

                visible: !grouped || leader
                width: list.buttonWidth
                anchors.verticalCenter: parent.verticalCenter
                title: leader ? (Shell.apps.nameForApp(appId) || appId) : model.title
                iconSource: list.iconFor(appId)
                active: leader ? groupActive : model.active
                needsAttention: model.needsAttention
                groupCount: leader ? group.length : 0

                function popupX() { return button.mapToItem(null, 0, 0).x }

                onClicked: {
                    if (leader)
                        list.popups.open(groupComponent, { rows: group }, popupX(), false)
                    else
                        Shell.windows.taskButtonClicked(index)
                }
                onMiddleClicked: if (!grouped) Shell.windows.close(index)
                onContextMenuRequested: (x) => list.popups.open(menuComponent,
                    { entries: leader ? list.groupMenu(group) : list.windowMenu(index) }, popupX() + x, false)
            }
        }
    }
}
