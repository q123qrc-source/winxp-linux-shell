// SPDX-License-Identifier: GPL-3.0-or-later
// Fingerprint prompt for the selected user. Wired up in phase 4; hidden until
// then (active stays false).
import QtQuick
import Meadow.Controls

Item {
    id: root
    property bool active: false
    visible: active
    implicitWidth: 0
    implicitHeight: 0
}
