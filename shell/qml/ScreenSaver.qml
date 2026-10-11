// SPDX-License-Identifier: GPL-3.0-or-later
// The screen saver: "blank" (black) or "logo" (the Meadow logo drifting
// slowly and bouncing off the edges; still under reduced motion). Shown
// full screen on an overlay surface when the session goes idle (Session.qml)
// and small inside Display Properties' monitor (preview: true).
//
// Any key, click or pointer movement ends it (dismissed()); the session then
// locks first when "On resume, password protect" is on.
import QtQuick
import QtQuick.Window
import Meadow.Controls
import Meadow.Shell

FocusScope {
    id: saver

    // "blank" or "logo".
    property string mode: "logo"
    property bool preview: false
    property bool running: visible

    readonly property var tk: Theme.tokens.screenSaver

    signal dismissed()

    // Input right after it appears (the pointer it appeared under, the key
    // that is still going up) does not end it.
    property bool armed: false
    onVisibleChanged: {
        armed = false
        if (visible)
            armTimer.restart()
    }
    Timer {
        id: armTimer
        interval: saver.tk.graceMs
        onTriggered: saver.armed = true
    }
    function dismiss() {
        if (armed && !preview)
            dismissed()
    }

    Accessible.role: Accessible.Animation
    Accessible.name: qsTr("Screen saver")

    Rectangle {
        anchors.fill: parent
        color: saver.tk.background
    }

    Logo {
        id: logo
        visible: saver.mode === "logo"
        width: Theme.px(saver.preview ? saver.tk.previewLogoSize : saver.tk.logoSize)
        height: width
        // Direction of travel, +1/-1 on each axis.
        property real dx: 1
        property real dy: 1
        x: (saver.width - width) / 2
        y: (saver.height - height) / 2
    }

    // Drift: steps at frame rate, bouncing at the edges.
    FrameAnimation {
        running: saver.running && saver.mode === "logo" && !Theme.reducedMotion
        onTriggered: {
            const speed = saver.tk.driftPxPerSecond * (saver.preview ? saver.width / Screen.width : 1) * frameTime
            let nx = logo.x + logo.dx * speed
            let ny = logo.y + logo.dy * speed
            if (nx <= 0 || nx >= saver.width - logo.width) {
                logo.dx = -logo.dx
                nx = Math.max(0, Math.min(nx, saver.width - logo.width))
            }
            if (ny <= 0 || ny >= saver.height - logo.height) {
                logo.dy = -logo.dy
                ny = Math.max(0, Math.min(ny, saver.height - logo.height))
            }
            logo.x = nx
            logo.y = ny
        }
    }

    // Input ends it (not in the Display Properties preview).
    MouseArea {
        anchors.fill: parent
        enabled: !saver.preview
        hoverEnabled: true
        property point first: Qt.point(-1, -1)
        onPositionChanged: (mouse) => {
            // The first hover event is where the pointer already was.
            if (first.x < 0)
                first = Qt.point(mouse.x, mouse.y)
            else if (Math.abs(mouse.x - first.x) + Math.abs(mouse.y - first.y) > Theme.px(Theme.tokens.desktop.dragThreshold))
                saver.dismiss()
        }
        onPressed: saver.dismiss()
        onWheel: saver.dismiss()
    }
    Keys.onPressed: (event) => {
        if (!preview) {
            event.accepted = true
            dismiss()
        }
    }
}
