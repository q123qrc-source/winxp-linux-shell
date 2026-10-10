// SPDX-License-Identifier: GPL-3.0-or-later
// Popup list of the windows behind a group button; choosing one activates it.
import QtQuick
import Meadow.Shell

ShellMenu {
    id: menu

    // Model rows of the grouped windows.
    property var rows: []

    entries: rows.map(r => {
        const w = Shell.windows.get(r)
        const icon = Shell.apps.iconForApp(w.appId)
        return { label: w.title, bold: w.active,
                 iconSource: "image://icon/" + (icon.length > 0 ? icon : w.appId),
                 action: () => Shell.windows.activate(r) }
    })
}
