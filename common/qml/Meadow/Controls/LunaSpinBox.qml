// SPDX-License-Identifier: GPL-3.0-or-later
// Spin box: a text field with small up/down buttons in the combo-box style.
import QtQuick
import QtQuick.Templates as T
import Meadow.Controls

T.SpinBox {
    id: control

    readonly property var tk: Theme.tokens.controls.spinBox
    readonly property var field: Theme.tokens.controls.textField

    implicitWidth: Theme.px(tk.width)
    implicitHeight: Theme.px(field.height)
    editable: true
    font: Theme.fonts.ui
    rightPadding: Theme.px(tk.buttonWidth) + Theme.px(1)

    Accessible.role: Accessible.SpinBox

    validator: IntValidator {
        bottom: Math.min(control.from, control.to)
        top: Math.max(control.from, control.to)
    }

    contentItem: TextInput {
        text: control.displayText
        font: control.font
        color: control.enabled ? control.field.textColor : Theme.tokens.colors.disabledText
        selectionColor: control.field.selectionColor
        selectedTextColor: Theme.tokens.colors.selectionText
        horizontalAlignment: Qt.AlignRight
        verticalAlignment: Qt.AlignVCenter
        leftPadding: Theme.px(control.field.padding)
        rightPadding: Theme.px(control.field.padding)
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhDigitsOnly
        selectByMouse: true
    }

    background: Rectangle {
        color: control.enabled ? control.field.background : control.field.disabledBackground
        border.color: control.activeFocus ? control.field.focusBorderColor : control.field.borderColor
        border.width: Theme.px(1)
    }

    up.indicator: GradientBox {
        x: control.width - width - Theme.px(1)
        y: Theme.px(1)
        width: Theme.px(control.tk.buttonWidth)
        height: control.height / 2 - Theme.px(1)
        radius: Theme.px(control.tk.buttonRadius)
        stops: control.up.pressed ? control.tk.buttonPressedGradient
             : control.up.hovered ? control.tk.buttonHoverGradient : control.tk.buttonGradient
        LunaGlyph {
            anchors.fill: parent
            anchors.margins: Theme.px(2)
            glyph: LunaGlyph.ArrowUp
            color: control.enabled ? control.tk.arrowColor : Theme.tokens.colors.disabledText
        }
    }
    down.indicator: GradientBox {
        x: control.width - width - Theme.px(1)
        y: control.height / 2
        width: Theme.px(control.tk.buttonWidth)
        height: control.height / 2 - Theme.px(1)
        radius: Theme.px(control.tk.buttonRadius)
        stops: control.down.pressed ? control.tk.buttonPressedGradient
             : control.down.hovered ? control.tk.buttonHoverGradient : control.tk.buttonGradient
        LunaGlyph {
            anchors.fill: parent
            anchors.margins: Theme.px(2)
            glyph: LunaGlyph.ArrowDown
            color: control.enabled ? control.tk.arrowColor : Theme.tokens.colors.disabledText
        }
    }
}
