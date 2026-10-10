// SPDX-License-Identifier: GPL-3.0-or-later
// Drop-down list: white field with a blue gradient arrow button on the right.
import QtQuick
import QtQuick.Templates as T
import Meadow.Controls

T.ComboBox {
    id: control

    // "", "hover", "pressed" or "focus" (gallery and screenshot tests only).
    property string previewState: ""

    readonly property var tk: Theme.tokens.controls.comboBox
    readonly property var colors: Theme.tokens.colors
    readonly property bool showPressed: enabled && (pressed || popup.visible || previewState === "pressed")
    readonly property bool showHover: enabled && !showPressed && (hovered || previewState === "hover")
    readonly property bool showFocus: enabled && !popup.visible && (visualFocus || previewState === "focus")

    implicitWidth: Theme.px(tk.defaultWidth)
    implicitHeight: Theme.px(tk.height)
    leftPadding: Theme.px(2)
    rightPadding: Theme.px(tk.buttonWidth) + Theme.px(2)
    font: Theme.fonts.ui
    hoverEnabled: true

    Accessible.role: Accessible.ComboBox
    Accessible.name: displayText

    delegate: T.ItemDelegate {
        required property var model
        required property int index

        width: ListView.view ? ListView.view.width : implicitWidth
        implicitHeight: Theme.px(control.tk.itemHeight)
        leftPadding: Theme.px(2)
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
        text: model[control.textRole] !== undefined && control.textRole !== "" ? model[control.textRole]
                                                                              : model.modelData

        contentItem: Text {
            text: parent.text
            font: control.font
            color: parent.highlighted ? control.colors.selectionText : control.colors.windowText
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: GradientBox {
            color: parent.highlighted ? control.colors.selection : "transparent"
        }
    }

    indicator: GradientBox {
        x: control.width - width - Theme.px(1)
        y: Theme.px(1)
        width: Theme.px(control.tk.buttonWidth)
        height: control.height - Theme.px(2)
        radius: Theme.px(2)
        stops: {
            const s = control.showHover ? control.tk.buttonHoverGradient : control.tk.buttonGradient
            if (!control.showPressed)
                return s
            const out = []
            for (let i = s.length - 1; i >= 0; --i)
                out.push({ pos: 1 - s[i].pos, color: s[i].color })
            return out
        }
        opacity: control.enabled ? 1 : 0.5

        LunaGlyph {
            anchors.fill: parent
            anchors.margins: Theme.px(2)
            glyph: LunaGlyph.ArrowDown
            color: control.tk.arrowColor
        }
    }

    contentItem: Item {
        GradientBox {
            anchors.fill: parent
            anchors.topMargin: Theme.px(2)
            anchors.bottomMargin: Theme.px(2)
            color: control.showFocus ? control.colors.selection : "transparent"
        }
        Text {
            anchors.fill: parent
            leftPadding: Theme.px(2)
            text: control.displayText
            font: control.font
            color: !control.enabled ? control.colors.disabledText
                 : control.showFocus ? control.colors.selectionText : control.colors.windowText
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    background: GradientBox {
        implicitWidth: Theme.px(control.tk.defaultWidth)
        implicitHeight: Theme.px(control.tk.height)
        color: control.enabled ? Theme.tokens.controls.textField.background : Theme.tokens.controls.textField.disabledBackground
        borderWidth: Theme.px(1)
        borderColor: control.tk.borderColor
    }

    popup: T.Popup {
        y: control.height
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding,
                                 Theme.px(control.tk.itemHeight) * control.tk.popupMaxItems + topPadding + bottomPadding)
        padding: Theme.px(1)

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: LunaScrollBar {}
        }

        background: GradientBox {
            color: Theme.tokens.controls.menu.background
            borderWidth: Theme.px(1)
            borderColor: control.tk.popupBorderColor
        }
    }
}
