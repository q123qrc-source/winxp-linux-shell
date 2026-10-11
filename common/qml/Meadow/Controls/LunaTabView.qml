// SPDX-License-Identifier: GPL-3.0-or-later
// A property sheet: LunaTabBar over a pane with a thin border and the pale
// tab-pane gradient. Pages are the view's children, shown one at a time;
// each page's `title` (a property on the page item) names its tab.
// Ctrl+Tab / Ctrl+Shift+Tab switch pages.
//
//   LunaTabView {
//       Item { property string title: qsTr("Desktop") ... }
//       Item { property string title: qsTr("Settings") ... }
//   }
import QtQuick
import Meadow.Controls

FocusScope {
    id: view

    default property alias pages: stack.data
    property alias currentIndex: tabBar.currentIndex
    readonly property alias tabBar: tabBar
    readonly property var tk: Theme.tokens.controls.tab
    readonly property int count: stack.children.length

    implicitWidth: Math.max(tabBar.implicitWidth, stack.childrenRect.width + Theme.px(tk.panePadding) * 2)

    function titles() {
        const out = []
        for (let i = 0; i < stack.children.length; ++i)
            out.push(stack.children[i].title !== undefined ? stack.children[i].title : "")
        return out
    }
    function page(i) { return stack.children[i] }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Tab && (event.modifiers & Qt.ControlModifier)) {
            currentIndex = (currentIndex + 1) % count
            event.accepted = true
        } else if (event.key === Qt.Key_Backtab && (event.modifiers & Qt.ControlModifier)) {
            currentIndex = (currentIndex - 1 + count) % count
            event.accepted = true
        }
    }

    LunaTabBar {
        id: tabBar
        width: parent.width
        tabs: view.titles()
        focus: true
        z: 1
    }

    GradientBox {
        id: pane
        anchors.top: tabBar.bottom
        anchors.topMargin: -Theme.px(1)
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        borderWidth: Theme.px(1)
        borderColor: view.tk.borderColor
        stops: view.tk.paneGradient

        Item {
            id: stack
            anchors.fill: parent
            anchors.margins: Theme.px(view.tk.panePadding)
            onChildrenChanged: view.updateVisibility()
        }
    }

    function updateVisibility() {
        for (let i = 0; i < stack.children.length; ++i) {
            const p = stack.children[i]
            p.visible = i === currentIndex
            if (p.width === 0)
                p.width = Qt.binding(() => stack.width)
            if (p.height === 0)
                p.height = Qt.binding(() => stack.height)
        }
    }
    onCurrentIndexChanged: updateVisibility()
    Component.onCompleted: updateVisibility()
}
