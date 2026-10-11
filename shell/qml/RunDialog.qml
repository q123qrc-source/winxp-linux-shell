// SPDX-License-Identifier: GPL-3.0-or-later
// Run: "Open:" field with the history (Up/Down) and inline completion from
// the history, PATH and the file system; OK, Cancel and Browse… (the desktop
// portal's file chooser). Errors appear under the field. It is the body of a
// normal window in a session and of a drawn LunaWindowFrame in --preview.
import QtQuick
import Meadow.Controls
import Meadow.Shell

FocusScope {
    id: dialog
    objectName: "runDialog"

    property string error
    // Position in the history while browsing it with Up/Down.
    property int historyIndex: -1
    property int previousLength: 0

    readonly property var tk: Theme.tokens.dialogs.run
    readonly property int pad: Theme.px(Theme.tokens.dialogs.padding)

    signal done()

    implicitWidth: Theme.px(tk.width)
    implicitHeight: content.implicitHeight + pad * 2

    function open() {
        error = ""
        historyIndex = -1
        field.text = Shell.run.history.length > 0 ? Shell.run.history[0] : ""
        previousLength = field.text.length
        field.selectAll()
        field.forceActiveFocus()
    }
    function accept() {
        if (field.text.trim().length === 0)
            return
        const message = Shell.run.run(field.text)
        if (message.length > 0) {
            error = message
            field.selectAll()
            field.forceActiveFocus()
            return
        }
        done()
    }
    function recall(delta) {
        const history = Shell.run.history
        if (history.length === 0)
            return
        historyIndex = Math.max(0, Math.min(history.length - 1, historyIndex + delta))
        field.text = history[historyIndex]
        previousLength = field.text.length
        field.selectAll()
    }
    // Inline completion while typing forward: the rest is selected, so
    // typing on replaces it.
    function autocomplete() {
        const typed = field.text
        const growing = typed.length > previousLength
        previousLength = typed.length
        error = ""
        if (!growing || field.cursorPosition !== typed.length)
            return
        const options = Shell.run.complete(typed)
        const first = options.find(o => o.startsWith(typed))
        if (first && first.length > typed.length) {
            field.text = first
            previousLength = typed.length
            field.select(typed.length, first.length)
        }
    }

    Keys.onEscapePressed: done()

    Connections {
        target: Shell.run
        function onBrowsed(path, tag) {
            if (tag !== "run")
                return
            field.text = path.indexOf(" ") >= 0 ? "\"" + path + "\"" : path
            field.forceActiveFocus()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.tokens.dialogs.background
    }

    Column {
        id: content
        x: dialog.pad
        y: dialog.pad
        width: dialog.width - dialog.pad * 2
        spacing: Theme.px(dialog.tk.spacing)

        Row {
            width: parent.width
            spacing: Theme.px(dialog.tk.spacing)
            LunaGlyph {
                width: Theme.px(dialog.tk.iconSize)
                height: width
                glyph: LunaGlyph.Run
                color: Theme.tokens.colors.accent
            }
            Text {
                width: parent.width - Theme.px(dialog.tk.iconSize) - parent.spacing
                text: qsTr("Type the name of a program, folder, document, or Internet address, and Meadow will open it for you.")
                wrapMode: Text.Wrap
                font: Theme.fonts.ui
                color: Theme.tokens.colors.windowText
            }
        }

        Row {
            width: parent.width
            spacing: Theme.px(dialog.tk.spacing)
            Text {
                id: openLabel
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.px(dialog.tk.iconSize)
                text: qsTr("Open:")
                font: Theme.fonts.ui
                color: Theme.tokens.colors.windowText
            }
            LunaTextField {
                id: field
                objectName: "runField"
                width: parent.width - openLabel.width - parent.spacing
                focus: true
                onTextEdited: dialog.autocomplete()
                Keys.onReturnPressed: dialog.accept()
                Keys.onEnterPressed: dialog.accept()
                Keys.onUpPressed: dialog.recall(dialog.historyIndex < 0 ? 0 : 1)
                Keys.onDownPressed: dialog.recall(-1)
                Accessible.name: qsTr("Open")
            }
        }

        Row {
            visible: dialog.error.length > 0
            width: parent.width
            spacing: Theme.px(dialog.tk.spacing)
            LunaGlyph {
                width: Theme.px(Theme.tokens.controls.balloon.iconSize)
                height: width
                glyph: LunaGlyph.Error
            }
            Text {
                objectName: "runError"
                width: parent.width - Theme.px(Theme.tokens.controls.balloon.iconSize) - parent.spacing
                text: dialog.error
                wrapMode: Text.Wrap
                font: Theme.fonts.ui
                color: Theme.tokens.colors.windowText
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
        }

        Row {
            anchors.right: parent.right
            spacing: Theme.px(Theme.tokens.dialogs.buttonSpacing)
            LunaButton {
                objectName: "runOk"
                text: qsTr("OK")
                isDefault: true
                enabled: field.text.trim().length > 0
                onClicked: dialog.accept()
            }
            LunaButton {
                objectName: "runCancel"
                text: qsTr("Cancel")
                onClicked: dialog.done()
            }
            LunaButton {
                objectName: "runBrowse"
                text: qsTr("Browse…")
                onClicked: Shell.run.browse(qsTr("Browse"))
            }
        }
    }
}
