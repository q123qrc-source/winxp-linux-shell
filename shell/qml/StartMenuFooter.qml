// SPDX-License-Identifier: GPL-3.0-or-later
// Start menu footer: "Log Off" and "Turn Off Computer", right-aligned, each
// with a small colored icon tile.
import QtQuick
import Meadow.Controls

GradientBox {
    id: footer

    readonly property var tk: Theme.tokens.startMenu.footer
    // Index of the keyboard-selected button, -1 for none.
    property int current: -1

    signal logOff()
    signal turnOff()

    height: Theme.px(tk.height)
    stops: tk.gradient

    component FooterButton: Item {
        id: button
        property string label
        property int glyph
        property var tileStops
        property bool hot: false
        signal triggered()

        width: row.implicitWidth + Theme.px(footer.tk.buttonPadding) * 2
        height: Theme.px(footer.tk.buttonIconSize) + Theme.px(footer.tk.buttonPadding) * 2
        Accessible.role: Accessible.Button
        Accessible.name: label
        Accessible.onPressAction: triggered()

        GradientBox {
            anchors.fill: parent
            radius: Theme.px(footer.tk.iconRadius)
            visible: button.hot || area.containsMouse
            color: footer.tk.buttonHoverBackground
        }
        Row {
            id: row
            anchors.centerIn: parent
            spacing: Theme.px(footer.tk.buttonPadding)
            GradientBox {
                width: Theme.px(footer.tk.buttonIconSize)
                height: width
                radius: Theme.px(footer.tk.iconRadius)
                stops: button.tileStops
                borderWidth: Theme.px(1)
                borderColor: Theme.tokens.colors.white
                LunaGlyph {
                    anchors.fill: parent
                    anchors.margins: Theme.px(footer.tk.glyphInset)
                    glyph: button.glyph
                    color: Theme.tokens.colors.white
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: button.label
                font: Theme.fonts.ui
                color: footer.tk.buttonTextColor
            }
        }
        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            onClicked: button.triggered()
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: Theme.px(footer.tk.buttonSpacing)
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.px(footer.tk.buttonSpacing)

        FooterButton {
            objectName: "startMenuLogOff"
            label: qsTr("Log Off")
            glyph: LunaGlyph.LogOff
            tileStops: footer.tk.logOffTileGradient
            hot: footer.current === 0
            onTriggered: footer.logOff()
        }
        FooterButton {
            objectName: "startMenuTurnOff"
            label: qsTr("Turn Off Computer")
            glyph: LunaGlyph.Power
            tileStops: footer.tk.turnOffTileGradient
            hot: footer.current === 1
            onTriggered: footer.turnOff()
        }
    }
}
