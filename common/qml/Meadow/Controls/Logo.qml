// SPDX-License-Identifier: GPL-3.0-or-later
// The Meadow logo. Drawn procedurally (MeadowLogo) unless the user placed a
// "meadow-logo.svg" in an asset override folder.
import QtQuick
import Meadow.Controls

Item {
    id: root

    // Draw every pane in one color, e.g. on a busy background.
    property color monochrome: "transparent"

    readonly property string overrideUrl: Theme.resolveAsset(Theme.tokens.assets.logo)

    implicitWidth: Theme.px(Theme.tokens.welcome.logo.size)
    implicitHeight: implicitWidth

    Accessible.role: Accessible.Graphic
    Accessible.name: qsTr("Meadow")

    MeadowLogo {
        anchors.fill: parent
        visible: root.overrideUrl.length === 0
        colors: Theme.tokens.logo.colors
        gapRatio: Theme.tokens.logo.gapRatio
        radiusRatio: Theme.tokens.logo.radiusRatio
        glossAlpha: Theme.tokens.logo.glossAlpha
        monochrome: root.monochrome.a > 0 ? root.monochrome : "transparent"
    }

    Image {
        anchors.fill: parent
        visible: root.overrideUrl.length > 0
        source: root.overrideUrl
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectFit
    }
}
