// SPDX-License-Identifier: GPL-3.0-or-later
// Single-line text input: white field, thin blue-grey border that turns
// selection blue while focused.
import QtQuick
import QtQuick.Templates as T
import Meadow.Controls

T.TextField {
    id: control

    // "" or "focus" (gallery and screenshot tests only).
    property string previewState: ""

    readonly property var tk: Theme.tokens.controls.textField
    readonly property bool showFocus: enabled && (activeFocus || previewState === "focus")

    implicitWidth: Math.max(Theme.px(tk.defaultWidth), contentWidth + leftPadding + rightPadding)
    implicitHeight: Theme.px(tk.height)
    padding: Theme.px(tk.padding)
    verticalAlignment: TextInput.AlignVCenter
    font: Theme.fonts.ui
    color: enabled ? tk.textColor : Theme.tokens.colors.disabledText
    selectionColor: tk.selectionColor
    selectedTextColor: Theme.tokens.colors.selectionText
    placeholderTextColor: Theme.tokens.colors.disabledText

    Accessible.role: Accessible.EditableText
    Accessible.name: placeholderText

    Text {
        x: control.leftPadding
        y: control.topPadding
        width: control.width - control.leftPadding - control.rightPadding
        height: control.height - control.topPadding - control.bottomPadding
        visible: control.length === 0 && control.preeditText.length === 0 && !control.activeFocus
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: GradientBox {
        implicitWidth: Theme.px(control.tk.defaultWidth)
        implicitHeight: Theme.px(control.tk.height)
        color: control.enabled ? control.tk.background : control.tk.disabledBackground
        borderWidth: Theme.px(1)
        borderColor: control.showFocus ? control.tk.focusBorderColor : control.tk.borderColor
    }
}
