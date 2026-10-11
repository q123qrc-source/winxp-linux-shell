// SPDX-License-Identifier: GPL-3.0-or-later
// Small translucent control for the Welcome screen's bottom band (session,
// keyboard layout, accessibility toggles).
import QtQuick
import QtQuick.Templates as T
import Meadow.Controls

T.AbstractButton {
    id: control

    property int glyph: LunaGlyph.None
    property bool highContrast: false
    // "" or "hover" (preview/screenshot only).
    property string previewState: ""

    readonly property var tk: Theme.tokens.welcome.bottomControls
    readonly property color ink: highContrast ? Theme.tokens.colors.highContrastText : tk.textColor

    implicitHeight: Theme.px(tk.height)
    implicitWidth: Math.max(implicitHeight, implicitContentWidth + leftPadding + rightPadding)
    leftPadding: Theme.px(tk.paddingH)
    rightPadding: Theme.px(tk.paddingH)
    font: Theme.fonts.ui
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.Button
    Accessible.name: text

    contentItem: Row {
        spacing: Theme.px(control.tk.spacing)
        LunaGlyph {
            anchors.verticalCenter: parent.verticalCenter
            width: control.glyph === LunaGlyph.None ? 0 : Theme.px(Theme.tokens.controls.balloon.iconSize)
            height: width
            visible: control.glyph !== LunaGlyph.None
            glyph: control.glyph
            color: control.ink
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: control.text.length > 0
            text: control.text
            font: control.font
            color: control.ink
        }
    }

    background: GradientBox {
        radius: Theme.px(control.tk.radius)
        color: control.highContrast ? Theme.tokens.colors.highContrastBackground
             : (control.hovered || control.down || control.previewState === "hover") ? control.tk.hoverBackground
             : control.tk.background
        borderWidth: Theme.px(1)
        borderColor: control.highContrast ? Theme.tokens.colors.highContrastAccent : control.tk.border

        GradientBox {
            anchors.fill: parent
            anchors.margins: Theme.px(2)
            visible: control.visualFocus
            borderWidth: 1
            borderDotted: true
            borderColor: control.ink
        }
    }
}
