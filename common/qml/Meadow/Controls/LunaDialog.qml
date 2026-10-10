// SPDX-License-Identifier: GPL-3.0-or-later
// Dialog: a window frame with a close button only, the body content and a
// right-aligned row of push buttons. Enter triggers the default button and
// Escape (or the close button) cancels.
//
//   LunaDialog {
//       title: qsTr("Run")
//       buttons: [qsTr("OK"), qsTr("Cancel")]
//       Text { text: qsTr("Type the name of a program…") }
//       onButtonClicked: (index) => …
//   }
import QtQuick
import Meadow.Controls

LunaWindowFrame {
    id: dialog

    property var buttons: [qsTr("OK"), qsTr("Cancel")]
    property int defaultButton: 0
    // Index reported for Escape / close; -1 if no button means "cancel".
    property int cancelButton: buttons.length - 1
    // Forced state of the default button (gallery and screenshot tests only).
    property string previewState: ""

    default property alias body: bodyArea.data

    readonly property int pad: Theme.px(Theme.tokens.dialogs.padding)

    signal buttonClicked(int index)
    signal accepted()
    signal rejected()

    function accept() {
        buttonClicked(defaultButton)
        accepted()
    }
    function reject() {
        if (cancelButton >= 0)
            buttonClicked(cancelButton)
        rejected()
    }

    showMinimize: false
    showMaximize: false
    contentWidth: Math.max(bodyArea.childrenRect.width, buttonRow.implicitWidth) + pad * 2
    contentHeight: bodyArea.childrenRect.height + buttonRow.implicitHeight + pad * 3 // theme-ok: top, gap and bottom padding
    onCloseClicked: reject()

    Accessible.role: Accessible.Dialog

    content: FocusScope {
        anchors.fill: parent
        focus: true

        Keys.onEscapePressed: dialog.reject()
        Keys.onReturnPressed: dialog.accept()
        Keys.onEnterPressed: dialog.accept()

        Item {
            id: bodyArea
            x: dialog.pad
            y: dialog.pad
            width: parent.width - dialog.pad * 2
            height: childrenRect.height
        }

        Row {
            id: buttonRow
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: dialog.pad
            spacing: Theme.px(Theme.tokens.dialogs.buttonSpacing)

            Repeater {
                model: dialog.buttons
                LunaButton {
                    required property string modelData
                    required property int index
                    text: modelData
                    isDefault: index === dialog.defaultButton
                    previewState: index === dialog.defaultButton ? dialog.previewState : ""
                    onClicked: {
                        if (index === dialog.defaultButton)
                            dialog.accept()
                        else if (index === dialog.cancelButton)
                            dialog.reject()
                        else
                            dialog.buttonClicked(index)
                    }
                }
            }
        }
    }
}
