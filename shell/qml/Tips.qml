// SPDX-License-Identifier: GPL-3.0-or-later
// Tooltip requests from taskbar items. The taskbar surface is only as tall as
// the bar, so tooltips are drawn by a TipHost in a separate, input-transparent
// overlay (or the preview window) instead of inside the item.
pragma Singleton
import QtQuick

QtObject {
    // Text and horizontal center (window coordinates) of the shown tip.
    property string text
    property real x
    property Item owner: null

    function show(item, tipText) {
        owner = item
        x = item.mapToItem(null, item.width / 2, 0).x
        text = tipText
    }
    function hide(item) {
        if (item === undefined || item === owner) {
            owner = null
            text = ""
        }
    }
}
