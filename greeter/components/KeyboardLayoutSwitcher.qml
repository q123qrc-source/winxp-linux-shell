// SPDX-License-Identifier: GPL-3.0-or-later
// Keyboard layout indicator ("EN", "AR", ...). Clicking cycles to the next
// layout; the arrow opens a list of all layouts. Alt+Shift also cycles
// (handled by WelcomeLayout so it works from the password field).
import QtQuick
import QtQuick.Templates as T
import Meadow.Controls

Row {
    id: root

    // SDDM's keyboard model: layouts (shortName, longName), currentLayout, enabled.
    property var keyboardModel
    property bool highContrast: false

    readonly property var layouts: keyboardModel && keyboardModel.layouts ? keyboardModel.layouts : []
    readonly property int current: keyboardModel ? keyboardModel.currentLayout : -1
    readonly property string shortName: current >= 0 && current < layouts.length ? layouts[current].shortName : ""
    readonly property string longName: current >= 0 && current < layouts.length ? layouts[current].longName : ""

    function cycle() {
        if (layouts.length > 1)
            keyboardModel.currentLayout = (current + 1) % layouts.length
    }

    visible: keyboardModel !== undefined && keyboardModel !== null && keyboardModel.enabled && layouts.length > 0
    spacing: Theme.px(1)

    PillButton {
        id: indicator
        text: root.shortName.toUpperCase()
        highContrast: root.highContrast
        onClicked: root.cycle()
        Accessible.name: qsTr("Keyboard layout: %1. Press to switch.").arg(root.longName)
        LunaTooltip {
            text: root.longName
            visible: indicator.hovered && root.longName.length > 0
            y: -height - Theme.px(Theme.tokens.controls.tooltip.padding)
        }
    }

    PillButton {
        id: listButton
        visible: root.layouts.length > 1
        glyph: LunaGlyph.ArrowUp
        highContrast: root.highContrast
        onClicked: menu.open()
        Accessible.name: qsTr("Choose keyboard layout")

        T.Popup {
            id: menu
            y: -height - Theme.px(2)
            x: listButton.width - width
            padding: Theme.px(1)
            implicitWidth: list.implicitWidth + leftPadding + rightPadding
            implicitHeight: list.contentHeight + topPadding + bottomPadding
            onOpened: list.forceActiveFocus()

            contentItem: ListView {
                id: list
                implicitWidth: {
                    let w = 0
                    for (let i = 0; i < contentItem.children.length; ++i)
                        w = Math.max(w, contentItem.children[i].implicitWidth)
                    return w
                }
                model: root.layouts
                currentIndex: root.current
                keyNavigationEnabled: true
                Keys.onReturnPressed: { root.keyboardModel.currentLayout = currentIndex; menu.close() }
                Keys.onEnterPressed: { root.keyboardModel.currentLayout = currentIndex; menu.close() }

                delegate: T.ItemDelegate {
                    required property var modelData
                    required property int index
                    width: ListView.view.width
                    implicitWidth: entry.implicitWidth + leftPadding + rightPadding
                    implicitHeight: Theme.px(Theme.tokens.controls.menu.itemHeight) + Theme.px(2) * 2
                    leftPadding: Theme.px(Theme.tokens.welcome.bottomControls.paddingH)
                    rightPadding: leftPadding
                    highlighted: ListView.isCurrentItem || hovered
                    hoverEnabled: true
                    contentItem: Text {
                        id: entry
                        text: modelData.shortName.toUpperCase() + "  " + modelData.longName
                        font: Theme.fonts.ui
                        color: parent.highlighted ? Theme.tokens.controls.menu.hoverText : Theme.tokens.colors.windowText
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: GradientBox {
                        color: parent.highlighted ? Theme.tokens.controls.menu.hoverBackground : "transparent"
                    }
                    onClicked: { root.keyboardModel.currentLayout = index; menu.close() }
                    Accessible.name: modelData.longName
                }
            }
            background: GradientBox {
                color: Theme.tokens.controls.menu.background
                borderWidth: Theme.px(1)
                borderColor: Theme.tokens.controls.menu.borderColor
            }
        }
    }
}
