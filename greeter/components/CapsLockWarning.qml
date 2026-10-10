// SPDX-License-Identifier: GPL-3.0-or-later
// Balloon under the password field while Caps Lock is on.
import QtQuick
import Meadow.Controls

BalloonTip {
    title: qsTr("Caps Lock is on")
    text: qsTr("Passwords are case-sensitive. Press Caps Lock to turn it off before typing your password.")
    icon: LunaGlyph.Warning
    tailOnTop: true
    timeout: 0
    width: Theme.px(Theme.tokens.controls.balloon.maxWidth)
}
