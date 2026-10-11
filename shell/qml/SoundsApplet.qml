// SPDX-License-Identifier: GPL-3.0-or-later
// "Sounds and Audio Devices": Volume (the device volume and mute, applied
// at once like the tray's volume control) and Sounds (the sound scheme:
// Meadow or No Sounds, shell.conf [Sounds] enabled).
import QtQuick
import Meadow.Controls
import Meadow.Shell

AppletSheet {
    id: applet

    property bool soundsEnabled

    function load(tab) {
        soundsEnabled = Shell.settings.boolSetting("Sounds/enabled", true)
        showTab(tab)
    }
    dirty: { Shell.settings.revision; return soundsEnabled !== Shell.settings.boolSetting("Sounds/enabled", true) }
    onApplyRequested: Shell.settings.set("Sounds/enabled", soundsEnabled)

    Item {
        property string title: qsTr("Volume")
        property string key: "volume"
        LunaGroupBox {
            width: parent.width
            title: qsTr("Device volume")
            titleBackground: Theme.tokens.controls.tab.paneBackground
            Column {
                spacing: applet.pad
                Row {
                    spacing: applet.pad
                    Image {
                        width: Theme.px(Theme.tokens.dialogs.properties.iconSize)
                        height: width
                        sourceSize: Qt.size(width, height)
                        source: "image://icon/audio-volume-high"
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        font: Theme.fonts.ui
                        text: Shell.volume.available ? qsTr("Speakers") : qsTr("No audio device")
                    }
                }
                Row {
                    spacing: applet.pad
                    enabled: Shell.volume.available
                    Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Low"); font: Theme.fonts.ui }
                    LunaSlider {
                        objectName: "volumeSlider"
                        from: 0
                        to: 1
                        tickCount: Theme.tokens.tray.volume.tickCount
                        value: Shell.volume.volume
                        onMoved: Shell.volume.volume = value
                    }
                    Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("High"); font: Theme.fonts.ui }
                }
                LunaCheckBox {
                    objectName: "option:mute"
                    enabled: Shell.volume.available
                    text: qsTr("Mute")
                    checked: Shell.volume.muted
                    onToggled: Shell.volume.muted = checked
                }
            }
        }
    }

    Item {
        property string title: qsTr("Sounds")
        property string key: "sounds"
        Column {
            width: parent.width
            spacing: applet.pad
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font: Theme.fonts.ui
                text: qsTr("A sound scheme is a set of sounds applied to events in Meadow, such as logging on and off.")
            }
            Text { text: qsTr("Sound scheme:"); font: Theme.fonts.ui }
            LunaComboBox {
                objectName: "soundSchemeCombo"
                width: parent.width
                model: [qsTr("Meadow Default"), qsTr("No Sounds")]
                currentIndex: applet.soundsEnabled ? 0 : 1
                onActivated: (index) => applet.soundsEnabled = index === 0
            }
        }
    }
}
