// SPDX-License-Identifier: GPL-3.0-or-later
// Full-screen host for Log Off and Turn Off Computer. Before a dialog shows,
// the screen is captured; the capture is shown in color and then fades to
// gray behind the dialog (dialogs.grayscaleFadeMs; instant with reduced
// motion), like XP. Without a capture it falls back to a dark overlay.
// In a session this is the content of an overlay layer surface.
import QtQuick
import Meadow.Controls
import Meadow.Shell

FocusScope {
    id: host

    // Captures the preview window instead of the screen (--preview).
    property var previewWindow: null
    // Screen name for grim (QScreen::name) in a session.
    property string outputName
    // A dialog is open (or about to be, while capturing). Tracked here, not
    // read from the dialogs' `visible`, which is false while this is hidden.
    readonly property bool active: pending.length > 0 || current.length > 0
    property string pending: ""
    property string current: ""

    readonly property var dk: Theme.tokens.dialogs

    function openLogOff() { request("logOff") }
    function openTurnOff() { request("turnOff") }

    function request(which) {
        if (active)
            return
        pending = which
        // Let the Start menu that asked for this disappear from the screen first.
        captureDelay.restart()
    }
    function capture() {
        if (previewWindow)
            Shell.screen.grab(previewWindow)
        else
            Shell.screen.grabOutput(outputName)
        captureTimeout.restart()
    }
    function show() {
        captureTimeout.stop()
        const which = pending
        pending = ""
        if (which === "")
            return
        current = which
        fade.restart()
        const dialog = which === "logOff" ? logOff : turnOff
        dialog.open()
    }

    Connections {
        target: Shell.screen
        function onFinished() { host.show() }
    }
    Timer {
        id: captureDelay
        interval: Theme.tokens.animation.fast
        onTriggered: host.capture()
    }
    // A capture that hangs must not keep the dialog away.
    Timer {
        id: captureTimeout
        interval: host.dk.captureTimeoutMs
        onTriggered: host.show()
    }

    // The captured screen, crossfading to gray.
    Item {
        anchors.fill: parent
        visible: host.current.length > 0 && Shell.screen.ready

        Image {
            anchors.fill: parent
            source: Shell.screen.ready ? "image://screen/color?" + Shell.screen.revision : ""
            cache: false
            smooth: true
        }
        Image {
            id: grayImage
            anchors.fill: parent
            source: Shell.screen.ready ? "image://screen/gray?" + Shell.screen.revision : ""
            cache: false
            smooth: true
            opacity: 0
        }
        NumberAnimation {
            id: fade
            target: grayImage
            property: "opacity"
            from: Theme.reducedMotion ? 1 : 0
            to: 1
            duration: Theme.reducedMotion ? 0 : host.dk.grayscaleFadeMs
        }
    }

    LogOffDialog {
        id: logOff
        anchors.fill: parent
        backdropColor: Shell.screen.ready ? "transparent" : host.dk.dimOverlayColor
        onClosed: { host.current = ""; Shell.screen.clear() }
    }
    TurnOffComputerDialog {
        id: turnOff
        anchors.fill: parent
        backdropColor: Shell.screen.ready ? "transparent" : host.dk.dimOverlayColor
        onClosed: { host.current = ""; Shell.screen.clear() }
    }
}
