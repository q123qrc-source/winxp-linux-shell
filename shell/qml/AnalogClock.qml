// SPDX-License-Identifier: GPL-3.0-or-later
// A round clock face with hour ticks and three hands, drawn from plain
// rectangles (works with every renderer). Shows `time`.
import QtQuick
import Meadow.Controls

Item {
    id: clock

    property date time: new Date()
    readonly property var tk: Theme.tokens.dialogs.controlPanel
    readonly property real r: Math.min(width, height) / 2
    readonly property int hours: 12 // theme-ok: hours on a clock face
    readonly property int sixty: 60 // theme-ok: minutes/seconds per turn
    readonly property real fullTurn: 360 // theme-ok: degrees

    implicitWidth: Theme.px(tk.clockSize)
    implicitHeight: implicitWidth

    Accessible.role: Accessible.Clock
    Accessible.name: Qt.formatTime(time, Qt.locale().timeFormat(Locale.LongFormat))

    Rectangle {
        anchors.centerIn: parent
        width: clock.r * 2
        height: width
        radius: width / 2
        color: clock.tk.clockFace
        border.color: clock.tk.clockRim
        border.width: Theme.px(2)
    }
    Repeater {
        model: clock.hours
        delegate: Rectangle {
            required property int index
            id: tick
            readonly property bool major: index % 3 === 0 // theme-ok: quarter hours
            readonly property int inset: Theme.px(clock.tk.clockTickInset)
            x: clock.width / 2 - width / 2
            y: clock.height / 2 - clock.r + inset
            width: major ? Theme.px(clock.tk.clockMajorTickWidth) : Theme.px(2)
            height: major ? Theme.px(clock.tk.clockMajorTickLength) : Theme.px(clock.tk.clockMinorTickLength)
            color: clock.tk.clockTick
            transform: Rotation { origin.x: tick.width / 2; origin.y: clock.r - tick.inset; angle: tick.index * clock.fullTurn / clock.hours }
        }
    }

    component Hand: Rectangle {
        id: hand
        property real handAngle
        property real length
        x: clock.width / 2 - width / 2
        y: clock.height / 2 - length
        height: length
        radius: width / 2
        transform: Rotation { origin.x: hand.width / 2; origin.y: hand.length; angle: hand.handAngle }
    }
    Hand {
        width: Theme.px(clock.tk.clockHourHandWidth)
        length: clock.r * 0.5 // theme-ok: hand length ratio
        color: clock.tk.clockHand
        handAngle: ((clock.time.getHours() % clock.hours) + clock.time.getMinutes() / clock.sixty) * clock.fullTurn / clock.hours
    }
    Hand {
        width: Theme.px(clock.tk.clockMinuteHandWidth)
        length: clock.r * 0.75 // theme-ok: hand length ratio
        color: clock.tk.clockHand
        handAngle: (clock.time.getMinutes() + clock.time.getSeconds() / clock.sixty) * clock.fullTurn / clock.sixty
    }
    Hand {
        width: Theme.px(1)
        length: clock.r * 0.85 // theme-ok: hand length ratio
        color: clock.tk.clockSecondHand
        handAngle: clock.time.getSeconds() * clock.fullTurn / clock.sixty
    }
    Rectangle {
        anchors.centerIn: parent
        width: Theme.px(clock.tk.clockCenterSize)
        height: width
        radius: width / 2
        color: clock.tk.clockHand
    }
}
