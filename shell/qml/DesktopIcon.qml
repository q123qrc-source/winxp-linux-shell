// SPDX-License-Identifier: GPL-3.0-or-later
// One desktop icon: a 32 px icon over a white label with a drop shadow,
// wrapped to two lines (the full name while selected). Selected: the icon is
// tinted with the selection color and the label gets a selection-blue
// background. The keyboard-current icon has a dotted focus rectangle.
// F2 (or Rename) edits the label in place.
import QtQuick
import Meadow.Controls

Item {
    id: icon

    required property string itemId
    required property string name
    required property string iconName
    property bool selected: false
    property bool current: false
    property bool renaming: false
    property bool showFocus: false

    signal renameFinished(string newName, bool accepted)

    readonly property var tk: Theme.tokens.desktop
    readonly property int iconSize: Theme.px(tk.iconSize)
    // The label's area in item coordinates, for rubber-band hit tests.
    readonly property rect hitRect: Qt.rect(Math.min(iconImage.x, labelBox.x), iconImage.y,
                                            Math.max(iconImage.width, labelBox.width),
                                            labelBox.y + labelBox.height - iconImage.y)

    Accessible.role: Accessible.ListItem
    Accessible.name: name
    Accessible.selected: selected
    Accessible.focused: current

    Image {
        id: iconImage
        anchors.horizontalCenter: parent.horizontalCenter
        y: Theme.px(icon.tk.iconTopMargin)
        width: icon.iconSize
        height: icon.iconSize
        sourceSize: Qt.size(width, height)
        // Selected: blended with the selection color over its opaque pixels,
        // as XP does (IconProvider's tint, which works with any renderer).
        source: icon.iconName.length === 0 ? ""
              : "image://icon/" + icon.iconName
                + (icon.selected ? "?tint=" + String(icon.tk.iconSelectedTint).slice(1) : "")
        smooth: true
    }

    Item {
        id: labelBox
        anchors.horizontalCenter: parent.horizontalCenter
        y: iconImage.y + iconImage.height + Theme.px(icon.tk.labelSpacing)
        // As wide as the name needs, at most labelWidth (then it wraps).
        width: Math.min(Theme.px(icon.tk.labelWidth), label.implicitWidth + pad * 2)
        height: label.height + pad * 2
        readonly property int pad: Theme.px(icon.tk.labelPadding)
        visible: !icon.renaming

        Rectangle {
            anchors.fill: parent
            visible: icon.selected
            color: icon.tk.labelSelectedBackground
        }
        Text {
            x: label.x + Theme.px(icon.tk.labelShadowOffset)
            y: label.y + Theme.px(icon.tk.labelShadowOffset)
            width: label.width
            visible: !icon.selected
            text: label.text
            font: label.font
            wrapMode: label.wrapMode
            maximumLineCount: label.maximumLineCount
            elide: label.elide
            horizontalAlignment: label.horizontalAlignment
            color: icon.tk.labelShadowColor
        }
        Text {
            id: label
            x: labelBox.pad
            y: labelBox.pad
            width: labelBox.width - labelBox.pad * 2
            text: icon.name
            font: Theme.fonts.ui
            wrapMode: Text.Wrap
            maximumLineCount: icon.selected ? undefined : icon.tk.labelMaxLines
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            color: icon.selected ? icon.tk.labelSelectedText : icon.tk.labelColor
        }
        // Keyboard focus: dotted rectangle around the label.
        Rectangle {
            anchors.fill: parent
            visible: icon.current && icon.showFocus
            color: "transparent"
            border.color: icon.tk.focusBorderColor
            border.width: Theme.px(1)
        }
    }

    LunaTextField {
        id: editor
        visible: icon.renaming
        anchors.horizontalCenter: parent.horizontalCenter
        y: labelBox.y
        width: Theme.px(icon.tk.labelWidth)
        onAccepted: icon.finishRename(true)
        Keys.onEscapePressed: icon.finishRename(false)
        onActiveFocusChanged: if (!activeFocus && icon.renaming) icon.finishRename(true)
    }

    function startRename() {
        editor.text = name
        renaming = true
        editor.forceActiveFocus()
        // Select the name without its extension, like Explorer.
        const dot = name.lastIndexOf(".")
        editor.select(0, dot > 0 ? dot : name.length)
    }
    function finishRename(accepted) {
        if (!renaming)
            return
        renaming = false
        renameFinished(editor.text, accepted)
    }
}
