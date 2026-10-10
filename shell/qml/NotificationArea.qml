// SPDX-License-Identifier: GPL-3.0-or-later
// The notification area: the lighter-blue segment at the right end of the
// taskbar. From left to right: the hide-inactive chevron, application tray
// icons (StatusNotifierItem), network, volume and battery, then the clock.
import QtQuick
import Meadow.Controls
import Meadow.Shell

Item {
    id: area

    property PopupHost popups: null
    // Show passive (inactive) icons although "hide inactive icons" is on.
    property bool expanded: false
    // Where balloons point, in this item's coordinates: the middle of the
    // icons, or of the clock when there are none.
    readonly property real balloonTargetLocalX: icons.width > 0 ? row.x + icons.x + icons.width / 2
                                                                : row.x + clock.x + clock.width / 2

    readonly property var tk: Theme.tokens.tray
    readonly property int hiddenCount: {
        let n = 0
        for (let i = 0; i < trayRepeater.count; ++i) {
            const item = trayRepeater.itemAt(i)
            if (item && item.passive)
                ++n
        }
        return n
    }
    readonly property bool hiding: Shell.settings.hideInactiveIcons && !expanded

    implicitWidth: Theme.px(tk.paddingLeft) + row.implicitWidth + Theme.px(tk.paddingRight)

    Accessible.role: Accessible.ToolBar
    Accessible.name: qsTr("Notification Area")

    function popupX(item) { return item.mapToItem(null, item.width, 0).x }

    function openCalendar() { popups.open(calendarComponent, {}, popupX(clock), true) }
    function openVolume() { popups.open(volumeComponent, {}, popupX(volumeIcon), true) }
    function openFirstItemMenu() {
        const item = trayRepeater.itemAt(0)
        if (!item) {
            // Not registered yet (preview start-up): try again shortly.
            firstItemRetry.start()
            return
        }
        const p = item.mapToItem(null, 0, 0)
        Shell.tray.requestMenu(item.itemKey, p.x, p.y)
    }
    Timer {
        id: firstItemRetry
        interval: Theme.tokens.animation.normal
        onTriggered: area.openFirstItemMenu()
    }
    function openNetwork() {
        openMenu([
            { label: networkText().split("\n").join(" — "), enabled: false },
            { separator: true },
            { label: qsTr("Open Network Connections"), bold: true, action: () => Shell.network.openSettings() },
        ], networkIcon)
    }

    function openMenu(entries, item) {
        popups.open(menuComponent, { entries: entries }, popupX(item), true)
    }

    // DBusMenu entries: clicking one is reported back to the application.
    function withTrayActions(key, entries) {
        return entries.map(e => Object.assign({}, e, {
            action: () => Shell.tray.triggerMenuItem(key, e.id),
            children: withTrayActions(key, e.children || [])
        }))
    }

    function batteryText() {
        const b = Shell.battery
        let text = b.charging ? qsTr("%1% available (charging)").arg(b.percent) : qsTr("%1% remaining").arg(b.percent)
        if (b.minutesLeft > 0) {
            const h = Math.floor(b.minutesLeft / 60) // theme-ok: minutes per hour
            const m = b.minutesLeft % 60 // theme-ok: minutes per hour
            text += " " + (h > 0 ? qsTr("(%1 hr %2 min)").arg(h).arg(m) : qsTr("(%1 min)").arg(m))
        }
        return text
    }

    function networkText() {
        const n = Shell.network
        if (!n.connected)
            return qsTr("Network cable unplugged or not connected")
        const name = n.connectionName.length > 0 ? n.connectionName : qsTr("Network connection")
        return n.limited ? qsTr("%1\nLimited or no connectivity").arg(name) : qsTr("%1\nConnected").arg(name)
    }

    Connections {
        target: Shell.tray
        function onMenuReady(key, entries) {
            const item = trayRepeater.itemFor(key)
            if (item && entries.length > 0)
                area.openMenu(area.withTrayActions(key, entries), item)
        }
    }

    Timer {
        id: collapse
        interval: area.tk.expandedTimeoutMs
        onTriggered: area.expanded = false
    }

    Component {
        id: menuComponent
        ShellMenu {}
    }
    Component {
        id: volumeComponent
        VolumePopup {}
    }
    Component {
        id: calendarComponent
        CalendarPopup {}
    }

    // Background: lighter gradient with a dark+light double line on the left.
    GradientBox {
        anchors.fill: parent
        stops: area.tk.gradient
    }
    Rectangle {
        width: Theme.px(area.tk.leftBorderWidth)
        height: parent.height
        color: area.tk.leftBorderColor
    }
    Rectangle {
        x: Theme.px(area.tk.leftBorderWidth)
        width: Theme.px(1)
        height: parent.height
        color: area.tk.leftBorderHighlight
    }

    Row {
        id: row
        x: Theme.px(area.tk.paddingLeft)
        height: parent.height
        spacing: Theme.px(area.tk.iconSpacing)

        // Chevron: shows (or hides again) the inactive icons.
        Item {
            id: chevron
            visible: Shell.settings.hideInactiveIcons && area.hiddenCount > 0
            width: Theme.px(area.tk.chevronSize)
            height: width
            anchors.verticalCenter: parent.verticalCenter

            Accessible.role: Accessible.Button
            Accessible.name: area.expanded ? qsTr("Hide inactive icons") : qsTr("Show hidden icons")
            Accessible.onPressAction: chevronMouse.clicked(null)

            GradientBox {
                anchors.fill: parent
                radius: width / 2
                stops: chevronMouse.containsMouse ? area.tk.chevronHoverGradient : area.tk.chevronGradient
                borderWidth: Theme.px(1)
                borderColor: area.tk.chevronBorderColor
            }
            LunaGlyph {
                anchors.fill: parent
                anchors.margins: Theme.px(area.tk.chevronGlyphInset)
                glyph: area.expanded ? LunaGlyph.ChevronRight : LunaGlyph.ChevronLeft
                color: area.tk.chevronColor
                lineWidth: Theme.px(1)
            }
            MouseArea {
                id: chevronMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    area.expanded = !area.expanded
                    if (area.expanded)
                        collapse.restart()
                }
                onContainsMouseChanged: containsMouse ? Tips.show(chevron, chevron.Accessible.name) : Tips.hide(chevron)
            }
        }

        Row {
            id: icons
            height: parent.height
            spacing: Theme.px(area.tk.iconSpacing)

            Repeater {
                id: trayRepeater
                model: Shell.tray

                function itemFor(key) {
                    for (let i = 0; i < count; ++i) {
                        if (itemAt(i) && itemAt(i).itemKey === key)
                            return itemAt(i)
                    }
                    return null
                }

                delegate: TrayButton {
                    id: trayItem
                    required property var model
                    readonly property string itemKey: model.key
                    readonly property bool passive: model.status === "Passive"

                    visible: !(area.hiding && passive)
                    anchors.verticalCenter: parent.verticalCenter
                    tipText: model.toolTip || model.title || ""

                    Image {
                        anchors.fill: parent
                        sourceSize: Qt.size(width, height)
                        source: trayItem.model.iconSource
                        smooth: true
                    }

                    onClicked: (button) => {
                        const p = trayItem.mapToItem(null, 0, 0)
                        if (button === Qt.RightButton)
                            Shell.tray.requestMenu(itemKey, p.x, p.y)
                        else if (button === Qt.MiddleButton)
                            Shell.tray.secondaryActivate(itemKey, p.x, p.y)
                        else
                            Shell.tray.activate(itemKey, p.x, p.y)
                    }
                    onWheel: (delta, horizontal) => Shell.tray.scroll(itemKey, delta, horizontal)
                }
            }

            TrayButton {
                id: networkIcon
                visible: Shell.network.available
                anchors.verticalCenter: parent.verticalCenter
                tipText: area.networkText()

                TrayGlyph {
                    anchors.fill: parent
                    kind: !Shell.network.connected ? TrayGlyph.NetworkOffline
                        : Shell.network.limited ? TrayGlyph.NetworkLimited
                        : Shell.network.kind === "wireless" ? TrayGlyph.NetworkWireless : TrayGlyph.NetworkWired
                }

                onClicked: area.openNetwork()
            }

            TrayButton {
                id: volumeIcon
                objectName: "volumeIcon"
                visible: Shell.volume.available
                anchors.verticalCenter: parent.verticalCenter
                tipText: Shell.volume.muted ? qsTr("Volume: muted") : qsTr("Volume: %1%").arg(Math.round(Shell.volume.volume * 100)) // theme-ok: percent

                TrayGlyph {
                    anchors.fill: parent
                    kind: Shell.volume.muted ? TrayGlyph.VolumeMuted : TrayGlyph.Volume
                }

                onClicked: (button) => {
                    if (button === Qt.RightButton)
                        area.openMenu([
                            { label: qsTr("Mute"), toggleType: "checkmark", checked: Shell.volume.muted,
                              action: () => Shell.volume.muted = !Shell.volume.muted },
                        ], volumeIcon)
                    else if (button === Qt.LeftButton)
                        area.openVolume()
                }
                onWheel: (delta) => Shell.volume.volume = Math.max(0, Math.min(1, Shell.volume.volume + (delta > 0 ? 1 : -1) * area.tk.volume.wheelStep))
            }

            TrayButton {
                id: batteryIcon
                visible: Shell.battery.present
                anchors.verticalCenter: parent.verticalCenter
                tipText: area.batteryText()

                TrayGlyph {
                    anchors.fill: parent
                    kind: Shell.battery.charging ? TrayGlyph.BatteryCharging : TrayGlyph.Battery
                    level: Shell.battery.percent / 100 // theme-ok: percent
                }
            }
        }

        Clock {
            id: clock
            objectName: "clock"
            visible: Shell.settings.showClock
            height: parent.height
            onClicked: area.openCalendar()
        }
    }
}
