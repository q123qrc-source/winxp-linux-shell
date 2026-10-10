// SPDX-License-Identifier: GPL-3.0-or-later
// Start menu header: dark-blue band with the user's picture in a white
// rounded frame and the real name (AccountsService) in bold white.
import QtQuick
import Meadow.Controls
import Meadow.Shell

GlossyGradient {
    id: header

    readonly property var tk: Theme.tokens.startMenu.header
    readonly property string displayName: account.realName.length > 0 ? account.realName : Shell.userName

    height: Theme.px(tk.height)
    stops: tk.gradient
    glossColor: tk.highlightColor
    glossFraction: Theme.tokens.startMenu.header.glossFraction
    topLeftRadius: Theme.px(Theme.tokens.startMenu.radiusTop)
    topRightRadius: Theme.px(Theme.tokens.startMenu.radiusTop)
    bottomLeftRadius: 0
    bottomRightRadius: 0

    Accessible.role: Accessible.StaticText
    Accessible.name: displayName

    UserAccount {
        id: account
        userName: Shell.userName
    }

    Avatar {
        id: avatar
        x: Theme.px(header.tk.padding)
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.px(header.tk.avatarSize)
        height: width
        userName: Shell.userName
        source: account.iconFile.length > 0 ? "file://" + account.iconFile : ""
        frameColor: header.tk.avatarBorderColor
        frameWidth: Theme.px(header.tk.avatarBorderWidth)
        radius: Theme.px(header.tk.avatarRadius)
    }

    ShadowText {
        anchors.left: avatar.right
        anchors.leftMargin: Theme.px(header.tk.spacing)
        anchors.right: parent.right
        anchors.rightMargin: Theme.px(header.tk.padding)
        anchors.verticalCenter: parent.verticalCenter
        text: header.displayName
        elide: Text.ElideRight
        font: Theme.fonts.menuHeader
        color: header.tk.nameColor
        shadowColor: header.tk.nameShadowColor
        shadowOffset: Theme.px(1)
    }
}
