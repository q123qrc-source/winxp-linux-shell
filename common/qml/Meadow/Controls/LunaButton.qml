// SPDX-License-Identifier: GPL-3.0-or-later
// Push button: cream face, dark-blue rounded border, an orange inner ring on
// hover and a blue inner ring for the default or focused button.
import QtQuick
import QtQuick.Templates as T
import Meadow.Controls

T.Button {
    id: control

    // The dialog's default button: blue ring, activated by Enter.
    property bool isDefault: false
    // Forces a visual state for the gallery and screenshot tests:
    // "", "hover", "pressed" or "focus".
    property string previewState: ""

    readonly property var tk: Theme.tokens.controls.button
    readonly property bool showPressed: enabled && (down || previewState === "pressed")
    readonly property bool showHover: enabled && !showPressed && (hovered || previewState === "hover")
    readonly property bool showFocus: enabled && (visualFocus || previewState === "focus")
    readonly property var ringColors: showHover ? tk.hoverRingColors
                                    : (!showPressed && enabled && (isDefault || showFocus)) ? tk.defaultRingColors : []

    function ringStops(colors) {
        const stops = []
        for (let i = 0; i < colors.length; ++i)
            stops.push({ pos: colors.length > 1 ? i / (colors.length - 1) : 0, color: colors[i] })
        return stops
    }

    implicitWidth: Math.max(Theme.px(tk.minWidth), implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Theme.px(tk.height)
    leftPadding: Theme.px(tk.paddingH)
    rightPadding: Theme.px(tk.paddingH)
    font: Theme.fonts.ui
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    Accessible.role: Accessible.Button
    Accessible.name: text

    Keys.onReturnPressed: if (isDefault) clicked()
    Keys.onEnterPressed: if (isDefault) clicked()

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? control.tk.textColor : Theme.tokens.colors.disabledText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Item {
        implicitWidth: Theme.px(control.tk.minWidth)
        implicitHeight: Theme.px(control.tk.height)

        // Border plus, when a ring is shown, the ring gradient under the face.
        GradientBox {
            id: frame
            anchors.fill: parent
            radius: Theme.px(control.tk.radius)
            borderWidth: Theme.px(1)
            borderColor: control.enabled ? control.tk.borderColor : control.tk.disabledBorderColor
            stops: control.ringColors.length > 0 ? control.ringStops(control.ringColors) : face.stops
        }

        GradientBox {
            id: face
            anchors.fill: parent
            anchors.margins: frame.borderWidth + (control.ringColors.length > 0 ? Theme.px(control.tk.ringWidth) : 0)
            radius: Math.max(0, frame.radius - anchors.margins)
            stops: !control.enabled ? control.tk.gradient.disabled
                 : control.showPressed ? control.tk.gradient.pressed
                 : control.showHover ? control.tk.gradient.hover
                 : control.tk.gradient.normal
        }

        GradientBox {
            visible: control.showFocus
            anchors.fill: parent
            anchors.margins: Theme.px(control.tk.focusRingInset)
            borderWidth: 1
            borderDotted: true
            borderColor: Theme.tokens.colors.focusRing
        }
    }
}
