// SPDX-License-Identifier: GPL-3.0-or-later
// The Log Off / Turn Off Computer dialog: a dark-blue header with the title
// and the Meadow logo, a blue body holding a row of PowerButtons, and a footer
// with Cancel, centered over a full-screen backdrop. Escape, Cancel or a click
// on the backdrop closes it.
import QtQuick
import Meadow.Controls

FocusScope {
    id: root

    property string title
    property bool highContrast: false
    // Covers whatever is behind the dialog; the shell makes it transparent
    // and shows its gray fade instead.
    property color backdropColor: highContrast ? Theme.tokens.colors.highContrastBackground : dk.dimOverlayColor
    // The button row; give its first button focus in open().
    default property alias buttons: buttonRow.data
    property Item cancelButton: cancel
    // Shown in the body when no button is visible (e.g. logind unavailable).
    property string emptyText
    property int dialogWidth: Theme.px(tk.width)
    property int dialogHeight: Theme.px(tk.height)

    readonly property var tk: Theme.tokens.dialogs.turnOff
    readonly property var dk: Theme.tokens.dialogs

    signal closed()

    function open() {
        visible = true
        forceActiveFocus()
        const first = buttonRow.children.find((b) => b.visible)
        if (first)
            first.forceActiveFocus()
    }
    function close() {
        visible = false
        closed()
    }

    visible: false
    Keys.onEscapePressed: close()

    Accessible.role: Accessible.Dialog
    Accessible.name: title

    GradientBox {
        anchors.fill: parent
        color: root.backdropColor
        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }
    }

    Item {
        id: dialog
        objectName: "powerDialogBox"
        anchors.centerIn: parent
        width: root.dialogWidth
        height: root.dialogHeight

        // Keep clicks inside the dialog from closing it.
        MouseArea { anchors.fill: parent }

        GradientBox {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.px(root.dk.headerHeight)
            stops: root.highContrast ? [] : root.dk.headerGradient
            color: Theme.tokens.colors.highContrastBackground
            topLeftRadius: Theme.px(root.dk.radius)
            topRightRadius: Theme.px(root.dk.radius)
            bottomLeftRadius: 0
            bottomRightRadius: 0
            borderWidth: root.highContrast ? Theme.px(1) : 0
            borderColor: Theme.tokens.colors.highContrastAccent

            Text {
                anchors.left: parent.left
                anchors.leftMargin: Theme.px(root.dk.padding)
                anchors.verticalCenter: parent.verticalCenter
                text: root.title
                font: Theme.fonts.menuHeader
                color: root.highContrast ? Theme.tokens.colors.highContrastText : root.dk.headerTextColor
            }
            Logo {
                anchors.right: parent.right
                anchors.rightMargin: Theme.px(root.dk.padding)
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.px(root.tk.logoSize)
                height: width
            }
        }

        GradientBox {
            id: body
            anchors.top: header.bottom
            anchors.bottom: footer.top
            anchors.left: parent.left
            anchors.right: parent.right
            orientation: Qt.Horizontal
            stops: root.highContrast ? [] : root.tk.bodyGradient
            color: Theme.tokens.colors.highContrastBackground

            Row {
                id: buttonRow
                anchors.centerIn: parent
                spacing: Theme.px(root.tk.buttonSpacing)
            }
            Text {
                objectName: "powerDialogEmpty"
                anchors.fill: parent
                anchors.margins: Theme.px(root.dk.padding)
                visible: root.emptyText.length > 0 && !buttonRow.children.some((b) => b.visible)
                text: root.emptyText
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font: Theme.fonts.ui
                color: root.highContrast ? Theme.tokens.colors.highContrastText : root.tk.labelColor
            }
        }

        GradientBox {
            id: footer
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.px(root.tk.footerHeight)
            color: root.highContrast ? Theme.tokens.colors.highContrastBackground : root.dk.footerColor
            bottomLeftRadius: Theme.px(root.dk.radius)
            bottomRightRadius: Theme.px(root.dk.radius)
            topLeftRadius: 0
            topRightRadius: 0
            borderWidth: root.highContrast ? Theme.px(1) : 0
            borderColor: Theme.tokens.colors.highContrastAccent

            LunaButton {
                id: cancel
                objectName: "cancelButton"
                anchors.right: parent.right
                anchors.rightMargin: Theme.px(root.dk.padding)
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Cancel")
                onClicked: root.close()
            }
        }
    }
}
