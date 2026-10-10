// SPDX-License-Identifier: GPL-3.0-or-later
// One account on the Welcome screen: framed picture and name. Hover shows a
// rounded highlight fading in from the left; selecting expands the tile to
// show the password panel.
import QtQuick
import Meadow.Controls

FocusScope {
    id: tile

    property string userName
    property string displayName
    property url iconSource
    property bool needsPassword: true
    property bool selected: false
    property bool current: false     // keyboard highlight in the list
    property bool dimmed: false      // another tile is selected
    property bool busy: false
    property bool highContrast: false
    // "", "hover" or "focus" (preview/screenshot only).
    property string previewState: ""
    readonly property alias passwordPanel: panel

    readonly property var tk: Theme.tokens.welcome.tile
    readonly property bool showHover: !selected && (hover.hovered || current || previewState === "hover")
    readonly property int pad: Theme.px(tk.padding)

    signal activated()
    signal submitted(string password)
    signal hintRequested()
    signal cancelled()

    width: Theme.px(tk.width)
    implicitHeight: Math.max(Theme.px(tk.height), content.implicitHeight + pad * 2)
    height: implicitHeight
    opacity: dimmed ? tk.dimmedOpacity : 1

    Behavior on opacity { NumberAnimation { duration: Theme.duration("fast") } }

    Accessible.role: Accessible.ListItem
    Accessible.name: displayName
    Accessible.description: selected ? qsTr("Selected. Type your password and press Enter.") : qsTr("Press Enter to log on as %1.").arg(displayName)

    HoverHandler { id: hover }
    TapHandler {
        onTapped: if (!tile.selected) tile.activated()
    }

    GradientBox {
        anchors.fill: parent
        radius: Theme.px(tile.tk.radius)
        orientation: Qt.Horizontal
        visible: tile.selected || tile.showHover
        stops: tile.highContrast ? [] : tile.selected ? tile.tk.selectedGradient : tile.tk.hoverGradient
        color: "transparent"
        borderWidth: tile.highContrast ? Theme.px(2) : 0
        borderColor: Theme.tokens.colors.highContrastAccent
    }

    Row {
        id: content
        x: tile.pad
        y: tile.pad
        spacing: Theme.px(tile.tk.spacing)

        Avatar {
            id: avatar
            source: tile.iconSource
            userName: tile.userName
            frameColor: tile.highContrast ? Theme.tokens.colors.highContrastText
                      : (tile.showHover || tile.selected) ? tile.tk.avatarFrameHoverColor : tile.tk.avatarFrameColor
        }

        Column {
            spacing: Theme.px(tile.tk.expandedGap)
            anchors.verticalCenter: tile.selected ? undefined : avatar.verticalCenter

            Text {
                width: Theme.px(tile.tk.width) - avatar.width - Theme.px(tile.tk.spacing) - tile.pad * 2
                text: tile.displayName
                font: Theme.fonts.welcomeUser
                color: tile.highContrast ? Theme.tokens.colors.highContrastText : tile.tk.nameColor
                elide: Text.ElideRight
            }

            PasswordPanel {
                id: panel
                visible: tile.selected && tile.needsPassword
                busy: tile.busy
                highContrast: tile.highContrast
                previewState: tile.previewState === "focus" ? "focus" : ""
                onSubmitted: (password) => tile.submitted(password)
                onHintRequested: tile.hintRequested()
                onCancelled: tile.cancelled()
            }
        }
    }
}
