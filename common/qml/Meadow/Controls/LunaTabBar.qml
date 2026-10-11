// SPDX-License-Identifier: GPL-3.0-or-later
// Property-sheet tabs: light gradient tabs on a pale pane border; the
// selected tab is raised and joins the pane, a hovered tab gets an orange
// top edge. Arrow keys move between tabs (Ctrl+Tab is handled by
// LunaTabView). `tabs` is a list of strings.
import QtQuick
import Meadow.Controls

FocusScope {
    id: bar

    property var tabs: []
    property int currentIndex: 0
    // -1 or a tab index forced to look hovered (gallery, screenshots).
    property int previewHover: -1

    readonly property var tk: Theme.tokens.controls.tab
    readonly property int raise: Theme.px(tk.selectedRaise)

    implicitHeight: Theme.px(tk.height) + raise
    implicitWidth: row.implicitWidth

    Accessible.role: Accessible.PageTabList

    Keys.onLeftPressed: currentIndex = Math.max(0, currentIndex - 1)
    Keys.onRightPressed: currentIndex = Math.min(tabs.length - 1, currentIndex + 1)

    Row {
        id: row
        x: Theme.px(bar.tk.firstTabInset)
        height: parent.height
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft

        Repeater {
            model: bar.tabs
            delegate: Item {
                id: tab
                required property string modelData
                required property int index
                readonly property bool selected: index === bar.currentIndex
                readonly property bool hot: mouse.containsMouse || bar.previewHover === index

                // The selected tab is wider and overlaps its neighbours.
                width: label.implicitWidth + Theme.px(bar.tk.paddingH) * 2 + (selected ? bar.raise * 2 : 0)
                height: bar.height
                z: selected ? 1 : 0
                objectName: "tab:" + modelData

                Accessible.role: Accessible.PageTab
                Accessible.name: modelData
                Accessible.selected: selected
                Accessible.onPressAction: bar.currentIndex = index

                GradientBox {
                    x: tab.selected ? -bar.raise : 0
                    y: tab.selected ? 0 : bar.raise
                    width: parent.width
                    height: parent.height - y + (tab.selected ? Theme.px(1) : 0)
                    radius: Theme.px(bar.tk.radius)
                    borderWidth: Theme.px(1)
                    borderColor: bar.tk.borderColor
                    stops: tab.selected ? [] : bar.tk.gradient
                    color: bar.tk.paneBackground

                    // Hover: an orange line along the top edge.
                    GradientBox {
                        visible: tab.hot || (tab.selected && bar.activeFocus)
                        x: Theme.px(1)
                        y: 0
                        width: parent.width - Theme.px(2)
                        height: Theme.px(bar.tk.hotLineHeight)
                        radius: Theme.px(bar.tk.radius)
                        stops: bar.tk.hotLineGradient
                    }
                }
                Text {
                    id: label
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: (tab.selected ? 0 : bar.raise) + (parent.height - (tab.selected ? 0 : bar.raise) - height) / 2
                    text: tab.modelData
                    font: Theme.fonts.ui
                    color: Theme.tokens.colors.windowText

                    // Keyboard focus: dotted rectangle around the label.
                    GradientBox {
                        anchors.fill: parent
                        anchors.margins: -Theme.px(1)
                        visible: tab.selected && bar.activeFocus
                        borderWidth: 1
                        borderDotted: true
                        borderColor: Theme.tokens.colors.focusRing
                    }
                }
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        bar.currentIndex = tab.index
                        bar.forceActiveFocus()
                    }
                }
            }
        }
    }
}
