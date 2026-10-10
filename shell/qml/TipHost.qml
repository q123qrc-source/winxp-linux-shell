// SPDX-License-Identifier: GPL-3.0-or-later
// Draws the tooltip requested through Tips just above the taskbar, after the
// usual tooltip delay, and hides it after the timeout.
import QtQuick
import Meadow.Controls

Item {
    id: host

    // Distance from the bottom of the host to the taskbar's top edge.
    property real bottomInset: 0
    property bool shown: false

    readonly property var tk: Theme.tokens.controls.tooltip

    Connections {
        target: Tips
        function onTextChanged() {
            host.shown = false
            if (Tips.text.length > 0)
                delay.restart()
            else
                delay.stop()
        }
    }
    Timer {
        id: delay
        interval: host.tk.delayMs
        onTriggered: {
            host.shown = true
            timeout.restart()
        }
    }
    Timer {
        id: timeout
        interval: host.tk.timeoutMs
        onTriggered: host.shown = false
    }

    GradientBox {
        visible: host.shown
        width: label.implicitWidth + Theme.px(host.tk.padding) * 2
        height: label.implicitHeight + Theme.px(host.tk.padding) * 2
        x: Math.max(0, Math.min(Tips.x - width / 2, host.width - width))
        y: host.height - host.bottomInset - height - Theme.px(host.tk.padding)
        color: host.tk.background
        borderWidth: Theme.px(1)
        borderColor: host.tk.borderColor

        Text {
            id: label
            anchors.centerIn: parent
            text: Tips.text
            font: Theme.fonts.ui
            color: host.tk.textColor
        }
    }
}
