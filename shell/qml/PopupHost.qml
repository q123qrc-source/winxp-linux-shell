// SPDX-License-Identifier: GPL-3.0-or-later
// Holds the taskbar's popups (menus, calendar, volume). It fills the screen
// above the taskbar: in a session it is the content of a full-screen overlay
// layer surface, in --preview it covers the preview window. A press outside
// every popup, or Escape, closes them all, like XP menus.
import QtQuick
import Meadow.Controls

FocusScope {
    id: host

    // Distance from the bottom of the host to the taskbar's top edge.
    property real bottomInset: 0
    readonly property bool active: layer.children.length > 0
    readonly property Item layerItem: layer

    // Placement functions of the open popups, re-run when the host resizes
    // (in a session the overlay surface gets its size only once it is shown).
    property var placers: []

    // Drag and drop between popups (All Programs -> pinned list): the entry
    // being dragged, and whether a drag is in progress.
    property var dragData: null
    readonly property bool dragging: dragData !== null

    signal closed()

    function beginDrag(entry, scenePos) {
        dragData = entry
        moveDrag(scenePos)
    }
    function moveDrag(scenePos) {
        const p = host.mapFromItem(null, scenePos.x, scenePos.y)
        dragProxy.x = p.x
        dragProxy.y = p.y
    }
    function endDrag() {
        if (!dragging)
            return
        dragProxy.Drag.drop()
        dragData = null
    }

    onWidthChanged: relayout()
    onHeightChanged: relayout()
    onBottomInsetChanged: relayout()

    function relayout() {
        for (let i = 0; i < placers.length; ++i)
            placers[i]()
    }

    // Creates `component` with `props` and places it just above the taskbar:
    // its left edge at `x` (or right edge, with alignRight), kept on screen.
    function open(component, props, x, alignRight) {
        closeAll()
        return openChild(component, props, x, alignRight)
    }

    // Opens another popup without closing the current ones (submenus).
    // `placer(popup)`, if given, positions it instead of the default.
    function openChild(component, props, x, alignRight, placer) {
        const popup = component.createObject(layer, props || {})
        if (!popup)
            return null
        popup.host = host
        const place = placer ? () => placer(popup) : function() {
            const w = popup.width
            const left = alignRight ? x - w : x
            popup.x = Math.max(0, Math.min(left, host.width - w))
            popup.y = Math.max(0, host.height - host.bottomInset - popup.height)
        }
        popup.widthChanged.connect(place)
        popup.heightChanged.connect(place)
        placers.push(place)
        popup.Component.destruction.connect(() => {
            const i = placers.indexOf(place)
            if (i >= 0)
                placers.splice(i, 1)
        })
        place()
        popup.forceActiveFocus()
        return popup
    }

    // A placer for submenus: beside `parentItem`, top aligned with the row at
    // `rowY` within it, flipped left when there is no room on the right.
    function besidePlacer(parentItem, rowY) {
        return function(popup) {
            const rightSpace = host.width - (parentItem.x + parentItem.width)
            popup.x = rightSpace >= popup.width ? parentItem.x + parentItem.width : Math.max(0, parentItem.x - popup.width)
            popup.y = Math.max(0, Math.min(parentItem.y + rowY, host.height - host.bottomInset - popup.height))
        }
    }

    function closeAll() {
        const popups = layer.children.slice()
        for (let i = 0; i < popups.length; ++i)
            popups[i].destroy()
        if (popups.length > 0)
            closed()
    }

    // Closes `popup` and every popup opened after it (its submenus).
    function closeFrom(popup) {
        const popups = layer.children.slice()
        const from = popups.indexOf(popup)
        if (from < 0)
            return
        for (let i = popups.length - 1; i >= from; --i)
            popups[i].destroy()
        if (from === 0)
            closed()
        else
            popups[from - 1].forceActiveFocus() // back to the menu below
    }

    Keys.onEscapePressed: closeAll()

    MouseArea {
        anchors.fill: parent
        enabled: host.active
        acceptedButtons: Qt.AllButtons
        onPressed: host.closeAll()
    }

    Item {
        id: layer
        anchors.fill: parent
    }

    // Follows the pointer during a drag; drop areas accept "meadow/app".
    Item {
        id: dragProxy
        width: Theme.px(Theme.tokens.startMenu.allPrograms.iconSize)
        height: width
        visible: host.dragging
        Drag.active: host.dragging
        Drag.keys: ["meadow/app"]
        Drag.hotSpot.x: 0
        Drag.hotSpot.y: 0

        Image {
            anchors.fill: parent
            sourceSize: Qt.size(width, height)
            source: host.dragData && host.dragData.iconName ? "image://icon/" + host.dragData.iconName : ""
            opacity: Theme.tokens.startMenu.dragIconOpacity
        }
    }
}
