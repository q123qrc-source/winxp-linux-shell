// SPDX-License-Identifier: GPL-3.0-or-later
// Pale-yellow tooltip with a thin black border.
import QtQuick
import QtQuick.Templates as T
import Meadow.Controls

T.ToolTip {
    id: control

    readonly property var tk: Theme.tokens.controls.tooltip

    x: 0
    y: parent ? Theme.px(tk.offsetY) : 0
    delay: tk.delayMs
    timeout: tk.timeoutMs
    padding: Theme.px(tk.padding)
    font: Theme.fonts.ui
    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.tk.textColor
    }

    background: GradientBox {
        color: control.tk.background
        borderWidth: Theme.px(1)
        borderColor: control.tk.borderColor
    }
}
