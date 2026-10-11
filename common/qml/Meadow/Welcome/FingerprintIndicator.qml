// SPDX-License-Identifier: GPL-3.0-or-later
// "Touch the fingerprint reader" prompt shown in the selected tile while a
// fingerprint attempt is running. The text follows the messages pam_fprintd
// sends through SDDM ("Place your finger...", "Failed to match...").
import QtQuick
import Meadow.Controls

Row {
    id: root

    property bool active: false
    property string text
    property bool highContrast: false

    readonly property var tk: Theme.tokens.welcome.fingerprint

    visible: active
    spacing: Theme.px(tk.spacing)

    Accessible.role: Accessible.StaticText
    Accessible.name: text

    LunaGlyph {
        id: icon
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.px(root.tk.iconSize)
        height: width
        glyph: LunaGlyph.Fingerprint
        color: root.highContrast ? Theme.tokens.colors.highContrastAccent : root.tk.textColor

        SequentialAnimation on opacity {
            running: root.active && !Theme.reducedMotion
            loops: Animation.Infinite
            NumberAnimation { to: root.tk.minOpacity; duration: root.tk.pulseMs / 2 }
            NumberAnimation { to: 1; duration: root.tk.pulseMs / 2 }
            onStopped: icon.opacity = 1
        }
    }
    Text {
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(implicitWidth, Theme.px(root.tk.maxWidth))
        text: root.text
        font: Theme.fonts.ui
        color: root.highContrast ? Theme.tokens.colors.highContrastText : root.tk.textColor
        wrapMode: Text.Wrap
    }
}
