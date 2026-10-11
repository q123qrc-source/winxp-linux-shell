// SPDX-License-Identifier: GPL-3.0-or-later
// SDDM entry point for the Meadow Welcome screen.
//
// SDDM creates one view per screen and provides these context properties:
// sddm, userModel, sessionModel, keyboard, screenModel, config, primaryScreen.
// The primary screen shows the full Welcome screen; other screens show only
// the background.
import QtQuick
import QtQuick.Window
import Meadow.Controls
import Meadow.Welcome

Item {
    id: root

    readonly property bool isPrimary: typeof primaryScreen === "undefined" ? true : primaryScreen

    LoggingCategory {
        id: lcGreeter
        name: "meadow.greeter"
        defaultLogLevel: LoggingCategory.Info
    }

    // theme.conf values arrive as strings; keep the parsing in one place.
    function configString(key, fallback) {
        const v = config.stringValue(key)
        return v === undefined || v === null || v === "" ? fallback : String(v)
    }
    function configBool(key, fallback) {
        const v = configString(key, "")
        return v === "" ? fallback : (v === "true" || v === "1" || v === "yes")
    }

    // "auto" follows the screen's logical DPI (Qt already applies any device
    // pixel ratio), snapped to the scales the theme is designed for.
    function applyScale() {
        const setting = configString("scale", "auto")
        if (setting !== "auto") {
            Theme.scale = Number(setting) || 1
            return
        }
        const ratio = Screen.logicalPixelDensity * 25.4 / Theme.tokens.scale.baseDpi // theme-ok: millimetres per inch
        const steps = Theme.tokens.scale.supported
        let chosen = steps[0]
        for (let i = 0; i < steps.length; ++i) {
            if (ratio + 0.1 >= steps[i]) // theme-ok: rounding tolerance for DPI snapping
                chosen = steps[i]
        }
        Theme.scale = chosen
    }

    // Entry points used by keyboard handling and by tools/greeter-preview.
    function selectUser(index) { layout.selectUser(index) }
    function submitPassword(text) { layout.submitPassword(text) }
    function openTurnOffDialog() { layout.openTurnOffDialog() }

    Component.onCompleted: {
        const scheme = configString("scheme", "luna-blue")
        if (scheme !== Theme.schemeId && !Theme.loadScheme(scheme))
            console.warn(lcGreeter, "unknown scheme", scheme, "- keeping", Theme.schemeId)
        applyScale()
        console.info(lcGreeter, "welcome screen ready on", isPrimary ? "primary" : "secondary", "screen; scheme",
                     Theme.schemeId, "scale", Theme.scale)
    }

    WelcomeLayout {
        id: layout
        anchors.fill: parent
        focus: true
        showUi: root.isPrimary
        greeter: sddm
        users: userModel
        sessions: sessionModel
        keyboardModel: keyboard
        background: root.configString("background", "welcome")
        showSessionSwitcher: root.configBool("showSessionSwitcher", true)
        showKeyboardLayout: root.configBool("showKeyboardLayout", true)
        highContrast: root.configBool("highContrast", false)
        showAllPowerActions: root.configBool("debugPowerButtons", false)
        fingerprintMode: root.configString("fingerprint", "auto")
        virtualKeyboardSource: Qt.resolvedUrl("components/VirtualKeyboardPanel.qml")
    }
}
