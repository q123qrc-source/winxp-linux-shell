// SPDX-License-Identifier: GPL-3.0-or-later
// Shows desktop notifications (org.freedesktop.Notifications) one at a time
// as XP balloons whose tail points at the notification area. Clicking the
// balloon runs the "default" action; the close button dismisses it; the
// server expires it after its timeout.
import QtQuick
import Meadow.Controls
import Meadow.Shell

Item {
    id: host

    // Window-coordinate x the tail points at (the tray icons).
    property real targetX: width
    // Distance from the bottom of the host to the taskbar's top edge.
    property real bottomInset: 0
    readonly property bool showing: balloon.visible
    readonly property alias balloonWidth: balloon.width
    readonly property alias balloonHeight: balloon.height

    BalloonTip {
        id: balloon
        objectName: "balloon"
        readonly property var n: Shell.notifications.count > 0 ? Shell.notifications.current : null
        visible: false
        onNChanged: n ? show() : (visible = false)
        width: implicitWidth
        height: implicitHeight
        x: Math.max(0, Math.min(host.targetX - width + Theme.px(tk.tailWidth) * 2, host.width - width))
        y: host.height - host.bottomInset - height
        tailX: Math.max(Theme.px(tk.radius) * 2, Math.min(host.targetX - x, width - Theme.px(tk.radius) * 2))
        timeout: 0 // the server expires notifications
        title: n ? (n.summary.length > 0 ? n.summary : n.appName) : ""
        text: n ? n.body : ""
        textFormat: Text.StyledText
        icon: LunaGlyph.Info
        iconSource: {
            if (!n)
                return ""
            if (n.iconSource.length > 0)
                return n.iconSource
            const name = n.desktopEntry.length > 0 ? Shell.apps.iconForDesktopId(n.desktopEntry + ".desktop") : ""
            return name.length > 0 ? "image://icon/" + name : ""
        }
        actions: n ? n.actions.filter(a => a.key !== "default") : []

        onClicked: {
            if (!n)
                return
            if (n.actions.some(a => a.key === "default"))
                Shell.notifications.invokeAction(n.notificationId, "default")
            else
                Shell.notifications.dismiss(n.notificationId)
        }
        onActionTriggered: (key) => { if (n) Shell.notifications.invokeAction(n.notificationId, key) }
        // The close button fades the balloon out; then the server forgets it.
        onClosed: if (n) Shell.notifications.dismiss(n.notificationId)
    }
}
