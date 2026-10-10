// SPDX-License-Identifier: GPL-3.0-or-later
// Scroll bar with arrow buttons at both ends, a pale track and a rounded blue
// thumb with grip lines. Works vertically and horizontally.
import QtQuick
import QtQuick.Templates as T
import Meadow.Controls

T.ScrollBar {
    id: control

    // "", "hover" or "pressed" (applies to the thumb; gallery/tests only).
    property string previewState: ""

    readonly property var tk: Theme.tokens.controls.scrollBar
    readonly property int thickness: Theme.px(tk.width)
    readonly property bool showPressed: pressed || previewState === "pressed"
    readonly property bool showHover: !showPressed && (hovered || previewState === "hover")
    // The thumb gradient runs across the bar, so it is perpendicular to it.
    readonly property int crossOrientation: vertical ? Qt.Horizontal : Qt.Vertical

    readonly property int minimumLength: 2 * thickness + Theme.px(tk.minThumbLength)
    implicitWidth: vertical ? thickness : minimumLength
    implicitHeight: vertical ? minimumLength : thickness
    topPadding: vertical ? thickness : 0
    bottomPadding: vertical ? thickness : 0
    leftPadding: vertical ? 0 : thickness
    rightPadding: vertical ? 0 : thickness
    minimumSize: vertical ? Theme.px(tk.minThumbLength) / Math.max(1, height - 2 * thickness)
                          : Theme.px(tk.minThumbLength) / Math.max(1, width - 2 * thickness)
    hoverEnabled: true
    visible: policy === T.ScrollBar.AlwaysOn || (policy === T.ScrollBar.AsNeeded && size < 1)

    Accessible.role: Accessible.ScrollBar

    // A gradient flipped end-to-end: the sunken look of a pressed part.
    function reversed(stops) {
        return stops.slice().reverse().map((s) => ({ pos: 1 - s.pos, color: s.color }))
    }

    component ArrowButton: GradientBox {
        id: arrow
        property int glyph
        property bool decrease
        width: control.thickness
        height: control.thickness
        radius: Theme.px(control.tk.thumbRadius)
        orientation: control.crossOrientation
        borderWidth: Theme.px(1)
        borderColor: control.tk.thumbBorderColor
        stops: area.pressed ? control.reversed(control.tk.thumbGradient)
             : area.containsMouse ? control.tk.thumbHoverGradient : control.tk.thumbGradient

        LunaGlyph {
            anchors.fill: parent
            anchors.margins: Theme.px(control.tk.arrowInset)
            glyph: arrow.glyph
            color: control.tk.arrowColor
        }

        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            onPressed: { arrow.step(); repeat.interval = control.tk.autoRepeatDelayMs; repeat.start() }
            onReleased: repeat.stop()
            onCanceled: repeat.stop()
        }
        Timer {
            id: repeat
            repeat: true
            onTriggered: { interval = control.tk.autoRepeatMs; arrow.step() }
        }
        function step() {
            if (decrease)
                control.decrease()
            else
                control.increase()
        }
    }

    background: GradientBox {
        color: control.tk.trackColor

        ArrowButton {
            x: 0
            y: 0
            decrease: true
            glyph: control.vertical ? LunaGlyph.ArrowUp : LunaGlyph.ArrowLeft
        }
        ArrowButton {
            x: control.vertical ? 0 : parent.width - width
            y: control.vertical ? parent.height - height : 0
            decrease: false
            glyph: control.vertical ? LunaGlyph.ArrowDown : LunaGlyph.ArrowRight
        }
    }

    contentItem: GradientBox {
        implicitWidth: control.thickness
        implicitHeight: control.thickness
        radius: Theme.px(control.tk.thumbRadius)
        orientation: control.crossOrientation
        borderWidth: Theme.px(1)
        borderColor: control.tk.thumbBorderColor
        stops: control.showPressed ? control.reversed(control.tk.thumbGradient)
             : control.showHover ? control.tk.thumbHoverGradient : control.tk.thumbGradient

        // Grip lines in the middle of the thumb (hidden when it is too short).
        Grid {
            anchors.centerIn: parent
            // Each grip line is 1px plus 1px spacing; keep a 2px margin at both ends.
            visible: (control.vertical ? parent.height : parent.width) > Theme.px(control.tk.gripLines * 2 + 2 * 2)
            rows: control.vertical ? control.tk.gripLines : 1
            columns: control.vertical ? 1 : control.tk.gripLines
            spacing: Theme.px(1)
            Repeater {
                model: control.tk.gripLines
                GradientBox {
                    width: control.vertical ? Theme.px(control.tk.gripLength) : Theme.px(1)
                    height: control.vertical ? Theme.px(1) : Theme.px(control.tk.gripLength)
                    color: control.tk.gripColor
                }
            }
        }
    }
}
