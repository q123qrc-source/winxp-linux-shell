// SPDX-License-Identifier: GPL-3.0-or-later
// Bottom-left "Turn off computer" button: red rounded power icon and label.
import QtQuick
import QtQuick.Templates as T
import Meadow.Controls

T.AbstractButton {
    id: control

    property bool highContrast: false
    readonly property var tk: Theme.tokens.welcome.turnOffButton

    text: qsTr("Turn off computer")
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    implicitWidth: contentItem.implicitWidth
    implicitHeight: contentItem.implicitHeight
    font: Theme.fonts.welcomeHint

    Accessible.role: Accessible.Button
    Accessible.name: text

    contentItem: Row {
        spacing: Theme.px(control.tk.spacing)

        GradientBox {
            width: Theme.px(control.tk.iconSize)
            height: width
            radius: Theme.px(Theme.tokens.controls.button.radius)
            stops: control.highContrast ? [] : control.tk.gradient
            color: Theme.tokens.colors.highContrastAccent
            borderWidth: Theme.px(1)
            borderColor: Theme.tokens.colors.white

            LunaGlyph {
                anchors.fill: parent
                anchors.margins: Theme.px(Theme.tokens.window.buttons.glyphInset)
                glyph: LunaGlyph.Power
                color: control.highContrast ? Theme.tokens.colors.highContrastBackground : Theme.tokens.colors.white
            }
            GradientBox {
                anchors.fill: parent
                radius: parent.radius
                color: control.down ? control.tk.pressedOverlay : control.hovered ? control.tk.hoverOverlay : "transparent"
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font.family: control.font.family
            font.pixelSize: control.font.pixelSize
            font.underline: control.hovered
            color: control.highContrast ? Theme.tokens.colors.highContrastText : control.tk.textColor

            GradientBox {
                anchors.fill: parent
                anchors.margins: -Theme.px(1)
                visible: control.visualFocus
                borderWidth: 1
                borderDotted: true
                borderColor: parent.color
            }
        }
    }
}
