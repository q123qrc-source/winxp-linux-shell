// SPDX-License-Identifier: GPL-3.0-or-later
// The shell's Turn Off Computer dialog: the shared Meadow.Controls dialog
// driven by logind through PowerService.
import QtQuick
import Meadow.Controls
import Meadow.Shell

TurnOffDialog {
    objectName: "turnOffDialog"
    power: Shell.power
}
