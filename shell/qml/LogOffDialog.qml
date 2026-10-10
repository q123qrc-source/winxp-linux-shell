// SPDX-License-Identifier: GPL-3.0-or-later
// "Log Off Meadow": Switch User (only when the display manager supports it)
// and Log Off. Same frame as Turn Off Computer.
import QtQuick
import Meadow.Controls
import Meadow.Shell

PowerDialogFrame {
    id: root
    objectName: "logOffDialog"

    readonly property var lk: Theme.tokens.dialogs.logOff

    title: qsTr("Log Off Meadow")
    dialogWidth: Theme.px(lk.width)
    dialogHeight: Theme.px(lk.height)

    PowerButton {
        id: switchUser
        objectName: "switchUserButton"
        visible: Shell.session.canSwitchUser
        square: true
        label: qsTr("Switch User")
        stops: root.lk.switchUserGradient
        glyph: LunaGlyph.SwitchUser
        onTriggered: { root.close(); Shell.session.switchUser() }
        KeyNavigation.right: logOff
    }
    PowerButton {
        id: logOff
        objectName: "logOffButton"
        square: true
        label: qsTr("Log Off")
        stops: root.lk.logOffGradient
        glyph: LunaGlyph.LogOff
        onTriggered: { root.close(); Shell.session.logOff() }
        KeyNavigation.left: switchUser
        KeyNavigation.right: root.cancelButton
    }
}
