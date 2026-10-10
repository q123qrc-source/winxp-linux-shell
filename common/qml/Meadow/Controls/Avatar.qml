// SPDX-License-Identifier: GPL-3.0-or-later
// A user picture in a rounded frame. Uses the given picture (~/.face.icon or
// AccountsService) when it exists and loads; otherwise picks one of Meadow's
// original avatars, chosen stably from the user name.
import QtQuick
import Meadow.Controls

Item {
    id: root

    // Picture URL from the display manager or AccountsService; may be empty
    // or point at a generic default face.
    property url source
    property string userName
    property color frameColor: Theme.tokens.welcome.tile.avatarFrameColor
    property real frameWidth: Theme.px(Theme.tokens.welcome.tile.avatarFrameWidth)
    property real radius: Theme.px(Theme.tokens.welcome.tile.avatarRadius)

    // SDDM's fallbacks are named ".face.icon" inside a "faces" directory;
    // a user's own picture never is.
    readonly property bool sourceIsGeneric: {
        const s = source.toString()
        return s.length === 0 || /\/faces\/\.face\.icon$/.test(s)
    }
    readonly property string fallbackUrl: {
        const list = Theme.tokens.assets.defaultAvatars
        let h = 0
        for (let i = 0; i < userName.length; ++i)
            h = (h * 31 + userName.charCodeAt(i)) % 65521 // theme-ok: string hash constants
        return list.length > 0 ? Theme.asset(list[h % list.length]) : ""
    }

    implicitWidth: Theme.px(Theme.tokens.welcome.tile.avatarSize)
    implicitHeight: implicitWidth

    GradientBox {
        anchors.fill: parent
        radius: root.radius
        color: root.frameColor
    }

    Image {
        id: picture
        anchors.fill: parent
        anchors.margins: root.frameWidth
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectCrop
        smooth: true
        asynchronous: false
        source: root.sourceIsGeneric ? root.fallbackUrl : root.source
        // A broken or unreadable user picture falls back to the avatar set.
        onStatusChanged: if (status === Image.Error && source.toString() !== root.fallbackUrl) source = root.fallbackUrl
    }
}
