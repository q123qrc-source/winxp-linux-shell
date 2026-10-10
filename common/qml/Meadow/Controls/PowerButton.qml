// SPDX-License-Identifier: GPL-3.0-or-later
// A big glossy button with a glyph and a caption under it, as in the Turn Off
// Computer (round) and Log Off (rounded square) dialogs.
import QtQuick
import Meadow.Controls

FocusScope {
    id: button

    property string label
    property var stops: []
    property int glyph: LunaGlyph.None
    // Rounded square instead of a circle.
    property bool square: false
    property bool highContrast: false

    readonly property var tk: Theme.tokens.dialogs.turnOff

    signal triggered()

    width: Math.max(face.width, caption.implicitWidth)
    height: face.height + Theme.px(tk.labelSpacing) + caption.implicitHeight
    activeFocusOnTab: true
    Keys.onReturnPressed: triggered()
    Keys.onEnterPressed: triggered()
    Keys.onSpacePressed: triggered()
    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.onPressAction: triggered()

    GradientBox {
        id: face
        anchors.horizontalCenter: parent.horizontalCenter
        width: Theme.px(button.tk.roundButtonSize)
        height: width
        radius: button.square ? Theme.px(Theme.tokens.dialogs.logOff.buttonRadius) : width / 2
        stops: button.highContrast ? [] : button.stops
        color: Theme.tokens.colors.highContrastAccent
        borderWidth: Theme.px(1)
        borderColor: Theme.tokens.colors.white

        LunaGlyph {
            anchors.fill: parent
            anchors.margins: Theme.px(Theme.tokens.window.buttons.glyphInset)
            glyph: button.glyph
            color: button.highContrast ? Theme.tokens.colors.highContrastBackground : button.tk.glyphColor
        }
        GradientBox {
            anchors.fill: parent
            radius: parent.radius
            color: area.pressed ? button.tk.pressedOverlay : area.containsMouse ? button.tk.hoverOverlay : "transparent"
        }
    }
    Text {
        id: caption
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: face.bottom
        anchors.topMargin: Theme.px(button.tk.labelSpacing)
        text: button.label
        font: Theme.fonts.ui
        color: button.highContrast ? Theme.tokens.colors.highContrastText : button.tk.labelColor

        GradientBox {
            anchors.fill: parent
            anchors.margins: -Theme.px(1)
            visible: button.activeFocus
            borderWidth: 1
            borderDotted: true
            borderColor: parent.color
        }
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: button.triggered()
    }
}
