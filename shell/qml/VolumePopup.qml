// SPDX-License-Identifier: GPL-3.0-or-later
// The tray volume popup: a beige panel with "Volume", a vertical slider with
// tick marks and a Mute check box. Changes apply immediately (wpctl).
import QtQuick
import Meadow.Controls
import Meadow.Shell

FocusScope {
    id: popup

    property var host: null
    readonly property var tk: Theme.tokens.tray.volume
    readonly property int pad: Theme.px(tk.padding)
    readonly property real travel: Theme.px(tk.sliderLength) - Theme.px(tk.thumbHeight)

    width: Theme.px(tk.width)
    height: column.implicitHeight + pad * 2
    focus: true

    Accessible.role: Accessible.Dialog
    Accessible.name: qsTr("Volume")

    function setVolume(v) { Shell.volume.volume = Math.max(0, Math.min(1, v)) }

    Keys.onUpPressed: setVolume(Shell.volume.volume + tk.keyStep)
    Keys.onDownPressed: setVolume(Shell.volume.volume - tk.keyStep)
    Keys.onSpacePressed: Shell.volume.muted = !Shell.volume.muted

    Rectangle {
        anchors.fill: parent
        color: popup.tk.background
        border.width: Theme.px(1)
        border.color: popup.tk.borderColor
    }

    Column {
        id: column
        x: popup.pad
        y: popup.pad
        width: popup.width - popup.pad * 2
        spacing: popup.pad

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Volume")
            font: Theme.fonts.ui
            color: Theme.tokens.colors.windowText
        }

        Item {
            id: slider
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            height: Theme.px(popup.tk.sliderLength)

            Accessible.role: Accessible.Slider
            Accessible.name: qsTr("Volume")

            // Tick marks on the left of the track.
            Repeater {
                model: popup.tk.tickCount
                delegate: Rectangle {
                    required property int index
                    x: slider.width / 2 - Theme.px(popup.tk.thumbWidth) / 2 - Theme.px(popup.tk.tickLength) - Theme.px(2)
                    y: Theme.px(popup.tk.thumbHeight) / 2 + popup.travel * index / (popup.tk.tickCount - 1)
                    width: Theme.px(popup.tk.tickLength)
                    height: Theme.px(1)
                    color: popup.tk.tickColor
                }
            }
            // Sunken track.
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Theme.px(popup.tk.trackWidth)
                height: parent.height
                color: popup.tk.trackColor
                border.width: Theme.px(1)
                border.color: popup.tk.trackBorderColor
            }
            GradientBox {
                id: thumb
                anchors.horizontalCenter: parent.horizontalCenter
                y: popup.travel * (1 - Shell.volume.volume)
                width: Theme.px(popup.tk.thumbWidth)
                height: Theme.px(popup.tk.thumbHeight)
                radius: Theme.px(2)
                stops: popup.tk.thumbGradient
                borderWidth: Theme.px(1)
                borderColor: popup.tk.thumbBorderColor
            }
            MouseArea {
                anchors.fill: parent
                function apply(y) { popup.setVolume(1 - (y - Theme.px(popup.tk.thumbHeight) / 2) / popup.travel) }
                onPressed: (event) => apply(event.y)
                onPositionChanged: (event) => { if (pressed) apply(event.y) }
                onWheel: (event) => popup.setVolume(Shell.volume.volume + (event.angleDelta.y > 0 ? 1 : -1) * popup.tk.wheelStep)
            }
        }

        LunaCheckBox {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Mute")
            checked: Shell.volume.muted
            onToggled: Shell.volume.muted = checked
        }
    }
}
