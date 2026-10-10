// SPDX-License-Identifier: GPL-3.0-or-later
// Password row shown under the selected user's name: field, green arrow
// (log on) and blue "?" (password hint).
import QtQuick
import Meadow.Controls

Column {
    id: root

    property bool busy: false
    property bool highContrast: false
    // "" or "focus" (preview/screenshot only).
    property string previewState: ""
    readonly property alias field: field
    readonly property var tk: Theme.tokens.welcome.passwordPanel

    signal submitted(string password)
    signal hintRequested()
    signal cancelled()

    function clear() { field.text = "" }
    function focusField() { field.forceActiveFocus() }
    function shake() { shakeAnimation.restart() }

    spacing: Theme.px(tk.spacing)

    Text {
        text: root.busy ? qsTr("Logging on…") : qsTr("Type your password")
        font: Theme.fonts.ui
        color: root.highContrast ? Theme.tokens.colors.highContrastText : root.tk.labelColor
    }

    Row {
        id: row
        spacing: Theme.px(root.tk.spacing)

        transform: Translate { id: shakeOffset }

        LunaTextField {
            id: field
            width: Theme.px(root.tk.fieldWidth)
            height: Theme.px(root.tk.fieldHeight)
            echoMode: TextInput.Password
            passwordCharacter: "●"
            enabled: !root.busy
            previewState: root.previewState
            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            Accessible.name: qsTr("Password")
            Keys.onReturnPressed: root.submitted(text)
            Keys.onEnterPressed: root.submitted(text)
            Keys.onEscapePressed: root.cancelled()
        }

        GradientBox {
            id: submit
            width: Theme.px(root.tk.submitSize)
            height: width
            radius: Theme.px(Theme.tokens.controls.button.radius)
            stops: root.highContrast ? [] : root.tk.submitGradient
            color: Theme.tokens.colors.highContrastAccent
            borderWidth: Theme.px(1)
            borderColor: Theme.tokens.colors.white
            opacity: root.busy ? 0.5 : 1

            LunaGlyph {
                anchors.fill: parent
                anchors.margins: Theme.px(Theme.tokens.window.buttons.glyphInset)
                // Points toward the reading direction's end.
                glyph: root.LayoutMirroring.enabled ? LunaGlyph.ArrowLeft : LunaGlyph.ArrowRight
                color: root.highContrast ? Theme.tokens.colors.highContrastBackground : Theme.tokens.colors.white
            }
            GradientBox {
                anchors.fill: parent
                radius: parent.radius
                color: submitArea.pressed ? Theme.tokens.welcome.turnOffButton.pressedOverlay
                     : submitArea.containsMouse ? Theme.tokens.welcome.turnOffButton.hoverOverlay : "transparent"
            }
            MouseArea {
                id: submitArea
                anchors.fill: parent
                hoverEnabled: true
                enabled: !root.busy
                onClicked: root.submitted(field.text)
            }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Log on")
            Accessible.onPressAction: root.submitted(field.text)
        }

        GradientBox {
            width: Theme.px(root.tk.hintButtonSize)
            height: width
            radius: Theme.px(Theme.tokens.controls.button.radius)
            stops: root.highContrast ? [] : root.tk.hintGradient
            color: Theme.tokens.colors.highContrastAccent
            borderWidth: Theme.px(1)
            borderColor: Theme.tokens.colors.white

            Text {
                anchors.centerIn: parent
                text: "?"
                font: Theme.fonts.menuHeader
                color: root.highContrast ? Theme.tokens.colors.highContrastBackground : Theme.tokens.colors.white
            }
            GradientBox {
                anchors.fill: parent
                radius: parent.radius
                color: hintArea.pressed ? Theme.tokens.welcome.turnOffButton.pressedOverlay
                     : hintArea.containsMouse ? Theme.tokens.welcome.turnOffButton.hoverOverlay : "transparent"
            }
            MouseArea {
                id: hintArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.hintRequested()
            }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Show password hint")
            Accessible.onPressAction: root.hintRequested()
        }
    }

    SequentialAnimation {
        id: shakeAnimation
        readonly property real distance: Theme.px(root.tk.shakeDistance)
        readonly property int step: Theme.reducedMotion ? 0 : Theme.tokens.welcome.passwordPanel.shakeDurationMs / 4 // theme-ok: the shake has four swings
        NumberAnimation { target: shakeOffset; property: "x"; to: -shakeAnimation.distance; duration: shakeAnimation.step / 2 }
        NumberAnimation { target: shakeOffset; property: "x"; to: shakeAnimation.distance; duration: shakeAnimation.step }
        NumberAnimation { target: shakeOffset; property: "x"; to: -shakeAnimation.distance / 2; duration: shakeAnimation.step }
        NumberAnimation { target: shakeOffset; property: "x"; to: 0; duration: shakeAnimation.step / 2 }
    }
}
