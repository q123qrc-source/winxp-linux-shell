// SPDX-License-Identifier: GPL-3.0-or-later
// Balloon for login feedback: wrong password (with the account's hint when it
// has one), the hint on request, and messages from the display manager.
import QtQuick
import Meadow.Controls

BalloonTip {
    id: balloon

    function showLoginFailed(hint) {
        title = qsTr("Did you forget your password?")
        text = hint.length > 0
            ? qsTr("That password did not work. Passwords are case-sensitive.\nYour hint: %1").arg(hint)
            : qsTr("That password did not work. Check that you typed it correctly; passwords are case-sensitive.")
        icon = LunaGlyph.Info
        show()
    }
    function showHint(hint) {
        title = qsTr("Password hint")
        text = hint.length > 0 ? hint : qsTr("No password hint has been set for this account.")
        icon = LunaGlyph.Question
        show()
    }
    function showFingerprintFailed() {
        title = qsTr("Fingerprint not recognized")
        text = qsTr("Type your password, or press Enter with the password box empty to use the fingerprint reader again.")
        icon = LunaGlyph.Info
        show()
    }
    function showMessage(message) {
        title = qsTr("Message")
        text = message
        icon = LunaGlyph.Info
        show()
    }

    visible: false
    tailOnTop: true
    timeout: Theme.tokens.controls.balloon.timeoutMs
    width: Theme.px(Theme.tokens.controls.balloon.maxWidth)
}
