// SPDX-License-Identifier: GPL-3.0-or-later
// "Keyboard Properties": Speed (repeat delay and rate, saved to shell.conf
// [Keyboard] and applied by labwc) and Input Languages (the language bar's
// layouts; adding or removing one applies at once, like the language bar).
import QtQuick
import Meadow.Controls
import Meadow.Shell

AppletSheet {
    id: applet

    readonly property var ktk: Theme.tokens.dialogs.keyboard
    property int repeatDelay
    property int repeatRate
    property var available: []

    function load(tab) {
        repeatDelay = Shell.settings.intSetting("Keyboard/repeatDelay", ktk.repeatDelayDefaultMs)
        repeatRate = Shell.settings.intSetting("Keyboard/repeatRate", ktk.repeatRateDefault)
        available = Shell.keyboard.availableLayouts()
        showTab(tab)
    }
    dirty: {
        Shell.settings.revision
        return repeatDelay !== Shell.settings.intSetting("Keyboard/repeatDelay", ktk.repeatDelayDefaultMs)
            || repeatRate !== Shell.settings.intSetting("Keyboard/repeatRate", ktk.repeatRateDefault)
    }
    onApplyRequested: {
        Shell.settings.set("Keyboard/repeatDelay", repeatDelay)
        Shell.settings.set("Keyboard/repeatRate", repeatRate)
    }

    Item {
        property string title: qsTr("Speed")
        property string key: "speed"
        Column {
            width: parent.width
            spacing: applet.pad
            LunaGroupBox {
                width: parent.width
                title: qsTr("Character repeat")
                titleBackground: Theme.tokens.controls.tab.paneBackground
                Column {
                    spacing: applet.pad
                    Text { text: qsTr("Repeat delay:"); font: Theme.fonts.ui }
                    Row {
                        spacing: applet.pad
                        Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Long"); font: Theme.fonts.ui }
                        LunaSlider {
                            objectName: "repeatDelaySlider"
                            from: applet.ktk.repeatDelayLongMs
                            to: applet.ktk.repeatDelayShortMs
                            stepSize: applet.ktk.repeatDelayStepMs
                            tickCount: applet.ktk.tickCount
                            value: applet.repeatDelay
                            onMoved: applet.repeatDelay = Math.round(value)
                        }
                        Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Short"); font: Theme.fonts.ui }
                    }
                    Text { text: qsTr("Repeat rate:"); font: Theme.fonts.ui }
                    Row {
                        spacing: applet.pad
                        Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Slow"); font: Theme.fonts.ui }
                        LunaSlider {
                            objectName: "repeatRateSlider"
                            from: applet.ktk.repeatRateSlow
                            to: applet.ktk.repeatRateFast
                            stepSize: 1
                            tickCount: applet.ktk.tickCount
                            value: applet.repeatRate
                            onMoved: applet.repeatRate = Math.round(value)
                        }
                        Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Fast"); font: Theme.fonts.ui }
                    }
                    Text { text: qsTr("Click here and hold down a key to test repeat rate (after Apply):"); font: Theme.fonts.ui }
                    LunaTextField { width: Theme.px(applet.tk.fieldWidth) }
                }
            }
        }
    }

    Item {
        id: languagesPage
        property string title: qsTr("Input Languages")
        property string key: "languages"
        Column {
            width: parent.width
            spacing: applet.pad
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font: Theme.fonts.ui
                text: qsTr("Installed input languages. The first one is in use; Alt+Shift or the language bar switches between them.")
            }
            LunaListView {
                id: installed
                objectName: "installedLayouts"
                width: parent.width
                model: Shell.keyboard.layouts.map(l => ({ label: l.shortName + "  " + l.name }))
            }
            Row {
                spacing: applet.pad
                LunaComboBox {
                    id: addCombo
                    objectName: "addLayoutCombo"
                    width: Theme.px(applet.tk.fieldWidth)
                    model: applet.available.map(l => l.name)
                }
                LunaButton {
                    objectName: "addLayout"
                    text: qsTr("Add")
                    enabled: addCombo.currentIndex >= 0
                    onClicked: Shell.keyboard.addLayout(applet.available[addCombo.currentIndex].code)
                }
            }
            LunaButton {
                objectName: "removeLayout"
                text: qsTr("Remove")
                enabled: Shell.keyboard.count > 1 && installed.currentIndex >= 0
                onClicked: Shell.keyboard.removeLayout(installed.currentIndex)
            }
        }
    }
}
