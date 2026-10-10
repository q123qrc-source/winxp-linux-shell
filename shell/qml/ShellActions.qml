// SPDX-License-Identifier: GPL-3.0-or-later
// Requests for the shell's system dialogs, from the Start menu, the IPC
// (meadow-shellctl) and tests. The session and preview roots open them.
pragma Singleton
import QtQuick

QtObject {
    signal runRequested()
    signal logOffRequested()
    signal turnOffRequested()
}
