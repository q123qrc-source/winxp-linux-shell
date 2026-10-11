// SPDX-License-Identifier: GPL-3.0-or-later
// Radio button: a round version of the check box with a green dot.
import QtQuick
import QtQuick.Templates as T
import Meadow.Controls

T.RadioButton {
    id: control

    // "", "hover", "pressed" or "focus" (gallery and screenshot tests only).
    property string previewState: ""

    readonly property var tk: Theme.tokens.controls.radio
    readonly property bool showPressed: enabled && (down || previewState === "pressed")
    readonly property bool showHover: enabled && (hovered || previewState === "hover" || showPressed)
    readonly property bool showFocus: enabled && (visualFocus || previewState === "focus")

    implicitWidth: leftPadding + implicitContentWidth + rightPadding
    implicitHeight: Math.max(implicitIndicatorHeight, implicitContentHeight)
    spacing: Theme.px(tk.spacing)
    font: Theme.fonts.ui
    hoverEnabled: true

    Accessible.role: Accessible.RadioButton
    Accessible.name: text

    indicator: GradientBox {
        implicitWidth: Theme.px(control.tk.size)
        implicitHeight: Theme.px(control.tk.size)
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: width / 2
        borderWidth: Theme.px(1)
        borderColor: control.enabled ? control.tk.borderColor : Theme.tokens.colors.disabledText
        stops: control.enabled ? (control.showPressed ? control.tk.pressedGradient : control.tk.gradient) : []
        color: control.enabled ? "transparent" : Theme.tokens.colors.windowBackground

        GradientBox {
            anchors.fill: parent
            anchors.margins: Theme.px(1)
            radius: width / 2
            visible: control.showHover
            borderWidth: Theme.px(2)
            borderColor: control.tk.hoverRingColor
        }
        GradientBox {
            anchors.centerIn: parent
            width: Theme.px(control.tk.dotSize)
            height: width
            radius: width / 2
            visible: control.checked
            stops: control.enabled ? control.tk.dotGradient : []
            color: Theme.tokens.colors.disabledText
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        color: control.enabled ? Theme.tokens.colors.windowText : Theme.tokens.colors.disabledText
        verticalAlignment: Text.AlignVCenter

        GradientBox {
            x: control.indicator.width + control.spacing - Theme.px(1)
            y: (parent.height - height) / 2
            width: parent.contentWidth + Theme.px(2)
            height: parent.contentHeight + Theme.px(1)
            visible: control.showFocus
            borderWidth: 1
            borderDotted: true
            borderColor: Theme.tokens.colors.focusRing
        }
    }
}
