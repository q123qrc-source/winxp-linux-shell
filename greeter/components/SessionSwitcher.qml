// SPDX-License-Identifier: GPL-3.0-or-later
// Desktop session picker (Meadow, Plasma, GNOME, ...) for the bottom band.
// A combo box styled as a pill; its list opens upward.
import QtQuick
import QtQuick.Templates as T
import Meadow.Controls

T.ComboBox {
    id: control

    property bool highContrast: false
    readonly property var tk: Theme.tokens.welcome.bottomControls
    readonly property color ink: highContrast ? Theme.tokens.colors.highContrastText : tk.textColor

    textRole: "name"
    implicitHeight: Theme.px(tk.height)
    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    leftPadding: Theme.px(tk.paddingH)
    rightPadding: Theme.px(tk.paddingH) + Theme.px(Theme.tokens.controls.balloon.iconSize)
    font: Theme.fonts.ui
    hoverEnabled: true

    Accessible.role: Accessible.ComboBox
    Accessible.name: qsTr("Session: %1").arg(displayText)

    contentItem: Text {
        text: control.displayText
        font: control.font
        color: control.ink
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: LunaGlyph {
        x: control.width - width - Theme.px(control.tk.paddingH) / 2
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.px(Theme.tokens.controls.balloon.iconSize)
        height: width
        glyph: LunaGlyph.ArrowUp
        color: control.ink
    }

    background: GradientBox {
        radius: Theme.px(control.tk.radius)
        color: control.highContrast ? Theme.tokens.colors.highContrastBackground
             : (control.hovered || control.popup.visible) ? control.tk.hoverBackground : control.tk.background
        borderWidth: Theme.px(1)
        borderColor: control.highContrast ? Theme.tokens.colors.highContrastAccent : control.tk.border
        GradientBox {
            anchors.fill: parent
            anchors.margins: Theme.px(2)
            visible: control.visualFocus
            borderWidth: 1
            borderDotted: true
            borderColor: control.ink
        }
    }

    delegate: T.ItemDelegate {
        required property var model
        required property int index
        width: ListView.view ? ListView.view.width : implicitWidth
        implicitWidth: label.implicitWidth + leftPadding + rightPadding
        implicitHeight: Theme.px(Theme.tokens.controls.menu.itemHeight) + Theme.px(2) * 2
        leftPadding: Theme.px(control.tk.paddingH)
        rightPadding: Theme.px(control.tk.paddingH)
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            id: label
            text: model.name
            font: control.font
            color: parent.highlighted ? Theme.tokens.controls.menu.hoverText : Theme.tokens.colors.windowText
            verticalAlignment: Text.AlignVCenter
        }
        background: GradientBox {
            color: parent.highlighted ? Theme.tokens.controls.menu.hoverBackground : "transparent"
        }
        Accessible.name: model.name
    }

    popup: T.Popup {
        y: -height - Theme.px(2)
        width: Math.max(control.width, contentItem.implicitWidth + leftPadding + rightPadding)
        implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
        padding: Theme.px(1)
        contentItem: ListView {
            implicitHeight: contentHeight
            implicitWidth: {
                let w = 0
                for (let i = 0; i < contentItem.children.length; ++i)
                    w = Math.max(w, contentItem.children[i].implicitWidth)
                return w
            }
            clip: true
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
        }
        background: GradientBox {
            color: Theme.tokens.controls.menu.background
            borderWidth: Theme.px(1)
            borderColor: Theme.tokens.controls.menu.borderColor
        }
    }
}
