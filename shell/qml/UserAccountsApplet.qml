// SPDX-License-Identifier: GPL-3.0-or-later
// "User Accounts": the accounts on this computer (AccountsService) with
// their picture and type. For your own account you can pick a new picture
// and change the name shown on the Welcome screen and Start menu. Changes
// to other accounts need an administrator (polkit asks).
import QtQuick
import Meadow.Controls
import Meadow.Shell

AppletSheet {
    id: applet

    readonly property var users: Shell.accounts.users
    property int selected: 0
    readonly property var user: selected >= 0 && selected < users.length ? users[selected] : null
    property string pendingPicture: ""
    property string pendingName: ""
    readonly property var pictures: Shell.accounts.pictures()

    function load(tab) {
        selected = Math.max(0, users.findIndex(u => u.self))
        Qt.callLater(() => accountList.currentIndex = selected)
        pendingPicture = ""
        pendingName = ""
        message = ""
        showTab(tab)
    }
    dirty: !!user && ((pendingPicture.length > 0 && pendingPicture !== user.iconFile)
                      || (pendingName.length > 0 && pendingName !== user.realName))
    onApplyRequested: {
        message = ""
        if (pendingPicture.length > 0 && pendingPicture !== user.iconFile)
            Shell.accounts.setIconFile(user.path, pendingPicture)
        if (pendingName.length > 0 && pendingName !== user.realName)
            Shell.accounts.setRealName(user.path, pendingName)
        pendingPicture = ""
        pendingName = ""
    }
    onSelectedChanged: { pendingPicture = ""; pendingName = "" }
    Connections {
        target: Shell.accounts
        function onFailed(text) { applet.message = text }
    }

    Item {
        property string title: qsTr("Users")
        property string key: "users"
        Column {
            width: parent.width
            spacing: applet.pad
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font: Theme.fonts.ui
                text: Shell.accounts.available ? qsTr("Pick an account to see or change it:")
                                               : qsTr("The accounts service is not available.")
            }
            LunaListView {
                id: accountList
                objectName: "accountList"
                width: parent.width
                model: applet.users.map(u => ({ label: (u.realName || u.name) + "  —  "
                                                       + (u.admin ? qsTr("Computer administrator") : qsTr("Limited account")),
                                                iconName: u.iconFile || "user-identity" }))
                onCurrentIndexChanged: if (activeFocus) applet.selected = currentIndex
            }
            Row {
                visible: !!applet.user
                spacing: applet.pad
                Avatar {
                    width: Theme.px(Theme.tokens.dialogs.dateTime.pictureSize)
                    height: width
                    source: {
                        const f = applet.pendingPicture || (applet.user ? applet.user.iconFile : "")
                        return f ? "file://" + f : ""
                    }
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: applet.pad
                    Text {
                        font: Theme.fonts.menuItemBold
                        text: applet.user ? (applet.user.realName || applet.user.name) : ""
                    }
                    Text {
                        font: Theme.fonts.ui
                        color: Theme.tokens.colors.disabledText
                        visible: !!applet.user && !applet.user.self
                        text: qsTr("Changing another account needs an administrator password.")
                    }
                }
            }
        }
    }

    Item {
        property string title: qsTr("Picture and Name")
        property string key: "picture"
        Column {
            width: parent.width
            spacing: applet.pad
            Text { text: qsTr("Pick a new picture:"); font: Theme.fonts.ui }
            Flow {
                width: parent.width
                spacing: applet.pad
                Repeater {
                    model: applet.pictures
                    delegate: Rectangle {
                        required property string modelData
                        readonly property bool chosen: modelData === (applet.pendingPicture || (applet.user ? applet.user.iconFile : ""))
                        width: Theme.px(Theme.tokens.dialogs.dateTime.userTileSize) + Theme.px(2) * 2
                        height: width
                        color: chosen ? Theme.tokens.colors.selection : "transparent"
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.substring(modelData.lastIndexOf("/") + 1)
                        Avatar {
                            anchors.centerIn: parent
                            width: Theme.px(Theme.tokens.dialogs.dateTime.userTileSize)
                            height: width
                            source: "file://" + parent.modelData
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: applet.pendingPicture = parent.modelData
                        }
                    }
                }
            }
            Text { text: qsTr("Name shown on the Welcome screen and Start menu:"); font: Theme.fonts.ui }
            LunaTextField {
                objectName: "realNameField"
                width: parent.width
                text: applet.user ? applet.user.realName : ""
                onTextEdited: applet.pendingName = text
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font: Theme.fonts.ui
                color: Theme.tokens.colors.disabledText
                text: qsTr("To change your password, run \"passwd\" in a terminal.")
            }
        }
    }
}
