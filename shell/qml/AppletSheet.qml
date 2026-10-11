// SPDX-License-Identifier: GPL-3.0-or-later
// A Control Panel applet's property sheet: tabs (the sheet's children, each
// with a `title` and a `key`) above OK / Cancel / Apply. The applet sets
// `dirty` and does its work in applyRequested(); OK applies and closes.
import QtQuick
import Meadow.Controls
import Meadow.Shell

FocusScope {
    id: sheet

    default property alias pages: tabView.pages
    property bool dirty: false
    readonly property alias tabs: tabView
    readonly property int pad: Theme.px(Theme.tokens.dialogs.padding)
    readonly property var tk: Theme.tokens.dialogs.applet
    // Shown left of the buttons (e.g. "You do not have permission ...").
    property string message: ""

    signal applyRequested()
    signal done()

    implicitWidth: Theme.px(tk.width)
    implicitHeight: Theme.px(tk.height)

    // Opens the tab whose `key` matches ("" or unknown: the first).
    function showTab(key) {
        for (let i = 0; i < tabView.count; ++i) {
            if (tabView.page(i).key === key) {
                tabView.currentIndex = i
                return
            }
        }
        tabView.currentIndex = 0
    }

    Keys.onEscapePressed: done()

    Rectangle {
        anchors.fill: parent
        color: Theme.tokens.dialogs.background
    }

    LunaTabView {
        id: tabView
        x: sheet.pad
        y: sheet.pad
        width: sheet.width - sheet.pad * 2
        height: buttons.y - sheet.pad * 2
        focus: true
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: sheet.pad
        anchors.right: buttons.left
        anchors.rightMargin: sheet.pad
        anchors.verticalCenter: buttons.verticalCenter
        text: sheet.message
        elide: Text.ElideRight
        font: Theme.fonts.ui
        color: Theme.tokens.dialogs.errorTextColor
    }

    Row {
        id: buttons
        anchors.right: parent.right
        anchors.rightMargin: sheet.pad
        anchors.bottom: parent.bottom
        anchors.bottomMargin: sheet.pad
        spacing: Theme.px(Theme.tokens.dialogs.buttonSpacing)
        LunaButton { objectName: "dialogButton:OK"; text: qsTr("OK"); isDefault: true; onClicked: { if (sheet.dirty) sheet.applyRequested(); sheet.done() } }
        LunaButton { objectName: "dialogButton:Cancel"; text: qsTr("Cancel"); onClicked: sheet.done() }
        LunaButton { objectName: "dialogButton:Apply"; text: qsTr("Apply"); enabled: sheet.dirty; onClicked: sheet.applyRequested() }
    }
}
