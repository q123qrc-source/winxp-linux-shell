// SPDX-License-Identifier: GPL-3.0-or-later
// Check box: small gradient square with a dark-blue border, an orange ring on
// hover and a green check mark.
import QtQuick
import QtQuick.Templates as T
import Meadow.Controls

T.CheckBox {
    id: control

    // "", "hover", "pressed" or "focus" (gallery and screenshot tests only).
    property string previewState: ""

    readonly property var tk: Theme.tokens.controls.checkBox
    readonly property bool showPressed: enabled && (down || previewState === "pressed")
    readonly property bool showHover: enabled && (hovered || previewState === "hover" || showPressed)
    readonly property bool showFocus: enabled && (visualFocus || previewState === "focus")

    implicitWidth: leftPadding + implicitContentWidth + rightPadding
    implicitHeight: Math.max(implicitIndicatorHeight, implicitContentHeight)
    spacing: Theme.px(tk.spacing)
    font: Theme.fonts.ui
    hoverEnabled: true

    Accessible.role: Accessible.CheckBox
    Accessible.name: text

    indicator: GradientBox {
        implicitWidth: Theme.px(control.tk.size)
        implicitHeight: Theme.px(control.tk.size)
        x: control.leftPadding
        y: (control.height - height) / 2
        borderWidth: Theme.px(1)
        borderColor: control.enabled ? control.tk.borderColor : Theme.tokens.colors.disabledText
        // Pressing inverts the face gradient for a sunken look.
        stops: {
            const s = control.tk.gradient
            if (!control.enabled)
                return []
            if (!control.showPressed)
                return s
            const out = []
            for (let i = s.length - 1; i >= 0; --i)
                out.push({ pos: 1 - s[i].pos, color: s[i].color })
            return out
        }
        color: control.enabled ? "transparent" : Theme.tokens.colors.windowBackground

        GradientBox {
            anchors.fill: parent
            anchors.margins: Theme.px(1)
            visible: control.showHover
            borderWidth: Theme.px(2)
            borderColor: control.tk.hoverRingColor
        }

        LunaGlyph {
            anchors.fill: parent
            anchors.margins: Theme.px(2)
            visible: control.checkState === Qt.Checked
            glyph: LunaGlyph.Check
            color: control.enabled ? control.tk.checkColor : Theme.tokens.colors.disabledText
        }

        // Partially checked: a filled square.
        GradientBox {
            anchors.centerIn: parent
            width: parent.width / 2
            height: width
            visible: control.checkState === Qt.PartiallyChecked
            color: control.enabled ? control.tk.checkColor : Theme.tokens.colors.disabledText
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
