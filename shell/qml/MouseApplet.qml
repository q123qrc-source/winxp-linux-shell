// SPDX-License-Identifier: GPL-3.0-or-later
// "Mouse Properties": Buttons (switch primary and secondary buttons,
// double-click speed with a folder to test it on) and Pointer Options
// (pointer speed). Saved to shell.conf [Mouse]; LabwcConfig hands them to
// labwc (libinput leftHanded / pointerSpeed, doubleClickTime).
import QtQuick
import Meadow.Controls
import Meadow.Shell

AppletSheet {
    id: applet

    readonly property var mtk: Theme.tokens.dialogs.mouse
    property bool swap
    property int doubleClickMs
    property real speed

    function load(tab) {
        swap = Shell.settings.boolSetting("Mouse/swapButtons", false)
        doubleClickMs = Shell.settings.intSetting("Mouse/doubleClickMs", mtk.doubleClickDefaultMs)
        speed = Shell.settings.realSetting("Mouse/pointerSpeed", 0)
        showTab(tab)
    }
    dirty: {
        Shell.settings.revision
        return swap !== Shell.settings.boolSetting("Mouse/swapButtons", false)
            || doubleClickMs !== Shell.settings.intSetting("Mouse/doubleClickMs", mtk.doubleClickDefaultMs)
            || Math.abs(speed - Shell.settings.realSetting("Mouse/pointerSpeed", 0)) > Number.EPSILON
    }
    onApplyRequested: {
        Shell.settings.set("Mouse/swapButtons", swap)
        Shell.settings.set("Mouse/doubleClickMs", doubleClickMs)
        Shell.settings.set("Mouse/pointerSpeed", Math.round(speed * 100) / 100) // theme-ok: two decimals
    }

    Item {
        property string title: qsTr("Buttons")
        property string key: "buttons"
        Column {
            width: parent.width
            spacing: applet.pad
            LunaGroupBox {
                width: parent.width
                title: qsTr("Button configuration")
                titleBackground: Theme.tokens.controls.tab.paneBackground
                Column {
                    spacing: applet.pad
                    LunaCheckBox {
                        objectName: "option:swapButtons"
                        text: qsTr("Switch primary and secondary buttons")
                        checked: applet.swap
                        onToggled: applet.swap = checked
                    }
                    Text {
                        width: Theme.px(applet.tk.fieldWidth)
                        wrapMode: Text.Wrap
                        font: Theme.fonts.ui
                        text: qsTr("Select this check box to make the button on the right the one you use for primary functions such as selecting and dragging.")
                    }
                }
            }
            LunaGroupBox {
                width: parent.width
                title: qsTr("Double-click speed")
                titleBackground: Theme.tokens.controls.tab.paneBackground
                Row {
                    spacing: applet.pad
                    Column {
                        spacing: applet.pad
                        Text {
                            width: Theme.px(applet.tk.fieldWidth)
                            wrapMode: Text.Wrap
                            font: Theme.fonts.ui
                            text: qsTr("Double-click the folder to test your setting. If the folder does not open or close, try using a slower setting.")
                        }
                        Row {
                            spacing: applet.pad
                            Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Speed:  Slow"); font: Theme.fonts.ui }
                            LunaSlider {
                                objectName: "doubleClickSlider"
                                // Fast on the right: the slider runs from the longest time to the shortest.
                                from: applet.mtk.doubleClickSlowMs
                                to: applet.mtk.doubleClickFastMs
                                stepSize: applet.mtk.doubleClickStepMs
                                tickCount: applet.mtk.tickCount
                                value: applet.doubleClickMs
                                onMoved: applet.doubleClickMs = Math.round(value)
                            }
                            Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Fast"); font: Theme.fonts.ui }
                        }
                    }
                    // Test folder: opens on a double-click within the pending time.
                    Image {
                        id: testFolder
                        objectName: "testFolder"
                        property bool open: false
                        property double lastClick: 0
                        width: Theme.px(applet.tk.testAreaSize)
                        height: width
                        sourceSize: Qt.size(width, height)
                        source: "image://icon/" + (open ? "folder-open" : "folder")
                        MouseArea {
                            anchors.fill: parent
                            onPressed: {
                                const now = Date.now()
                                if (now - testFolder.lastClick <= applet.doubleClickMs) {
                                    testFolder.open = !testFolder.open
                                    testFolder.lastClick = 0
                                } else {
                                    testFolder.lastClick = now
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Item {
        property string title: qsTr("Pointer Options")
        property string key: "pointer"
        LunaGroupBox {
            width: parent.width
            title: qsTr("Motion")
            titleBackground: Theme.tokens.controls.tab.paneBackground
            Column {
                spacing: applet.pad
                Text { text: qsTr("Select a pointer speed:"); font: Theme.fonts.ui }
                Row {
                    spacing: applet.pad
                    Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Slow"); font: Theme.fonts.ui }
                    LunaSlider {
                        objectName: "pointerSpeedSlider"
                        from: -1
                        to: 1
                        stepSize: applet.mtk.speedStep
                        tickCount: applet.mtk.tickCount
                        value: applet.speed
                        onMoved: applet.speed = value
                    }
                    Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Fast"); font: Theme.fonts.ui }
                }
            }
        }
    }
}
