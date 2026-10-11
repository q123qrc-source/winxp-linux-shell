// SPDX-License-Identifier: GPL-3.0-or-later
// "Network Connections": the current connection from NetworkManager
// (Shell.network) and a button to the full network settings (the installed
// connection editor).
import QtQuick
import Meadow.Controls
import Meadow.Shell

AppletSheet {
    id: applet

    function load(tab) { showTab(tab) }

    Item {
        property string title: qsTr("General")
        property string key: "general"
        Column {
            width: parent.width
            spacing: applet.pad
            LunaGroupBox {
                width: parent.width
                title: qsTr("Connection")
                titleBackground: Theme.tokens.controls.tab.paneBackground
                Grid {
                    columns: 2
                    columnSpacing: applet.pad
                    rowSpacing: applet.pad
                    Text { text: qsTr("Status:"); font: Theme.fonts.ui }
                    Text {
                        objectName: "networkStatus"
                        font: Theme.fonts.ui
                        text: !Shell.network.available ? qsTr("Network service not available")
                            : !Shell.network.connected ? qsTr("Disconnected")
                            : Shell.network.limited ? qsTr("Limited or no connectivity") : qsTr("Connected")
                    }
                    Text { text: qsTr("Name:"); font: Theme.fonts.ui }
                    Text { font: Theme.fonts.ui; text: Shell.network.connectionName || "—" }
                    Text { text: qsTr("Type:"); font: Theme.fonts.ui }
                    Text {
                        font: Theme.fonts.ui
                        text: Shell.network.kind === "wireless" ? qsTr("Wireless")
                            : Shell.network.kind === "wired" ? qsTr("Local Area Connection") : "—"
                    }
                }
            }
            LunaButton {
                objectName: "networkSettings"
                text: qsTr("Network Settings…")
                onClicked: Shell.network.openSettings()
            }
        }
    }
}
