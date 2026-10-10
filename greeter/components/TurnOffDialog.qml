// SPDX-License-Identifier: GPL-3.0-or-later
// "Turn off computer" dialog: Stand By (yellow), Turn Off (red) and Restart
// (green) round buttons over a dimmed screen. Holding Shift turns Stand By
// into Hibernate. Buttons whose action the system cannot perform are hidden.
import QtQuick
import Meadow.Controls

FocusScope {
    id: root

    // SDDM proxy (canSuspend, canHibernate, canPowerOff, canReboot and the
    // matching slots).
    property var greeter
    // Show every button even if SDDM reports it unavailable (theme.conf
    // debugPowerButtons, for test mode).
    property bool showAll: false
    property bool highContrast: false
    property bool shiftHeld: false

    readonly property var tk: Theme.tokens.dialogs.turnOff
    readonly property var dk: Theme.tokens.dialogs
    readonly property bool canSuspend: showAll || (greeter && greeter.canSuspend)
    readonly property bool canHibernate: showAll || (greeter && greeter.canHibernate)
    readonly property bool hibernateMode: canHibernate && (shiftHeld || !canSuspend)

    signal closed()

    LoggingCategory {
        id: lcPower
        name: "meadow.greeter.power"
        defaultLogLevel: LoggingCategory.Info
    }

    function open() {
        visible = true
        forceActiveFocus()
        const first = [standBy, turnOff, restart].find((b) => b.visible)
        if (first)
            first.forceActiveFocus()
    }
    function close() {
        visible = false
        shiftHeld = false
        closed()
    }
    function perform(action) {
        console.info(lcPower, "power action requested:", action)
        close()
        if (!greeter || typeof greeter[action] !== "function") {
            console.warn(lcPower, "greeter cannot", action)
            return
        }
        greeter[action]()
    }

    visible: false
    Keys.onEscapePressed: close()
    Keys.onPressed: (event) => { if (event.key === Qt.Key_Shift) shiftHeld = true }
    Keys.onReleased: (event) => { if (event.key === Qt.Key_Shift) shiftHeld = false }

    Accessible.role: Accessible.Dialog
    Accessible.name: qsTr("Turn off computer")

    GradientBox {
        anchors.fill: parent
        color: root.highContrast ? Theme.tokens.colors.highContrastBackground : root.dk.dimOverlayColor
        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }
    }

    component RoundButton: FocusScope {
        id: button
        property string label
        property var stops
        property int glyph
        signal triggered()

        width: Math.max(circle.width, caption.implicitWidth)
        height: circle.height + Theme.px(root.tk.labelSpacing) + caption.implicitHeight
        activeFocusOnTab: true
        Keys.onReturnPressed: triggered()
        Keys.onEnterPressed: triggered()
        Keys.onSpacePressed: triggered()
        Accessible.role: Accessible.Button
        Accessible.name: label
        Accessible.onPressAction: triggered()

        GradientBox {
            id: circle
            anchors.horizontalCenter: parent.horizontalCenter
            width: Theme.px(root.tk.roundButtonSize)
            height: width
            radius: width / 2
            stops: root.highContrast ? [] : button.stops
            color: Theme.tokens.colors.highContrastAccent
            borderWidth: Theme.px(1)
            borderColor: Theme.tokens.colors.white

            LunaGlyph {
                anchors.fill: parent
                anchors.margins: Theme.px(Theme.tokens.window.buttons.glyphInset)
                glyph: button.glyph
                color: root.highContrast ? Theme.tokens.colors.highContrastBackground : root.tk.glyphColor
            }
            GradientBox {
                anchors.fill: parent
                radius: parent.radius
                color: area.pressed ? root.tk.pressedOverlay : area.containsMouse ? root.tk.hoverOverlay : "transparent"
            }
        }
        Text {
            id: caption
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: circle.bottom
            anchors.topMargin: Theme.px(root.tk.labelSpacing)
            text: button.label
            font: Theme.fonts.ui
            color: root.highContrast ? Theme.tokens.colors.highContrastText : root.tk.labelColor

            GradientBox {
                anchors.fill: parent
                anchors.margins: -Theme.px(1)
                visible: button.activeFocus
                borderWidth: 1
                borderDotted: true
                borderColor: parent.color
            }
        }
        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            onClicked: button.triggered()
        }
    }

    Item {
        id: dialog
        anchors.centerIn: parent
        width: Theme.px(root.tk.width)
        height: Theme.px(root.tk.height)

        // Keep clicks inside the dialog from closing it.
        MouseArea { anchors.fill: parent }

        GradientBox {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.px(root.dk.headerHeight)
            stops: root.highContrast ? [] : root.dk.headerGradient
            color: Theme.tokens.colors.highContrastBackground
            topLeftRadius: Theme.px(root.dk.radius)
            topRightRadius: Theme.px(root.dk.radius)
            bottomLeftRadius: 0
            bottomRightRadius: 0
            borderWidth: root.highContrast ? Theme.px(1) : 0
            borderColor: Theme.tokens.colors.highContrastAccent

            Text {
                anchors.left: parent.left
                anchors.leftMargin: Theme.px(root.dk.padding)
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Turn off computer")
                font: Theme.fonts.menuHeader
                color: root.highContrast ? Theme.tokens.colors.highContrastText : root.dk.headerTextColor
            }
            Logo {
                anchors.right: parent.right
                anchors.rightMargin: Theme.px(root.dk.padding)
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.px(root.tk.logoSize)
                height: width
            }
        }

        GradientBox {
            id: body
            anchors.top: header.bottom
            anchors.bottom: footer.top
            anchors.left: parent.left
            anchors.right: parent.right
            orientation: Qt.Horizontal
            stops: root.highContrast ? [] : root.tk.bodyGradient
            color: Theme.tokens.colors.highContrastBackground

            Row {
                anchors.centerIn: parent
                spacing: Theme.px(root.tk.buttonSpacing)

                RoundButton {
                    id: standBy
                    objectName: "standByButton"
                    visible: root.canSuspend || root.canHibernate
                    label: root.hibernateMode ? qsTr("Hibernate") : qsTr("Stand By")
                    stops: root.tk.standByGradient
                    glyph: LunaGlyph.Power
                    onTriggered: root.perform(root.hibernateMode ? "hibernate" : "suspend")
                    KeyNavigation.right: turnOff.visible ? turnOff : restart
                }
                RoundButton {
                    id: turnOff
                    objectName: "turnOffButton"
                    visible: root.showAll || (root.greeter && root.greeter.canPowerOff)
                    label: qsTr("Turn Off")
                    stops: root.tk.turnOffGradient
                    glyph: LunaGlyph.Power
                    onTriggered: root.perform("powerOff")
                    KeyNavigation.left: standBy
                    KeyNavigation.right: restart
                }
                RoundButton {
                    id: restart
                    objectName: "restartButton"
                    visible: root.showAll || (root.greeter && root.greeter.canReboot)
                    label: qsTr("Restart")
                    stops: root.tk.restartGradient
                    glyph: LunaGlyph.Restart
                    onTriggered: root.perform("reboot")
                    KeyNavigation.left: turnOff.visible ? turnOff : standBy
                    KeyNavigation.right: cancel
                }
            }
        }

        GradientBox {
            id: footer
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.px(root.tk.footerHeight)
            color: root.highContrast ? Theme.tokens.colors.highContrastBackground : root.dk.footerColor
            bottomLeftRadius: Theme.px(root.dk.radius)
            bottomRightRadius: Theme.px(root.dk.radius)
            topLeftRadius: 0
            topRightRadius: 0
            borderWidth: root.highContrast ? Theme.px(1) : 0
            borderColor: Theme.tokens.colors.highContrastAccent

            LunaButton {
                id: cancel
                anchors.right: parent.right
                anchors.rightMargin: Theme.px(root.dk.padding)
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Cancel")
                onClicked: root.close()
                KeyNavigation.left: restart.visible ? restart : turnOff
            }
        }
    }
}
