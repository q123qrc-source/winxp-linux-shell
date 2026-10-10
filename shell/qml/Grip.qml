// SPDX-License-Identifier: GPL-3.0-or-later
// Dotted toolbar grip shown in front of Quick Launch and the task list while
// the taskbar is unlocked.
import QtQuick
import Meadow.Controls

Item {
    id: root

    readonly property var tk: Theme.tokens.quickLaunch
    readonly property int pitch: Theme.px(tk.gripDotPitch)

    implicitWidth: Theme.px(tk.gripWidth)

    Column {
        anchors.centerIn: parent
        spacing: root.pitch - Theme.px(2)
        Repeater {
            model: Math.max(0, Math.floor((root.height - root.pitch) / root.pitch))
            delegate: Item {
                width: Theme.px(2) + Theme.px(1)
                height: Theme.px(2)
                Rectangle { x: Theme.px(1); y: Theme.px(1); width: Theme.px(2); height: width; color: root.tk.gripDotHighlight }
                Rectangle { width: Theme.px(2); height: width; color: root.tk.gripDotColor }
            }
        }
    }
}
