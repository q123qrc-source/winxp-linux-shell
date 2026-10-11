// SPDX-License-Identifier: GPL-3.0-or-later
// Track bar: a thin sunken track, tick marks underneath (tickCount, 0 for
// none) and a light thumb with a blue edge that turns orange on hover.
// Horizontal by default; `orientation: Qt.Vertical` is supported too.
import QtQuick
import QtQuick.Templates as T
import Meadow.Controls

T.Slider {
    id: control

    property int tickCount: 0
    // "", "hover" or "pressed" (gallery and screenshot tests only).
    property string previewState: ""

    readonly property var tk: Theme.tokens.controls.slider
    readonly property bool isHorizontal: orientation === Qt.Horizontal
    readonly property bool showHover: enabled && (hovered || pressed || previewState !== "")

    implicitWidth: isHorizontal ? Theme.px(tk.defaultLength) : Theme.px(tk.thumbLength) + Theme.px(tk.tickLength) * 2
    implicitHeight: isHorizontal ? Theme.px(tk.thumbLength) + Theme.px(tk.tickLength) * 2 : Theme.px(tk.defaultLength)
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.Slider

    background: Item {
        x: control.leftPadding
        y: control.topPadding
        width: control.availableWidth
        height: control.availableHeight

        // Track.
        Rectangle {
            readonly property int thick: Theme.px(control.tk.trackWidth)
            x: control.isHorizontal ? control.handle.width / 2 : (control.handle.width - thick) / 2
            y: control.isHorizontal ? (control.handle.height - thick) / 2 : control.handle.height / 2
            width: control.isHorizontal ? parent.width - control.handle.width : thick
            height: control.isHorizontal ? thick : parent.height - control.handle.height
            color: control.tk.trackColor
            border.color: control.tk.trackBorderColor
            border.width: Theme.px(1)
        }
        // Ticks below (or right of) the track.
        Repeater {
            model: control.tickCount
            delegate: Rectangle {
                required property int index
                readonly property real t: control.tickCount > 1 ? index / (control.tickCount - 1) : 0
                x: control.isHorizontal ? control.handle.width / 2 + t * (parent.width - control.handle.width)
                                      : control.handle.width + Theme.px(1)
                y: control.isHorizontal ? control.handle.height + Theme.px(1)
                                      : control.handle.height / 2 + t * (parent.height - control.handle.height)
                width: control.isHorizontal ? Theme.px(1) : Theme.px(control.tk.tickLength)
                height: control.isHorizontal ? Theme.px(control.tk.tickLength) : Theme.px(1)
                color: control.tk.tickColor
            }
        }
    }

    handle: GradientBox {
        x: control.leftPadding + (control.isHorizontal ? control.visualPosition * (control.availableWidth - width) : 0)
        y: control.topPadding + (control.isHorizontal ? 0 : control.visualPosition * (control.availableHeight - height))
        width: control.isHorizontal ? Theme.px(control.tk.thumbWidth) : Theme.px(control.tk.thumbLength)
        height: control.isHorizontal ? Theme.px(control.tk.thumbLength) : Theme.px(control.tk.thumbWidth)
        radius: Theme.px(control.tk.thumbRadius)
        borderWidth: Theme.px(1)
        borderColor: !control.enabled ? Theme.tokens.colors.disabledText
                   : control.showHover ? control.tk.thumbHoverBorderColor : control.tk.thumbBorderColor
        stops: control.tk.thumbGradient

        // Keyboard focus: dotted rectangle around the whole control.
        GradientBox {
            parent: control
            anchors.fill: parent
            visible: control.visualFocus
            borderWidth: 1
            borderDotted: true
            borderColor: Theme.tokens.colors.focusRing
        }
    }
}
