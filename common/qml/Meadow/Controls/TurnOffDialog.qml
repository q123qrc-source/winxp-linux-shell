// SPDX-License-Identifier: GPL-3.0-or-later
// "Turn off computer": Stand By (yellow), Turn Off (red) and Restart (green).
// Holding Shift turns Stand By into Hibernate. Buttons whose action the
// system cannot perform are hidden. `power` is SDDM's greeter proxy or the
// shell's PowerService (canSuspend, canHibernate, canPowerOff, canReboot and
// the matching suspend/hibernate/powerOff/reboot methods).
import QtQuick
import Meadow.Controls

PowerDialogFrame {
    id: root

    property var power
    // Show every button even if `power` reports it unavailable (theme.conf
    // debugPowerButtons, for SDDM test mode).
    property bool showAll: false
    property bool shiftHeld: false
    // Optional function(action) that performs the action instead of calling
    // power[action]() directly (the shell plays the shutdown sound first).
    property var actionHandler: null

    readonly property bool canSuspend: showAll || (power && power.canSuspend)
    readonly property bool canHibernate: showAll || (power && power.canHibernate)
    readonly property bool hibernateMode: canHibernate && (shiftHeld || !canSuspend)

    // The action chosen; the dialog closes first.
    signal actionRequested(string action)

    title: qsTr("Turn off computer")
    emptyText: qsTr("This session cannot turn off, restart or suspend the computer.")

    LoggingCategory {
        id: lcPower
        name: "meadow.power"
        defaultLogLevel: LoggingCategory.Info
    }

    function perform(action) {
        console.info(lcPower, "power action requested:", action)
        close()
        shiftHeld = false
        actionRequested(action)
        if (typeof actionHandler === "function") {
            actionHandler(action)
            return
        }
        if (!power || typeof power[action] !== "function") {
            console.warn(lcPower, "cannot", action)
            return
        }
        power[action]()
    }

    onClosed: shiftHeld = false
    Keys.onPressed: (event) => { if (event.key === Qt.Key_Shift) shiftHeld = true }
    Keys.onReleased: (event) => { if (event.key === Qt.Key_Shift) shiftHeld = false }

    PowerButton {
        id: standBy
        objectName: "standByButton"
        visible: root.canSuspend || root.canHibernate
        label: root.hibernateMode ? qsTr("Hibernate") : qsTr("Stand By")
        stops: root.tk.standByGradient
        glyph: LunaGlyph.Power
        highContrast: root.highContrast
        onTriggered: root.perform(root.hibernateMode ? "hibernate" : "suspend")
        KeyNavigation.right: turnOff.visible ? turnOff : restart
    }
    PowerButton {
        id: turnOff
        objectName: "turnOffButton"
        visible: root.showAll || (root.power && root.power.canPowerOff)
        label: qsTr("Turn Off")
        stops: root.tk.turnOffGradient
        glyph: LunaGlyph.Power
        highContrast: root.highContrast
        onTriggered: root.perform("powerOff")
        KeyNavigation.left: standBy
        KeyNavigation.right: restart
    }
    PowerButton {
        id: restart
        objectName: "restartButton"
        visible: root.showAll || (root.power && root.power.canReboot)
        label: qsTr("Restart")
        stops: root.tk.restartGradient
        glyph: LunaGlyph.Restart
        highContrast: root.highContrast
        onTriggered: root.perform("reboot")
        KeyNavigation.left: turnOff.visible ? turnOff : standBy
        KeyNavigation.right: root.cancelButton
    }
}
