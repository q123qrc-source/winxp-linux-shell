// SPDX-License-Identifier: GPL-3.0-or-later
// The polkit "Authenticate" dialog (Shell.polkit): what the program wants
// to do, which account to authenticate as (administrators, or your own),
// and the password. A wrong password shows a message and asks again.
// Shown over a dimmed screen on the overlay layer, keyboard exclusive.
import QtQuick
import Meadow.Controls
import Meadow.Shell

FocusScope {
    id: auth

    readonly property var tk: Theme.tokens.dialogs.authenticate
    readonly property int pad: Theme.px(Theme.tokens.dialogs.padding)

    visible: Shell.polkit.active
    onVisibleChanged: if (visible) Qt.callLater(() => field.forceActiveFocus())

    Rectangle {
        anchors.fill: parent
        color: Theme.tokens.dialogs.dimOverlayColor
        MouseArea { anchors.fill: parent }
    }

    LunaDialog {
        id: dialog
        objectName: "authenticateDialog"
        anchors.centerIn: parent
        width: implicitWidth
        height: implicitHeight
        title: qsTr("Authenticate")
        buttons: [qsTr("OK"), qsTr("Cancel")]
        onButtonClicked: (index) => {
            if (index === 0)
                auth.submit()
            else
                Shell.polkit.cancel()
        }

        Column {
            width: Theme.px(auth.tk.width)
            spacing: Theme.px(auth.tk.spacing)
            Row {
                width: parent.width
                spacing: Theme.px(auth.tk.spacing)
                Image {
                    width: Theme.px(auth.tk.iconSize)
                    height: width
                    sourceSize: Qt.size(width, height)
                    source: "image://icon/" + (Shell.polkit.iconName || "dialog-password")
                }
                Text {
                    width: parent.width - Theme.px(auth.tk.iconSize) - parent.spacing
                    wrapMode: Text.Wrap
                    font: Theme.fonts.ui
                    text: Shell.polkit.message
                }
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font: Theme.fonts.ui
                color: Theme.tokens.colors.disabledText
                text: qsTr("An application is attempting to perform an action that requires privileges. Authentication is required to perform this action.")
            }
            Text { visible: Shell.polkit.identities.length > 1; text: qsTr("Authenticate as:"); font: Theme.fonts.ui }
            LunaComboBox {
                objectName: "identityCombo"
                visible: Shell.polkit.identities.length > 1
                width: parent.width
                model: Shell.polkit.identities
                currentIndex: Shell.polkit.identityIndex
                onActivated: (index) => Shell.polkit.identityIndex = index
            }
            Text {
                visible: Shell.polkit.identities.length === 1
                font: Theme.fonts.menuItemBold
                text: Shell.polkit.identities.length === 1 ? Shell.polkit.identities[0] : ""
            }
            Text {
                text: Shell.polkit.prompt.length > 0 ? Shell.polkit.prompt : qsTr("Password:")
                font: Theme.fonts.ui
            }
            LunaTextField {
                id: field
                objectName: "authPassword"
                width: parent.width
                enabled: !Shell.polkit.busy
                echoMode: Shell.polkit.echo ? TextInput.Normal : TextInput.Password
                onAccepted: auth.submit()
            }
            Text {
                objectName: "authFeedback"
                width: parent.width
                visible: text.length > 0
                wrapMode: Text.Wrap
                font: Theme.fonts.ui
                color: Theme.tokens.dialogs.errorTextColor
                text: Shell.polkit.feedback
            }
        }
    }

    function submit() {
        const secret = field.text
        field.clear() // the password leaves the field (and the dialog) at once
        Shell.polkit.submit(secret)
    }
    Keys.onEscapePressed: Shell.polkit.cancel()
}
