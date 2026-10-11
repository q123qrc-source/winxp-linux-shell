// SPDX-License-Identifier: GPL-3.0-or-later
// A Yes/No question with a question-mark icon ("Are you sure you want to
// delete all items in the Recycle Bin?"). The body of a small labwc-
// decorated window in a session, of a drawn frame in --preview.
import QtQuick
import Meadow.Controls

FocusScope {
    id: confirm

    property string text
    property var onYes: null

    signal done()

    readonly property var tk: Theme.tokens.dialogs.message
    readonly property int pad: Theme.px(Theme.tokens.dialogs.padding)

    implicitWidth: Theme.px(tk.width)
    implicitHeight: body.implicitHeight + buttons.implicitHeight + pad * 3 // theme-ok: top, gap, bottom padding

    Accessible.role: Accessible.AlertMessage
    Accessible.name: text

    Keys.onReturnPressed: yes()
    Keys.onEscapePressed: done()
    function yes() {
        if (onYes)
            onYes()
        done()
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.tokens.dialogs.background
    }
    Row {
        id: body
        x: confirm.pad
        y: confirm.pad
        width: parent.width - confirm.pad * 2
        spacing: confirm.pad
        LunaGlyph {
            width: Theme.px(confirm.tk.iconSize)
            height: width
            glyph: LunaGlyph.Question
            color: Theme.tokens.colors.accent
        }
        Text {
            width: parent.width - Theme.px(confirm.tk.iconSize) - parent.spacing
            anchors.verticalCenter: parent.verticalCenter
            wrapMode: Text.Wrap
            font: Theme.fonts.ui
            text: confirm.text
        }
    }
    Row {
        id: buttons
        anchors.right: parent.right
        anchors.rightMargin: confirm.pad
        anchors.bottom: parent.bottom
        anchors.bottomMargin: confirm.pad
        spacing: Theme.px(Theme.tokens.dialogs.buttonSpacing)
        LunaButton { objectName: "dialogButton:Yes"; text: qsTr("Yes"); isDefault: true; onClicked: confirm.yes() }
        LunaButton { objectName: "dialogButton:No"; text: qsTr("No"); onClicked: confirm.done() }
    }
}
